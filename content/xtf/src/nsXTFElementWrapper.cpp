/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
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
 * The Original Code is the Mozilla XTF project.
 *
 * The Initial Developer of the Original Code is
 * Alex Fritze.
 * Portions created by the Initial Developer are Copyright (C) 2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alex Fritze <alex@croczilla.com> (original author)
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

#include "nsXTFElementWrapper.h"
#include "nsIXTFElement.h"
#include "nsCOMPtr.h"
#include "nsString.h"
#include "nsXTFInterfaceAggregator.h"
#include "nsIClassInfo.h"
#include "nsPIDOMWindow.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIDocument.h"
#include "nsGkAtoms.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsIEventStateManager.h"
#include "nsIEventListenerManager.h"
#include "nsIDOMEvent.h"
#include "nsGUIEvent.h"
#include "nsContentUtils.h"
#include "nsIXTFService.h"
#include "nsIDOMAttr.h"
#include "nsIAttribute.h"
#include "nsDOMAttributeMap.h"
#include "nsUnicharUtils.h"
#include "nsEventDispatcher.h"
#include "nsIProgrammingLanguage.h"
#include "nsIXPConnect.h"
#include "nsXTFWeakTearoff.h"

nsXTFElementWrapper::nsXTFElementWrapper(nsINodeInfo* aNodeInfo,
                                         nsIXTFElement* aXTFElement)
    : nsXTFElementWrapperBase(aNodeInfo),
      mXTFElement(aXTFElement),
      mNotificationMask(0),
      mIntrinsicState(0),
      mTmpAttrName(nsGkAtoms::_asterix) // XXX this is a hack, but names
                                            // have to have a value
{
}

nsXTFElementWrapper::~nsXTFElementWrapper()
{
  mXTFElement->OnDestroyed();
  mXTFElement = nsnull;
}

nsresult
nsXTFElementWrapper::Init()
{
  // pass a weak wrapper (non base object ref-counted), so that
  // our mXTFElement can safely addref/release.
  nsISupports* weakWrapper = nsnull;
  nsresult rv = NS_NewXTFWeakTearoff(NS_GET_IID(nsIXTFElementWrapper),
                                     (nsIXTFElementWrapper*)this,
                                     &weakWrapper);
  NS_ENSURE_SUCCESS(rv, rv);

  mXTFElement->OnCreated(NS_STATIC_CAST(nsIXTFElementWrapper*, weakWrapper));
  weakWrapper->Release();

  PRBool innerHandlesAttribs = PR_FALSE;
  GetXTFElement()->GetIsAttributeHandler(&innerHandlesAttribs);
  if (innerHandlesAttribs)
    mAttributeHandler = do_QueryInterface(GetXTFElement());
  return NS_OK;
}

//----------------------------------------------------------------------
// nsISupports implementation

NS_IMPL_ADDREF_INHERITED(nsXTFElementWrapper,nsXTFElementWrapperBase)
NS_IMPL_RELEASE_INHERITED(nsXTFElementWrapper,nsXTFElementWrapperBase)

