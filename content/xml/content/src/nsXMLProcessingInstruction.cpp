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
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsIDocument.h"
#include "nsIStyleSheet.h"
#include "nsIURI.h"
#include "nsGenericDOMDataNode.h"
#include "nsGenericElement.h"
#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h"
#include "nsString.h"
#include "nsXPIDLString.h"
#include "nsIXMLContent.h"
#include "nsStyleLinkElement.h"

#include "nsNetUtil.h"


class nsXMLProcessingInstruction : public nsIDOMProcessingInstruction,
                                   public nsIContent,
                                   public nsStyleLinkElement
{
public:
  nsXMLProcessingInstruction(const nsAReadableString& aTarget, const nsAReadableString& aData);
  virtual ~nsXMLProcessingInstruction();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMETHOD GetNodeName(nsAWritableString& aNodeName);
  NS_IMETHOD GetLocalName(nsAWritableString& aLocalName) {
    return mInner.GetLocalName(aLocalName);
  }
  NS_IMETHOD GetNodeValue(nsAWritableString& aNodeValue) {
    return mInner.GetNodeValue(aNodeValue);
  }
  NS_IMETHOD SetNodeValue(const nsAReadableString& aNodeValue) {
    nsresult rv = mInner.SetNodeValue(this, aNodeValue);
    UpdateStyleSheet(PR_TRUE);
    return rv;
  }
  NS_IMETHOD GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD GetParentNode(nsIDOMNode** aParentNode) {
    return mInner.GetParentNode(aParentNode);
  }
  NS_IMETHOD GetChildNodes(nsIDOMNodeList** aChildNodes) {
    return mInner.GetChildNodes(aChildNodes);
  }
  NS_IMETHOD HasChildNodes(PRBool* aHasChildNodes) {
    return mInner.HasChildNodes(aHasChildNodes);
  }
  NS_IMETHOD HasAttributes(PRBool* aHasAttributes) {
    return mInner.HasAttributes(aHasAttributes);
  }
  NS_IMETHOD GetFirstChild(nsIDOMNode** aFirstChild) {
    return mInner.GetFirstChild(aFirstChild);
  }
  NS_IMETHOD GetLastChild(nsIDOMNode** aLastChild) {
    return mInner.GetLastChild(aLastChild);
  }
  NS_IMETHOD GetPreviousSibling(nsIDOMNode** aPreviousSibling) {
    return mInner.GetPreviousSibling(this, aPreviousSibling);
  }
  NS_IMETHOD GetNextSibling(nsIDOMNode** aNextSibling) {
    return mInner.GetNextSibling(this, aNextSibling);
  }
  NS_IMETHOD GetAttributes(nsIDOMNamedNodeMap** aAttributes) {
    return mInner.GetAttributes(aAttributes);
  }
  NS_IMETHOD InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild,
                             nsIDOMNode** aReturn) {
    return mInner.InsertBefore(aNewChild, aRefChild, aReturn);
  }
  NS_IMETHOD AppendChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return mInner.AppendChild(aOldChild, aReturn);
  }
  NS_IMETHOD ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild,
                             nsIDOMNode** aReturn) {
    return mInner.ReplaceChild(aNewChild, aOldChild, aReturn);
  }
  NS_IMETHOD RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn) {
    return mInner.RemoveChild(aOldChild, aReturn);
  }
  NS_IMETHOD GetOwnerDocument(nsIDOMDocument** aOwnerDocument) {
    return mInner.GetOwnerDocument(aOwnerDocument);
  }
  NS_IMETHOD GetNamespaceURI(nsAWritableString& aNamespaceURI) {
    return mInner.GetNamespaceURI(aNamespaceURI);
  }
  NS_IMETHOD GetPrefix(nsAWritableString& aPrefix) {
    return mInner.GetPrefix(aPrefix);
  }
  NS_IMETHOD SetPrefix(const nsAReadableString& aPrefix) {
    return mInner.SetPrefix(aPrefix);
  }
  NS_IMETHOD Normalize() {
    return NS_OK;
  }
  NS_IMETHOD IsSupported(const nsAReadableString& aFeature,
                      const nsAReadableString& aVersion,
                      PRBool* aReturn) {
    return mInner.IsSupported(aFeature, aVersion, aReturn);
  }
  NS_IMETHOD GetBaseURI(nsAWritableString& aURI) {
    return mInner.GetBaseURI(aURI);
  }
  NS_IMETHOD LookupNamespacePrefix(const nsAReadableString& aNamespaceURI,
                                   nsAWritableString& aPrefix) {
    return mInner.LookupNamespacePrefix(aNamespaceURI, aPrefix);
  }
  NS_IMETHOD LookupNamespaceURI(const nsAReadableString& aNamespacePrefix,
                                nsAWritableString& aNamespaceURI) {
    return mInner.LookupNamespaceURI(aNamespacePrefix, aNamespaceURI);
  }

  NS_IMETHOD CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // nsIDOMProcessingInstruction
  NS_DECL_NSIDOMPROCESSINGINSTRUCTION

  // nsIContent
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {
    return mInner.GetDocument(aResult);
  }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep,
                         PRBool aCompileEventHandlers) {
    nsIDocument *oldDoc = mInner.mDocument;
    nsresult rv = mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers);
    UpdateStyleSheet(PR_TRUE, oldDoc);
    return rv;
  }
  NS_IMETHOD GetParent(nsIContent*& aResult) const {
    return mInner.GetParent(aResult);
  }
  NS_IMETHOD SetParent(nsIContent* aParent) {
    return mInner.SetParent(aParent);
  }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const {
    return mInner.CanContainChildren(aResult);
  }
  NS_IMETHOD ChildCount(PRInt32& aResult) const {
    return mInner.ChildCount(aResult);
  }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const {
    return mInner.ChildAt(aIndex, aResult);
  }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aResult) const {
    return mInner.IndexOf(aPossibleChild, aResult);
  }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify, PRBool aDeepSetDocument) {
    return mInner.InsertChildAt(aKid, aIndex, aNotify, aDeepSetDocument);
  }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify, PRBool aDeepSetDocument) {
    return mInner.ReplaceChildAt(aKid, aIndex, aNotify, aDeepSetDocument);
  }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify,
                           PRBool aDeepSetDocument) {
    return mInner.AppendChildTo(aKid, aNotify, aDeepSetDocument);
  }
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify) {
    return mInner.RemoveChildAt(aIndex, aNotify);
  }
  NS_IMETHOD GetNameSpaceID(PRInt32& aID) const {
    return mInner.GetNameSpaceID(aID);
  }
  NS_IMETHOD GetTag(nsIAtom*& aResult) const;
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const;
  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,
                                 nsINodeInfo*& aNodeInfo) {
    return mInner.NormalizeAttributeString(aStr, aNodeInfo);
  }
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                     nsAWritableString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aAttribute, aResult);
  }
  NS_IMETHOD GetAttr(PRInt32 aNameSpaceID, nsIAtom *aAttribute,
                     nsIAtom*& aPrefix, nsAWritableString& aResult) const {
    return mInner.GetAttribute(aNameSpaceID, aAttribute, aPrefix, aResult);
  }
  NS_IMETHOD SetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                     const nsAReadableString& aValue, PRBool aNotify) {
    return mInner.SetAttribute(aNameSpaceID, aAttribute, aValue, aNotify);
  }
  NS_IMETHOD SetAttr(nsINodeInfo* aNodeInfo,
                     const nsAReadableString& aValue, PRBool aNotify) {
    return mInner.SetAttribute(aNodeInfo, aValue, aNotify);
  }
  NS_IMETHOD UnsetAttr(PRInt32 aNameSpaceID, nsIAtom* aAttribute,
                       PRBool aNotify) {
    return mInner.UnsetAttribute(aNameSpaceID, aAttribute, aNotify);
  }
  NS_IMETHOD GetAttrNameAt(PRInt32 aIndex,
                           PRInt32& aNameSpaceID,
                           nsIAtom*& aName,
                           nsIAtom*& aPrefix) const {
    return mInner.GetAttributeNameAt(aIndex, aNameSpaceID, aName, aPrefix);
  }
  NS_IMETHOD GetAttrCount(PRInt32& aResult) const {
    return mInner.GetAttributeCount(aResult);
  }
  NS_IMETHOD List(FILE* out, PRInt32 aIndent) const;
  NS_IMETHOD DumpContent(FILE* out,
                         PRInt32 aIndent,
                         PRBool aDumpAll) const;
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);
  NS_IMETHOD GetContentID(PRUint32* aID);
  NS_IMETHOD SetContentID(PRUint32 aID);
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange){
    return mInner.RangeAdd(aRange);
  }
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange){
    return mInner.RangeRemove(aRange);
  }
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {
    return mInner.GetRangeList(aResult);
  }
  NS_IMETHOD SetFocus(nsIPresContext* aPresContext) {
    return mInner.SetFocus(aPresContext);
  }
  NS_IMETHOD RemoveFocus(nsIPresContext* aPresContext) {
    return mInner.RemoveFocus(aPresContext);
  }
  NS_IMETHOD GetBindingParent(nsIContent** aContent) {
    return mInner.GetBindingParent(aContent);
  }
  NS_IMETHOD SetBindingParent(nsIContent* aParent) {
    return mInner.SetBindingParent(aParent);
  }
  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager **aResult) {
    return mInner.GetListenerManager(this, aResult);
  }

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const;

  void GetStyleSheetInfo(nsAWritableString& aUrl,
                         nsAWritableString& aTitle,
                         nsAWritableString& aType,
                         nsAWritableString& aMedia,
                         PRBool* aIsAlternate);

