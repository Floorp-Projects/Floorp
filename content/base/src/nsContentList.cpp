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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

#include "nsContentList.h"
#include "nsIContent.h"
#include "nsIDOMNode.h"
#include "nsIScriptGlobalObject.h"
#include "nsIDocument.h"
#include "nsINameSpaceManager.h"
#include "nsIDOMScriptObjectFactory.h"
#include "nsGenericElement.h"

#include "nsLayoutAtoms.h"
#include "nsHTMLAtoms.h" // XXX until atoms get factored into nsLayoutAtoms

nsContentList::nsContentList(nsIDocument *aDocument)
{
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
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
                             const nsString* aData,
                             nsIContent* aRootContent)
{
  mFunc = aFunc;
  if (nsnull != aData) {
    mData = new nsString(*aData);
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
  NS_INIT_REFCNT();
  mScriptObject = nsnull;
  // We don't reference count the reference to the document
  // If the document goes away first, we'll be informed and we
  // can drop our reference.
  // If we go away first, we'll get rid of ourselves from the
  // document's observer list.
  mDocument = aDocument;
  if (nsnull != mDocument) {
    mDocument->AddObserver(this);
  }
  PopulateSelf();
}
 
nsContentList::~nsContentList()
{
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
  }
  
  NS_IF_RELEASE(mMatchAtom);

  if (nsnull != mData) {
    delete mData;
  }
}

nsresult nsContentList::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (nsnull == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMNodeList))) {
    *aInstancePtr = (void*)(nsIDOMNodeList*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIDOMHTMLCollection))) {
    *aInstancePtr = (void*)(nsIDOMHTMLCollection*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsIScriptObjectOwner))) {
    *aInstancePtr = (void*)(nsIScriptObjectOwner*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = (void*)(nsISupports*)(nsIDOMNodeList*)this;
    NS_ADDREF_THIS();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMPL_ADDREF(nsContentList)
NS_IMPL_RELEASE(nsContentList)

NS_IMETHODIMP 
nsContentList::GetLength(PRUint32* aLength)
{
  nsresult result = CheckDocumentExistence();
  if (NS_OK == result) {
    *aLength = mContent.Count();
  }

  return result;
}

NS_IMETHODIMP 
nsContentList::Item(PRUint32 aIndex, nsIDOMNode** aReturn)
{
  nsresult result = CheckDocumentExistence();
  if (NS_OK == result) {
    if (nsnull != mDocument) {
      mDocument->FlushPendingNotifications(); // Flush pending content changes Bug 4891
    }
      
    nsISupports *element = (nsISupports *)mContent.ElementAt(aIndex);
    
    if (nsnull != element) {
      result = element->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aReturn);
    }
    else {
      *aReturn = nsnull;
    }
  }
  
  return result;
}

NS_IMETHODIMP 
nsContentList::NamedItem(const nsAReadableString& aName, nsIDOMNode** aReturn)
{
  nsresult result = CheckDocumentExistence();
  
  if (NS_OK == result) {
    if (nsnull != mDocument) {
      mDocument->FlushPendingNotifications(); // Flush pending content changes Bug 4891
    }

    PRInt32 i, count = mContent.Count();

    for (i = 0; i < count; i++) {
      nsIContent *content = (nsIContent *)mContent.ElementAt(i);
      if (nsnull != content) {
        nsAutoString name;
        // XXX Should it be an EqualsIgnoreCase?
        if (((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name))) ||
            ((content->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::id, name) == NS_CONTENT_ATTR_HAS_VALUE) &&
             (aName.Equals(name)))) {
          return content->QueryInterface(NS_GET_IID(nsIDOMNode), (void **)aReturn);
        }
      }
    }
  }

  *aReturn = nsnull;
  return result;
}

NS_IMETHODIMP 
nsContentList::GetScriptObject(nsIScriptContext *aContext, void** aScriptObject)
{
  nsresult res = NS_OK;
  nsIScriptGlobalObject *global = aContext->GetGlobalObject();

  if (nsnull == mScriptObject) {
    nsIDOMScriptObjectFactory *factory;
    
    res = nsGenericElement::GetScriptObjectFactory(&factory);
    if (NS_OK != res) {
      return res;
    }

    res = factory->NewScriptHTMLCollection(aContext, 
                                           (nsISupports*)(nsIDOMHTMLCollection*)this, 
                                           global, 
                                           (void**)&mScriptObject);
    NS_RELEASE(factory);
  }
  *aScriptObject = mScriptObject;

  NS_RELEASE(global);
  return res;
}

