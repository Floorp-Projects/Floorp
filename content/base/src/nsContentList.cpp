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
 * The Original Code is mozilla.org code.
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

#include "nsContentList.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsGenericElement.h"

#include "nsContentUtils.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms

// Form related includes
#include "nsIDOMHTMLFormElement.h"
#include "nsIContentList.h"

nsBaseContentList::nsBaseContentList()
{
  NS_INIT_REFCNT();
}

nsBaseContentList::~nsBaseContentList()
{
  // mElements only has weak references to the content objects so we
  // don't need to do any cleanup here.
}


// QueryInterface implementation for nsBaseContentList
NS_INTERFACE_MAP_BEGIN(nsBaseContentList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMNodeList)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIDOMNodeList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(NodeList)
NS_INTERFACE_MAP_END


NS_IMPL_ADDREF(nsBaseContentList)
NS_IMPL_RELEASE(nsBaseContentList)


NS_IMETHODIMP
nsBaseContentList::GetLength(PRUint32* aLength)
{
  *aLength = mElements.Count();

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsISupports *tmp = NS_REINTERPRET_CAST(nsISupports *,
                                         mElements.ElementAt(aIndex));

  if (!tmp) {
    *aReturn = nsnull;

    return NS_OK;
  }

  return CallQueryInterface(tmp, aReturn);
}

