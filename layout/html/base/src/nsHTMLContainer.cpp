/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsIHTMLDocument.h"
#include "nsIDocument.h"
#include "nsIAtom.h"
#include "nsIArena.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsBlockFrame.h"
#include "nsIPresContext.h"
#include "nsIDeviceContext.h"
#include "nsHTMLIIDs.h"
#include "nsHTMLAtoms.h"
#include "nsIHTMLAttributes.h"
#include "nsIHTMLStyleSheet.h"
#include "nsDOMNodeList.h"
#include "nsUnitConversion.h"
#include "nsStyleUtil.h"
#include "nsIURL.h"
#include "prprf.h"
#include "nsISizeOfHandler.h"


static NS_DEFINE_IID(kIHTMLDocumentIID, NS_IHTMLDOCUMENT_IID);

nsresult
NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                    nsIAtom* aTag)
{
  nsHTMLContainer* it = new nsHTMLContainer(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsresult
NS_NewHTMLContainer(nsIHTMLContent** aInstancePtrResult,
                    nsIArena* aArena, nsIAtom* aTag)
{
  nsHTMLContainer* it = new(aArena) nsHTMLContainer(aTag);
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

nsHTMLContainer::nsHTMLContainer()
{
  mChildNodes = nsnull;
}

nsHTMLContainer::nsHTMLContainer(nsIAtom* aTag)
  : nsHTMLTagContent(aTag)
{
  mChildNodes = nsnull;
}

nsHTMLContainer::~nsHTMLContainer()
{
  PRInt32 n = mChildren.Count();
  for (PRInt32 i = 0; i < n; i++) {
    nsIContent* kid = (nsIContent*) mChildren.ElementAt(i);
    NS_RELEASE(kid);
  }
  
  if (nsnull != mChildNodes) {
    mChildNodes->ReleaseContent();
    NS_RELEASE(mChildNodes);
  }
}

NS_IMETHODIMP
nsHTMLContainer::SizeOf(nsISizeOfHandler* aHandler) const
{
  aHandler->Add(sizeof(*this));
  nsHTMLContainer::SizeOfWithoutThis(aHandler);
  return NS_OK;
}

void
nsHTMLContainer::SizeOfWithoutThis(nsISizeOfHandler* aHandler) const
{
  aHandler->Add((size_t) (- (PRInt32)sizeof(mChildren) ) );
  mChildren.SizeOf(aHandler);

  PRInt32 i, n = mChildren.Count();
  for (i = 0; i < n; i++) {
    nsIContent* child = (nsIContent*) mChildren[i];
    if (!aHandler->HaveSeen(child)) {
      child->SizeOf(aHandler);
    }
  }
}

NS_IMETHODIMP
nsHTMLContainer::CanContainChildren(PRBool& aResult) const
{
  aResult = PR_TRUE;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ChildCount(PRInt32& aCount) const
{
  aCount = mChildren.Count();
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != child) {
    NS_ADDREF(child);
  }
  aResult = child;
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
{
  NS_PRECONDITION(nsnull != aPossibleChild, "null ptr");
  aIndex = mChildren.IndexOf(aPossibleChild);
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                               PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  PRBool rv = mChildren.InsertElementAt(aKid, aIndex);/* XXX fix up void array api to use nsresult's*/
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentInserted(this, aKid, aIndex);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                                PRBool aNotify)
{
  NS_PRECONDITION(nsnull != aKid, "null ptr");
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  PRBool rv = mChildren.ReplaceElementAt(aKid, aIndex);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentReplaced(this, oldKid, aKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::AppendChildTo(nsIContent* aKid, PRBool aNotify)
{
  NS_PRECONDITION((nsnull != aKid) && (aKid != this), "null ptr");
  PRBool rv = mChildren.AppendElement(aKid);
  if (rv) {
    NS_ADDREF(aKid);
    aKid->SetParent(this);
    nsIDocument* doc = mDocument;
    if (nsnull != doc) {
      aKid->SetDocument(doc);
      if (aNotify) {
        doc->ContentAppended(this, mChildren.Count() - 1);
      }
    }
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
{
  nsIContent* oldKid = (nsIContent*) mChildren.ElementAt(aIndex);
  if (nsnull != oldKid ) {
    nsIDocument* doc = mDocument;
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentWillBeRemoved(this, oldKid, aIndex);
      }
    }
    PRBool rv = mChildren.RemoveElementAt(aIndex);
    if (aNotify) {
      if (nsnull != doc) {
        doc->ContentHasBeenRemoved(this, oldKid, aIndex);
      }
    }
    oldKid->SetDocument(nsnull);
    oldKid->SetParent(nsnull);
    NS_RELEASE(oldKid);
  }

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLContainer::Compact()
{
  //XXX I'll turn this on in a bit... mChildren.Compact();
  return NS_OK;
}

//----------------------------------------------------------------------

NS_IMETHODIMP
nsHTMLContainer::SetAttribute(nsIAtom* aAttribute,
                              const nsString& aValue,
                              PRBool aNotify)
{
  return nsHTMLTagContent::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsHTMLContainer::AttributeToString(nsIAtom* aAttribute,
                                   nsHTMLValue& aValue,
                                   nsString& aResult) const
{
  return nsHTMLTagContent::AttributeToString(aAttribute, aValue, aResult);
}

NS_IMETHODIMP
nsHTMLContainer::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  NS_ERROR("you must override this method");
  return NS_OK;
}


void
nsHTMLContainer::MapBackgroundAttributesInto(nsIStyleContext* aContext,
                                             nsIPresContext* aPresContext)
{
  nsHTMLValue value;

  // background
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsHTMLAtoms::background, value)) {
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString absURLSpec;
      nsAutoString spec;
      value.GetStringValue(spec);
      if (spec.Length() > 0) {
        // Resolve url to an absolute url
        nsIURL* docURL = nsnull;
        nsIDocument* doc = mDocument;
        if (nsnull != doc) {
          docURL = doc->GetDocumentURL();
        }

        nsresult rv = NS_MakeAbsoluteURL(docURL, "", spec, absURLSpec);
        if (nsnull != docURL) {
          NS_RELEASE(docURL);
        }
        nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
        color->mBackgroundImage = absURLSpec;
        color->mBackgroundFlags &= ~NS_STYLE_BG_IMAGE_NONE;
        color->mBackgroundRepeat = NS_STYLE_BG_REPEAT_XY;
      }
    }
  }

  // bgcolor
  if (NS_CONTENT_ATTR_HAS_VALUE == GetAttribute(nsHTMLAtoms::bgcolor, value)) {
    if (eHTMLUnit_Color == value.GetUnit()) {
      nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
      color->mBackgroundColor = value.GetColorValue();
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
    else if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString buffer;
      value.GetStringValue(buffer);
      char cbuf[40];
      buffer.ToCString(cbuf, sizeof(cbuf));

      nsStyleColor* color = (nsStyleColor*)aContext->GetMutableStyleData(eStyleStruct_Color);
      NS_ColorNameToRGB(cbuf, &(color->mBackgroundColor));
      color->mBackgroundFlags &= ~NS_STYLE_BG_COLOR_TRANSPARENT;
    }
  }
}


// nsIDOMNode interface
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kIContentIID, NS_ICONTENT_IID);

NS_IMETHODIMP    
nsHTMLContainer::GetChildNodes(nsIDOMNodeList** aChildNodes)
{
  NS_PRECONDITION(nsnull != aChildNodes, "null pointer");
  if (nsnull == mChildNodes) {
    mChildNodes = new nsDOMNodeList(this);
    NS_ADDREF(mChildNodes);
  }
  *aChildNodes = mChildNodes;
  NS_ADDREF(mChildNodes);
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContainer::GetHasChildNodes(PRBool* aReturn)
{
  if (0 != mChildren.Count()) {
    *aReturn = PR_TRUE;
  } 
  else {
    *aReturn = PR_FALSE;
  }
  return NS_OK;
}

NS_IMETHODIMP    
nsHTMLContainer::GetFirstChild(nsIDOMNode **aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(0);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node

    return res;
  }
  else {
    aNode = nsnull;
    return NS_OK;
  }
}

NS_IMETHODIMP    
nsHTMLContainer::GetLastChild(nsIDOMNode** aNode)
{
  nsIContent *child = (nsIContent*) mChildren.ElementAt(mChildren.Count()-1);
  if (nsnull != child) {
    nsresult res = child->QueryInterface(kIDOMNodeIID, (void**)aNode);
    NS_ASSERTION(NS_OK == res, "Must be a DOM Node"); // must be a DOM Node

    return res;
  }
  else {
    aNode = nsnull;
    return NS_OK;
  }
}

static void
SetDocumentInChildrenOf(nsIContent* aContent, nsIDocument* aDocument)
{
  PRInt32 i, n;
  aContent->ChildCount(n);
  for (i = 0; i < n; i++) {
    nsIContent* child;
    aContent->ChildAt(i, child);
    if (nsnull != child) {
      child->SetDocument(aDocument);
      SetDocumentInChildrenOf(child, aDocument);
    }
  }
}

// XXX It's possible that newChild has already been inserted in the
// tree; if this is the case then we need to remove it from where it
// was before placing it in it's new home

NS_IMETHODIMP    
nsHTMLContainer::InsertBefore(nsIDOMNode* aNewChild, 
                              nsIDOMNode* aRefChild, 
                              nsIDOMNode** aReturn)
{
  if (nsnull == aNewChild) {
    *aReturn = nsnull;
    return NS_OK;/* XXX wrong error value */
  }

  // Get the nsIContent interface for the new content
  nsIContent* newContent = nsnull;
  nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
  NS_ASSERTION(NS_OK == res, "New child must be an nsIContent");
  if (NS_OK == res) {
    if (nsnull == aRefChild) {
      // Append the new child to the end
      SetDocumentInChildrenOf(newContent, mDocument);
      res = AppendChildTo(newContent, PR_TRUE);
    }
    else {
      // Get the index of where to insert the new child
      nsIContent* refContent = nsnull;
      res = aRefChild->QueryInterface(kIContentIID, (void**)&refContent);
      NS_ASSERTION(NS_OK == res, "Ref child must be an nsIContent");
      if (NS_OK == res) {
        PRInt32 pos;
        IndexOf(refContent, pos);
        if (pos >= 0) {
          SetDocumentInChildrenOf(newContent, mDocument);
          res = InsertChildAt(newContent, pos, PR_TRUE);
        }
        NS_RELEASE(refContent);
      }
    }
    NS_RELEASE(newContent);

    *aReturn = aNewChild;
    NS_ADDREF(aNewChild);
  }
  else {
    *aReturn = nsnull;
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::ReplaceChild(nsIDOMNode* aNewChild, 
                              nsIDOMNode* aOldChild, 
                              nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      nsIContent* newContent = nsnull;
      nsresult res = aNewChild->QueryInterface(kIContentIID, (void**)&newContent);
      NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
      if (NS_OK == res) {
        res = ReplaceChildAt(newContent, pos, PR_TRUE);
        NS_RELEASE(newContent);
      }
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }
  
  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::RemoveChild(nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn)
{
  nsIContent* content = nsnull;
  *aReturn = nsnull;
  nsresult res = aOldChild->QueryInterface(kIContentIID, (void**)&content);
  NS_ASSERTION(NS_OK == res, "Must be an nsIContent");
  if (NS_OK == res) {
    PRInt32 pos;
    IndexOf(content, pos);
    if (pos >= 0) {
      res = RemoveChildAt(pos, PR_TRUE);
      *aReturn = aOldChild;
      NS_ADDREF(aOldChild);
    }
    NS_RELEASE(content);
  }

  return res;
}

NS_IMETHODIMP    
nsHTMLContainer::AppendChild(nsIDOMNode* aNewChild, 
                             nsIDOMNode** aReturn)
{
  return InsertBefore(aNewChild, nsnull, aReturn);
}
