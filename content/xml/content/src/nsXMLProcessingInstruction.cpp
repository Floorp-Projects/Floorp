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


#include "nsIDOMProcessingInstruction.h"
#include "nsIDOMLinkStyle.h"
#include "nsIDOMStyleSheet.h"
#include "nsIDocument.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsAString.h"
#include "nsXPIDLString.h"
#include "nsUnicharUtils.h"
#include "nsStyleLinkElement.h"
#include "nsParserUtils.h"
#include "nsNetUtil.h"


class nsXMLProcessingInstruction : public nsGenericDOMDataNode,
                                   public nsIDOMProcessingInstruction,
                                   public nsStyleLinkElement
{
public:
  nsXMLProcessingInstruction(const nsAString& aTarget,
                             const nsAString& aData);
  virtual ~nsXMLProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS_INHERITED

  // nsIDOMNode
  NS_IMETHOD GetNodeName(nsAString& aNodeName);
  NS_IMETHOD GetLocalName(nsAString& aLocalName) {
    return nsGenericDOMDataNode::GetLocalName(aLocalName);
  }
  NS_IMETHOD GetNodeValue(nsAString& aNodeValue) {
    return nsGenericDOMDataNode::GetNodeValue(aNodeValue);
  }
  NS_IMETHOD SetNodeValue(const nsAString& aNodeValue) {
    nsresult rv = nsGenericDOMDataNode::SetNodeValue(aNodeValue);
    if (NS_SUCCEEDED(rv)) {
      UpdateStyleSheet();
    }
    return rv;
  }
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {
    return nsGenericDOMDataNode::GetParentNode(aParentNode);
  }
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {
    return nsGenericDOMDataNode::GetChildNodes(aChildNodes);
  }
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes) {
    return nsGenericDOMDataNode::HasChildNodes(aHasChildNodes);
  }
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes) {
    return nsGenericDOMDataNode::HasAttributes(aHasAttributes);
  }
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {
    return nsGenericDOMDataNode::GetFirstChild(aFirstChild);
  }
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {
    return nsGenericDOMDataNode::GetLastChild(aLastChild);
  }
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {
    return nsGenericDOMDataNode::GetPreviousSibling(aPreviousSibling);
  }
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {
    return nsGenericDOMDataNode::GetNextSibling(aNextSibling);
  }
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {
    return nsGenericDOMDataNode::GetAttributes(aAttributes);
  }
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                             nsIDOMNode** aReturn) {
    return nsGenericDOMDataNode::InsertBefore(aNewChild, aRefChild, aReturn);
  }
  NS_IMETHOD AppendChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return nsGenericDOMDataNode::AppendChild(aOldChild, aReturn);
  }
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                             nsIDOMNode** aReturn) {
    return nsGenericDOMDataNode::ReplaceChild(aNewChild, aOldChild, aReturn);
  }
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return nsGenericDOMDataNode::RemoveChild(aOldChild, aReturn);
  }
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {
    return nsGenericDOMDataNode::GetOwnerDocument(aOwnerDocument);
  }
  NS_IMETHOD GetNamespaceURI(nsAString& aNamespaceURI) {
    return nsGenericDOMDataNode::GetNamespaceURI(aNamespaceURI);
  }
  NS_IMETHOD GetPrefix(nsAString& aPrefix) {
    return nsGenericDOMDataNode::GetPrefix(aPrefix);
  }
  NS_IMETHOD SetPrefix(const nsAString& aPrefix) {
    return nsGenericDOMDataNode::SetPrefix(aPrefix);
  }
  NS_IMETHOD Normalize() {
    return NS_OK;
  }
  NS_IMETHOD IsSupported(const nsAString& aFeature,
                      const nsAString& aVersion,
                      PRBool* aReturn) {
    return nsGenericDOMDataNode::IsSupported(aFeature, aVersion, aReturn);
  }
  NS_IMETHOD GetBaseURI(nsAString& aURI) {
    return nsGenericDOMDataNode::GetBaseURI(aURI);
  }
  NS_IMETHOD LookupNamespacePrefix(const nsAString& aNamespaceURI,
                                   nsAString& aPrefix) {
    return nsGenericDOMDataNode::LookupNamespacePrefix(aNamespaceURI, aPrefix);
  }
  NS_IMETHOD LookupNamespaceURI(const nsAString& aNamespacePrefix,
                                nsAString& aNamespaceURI) {
    return nsGenericDOMDataNode::LookupNamespaceURI(aNamespacePrefix,
                                                    aNamespaceURI);
  }

  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // nsIDOMProcessingInstruction
  NS_DECL_NSIDOMPROCESSINGINSTRUCTION

  // nsIContent
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers)
  {
    nsCOMPtr<nsIDocument> oldDoc = mDocument;
    nsresult rv = nsGenericDOMDataNode::SetDocument(aDocument, aDeep,
                                                    aCompileEventHandlers);
    if (NS_SUCCEEDED(rv)) {
      UpdateStyleSheet(oldDoc);
    }
    return rv;
  }
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;

