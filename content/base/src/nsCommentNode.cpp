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
#include "nsIDOMComment.h"
#include "nsGenericDOMDataNode.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsFrame.h"

static NS_DEFINE_IID(kIDOMCommentIID, NS_IDOMCOMMENT_IID);

class nsCommentNode : public nsIDOMComment,
                      public nsIScriptObjectOwner,
                      public nsIDOMEventReceiver,
                      public nsIContent
{
public:
  nsCommentNode();
  ~nsCommentNode();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_IDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMCharacterData
  NS_IMPL_IDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMComment

  // nsIScriptObjectOwner
  NS_IMPL_ISCRIPTOBJECTOWNER_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMEventReceiver
  NS_IMPL_IDOMEVENTRECEIVER_USING_GENERIC_DOM_DATA(mInner)

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

protected:
  nsGenericDOMDataNode mInner;
};

nsresult NS_NewCommentNode(nsIContent** aInstancePtrResult);
nsresult
NS_NewCommentNode(nsIContent** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsCommentNode();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIContentIID, (void **) aInstancePtrResult);
}

nsCommentNode::nsCommentNode()
{
  NS_INIT_REFCNT();
  mInner.Init(this);
}

nsCommentNode::~nsCommentNode()
{
}

NS_IMPL_ADDREF(nsCommentNode)

NS_IMPL_RELEASE(nsCommentNode)

NS_IMETHODIMP
nsCommentNode::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  NS_IMPL_DOM_DATA_QUERY_INTERFACE(aIID, aInstancePtr, this)
  if (aIID.Equals(kIDOMCommentIID)) {
    nsIDOMComment* tmp = this;
    *aInstancePtr = (void*) tmp;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP
nsCommentNode::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::COMMENT_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsCommentNode::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsCommentNode* it = new nsCommentNode();
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
nsCommentNode::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, " refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsCommentNode::HandleDOMEvent(nsIPresContext& aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus& aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

nsresult NS_NewCommentFrame(nsIFrame*& aResult);
nsresult
NS_NewCommentFrame(nsIFrame*& aResult)
{
  nsIFrame* frame;
  NS_NewEmptyFrame(&frame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  return NS_OK;
}
