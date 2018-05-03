/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/HTMLFormSubmission.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPluginDocument.h"
#include "nsIDOMDocument.h"
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
  : nsGenericHTMLFormElement(aNodeInfo, NS_FORM_OBJECT),
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

void
HTMLObjectElement::AsyncEventRunning(AsyncEventDispatcher* aEvent)
{
  nsImageLoadingContent::AsyncEventRunning(aEvent);
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
    StartObjectLoad(aHaveNotified, false);
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

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(HTMLObjectElement,
                                             nsGenericHTMLFormElement,
                                             imgINotificationObserver,
                                             nsIRequestObserver,
                                             nsIStreamListener,
                                             nsIFrameLoaderOwner,
                                             nsIObjectLoadingContent,
                                             nsIImageLoadingContent,
                                             nsIChannelEventSink,
                                             nsIConstraintValidation)

NS_IMPL_ELEMENT_CLONE(HTMLObjectElement)

#ifdef XP_MACOSX

static nsIWidget* GetWidget(Element* aElement)
{
  return nsContentUtils::WidgetForDocument(aElement->OwnerDoc());
}

Element* HTMLObjectElement::sLastFocused = nullptr; // Weak

class PluginFocusSetter : public Runnable
{
public:
  PluginFocusSetter(nsIWidget* aWidget, Element* aElement)
    : Runnable("PluginFocusSetter")
    , mWidget(aWidget)
    , mElement(aElement)
  {
  }

  NS_IMETHOD Run() override
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
  nsFocusManager *fm = nsFocusManager::GetFocusManager();
  if (fm && fm->GetFocusedElement() == aElement) {
    OnFocusBlurPlugin(aElement, true);
  }
}

void
HTMLObjectElement::HandleFocusBlurPlugin(Element* aElement,
                                         WidgetEvent* aEvent)
{
  if (!aEvent->IsTrusted()) {
    return;
  }
  switch (aEvent->mMessage) {
    case eFocus: {
      OnFocusBlurPlugin(aElement, true);
      break;
    }
    case eBlur: {
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
    nsContentUtils::AddScriptRunner(
      NewRunnableMethod("dom::HTMLObjectElement::BindToTree", this, start));
  }

  return NS_OK;
}

void
HTMLObjectElement::UnbindFromTree(bool aDeep,
                                  bool aNullParent)
{
#ifdef XP_MACOSX
  // When a page is reloaded (when an nsIDocument's content is removed), the
  // focused element isn't necessarily sent an eBlur event. See
  // nsFocusManager::ContentRemoved(). This means that a widget may think it
  // still contains a focused plugin when it doesn't -- which in turn can
  // disable text input in the browser window. See bug 1137229.
  OnFocusBlurPlugin(this, false);
#endif
  nsObjectLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
HTMLObjectElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                const nsAttrValue* aValue,
                                const nsAttrValue* aOldValue,
                                nsIPrincipal* aSubjectPrincipal,
                                bool aNotify)
{
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormElement::AfterSetAttr(aNamespaceID, aName, aValue,
                                                aOldValue, aSubjectPrincipal, aNotify);
}

nsresult
HTMLObjectElement::OnAttrSetButNotChanged(int32_t aNamespaceID, nsAtom* aName,
                                          const nsAttrValueOrString& aValue,
                                          bool aNotify)
{
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                          aValue, aNotify);
}

nsresult
HTMLObjectElement::AfterMaybeChangeAttr(int32_t aNamespaceID, nsAtom* aName,
                                        bool aNotify)
{
  if (aNamespaceID == kNameSpaceID_None) {
    // if aNotify is false, we are coming from the parser or some such place;
    // we'll get bound after all the attributes have been set, so we'll do the
    // object load from BindToTree/DoneAddingChildren.
    // Skip the LoadObject call in that case.
    // We also don't want to start loading the object when we're not yet in
    // a document, just in case that the caller wants to set additional
    // attributes before inserting the node into the document.
    if (aNotify && IsInComposedDoc() && mIsDoneAddingChildren &&
        aName == nsGkAtoms::data && !BlockEmbedOrObjectContentLoading()) {
      return LoadObject(aNotify, true);
    }
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
         ((Type() == eType_Document || Type() == eType_FakePlugin) &&
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
      *aTabIndex = TabIndex();
    }

    *aIsFocusable = false;

    return false;
  }

  // This method doesn't call nsGenericHTMLFormElement intentionally.
  // TODO: It should probably be changed when bug 597242 will be fixed.
  if (Type() == eType_Plugin || IsEditableRoot() ||
      ((Type() == eType_Document || Type() == eType_FakePlugin) &&
       nsContentUtils::IsSubDocumentTabbable(this))) {
    // Has plugin content: let the plugin decide what to do in terms of
    // internal focus from mouse clicks
    if (aTabIndex) {
      *aTabIndex = TabIndex();
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
HTMLObjectElement::SubmitNamesValues(HTMLFormSubmission *aFormSubmission)
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

  RefPtr<nsNPAPIPluginInstance> pi;
  objFrame->GetPluginInstance(getter_AddRefs(pi));
  if (!pi)
    return NS_OK;

  nsAutoString value;
  nsresult rv = pi->GetFormValue(value);
  NS_ENSURE_SUCCESS(rv, rv);

  return aFormSubmission->AddNameValuePair(name, value);
}

int32_t
HTMLObjectElement::TabIndexDefault()
{
  return IsFocusableForTabIndex() ? 0 : -1;
}

nsPIDOMWindowOuter*
HTMLObjectElement::GetContentWindow(nsIPrincipal& aSubjectPrincipal)
{
  nsIDocument* doc = GetContentDocument(aSubjectPrincipal);
  if (doc) {
    return doc->GetWindow();
  }

  return nullptr;
}

bool
HTMLObjectElement::ParseAttribute(int32_t aNamespaceID,
                                  nsAtom *aAttribute,
                                  const nsAString &aValue,
                                  nsIPrincipal* aMaybeScriptedPrincipal,
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
                                                  aValue, aMaybeScriptedPrincipal, aResult);
}

void
HTMLObjectElement::MapAttributesIntoRule(const nsMappedAttributes *aAttributes,
                                         GenericSpecifiedValues *aData)
{
  nsGenericHTMLFormElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLFormElement::MapCommonAttributesInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
HTMLObjectElement::IsAttributeMapped(const nsAtom *aAttribute) const
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
HTMLObjectElement::StartObjectLoad(bool aNotify, bool aForce)
{
  // BindToTree can call us asynchronously, and we may be removed from the tree
  // in the interim
  if (!IsInComposedDoc() || !OwnerDoc()->IsActive() ||
      BlockEmbedOrObjectContentLoading()) {
    return;
  }

  LoadObject(aNotify, aForce);
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
  return nsObjectLoadingContent::GetCapabilities() | eFallbackIfClassIDPresent;
}

void
HTMLObjectElement::DestroyContent()
{
  nsObjectLoadingContent::DestroyContent();
  nsGenericHTMLFormElement::DestroyContent();
}

nsresult
HTMLObjectElement::CopyInnerTo(Element* aDest, bool aPreallocateChildren)
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest,
                                                      aPreallocateChildren);
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
