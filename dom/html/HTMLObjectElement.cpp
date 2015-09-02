/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsAutoPtr.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPluginDocument.h"
#include "nsIDOMDocument.h"
#include "nsFormSubmission.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#ifdef XP_MACOSX
#include "mozilla/EventDispatcher.h"
#include "mozilla/dom/Event.h"
#include "nsFocusManager.h"
#endif

namespace mozilla {
namespace dom {

HTMLObjectElement::HTMLObjectElement(already_AddRefed<mozilla::dom::NodeInfo>& aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mIsDoneAddingChildren(!aFromParser)
{
  RegisterActivityObserver();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // <object> is always barred from constraint validation.
  SetBarredFromConstraintValidation(true);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

HTMLObjectElement::~HTMLObjectElement()
{
#ifdef XP_MACOSX
  OnFocusBlurPlugin(this, false);
#endif
  UnregisterActivityObserver();
  DestroyImageLoadingContent();
}

bool
HTMLObjectElement::IsInteractiveHTMLContent(bool aIgnoreTabindex) const
{
  return HasAttr(kNameSpaceID_None, nsGkAtoms::usemap) ||
         nsGenericHTMLFormElement::IsInteractiveHTMLContent(aIgnoreTabindex);
}

bool
HTMLObjectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
HTMLObjectElement::DoneAddingChildren(bool aHaveNotified)
{
  mIsDoneAddingChildren = true;

  // If we're already in a document, we need to trigger the load
  // Otherwise, BindToTree takes care of that.
  if (IsInComposedDoc()) {
    StartObjectLoad(aHaveNotified);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLObjectElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLObjectElement,
                                                  nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLObjectElement,
                                                nsGenericHTMLFormElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ADDREF_INHERITED(HTMLObjectElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLObjectElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLObjectElement)
  NS_INTERFACE_TABLE_INHERITED(HTMLObjectElement,
                               nsIDOMHTMLObjectElement,
                               imgINotificationObserver,
                               nsIRequestObserver,
                               nsIStreamListener,
                               nsIFrameLoaderOwner,
                               nsIObjectLoadingContent,
                               nsIImageLoadingContent,
                               imgIOnloadBlocker,
                               nsIChannelEventSink,
                               nsIConstraintValidation)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLFormElement)

NS_IMPL_ELEMENT_CLONE(HTMLObjectElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION(HTMLObjectElement)

#ifdef XP_MACOSX

static nsIWidget* GetWidget(Element* aElement)
{
  return nsContentUtils::WidgetForDocument(aElement->OwnerDoc());
}

Element* HTMLObjectElement::sLastFocused = nullptr; // Weak

class PluginFocusSetter : public nsRunnable
{
public:
  PluginFocusSetter(nsIWidget* aWidget, Element* aElement)
  : mWidget(aWidget), mElement(aElement)
  {
  }

  NS_IMETHOD Run()
  {
    if (mElement) {
      HTMLObjectElement::sLastFocused = mElement;
      bool value = true;
      mWidget->SetPluginFocused(value);
    } else if (!HTMLObjectElement::sLastFocused) {
      bool value = false;
      mWidget->SetPluginFocused(value);
    }

    return NS_OK;
  }

private:
  nsCOMPtr<nsIWidget> mWidget;
  nsCOMPtr<Element> mElement;
};

void
HTMLObjectElement::OnFocusBlurPlugin(Element* aElement, bool aFocus)
{
  // In general we don't want to call nsIWidget::SetPluginFocused() for any
  // Element that doesn't have a plugin running.  But if SetPluginFocused(true)
  // was just called for aElement while it had a plugin running, we want to
  // make sure nsIWidget::SetPluginFocused(false) gets called for it now, even
  // if aFocus is true.
  if (aFocus) {
    nsCOMPtr<nsIObjectLoadingContent> olc = do_QueryInterface(aElement);
    bool hasRunningPlugin = false;
    if (olc) {
      // nsIObjectLoadingContent::GetHasRunningPlugin() fails when
      // nsContentUtils::IsCallerChrome() returns false (which it can do even
      // when we're processing a trusted focus event).  We work around this by
      // calling nsObjectLoadingContent::HasRunningPlugin() directly.
      hasRunningPlugin =
        static_cast<nsObjectLoadingContent*>(olc.get())->HasRunningPlugin();
    }
    if (!hasRunningPlugin) {
      aFocus = false;
    }
  }

  if (aFocus || aElement == sLastFocused) {
    if (!aFocus) {
      sLastFocused = nullptr;
    }
    nsIWidget* widget = GetWidget(aElement);
    if (widget) {
      nsContentUtils::AddScriptRunner(
        new PluginFocusSetter(widget, aFocus ? aElement : nullptr));
    }
  }
}

void
HTMLObjectElement::HandlePluginCrashed(Element* aElement)
{
  OnFocusBlurPlugin(aElement, false);
}

void
HTMLObjectElement::HandlePluginInstantiated(Element* aElement)
{
  // If aElement is already focused when a plugin is instantiated, we need
  // to initiate a call to nsIWidget::SetPluginFocused(true).  Otherwise
  // keyboard input won't work in a click-to-play plugin until aElement
  // loses focus and regains it.
  nsIContent* focusedContent = nullptr;
  nsFocusManager *fm = nsFocusManager::GetFocusManager();
  if (fm) {
    focusedContent = fm->GetFocusedContent();
  }
  if (SameCOMIdentity(focusedContent, aElement)) {
    OnFocusBlurPlugin(aElement, true);
  }
}

void
HTMLObjectElement::HandleFocusBlurPlugin(Element* aElement,
                                         WidgetEvent* aEvent)
{
  if (!aEvent->mFlags.mIsTrusted) {
    return;
  }
  switch (aEvent->mMessage) {
    case eFocus: {
      OnFocusBlurPlugin(aElement, true);
      break;
    }
    case NS_BLUR_CONTENT: {
      OnFocusBlurPlugin(aElement, false);
      break;
    }
    default:
      break;
  }
}

NS_IMETHODIMP
HTMLObjectElement::PostHandleEvent(EventChainPostVisitor& aVisitor)
{
  HandleFocusBlurPlugin(this, aVisitor.mEvent);
  return NS_OK;
}

#endif // #ifdef XP_MACOSX

NS_IMETHODIMP
HTMLObjectElement::GetForm(nsIDOMHTMLFormElement **aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

void
HTMLObjectElement::GetItemValueText(DOMString& aValue)
{
  GetData(aValue);
}

void
HTMLObjectElement::SetItemValueText(const nsAString& aValue)
{
  SetData(aValue);
}

nsresult
HTMLObjectElement::BindToTree(nsIDocument *aDocument,
                              nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
                                                     aBindingParent,
                                                     aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsObjectLoadingContent::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // Don't kick off load from being bound to a plugin document - the plugin
  // document will call nsObjectLoadingContent::InitializeFromChannel() for the
  // initial load.
  nsCOMPtr<nsIPluginDocument> pluginDoc = do_QueryInterface(aDocument);

  // If we already have all the children, start the load.
  if (mIsDoneAddingChildren && !pluginDoc) {
    void (HTMLObjectElement::*start)() = &HTMLObjectElement::StartObjectLoad;
    nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, start));
  }

  return NS_OK;
}

void
HTMLObjectElement::UnbindFromTree(bool aDeep,
                                  bool aNullParent)
{
#ifdef XP_MACOSX
  // When a page is reloaded (when an nsIDocument's content is removed), the
  // focused element isn't necessarily sent an NS_BLUR_CONTENT event. See
  // nsFocusManager::ContentRemoved(). This means that a widget may think it
  // still contains a focused plugin when it doesn't -- which in turn can
  // disable text input in the browser window. See bug 1137229.
  OnFocusBlurPlugin(this, false);
#endif
  nsObjectLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}



nsresult
HTMLObjectElement::SetAttr(int32_t aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify)
{
  nsresult rv = nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                                  aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  // if aNotify is false, we are coming from the parser or some such place;
  // we'll get bound after all the attributes have been set, so we'll do the
  // object load from BindToTree/DoneAddingChildren.
  // Skip the LoadObject call in that case.
  // We also don't want to start loading the object when we're not yet in
  // a document, just in case that the caller wants to set additional
  // attributes before inserting the node into the document.
  if (aNotify && IsInComposedDoc() && mIsDoneAddingChildren &&
      aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::data) {
    return LoadObject(aNotify, true);
  }

  return NS_OK;
}

nsresult
HTMLObjectElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify)
{
  nsresult rv = nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID,
                                                    aAttribute, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  // See comment in SetAttr
  if (aNotify && IsInComposedDoc() && mIsDoneAddingChildren &&
      aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::data) {
    return LoadObject(aNotify, true);
  }

  return NS_OK;
}

bool
HTMLObjectElement::IsFocusableForTabIndex()
{
  nsIDocument* doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    return false;
  }

  return IsEditableRoot() ||
         (Type() == eType_Document &&
          nsContentUtils::IsSubDocumentTabbable(this));
}

bool
HTMLObjectElement::IsHTMLFocusable(bool aWithMouse,
                                   bool *aIsFocusable, int32_t *aTabIndex)
{
  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  nsIDocument *doc = GetComposedDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }

    *aIsFocusable = false;

    return false;
  }