#ifdef DEBUG
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out, PRInt32 aIndent, PRBool aDumpAll) const;
  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;
#endif

  // nsStyleLinkElement
  NS_IMETHOD GetCharset(nsAString& aCharset);

protected:
  void GetStyleSheetURL(PRBool* aIsInline,
                        nsAString& aUrl);
  void GetStyleSheetInfo(nsAString& aTitle,
                         nsAString& aType,
                         nsAString& aMedia,
                         PRBool* aIsAlternate);

  PRBool GetAttrValue(const nsAString& aAttr, nsAString& aValue);

  nsAutoString mTarget;
};

nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsAString& aTarget,
                               const nsAString& aData)
{
  *aInstancePtrResult = new nsXMLProcessingInstruction(aTarget, aData);
  NS_ENSURE_TRUE(*aInstancePtrResult, NS_ERROR_OUT_OF_MEMORY);

  NS_ADDREF(*aInstancePtrResult);

  return NS_OK;
}

nsXMLProcessingInstruction::nsXMLProcessingInstruction(const nsAString& aTarget,
                                                       const nsAString& aData) :
  mTarget(aTarget)
{
  nsGenericDOMDataNode::SetData(aData);
}

nsXMLProcessingInstruction::~nsXMLProcessingInstruction()
{
}


// QueryInterface implementation for nsXMLProcessingInstruction
NS_INTERFACE_MAP_BEGIN(nsXMLProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNode)
  NS_INTERFACE_MAP_ENTRY(nsIDOMProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ProcessingInstruction)
NS_INTERFACE_MAP_END_INHERITING(nsGenericDOMDataNode)


NS_IMPL_ADDREF_INHERITED(nsXMLProcessingInstruction, nsGenericDOMDataNode)
NS_IMPL_RELEASE_INHERITED(nsXMLProcessingInstruction, nsGenericDOMDataNode)


