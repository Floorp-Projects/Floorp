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
 * nsTableCol
 * data structure to maintain information about a single table column
 *
 * @author  sclark
 */
class nsTableCol : public nsTableContent
{

  //NS_DECL_ISUPPORTS

private:

  nsTableColGroup *  mColGroup;
  PRInt32            mColIndex;
  PRInt32            mRepeat;

public:

  nsTableCol ();

  nsTableCol (PRBool aImplicit);

  nsTableCol (nsIAtom* aTag);

  virtual ~nsTableCol();

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual int GetType();

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual int GetRepeat ();

  void SetRepeat (int aRepeat);

  virtual nsTableColGroup *GetColGroup ();

  /** 
  * Since mColGroup is the parent of the table column,
  * reference counting should NOT be done on 
  * this variable when setting the row.
  * see /ns/raptor/doc/MemoryModel.html
  **/
  virtual void SetColGroup (nsTableColGroup * aColGroup);

  virtual int GetColumnIndex ();

  virtual void SetColumnIndex (int aColIndex);

  virtual void ResetColumns ();


  /* ----------- nsITableContent overrides ----------- */

protected:

  PRBool SetInternalAttribute (nsString *aName, nsString *aValue);  

  nsString *GetInternalAttribute (nsString *aName);

  PRBool UnsetInternalAttribute (nsString *aName);

  int GetInternalAttributeState (nsString *aName);

  PRBool IsInternalAttribute (nsString *aName);

  static nsString * kInternalAttributeNames;

  nsString * GetAllInternalAttributeNames ();


};

#endif



