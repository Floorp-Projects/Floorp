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
#include "nsGenericDOMDataNode.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"


class nsXMLEntity : public nsGenericDOMDataNode,
                    public nsIDOMEntity
{
public:
  nsXMLEntity(const nsAString& aName, 
              const nsAString& aPublicId,
              const nsAString& aSystemId, 
              const nsAString& aNotationName);
  virtual ~nsXMLEntity();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA

  // nsIDOMEntity
  NS_DECL_NSIDOMENTITY

  NS_IMETHOD GetTag(nsIAtom** aResult) const;

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
#endif

protected:
  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsAutoString mName;
  nsString mPublicId;
  nsString mSystemId;
  nsString mNotationName;
};

nsresult
NS_NewXMLEntity(nsIContent** aInstancePtrResult,
                const nsAString& aName,
                const nsAString& aPublicId,
                const nsAString& aSystemId,
                const nsAString& aNotationName)
{
  *aInstancePtrResult = new nsXMLEntity(aName, aPublicId, aSystemId,
                                        aNotationName);
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsXMLEntity::nsXMLEntity(const nsAString& aName,
                         const nsAString& aPublicId,
                         const nsAString& aSystemId,
                         const nsAString& aNotationName) :
  mName(aName), mPublicId(aPublicId), mSystemId(aSystemId),
  mNotationName(aNotationName)
{
}

nsXMLEntity::~nsXMLEntity()
{
}


// QueryInterface implementation for nsXMLEntity
NS_INTERFACE_MAP_BEGIN(nsXMLEntity)
  NS_INTERFACE_MAP_ENTRY(nsIDOMEntity)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Entity)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF_INHERITED(nsXMLEntity, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsXMLEntity, nsGenericDOMDataNode)


NS_IMETHODIMP    
nsXMLEntity::GetPublicId(nsAString& aPublicId)
{
  aPublicId.Assign(mPublicId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetSystemId(nsAString& aSystemId)
{
  aSystemId.Assign(mSystemId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLEntity::GetNotationName(nsAString& aNotationName)
{
  aNotationName.Assign(mNotationName);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLEntity::GetTag(nsIAtom** aResult) const
{
//  *aResult = nsLayoutAtoms::EntityTagName;
//  NS_ADDREF(*aResult);

  *aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLEntity::GetNodeName(nsAString& aNodeName)
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
  *aReturn = new nsXMLEntity(mName, mSystemId, mPublicId, mNotationName);
  NS_ENSURE_TRUE(*aReturn, NS_ERROR_OUT_OF_MEMORY);

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLEntity::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Entity refcount=%d<!ENTITY ", mRefCnt.get());

  nsAutoString tmp(mName);
  if (!mPublicId.IsEmpty()) {
    tmp.Append(NS_LITERAL_STRING(" PUBLIC \""));
    tmp.Append(mPublicId);
    tmp.Append(NS_LITERAL_STRING("\""));
  }

  if (!mSystemId.IsEmpty()) {
    tmp.Append(NS_LITERAL_STRING(" SYSTEM \""));
    tmp.Append(mSystemId);
    tmp.Append(NS_LITERAL_STRING("\""));
  }

  if (!mNotationName.IsEmpty()) {
    tmp.Append(NS_LITERAL_STRING(" NDATA "));
    tmp.Append(mNotationName);
  }

  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLEntity::DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const
{
  return NS_OK;
}
#endif
