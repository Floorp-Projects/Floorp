/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set et sw=2 sts=2 cin:
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsGenericHTMLElement.h"
#include "nsObjectLoadingContent.h"
#include "nsGkAtoms.h"
#include "nsDOMError.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#ifdef MOZ_SVG
#include "nsIDOMSVGDocument.h"
#include "nsIDOMGetSVGDocument.h"
#endif
#include "nsIDOMHTMLObjectElement.h"
#include "nsFormSubmission.h"
#include "nsIObjectFrame.h"
#include "nsIPluginInstance.h"
#include "nsIConstraintValidation.h"


class nsHTMLObjectElement : public nsGenericHTMLFormElement,
                            public nsObjectLoadingContent,
                            public nsIDOMHTMLObjectElement,
                            public nsIConstraintValidation
#ifdef MOZ_SVG
                            , public nsIDOMGetSVGDocument
#endif
{
public:
  using nsIConstraintValidation::GetValidationMessage;

  nsHTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                      PRUint32 aFromParser = 0);
  virtual ~nsHTMLObjectElement();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_FORWARD_NSIDOMNODE(nsGenericHTMLFormElement::)

  // nsIDOMElement
  NS_FORWARD_NSIDOMELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLElement
  NS_FORWARD_NSIDOMHTMLELEMENT(nsGenericHTMLFormElement::)

  // nsIDOMHTMLObjectElement
  NS_DECL_NSIDOMHTMLOBJECTELEMENT

#ifdef MOZ_SVG
  // nsIDOMGetSVGDocument
  NS_DECL_NSIDOMGETSVGDOCUMENT
#endif

  virtual nsresult BindToTree(nsIDocument *aDocument, nsIContent *aParent,
                              nsIContent *aBindingParent,
                              PRBool aCompileEventHandlers);
  virtual void UnbindFromTree(PRBool aDeep = PR_TRUE,
                              PRBool aNullParent = PR_TRUE);
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom *aName,
                           nsIAtom *aPrefix, const nsAString &aValue,
                           PRBool aNotify);
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                             PRBool aNotify);

  virtual PRBool IsHTMLFocusable(PRBool aWithMouse, PRBool *aIsFocusable, PRInt32 *aTabIndex);
  virtual PRUint32 GetDesiredIMEState();

  // Overriden nsIFormControl methods
  NS_IMETHOD_(PRUint32) GetType() const
  {
    return NS_FORM_OBJECT;
  }

  NS_IMETHOD Reset();
  NS_IMETHOD SubmitNamesValues(nsFormSubmission *aFormSubmission);

  virtual bool IsDisabled() const { return PR_FALSE; }

  virtual nsresult DoneAddingChildren(PRBool aHaveNotified);
  virtual PRBool IsDoneAddingChildren();

  virtual PRBool ParseAttribute(PRInt32 aNamespaceID,
                                nsIAtom *aAttribute,
                                const nsAString &aValue,
                                nsAttrValue &aResult);
  virtual nsMapRuleToAttributesFunc GetAttributeMappingFunction() const;
  NS_IMETHOD_(PRBool) IsAttributeMapped(const nsIAtom *aAttribute) const;
  virtual PRInt32 IntrinsicState() const;
  virtual void DestroyContent();

  // nsObjectLoadingContent
  virtual PRUint32 GetCapabilities() const;

  virtual nsresult Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const;

  nsresult CopyInnerTo(nsGenericElement* aDest) const;

  void StartObjectLoad() { StartObjectLoad(PR_TRUE); }

  NS_DECL_CYCLE_COLLECTION_CLASS_INHERITED_NO_UNLINK(nsHTMLObjectElement,
                                                     nsGenericHTMLFormElement)

  virtual nsXPCClassInfo* GetClassInfo();
private:
  /**
   * Calls LoadObject with the correct arguments to start the plugin load.
   */
  NS_HIDDEN_(void) StartObjectLoad(PRBool aNotify);

  PRPackedBool mIsDoneAddingChildren;
};


NS_IMPL_NS_NEW_HTML_ELEMENT_CHECK_PARSER(Object)


nsHTMLObjectElement::nsHTMLObjectElement(already_AddRefed<nsINodeInfo> aNodeInfo,
                                         PRUint32 aFromParser)
  : nsGenericHTMLFormElement(aNodeInfo),
    mIsDoneAddingChildren(!aFromParser)
{
  RegisterFreezableElement();
  SetIsNetworkCreated(aFromParser == NS_FROM_PARSER_NETWORK);

  // <object> is always barred from constraint validation.
  SetBarredFromConstraintValidation(PR_TRUE);
}

