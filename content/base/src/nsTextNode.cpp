/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsIDOMText.h"
#include "nsGenericDOMDataNode.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsFrame.h"
#include "nsIDocument.h"
#include "nsCRT.h"
#include "nsLayoutAtoms.h"

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

/* XXX should not be html content; should be nsITextContent */

class nsTextNode : public nsIDOMText,
                   public nsIScriptObjectOwner,
                   public nsIDOMEventReceiver,
                   public nsIContent,
                   public nsITextContent
{
public:
  nsTextNode();
  ~nsTextNode();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMCharacterData
  NS_IMPL_IDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMText
  NS_IMETHOD SplitText(PRUint32 aOffset, nsIDOMText** aReturn);

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  // nsITextContent
  NS_IMETHOD GetText(const nsTextFragment*& aFragmentsResult,
                     PRInt32& aNumFragmentsResult);
  NS_IMETHOD SetText(const PRUnichar* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify);
  NS_IMETHOD SetText(const char* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify);

  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult);

protected:
  nsGenericDOMDataNode mInner;
};

nsresult NS_NewTextNode(nsIContent** aInstancePtrResult);
nsresult
NS_NewTextNode(nsIContent** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsTextNode* it;
  NS_NEWXPCOM(it, nsTextNode);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentIID, (void **) aInstancePtrResult);
}

nsTextNode::nsTextNode()
{
  NS_INIT_REFCNT();
  mInner.Init(this);
}

nsTextNode::~nsTextNode()
{
}

NS_IMPL_ADDREF(nsTextNode)
NS_IMPL_RELEASE(nsTextNode)

NS_IMETHODIMP
nsTextNode::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_DOM_DATA_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMTextIID)) {
    nsIDOMText* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(kITextContentIID)) {
    nsITextContent* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP 
nsTextNode::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::textTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::GetNodeName(nsString& aNodeName)
{
  aNodeName.SetString("#text");
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::TEXT_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsTextNode* it;
  NS_NEWXPCOM(it, nsTextNode);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  nsAutoString data;
  nsresult result = GetData(data);
  if (NS_FAILED(result)) {
    return result;
  }
  result = it->SetData(data);
  if (NS_FAILED(result)) {
    return result;
  }
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsTextNode::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Text refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::HandleDOMEvent(nsIPresContext& aPresContext,
                           nsEvent* aEvent,
                           nsIDOMEvent** aDOMEvent,
                           PRUint32 aFlags,
                           nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

//----------------------------------------------------------------------

// Implementation of the nsIDOMText interface

NS_IMETHODIMP    
nsTextNode::SplitText(PRUint32 aOffset, nsIDOMText** aReturn)
{
  nsresult result = NS_OK;
  nsIContent* newNode;
  nsITextContent* text;
  nsAutoString cutText;
  nsIContent* parentNode;
  PRUint32 length;

  GetLength(&length);
  // Cut the second part out of the original text node
  result = SubstringData(aOffset, length-aOffset, cutText);
  if (NS_OK == result) {
    result = DeleteData(aOffset, length-aOffset);
    if (NS_OK == result) {
      // Create a new text node and set its data to the
      // string we just cut out
      result = NS_NewTextNode(&newNode);
      if (NS_OK == result) {
        result = newNode->QueryInterface(kITextContentIID, (void**)&text);
        if (NS_OK == result) {
          text->SetText(cutText, cutText.Length(), PR_FALSE);
          // Find the parent of the current node and insert the
          // new text node as a child after the current node
          GetParent(parentNode);
          if (nsnull != parentNode) {
            PRInt32 index;

            result = parentNode->IndexOf(this, index);
            if (NS_OK == result) {
              result = parentNode->InsertChildAt(newNode, index+1, PR_TRUE);
            }
            NS_RELEASE(parentNode);
          }
          result = text->QueryInterface(kIDOMTextIID, (void**)aReturn);
          NS_RELEASE(text);
        }
        NS_RELEASE(newNode);
      }
    }
  }

  return result;
}

//----------------------------------------------------------------------

// Implementation of the nsITextContent interface

NS_IMETHODIMP
nsTextNode::GetText(const nsTextFragment*& aFragmentsResult,
                    PRInt32& aNumFragmentsResult)
{
  aFragmentsResult = &mInner.mText;
  aNumFragmentsResult = 1;
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::SetText(const PRUnichar* aBuffer, PRInt32 aLength,
                    PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  mInner.mText.SetTo(aBuffer, aLength);

  // Trigger a reflow
  if (aNotify && (nsnull != mInner.mDocument)) {
    mInner.mDocument->ContentChanged(this, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::SetText(const char* aBuffer, PRInt32 aLength,
                    PRBool aNotify)
{
  NS_PRECONDITION((aLength >= 0) && (nsnull != aBuffer), "bad args");
  if (aLength < 0) {
    return NS_ERROR_ILLEGAL_VALUE;
  }
  if (nsnull == aBuffer) {
    return NS_ERROR_NULL_POINTER;
  }
  mInner.mText.SetTo(aBuffer, aLength);

  // Trigger a reflow
  if (aNotify && (nsnull != mInner.mDocument)) {
    mInner.mDocument->ContentChanged(this, nsnull);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsTextNode::IsOnlyWhitespace(PRBool* aResult)
{
  nsTextFragment& frag = mInner.mText;
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
