/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */


#include "nsIDOMNotation.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"


class nsXMLNotation : public nsIDOMNotation,
                      public nsIContent
{
public:
  nsXMLNotation(const nsAReadableString& aName,
                const nsAReadableString& aPublicId,
                const nsAReadableString& aSystemId);
  virtual ~nsXMLNotation();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMNotation
  NS_IMETHOD    GetPublicId(nsAWritableString& aPublicId);
  NS_IMETHOD    GetSystemId(nsAWritableString& aSystemId);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

protected:
  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mName;
  nsString mPublicId;
  nsString mSystemId;
};

nsresult
NS_NewXMLNotation(nsIContent** aInstancePtrResult,
                  const nsAReadableString& aName,
                  const nsAReadableString& aPublicId,
                  const nsAReadableString& aSystemId)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLNotation(aName, aPublicId, aSystemId);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLNotation::nsXMLNotation(const nsAReadableString& aName,
                             const nsAReadableString& aPublicId,
                             const nsAReadableString& aSystemId) :
  mName(aName), mPublicId(aPublicId), mSystemId(aSystemId)
{
  NS_INIT_REFCNT();
}

nsXMLNotation::~nsXMLNotation()
{
}


// QueryInterface implementation for nsXMLNotation
NS_INTERFACE_MAP_BEGIN(nsXMLNotation)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsIDOMNotation)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Notation)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLNotation)
NS_IMPL_RELEASE(nsXMLNotation)


NS_IMETHODIMP    
nsXMLNotation::GetPublicId(nsAWritableString& aPublicId)
{
  aPublicId.Assign(mPublicId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNotation::GetSystemId(nsAWritableString& aSystemId)
{
  aSystemId.Assign(mSystemId);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLNotation::GetTag(nsIAtom*& aResult) const
{
//  aResult = nsLayoutAtoms::NotationTagName;
//  NS_ADDREF(aResult);

  aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLNotation::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::NOTATION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsXMLNotation* it = new nsXMLNotation(mName,
                                        mSystemId,
                                        mPublicId);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

NS_IMETHODIMP
nsXMLNotation::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Notation refcount=%d<!NOTATION ", mRefCnt);

  nsAutoString tmp(mName);
  if (mPublicId.Length()) {
    tmp.AppendWithConversion(" PUBLIC \"");
    tmp.Append(mPublicId);
    tmp.AppendWithConversion("\"");
  }

  if (mSystemId.Length()) {
    tmp.AppendWithConversion(" SYSTEM \"");
    tmp.Append(mSystemId);
    tmp.AppendWithConversion("\"");
  }

  fputs(tmp, out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const 
{
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for notation");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLNotation::GetContentID(PRUint32* aID) 
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLNotation::SetContentID(PRUint32 aID) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(PRBool)
nsXMLNotation::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}

NS_IMETHODIMP
nsXMLNotation::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mName) + ssize;
  mPublicId.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mPublicId) + ssize;
  mSystemId.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mSystemId) + ssize;
#endif
  return NS_OK;
}