protected:
  PRBool GetAttrValue(const char *aAttr, nsString& aValue);

  // XXX Processing instructions are currently implemented by using
  // the generic CharacterData inner object, even though PIs are not
  // character data. This is done simply for convenience and should
  // be changed if this restricts what should be done for character data.
  nsGenericDOMDataNode mInner;
  nsString mTarget;
};

nsresult
NS_NewXMLProcessingInstruction(nsIContent** aInstancePtrResult,
                               const nsAReadableString& aTarget,
                               const nsAReadableString& aData)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIContent* it = new nsXMLProcessingInstruction(aTarget, aData);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsXMLProcessingInstruction::nsXMLProcessingInstruction(const nsAReadableString& aTarget,
                                                       const nsAReadableString& aData) :
  mTarget(aTarget)
{
  NS_INIT_REFCNT();
  mInner.SetData(this, aData);
}

nsXMLProcessingInstruction::~nsXMLProcessingInstruction()
{
}


// QueryInterface implementation for nsXMLProcessingInstruction
NS_INTERFACE_MAP_BEGIN(nsXMLProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsIDOMProcessingInstruction)
  NS_INTERFACE_MAP_ENTRY(nsIDOMLinkStyle)
  NS_INTERFACE_MAP_ENTRY(nsIStyleSheetLinkingElement)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(ProcessingInstruction)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsXMLProcessingInstruction)