  // This method doesn't call nsGenericHTMLFormElement intentionally.
  // TODO: It should probably be changed when bug 597242 will be fixed.
  if (Type() == eType_Plugin || IsEditableRoot() ||
      (Type() == eType_Document && nsContentUtils::IsSubDocumentTabbable(this))) {
    // Has plugin content: let the plugin decide what to do in terms of
    // internal focus from mouse clicks
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }

    *aIsFocusable = true;

    return false;
  }

  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  const nsAttrValue* attrVal = mAttrsAndChildren.GetAttr(nsGkAtoms::tabindex);

  *aIsFocusable = attrVal && attrVal->Type() == nsAttrValue::eInteger;

  if (aTabIndex && *aIsFocusable) {
    *aTabIndex = attrVal->GetIntegerValue();
  }

  return false;
}

nsIContent::IMEState
HTMLObjectElement::GetDesiredIMEState()
{
  if (Type() == eType_Plugin) {
    return IMEState(IMEState::PLUGIN);
  }
   
  return nsGenericHTMLFormElement::GetDesiredIMEState();
}

NS_IMETHODIMP
HTMLObjectElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
HTMLObjectElement::SubmitNamesValues(nsFormSubmission *aFormSubmission)
{
  nsAutoString name;
  if (!GetAttr(kNameSpaceID_None, nsGkAtoms::name, name)) {
    // No name, don't submit.

    return NS_OK;
  }

  nsIFrame* frame = GetPrimaryFrame();

  nsIObjectFrame *objFrame = do_QueryFrame(frame);
  if (!objFrame) {
    // No frame, nothing to submit.

    return NS_OK;
  }

  nsRefPtr<nsNPAPIPluginInstance> pi;
  objFrame->GetPluginInstance(getter_AddRefs(pi));
  if (!pi)
    return NS_OK;

  nsAutoString value;
  nsresult rv = pi->GetFormValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  return aFormSubmission->AddNameValuePair(name, value);
}

