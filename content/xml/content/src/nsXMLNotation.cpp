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

#include "nsIDOMNotation.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsString.h"
#include "nsIXMLContent.h"


class nsXMLNotation : public nsGenericDOMDataNode,
                      public nsIDOMNotation
{
public:
  nsXMLNotation(const nsAString& aName,
                const nsAString& aPublicId,
                const nsAString& aSystemId);
  virtual ~nsXMLNotation();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA

  // nsIDOMNotation
  NS_DECL_NSIDOMNOTATION

#ifdef DEBUG 
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
#endif

  // nsIContent
  NS_IMETHOD GetTag(nsIAtom** aResult) const;

protected:
  nsAutoString mName;
  nsString mPublicId;
  nsString mSystemId;
};

nsresult
NS_NewXMLNotation(nsIContent** aInstancePtrResult,
                  const nsAString& aName,
                  const nsAString& aPublicId,
                  const nsAString& aSystemId)
{
  *aInstancePtrResult = new nsXMLNotation(aName, aPublicId, aSystemId);
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsXMLNotation::nsXMLNotation(const nsAString& aName,
                             const nsAString& aPublicId,
                             const nsAString& aSystemId) :
  mName(aName), mPublicId(aPublicId), mSystemId(aSystemId)
{
}

nsXMLNotation::~nsXMLNotation()
{
}


// QueryInterface implementation for nsXMLNotation
NS_INTERFACE_MAP_BEGIN(nsXMLNotation)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNotation)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIContent)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Notation)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF_INHERITED(nsXMLNotation, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsXMLNotation, nsGenericDOMDataNode)


NS_IMETHODIMP    
nsXMLNotation::GetPublicId(nsAString& aPublicId)
{
  aPublicId.Assign(mPublicId);

  return NS_OK;
}

NS_IMETHODIMP    
nsXMLNotation::GetSystemId(nsAString& aSystemId)
{
  aSystemId.Assign(mSystemId);

  return NS_OK;
}

NS_IMETHODIMP 
nsXMLNotation::GetTag(nsIAtom** aResult) const
{
//  *aResult = nsLayoutAtoms::NotationTagName;
//  NS_ADDREF(*aResult);

  *aResult = nsnull;

  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::GetNodeName(nsAString& aNodeName)
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
  *aReturn = new nsXMLNotation(mName, mSystemId, mPublicId);
  NS_ENSURE_TRUE(*aReturn, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aReturn);

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLNotation::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Notation refcount=%d<!NOTATION ", mRefCnt.get());

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

  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLNotation::DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const 
{
  return NS_OK;
}
#endif
