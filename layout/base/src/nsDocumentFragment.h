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

#include "nsISupports.h"
#include "nsIContent.h"
#include "nsIDOMDocumentFragment.h"
#include "nsIScriptObjectOwner.h"
#include "nsGenericElement.h"
#include "nsISizeOfHandler.h"
#include "nsINameSpaceManager.h"

class nsIDocument;

class nsDocumentFragment : public nsIContent,
			   public nsIDOMDocumentFragment,
			   public nsIScriptObjectOwner
{
public:
  nsDocumentFragment(nsIDocument* aOwnerDocument);
  virtual ~nsDocumentFragment();

  // nsISupports
  NS_DECL_ISUPPORTS

  // interface nsIDOMDocumentFragment
  NS_IMETHOD    GetNodeName(nsString& aNodeName);
  NS_IMETHOD    GetNodeValue(nsString& aNodeValue);
  NS_IMETHOD    SetNodeValue(const nsString& aNodeValue);
  NS_IMETHOD    GetNodeType(PRUint16* aNodeType);
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode)
    { 
      *aParentNode = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes)
    { return mInner.GetChildNodes(aChildNodes); }
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild)
    { return mInner.GetFirstChild(aFirstChild); }
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild)
    { return mInner.GetLastChild(aLastChild); }
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling)
    {
      *aPreviousSibling = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling)
    {
      *aNextSibling = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes)
    {
      *aAttributes = nsnull;
      return NS_OK;
    }
  NS_IMETHOD    GetOwnerDocument(nsIDOMDocument** aOwnerDocument);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, 
			     nsIDOMNode** aReturn)
    { return mInner.InsertBefore(aNewChild, aRefChild, aReturn); }
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, 
			     nsIDOMNode** aReturn)
    { return mInner.ReplaceChild(aNewChild, aOldChild, aReturn); }
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn)
    { return mInner.RemoveChild(aOldChild, aReturn); }
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn)
    { return mInner.AppendChild(aNewChild, aReturn); }
  NS_IMETHOD    HasChildNodes(PRBool* aReturn)
    { return mInner.HasChildNodes(aReturn); }
  NS_IMETHOD    CloneNode(PRBool aDeep, nsIDOMNode** aReturn);

  // interface nsIScriptObjectOwner
  NS_IMETHOD GetScriptObject(nsIScriptContext* aContext, void** aScriptObject);
  NS_IMETHOD SetScriptObject(void* aScriptObject);

  // interface nsIContent
  NS_IMETHOD GetDocument(nsIDocument*& aResult) const
    { return mInner.GetDocument(aResult); }
  NS_IMETHOD SetDocument(nsIDocument* aDocument, PRBool aDeep)
    { return mInner.SetDocument(aDocument, aDeep); }
  NS_IMETHOD GetParent(nsIContent*& aResult) const
    {
      aResult = nsnull;
      return NS_OK;
    }
  NS_IMETHOD SetParent(nsIContent* aParent)
    { return NS_OK; }
  NS_IMETHOD GetNameSpaceID(PRInt32& aResult) const
    {
      aResult = kNameSpaceID_None;
      return NS_OK;
    }
  NS_IMETHOD GetTag(nsIAtom*& aResult) const
    {
      aResult = nsnull;
      return NS_OK;
    }
  NS_IMETHOD CanContainChildren(PRBool& aResult) const
    { return mInner.CanContainChildren(aResult); }
  NS_IMETHOD ChildCount(PRInt32& aResult) const
    { return mInner.ChildCount(aResult); }
  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const
    { return mInner.ChildAt(aIndex, aResult); }
  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const
    { return mInner.IndexOf(aPossibleChild, aIndex); }
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify)
    { return mInner.InsertChildAt(aKid, aIndex, aNotify); }
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify)
    { return mInner.ReplaceChildAt(aKid, aIndex, aNotify); }
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify)
    { return mInner.AppendChildTo(aKid, aNotify); }  
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify)
    { return mInner.RemoveChildAt(aIndex, aNotify); }
  NS_IMETHOD IsSynthetic(PRBool& aResult)
    { return mInner.IsSynthetic(aResult); }
  NS_IMETHOD SetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName,
                          const nsString& aValue,
                          PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttribute(PRInt32 aNameSpaceID, nsIAtom* aName, 
                          nsString& aResult) const
    { return NS_CONTENT_ATTR_NOT_THERE; }
  NS_IMETHOD UnsetAttribute(PRInt32 aNameSpaceID, nsIAtom* aAttribute, 
                            PRBool aNotify)
    { return NS_OK; }
  NS_IMETHOD GetAttributeNameAt(PRInt32 aIndex,
                                PRInt32& aNameSpaceID, 
                                nsIAtom*& aName) const
    { 
      aName = nsnull;
      return NS_ERROR_ILLEGAL_VALUE;
    }
  NS_IMETHOD GetAttributeCount(PRInt32& aCountResult) const
    { 
      aCountResult = 0;
      return NS_OK;
    }
  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const
    { return mInner.List(out, aIndent); }
  NS_IMETHOD BeginConvertToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD ConvertContentToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD FinishConvertToXIF(nsXIFConverter& aConverter) const
    { return NS_OK; }
  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const 
    {
      aHandler->Add(sizeof(*this));
      return NS_OK;
    }
  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent,
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus)
    {
      aEventStatus = nsEventStatus_eIgnore;
      return NS_OK;
    }
  NS_IMETHOD RangeAdd(nsIDOMRange& aRange)
    {  return mInner.RangeAdd(aRange); } 
  NS_IMETHOD RangeRemove(nsIDOMRange& aRange)
    {  return mInner.RangeRemove(aRange); }                                                                        
  NS_IMETHOD GetRangeList(nsVoidArray*& aResult) const 
    {  return mInner.GetRangeList(aResult); }                                                                        

protected:
  nsGenericContainerElement mInner;
  void* mScriptObject;
  nsIDocument* mOwnerDocument;
};


extern NS_LAYOUT nsresult
   NS_NewDocumentFragment(nsIDOMDocumentFragment** aInstancePtrResult,
			  nsIDocument* aOwnerDocument);
