/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
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
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nsGenericElement.h"
#include "nsIDocument.h"
#include "nsIDocument.h"
#include "nsIDOMRange.h"
#include "nsIDOMDocument.h"
#include "nsIDOMDocumentFragment.h"
#include "nsRange.h"

#include "nsISelection.h"
#include "nsIEnumerator.h"

#include "nsCRT.h"
#include "nsIEventStateManager.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDOMEvent.h"
#include "nsIDOMText.h"
#include "nsIDOMScriptObjectFactory.h"
#include "prprf.h"
#include "nsCOMPtr.h"

#include "nsIContent.h"
#include "nsTextFragment.h"
#include "nsVoidArray.h"
#include "nsINameSpaceManager.h"
#include "nsITextContent.h"
#include "nsIURI.h"
#include "nsLayoutAtoms.h"
// XXX share all id's in this dir


class nsAttributeContent : public nsITextContent
{
public:
  nsAttributeContent(nsIContent* aContent, PRInt32 aNameSpaceID,
                     nsIAtom* aAttrName);
  virtual ~nsAttributeContent();

  // nsISupports
  NS_DECL_ISUPPORTS

  // Implementation for nsIContent
  virtual PRBool IsNativeAnonymous() const { return PR_TRUE; }
  virtual void SetNativeAnonymous(PRBool aAnonymous) { }

  virtual void GetNameSpaceID(PRInt32* aID) const
  {
    *aID = kNameSpaceID_None;
  }

  virtual nsIAtom *Tag() const
  {
    return nsLayoutAtoms::textTagName;
  }

  virtual nsINodeInfo *GetNodeInfo() const
  {
    return nsnull;
  }

  virtual nsIAtom *GetIDAttributeName() const
  {
    return nsnull;
  }
  
  virtual nsIAtom * GetClassAttributeName() const
  {
    return nsnull;
  }
  
  virtual already_AddRefed<nsINodeInfo> GetExistingAttrNameFromQName(const nsAString& aStr) const
  {
    return nsnull; 
  }

  virtual nsIContent *GetBindingParent() const {
    return nsnull;
  }

  virtual nsresult SetBindingParent(nsIContent* aParent) {
    return NS_OK;
  }

  virtual PRBool IsContentOfType(PRUint32 aFlags) const {
    return !(aFlags & ~eTEXT);
  }

  virtual nsresult GetListenerManager(nsIEventListenerManager **aResult) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  virtual already_AddRefed<nsIURI> GetBaseURI() const;
  
