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
#include "nsIDOMComment.h"
#include "nsGenericDOMDataNode.h"
#include "nsIDOMEventReceiver.h"
#include "nsIContent.h"
#include "nsLayoutAtoms.h"
#include "nsISelection.h"
#include "nsISelectionPrivate.h"
#include "nsIDocument.h"
#include "nsIEnumerator.h"
#include "nsCOMPtr.h"
#include "nsIDOMRange.h"
#include "nsString.h"

#include "nsContentUtils.h"


class nsCommentNode : public nsIDOMComment,
                      public nsITextContent
{
public:
  nsCommentNode();
  virtual ~nsCommentNode();

  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIDOMNode
  NS_IMPL_NSIDOMNODE_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMCharacterData
  NS_IMPL_NSIDOMCHARACTERDATA_USING_GENERIC_DOM_DATA(mInner)

  // nsIDOMComment

  // nsIContent
  //NS_IMPL_ICONTENT_USING_GENERIC_DOM_DATA(mInner)

  NS_IMETHOD GetDocument(nsIDocument*& aResult) const {
    return mInner.GetDocument(aResult);
  }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep, PRBool aCompileEventHandlers) {
    return mInner.SetDocument(aDocument, aDeep, aCompileEventHandlers);
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
  NS_IMETHOD GetNodeInfo(nsINodeInfo*& aResult) const {
    aResult = nsnull; return NS_OK;
  }
  NS_IMETHOD NormalizeAttrString(const nsAReadableString& aStr,
                                 nsINodeInfo*& aNodeInfo) {
    aNodeInfo = nsnull;
    return NS_OK;
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
  NS_IMETHOD DumpContent(FILE* out = stdout, PRInt32 aIndent = 0,PRBool aDumpAll=PR_TRUE) const {
    return NS_OK;
  }
  NS_IMETHOD HandleDOMEvent(nsIPresContext* aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus* aEventStatus);

  NS_IMETHOD GetContentID(PRUint32* aID) {
    *aID = mContentID;
    return NS_OK;
  }
  NS_IMETHOD SetContentID(PRUint32 aID) {
    mContentID = aID;
    return NS_OK;
  }

  NS_IMETHOD RangeAdd(nsIDOMRange& aRange){
    return mInner.RangeAdd(aRange);
  }
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange){
    return mInner.RangeRemove(aRange);
  }
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const {
    return mInner.GetRangeList(aResult);
  }      
  NS_IMETHOD SetFocus(nsIPresContext* aContext) {
    return mInner.SetFocus(aContext);
  }
  NS_IMETHOD RemoveFocus(nsIPresContext* aContext) {
    return mInner.RemoveFocus(aContext);
  }

  NS_IMETHOD GetBindingParent(nsIContent** aContent) {
    return mInner.GetBindingParent(aContent);
  }

  NS_IMETHOD SetBindingParent(nsIContent* aParent) {
    return mInner.SetBindingParent(aParent);
  }

  NS_IMETHOD_(PRBool) IsContentOfType(PRUint32 aFlags) {
    return PR_FALSE;
  }

  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aResult) {
    return mInner.GetListenerManager(this, aResult);
  }

  NS_IMETHOD SizeOf(nsISizeOfHandler* aSizer, PRUint32* aResult) const {
    if (!aResult) {
      return NS_ERROR_NULL_POINTER;
    }
#ifdef DEBUG
    *aResult = sizeof(*this);
#else
    *aResult = 0;
#endif
    return NS_OK;
  }

  NS_IMETHOD GetText(const nsTextFragment** aFragmentsResult)
    { return mInner.GetText(aFragmentsResult); }
  NS_IMETHOD GetTextLength(PRInt32* aLengthResult) {
    return mInner.GetTextLength(aLengthResult);
  }
  NS_IMETHOD CopyText(nsAWritableString& aResult) {
    return mInner.CopyText(aResult);
  }
  NS_IMETHOD SetText(const PRUnichar* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify);
  NS_IMETHOD SetText(const nsAReadableString& aStr,
                     PRBool aNotify);
  NS_IMETHOD SetText(const char* aBuffer,
                     PRInt32 aLength,
                     PRBool aNotify);
  NS_IMETHOD IsOnlyWhitespace(PRBool* aResult)
    { return mInner.IsOnlyWhitespace(aResult); }
  NS_IMETHOD CloneContent(PRBool aCloneText, nsITextContent** aClone); 

protected:
  nsGenericDOMDataNode mInner;
  PRUint32 mContentID;
};

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
  return it->QueryInterface(NS_GET_IID(nsIContent), (void **) aInstancePtrResult);
}

nsCommentNode::nsCommentNode()
{
  NS_INIT_REFCNT();
  mContentID = 0;
}

nsCommentNode::~nsCommentNode()
{
}


