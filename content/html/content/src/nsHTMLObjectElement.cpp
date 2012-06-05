/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "mozilla/Util.h"

#include "nsAutoPtr.h"
#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsDOMError.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMSVGDocument.h"
#include "nsIDOMGetSVGDocument.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsFormSubmission.h"
#include "nsIObjectFrame.h"
#include "nsNPAPIPluginInstance.h"
#include "nsIConstraintValidation.h"
#include "nsIWidget.h"

using namespace mozilla;
using namespace mozilla::dom;

class nsHTMLObjectElement : public nsGenericHTMLFormElement
                          , public nsObjectLoadingContent
                          , public nsIDOMHTMLObjectElement
                          , public nsIConstraintValidation
                          , public nsIDOMGetSVGDocument
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      mozilla::dom::FromParser aFromParser = mozilla::dom::NOT_FROM_PARSER);
  virtual ~nsHTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT_BASIC(nsGenericHTMLFormElement::)
  NS_SCRIPTABLE NS_IMETHOD Click() {
    return nsGenericHTMLFormElement::Click();
  }
  NS_SCRIPTABLE NS_IMETHOD GetTabIndex(PRInt32* aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD SetTabIndex(PRInt32 aTabIndex);
  NS_SCRIPTABLE NS_IMETHOD Focus() {
    return nsGenericHTMLFormElement::Focus();
  }
  NS_SCRIPTABLE NS_IMETHOD GetDraggable(bool* aDraggable) {
    return nsGenericHTMLFormElement::GetDraggable(aDraggable);
  }
  NS_SCRIPTABLE NS_IMETHOD GetInnerHTML(nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::GetInnerHTML(aInnerHTML);
  }
  NS_SCRIPTABLE NS_IMETHOD SetInnerHTML(const nsAString& aInnerHTML) {
    return nsGenericHTMLFormElement::SetInnerHTML(aInnerHTML);
  }

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

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
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             bool aNotify);

  virtual bool IsHTMLFocusable(bool aWithMouse, bool *aIsFocusable, PRInt32 *aTabIndex);
  virtual IMEState GetDesiredIMEState();

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRUint32) GetType() const
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission);

  virtual bool IsDisabled() const { return false; }

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

  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  void StartObjectLoad() { StartObjectLoad(true); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLObjectElement,
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


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)


nsHTMLObjectElement::nsHTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
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

nsHTMLObjectElement::~nsHTMLObjectElement()
{
  UnregisterFreezableElement();
  DestroyImageLoadingContent();
}

bool
nsHTMLObjectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

void
nsHTMLObjectElement::DoneAddingChildren(bool aHaveNotified)
{
  mIsDoneAddingChildren = true;

  // If we're already in a document, we need to trigger the load
  // Otherwise, BindToTree takes care of that.
  if (IsInDoc()) {
    StartObjectLoad(aHaveNotified);
  }
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLObjectElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLObjectElement,
                                                  nsGenericHTMLFormElement)
  nsObjectLoadingContent::Traverse(tmp, cb);
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsHTMLObjectElement, nsGenericElement) 
NS_IMPL_RELEASE_INHERITED(nsHTMLObjectElement, nsGenericElement) 

DOMCI_NODE_DATA(HTMLObjectElement, nsHTMLObjectElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsHTMLObjectElement)
  NS_HTML_CONTENT_INTERFACE_TABLE_BEGIN(nsHTMLObjectElement)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIDOMHTMLObjectElement)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, imgIDecoderObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIRequestObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIStreamListener)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIFrameLoaderOwner)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIObjectLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIImageLoadingContent)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, imgIContainerObserver)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIInterfaceRequestor)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIChannelEventSink)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIConstraintValidation)
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIDOMGetSVGDocument)
  NS_OFFSET_AND_INTERFACE_TABLE_END
  NS_HTML_CONTENT_INTERFACE_TABLE_TO_MAP_SEGUE(nsHTMLObjectElement,
                                               nsGenericHTMLFormElement)
NS_HTML_CONTENT_INTERFACE_TABLE_TAIL_CLASSINFO(HTMLObjectElement)

NS_IMPL_ELEMENT_CLONE(nsHTMLObjectElement)

// nsIConstraintValidation
NS_IMPL_NSICONSTRAINTVALIDATION(nsHTMLObjectElement)

NS_IMETHODIMP
nsHTMLObjectElement::GetForm(nsIDOMHTMLFormElement **aForm)
{
  return nsGenericHTMLFormElement::GetForm(aForm);
}

void
nsHTMLObjectElement::GetItemValueText(nsAString& aValue)
{
  GetData(aValue);
}

void
nsHTMLObjectElement::SetItemValueText(const nsAString& aValue)
{
  SetData(aValue);
}

nsresult
nsHTMLObjectElement::BindToTree(nsIDocument *aDocument,
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

  // If we already have all the children, start the load.
  if (mIsDoneAddingChildren) {
    void (nsHTMLObjectElement::*start)() = &nsHTMLObjectElement::StartObjectLoad;
    nsContentUtils::AddScriptRunner(NS_NewRunnableMethod(this, start));
  }

  return NS_OK;
}

