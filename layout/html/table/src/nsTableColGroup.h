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
#ifndef nsTableColGroup_h__
#define nsTableColGroup_h__

#include "nscore.h"
#include "nsTableContent.h"

class nsIPresContext;

/**
 * nsTableColGroup is the content object that represents table col groups 
 * (HTML tag COLGROUP). This class cannot be reused
 * outside of an nsTablePart.  It assumes that its parent is an nsTablePart, and 
 * its children are nsTableCols.
 * 
 * @see nsTablePart
 * @see nsTableCol
 *
 * @author  sclark
 */
class nsTableColGroup : public nsTableContent
{
protected:

  /** the number of columns this col group represents.  Must be >=1.
    * Default is 1.
    * Must be ignored if the colgroup contains any explicit col content objects.
    */
  int mSpan;

  /** the starting column index this col group represents. Must be >= 0. */
  int mStartColIndex;

  /** the number of columns represented by this col group when col content
    * objects are contained herein.  If no col children, then mSpan is the 
    * proper place to check.
    * @see GetColumnCount
    */
  int mColCount;

public:

  /** constructor
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this col group.
    */
  nsTableColGroup (PRBool aImplicit);

  /** constructor
    * @param aTag  the HTML tag causing this caption to get constructed.
    * @param aSpan the number of columns this col group represents 
    *              (unless overridden by col children.)
    */
  nsTableColGroup (nsIAtom* aTag, int aSpan);

  /** destructor, not responsible for any memory destruction itself */
  virtual ~nsTableColGroup();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);
  
  /** @see nsIHTMLContent::CreateFrame */
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  /** returns nsITableContent::kTableColGroupType */
  virtual int GetType();

  /** returns the span attribute, always >= 1.  Not necessarily representative
    * of the number of columns spanned, since span is overridden by any COL 
    * children.
    * @see GetColumnCount
    */
  virtual int GetSpan ();
  
  /** set the span attribute, must be >= 1. */
  virtual void SetSpan (int aSpan);
  
  /** get the starting column index, always >= 0. */
  virtual int GetStartColumnIndex ();
  
  /** set the starting column index, must be >= 0. */
  virtual void SetStartColumnIndex (int aIndex);
  
  /** called whenever the col structure changes.  
    * Propogates change notification up to the nsTablePart parent
    */
  virtual void ResetColumns ();

  /** returns the number of columns represented by this group.
    * if there are col children, count them (taking into account the span of each)
    * else, check my own span attribute.
    */
  virtual int GetColumnCount ();

  /** helper routine returns PR_TRUE if aContent represents a column */
  virtual PRBool IsCol(nsIContent * aContent) const;

  /** can only append objects that are columns (implement nsITableContent and are .
    * of type nsITableContent::kTableColType.)
    * @see nsIContent::AppendChild
    */
  NS_IMETHOD AppendChild(nsIContent* aKid, PRBool aNotify);

  /** can only insert objects that are columns (implement nsITableContent and are .
    * of type nsITableContent::kTableColType.)
    * @see nsIContent::InsertChildAt
    */
  NS_IMETHOD InsertChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);

  /** can only replace child objects with objects that are columns 
    * (implement nsITableContent and are * of type nsITableContent::kTableColType.)
    * @param aContent the object to insert, must be a column
    * @param aIndex   the index of the object to replace.  Must be in the range
    *                 0<=aIndex<ChildCount().
    * @see nsIContent::ReplaceChildAt
    */
  NS_IMETHOD ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex, PRBool aNotify);

  /** @see nsIContent::InsertChildAt */
  NS_IMETHOD RemoveChildAt(PRInt32 aIndex, PRBool aNotify);
};

#endif