NS_IMPL_STRING_ATTR(HTMLObjectElement, Align, align)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Border, border)
NS_IMPL_URI_ATTR_WITH_BASE(HTMLObjectElement, Code, code, codebase)
NS_IMPL_URI_ATTR(HTMLObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(HTMLObjectElement, CodeType, codetype)
NS_IMPL_URI_ATTR_WITH_BASE(HTMLObjectElement, Data, data, codebase)
NS_IMPL_BOOL_ATTR(HTMLObjectElement, Declare, declare)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Height, height)
NS_IMPL_INT_ATTR(HTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Standby, standby)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(HTMLObjectElement, UseMap, usemap)
NS_IMPL_INT_ATTR(HTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(HTMLObjectElement, Width, width)

int32_t
HTMLObjectElement::TabIndexDefault()
{
  return IsFocusableForTabIndex() ? 0 : -1;
}

NS_IMETHODIMP
HTMLObjectElement::GetContentDocument(nsIDOMDocument **aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  nsCOMPtr<nsIDOMDocument> domDoc = do_QueryInterface(GetContentDocument());
  domDoc.forget(aContentDocument);
  return NS_OK;
}

nsIDOMWindow*
HTMLObjectElement::GetContentWindow()
{
  nsIDocument* doc = GetContentDocument();
  if (doc) {
    return doc->GetWindow();
  }

  return nullptr;
}

bool
HTMLObjectElement::ParseAttribute(int32_t aNamespaceID,
                                  nsIAtom *aAttribute,
                                  const nsAString &aValue,
                                  nsAttrValue &aResult)
{
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLFormElement::ParseAttribute(aNamespaceID, aAttribute,
                                                  aValue, aResult);
}

void
HTMLObjectElement::MapAttributesIntoRule(const nsMappedAttributes *aAttributes,
                                         nsRuleData *aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLObjectElement::IsAttributeMapped(const nsIAtom *aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}


nsMapRuleToAttributesFunc
HTMLObjectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

void
HTMLObjectElement::StartObjectLoad(bool aNotify)
{
  // BindToTree can call us asynchronously, and we may be removed from the tree
  // in the interim
  if (!IsInComposedDoc() || !OwnerDoc()->IsActive()) {
    return;
  }

  LoadObject(aNotify);
  SetIsNetworkCreated(false);
}

EventStates
HTMLObjectElement::IntrinsicState() const
{
  return nsGenericHTMLFormElement::IntrinsicState() | ObjectState();
}

uint32_t
HTMLObjectElement::GetCapabilities() const
{
  return nsObjectLoadingContent::GetCapabilities() | eSupportClassID;
}

void
HTMLObjectElement::DestroyContent()
{
  nsObjectLoadingContent::DestroyContent();
  nsGenericHTMLFormElement::DestroyContent();
}

nsresult
HTMLObjectElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<HTMLObjectElement*>(aDest));
  }

  return rv;
}

JSObject*
HTMLObjectElement::WrapNode(JSContext* aCx, JS::Handle<JSObject*> aGivenProto)
{
  JS::Rooted<JSObject*> obj(aCx,
    HTMLObjectElementBinding::Wrap(aCx, this, aGivenProto));
  if (!obj) {
    return nullptr;
  }
  SetupProtoChain(aCx, obj);
  return obj;
}

} // namespace dom
} // namespace mozilla

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)
