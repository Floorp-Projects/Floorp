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

#include "nsDOMDocumentType.h"
#include "nsDOMAttributeMap.h"
#include "nsIDOMNamedNodeMap.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsLayoutAtoms.h"
#include "nsCOMPtr.h"

nsresult
NS_NewDOMDocumentType(nsIDOMDocumentType** aDocType,
                      const nsAReadableString& aName,
                      nsIDOMNamedNodeMap *aEntities,
                      nsIDOMNamedNodeMap *aNotations,
                      const nsAReadableString& aPublicId,
                      const nsAReadableString& aSystemId,
                      const nsAReadableString& aInternalSubset)
{
  NS_ENSURE_ARG_POINTER(aDocType);

  *aDocType = new nsDOMDocumentType(aName, aEntities, aNotations,
                                    aPublicId, aSystemId, aInternalSubset);
  if (!*aDocType) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aDocType);

  return NS_OK;
}

nsDOMDocumentType::nsDOMDocumentType(const nsAReadableString& aName,
                                     nsIDOMNamedNodeMap *aEntities,
                                     nsIDOMNamedNodeMap *aNotations,
                                     const nsAReadableString& aPublicId,
                                     const nsAReadableString& aSystemId,
                                     const nsAReadableString& aInternalSubset) :
  mName(aName),
  mPublicId(aPublicId),
  mSystemId(aSystemId),
  mInternalSubset(aInternalSubset)
{
  NS_INIT_REFCNT();

  mEntities = aEntities;
  mNotations = aNotations;

  NS_IF_ADDREF(mEntities);
  NS_IF_ADDREF(mNotations);
}

nsDOMDocumentType::~nsDOMDocumentType()
{
  NS_IF_RELEASE(mEntities);
  NS_IF_RELEASE(mNotations);
}

NS_IMPL_ADDREF(nsDOMDocumentType)
NS_IMPL_RELEASE(nsDOMDocumentType)

NS_INTERFACE_MAP_BEGIN(nsDOMDocumentType)
   NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMDocumentType)
   NS_INTERFACE_MAP_ENTRY(nsIDOMDocumentType)
   NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
   NS_INTERFACE_MAP_ENTRY(nsIContent)
   NS_INTERFACE_MAP_ENTRY(nsIScriptObjectOwner)
NS_INTERFACE_MAP_END_THREADSAFE

NS_IMETHODIMP    
nsDOMDocumentType::GetName(nsAWritableString& aName)
{
  aName=mName;

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMDocumentType::GetEntities(nsIDOMNamedNodeMap** aEntities)
{
  NS_ENSURE_ARG_POINTER(aEntities);

  *aEntities = mEntities;

  NS_IF_ADDREF(*aEntities);

  return NS_OK;
}

NS_IMETHODIMP    
nsDOMDocumentType::GetNotations(nsIDOMNamedNodeMap** aNotations)
{
  NS_ENSURE_ARG_POINTER(aNotations);

  *aNotations = mNotations;

  NS_IF_ADDREF(*aNotations);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetPublicId(nsAWritableString& aPublicId)
{
  aPublicId = mPublicId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetSystemId(nsAWritableString& aSystemId)
{
  aSystemId = mSystemId;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetInternalSubset(nsAWritableString& aInternalSubset)
{
  aInternalSubset = mInternalSubset;

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMDocumentType::GetTag(nsIAtom*& aResult) const
{
  aResult = NS_NewAtom(mName.GetUnicode());

  return NS_OK;
}

NS_IMETHODIMP 
nsDOMDocumentType::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName=mName;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = nsIDOMNode::DOCUMENT_TYPE_NODE;

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsDOMDocumentType* it = new nsDOMDocumentType(mName,
                                                mEntities,
                                                mNotations,
                                                mPublicId,
                                                mSystemId,
                                                mInternalSubset);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIDOMNodeIID, (void**) aReturn);
}

NS_IMETHODIMP
nsDOMDocumentType::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Document type refcount: %d\n", mRefCnt);

  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,PRBool aDumpAll=PR_TRUE) const {
  return NS_OK;
}

NS_IMETHODIMP
nsDOMDocumentType::HandleDOMEvent(nsIPresContext* aPresContext,
                                           nsEvent* aEvent,
                                           nsIDOMEvent** aDOMEvent,
                                           PRUint32 aFlags,
                                           nsEventStatus* aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for document type");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsDOMDocumentType::GetContentID(PRUint32* aID) 
{
  NS_ENSURE_ARG_POINTER(aID);

  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMDocumentType::SetContentID(PRUint32 aID) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsDOMDocumentType::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  NS_ENSURE_ARG_POINTER(aResult);

#ifdef DEBUG
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mName) + ssize;
  if (mEntities) {
    PRBool recorded;
    aSizer->RecordObject((void*) mEntities, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mEntities, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_entities, size);
    }
  }
  if (mNotations) {
    PRBool recorded;
    aSizer->RecordObject((void*) mNotations, &recorded);
    if (!recorded) {
      PRUint32 size;
      nsDOMAttributeMap::SizeOfNamedNodeMap(mNotations, aSizer, &size);
      aSizer->AddSize(nsLayoutAtoms::xml_document_notations, size);
    }
  }
#endif
  return NS_OK;
}