nsHTMLObjectElement::~nsHTMLObjectElement()
{
  UnregisterFreezableElement();
  DestroyImageLoadingContent();
}

PRBool
nsHTMLObjectElement::IsDoneAddingChildren()
{
  return mIsDoneAddingChildren;
}

nsresult
nsHTMLObjectElement::DoneAddingChildren(PRBool aHaveNotified)
{
  mIsDoneAddingChildren = PR_TRUE;

  // If we're already in a document, we need to trigger the load
  // Otherwise, BindToTree takes care of that.
  if (IsInDoc()) {
    StartObjectLoad(aHaveNotified);
  }
  return NS_OK;
}

NS_IMPL_CYCLE_COLLECTION_CLASS(nsHTMLObjectElement)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsHTMLObjectElement,
                                                  nsGenericHTMLFormElement)
  tmp->Traverse(cb);
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
#ifdef MOZ_SVG
    NS_INTERFACE_TABLE_ENTRY(nsHTMLObjectElement, nsIDOMGetSVGDocument)
#endif
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

nsresult
nsHTMLObjectElement::BindToTree(nsIDocument *aDocument,
                                nsIContent *aParent,
                                nsIContent *aBindingParent,
                                PRBool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLFormElement::BindToTree(aDocument, aParent,
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
nsHTMLObjectElement::UnbindFromTree(PRBool aDeep,
                                    PRBool aNullParent)
{
  RemovedFromDocument();
  nsGenericHTMLFormElement::UnbindFromTree(aDeep, aNullParent);
}



nsresult
nsHTMLObjectElement::SetAttr(PRInt32 aNameSpaceID, nsIAtom *aName,
                             nsIAtom *aPrefix, const nsAString &aValue,
                             PRBool aNotify)
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
    LoadObject(aValue, aNotify, NS_ConvertUTF16toUTF8(type), PR_TRUE);
  }

  return nsGenericHTMLFormElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                           aValue, aNotify);
}

nsresult
nsHTMLObjectElement::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                               PRBool aNotify)
{
  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::data) {
    Fallback(aNotify);
  }

  return nsGenericHTMLFormElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
}

PRBool
nsHTMLObjectElement::IsHTMLFocusable(PRBool aWithMouse,
                                     PRBool *aIsFocusable, PRInt32 *aTabIndex)
{
  if (Type() == eType_Plugin) {
    // Has plugin content: let the plugin decide what to do in terms of
    // internal focus from mouse clicks
    if (aTabIndex) {
      GetTabIndex(aTabIndex);
    }
  
    *aIsFocusable = PR_TRUE;

    return PR_FALSE;
  }

  return nsGenericHTMLFormElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex);
}

PRUint32
nsHTMLObjectElement::GetDesiredIMEState()
{
  if (Type() == eType_Plugin) {
    return nsIContent::IME_STATUS_PLUGIN;
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

  nsCOMPtr<nsIPluginInstance> pi;
  objFrame->GetPluginInstance(*getter_AddRefs(pi));
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
NS_IMPL_INT_ATTR_DEFAULT_VALUE(nsHTMLObjectElement, TabIndex, tabindex, 0)
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
  nsIDocument *sub_doc = GetOwnerDoc()->GetSubDocumentFor(this);
  if (!sub_doc) {
    return NS_OK;
  }

  return CallQueryInterface(sub_doc, aContentDocument);
}

#ifdef MOZ_SVG
NS_IMETHODIMP
nsHTMLObjectElement::GetSVGDocument(nsIDOMDocument **aResult)
{
  return GetContentDocument(aResult);
}
#endif

PRBool
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
      return PR_TRUE;
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

NS_IMETHODIMP_(PRBool)
nsHTMLObjectElement::IsAttributeMapped(const nsIAtom *aAttribute) const
{
  static const MappedAttributeEntry* const map[] = {
    sCommonAttributeMap,
    sImageMarginSizeAttributeMap,
    sImageBorderAttributeMap,
    sImageAlignAttributeMap,
  };

  return FindAttributeDependence(aAttribute, map, NS_ARRAY_LENGTH(map));
}


nsMapRuleToAttributesFunc
nsHTMLObjectElement::GetAttributeMappingFunction() const
{
  return &MapAttributesIntoRule;
}

void
nsHTMLObjectElement::StartObjectLoad(PRBool aNotify)
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
  SetIsNetworkCreated(PR_FALSE);
}

PRInt32
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

  if (aDest->GetOwnerDoc()->IsStaticDocument()) {
    CreateStaticClone(static_cast<nsHTMLObjectElement*>(aDest));
  }

  return rv;
}
