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
#include "nsIAttributeContent.h"
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
#include "nsISizeOfHandler.h"
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

class nsIDOMAttr;
class nsIDOMNodeList;
class nsIFrame;
class nsIStyleContext;
class nsIStyleRule;
class nsISupportsArray;
class nsIDOMText;


// XXX share all id's in this dir




class nsAttributeContent : public nsITextContent, public nsIAttributeContent {
public:
  friend nsresult NS_NewAttributeContent(nsAttributeContent** aNewFrame);

  nsAttributeContent();
  virtual ~nsAttributeContent();

  NS_IMETHOD Init(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttrName);

  // nsISupports
  NS_DECL_ISUPPORTS

  // Implementation for nsIContent
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers);
  NS_IMETHOD GetParent(nsIContent*& aResult) const;
  NS_IMETHOD SetParent(nsIContent* aParent);

  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const {
    aID = kNameSpaceID_None;
    return NS_OK;
  }

  NS_IMETHOD GetTag(nsIAtom*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }

  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {
    aResult = nsnull;
    return NS_OK;
  }


  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr, 
                                 nsINodeInfo*& aNodeInfo) { 
    aNodeInfo = nsnull;
    return NS_OK; 
  }

  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) { return NS_OK; }
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) { return NS_OK; }

  NS_IMETHOD GetBindingParent(nsIContent** aContent) {
    return NS_OK;
  }

  NS_IMETHOD SetBindingParent(nsIContent* aParent) {
    return NS_OK;
  }

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags) {
    return !(aFlags & ~eTEXT);
  }

  NS_IMETHOD GetListenerManager(nsIEventListenerManager **aResult) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, const nsAReadableString& aValue,
                     PRBool aNotify) {  return NS_OK; }
  NS_IMETHOD SetAttr(nsINodeInfo *aNodeInfo, const nsAReadableString& aValue,
                     PRBool aNotify) {  return NS_OK; }
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute, PRBool aNotify) { return NS_OK; }
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsAWritableString& aResult) const {return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute, nsIAtom*& aPrefix, nsAWritableString& aResult) const {return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex, PRInt32& aNameSpaceID, nsIAtom*& aName, nsIAtom*& aPrefix) const {
    aName = nsnull;
    aPrefix = nsnull;
    return NS_ERROR_ILLEGAL_VALUE;
  }

  NS_IMETHOD GetAttrCount(PRInt32& aResult) const { aResult = 0; return NS_OK; }

  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const {  return NS_OK;  }
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {  return NS_OK;  }
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                          nsEvent* aEvent,
                          nsIDOMEvent** aDOMEvent,
                          PRUint32 aFlags,
                          nsEventStatus* aEventStatus);

  NS_IMETHOD GetContentID(PRUint32* aID) {
    *aID = 0;
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD SetContentID(PRUint32 aID) {
    return NS_ERROR_NOT_IMPLEMENTED;
  }

  NS_IMETHOD RangeAdd(nsIDOMRange& aRange);
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange);
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const;

  // Implementation for nsIContent
  NS_IMETHOD CanContainChildren(PRBool& aResult) const { aResult = PR_FALSE; return NS_OK; }

  NS_IMETHOD ChildCount(PRInt32& aResult) const { aResult = 0; return NS_OK;  }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const { aResult = nsnull; return NS_OK;  }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const { aResult = -1; return NS_OK;  }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                           PRBool aDeepSetDocument) { return NS_OK;  }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify,
                            PRBool aDeepSetDocument) { return NS_OK;  }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument) { return NS_OK;  }
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {  return NS_OK;  }
  NS_IMETHOD SplitText(PRUint32 aOffset, nsIDOMText** aReturn){  return NS_OK;  }
 
  ///////////////////
  // Implementation for nsITextContent
  NS_IMETHOD GetText(const nsTextFragment** aFragmentsResult);
  NS_IMETHOD GetTextLength(PRInt32* aLengthResult);
  NS_IMETHOD CopyText(nsAWritableString& aResult);
  NS_IMETHOD SetText(const PRUnichar* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  NS_IMETHOD SetText(const nsAReadableString& aStr,
                     PRBool aNotify);
  NS_IMETHOD SetText(const char* aBuffer,
                   PRInt32 aLength,
                   PRBool aNotify);
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult);
  NS_IMETHOD CloneContent(PRBool aCloneText, nsITextContent** aClone); 

  //----------------------------------------

  void ValidateTextFragment();

  void ToCString(nsAWritableString& aBuf, PRInt32 aOffset, PRInt32 aLen) const;

  // Up pointer to the real content object that we are
  // supporting. Sometimes there is work that we just can't do
  // ourselves, so this is needed to ask the real object to do the
  // work.
  nsIContent*  mContent;
  nsIDocument* mDocument;
  nsIContent*  mParent;

  nsTextFragment mText;
  PRInt32        mNameSpaceID;
  nsIAtom*       mAttrName;

};


