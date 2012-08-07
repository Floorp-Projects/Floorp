/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsDOMError.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsIDOMHTMLEmbedElement.h"
#include "nsThreadUtils.h"
#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMSVGDocument.h"
#include "nsIScriptError.h"
#include "nsIWidget.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLSharedObjectElement : public nsGenericHTMLElement
                                , public nsObjectLoadingContent
                                , public nsIDOMHTMLAppletElement
                                , public nsIDOMHTMLEmbedElement
                                , public nsIDOMGetSVGDocument
{
public:
  nsHTMLSharedObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                            mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~nsHTMLSharedObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLElement::)
  NS_IMETHOD Click() {
    return nsGenericHTMLElement::Click();
  }
  NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_IMETHOD Focus() {
    return nsGenericHTMLElement::Focus();
  }
  NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLElement::GetDraggable(aDraggable);
  }
  NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLElement::GetInnerHTML(aInnerHTML);
  }
  NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLElement::SetInnerHTML(aInnerHTML);
  }

  // nsIDOMHTMLAppletElement
  NS_DECL_NSIDOMHTMLAPPLETELEMENT

  // Can't use macro for nsIDOMHTMLEmbedElement because it has conflicts with
  // NS_DECL_NSIDOMHTMLAPPLETELEMENT.

  // nsIDOMHTMLEmbedElement
  NS_IMETHOD GetSrc(nsAString &aSrc);
  NS_IMETHOD SetSrc(const nsAString &aSrc);
  NS_IMETHOD GetType(nsAString &aType);
  NS_IMETHOD SetType(const nsAString &aType);

  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);
  virtual IMEState GetDesiredIMEState();

  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();

  virtual bool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom *aAttribute) const;
  virtual nsEventStates IntrinsicState() const;
  virtual void DestroyContent();

  // nsObjectLoadingContent
  virtual PRUint32 GetCapabilities() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(nsGenericElement* aDest);

  void StartObjectLoad() { StartObjectLoad(true); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLSharedObjectElement,
                                                     nsGenericHTMLElement)

  virtual nsXPCClassInfo* GetClassInfo()
  {
    return static_cast<nsXPCClassInfo*>(GetClassInfoInternal());
  }
  nsIClassInfo* GetClassInfoInternal();

  virtual nsIDOMNode* AsDOMNode()
  {
    return static_cast<nsIDOMHTMLAppletElement*>(this);
  }
private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  NS_HIDDEN_(void) StartObjectLoad(bool aNotify);

  void GetTypeAttrValue(nsCString &aValue) const
  {
    if (mNodeInfo->Equals(nsGkAtoms::applet)) {
      aValue.AppendLiteral("application/x-java-vm");
    }
    else {
      nsAutoString type;
      GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);

      CopyUTF16toUTF8(type, aValue);
    }
  }

  nsIAtom *URIAttrName() const
  {
    return mNodeInfo->Equals(nsGkAtoms::applet) ?
           nsGkAtoms::code :
           nsGkAtoms::src;
  }

  // mIsDoneAddingChildren is only really used for <applet>.  This boolean is
  // always true for <embed>, per the documentation in nsIContent.h.
  bool mIsDoneAddingChildren;

  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(SharedObject)


