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


#include "nsIDOMEntity.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"


class nsXMLEntity : public nsIDOMEntity,
                    public nsIContent
{
public:
  nsXMLEntity(const nsAReadableString& aName, 
              const nsAReadableString& aPublicId,
              const nsAReadableString& aSystemId, 
              const nsAReadableString& aNotationName);
  virtual ~nsXMLEntity();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMEntity
  NS_IMETHOD    GetPublicId(nsAWritableString& aPublicId);
  NS_IMETHOD    GetSystemId(nsAWritableString& aSystemId);
  NS_IMETHOD    GetNotationName(nsAWritableString& aNotationName);

  // nsIContent
  NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

#ifdef DEBUG
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

protected:
  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mName;
  nsString mPublicId;
  nsString mSystemId;
  nsString mNotationName;
};

nsresult
NS_NewXMLEntity(nsIContent** aInstancePtrResult,
                const nsAReadableString& aName,
                const nsAReadableString& aPublicId,
                const nsAReadableString& aSystemId,
                const nsAReadableString& aNotationName)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLEntity(aName, aPublicId, aSystemId, aNotationName);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLEntity::nsXMLEntity(const nsAReadableString& aName,
                         const nsAReadableString& aPublicId,
                         const nsAReadableString& aSystemId,
                         const nsAReadableString& aNotationName) :
  mName(aName), mPublicId(aPublicId), mSystemId(aSystemId), mNotationName(aNotationName)
{
  NS_INIT_REFCNT();
}

nsXMLEntity::~nsXMLEntity()
{
}


// QueryInterface implementation for nsXMLEntity
NS_INTERFACE_MAP_BEGIN(nsXMLEntity)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsIDOMEntity)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Entity)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLEntity)
NS_IMPL_RELEASE(nsXMLEntity)


NS_IMETHODIMP    
nsXMLEntity::GetPublicId(nsAWritableString& aPublicId)
{
  aPublicId.Assign(mPublicId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetSystemId(nsAWritableString& aSystemId)
{
  aSystemId.Assign(mSystemId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetNotationName(nsAWritableString& aNotationName)
{
  aNotationName.Assign(mNotationName);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLEntity::GetTag(nsIAtom*& aResult) const
{
//  aResult = nsLayoutAtoms::EntityTagName;
//  NS_ADDREF(aResult);

  aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLEntity::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(mName);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::ENTITY_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsXMLEntity* it = new nsXMLEntity(mName,
                                    mSystemId,
                                    mPublicId,
                                    mNotationName);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLEntity::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Entity refcount=%d<!ENTITY ", mRefCnt);

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

  if (mNotationName.Length()) {
    tmp.AppendWithConversion(" NDATA ");
    tmp.Append(mNotationName);
  }

  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const
{
  return NS_OK;
}
#endif

NS_IMETHODIMP
nsXMLEntity::HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for entity");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLEntity::GetContentID(PRUint32* aID) 
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLEntity::SetContentID(PRUint32 aID) 
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP_(PRBool)
nsXMLEntity::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLEntity::SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mName) + ssize;
  mPublicId.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mPublicId) + ssize;
  mSystemId.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mSystemId) + ssize;
  mNotationName.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mNotationName) + ssize;
  return NS_OK;
}
#endif