NS_IMETHODIMP
nsXTFElementWrapper::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  nsresult rv;
  
  if(aIID.Equals(NS_GET_IID(nsIClassInfo))) {
    *aInstancePtr = NS_STATIC_CAST(nsIClassInfo*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if(aIID.Equals(NS_GET_IID(nsIXTFElementWrapper))) {
    *aInstancePtr = NS_STATIC_CAST(nsIXTFElementWrapper*, this);
    NS_ADDREF_THIS();
    return NS_OK;
  }
  else if (NS_SUCCEEDED(rv = nsXTFElementWrapperBase::QueryInterface(aIID, aInstancePtr))) {
    return rv;
  }
  else {
    // try to get get the interface from our wrapped element:
    nsCOMPtr<nsISupports> inner;
    QueryInterfaceInner(aIID, getter_AddRefs(inner));

    if (inner) {
      rv = NS_NewXTFInterfaceAggregator(aIID, inner,
                                        NS_STATIC_CAST(nsIContent*, this),
                                        aInstancePtr);

      return rv;
    }
  }

  return NS_ERROR_NO_INTERFACE;
}

//----------------------------------------------------------------------
// nsIContent methods:

nsresult
nsXTFElementWrapper::BindToTree(nsIDocument* aDocument, nsIContent* aParent,
                                nsIContent* aBindingParent,
                                PRBool aCompileEventHandlers)
{
  // XXXbz making up random order for the notifications... Perhaps
  // this api should more closely match BindToTree/UnbindFromTree?
  nsCOMPtr<nsIDOMElement> domParent;
  if (aParent != GetParent()) {
    domParent = do_QueryInterface(aParent);
  }

  nsCOMPtr<nsIDOMDocument> domDocument;
  if (aDocument &&
      (mNotificationMask & (nsIXTFElement::NOTIFY_WILL_CHANGE_DOCUMENT |
                            nsIXTFElement::NOTIFY_DOCUMENT_CHANGED))) {
    domDocument = do_QueryInterface(aDocument);
  }

  if (domDocument &&
      (mNotificationMask & (nsIXTFElement::NOTIFY_WILL_CHANGE_DOCUMENT))) {
    GetXTFElement()->WillChangeDocument(domDocument);
  }

  if (domParent &&
      (mNotificationMask & (nsIXTFElement::NOTIFY_WILL_CHANGE_PARENT))) {
    GetXTFElement()->WillChangeParent(domParent);
  }

  nsresult rv = nsXTFElementWrapperBase::BindToTree(aDocument, aParent,
                                                    aBindingParent,
                                                    aCompileEventHandlers);

  NS_ENSURE_SUCCESS(rv, rv);

  if (mNotificationMask & nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY)
    RegUnregAccessKey(PR_TRUE);

  if (domDocument &&
      (mNotificationMask & (nsIXTFElement::NOTIFY_DOCUMENT_CHANGED))) {
    GetXTFElement()->DocumentChanged(domDocument);
  }

  if (domParent &&
      (mNotificationMask & (nsIXTFElement::NOTIFY_PARENT_CHANGED))) {
    GetXTFElement()->ParentChanged(domParent);
  }

  return rv;  
}

void
nsXTFElementWrapper::UnbindFromTree(PRBool aDeep, PRBool aNullParent)
{
  // XXXbz making up random order for the notifications... Perhaps
  // this api should more closely match BindToTree/UnbindFromTree?

  PRBool inDoc = IsInDoc();
  if (inDoc &&
      (mNotificationMask & nsIXTFElement::NOTIFY_WILL_CHANGE_DOCUMENT)) {
    GetXTFElement()->WillChangeDocument(nsnull);
  }

  PRBool parentChanged = aNullParent && GetParent();

  if (parentChanged &&
      (mNotificationMask & nsIXTFElement::NOTIFY_WILL_CHANGE_PARENT)) {
    GetXTFElement()->WillChangeParent(nsnull);
  }

  if (mNotificationMask & nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY)
    RegUnregAccessKey(PR_FALSE);

  nsXTFElementWrapperBase::UnbindFromTree(aDeep, aNullParent);

  if (parentChanged &&
      (mNotificationMask & nsIXTFElement::NOTIFY_PARENT_CHANGED)) {
    GetXTFElement()->ParentChanged(nsnull);
  }

  if (inDoc &&
      (mNotificationMask & nsIXTFElement::NOTIFY_DOCUMENT_CHANGED)) {
    GetXTFElement()->DocumentChanged(nsnull);
  }
}

nsresult
nsXTFElementWrapper::InsertChildAt(nsIContent* aKid, PRUint32 aIndex,
                                   PRBool aNotify)
{
  nsresult rv;

  nsCOMPtr<nsIDOMNode> domKid;
  if (mNotificationMask & (nsIXTFElement::NOTIFY_WILL_INSERT_CHILD |
                           nsIXTFElement::NOTIFY_CHILD_INSERTED))
    domKid = do_QueryInterface(aKid);
  
  if (mNotificationMask & nsIXTFElement::NOTIFY_WILL_INSERT_CHILD)
    GetXTFElement()->WillInsertChild(domKid, aIndex);
  rv = nsXTFElementWrapperBase::InsertChildAt(aKid, aIndex, aNotify);
  if (mNotificationMask & nsIXTFElement::NOTIFY_CHILD_INSERTED)
    GetXTFElement()->ChildInserted(domKid, aIndex);
  
  return rv;
}

nsresult
nsXTFElementWrapper::RemoveChildAt(PRUint32 aIndex, PRBool aNotify)
{
  nsresult rv;
  if (mNotificationMask & nsIXTFElement::NOTIFY_WILL_REMOVE_CHILD)
    GetXTFElement()->WillRemoveChild(aIndex);
  rv = nsXTFElementWrapperBase::RemoveChildAt(aIndex, aNotify);
  if (mNotificationMask & nsIXTFElement::NOTIFY_CHILD_REMOVED)
    GetXTFElement()->ChildRemoved(aIndex);
  return rv;
}

nsIAtom *
nsXTFElementWrapper::GetIDAttributeName() const
{
  // XXX:
  return nsGkAtoms::id;
}

nsresult
nsXTFElementWrapper::SetAttr(PRInt32 aNameSpaceID, nsIAtom* aName,
                             nsIAtom* aPrefix, const nsAString& aValue,
                             PRBool aNotify)
{
  nsresult rv;

  if (aNameSpaceID == kNameSpaceID_None &&
      (mNotificationMask & nsIXTFElement::NOTIFY_WILL_SET_ATTRIBUTE))
    GetXTFElement()->WillSetAttribute(aName, aValue);

  if (aNameSpaceID==kNameSpaceID_None && HandledByInner(aName)) {
    rv = mAttributeHandler->SetAttribute(aName, aValue);
    // XXX mutation events?
  }
  else { // let wrapper handle it
    rv = nsXTFElementWrapperBase::SetAttr(aNameSpaceID, aName, aPrefix, aValue, aNotify);
  }
  
  if (aNameSpaceID == kNameSpaceID_None &&
      (mNotificationMask & nsIXTFElement::NOTIFY_ATTRIBUTE_SET))
    GetXTFElement()->AttributeSet(aName, aValue);

  if (mNotificationMask & nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY) {
    nsCOMPtr<nsIDOMAttr> accesskey;
    GetXTFElement()->GetAccesskeyNode(getter_AddRefs(accesskey));
    nsCOMPtr<nsIAttribute> attr(do_QueryInterface(accesskey));
    if (attr && attr->NodeInfo()->Equals(aName, aNameSpaceID))
      RegUnregAccessKey(PR_TRUE);
  }

  return rv;
}

PRBool
nsXTFElementWrapper::GetAttr(PRInt32 aNameSpaceID, nsIAtom* aName, 
                             nsAString& aResult) const
{
  if (aNameSpaceID==kNameSpaceID_None && HandledByInner(aName)) {
    // XXX we don't do namespaced attributes yet
    nsresult rv = mAttributeHandler->GetAttribute(aName, aResult);
    return NS_SUCCEEDED(rv) && !aResult.IsVoid();
  }
  else { // try wrapper
    return nsXTFElementWrapperBase::GetAttr(aNameSpaceID, aName, aResult);
  }
}

PRBool
nsXTFElementWrapper::HasAttr(PRInt32 aNameSpaceID, nsIAtom* aName) const
{
  if (aNameSpaceID==kNameSpaceID_None && HandledByInner(aName)) {
    PRBool rval = PR_FALSE;
    mAttributeHandler->HasAttribute(aName, &rval);
    return rval;
  }
  else { // try wrapper
    return nsXTFElementWrapperBase::HasAttr(aNameSpaceID, aName);
  }
}

PRBool
nsXTFElementWrapper::AttrValueIs(PRInt32 aNameSpaceID,
                                 nsIAtom* aName,
                                 const nsAString& aValue,
                                 nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");

  if (aNameSpaceID == kNameSpaceID_None && HandledByInner(aName)) {
    nsAutoString ourVal;
    if (!GetAttr(aNameSpaceID, aName, ourVal)) {
      return PR_FALSE;
    }
    return aCaseSensitive == eCaseMatters ?
      aValue.Equals(ourVal) :
      aValue.Equals(ourVal, nsCaseInsensitiveStringComparator());
  }

  return nsXTFElementWrapperBase::AttrValueIs(aNameSpaceID, aName, aValue,
                                              aCaseSensitive);
}

PRBool
nsXTFElementWrapper::AttrValueIs(PRInt32 aNameSpaceID,
                                 nsIAtom* aName,
                                 nsIAtom* aValue,
                                 nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValue, "Null value atom");

  if (aNameSpaceID == kNameSpaceID_None && HandledByInner(aName)) {
    nsAutoString ourVal;
    if (!GetAttr(aNameSpaceID, aName, ourVal)) {
      return PR_FALSE;
    }
    if (aCaseSensitive == eCaseMatters) {
      return aValue->Equals(ourVal);
    }
    nsAutoString val;
    aValue->ToString(val);
    return val.Equals(ourVal, nsCaseInsensitiveStringComparator());
  }

  return nsXTFElementWrapperBase::AttrValueIs(aNameSpaceID, aName, aValue,
                                              aCaseSensitive);
}

