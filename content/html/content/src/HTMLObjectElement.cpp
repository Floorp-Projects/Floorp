/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsAutoPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsAttrValueInlines.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsError.h"
#include "nsIDocument.h"
#include "nsIPluginDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsFormSubmission.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIConstraintValidation.h"
#include "nsIWidget.h"

namespace mozilla {
namespace dom {

class HTMLObjectElement MOZ_FINAL : public nsGenericHTMLFormElement
                                  , public nsObjectLoadingContent
                                  , public nsIDOMHTMLObjectElement
                                  , public nsIConstraintValidation
                                  , public nsIDOMGetSVGDocument
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  HTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                    FromParser aFromParser = NOT_FROM_PARSER);
  virtual ~HTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE_TO_NSINODE

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT_TO_GENERIC

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_TO_GENERIC

  virtual int32_t TabIndexDefault() MOZ_OVERRIDE;

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              bool aCompileEventHandlers);
  virtual void UnbindFromTree(bool aDeep = true,
                              bool aNullParent = true);
  virtual nsresult SetAttr(int32_t aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           bool aNotify);
  virtual nsresult UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, int32_t *aTabIndex);
  virtual IMEState GetDesiredIMEState();

  // Overriden nsIFormControl methods
  NS_IMETHOD_(uint32_t) GetType() const
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission);

  virtual bool IsDisabled() const { return false; }

  virtual void DoneAddingChildren(bool aHaveNotified);
  virtual bool IsDoneAddingChildren();

  virtual bool ParseAttribute(int32_t aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(bool) IsAttributeMapped(const nsIAtom *aAttribute) const;
  virtual nsEventStates IntrinsicState() const;
  virtual void DestroyContent();

  // nsObjectLoadingContent
  virtual uint32_t GetCapabilities() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(Element* aDest);

  void StartObjectLoad() { StartObjectLoad(true); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(HTMLObjectElement,
                                                     nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();

  virtual nsIDOMNode* AsDOMNode() { return this; }
private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  NS_HIDDEN_(void) StartObjectLoad(bool aNotify);

  /**
   * Returns if the element is currently focusable regardless of it's tabindex
   * value. This is used to know the default tabindex value.
   */
  bool IsFocusableForTabIndex();
  
  virtual void GetItemValueText(nsAString& text);
  virtual void SetItemValueText(const nsAString& text);

  bool mIsDoneAddingChildren;
};

HTMLObjectElement::HTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                     FromParser aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mIsDoneAddingChildren(!aFromParser)
{
  RegisterFreezableElement();
  SetIsNetworkCreated(aFromParser == FROM_PARSER_NETWORK);

  // <object> is always barred from constraint validation.
  SetBarredFromConstraintValidation(true);

  // By default we're in the loading state
  AddStatesSilently(NS_EVENT_STATE_LOADING);
}

HTMLObjectElement::~HTMLObjectElement()
{
  UnregisterFreezableElement();
  DestroyImageLoadingContent();
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
  if (IsInDoc()) {
    StartObjectLoad(aHaveNotified);
  }
}

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(HTMLObjectElement,
                                                  nsGenericHTMLFormElement)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(HTMLObjectElement, Element)
NS_IMPL_RELEASE_INHERITED(HTMLObjectElement, Element)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(HTMLObjectElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(HTMLObjectElement)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIDOMHTMLObjectElement)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, imgINotificationObserver)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIRequestObserver)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIStreamListener)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIFrameLoaderOwner)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIObjectLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIImageLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, imgIOnloadBlocker)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIInterfaceRequestor)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIChannelEventSink)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIConstraintValidation)
    NS_INTERFACE_TABLE_ENTRY(HTMLObjectElement, nsIDOMGetSVGDocument)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(HTMLObjectElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLObjectElement)

NS_IMPL_ELEMENT_CLONE(HTMLObjectElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION(HTMLObjectElement)

NS_IMETHODIMP
HTMLObjectElement::GetForm(nsIDOMHTMLFormElement **aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

void
HTMLObjectElement::GetItemValueText(nsAString& aValue)
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
  if (aNotify && IsInDoc() && mIsDoneAddingChildren &&
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
  if (aNotify && IsInDoc() && mIsDoneAddingChildren &&
      aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::data) {
    return LoadObject(aNotify, true);
  }

  return NS_OK;
}

bool
HTMLObjectElement::IsFocusableForTabIndex()
{
  nsIDocument* doc = GetCurrentDoc();
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
  nsIDocument *doc = GetCurrentDoc();
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

  *aContentDocument = nullptr;

  if (!IsInDoc()) {
    return NS_OK;
  }

  // XXXbz should this use GetCurrentDoc()?  sXBL/XBL2 issue!
  nsIDocument *sub_doc = OwnerDoc()->GetSubDocumentFor(this);
  if (!sub_doc) {
    return NS_OK;
  }

  return CallQueryInterface(sub_doc, aContentDocument);
}

NS_IMETHODIMP
HTMLObjectElement::GetSVGDocument(nsIDOMDocument **aResult)
{
  return GetContentDocument(aResult);
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

static void
MapAttributesIntoRule(const nsMappedAttributes *aAttributes,
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
  if (!IsInDoc() || !OwnerDoc()->IsActive()) {
    return;
  }

  LoadObject(aNotify);
  SetIsNetworkCreated(false);
}

nsEventStates
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

} // namespace dom
} // namespace mozilla

DOMCI_NODE_DATA(HTMLObjectElement, mozilla::dom::HTMLObjectElement)

NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)
