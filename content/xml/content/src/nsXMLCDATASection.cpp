/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
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
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "nsIDOMCDATASection.h"
#include "nsGenericDOMDataNode.h"
#include "nsLayoutAtoms.h"

#include "nsContentUtils.h"


class nsXMLCDATASection : public nsGenericDOMDataNode,
                          public nsIDOMCDATASection
{
public:
  nsXMLCDATASection();
  virtual ~nsXMLCDATASection();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA

  // nsIDOMCharacterData
  NS_FORWARD_NSIDOMCHARACTERDATA(nsGenericDOMDataNode::)

  // nsIDOMText
  NS_FORWARD_NSIDOMTEXT(nsGenericDOMDataNode::)

  // nsIDOMCDATASection
  // Empty interface

  // nsIContent
  virtual nsIAtom *Tag() const;
  virtual PRBool IsContentOfType(PRUint32 aFlags) const;
#ifdef DEBUG
  virtual void List(FILE* out, PRInt32 aIndent) const;
  virtual void DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const;
#endif

  // nsITextContent
  virtual already_AddRefed<nsITextContent> CloneContent(PRBool aCloneText);

protected:
};

nsresult
NS_NewXMLCDATASection(nsIContent** aInstancePtrResult)
{
  *aInstancePtrResult = new nsXMLCDATASection();
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsXMLCDATASection::nsXMLCDATASection()
{
}

nsXMLCDATASection::~nsXMLCDATASection()
{
}


// QueryInterface implementation for nsXMLCDATASection
NS_INTERFACE_MAP_BEGIN(nsXMLCDATASection)
  NS_INTERFACE_MAP_ENTRY(nsITextContent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRY(nsIDOMText)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCDATASection)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(CDATASection)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)

NS_IMPL_ADDREF_INHERITED(nsXMLCDATASection, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsXMLCDATASection, nsGenericDOMDataNode)


nsIAtom *
nsXMLCDATASection::Tag() const
{
  return nsLayoutAtoms::textTagName;
}

PRBool
nsXMLCDATASection::IsContentOfType(PRUint32 aFlags) const
{
  return !(aFlags & ~eTEXT);
}

NS_IMETHODIMP
nsXMLCDATASection::GetNodeName(nsAString& aNodeName)
{
  aNodeName.AssignLiteral("#cdata-section");
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::GetNodeValue(nsAString& aNodeValue)
{
  return nsGenericDOMDataNode::GetNodeValue(aNodeValue);
}

NS_IMETHODIMP
nsXMLCDATASection::SetNodeValue(const nsAString& aNodeValue)
{
  return nsGenericDOMDataNode::SetNodeValue(aNodeValue);
}

NS_IMETHODIMP
nsXMLCDATASection::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::CDATA_SECTION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLCDATASection::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsCOMPtr<nsITextContent> textContent = CloneContent(PR_TRUE);
  NS_ENSURE_TRUE(textContent, NS_ERROR_OUT_OF_MEMORY);

  return CallQueryInterface(textContent, aReturn);
}

already_AddRefed<nsITextContent> 
nsXMLCDATASection::CloneContent(PRBool aCloneText)
{
  nsXMLCDATASection* it = new nsXMLCDATASection();
  if (!it)
    return nsnull;

  if (aCloneText) {
    it->mText = mText;
  }

  NS_ADDREF(it);

  return it;
}

#ifdef DEBUG
void
nsXMLCDATASection::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "CDATASection refcount=%d<", mRefCnt.get());

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
}

void
nsXMLCDATASection::DumpContent(FILE* out, PRInt32 aIndent,
                               PRBool aDumpAll) const {
}
#endif
