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
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsITextContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsFrame.h"
#include "nsIDocument.h"
#include "nsCRT.h"
#include "nsLayoutAtoms.h"

static NS_DEFINE_IID(kIDOMTextIID, NS_IDOMTEXT_IID);
static NS_DEFINE_IID(kITextContentIID, NS_ITEXT_CONTENT_IID);

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
  NS_IMPL_IDOMTEXT_USING_GENERIC_DOM_DATA(mInner)

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  // nsITextContent
  NS_IMPL_ITEXTCONTENT_USING_GENERIC_DOM_DATA(mInner)

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