nsHTMLSharedObjectElement::nsHTMLSharedObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                                     FromParser aFromParser)
  : nsGenericHTMLElement(aNodeInfo),
    mIsDoneAddingChildren(mNodeInfo->Equals(nsGkAtoms::embed) || !aFromParser)
{
  RegisterFreezableElement();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

void
nsHTMLSharedObjectElement::GetItemValueText(nsAString& aValue)
{
  if (mNodeInfo->Equals(nsGkAtoms::applet)) {
    nsGenericHTMLElement::GetItemValueText(aValue);
  } else {
    GetSrc(aValue);
  }
}

void
nsHTMLSharedObjectElement::SetItemValueText(const nsAString& aValue)
{
  if (mNodeInfo->Equals(nsGkAtoms::applet)) {
    nsGenericHTMLElement::SetItemValueText(aValue);
  } else {
    SetSrc(aValue);
  }
}

nsHTMLSharedObjectElement::~nsHTMLSharedObjectElement()
{
  UnregisterFreezableElement();
  DestroyImageLoadingContent();
}

bool
nsHTMLSharedObjectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
nsHTMLSharedObjectElement::DoneAddingChildren(bool aHaveNotified)
{
  if (!mIsDoneAddingChildren) {
    mIsDoneAddingChildren = true;

    // If we're already in a document, we need to trigger the load
    // Otherwise, BindToTree takes care of that.
    if (IsInDoc()) {
      StartObjectLoad(aHaveNotified);
    }
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLSharedObjectElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLSharedObjectElement,
                                                  nsGenericHTMLElement)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLSharedObjectElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLSharedObjectElement, nsGenericElement) 

DOMCI_DATA(HTMLAppletElement, nsHTMLSharedObjectElement)
DOMCI_DATA(HTMLEmbedElement, nsHTMLSharedObjectElement)

nsIClassInfo*
nsHTMLSharedObjectElement::GetClassInfoInternal()
{
  if (mNodeInfo->Equals(nsGkAtoms::applet)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLAppletElement_id);
  }
  if (mNodeInfo->Equals(nsGkAtoms::embed)) {
    return NS_GetDOMClassInfoInstance(eDOMClassInfo_HTMLEmbedElement_id);
  }
  return nullptr;
}

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLSharedObjectElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_AMBIGUOUS_BEGIN(nsHTMLSharedObjectElement,
                                                  nsIDOMHTMLAppletElement)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIRequestObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIStreamListener)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIFrameLoaderOwner)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, imgIContainerObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIObjectLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, imgIDecoderObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIImageLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIInterfaceRequestor)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLSharedObjectElement, nsIChannelEventSink)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE_AMBIGUOUS(nsHTMLSharedObjectElement,
                                                         nsGenericHTMLElement,
                                                         nsIDOMHTMLAppletElement)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLAppletElement, applet)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMHTMLEmbedElement, embed)
  NS_INTERFACE_MAP_ENTRY_IF_TAG(nsIDOMGetSVGDocument, embed)
  NS_DOM_INTERFACE_MAP_ENTRY_CLASSINFO_GETTER(GetClassInfoInternal)
NS_HTML_CONTENT_INTERFACE_MAP_END

NS_IMPL_ELEMENT_CLONE(nsHTMLSharedObjectElement)

nsresult
nsHTMLSharedObjectElement::BindToTree(nsIDocument *aDocument,
                                      nsIContent *aParent,
                                      nsIContent *aBindingParent,
                                      bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  rv = nsObjectLoadingContent::BindToTree(aDocument, aParent,
                                          aBindingParent,
                                          aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  // If we already have all the children, start the load.
  if (mIsDoneAddingChildren) {
    void (nsHTMLSharedObjectElement::*start)() =
      &nsHTMLSharedObjectElement::StartObjectLoad;
    nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, start));
  }

  return NS_OK;
}

void
nsHTMLSharedObjectElement::UnbindFromTree(bool aDeep,
                                          bool aNullParent)
{
  nsObjectLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}


nsresult
nsHTMLSharedObjectElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom *aName,
                                   nsIAtom *aPrefix, const nsAString &aValue,
                                   bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  // if aNotify is false, we are coming from the parser or some such place;
  // we'll get bound after all the attributes have been set, so we'll do the
  // object load from BindToTree/DoneAddingChildren.
  // Skip the LoadObject call in that case.
  // We also don't want to start loading the object when we're not yet in
  // a document, just in case that the caller wants to set additional
  // attributes before inserting the node into the document.
  if (aNotify && IsInDoc() && mIsDoneAddingChildren &&
      aNameSpaceID == kNameSpaceID_None && aName == URIAttrName()) {
    return LoadObject(aNotify, true);
  }

  return NS_OK;
}

bool
nsHTMLSharedObjectElement::IsHTMLFocusable(bool aWithMouse,
                                           bool *aIsFocusable,
                                           PRInt32 *aTabIndex)
{
  if (mNodeInfo->Equals(nsGkAtoms::embed) || Type() == eType_Plugin) {
    // Has plugin content: let the plugin decide what to do in terms of
    // internal focus from mouse clicks
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }

    *aIsFocusable = true;

    // Let the plugin decide, so override.
    return true;
  }

  return nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex);
}