// QueryInterface implementation for nsCommentNode
NS_INTERFACE_MAP_BEGIN(nsCommentNode)
  NS_INTERFACE_MAP_ENTRY_DOM_DATA()
  NS_INTERFACE_MAP_ENTRY(nsITextContent)
  NS_INTERFACE_MAP_ENTRY(nsIDOMComment)
  NS_INTERFACE_MAP_ENTRY(nsIDOMCharacterData)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(Comment)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsCommentNode)
NS_IMPL_RELEASE(nsCommentNode)


NS_IMETHODIMP 
nsCommentNode::GetTag(nsIAtom*& aResult) const
{
  aResult = nsLayoutAtoms::commentTagName;
  NS_ADDREF(aResult);
  return NS_OK;
}

NS_IMETHODIMP
nsCommentNode::GetNodeName(nsAWritableString& aNodeName)
{
  aNodeName.Assign(NS_LITERAL_STRING("#comment"));
  return NS_OK;
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
  nsresult result = NS_OK;
  nsCommentNode* it = new nsCommentNode();
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  // XXX Increment the ref count before calling any
  // methods. If they do a QI and then a Release()
  // the instance will be deleted.
  result = it->QueryInterface(NS_GET_IID(nsIDOMNode), (void**) aReturn);
  if (NS_FAILED(result)) {
    return result;
  }
  nsAutoString data;
  result = GetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  result = it->SetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  return result;
}

NS_IMETHODIMP 
nsCommentNode::CloneContent(PRBool aCloneText, nsITextContent** aReturn)
{
  nsresult result = NS_OK;
  nsCommentNode* it;
  NS_NEWXPCOM(it, nsCommentNode);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  result = it->QueryInterface(NS_GET_IID(nsITextContent), (void**) aReturn);
  if (NS_FAILED(result) || !aCloneText) {
    return result;
  }
  nsAutoString data;
  result = GetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  result = it->SetData(data);
  if (NS_FAILED(result)) {
    NS_RELEASE(*aReturn);
    return result;
  }
  return result;
}

NS_IMETHODIMP
nsCommentNode::List(FILE* out, PRInt32 aIndent) const
{
  NS_PRECONDITION(nsnull != mInner.mDocument, "bad content");

  PRInt32 indx;
  for (indx = aIndent; --indx >= 0; ) fputs("  ", out);

  fprintf(out, "Comment@%p refcount=%d<!--", this, mRefCnt);

  nsAutoString tmp;
  mInner.ToCString(tmp, 0, mInner.mText.GetLength());
  fputs(tmp, out);

  fputs("-->\n", out);
  return NS_OK;
}

NS_IMETHODIMP
nsCommentNode::HandleDOMEvent(nsIPresContext* aPresContext,
                              nsEvent* aEvent,
                              nsIDOMEvent** aDOMEvent,
                              PRUint32 aFlags,
                              nsEventStatus* aEventStatus)
{
  return mInner.HandleDOMEvent(aPresContext, aEvent, aDOMEvent,
                               aFlags, aEventStatus);
}

// This would ideally be done by the parser, but for the sake
// of "genericity" it's being done in the comment content code
static void
StripCommentDelimiters(nsString& aCommentString)
{
  PRInt32 offset;
  static char* kCommentStart = "<!";
  static char* kCommentEnd = "->";
  static char* kCommentAlternateEnd = "--!>";
  static char kMinus = '-';

  offset = aCommentString.Find(kCommentStart);
  if (-1 != offset) {
    // Take up to 2 '-' characters
    offset += strlen(kCommentStart);
    if (kMinus == aCommentString.CharAt(offset)) {
      offset++;
      if (kMinus == aCommentString.CharAt(offset)) {
        offset++;
      }
    }
    aCommentString.Cut(0, offset);
  }

  offset = aCommentString.RFind(kCommentEnd);
  if (offset > 0) {
    // Take up to 1 more '-'
    if (kMinus == aCommentString.CharAt(offset-1)) {
      offset--;
    }
    aCommentString.Cut(offset, aCommentString.Length()-offset);
  }
  else {
    offset = aCommentString.RFind(kCommentAlternateEnd);
    if (-1 != offset) {
      aCommentString.Cut(offset, aCommentString.Length()-offset);
    }
  }
}

NS_IMETHODIMP 
nsCommentNode::SetText(const PRUnichar* aBuffer,
                       PRInt32 aLength,
                       PRBool aNotify)
{
  nsAutoString str(aBuffer);

  StripCommentDelimiters(str);
  return mInner.SetText(this, str.get(), str.Length(), aNotify);
}

NS_IMETHODIMP 
nsCommentNode::SetText(const nsAReadableString& aStr,
                       PRBool aNotify)
{
  nsAutoString str(aStr);

  StripCommentDelimiters(str);
  return mInner.SetText(this, str.get(), str.Length(), aNotify);
}

NS_IMETHODIMP
nsCommentNode::SetText(const char* aBuffer,
                       PRInt32 aLength,
                       PRBool aNotify)
{
  nsAutoString str; str.AssignWithConversion(aBuffer);

  StripCommentDelimiters(str);
  return mInner.SetText(this, str.get(), str.Length(), aNotify);
}
