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
#ifndef nsHTMLContainer_h___
#define nsHTMLContainer_h___

#include "nsHTMLTagContent.h"
#include "nsVoidArray.h"

// Generic HTML container class. This code manages an array of
// children nodes that can be any kind of nsIContent
class nsHTMLContainer : public nsHTMLTagContent {
public:
  nsHTMLContainer(nsIAtom* aTag);

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;
  virtual PRBool CanContainChildren() const;
  virtual PRInt32 ChildCount() const;
  virtual nsIContent* ChildAt(PRInt32 aIndex) const;
  virtual PRInt32 IndexOf(nsIContent* aPossibleChild) const;

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD AppendChild(nsIContent* aKid, PRBool aNotify);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  virtual void Compact();

  virtual nsresult CreateFrame(nsIPresContext* aPresContext,
                               nsIFrame* aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*& aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);
  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

  NS_IMETHOD GetChildNodes(nsIDOMNodeIterator **aIterator);
  NS_IMETHOD HasChildNodes(PRBool *aReturn);
  NS_IMETHOD GetFirstChild(nsIDOMNode **aNode);
  NS_IMETHOD InsertBefore(nsIDOMNode *newChild, nsIDOMNode *refChild);
  NS_IMETHOD ReplaceChild(nsIDOMNode *newChild, 
                                            nsIDOMNode *oldChild);
  NS_IMETHOD RemoveChild(nsIDOMNode *oldChild);

protected:
  nsHTMLContainer();
  virtual ~nsHTMLContainer();
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;

  virtual nsContentAttr AttributeToString(nsIAtom* aAttribute,
                                          nsHTMLValue& aValue,
                                          nsString& aResult) const;

  /**
   * Helper method that maps "background" and "bgcolor" into
   * the style context.
   */
  void MapBackgroundAttributesInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext);

  nsVoidArray mChildren;
};

#endif /* nsHTMLContainer_h___ */