PRInt32
nsXTFElementWrapper::FindAttrValueIn(PRInt32 aNameSpaceID,
                                     nsIAtom* aName,
                                     AttrValuesArray* aValues,
                                     nsCaseTreatment aCaseSensitive) const
{
  NS_ASSERTION(aName, "Must have attr name");
  NS_ASSERTION(aNameSpaceID != kNameSpaceID_Unknown, "Must have namespace");
  NS_ASSERTION(aValues, "Null value array");
  
  if (aNameSpaceID == kNameSpaceID_None && HandledByInner(aName)) {
    nsAutoString ourVal;
    if (!GetAttr(aNameSpaceID, aName, ourVal)) {
      return ATTR_MISSING;
    }
    
    for (PRInt32 i = 0; aValues[i]; ++i) {
      if (aCaseSensitive == eCaseMatters) {
        if ((*aValues[i])->Equals(ourVal)) {
          return i;
        }
      } else {
        nsAutoString val;
        (*aValues[i])->ToString(val);
        if (val.Equals(ourVal, nsCaseInsensitiveStringComparator())) {
          return i;
        }
      }
    }
    return ATTR_VALUE_NO_MATCH;
  }

  return nsXTFElementWrapperBase::FindAttrValueIn(aNameSpaceID, aName, aValues,
                                                  aCaseSensitive);
}