NS_IMETHODIMP 
nsContentList::SetScriptObject(void *aScriptObject)
{
  mScriptObject = aScriptObject;
  return NS_OK;
}


NS_IMETHODIMP
nsContentList::ContentAppended(nsIDocument *aDocument,
                               nsIContent* aContainer,
                               PRInt32     aNewIndexInContainer)
{
  PRInt32 i, count;
  aContainer->ChildCount(count);
  if ((count > 0) && IsDescendantOfRoot(aContainer)) {
    PRBool repopulate = PR_FALSE;
    for (i = aNewIndexInContainer; i <= count-1; i++) {
      nsIContent *content;
      aContainer->ChildAt(i, content);
      if (mMatchAll || MatchSelf(content)) {
        repopulate = PR_TRUE;
      }
      NS_RELEASE(content);
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
  if (nsnull != mDocument) {
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
  else if (nsnull != mFunc) {
    *aMatch = (*mFunc)(aContent, mData);
  }

  return NS_OK;
}

nsresult
nsContentList::Add(nsIContent *aContent)
{
  // Shouldn't hold a reference since we'll be
  // told when the content leaves the document or
  // the document will be destroyed.
  mContent.AppendElement(aContent);
  
  return NS_OK;
}

nsresult
nsContentList::Remove(nsIContent *aContent)
{
  mContent.RemoveElement(aContent);
  
  return NS_OK;
}

nsresult
nsContentList::IndexOf(nsIContent *aContent, PRInt32& aIndex)
{
  aIndex = mContent.IndexOf(aContent);

  return NS_OK;
}

nsresult
nsContentList::Reset()
{
  mContent.Clear();
  
  return NS_OK;
}

// If we were created outside the context of a document and we
// have root content, then check if our content has been added 
// to a document yet. If so, we'll become an observer of the document.
nsresult
nsContentList::CheckDocumentExistence()
{
  nsresult result = NS_OK;
  if ((nsnull == mDocument) && (nsnull != mRootContent)) {
    result = mRootContent->GetDocument(mDocument);
    if (nsnull != mDocument) {
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

// Add all elements in this subtree that match to
// our list.
void 
nsContentList::PopulateWith(nsIContent *aContent, PRBool aIncludeRoot)
{
  PRBool match;
  PRInt32 i, count;

  if (aIncludeRoot) {
    Match(aContent, &match);
    if (match) {
      Add(aContent);
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
  if (nsnull != mRootContent) {
    PopulateWith(mRootContent, PR_FALSE);
  }
  else if (nsnull != mDocument) {
    nsIContent *root;
    root = mDocument->GetRootContent();
    PopulateWith(root, PR_TRUE);
    NS_RELEASE(root);
  }
}

// Is the specified element a descendant of the root? If there
// is no root, then yes. Otherwise keep tracing up the tree from
// the element till we find our root, or until we reach the
// document root.
PRBool
nsContentList::IsDescendantOfRoot(nsIContent* aContainer) 
{
  if (nsnull == mRootContent) {
    return PR_TRUE;
  }
  else if (mRootContent == aContainer) {
    return PR_TRUE;
  }
  else if (nsnull == aContainer) {
    return PR_FALSE;
  }
  else {
    nsIContent* parent;
    PRBool ret;

    aContainer->GetParent(parent);
    ret = IsDescendantOfRoot(parent);
    NS_IF_RELEASE(parent);

    return ret;
  }
}

// Does this subtree contain the root?
PRBool
nsContentList::ContainsRoot(nsIContent* aContent)
{
  if (nsnull == mRootContent) {
    return PR_FALSE;
  }
  else if (mRootContent == aContent) {
    return PR_TRUE;
  }
  else {
    PRInt32 i, count;

    aContent->ChildCount(count);
    for (i = 0; i < count; i++) {
      nsIContent *child;
      aContent->ChildAt(i, child);
      if (ContainsRoot(child)) {
        NS_RELEASE(child);
        return PR_TRUE;
      }
      NS_RELEASE(child);
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
  if (nsnull != mDocument) {
    mDocument->RemoveObserver(this);
    mDocument = nsnull;
  }
}