  virtual nsresult SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, const nsAString& aValue,
                           PRBool aNotify) {  return NS_OK; }
  virtual nsresult SetAttr(nsINodeInfo *aNodeInfo, const nsAString& aValue,
                           PRBool aNotify) {  return NS_OK; }
  virtual nsresult UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify) { return NS_OK; }
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsAString& aResult) const {return NS_CONTENT_ATTR_NOT_THERE; }
  virtual nsresult GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsIAtom** aPrefix, nsAString& aResult) const {return NS_CONTENT_ATTR_NOT_THERE; }
  virtual PRBool HasAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute) const {
    return PR_FALSE;
  }
  virtual nsresult GetAttrNameAt(PRUint32 aIndex, PRInt32* aNameSpaceID,
                                 nsIAtom** aName, nsIAtom** aPrefix) const {
    aName = nsnull;
    aPrefix = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  virtual PRUint32 GetAttrCount() const { return 0; }

#ifdef DEBUG
  void List(FILE* out, PRInt32 aIndent) const { }
  void DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const { }
#endif
  virtual nsresult HandleDOMEvent(nsIPresContext* aPresContext,
                                  nsEvent* aEvent, nsIDOMEvent** aDOMEvent,
                                  PRUint32 aFlags,
                                  nsEventStatus* aEventStatus);

  virtual PRUint32 ContentID() const {
    NS_ERROR("nsAttributeContent::ContentID() not implemented!");
    return 0;
  }

  virtual nsresult RangeAdd(nsIDOMRange* aRange);
  virtual void RangeRemove(nsIDOMRange* aRange);
  const nsVoidArray * GetRangeList() const;

  // Implementation for nsIContent
  virtual PRBool CanContainChildren() const { return PR_FALSE; }

  virtual PRUint32 GetChildCount() const { return 0; }
  virtual nsIContent *GetChildAt(PRUint32 aIndex) const { return nsnull; }
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const { return -1; }
  virtual nsresult InsertChildAt(nsIContent* aKid, PRUint32 aIndex, PRBool aNotify,
                                 PRBool aDeepSetDocument) { return NS_OK; }
  virtual nsresult ReplaceChildAt(nsIContent* aKid, PRUint32 aIndex, PRBool aNotify,
                                  PRBool aDeepSetDocument) { return NS_OK; }
  virtual nsresult AppendChildTo(nsIContent* aKid, PRBool aNotify,
                                 PRBool aDeepSetDocument) { return NS_OK; }
  virtual nsresult RemoveChildAt(PRUint32 aIndex, PRBool aNotify) { return NS_OK; }
  NS_IMETHOD SplitText(PRUint32 aOffset, nsIDOMText** aReturn){ return NS_OK; }

  ///////////////////
  // Implementation for nsITextContent
  NS_IMETHOD GetText(const nsTextFragment** aFragmentsResult);
  NS_IMETHOD GetTextLength(PRInt32* aLengthResult);
  NS_IMETHOD CopyText(nsAString& aResult);
  NS_IMETHOD SetText(const PRUnichar* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  NS_IMETHOD SetText(const nsAString& aStr,
                     PRBool aNotify);
  NS_IMETHOD SetText(const char* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult);
  NS_IMETHOD CloneContent(PRBool aCloneText, nsITextContent** aClone);
  NS_IMETHOD AppendTextTo(nsAString& aResult);

  //----------------------------------------

  void ValidateTextFragment();

  void ToCString(nsAString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  nsIContent*  GetParent() const {
    // Override nsIContent::GetParent to be more efficient internally,
    // since we don't use the low 2 bits of mParentPtrBits for anything.
 
    return NS_REINTERPRET_CAST(nsIContent *, mParentPtrBits);
  }

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIContent*  mContent;

  nsTextFragment mText;
  PRInt32        mNameSpaceID;
  nsCOMPtr<nsIAtom> mAttrName;
};


nsresult
NS_NewAttributeContent(nsIContent* aContent, PRInt32 aNameSpaceID,
                       nsIAtom* aAttrName, nsIContent** aResult)
{
  NS_ENSURE_TRUE(aContent && aAttrName && aResult, NS_ERROR_ILLEGAL_VALUE);

  *aResult = new nsAttributeContent(aContent, aNameSpaceID, aAttrName);
  if (!*aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aResult);

  return NS_OK;
}

//----------------------------------------------------------------------

nsAttributeContent::nsAttributeContent(nsIContent* aContent,
                                       PRInt32 aNameSpaceID,
                                       nsIAtom* aAttrName)
  : mContent(aContent), mNameSpaceID(aNameSpaceID), mAttrName(aAttrName)
{
}

//----------------------------------------------------------------------
nsAttributeContent::~nsAttributeContent()
{
}

/**
 * @param aIID The name of the class implementing the method
 * @param _classiiddef The name of the #define symbol that defines the IID
 * for the class (e.g. NS_ISUPPORTS_IID)
 * 
 */

NS_INTERFACE_MAP_BEGIN(nsAttributeContent)
  NS_INTERFACE_MAP_ENTRY(nsIContent)
  NS_INTERFACE_MAP_ENTRY(nsITextContent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsAttributeContent)
NS_IMPL_RELEASE(nsAttributeContent)


//----------------------------------------------------------------------

// Implementation of nsIContent


void
nsAttributeContent::ToCString(nsAString& aBuf, PRInt32 aOffset,
                                PRInt32 aLen) const
{
}

nsresult
nsAttributeContent::HandleDOMEvent(nsIPresContext* aPresContext,
                                     nsEvent* aEvent,
                                     nsIDOMEvent** aDOMEvent,
                                     PRUint32 aFlags,
                                     nsEventStatus* aEventStatus)
{
  nsresult ret = NS_OK;
  return ret;
}


nsresult 
nsAttributeContent::RangeAdd(nsIDOMRange* aRange)
{
  return NS_ERROR_FAILURE;
}


void
nsAttributeContent::RangeRemove(nsIDOMRange* aRange)
{
}


const nsVoidArray *
nsAttributeContent::GetRangeList() const
{
  return nsnull;
}

already_AddRefed<nsIURI>
nsAttributeContent::GetBaseURI() const
{
  if (GetParent()) {
    return GetParent()->GetBaseURI();
  }

  nsIURI *uri;

  if (mDocument) {
    uri = mDocument->GetBaseURI();
    NS_IF_ADDREF(uri);
  } else {
    uri = nsnull;
  }

  return uri;
}



//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

void
nsAttributeContent::ValidateTextFragment()
{
  if (nsnull != mContent) {
    nsAutoString result;
    mContent->GetAttr(mNameSpaceID, mAttrName, result);

    mText.SetTo(result.get(), result.Length());
  }
  else {
    mText.SetTo("", 0);
  }
}

nsresult
nsAttributeContent::GetText(const nsTextFragment** aFragmentsResult)
{
  ValidateTextFragment();
  if (nsnull != mContent) {
    *aFragmentsResult = &mText;
    return NS_OK;
  } 
  // XXX is this a good idea, or should we just return an empty
  // fragment with no data in it?
  return NS_ERROR_FAILURE;
}

nsresult
nsAttributeContent::GetTextLength(PRInt32* aLengthResult)
{
  if (!aLengthResult) {
    return NS_ERROR_NULL_POINTER;
  }

  ValidateTextFragment();
  *aLengthResult = mText.GetLength();
  return NS_OK;
}

nsresult
nsAttributeContent::CopyText(nsAString& aResult)
{
  ValidateTextFragment();
  if (mText.Is2b()) {
    aResult.Assign(mText.Get2b(), mText.GetLength());
  }
  else {
    const char *data = mText.Get1b();
    CopyASCIItoUTF16(Substring(data, data + mText.GetLength()), aResult);
  }
  return NS_OK;
}

// XXX shouldn't these update mContent's attribute?
nsresult
nsAttributeContent::SetText(const PRUnichar* aBuffer, PRInt32 aLength,
                            PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  mText.SetTo(aBuffer, aLength);

  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(mContent, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP 
nsAttributeContent::SetText(const nsAString& aStr,
                            PRBool aNotify)
{
  mText = aStr;
  
  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(mContent, nsnull);
  }
  return NS_OK;
}

// XXX shouldn't these update mContent's attribute?
nsresult
nsAttributeContent::SetText(const char* aBuffer, 
                            PRInt32 aLength,
                            PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  mText.SetTo(aBuffer, aLength);

  // Trigger a reflow
  if (aNotify && (nsnull != mDocument)) {
    mDocument->ContentChanged(mContent, nsnull);
  }
  return NS_OK;
}


nsresult
nsAttributeContent::IsOnlyWhitespace(PRBool* aResult)
{
  ValidateTextFragment();

  nsTextFragment& frag = mText;
  if (frag.Is2b()) {
    const PRUnichar* cp = frag.Get2b();
    const PRUnichar* end = cp + frag.GetLength();
    while (cp < end) {
      PRUnichar ch = *cp++;
      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }
  else {
    const char* cp = frag.Get1b();
    const char* end = cp + frag.GetLength();
    while (cp < end) {
      PRUnichar ch = PRUnichar(*(unsigned char*)cp);
      cp++;
      if (!XP_IS_SPACE(ch)) {
        *aResult = PR_FALSE;
        return NS_OK;
      }
    }
  }

  *aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsAttributeContent::CloneContent(PRBool aCloneText, nsITextContent** aReturn)
{
  nsAttributeContent* it =
    new nsAttributeContent(mContent, mNameSpaceID, mAttrName);
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  it->mText = mText;

  NS_ADDREF(*aReturn = it);

  return NS_OK;
}

NS_IMETHODIMP
nsAttributeContent::AppendTextTo(nsAString& aResult)
{
  ValidateTextFragment();
  if (mText.Is2b()) {
    aResult.Append(mText.Get2b(), mText.GetLength());
  }
  else {
    AppendASCIItoUTF16(mText.Get1b(), aResult);
  }

  return NS_OK;
}