nsIContent::IMEState
nsHTMLSharedObjectElement::GetDesiredIMEState()
{
  if (Type() == eType_Plugin) {
    return IMEState(IMEState::PLUGIN);
  }
   
  return nsGenericHTMLElement::GetDesiredIMEState();
}

NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Alt, alt)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Code, code)
NS_IMPL_URI_ATTR(nsHTMLSharedObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Height, height)
NS_IMPL_INT_ATTR(nsHTMLSharedObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Name, name)
NS_IMPL_URI_ATTR_WITH_BASE(nsHTMLSharedObjectElement, Object, object, codebase)
NS_IMPL_URI_ATTR(nsHTMLSharedObjectElement, Src, src)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLSharedObjectElement, TabIndex, tabindex, -1)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Type, type)
NS_IMPL_INT_ATTR(nsHTMLSharedObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLSharedObjectElement, Width, width)

NS_IMETHODIMP
nsHTMLSharedObjectElement::GetSVGDocument(nsIDOMDocument **aResult)
{
  NS_ENSURE_ARG_POINTER(aResult);

  *aResult = nullptr;

  if (!IsInDoc()) {
    return NS_OK;
  }

  // XXXbz should this use GetCurrentDoc()?  sXBL/XBL2 issue!
  nsIDocument *sub_doc = OwnerDoc()->GetSubDocumentFor(this);
  if (!sub_doc) {
    return NS_OK;
  }

  return CallQueryInterface(sub_doc, aResult);
}

bool
nsHTMLSharedObjectElement::ParseAttribute(PRInt32 aNamespaceID,
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

  return nsGenericHTMLElement::ParseAttribute(aNamespaceID, aAttribute, aValue,
                                              aResult);
}

static void
MapAttributesIntoRule(const nsMappedAttributes *aAttributes,
                      nsRuleData *aData)
{
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aData);
}

static void
EmbedMapAttributesIntoRule(const nsMappedAttributes *aAttributes,
                           nsRuleData *aData)
{
  // NOTE: this should call the exact some methods than MapAttributesIntoRule
  // except that MapCommonAttributesExceptHiddenInto is called instead of
  // MapCommonAttributesInto.
  // TODO: This method should be removed when bug 614825 will be fixed.
  nsGenericHTMLElement::MapImageBorderAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageMarginAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageSizeAttributesInto(aAttributes, aData);
  nsGenericHTMLElement::MapImageAlignAttributeInto(aAttributes, aData);
  nsGenericHTMLElement::MapCommonAttributesExceptHiddenInto(aAttributes, aData);
}

NS_IMETHODIMP_(bool)
nsHTMLSharedObjectElement::IsAttributeMapped(const nsIAtom *aAttribute) const
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
nsHTMLSharedObjectElement::GetAttributeMappingFunction() const
{
  if (mNodeInfo->Equals(nsGkAtoms::embed)) {
    return &EmbedMapAttributesIntoRule;
  }

  return &MapAttributesIntoRule;
}

void
nsHTMLSharedObjectElement::StartObjectLoad(bool aNotify)
{
  LoadObject(aNotify);
  SetIsNetworkCreated(false);
}

nsEventStates
nsHTMLSharedObjectElement::IntrinsicState() const
{
  return nsGenericHTMLElement::IntrinsicState() | ObjectState();
}

PRUint32
nsHTMLSharedObjectElement::GetCapabilities() const
{
  PRUint32 capabilities = eSupportPlugins | eAllowPluginSkipChannel;
  if (mNodeInfo->Equals(nsGkAtoms::embed)) {
    capabilities |= eSupportSVG | eSupportImages;
  }

  return capabilities;
}

void
nsHTMLSharedObjectElement::DestroyContent()
{
  nsObjectLoadingContent::DestroyContent();
  nsGenericHTMLElement::DestroyContent();
}

nsresult
nsHTMLSharedObjectElement::CopyInnerTo(nsGenericElement* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<nsHTMLSharedObjectElement*>(aDest));
  }

  return rv;
}
