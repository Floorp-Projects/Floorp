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
#ifndef nsHTMLContent_h___
#define nsHTMLContent_h___

#include "nsIHTMLContent.h"
#include "nsIDOMNode.h"
#include "nsIScriptObjectOwner.h"
#include "nsIDOMEventReceiver.h"
#include "nsGUIEvent.h"
class nsIEventListenerManager;
class nsIArena;
class nsIAtom;
class nsXIFConverter;

/**
 * Abstract base class for un-tagged html content objects. Supports
 * allocation from the malloc heap as well as from an arena. Note
 * that instances of this object are created with zero'd memory.
 */
class nsHTMLContent : public nsIHTMLContent,
                      public nsIDOMNode,
                      public nsIScriptObjectOwner,
                      public nsIDOMEventReceiver
{
public:
  /**
   * This new method allocates memory from the standard malloc heap
   * and zeros out the content object.
   */
  void* operator new(size_t size);

  /**
   * This new method allocates memory from the given arena and
   * and zeros out the content object.
   */
  void* operator new(size_t size, nsIArena* aArena);

  /**
   * Release the memory associated with the content object. If the
   * object was allocated from an arena then nothing happens.
   */
  void operator delete(void* ptr);

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  NS_IMETHOD GetDocument(nsIDocument*& aResult) const;
  NS_IMETHOD SetDocument(nsIDocument* aDocument);

  NS_IMETHOD GetParent(nsIContent*& aResult) const;
  NS_IMETHOD SetParent(nsIContent* aParent);

  NS_IMETHOD CanContainChildren(PRBool& aResult) const;

  NS_IMETHOD ChildCount(PRInt32& aResult) const;

  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;

  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex,
                           PRBool aNotify);

  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex,
                            PRBool aNotify);

  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);

  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  NS_IMETHOD IsSynthetic(PRBool& aResult);

  NS_IMETHOD Compact();
  
  NS_IMETHOD SetAttribute(const nsString& aName,
                          const nsString& aValue,
                          PRBool aNotify);

  NS_IMETHOD GetAttribute(const nsString& aName, nsString& aResult) const;

  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify);

  NS_IMETHOD SetAttribute(nsIAtom* aAttribute,
                          const nsHTMLValue& aValue,
                          PRBool aNotify);

  NS_IMETHOD UnsetAttribute(nsIAtom* aAttribute);

  NS_IMETHOD GetAttribute(nsIAtom *aAttribute,
                          nsString &aResult) const;
  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aValue) const;

  NS_IMETHOD GetAllAttributeNames(nsISupportsArray* aArray,
                                  PRInt32& aCountResult) const;
  NS_IMETHOD GetAttributeCount(PRInt32& aCountResult) const;
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;

  NS_IMETHOD SetID(nsIAtom* aID);
  NS_IMETHOD GetID(nsIAtom*& aResult) const;
  // XXX this will have to change for CSS2
  NS_IMETHOD SetClass(nsIAtom* aClass);
  // XXX this will have to change for CSS2
  NS_IMETHOD GetClass(nsIAtom*& aResult) const;

  NS_IMETHOD GetStyleRule(nsIStyleRule*& aResult);

  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const = 0;

  NS_IMETHOD StringToAttribute(nsIAtom* aAttribute,
                               const nsString& aValue,
                               nsHTMLValue& aResult) = 0;

  NS_IMETHOD List(FILE* out = stdout, PRInt32 aIndent = 0) const;

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  NS_IMETHOD GetTag(nsIAtom*& aResult) const;

  NS_IMETHOD ToHTML(FILE* out) const;

  static void QuoteForHTML(const nsString& aValue, nsString& aResult);

  NS_IMETHOD SetScriptObject(void *aScriptObject);

  // nsIDOMNode interface
  NS_IMETHOD    GetParentNode(nsIDOMNode** aParentNode);
  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD    GetFirstChild(nsIDOMNode** aFirstChild);
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD    GetPreviousSibling(nsIDOMNode** aPreviousSibling);
  NS_IMETHOD    GetNextSibling(nsIDOMNode** aNextSibling);
  NS_IMETHOD    GetAttributes(nsIDOMNamedNodeMap** aAttributes);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, nsIDOMNode* aRefChild, nsIDOMNode** aReturn);
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, nsIDOMNode** aReturn);
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, nsIDOMNode** aReturn);

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD GetListenerManager(nsIEventListenerManager** aInstancePtrResult);
  NS_IMETHOD GetNewListenerManager(nsIEventListenerManager** aInstancePtrResult);

  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsEvent* aEvent, 
                            nsIDOMEvent** aDOMEvent,
                            PRUint32 aFlags,
                            nsEventStatus& aEventStatus);

protected:
  nsHTMLContent();
  virtual ~nsHTMLContent();
  virtual void ListAttributes(FILE* out) const;
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  PRUint32 mInHeap : 1;
  PRUint32 mRefCnt : 31;
  nsIDocument* mDocument;
  nsIContent* mParent;
  void* mScriptObject;

  nsIEventListenerManager* mListenerManager;
};

#endif /* nsHTMLContent_h___ */
