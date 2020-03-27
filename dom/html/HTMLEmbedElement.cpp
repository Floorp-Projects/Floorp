/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/EventStates.h"
#include "mozilla/dom/BindContext.h"
#include "mozilla/dom/HTMLEmbedElement.h"
#include "mozilla/dom/HTMLEmbedElementBinding.h"
#include "mozilla/dom/ElementInlines.h"

#include "mozilla/dom/Document.h"
#include "nsIPluginDocument.h"
#include "nsThreadUtils.h"
#include "nsIWidget.h"
#include "nsContentUtils.h"
#ifdef XP_MACOSX
#  include "mozilla/EventDispatcher.h"
#  include "mozilla/dom/Event.h"
#endif
#include "mozilla/dom/HTMLObjectElement.h"

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Embed)

namespace mozilla {
namespace dom {

HTMLEmbedElement::HTMLEmbedElement(
    already_AddRefed<mozilla::dom::NodeInfo>&& aNodeInfo,
    FromParser aFromParser)
    : nsGenericHTMLElement(std::move(aNodeInfo)) {
  RegisterActivityObserver();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

HTMLEmbedElement::~HTMLEmbedElement() {
#ifdef XP_MACOSX
  HTMLObjectElement::OnFocusBlurPlugin(this, false);
#endif
  UnregisterActivityObserver();
  DestroyImageLoadingContent();
}

NS_IMPL_CYCLE_COLLECTION_CLASS(HTMLEmbedElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLEmbedElement,
                                                  nsGenericHTMLElement)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ISUPPORTS_CYCLE_COLLECTION_INHERITED(
    HTMLEmbedElement, nsGenericHTMLElement, nsIRequestObserver,
    nsIStreamListener, nsFrameLoaderOwner, nsIObjectLoadingContent,
    imgINotificationObserver, nsIImageLoadingContent, nsIChannelEventSink)

NS_IMPL_ELEMENT_CLONE(HTMLEmbedElement)

#ifdef XP_MACOSX

NS_IMETHODIMP
HTMLEmbedElement::PostHandleEvent(EventChainPostVisitor& aVisitor) {
  HTMLObjectElement::HandleFocusBlurPlugin(this, aVisitor.mEvent);
  return NS_OK;
}

#endif  // #ifdef XP_MACOSX

void HTMLEmbedElement::AsyncEventRunning(AsyncEventDispatcher* aEvent) {
  nsImageLoadingContent::AsyncEventRunning(aEvent);
}

nsresult HTMLEmbedElement::BindToTree(BindContext& aContext, nsINode& aParent) {
  nsresult rv = nsGenericHTMLElement::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsObjectLoadingContent::BindToTree(aContext, aParent);
  NS_ENSURE_SUCCESS(rv, rv);

  if (IsInComposedDoc()) {
    // Don't kick off load from being bound to a plugin document - the plugin
    // document will call nsObjectLoadingContent::InitializeFromChannel() for
    // the initial load.
    nsCOMPtr<nsIPluginDocument> pluginDoc =
        do_QueryInterface(&aContext.OwnerDoc());
    if (!pluginDoc) {
      void (HTMLEmbedElement::*start)() = &HTMLEmbedElement::StartObjectLoad;
      nsContentUtils::AddScriptRunner(
          NewRunnableMethod("dom::HTMLEmbedElement::BindToTree", this, start));
    }
  }

  return NS_OK;
}

void HTMLEmbedElement::UnbindFromTree(bool aNullParent) {
#ifdef XP_MACOSX
  // When a page is reloaded (when an Document's content is removed), the
  // focused element isn't necessarily sent an eBlur event. See
  // nsFocusManager::ContentRemoved(). This means that a widget may think it
  // still contains a focused plugin when it doesn't -- which in turn can
  // disable text input in the browser window. See bug 1137229.
  HTMLObjectElement::OnFocusBlurPlugin(this, false);
#endif
  nsObjectLoadingContent::UnbindFromTree(aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aNullParent);
}

nsresult HTMLEmbedElement::AfterSetAttr(int32_t aNamespaceID, nsAtom* aName,
                                        const nsAttrValue* aValue,
                                        const nsAttrValue* aOldValue,
                                        nsIPrincipal* aSubjectPrincipal,
                                        bool aNotify) {
  if (aValue) {
    nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  return nsGenericHTMLElement::AfterSetAttr(
      aNamespaceID, aName, aValue, aOldValue, aSubjectPrincipal, aNotify);
}

nsresult HTMLEmbedElement::OnAttrSetButNotChanged(
    int32_t aNamespaceID, nsAtom* aName, const nsAttrValueOrString& aValue,
    bool aNotify) {
  nsresult rv = AfterMaybeChangeAttr(aNamespaceID, aName, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  return nsGenericHTMLElement::OnAttrSetButNotChanged(aNamespaceID, aName,
                                                      aValue, aNotify);
}

nsresult HTMLEmbedElement::AfterMaybeChangeAttr(int32_t aNamespaceID,
                                                nsAtom* aName, bool aNotify) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aName == nsGkAtoms::src) {
      // If aNotify is false, we are coming from the parser or some such place;
      // we'll get bound after all the attributes have been set, so we'll do the
      // object load from BindToTree.
      // Skip the LoadObject call in that case.
      // We also don't want to start loading the object when we're not yet in
      // a document, just in case that the caller wants to set additional
      // attributes before inserting the node into the document.
      if (aNotify && IsInComposedDoc() && !BlockEmbedOrObjectContentLoading()) {
        nsresult rv = LoadObject(aNotify, true);
        NS_ENSURE_SUCCESS(rv, rv);
      }
    }
  }

  return NS_OK;
}

bool HTMLEmbedElement::IsHTMLFocusable(bool aWithMouse, bool* aIsFocusable,
                                       int32_t* aTabIndex) {
  // Has plugin content: let the plugin decide what to do in terms of
  // internal focus from mouse clicks
  if (aTabIndex) {
    *aTabIndex = TabIndex();
  }

  *aIsFocusable = true;

  // Let the plugin decide, so override.
  return true;
}

nsIContent::IMEState HTMLEmbedElement::GetDesiredIMEState() {
  if (Type() == eType_Plugin) {
    return IMEState(IMEState::PLUGIN);
  }

  return nsGenericHTMLElement::GetDesiredIMEState();
}

bool HTMLEmbedElement::ParseAttribute(int32_t aNamespaceID, nsAtom* aAttribute,
                                      const nsAString& aValue,
                                      nsIPrincipal* aMaybeScriptedPrincipal,
                                      nsAttrValue& aResult) {
  if (aNamespaceID == kNameSpaceID_None) {
    if (aAttribute == nsGkAtoms::align) {
      return ParseAlignValue(aValue, aResult);
    }
    if (aAttribute == nsGkAtoms::width || aAttribute == nsGkAtoms::height ||
        aAttribute == nsGkAtoms::hspace || aAttribute == nsGkAtoms::vspace) {
      return aResult.ParseHTMLDimension(aValue);
    }
  }

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aMaybeScriptedPrincipal, aResult);
}

static void MapAttributesIntoRuleBase(const nsMappedAttributes* aAttributes,
                                      MappedDeclarations& aDecls) {
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aDecls);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aDecls);
}