nsresult
nsXTFElementWrapper::UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttr, 
                               PRBool aNotify)
{
  nsresult rv;

  if (aNameSpaceID == kNameSpaceID_None &&
      (mNotificationMask & nsIXTFElement::NOTIFY_WILL_REMOVE_ATTRIBUTE))
    GetXTFElement()->WillRemoveAttribute(aAttr);

  if (mNotificationMask & nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY) {
    nsCOMPtr<nsIDOMAttr> accesskey;
    GetXTFElement()->GetAccesskeyNode(getter_AddRefs(accesskey));
    nsCOMPtr<nsIAttribute> attr(do_QueryInterface(accesskey));
    if (attr && attr->NodeInfo()->Equals(aAttr, aNameSpaceID))
      RegUnregAccessKey(PR_FALSE);
  }

  if (aNameSpaceID==kNameSpaceID_None && HandledByInner(aAttr)) {
    nsDOMSlots *slots = GetExistingDOMSlots();
    if (slots && slots->mAttributeMap) {
      slots->mAttributeMap->DropAttribute(aNameSpaceID, aAttr);
    }
    rv = mAttributeHandler->RemoveAttribute(aAttr);

    // XXX if the RemoveAttribute() call fails, we might end up having removed
    // the attribute from the attribute map even though the attribute is still
    // on the element
    // https://bugzilla.mozilla.org/show_bug.cgi?id=296205

    // XXX mutation events?
  }
  else { // try wrapper
    rv = nsXTFElementWrapperBase::UnsetAttr(aNameSpaceID, aAttr, aNotify);
  }

  if (aNameSpaceID == kNameSpaceID_None &&
      (mNotificationMask & nsIXTFElement::NOTIFY_ATTRIBUTE_REMOVED))
    GetXTFElement()->AttributeRemoved(aAttr);

  return rv;
}