nsresult
NS_NewAttributeContent(nsIContent** aContent)
{
  NS_PRECONDITION(aContent, "null OUT ptr");
  if (nsnull == aContent) {
    return NS_ERROR_NULL_POINTER;
  }
  nsAttributeContent* it = new nsAttributeContent;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_SUCCEEDED(it->QueryInterface(NS_GET_IID(nsIContent), (void **)aContent)) ?
         NS_OK : NS_ERROR_FAILURE;
}

//----------------------------------------------------------------------

nsAttributeContent::nsAttributeContent()
  : mText()
{
  NS_INIT_REFCNT();
  mDocument = nsnull;
  mParent   = nsnull;
  mContent  = nsnull;
  mAttrName = nsnull;
}

//----------------------------------------------------------------------
nsAttributeContent::~nsAttributeContent()
{
  NS_IF_RELEASE(mAttrName);
  //NS_IF_RELEASE(mDocument);
}

//----------------------------------------------------------------------
NS_IMETHODIMP
nsAttributeContent::Init(nsIContent* aContent, PRInt32 aNameSpaceID, nsIAtom* aAttrName)
{
  NS_ASSERTION((nsnull == mContent) && (nsnull != aContent), "null ptr");
  mContent = aContent;

  NS_IF_RELEASE(mAttrName);
  mNameSpaceID = aNameSpaceID;
  mAttrName    = aAttrName;
  NS_ADDREF(mAttrName);
  return NS_OK;
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
  NS_INTERFACE_MAP_ENTRY(nsIAttributeContent)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsAttributeContent)
NS_IMPL_RELEASE(nsAttributeContent)


//----------------------------------------------------------------------

// Implementation of nsIContent


void
nsAttributeContent::ToCString(nsAWritableString& aBuf, PRInt32 aOffset,
                                PRInt32 aLen) const
{
}

nsresult
nsAttributeContent::GetDocument(nsIDocument*& aResult) const
{
  aResult = mDocument;
  NS_IF_ADDREF(mDocument);
  return NS_OK;
}


nsresult
nsAttributeContent::SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers)
{
  mDocument = aDocument;
  //NS_IF_ADDREF(mDocument);
  return NS_OK;
}

nsresult
nsAttributeContent::GetParent(nsIContent*& aResult) const
{
  NS_IF_ADDREF(mParent);
  aResult = mParent;
  return NS_OK;;
}

nsresult
nsAttributeContent::SetParent(nsIContent* aParent)
{
  mParent = aParent;
  return NS_OK;
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
nsAttributeContent::RangeAdd(nsIDOMRange& aRange)
{
  return NS_ERROR_FAILURE;
}


nsresult 
nsAttributeContent::RangeRemove(nsIDOMRange& aRange)
{
  return NS_ERROR_FAILURE;
}


nsresult 
nsAttributeContent::GetRangeList(nsVoidArray*& aResult) const
{
  return NS_ERROR_FAILURE;
}




//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

void
nsAttributeContent::ValidateTextFragment()
{
  if (nsnull != mContent) {
    nsAutoString result;
    mContent->GetAttr(mNameSpaceID, mAttrName, result);

    PRUnichar * text = result.ToNewUnicode();
    mText.SetTo(text, result.Length());
    nsCRT::free(text);
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
nsAttributeContent::CopyText(nsAWritableString& aResult)
{
  ValidateTextFragment();
  if (mText.Is2b()) {
    aResult.Assign(mText.Get2b(), mText.GetLength());
  }
  else {
    aResult.Assign(NS_ConvertASCIItoUCS2(mText.Get1b(), mText.GetLength()).get(), mText.GetLength());
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
nsAttributeContent::SetText(const nsAReadableString& aStr,
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
  nsresult result = NS_OK;
  nsAttributeContent* it;
  NS_NEWXPCOM(it, nsAttributeContent);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  result = it->QueryInterface(NS_GET_IID(nsITextContent), (void**) aReturn);
  if (NS_FAILED(result)) {
    return result;
  }
  result = it->Init(mContent, mNameSpaceID, mAttrName);
  if (NS_FAILED(result) || !aCloneText) {
    return result;
  }
  it->mText = mText;
  return result;
}

NS_IMETHODIMP
nsAttributeContent::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  *aResult = sizeof(*this);
  return NS_OK;
}