NS_IMPL_RELEASE(nsXMLProcessingInstruction)


NS_IMETHODIMP
nsXMLProcessingInstruction::GetTarget(nsAWritableString& aTarget)
{
  aTarget.Assign(mTarget);

  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetData(nsAWritableString& aData)
{
  return mInner.GetData(aData);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SetData(const nsAReadableString& aData)
{
  nsresult rv = mInner.SetData(this, aData);
  UpdateStyleSheet(PR_TRUE);
  return rv;
}

PRBool
nsXMLProcessingInstruction::GetAttrValue(const char *aAttr, nsString& aValue)
{
  nsAutoString data;

  mInner.GetData(data);

  while (1) {
    aValue.Truncate();

    PRInt32 pos = data.Find(aAttr);

    if (pos < 0)
      return PR_FALSE;

    // Cut off data up to the end of the attr string
    data.Cut(0, pos + nsCRT::strlen(aAttr));
    data.CompressWhitespace();

    if (data.First() != '=')
      continue;

    // Cut off the '='
    data.Cut(0, 1);
    data.CompressWhitespace();

    PRUnichar q = data.First();

    if (q != '"' && q != '\'')
      continue;

    // Cut off the first quote character
    data.Cut(0, 1);

    pos = data.FindChar(q);

    if (pos < 0)
      return PR_FALSE;

    data.Left(aValue, pos);

    return PR_TRUE;
  }

  return PR_FALSE;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::processingInstructionTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeInfo(nsINodeInfo*& aResult) const
{
  aResult = nsnull;
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetNodeName(nsAWritableString& aNodeName)
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
  nsString data;
  mInner.GetData(data);

  nsXMLProcessingInstruction* it = new nsXMLProcessingInstruction(mTarget,
                                                                  data);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**)aReturn);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 index;
  for (index = aIndent; --index >= 0; ) fputs("  ", out);

  fprintf(out, "Processing instruction refcount=%d<", mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  tmp.Insert(mTarget.get(), 0);
  fputs(NS_LossyConvertUCS2toASCII(tmp).get(), out);

  fputs(">\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::DumpContent(FILE* out, PRInt32 aIndent,PRBool aDumpAll) const {
  return NS_OK;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::HandleDOMEvent(nsIPresContext* aPresContext,
                                           nsEvent* aEvent,
                                           nsIDOMEvent** aDOMEvent,
                                           PRUint32 aFlags,
                                           nsEventStatus* aEventStatus)
{
  // We should never be getting events
  NS_ASSERTION(0, "event handler called for processing instruction");
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

NS_IMETHODIMP
nsXMLProcessingInstruction::GetContentID(PRUint32* aID)
{
  *aID = 0;
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsXMLProcessingInstruction::SetContentID(PRUint32 aID)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}


NS_IMETHODIMP
nsXMLProcessingInstruction::SizeOf(nsISizeOfHandler* aSizer,
                                   PRUint32* aResult) const
{
  if (!aResult) return NS_ERROR_NULL_POINTER;
#ifdef DEBUG
  PRUint32 sum;
  mInner.SizeOf(aSizer, &sum, sizeof(*this));
  PRUint32 ssize;
  mTarget.SizeOf(aSizer, &ssize);
  sum = sum - sizeof(mTarget) + ssize;
#endif
  return NS_OK;
}

NS_IMETHODIMP_(PRBool)
nsXMLProcessingInstruction::IsContentOfType(PRUint32 aFlags)
{
  return PR_FALSE;
}

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

void
nsXMLProcessingInstruction::GetStyleSheetInfo(nsAWritableString& aUrl,
                                              nsAWritableString& aTitle,
                                              nsAWritableString& aType,
                                              nsAWritableString& aMedia,
                                              PRBool* aIsAlternate)
{
  nsresult rv = NS_OK;

  aUrl.Truncate();
  aTitle.Truncate();
  aType.Truncate();
  aMedia.Truncate();
  *aIsAlternate = PR_FALSE;

  if (!mTarget.Equals(NS_LITERAL_STRING("xml-stylesheet"))) {
    return;
  }

  // xml-stylesheet PI is special only in prolog
  if (!InProlog(this))
    return;

  nsAutoString href, title, type, media, alternate;

  GetAttrValue("href", href);
  if (href.IsEmpty()) {
    // if href is empty then just bail
    return;
  }

  GetAttrValue("title", title);
  title.CompressWhitespace();
  aTitle.Assign(title);

  GetAttrValue("alternate", alternate);

  // if alternate, does it have title?
  if (alternate.Equals(NS_LITERAL_STRING("yes"))) {
    if (aTitle.IsEmpty()) { // alternates must have title
      return;
    } else {
      *aIsAlternate = PR_TRUE;
    }
  }

  GetAttrValue("media", media);
  aMedia.Assign(media);
  ToLowerCase(aMedia); // case sensitivity?

  GetAttrValue("type", type);

  nsAutoString mimeType;
  nsAutoString notUsed;
  SplitMimeType(type, mimeType, notUsed);
  if (!mimeType.IsEmpty() && !mimeType.EqualsIgnoreCase("text/css")) {
    aType.Assign(type);
    return;
  }

  // If we get here we assume that we're loading a css file, so set the
  // type to 'text/css'
  aType.Assign(NS_LITERAL_STRING("text/css"));

  nsCOMPtr<nsIURI> url, baseURL;
  if (mInner.mDocument) {
    mInner.mDocument->GetBaseURL(*getter_AddRefs(baseURL));
  }
  rv = NS_MakeAbsoluteURI(aUrl, href, baseURL);

  if (!*aIsAlternate) {
    if (!aTitle.IsEmpty()) {  // possibly preferred sheet
      nsAutoString prefStyle;
      mInner.mDocument->GetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                                      prefStyle);

      if (prefStyle.IsEmpty()) {
        mInner.mDocument->SetHeaderData(nsHTMLAtoms::headerDefaultStyle,
                                        title);
      }
    }
  }

  return;
 }