NS_IMETHODIMP
nsBaseContentList::AppendElement(nsIContent *aContent)
{
  // Shouldn't hold a reference since we'll be told when the content
  // leaves the document or the document will be destroyed.
  mElements.AppendElement(aContent);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::RemoveElement(nsIContent *aContent)
{
  mElements.RemoveElement(aContent);
  
  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::IndexOf(nsIContent *aContent, PRInt32& aIndex)
{
  aIndex = mElements.IndexOf(aContent);

  return NS_OK;
}

NS_IMETHODIMP
nsBaseContentList::Reset()
{
  mElements.Clear();

  return NS_OK;
}

// nsFormContentList

// This helper function checks if aContent is in some way associated
// with aForm, this check is only successful if the form is a
// container (and a form is a container as long as the document is
// wellformed). If the form is a container the only elements that are
// considerd to be associated with a form are the elements that are
// contained within the form. If the form is a leaf element then all
// the elements will be accepted into this list.

static PRBool BelongsInForm(nsIDOMHTMLFormElement *aForm,
                            nsIContent *aContent)
{
  nsCOMPtr<nsIContent> form(do_QueryInterface(aForm));

  if (!form) {
    NS_WARNING("This should not happen, form is not an nsIContent!");

    return PR_TRUE;
  }

  if (form.get() == aContent) {
    // The list for aForm contains the form itself, forms should not
    // be reachable by name in the form namespace, so we return false
    // here.

    return PR_FALSE;
  }

  nsCOMPtr<nsIContent> content;

  aContent->GetParent(*getter_AddRefs(content));

  while (content) {
    if (content == form) {
      // aContent is contained within the form so we return true.

      return PR_TRUE;
    }

    nsCOMPtr<nsIAtom> tag;

    content->GetTag(*getter_AddRefs(tag));

    if (tag.get() == nsHTMLAtoms::form) {
      // The child is contained within a form, but not the right form
      // so we ignore it.

      return PR_FALSE;
    }

    nsIContent *tmp = content;

    tmp->GetParent(*getter_AddRefs(content));
  }

  PRInt32 count = 0;

  form->ChildCount(count);

  if (!count) {
    // The form is a leaf and aContent wasn't inside any other form so
    // we return true

    return PR_TRUE;
  }

  // The form is a container but aContent wasn't inside the form,
  // return false

  return PR_FALSE;
}

nsFormContentList::nsFormContentList(nsIDOMHTMLFormElement *aForm,
                                     nsBaseContentList& aContentList)
  : nsBaseContentList()
{
  NS_INIT_REFCNT();

  // move elements that belong to mForm into this content list

  PRUint32 i, length = 0;
  nsCOMPtr<nsIDOMNode> item;

  aContentList.GetLength(&length);

  for (i = 0; i < length; i++) {
    aContentList.Item(i, getter_AddRefs(item));

    nsCOMPtr<nsIContent> c(do_QueryInterface(item));

    if (c && BelongsInForm(aForm, c)) {
      AppendElement(c);
    }
  }
}

nsFormContentList::~nsFormContentList()
{
  Reset();
}

NS_IMETHODIMP
nsFormContentList::AppendElement(nsIContent *aContent)
{
  NS_ADDREF(aContent);

  return nsBaseContentList::AppendElement(aContent);
}

NS_IMETHODIMP
nsFormContentList::RemoveElement(nsIContent *aContent)
{
  PRInt32 i = mElements.IndexOf(aContent);

  if (i >= 0) {
    nsIContent *content = NS_STATIC_CAST(nsIContent *, mElements.ElementAt(i));

    NS_RELEASE(content);

    mElements.RemoveElementAt(i);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsFormContentList::Reset()
{
  PRInt32 i, length = mElements.Count();

  for (i = 0; i < length; i++) {
    nsIContent *content = NS_STATIC_CAST(nsIContent *, mElements.ElementAt(i));

    NS_RELEASE(content);
  }

  return nsBaseContentList::Reset();
}


// nsContentList implementation

nsContentList::nsContentList(const nsContentList& aContentList)
  : nsBaseContentList()
{
  mFunc = aContentList.mFunc;
  mMatchAtom = aContentList.mMatchAtom;
  mDocument = aContentList.mDocument;

  if (aContentList.mData) {
    mData = new nsString(*aContentList.mData);
  } else {
    mData = nsnull;
  }

  mMatchAll = aContentList.mMatchAll;
  mRootContent = aContentList.mRootContent;
  mElements = aContentList.mElements;
}

nsContentList::nsContentList(nsIDocument *aDocument)
  : nsBaseContentList()
{
  mFunc = nsnull;
  mMatchAtom = nsnull;
  mDocument = aDocument;
  mData = nsnull;
  mMatchAll = PR_FALSE;
  mRootContent = nsnull;
}

nsContentList::nsContentList(nsIDocument *aDocument,
                             nsIAtom* aMatchAtom,
                             PRInt32 aMatchNameSpaceId,
                             nsIContent* aRootContent)
  : nsBaseContentList()
{
  mMatchAtom = aMatchAtom;
  NS_IF_ADDREF(mMatchAtom);
  if (nsLayoutAtoms::wildcard == mMatchAtom) {
    mMatchAll = PR_TRUE;
  }
  else {
    mMatchAll = PR_FALSE;
  }
  mMatchNameSpaceId = aMatchNameSpaceId;
  mFunc = nsnull;
  mData = nsnull;
  mRootContent = aRootContent;
  Init(aDocument);
}

nsContentList::nsContentList(nsIDocument *aDocument,
                             nsContentListMatchFunc aFunc,
                             const nsAReadableString& aData,
                             nsIContent* aRootContent)
  : nsBaseContentList()
{
  mFunc = aFunc;
  if (!aData.IsEmpty()) {
    mData = new nsString(aData);
    // If this fails, fail silently
  }
  else {
    mData = nsnull;
  }
  mMatchAtom = nsnull;
  mRootContent = aRootContent;
  mMatchAll = PR_FALSE;
  Init(aDocument);
}

void nsContentList::Init(nsIDocument *aDocument)
{
  // We don't reference count the reference to the document
  // If the document goes away first, we'll be informed and we
  // can drop our reference.
  // If we go away first, we'll get rid of ourselves from the
  // document's observer list.
  mDocument = aDocument;
  if (mDocument) {
    mDocument->AddObserver(this);
  }
  PopulateSelf();
}

nsContentList::~nsContentList()
{
  if (mDocument) {
    mDocument->RemoveObserver(this);
  }
  
  NS_IF_RELEASE(mMatchAtom);

  delete mData;
}


// QueryInterface implementation for nsContentList
NS_INTERFACE_MAP_BEGIN(nsContentList)
  NS_INTERFACE_MAP_ENTRY(nsIDOMHTMLCollection)
  NS_INTERFACE_MAP_ENTRY(nsIContentList)
  NS_INTERFACE_MAP_ENTRY_CONTENT_CLASSINFO(HTMLCollection)
NS_INTERFACE_MAP_END_INHERITING(nsBaseContentList)


NS_IMPL_ADDREF_INHERITED(nsContentList, nsBaseContentList)
NS_IMPL_RELEASE_INHERITED(nsContentList, nsBaseContentList)


NS_IMETHODIMP 
nsContentList::GetLength(PRUint32* aLength, PRBool aDoFlush)
{
  nsresult result = CheckDocumentExistence();
  if (NS_SUCCEEDED(result)) {
    if (mDocument && aDoFlush) {
      mDocument->FlushPendingNotifications(PR_FALSE);
    }

    *aLength = mElements.Count();
  }

  return result;
}

NS_IMETHODIMP 
nsContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn, PRBool aDoFlush)
{
  nsresult result = CheckDocumentExistence();
  if (NS_SUCCEEDED(result)) {
    if (mDocument && aDoFlush) {
      // Flush pending content changes Bug 4891
      mDocument->FlushPendingNotifications(PR_FALSE);
    }

    nsISupports *element = NS_STATIC_CAST(nsISupports *,
                                          mElements.ElementAt(aIndex));

    if (element) {
      result = CallQueryInterface(element, aReturn);
    }
    else {
      *aReturn = nsnull;
    }
  }

  return result;
}

NS_IMETHODIMP
nsContentList::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn, PRBool aDoFlush)
{
  nsresult result = CheckDocumentExistence();

  if (NS_SUCCEEDED(result)) {
    if (mDocument && aDoFlush) {
      mDocument->FlushPendingNotifications(PR_FALSE); // Flush pending content changes Bug 4891
    }

    PRInt32 i, count = mElements.Count();

    for (i = 0; i < count; i++) {
      nsIContent *content = NS_STATIC_CAST(nsIContent *,
                                           mElements.ElementAt(i));
      if (content) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?
        if (((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::id, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          return CallQueryInterface(content, aReturn);
        }
      }
    }
  }

  *aReturn = nsnull;
  return result;
}

NS_IMETHODIMP
nsContentList::IndexOf(nsIContent *aContent, PRInt32& aIndex, PRBool aDoFlush)
{
  nsresult result = CheckDocumentExistence();
  if (NS_SUCCEEDED(result)) {
    if (mDocument && aDoFlush) {
      mDocument->FlushPendingNotifications(PR_FALSE);
    }

    aIndex = mElements.IndexOf(aContent);
  }

  return result;
}

NS_IMETHODIMP
nsContentList::GetLength(PRUint32* aLength)
{
  return GetLength(aLength, PR_TRUE);
}

NS_IMETHODIMP
nsContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  return Item(aIndex, aReturn, PR_TRUE);
}

NS_IMETHODIMP
nsContentList::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  return NamedItem(aName, aReturn, PR_TRUE);
}

NS_IMETHODIMP 
nsContentList::ContentAppended(nsIDocument *aDocument, nsIContent* aContainer,
                               PRInt32 aNewIndexInContainer)
{
  PRInt32 i, count;

  aContainer->ChildCount(count);

  if ((count > 0) && IsDescendantOfRoot(aContainer)) {
    PRBool repopulate = PR_FALSE;

    for (i = aNewIndexInContainer; i <= count-1; i++) {
      nsCOMPtr<nsIContent> content;
      aContainer->ChildAt(i, *getter_AddRefs(content));
      if (mMatchAll || MatchSelf(content)) {
        repopulate = PR_TRUE;
      }
    }
    if (repopulate) {
      PopulateSelf();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP 
nsContentList::ContentInserted(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aChild,
                               PRInt32 aIndexInContainer)
{
  if (IsDescendantOfRoot(aContainer)) {
    if (mMatchAll || MatchSelf(aChild)) {
      PopulateSelf();
    }
  }

  return NS_OK;
}
 
NS_IMETHODIMP
nsContentList::ContentReplaced(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               nsIContent* aOldChild,
                               nsIContent* aNewChild,
                               PRInt32 aIndexInContainer)
{
  if (IsDescendantOfRoot(aContainer)) {
    if (mMatchAll || MatchSelf(aOldChild) || MatchSelf(aNewChild)) {
      PopulateSelf();
    }
  }
  else if (ContainsRoot(aOldChild)) {
    DisconnectFromDocument();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::ContentRemoved(nsIDocument *aDocument,
                              nsIContent* aContainer,
                              nsIContent* aChild,
                              PRInt32 aIndexInContainer)
{
  if (IsDescendantOfRoot(aContainer) && MatchSelf(aChild)) {
    PopulateSelf();
  }
  else if (ContainsRoot(aChild)) {
    DisconnectFromDocument();
  }

  return NS_OK;
}

NS_IMETHODIMP
nsContentList::DocumentWillBeDestroyed(nsIDocument *aDocument)
{
  if (mDocument) {
    aDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
  Reset();
  
  return NS_OK;
}


// Returns whether the content element matches the
// criterion
nsresult
nsContentList::Match(nsIContent *aContent, PRBool *aMatch)
{
  *aMatch = PR_FALSE;

  if (!aContent) {
    return NS_OK;
  }

  if (mMatchAtom) {
    nsCOMPtr<nsINodeInfo> ni;
    aContent->GetNodeInfo(*getter_AddRefs(ni));

    if (!ni)
      return NS_OK;

    nsCOMPtr<nsIDOMNode> node(do_QueryInterface(aContent));

    if (!node)
      return NS_OK;

    PRUint16 type;
    node->GetNodeType(&type);

    if (type != nsIDOMNode::ELEMENT_NODE)
      return NS_OK;

    if (mMatchNameSpaceId == kNameSpaceID_Unknown) {
      if (mMatchAll || ni->Equals(mMatchAtom)) {
        *aMatch = PR_TRUE;
      }
    } else if ((mMatchAll && ni->NamespaceEquals(mMatchNameSpaceId)) ||
               ni->Equals(mMatchAtom, mMatchNameSpaceId)) {
      *aMatch = PR_TRUE;
    }
  }
  else if (mFunc) {
    *aMatch = (*mFunc)(aContent, mData);
  }

  return NS_OK;
}

// If we were created outside the context of a document and we
// have root content, then check if our content has been added 
// to a document yet. If so, we'll become an observer of the document.
nsresult
nsContentList::CheckDocumentExistence()
{
  nsresult result = NS_OK;
  if (!mDocument && mRootContent) {
    result = mRootContent->GetDocument(mDocument);
    if (mDocument) {
      mDocument->AddObserver(this);
      PopulateSelf();
    }
  }

  return result;
}

// Match recursively. See if anything in the subtree
// matches the criterion.
PRBool 
nsContentList::MatchSelf(nsIContent *aContent)
{
  PRBool match;
  PRInt32 i, count;

  Match(aContent, &match);
  if (match) {
    return PR_TRUE;
  }
  
  aContent->ChildCount(count);
  for (i = 0; i < count; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    if (MatchSelf(child)) {
      NS_RELEASE(child);
      return PR_TRUE;
    }
    NS_RELEASE(child);
  }
  
  return PR_FALSE;
}

// Add all elements in this subtree that match to our list.
void 
nsContentList::PopulateWith(nsIContent *aContent, PRBool aIncludeRoot)
{
  PRBool match;
  PRInt32 i, count;

  if (aIncludeRoot) {
    Match(aContent, &match);
    if (match) {
      mElements.AppendElement(aContent);
    }
  }
  
  aContent->ChildCount(count);
  for (i = 0; i < count; i++) {
    nsIContent *child;
    aContent->ChildAt(i, child);
    PopulateWith(child, PR_TRUE);
    NS_RELEASE(child);
  }
}

// Clear out our old list and build up a new one
void 
nsContentList::PopulateSelf()
{
  Reset();
  if (mRootContent) {
    PopulateWith(mRootContent, PR_FALSE);
  }
  else if (mDocument) {
    nsIContent *root = nsnull;
    mDocument->GetRootContent(&root);
    if (root) {
      PopulateWith(root, PR_TRUE);
      NS_RELEASE(root);
    }
  }
}

// Is the specified element a descendant of the root? If there
// is no root, then yes. Otherwise keep tracing up the tree from
// the element till we find our root, or until we reach the
// document root.
PRBool
nsContentList::IsDescendantOfRoot(nsIContent* aContainer) 
{
  if (!mRootContent) {
    return PR_TRUE;
  }
  else if (mRootContent == aContainer) {
    return PR_TRUE;
  }
  else if (!aContainer) {
    return PR_FALSE;
  }
  else {
    nsCOMPtr<nsIContent> parent;
    PRBool ret;

    aContainer->GetParent(*getter_AddRefs(parent));
    ret = IsDescendantOfRoot(parent);

    return ret;
  }
}

// Does this subtree contain the root?
PRBool
nsContentList::ContainsRoot(nsIContent* aContent)
{
  if (!mRootContent) {
    return PR_FALSE;
  }
  else if (mRootContent == aContent) {
    return PR_TRUE;
  }
  else {
    PRInt32 i, count;

    aContent->ChildCount(count);
    for (i = 0; i < count; i++) {
      nsCOMPtr<nsIContent> child;

      aContent->ChildAt(i, *getter_AddRefs(child));

      if (ContainsRoot(child)) {
        return PR_TRUE;
      }
    }
    
    return PR_FALSE;
  }
}

// Our root content has been disconnected from the 
// document, so stop observing. The list then becomes
// a snapshot rather than a dynamic list.
void 
nsContentList::DisconnectFromDocument()
{
  if (mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
}