void
nsHTMLObjectElement::UnbindFromTree(bool aDeep,
                                    bool aNullParent)
{
  RemovedFromDocument();
  nsObjectLoadingContent::UnbindFromTree(aDeep, aNullParent);
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}



nsresult
nsHTMLObjectElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom *aName,
                             nsIAtom *aPrefix, const nsAString &aValue,
                             bool aNotify)
{
  // If we plan to call LoadObject, we want to do it first so that the
  // object load kicks off _before_ the reflow triggered by the SetAttr.  But if
  // aNotify is false, we are coming from the parser or some such place; we'll
  // get bound after all the attributes have been set, so we'll do the
  // object load from BindToTree/DoneAddingChildren.
  // Skip the LoadObject call in that case.
  // We also don't want to start loading the object when we're not yet in
  // a document, just in case that the caller wants to set additional
  // attributes before inserting the node into the document.
  if (aNotify && IsInDoc() && mIsDoneAddingChildren &&
      aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::data) {
    nsAutoString type;
    GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
    LoadObject(aValue, aNotify, NS_ConvertUTF16toUTF8(type), true);
  }

  return nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                           aValue, aNotify);
}

nsresult
nsHTMLObjectElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               bool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::data) {
    Fallback(aNotify);
  }

  return nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

bool
nsHTMLObjectElement::IsFocusableForTabIndex()
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc || doc->HasFlag(NODE_IS_EDITABLE)) {
    return false;
  }

  return IsEditableRoot() || (Type() == eType_Document &&
                              nsContentUtils::IsSubDocumentTabbable(this));
}

bool
nsHTMLObjectElement::IsHTMLFocusable(bool aWithMouse,
                                     bool *aIsFocusable, PRInt32 *aTabIndex)
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
nsHTMLObjectElement::GetDesiredIMEState()
{
  if (Type() == eType_Plugin) {
    return IMEState(IMEState::PLUGIN);
  }
   
  return nsGenericHTMLFormElement::GetDesiredIMEState();
}

NS_IMETHODIMP
nsHTMLObjectElement::Reset()
{
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLObjectElement::SubmitNamesValues(nsFormSubmission *aFormSubmission)
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

NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Align, align)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Archive, archive)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Border, border)
NS_IMPL_URI_ATTR_WITH_BASE(nsHTMLObjectElement, Code, code, codebase)
NS_IMPL_URI_ATTR(nsHTMLObjectElement, CodeBase, codebase)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, CodeType, codetype)
NS_IMPL_URI_ATTR_WITH_BASE(nsHTMLObjectElement, Data, data, codebase)
NS_IMPL_BOOL_ATTR(nsHTMLObjectElement, Declare, declare)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Height, height)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, Hspace, hspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Name, name)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Standby, standby)
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLObjectElement, TabIndex, tabindex,
                               IsFocusableForTabIndex() ? 0 : -1)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Type, type)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, UseMap, usemap)
NS_IMPL_INT_ATTR(nsHTMLObjectElement, Vspace, vspace)
NS_IMPL_STRING_ATTR(nsHTMLObjectElement, Width, width)

NS_IMETHODIMP
nsHTMLObjectElement::GetContentDocument(nsIDOMDocument **aContentDocument)
{
  NS_ENSURE_ARG_POINTER(aContentDocument);

  *aContentDocument = nsnull;

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
nsHTMLObjectElement::GetSVGDocument(nsIDOMDocument **aResult)
{
  return GetContentDocument(aResult);
}

bool
nsHTMLObjectElement::ParseAttribute(PRInt32 aNamespaceID,
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
nsHTMLObjectElement::IsAttributeMapped(const nsIAtom *aAttribute) const
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
nsHTMLObjectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

void
nsHTMLObjectElement::StartObjectLoad(bool aNotify)
{
  nsAutoString type;
  GetAttr(kNameSpaceID_None, nsGkAtoms::type, type);
  NS_ConvertUTF16toUTF8 ctype(type);

  nsAutoString uri;
  if (GetAttr(kNameSpaceID_None, nsGkAtoms::data, uri)) {
    LoadObject(uri, aNotify, ctype);
  }
  else {
    // Be sure to call the nsIURI version if we have no attribute
    // That handles the case where no URI is specified. An empty string would
    // get interpreted as the page itself, instead of absence of URI.
    LoadObject(nsnull, aNotify, ctype);
  }
  SetIsNetworkCreated(false);
}

nsEventStates
nsHTMLObjectElement::IntrinsicState() const
{
  return nsGenericHTMLFormElement::IntrinsicState() | ObjectState();
}

PRUint32
nsHTMLObjectElement::GetCapabilities() const
{
  return nsObjectLoadingContent::GetCapabilities() | eSupportClassID;
}

void
nsHTMLObjectElement::DestroyContent()
{
  RemovedFromDocument();
  nsGenericHTMLFormElement::DestroyContent();
}

nsresult
nsHTMLObjectElement::CopyInnerTo(nsGenericElement* aDest) const
{
  nsresult rv = nsGenericHTMLFormElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDest->OwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<nsHTMLObjectElement*>(aDest));
  }

  return rv;
}
