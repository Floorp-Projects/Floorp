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

/**
 * Abstract base class for un-tagged html content objects. Supports
 * allocation from the malloc heap as well as from an arena. Note
 * that instances of this object are created with zero'd memory.
 */
class nsHTMLContent : public nsIHTMLContent, public nsIDOMNode, public nsIScriptObjectOwner, public nsIDOMEventReceiver {
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
  virtual void SetDocument(nsIDocument* aDocument);

  virtual nsIContent* GetParent() const;
  virtual void SetParent(nsIContent* aParent);

  virtual PRBool CanContainChildren() const;
  virtual PRInt32 ChildCount() const;
  virtual nsIContent* ChildAt(PRInt32 aIndex) const;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD AppendChild(nsIContent* aKid, PRBool aNotify);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  NS_IMETHOD IsSynthetic(PRBool& aResult);

  virtual void Compact();
  
  virtual void SetAttribute(const nsString& aName, const nsString& aValue);
  virtual nsContentAttr GetAttribute(const nsString& aName,
                                     nsString& aResult) const;

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual void SetAttribute(nsIAtom* aAttribute,
                            const nsHTMLValue& aValue = nsHTMLValue::kNull);
  virtual void UnsetAttribute(nsIAtom* aAttribute);
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const;
  virtual PRInt32 GetAllAttributeNames(nsISupportsArray* aArray) const;
  virtual PRInt32 GetAttributeCount(void) const;

  virtual void      SetID(nsIAtom* aID);
  virtual nsIAtom*  GetID(void) const;
  virtual void      SetClass(nsIAtom* aClass);  // XXX this will have to change for CSS2
  virtual nsIAtom*  GetClass(void) const;  // XXX this will have to change for CSS2

  virtual nsIStyleRule* GetStyleRule(void);

  virtual void MapAttributesInto(nsIStyleContext* aContext, 
                                 nsIPresContext* aPresContext);

  virtual void List(FILE* out, PRInt32 aIndent) const;

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  virtual nsIAtom* GetTag() const;

  virtual void ToHTML(FILE* out) const;

  virtual nsIContentDelegate* GetDelegate(nsIPresContext* aCX);

  static void NewGlobalAtom(const char* aString, nsIAtom** aAtomResult);

  static void ReleaseGlobalAtom(nsIAtom** aAtomResult);

  static void QuoteForHTML(const nsString& aValue, nsString& aResult);

public:
  NS_IMETHOD ResetScriptObject();

  // nsIDOMNode interface
  NS_IMETHOD GetParentNode(nsIDOMNode **aNode);
  NS_IMETHOD GetChildNodes(nsIDOMNodeIterator **aIterator);
  NS_IMETHOD HasChildNodes();
  NS_IMETHOD GetFirstChild(nsIDOMNode **aNode);
  NS_IMETHOD GetPreviousSibling(nsIDOMNode **aNode);
  NS_IMETHOD GetNextSibling(nsIDOMNode **aNode);
  NS_IMETHOD InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild);
  NS_IMETHOD ReplaceChild(nsIDOMNode *newChild, nsIDOMNode *oldChild);
  NS_IMETHOD RemoveChild(nsIDOMNode *oldChild);

  // nsIDOMEventReceiver interface
  NS_IMETHOD AddEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);
  NS_IMETHOD RemoveEventListener(nsIDOMEventListener *aListener, const nsIID& aIID);

  NS_IMETHOD HandleDOMEvent(nsIPresContext& aPresContext,
                            nsGUIEvent* aEvent, 
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

  nsresult GetListenerManager(nsIEventListenerManager** aInstancePtrResult);

  nsIEventListenerManager* mListenerManager;
};

#endif /* nsHTMLContent_h___ */
