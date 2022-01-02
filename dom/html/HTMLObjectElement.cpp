/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLObjectElement.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/ElementInlines.h"
#include "mozilla/dom/WindowProxyHolder.h"
#include "nsAttrValueInlines.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIContentInlines.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#ifdef XP_MACOSX
#  include "mozilla/EventDispatcher.h"
#  include "mozilla/dom/Event.h"
#  include "nsFocusManager.h"
#endif

namespace mozilla::dom {

HTMLObjectElement::HTMLObjectElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLFormControlElement(std::move(aNodeInfo),
                                      FormControlType::Object),
      mIsDoneAddingChildren(!aFromParser) {
  RegisterActivityObserver();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // <object> is always barred from constraint validation.
  SetBarredFromConstraintValidation(true);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

HTMLObjectElement::~HTMLObjectElement() {
  UnregisterActivityObserver();
  nsImageLoadingContent::Destroy();
}

bool HTMLObjectElement::IsInteractiveHTMLContent() const {
  return HasAttr(kNameSpaceID_None, nsGkAtoms::usemap) ||
         nsGenericHTMLFormControlElement::IsInteractiveHTMLContent();
}

void HTMLObjectElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

bool HTMLObjectElement::IsDoneAddingChildren() { return mIsDoneAddingChildren; }

void HTMLObjectElement::DoneAddingChildren(bool aHaveNotified) {
  mIsDoneAddingChildren = true;

  // If we're already in a document, we need to trigger the load
  // Otherwise, BindToTree takes care of that.
  if (IsInComposedDoc()) {
    StartObjectLoad(aHaveNotified, false);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLObjectElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(
    HTMLObjectElement, nsGenericHTMLFormControlElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mValidity)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_CYCLE_COLLECTION_UNLINK_BEGIN_INHERITED(HTMLObjectElement,
                                                nsGenericHTMLFormControlElement)
  NS_IMPL_CYCLE_COLLECTION_UNLINK(mValidity)
  nsObjectLoadingContent::Unlink(tmp);
NS_IMPL_CYCLE_COLLECTION_UNLINK_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(
    HTMLObjectElement, nsGenericHTMLFormControlElement,
    imgINotificationObserver, nsIRequestObserver, nsIStreamListener,
    nsFrameLoaderOwner, nsIObjectLoadingContent, nsIImageLoadingContent,
    nsIChannelEventSink, nsIConstraintValidation)

NS_IMPL_ELEMENT_CLONE(HTMLObjectElement)

nsresult HTMLObjectElement::BindToTree(BindContext& aContext,
                                       nsINode& aParent) {
  nsresult rv = nsGenericHTMLFormControlElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsObjectLoadingContent::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we already have all the children, start the load.
  if (IsInComposedDoc() && mIsDoneAddingChildren) {
    void (HTMLObjectElement::*start)() = &HTMLObjectElement::StartObjectLoad;
    nsContentUtils::AddScriptRunner(
        NewRunnableMethod("dom::HTMLObjectElement::BindToTree", this, start));
  }

  return NS_OK;
}

void HTMLObjectElement::UnbindFromTree(bool aNullParent) {
  nsObjectLoadingContent::UnbindFromTree(aNullParent);
  nsGenericHTMLFormControlElement::UnbindFromTree(aNullParent);
}

nsresult HTMLObjectElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                         const nsAttrValue* aValue,
                                         const nsAttrValue* aOldValue,
                                         nsIPrincipal* aSubjectPrincipal,
                                         bool aNotify) {
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormControlElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLObjectElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLFormControlElement::OnAttrSetButNotChanged(
      aNamespaceID, aName, aValue, aNotify);
}

nsresult HTMLObjectElement::AfterMaybeChangeAttr(int32_t aNamespaceID,
                                                 nsAtom* aName, bool aNotify) {
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
      nsContentUtils::AddScriptRunner(NS_NewRunnableFunction(
          "HTMLObjectElement::LoadObject",
          [self = RefPtr<HTMLObjectElement>(this), aNotify]() {
            if (self->IsInComposedDoc()) {
              self->LoadObject(aNotify, true);
            }
          }));
      return NS_OK;
    }
  }

  return NS_OK;
}

bool HTMLObjectElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                        int32_t* aTabIndex) {
  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  Document* doc = GetComposedDoc();
  if (!doc || IsInDesignMode()) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;
    return false;
  }

  // Plugins that show the empty fallback should not accept focus.
  if (Type() == eType_Fallback) {
    if (aTabIndex) {
      *aTabIndex = -1;
    }

    *aIsFocusable = false;
    return false;
  }

  const nsAttrValue* attrVal = mAttrs.GetAttr(nsGkAtoms::tabindex);
  bool isFocusable = attrVal && attrVal->Type() == nsAttrValue::eInteger;

  // This method doesn't call nsGenericHTMLFormControlElement intentionally.
  // TODO: It should probably be changed when bug 597242 will be fixed.
  if (IsEditableRoot() ||
      ((Type() == eType_Document || Type() == eType_FakePlugin) &&
       nsContentUtils::IsSubDocumentTabbable(this))) {
    if (aTabIndex) {
      *aTabIndex = isFocusable ? attrVal->GetIntegerValue() : 0;
    }

    *aIsFocusable = true;
    return false;
  }

  // TODO: this should probably be managed directly by IsHTMLFocusable.
  // See bug 597242.
  if (aTabIndex && isFocusable) {
    *aTabIndex = attrVal->GetIntegerValue();
    *aIsFocusable = true;
  }

  return false;
}