const nsAttrName*
nsXTFElementWrapper::GetAttrNameAt(PRUint32 aIndex) const
{
  PRUint32 innerCount=0;
  if (mAttributeHandler) {
    mAttributeHandler->GetAttributeCount(&innerCount);
  }
  
  if (aIndex < innerCount) {
    nsCOMPtr<nsIAtom> localName;
    nsresult rv = mAttributeHandler->GetAttributeNameAt(aIndex, getter_AddRefs(localName));
    NS_ENSURE_SUCCESS(rv, nsnull);

    NS_CONST_CAST(nsXTFElementWrapper*, this)->mTmpAttrName.SetTo(localName);
    return &mTmpAttrName;
  }
  else { // wrapper handles attrib
    return nsXTFElementWrapperBase::GetAttrNameAt(aIndex - innerCount);
  }
}

PRUint32
nsXTFElementWrapper::GetAttrCount() const
{
  PRUint32 innerCount = 0;
  if (mAttributeHandler) {
    mAttributeHandler->GetAttributeCount(&innerCount);
  }
  // add wrapper attribs
  return innerCount + nsXTFElementWrapperBase::GetAttrCount();
}

void
nsXTFElementWrapper::BeginAddingChildren()
{
  if (mNotificationMask & nsIXTFElement::NOTIFY_BEGIN_ADDING_CHILDREN)
    GetXTFElement()->BeginAddingChildren();
}

nsresult
nsXTFElementWrapper::DoneAddingChildren(PRBool aHaveNotified)
{
  if (mNotificationMask & nsIXTFElement::NOTIFY_DONE_ADDING_CHILDREN)
    GetXTFElement()->DoneAddingChildren();

  return NS_OK;
}

already_AddRefed<nsINodeInfo>
nsXTFElementWrapper::GetExistingAttrNameFromQName(const nsAString& aStr) const
{
  nsINodeInfo* nodeInfo = nsXTFElementWrapperBase::GetExistingAttrNameFromQName(aStr).get();

  // Maybe this attribute is handled by our inner element:
  if (!nodeInfo) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aStr);
    if (HandledByInner(nameAtom)) 
      mNodeInfo->NodeInfoManager()->GetNodeInfo(nameAtom, nsnull, kNameSpaceID_None, &nodeInfo);
  }
  
  return nodeInfo;
}

PRInt32
nsXTFElementWrapper::IntrinsicState() const
{
  return nsXTFElementWrapperBase::IntrinsicState() | mIntrinsicState;
}

void
nsXTFElementWrapper::PerformAccesskey(PRBool aKeyCausesActivation,
                                      PRBool aIsTrustedEvent)
{
  if (mNotificationMask & nsIXTFElement::NOTIFY_PERFORM_ACCESSKEY)
    GetXTFElement()->PerformAccesskey();
}

nsresult
nsXTFElementWrapper::Clone(nsINodeInfo *aNodeInfo, nsINode **aResult) const
{
  *aResult = nsnull;
  nsCOMPtr<nsIContent> it;
  nsContentUtils::GetXTFService()->CreateElement(getter_AddRefs(it),
                                                 aNodeInfo);
  if (!it)
    return NS_ERROR_OUT_OF_MEMORY;

  nsXTFElementWrapper* wrapper =
    NS_STATIC_CAST(nsXTFElementWrapper*, it.get());
  nsresult rv = CopyInnerTo(wrapper);

  if (NS_SUCCEEDED(rv)) {
    if (mAttributeHandler) {
      PRUint32 innerCount = 0;
      mAttributeHandler->GetAttributeCount(&innerCount);
      for (PRUint32 i = 0; i < innerCount; ++i) {
        nsCOMPtr<nsIAtom> attrName;
        mAttributeHandler->GetAttributeNameAt(i, getter_AddRefs(attrName));
        if (attrName) {
          nsAutoString value;
          if (NS_SUCCEEDED(mAttributeHandler->GetAttribute(attrName, value)))
            it->SetAttr(kNameSpaceID_None, attrName, value, PR_TRUE);
        }
      }
    }
    NS_ADDREF(*aResult = it);
  }

  // XXX CloneState should take |const nIDOMElement*|
  wrapper->CloneState(NS_CONST_CAST(nsXTFElementWrapper*, this));
  return rv;
}

//----------------------------------------------------------------------
// nsIDOMElement methods:

NS_IMETHODIMP
nsXTFElementWrapper::GetAttribute(const nsAString& aName, nsAString& aReturn)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
  if (name) {
    GetAttr(name->NamespaceID(), name->LocalName(), aReturn);
    return NS_OK;
  }

  // Maybe this attribute is handled by our inner element:
  if (mAttributeHandler) {
    nsresult rv = nsContentUtils::CheckQName(aName, PR_FALSE);
    NS_ENSURE_SUCCESS(rv, rv);
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
    if (HandledByInner(nameAtom)) {
      GetAttr(kNameSpaceID_None, nameAtom, aReturn);
      return NS_OK;
    }
  }
  
  SetDOMStringToNull(aReturn);
  return NS_OK;
}

NS_IMETHODIMP
nsXTFElementWrapper::RemoveAttribute(const nsAString& aName)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);

  if (name) {
    nsAttrName tmp(*name);
    return UnsetAttr(name->NamespaceID(), name->LocalName(), PR_TRUE);
  }

  // Maybe this attribute is handled by our inner element:
  if (mAttributeHandler) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
    return UnsetAttr(kNameSpaceID_None, nameAtom, PR_TRUE);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsXTFElementWrapper::HasAttribute(const nsAString& aName, PRBool* aReturn)
{
  const nsAttrName* name = InternalGetExistingAttrNameFromQName(aName);
  if (name) {
    *aReturn = PR_TRUE;
    return NS_OK;
  }
  
  // Maybe this attribute is handled by our inner element:
  if (mAttributeHandler) {
    nsCOMPtr<nsIAtom> nameAtom = do_GetAtom(aName);
    *aReturn = HasAttr(kNameSpaceID_None, nameAtom);
    return NS_OK;
  }

  *aReturn = PR_FALSE;
  return NS_OK;
}


//----------------------------------------------------------------------
// nsIClassInfo implementation

/* void getInterfaces (out PRUint32 count, [array, size_is (count), retval] out nsIIDPtr array); */
NS_IMETHODIMP 
nsXTFElementWrapper::GetInterfaces(PRUint32* aCount, nsIID*** aArray)
{
  *aArray = nsnull;
  *aCount = 0;
  PRUint32 baseCount = 0;
  nsIID** baseArray = nsnull;
  PRUint32 xtfCount = 0;
  nsIID** xtfArray = nsnull;

  nsCOMPtr<nsIClassInfo> baseCi =
    NS_GetDOMClassInfoInstance(eDOMClassInfo_Element_id);
  if (baseCi) {
    baseCi->GetInterfaces(&baseCount, &baseArray);
  }

  GetXTFElement()->GetScriptingInterfaces(&xtfCount, &xtfArray);
  if (!xtfCount) {
    *aCount = baseCount;
    *aArray = baseArray;
    return NS_OK;
  } else if (!baseCount) {
    *aCount = xtfCount;
    *aArray = xtfArray;
    return NS_OK;
  }

  PRUint32 count = baseCount + xtfCount;
  nsIID** iids = NS_STATIC_CAST(nsIID**,
                                nsMemory::Alloc(count * sizeof(nsIID*)));
  NS_ENSURE_TRUE(iids, NS_ERROR_OUT_OF_MEMORY);

  PRUint32 i = 0;
  for (; i < baseCount; ++i) {
    iids[i] = NS_STATIC_CAST(nsIID*,
                             nsMemory::Clone(baseArray[i], sizeof(nsIID)));
    if (!iids[i]) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(baseCount, baseArray);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(xtfCount, xtfArray);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, iids);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  for (; i < count; ++i) {
    iids[i] = NS_STATIC_CAST(nsIID*,
                             nsMemory::Clone(xtfArray[i - baseCount], sizeof(nsIID)));
    if (!iids[i]) {
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(baseCount, baseArray);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(xtfCount, xtfArray);
      NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(i, iids);
      return NS_ERROR_OUT_OF_MEMORY;
    }
  }

  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(baseCount, baseArray);
  NS_FREE_XPCOM_ALLOCATED_POINTER_ARRAY(xtfCount, xtfArray);
  *aArray = iids;
  *aCount = count;

  return NS_OK;
}

/* nsISupports getHelperForLanguage (in PRUint32 language); */
NS_IMETHODIMP 
nsXTFElementWrapper::GetHelperForLanguage(PRUint32 language,
                                          nsISupports** aHelper)
{
  *aHelper = nsnull;
  nsCOMPtr<nsIClassInfo> ci = 
    NS_GetDOMClassInfoInstance(eDOMClassInfo_Element_id);
  return
    ci ? ci->GetHelperForLanguage(language, aHelper) : NS_ERROR_NOT_AVAILABLE;
}

