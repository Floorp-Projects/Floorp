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
#ifndef nsTableCol_h__
#define nsTableCol_h__

#include "nscore.h"
#include "nsTableContent.h"
#include "nsTableColGroup.h"

class nsTablePart;

/**
 * nsTableCol is the content object that represents table cols 
 * (HTML tag COL). This class cannot be reused
 * outside of an nsTableColGroup.  It assumes that its parent is an nsTableColGroup, and 
 * it has no children.
 * 
 * @see nsTablePart
 * @see nsTableColGroup
 *
 * @author  sclark
 */
class nsTableCol : public nsTableContent
{

private:

  /** parent pointer, the column group to which this column belongs */
  nsTableColGroup *  mColGroup;

  /** the starting index of the column (starting at 0) that this col object represents */
  PRInt32            mColIndex;

  /** the number of columns that the attributes of this column extend to */
  PRInt32            mRepeat;

public:

  /** default constructor */
  nsTableCol ();

  /** constructor
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this col.
    */
  nsTableCol (PRBool aImplicit);

  /** constructor
    * @param aTag  the HTML tag causing this col to get constructed.
    */
  nsTableCol (nsIAtom* aTag);

  /** destructor, not responsible for any memory destruction itself */
  virtual ~nsTableCol();

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /** returns nsITableContent::kTableCellType */
  virtual int GetType();

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);

  /** @see nsIHTMLContent::CreateFrame */
  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  /** return the number of columns this content object represents.  always >= 1*/
  virtual int GetRepeat ();

  /** set the number of columns this content object represents.  must be >= 1*/
  void SetRepeat (int aRepeat);

  virtual nsTableColGroup *GetColGroup ();

  /** set the parent col group.<br>
    * NOTE: Since mColGroup is the parent of the table column,
    * reference counting should NOT be done on 
    * this variable when setting the row.
    * see /ns/raptor/doc/MemoryModel.html
    **/
  virtual void SetColGroup (nsTableColGroup * aColGroup);

  /** return the index of the column this content object represents.  always >= 0 */
  virtual int GetColumnIndex ();

  /** set the index of the column this content object represents.  must be >= 0 */
  virtual void SetColumnIndex (int aColIndex);

  virtual void ResetColumns ();

private:
  void Init();


};

/* ---------- inlines ---------- */


inline int nsTableCol::GetType()
{  return nsITableContent::kTableColType;}

inline int nsTableCol::GetRepeat ()
{
  if (0 < mRepeat)
    return mRepeat;
  return 1;
}

inline nsTableColGroup * nsTableCol::GetColGroup ()
{
  NS_IF_ADDREF(mColGroup);
  return mColGroup;
}

inline void nsTableCol::SetColGroup (nsTableColGroup * aColGroup)
{  mColGroup = aColGroup;}

inline int nsTableCol::GetColumnIndex ()
{  return mColIndex;}
  
inline void nsTableCol::SetColumnIndex (int aColIndex)
{  mColIndex = aColIndex;}

inline void nsTableCol::SetRepeat (int aRepeat)
{
  mRepeat = aRepeat;
  ResetColumns ();
}

inline void nsTableCol::ResetColumns ()
{
  if (nsnull != mColGroup)
    mColGroup->ResetColumns ();
}

#endif