int32_t HTMLObjectElement::TabIndexDefault() { return 0; }

Nullable<WindowProxyHolder> HTMLObjectElement::GetContentWindow(
    nsIPrincipal& aSubjectPrincipal) {
  Document* doc = GetContentDocument(aSubjectPrincipal);
  if (doc) {
    nsPIDOMWindowOuter* win = doc->GetWindow();
    if (win) {
      return WindowProxyHolder(win->GetBrowsingContext());
    }
  }

  return nullptr;
}

bool HTMLObjectElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                       const nsAString& aValue,
                                       nsIPrincipal* aMaybeScriptedPrincipal,
                                       nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (ParseImageAttribute(aAttribute, aValue, aResult)) {
      return true;
    }
  }

  return nsGenericHTMLFormControlElement::ParseAttribute(
      aNamespaceID, aAttribute, aValue, aMaybeScriptedPrincipal, aResult);
}

void HTMLObjectElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  nsGenericHTMLFormControlElement::MapImageAlignAttributeInto(aAttributes,
                                                              aDecls);
  nsGenericHTMLFormControlElement::MapImageBorderAttributeInto(aAttributes,
                                                               aDecls);
  nsGenericHTMLFormControlElement::MapImageMarginAttributeInto(aAttributes,
                                                               aDecls);
  nsGenericHTMLFormControlElement::MapImageSizeAttributesInto(aAttributes,
                                                              aDecls);
  nsGenericHTMLFormControlElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLObjectElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {
      sCommonAttributeMap,
      sImageMarginSizeAttributeMap,
      sImageBorderAttributeMap,
      sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLObjectElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRule;
}

void HTMLObjectElement::StartObjectLoad(bool aNotify, bool aForce) {
  // BindToTree can call us asynchronously, and we may be removed from the tree
  // in the interim
  if (!IsInComposedDoc() || !OwnerDoc()->IsActive() ||
      BlockEmbedOrObjectContentLoading()) {
    return;
  }

  LoadObject(aNotify, aForce);
  SetIsNetworkCreated(false);
}

EventStates HTMLObjectElement::IntrinsicState() const {
  return nsGenericHTMLFormControlElement::IntrinsicState() | ObjectState();
}

uint32_t HTMLObjectElement::GetCapabilities() const {
  return nsObjectLoadingContent::GetCapabilities() | eFallbackIfClassIDPresent;
}

void HTMLObjectElement::DestroyContent() {
  nsObjectLoadingContent::Destroy();
  nsGenericHTMLFormControlElement::DestroyContent();
}

nsresult HTMLObjectElement::CopyInnerTo(Element* aDest) {
  nsresult rv = nsGenericHTMLFormControlElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<HTMLObjectElement*>(aDest));
  }

  return rv;
}

JSObject* HTMLObjectElement::WrapNode(JSContext* aCx,
                                      JS::Handle<JSObject*> aGivenProto) {
  return HTMLObjectElement_Binding::Wrap(aCx, this, aGivenProto);
}

}  // namespace mozilla::dom

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)