/* readonly attribute string contractID; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetContractID(char * *aContractID)
{
  *aContractID = nsnull;
  return NS_OK;
}

/* readonly attribute string classDescription; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetClassDescription(char * *aClassDescription)
{
  *aClassDescription = nsnull;
  return NS_OK;
}

/* readonly attribute nsCIDPtr classID; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetClassID(nsCID * *aClassID)
{
  *aClassID = nsnull;
  return NS_OK;
}

/* readonly attribute PRUint32 implementationLanguage; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetImplementationLanguage(PRUint32 *aImplementationLanguage)
{
  *aImplementationLanguage = nsIProgrammingLanguage::UNKNOWN;
  return NS_OK;
}

/* readonly attribute PRUint32 flags; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetFlags(PRUint32 *aFlags)
{
  *aFlags = nsIClassInfo::DOM_OBJECT;
  return NS_OK;
}

/* [notxpcom] readonly attribute nsCID classIDNoAlloc; */
NS_IMETHODIMP 
nsXTFElementWrapper::GetClassIDNoAlloc(nsCID *aClassIDNoAlloc)
{
  return NS_ERROR_NOT_AVAILABLE;
}

//----------------------------------------------------------------------
// nsIXTFElementWrapper implementation:

/* readonly attribute nsIDOMElement elementNode; */
NS_IMETHODIMP
nsXTFElementWrapper::GetElementNode(nsIDOMElement * *aElementNode)
{
  *aElementNode = (nsIDOMElement*)this;
  NS_ADDREF(*aElementNode);
  return NS_OK;
}

/* readonly attribute nsIDOMElement documentFrameElement; */
NS_IMETHODIMP
nsXTFElementWrapper::GetDocumentFrameElement(nsIDOMElement * *aDocumentFrameElement)
{
  *aDocumentFrameElement = nsnull;
  
  nsIDocument *doc = GetCurrentDoc();
  if (!doc) {
    NS_WARNING("no document");
    return NS_OK;
  }
  nsCOMPtr<nsISupports> container = doc->GetContainer();
  if (!container) {
    NS_ERROR("no docshell");
    return NS_ERROR_FAILURE;
  }
  nsCOMPtr<nsPIDOMWindow> pidomwin = do_GetInterface(container);
  if (!pidomwin) {
    NS_ERROR("no nsPIDOMWindow interface on docshell");
    return NS_ERROR_FAILURE;
  }
  *aDocumentFrameElement = pidomwin->GetFrameElementInternal();
  NS_IF_ADDREF(*aDocumentFrameElement);
  return NS_OK;
}

/* attribute unsigned long notificationMask; */
NS_IMETHODIMP
nsXTFElementWrapper::GetNotificationMask(PRUint32 *aNotificationMask)
{
  *aNotificationMask = mNotificationMask;
  return NS_OK;
}
NS_IMETHODIMP
nsXTFElementWrapper::SetNotificationMask(PRUint32 aNotificationMask)
{
  mNotificationMask = aNotificationMask;
  return NS_OK;
}

//----------------------------------------------------------------------
// implementation helpers:
PRBool
nsXTFElementWrapper::QueryInterfaceInner(REFNSIID aIID, void** result)
{
  // We must ensure that the inner element has a distinct xpconnect
  // identity, so we mustn't aggregate nsIXPConnectWrappedJS:
  if (aIID.Equals(NS_GET_IID(nsIXPConnectWrappedJS))) return PR_FALSE;

  GetXTFElement()->QueryInterface(aIID, result);
  return (*result!=nsnull);
}

PRBool
nsXTFElementWrapper::HandledByInner(nsIAtom *attr) const
{
  PRBool retval = PR_FALSE;
  if (mAttributeHandler)
    mAttributeHandler->HandlesAttribute(attr, &retval);
  return retval;
}