NS_IMETHODIMP
nsXMLProcessingInstruction::GetTarget(nsAString& aTarget)
{
  aTarget.Assign(mTarget);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SetData(const nsAString& aData)
{
  nsresult rv = nsGenericDOMDataNode::SetData(aData);
  if (NS_SUCCEEDED(rv)) {
    UpdateStyleSheet();
  }
  return rv;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetData(nsAString& aData)
{
  return nsGenericDOMDataNode::GetData(aData);
}

PRBool
nsXMLProcessingInstruction::GetAttrValue(const nsAString& aAttr,
                                         nsAString& aValue)
{
  nsAutoString data;

  GetData(data);
  return nsParserUtils::GetQuotedAttributeValue(data, aAttr, aValue);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::processingInstructionTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeName(nsAString& aNodeName)
{
  aNodeName.Assign(mTarget);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeType(PRUint16* aNodeType)
{
  *aNodeType = (PRUint16)nsIDOMNode::PROCESSING_INSTRUCTION_NODE;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::CloneNode(PRBool aDeep, nsIDOMNode** aReturn)
{
  nsAutoString data;
  GetData(data);

  *aReturn = new nsXMLProcessingInstruction(mTarget, data);
  if (!*aReturn) {
    return NS_ERROR_OUT_OF_MEMORY;
  }

  NS_ADDREF(*aReturn);

  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsXMLProcessingInstruction::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt);

  nsAutoString tmp;
  ToCString(tmp, 0, mText.GetLength());
  tmp.Insert(mTarget.get(), 0);
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::DumpContent(FILE* out, PRInt32 aIndent,
                                        PRBool aDumpAll) const {
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SizeOf(nsISizeOfHandler* aSizer,
                                   PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
  PRUint32 sum;
  nsGenericDOMDataNode::SizeOf(aSizer, &sum);
  PRUint32 ssize;
  mTarget.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mTarget) + ssize;
  return NS_OK;
}
#endif

static PRBool InProlog(nsIDOMNode *aThis)
{
  // Check that there are no ancestor elements
  // Typically this should loop once
  nsCOMPtr<nsIDOMNode> current = aThis;
  for(;;) {
    nsCOMPtr<nsIDOMNode> parent;
    current->GetParentNode(getter_AddRefs(parent));
    if (!parent)
      break;
    PRUint16 type;
    parent->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE)
      return PR_FALSE;
    current = parent;
  }

  // Check that there are no elements before
  // Makes sure we are not in epilog
  current = aThis;
  for(;;) {
    nsCOMPtr<nsIDOMNode> prev;
    current->GetPreviousSibling(getter_AddRefs(prev));
    if (!prev)
      break;
    PRUint16 type;
    prev->GetNodeType(&type);
    if (type == nsIDOMNode::ELEMENT_NODE)
      return PR_FALSE;
    current = prev;
  }

  return PR_TRUE;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetCharset(nsAString& aCharset)
{
  if (!GetAttrValue(NS_LITERAL_STRING("charset"), aCharset)) {
    return NS_ERROR_FAILURE;
  }

  return NS_OK;
}

void
nsXMLProcessingInstruction::GetStyleSheetURL(PRBool* aIsInline,
                                             nsAString& aUrl)
{
  *aIsInline = PR_FALSE;
  aUrl.Truncate();

  nsAutoString href;
  GetAttrValue(NS_LITERAL_STRING("href"), href);
  if (href.IsEmpty()) {
    return;
  }

  nsCOMPtr<nsIURI> url, baseURL;
  if (mDocument) {
    mDocument->GetBaseURL(*getter_AddRefs(baseURL));
  }
  NS_MakeAbsoluteURI(aUrl, href, baseURL);
}

void
nsXMLProcessingInstruction::GetStyleSheetInfo(nsAString& aTitle,
                                              nsAString& aType,
                                              nsAString& aMedia,
                                              PRBool* aIsAlternate)
{
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  if (!mTarget.Equals(NS_LITERAL_STRING("xml-stylesheet"))) {
    return;
  }

  // xml-stylesheet PI is special only in prolog
  if (!InProlog(this)) {
    return;
  }

  nsAutoString title, type, media, alternate;

  GetAttrValue(NS_LITERAL_STRING("title"), title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttrValue(NS_LITERAL_STRING("alternate"), alternate);

  // if alternate, does it have title?
  if (alternate.Equals(NS_LITERAL_STRING("yes"))) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = PR_TRUE;
    }
  }

  GetAttrValue(NS_LITERAL_STRING("media"), media);
  aMedia.Assign(media);
  ToLowerCase(aMedia); // case sensitivity?

  GetAttrValue(NS_LITERAL_STRING("type"), type);

  nsAutoString mimeType;
  nsAutoString notUsed;
  nsParserUtils::SplitMimeType(type, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.EqualsIgnoreCase("text/css")) {
    aType.Assign(type);
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.Assign(NS_LITERAL_STRING("text/css"));

  return;
}