static void MapAttributesIntoRuleExceptHidden(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  MapAttributesIntoRuleBase(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesIntoExceptHidden(aAttributes,
                                                            aDecls);
}

void HTMLEmbedElement::MapAttributesIntoRule(
    const nsMappedAttributes* aAttributes, MappedDeclarations& aDecls) {
  MapAttributesIntoRuleBase(aAttributes, aDecls);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aDecls);
}

NS_IMETHODIMP_(bool)
HTMLEmbedElement::IsAttributeMapped(const nsAtom* aAttribute) const {
  static const MappedAttributeEntry* const map[] = {
      sCommonAttributeMap,
      sImageMarginSizeAttributeMap,
      sImageBorderAttributeMap,
      sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map);
}

nsMapRuleToAttributesFunc HTMLEmbedElement::GetAttributeMappingFunction()
    const {
  return &MapAttributesIntoRuleExceptHidden;
}

void HTMLEmbedElement::StartObjectLoad(bool aNotify, bool aForceLoad) {
  // BindToTree can call us asynchronously, and we may be removed from the tree
  // in the interim
  if (!IsInComposedDoc() || !OwnerDoc()->IsActive() ||
      BlockEmbedOrObjectContentLoading()) {
    return;
  }

  LoadObject(aNotify, aForceLoad);
  SetIsNetworkCreated(false);
}

EventStates HTMLEmbedElement::IntrinsicState() const {
  return nsGenericHTMLElement::IntrinsicState() | ObjectState();
}

uint32_t HTMLEmbedElement::GetCapabilities() const {
  return eSupportPlugins | eAllowPluginSkipChannel | eSupportImages |
         eSupportDocuments;
}

void HTMLEmbedElement::DestroyContent() {
  nsObjectLoadingContent::DestroyContent();
  nsGenericHTMLElement::DestroyContent();
}

nsresult HTMLEmbedElement::CopyInnerTo(HTMLEmbedElement* aDest) {
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(aDest);
  }

  return rv;
}

JSObject* HTMLEmbedElement::WrapNode(JSContext* aCx,
                                     JS::Handle<JSObject*> aGivenProto) {
  JSObject* obj;
  obj = HTMLEmbedElement_Binding::Wrap(aCx, this, aGivenProto);

  if (!obj) {
    return nullptr;
  }
  JS::Rooted<JSObject*> rootedObj(aCx, obj);
  SetupProtoChain(aCx, rootedObj);
  return rootedObj;
}

nsContentPolicyType HTMLEmbedElement::GetContentPolicyType() const {
  return nsIContentPolicy::TYPE_INTERNAL_EMBED;
}

}  // namespace dom
}  // namespace mozilla