nsresult
nsXTFElementWrapper::PostHandleEvent(nsEventChainPostVisitor& aVisitor)
{
  nsresult rv = NS_OK;
  if (aVisitor.mEventStatus == nsEventStatus_eConsumeNoDefault) {
    return rv;
  }

  if (!aVisitor.mDOMEvent) {
    // We haven't made a DOMEvent yet.  Force making one now.
    if (NS_FAILED(rv = nsEventDispatcher::CreateEvent(aVisitor.mPresContext,
                                                      aVisitor.mEvent,
                                                      EmptyString(),
                                                      &aVisitor.mDOMEvent)))
      return rv;
  }
  if (!aVisitor.mDOMEvent)
    return NS_ERROR_FAILURE;
  
  PRBool defaultHandled = PR_FALSE;
  nsIXTFElement* xtfElement = GetXTFElement();
  if (xtfElement)
    rv = xtfElement->HandleDefault(aVisitor.mDOMEvent, &defaultHandled);
  if (defaultHandled)
    aVisitor.mEventStatus = nsEventStatus_eConsumeNoDefault;
  return rv;
}

NS_IMETHODIMP
nsXTFElementWrapper::SetIntrinsicState(PRInt32 aNewState)
{
  nsIDocument *doc = GetCurrentDoc();
  PRInt32 bits = mIntrinsicState ^ aNewState;
  
  if (!doc || !bits)
    return NS_OK;

  mIntrinsicState = aNewState;
  mozAutoDocUpdate upd(doc, UPDATE_CONTENT_STATE, PR_TRUE);
  doc->ContentStatesChanged(this, nsnull, bits);

  return NS_OK;
}

nsIAtom *
nsXTFElementWrapper::GetClassAttributeName() const
{
  return mClassAttributeName;
}

const nsAttrValue*
nsXTFElementWrapper::GetClasses() const
{
  const nsAttrValue* val = nsnull;
  nsIAtom* clazzAttr = GetClassAttributeName();
  if (clazzAttr) {
    val = mAttrsAndChildren.GetAttr(clazzAttr);
    // This is possibly the first time we need any classes.
    if (val && val->Type() == nsAttrValue::eString) {
      nsAutoString value;
      val->ToString(value);
      nsAttrValue newValue;
      newValue.ParseAtomArray(value);
      NS_CONST_CAST(nsAttrAndChildArray*, &mAttrsAndChildren)->
        SetAndTakeAttr(clazzAttr, newValue);
    }
  }
  return val;
}

nsresult
nsXTFElementWrapper::SetClassAttributeName(nsIAtom* aName)
{
  // The class attribute name can be set only once
  if (mClassAttributeName || !aName)
    return NS_ERROR_FAILURE;
  
  mClassAttributeName = aName;
  return NS_OK;
}

void
nsXTFElementWrapper::RegUnregAccessKey(PRBool aDoReg)
{
  nsIDocument* doc = GetCurrentDoc();
  if (!doc)
    return;

  // Get presentation shell 0
  nsIPresShell *presShell = doc->GetShellAt(0);
  if (!presShell)
    return;

  nsPresContext *presContext = presShell->GetPresContext();
  if (!presContext)
    return;

  nsIEventStateManager *esm = presContext->EventStateManager();
  if (!esm)
    return;

  // Register or unregister as appropriate.
  nsCOMPtr<nsIDOMAttr> accesskeyNode;
  GetXTFElement()->GetAccesskeyNode(getter_AddRefs(accesskeyNode));
  if (!accesskeyNode)
    return;

  nsAutoString accessKey;
  accesskeyNode->GetValue(accessKey);

  if (aDoReg && !accessKey.IsEmpty())
    esm->RegisterAccessKey(this, (PRUint32)accessKey.First());
  else
    esm->UnregisterAccessKey(this, (PRUint32)accessKey.First());
}

nsresult
NS_NewXTFElementWrapper(nsIXTFElement* aXTFElement,
                        nsINodeInfo* aNodeInfo,
                        nsIContent** aResult)
{
  *aResult = nsnull;
   NS_ENSURE_ARG(aXTFElement);

  nsXTFElementWrapper* result = new nsXTFElementWrapper(aNodeInfo, aXTFElement);
  if (!result) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(result);

  nsresult rv = result->Init();
  if (NS_FAILED(rv)) {
    NS_RELEASE(result);
    return rv;
  }

  *aResult = result;
  return NS_OK;
}
