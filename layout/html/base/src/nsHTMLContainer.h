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

class nsDOMNodeList;
// Generic HTML container class. This code manages an array of
// children nodes that can be any kind of nsIContent
class nsHTMLContainer : public nsHTMLTagContent {
public:
  nsHTMLContainer(nsIAtom* aTag);

  NS_IMETHOD SizeOf(nsISizeOfHandler* aHandler) const;

  NS_IMETHOD CanContainChildren(PRBool& aResult) const;

  NS_IMETHOD ChildCount(PRInt32& aResult) const;

  NS_IMETHOD ChildAt(PRInt32 aIndex, nsIContent*& aResult) const;

  NS_IMETHOD IndexOf(nsIContent* aPossibleChild, PRInt32& aIndex) const;

  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);
  NS_IMETHOD AppendChildTo(nsIContent* aKid, PRBool aNotify);
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);

  NS_IMETHOD Compact();

  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify);
  NS_IMETHOD GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const;

  NS_IMETHOD AttributeToString(nsIAtom* aAttribute,
                               nsHTMLValue& aValue,
                               nsString& aResult) const;

  NS_IMETHOD    GetChildNodes(nsIDOMNodeList** aChildNodes);
  NS_IMETHOD    GetHasChildNodes(PRBool* aHasChildNodes);
  NS_IMETHOD    GetFirstChild(nsIDOMNode **aFirstChild);
  NS_IMETHOD    GetLastChild(nsIDOMNode** aLastChild);
  NS_IMETHOD    InsertBefore(nsIDOMNode* aNewChild, 
                             nsIDOMNode* aRefChild, 
                             nsIDOMNode** aReturn);
  NS_IMETHOD    ReplaceChild(nsIDOMNode* aNewChild, 
                             nsIDOMNode* aOldChild, 
                             nsIDOMNode** aReturn);
  NS_IMETHOD    RemoveChild(nsIDOMNode* aOldChild, 
                            nsIDOMNode** aReturn);
  NS_IMETHOD    AppendChild(nsIDOMNode* aNewChild, 
                            nsIDOMNode** aReturn);

protected:
  nsHTMLContainer();
  virtual ~nsHTMLContainer();
  void SizeOfWithoutThis(nsISizeOfHandler* aHandler) const;


  /**
   * Helper method that maps "background" and "bgcolor" into
   * the style context.
   */
  void MapBackgroundAttributesInto(nsIStyleContext* aContext,
                                   nsIPresContext* aPresContext);

  nsVoidArray mChildren;
  nsDOMNodeList *mChildNodes;
};

#endif /* nsHTMLContainer_h___ */
