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
 * nsTableColGroup
 * datastructure to maintain information about a single table column
 *
 * @author  sclark
 */
class nsTableColGroup : public nsTableContent
{
protected:

  int mSpan;
  int mStartColIndex;
  int mColCount;

public:

  nsTableColGroup (PRBool aImplicit);

  nsTableColGroup (nsIAtom* aTag, int aSpan);

  virtual ~nsTableColGroup();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();
  
  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual int GetType();

  virtual int GetSpan ();
  
  virtual void SetSpan (int aSpan);
  
  virtual int GetStartColumnIndex ();
  
  virtual void SetStartColumnIndex (int aIndex);
  
  virtual void ResetColumns ();

  virtual int GetColumnCount ();

  virtual PRBool IsCol(nsIContent * aContent) const;

  virtual PRBool AppendChild (nsIContent *  aContent);

  virtual PRBool InsertChildAt (nsIContent *  aContent, int aIndex);

  virtual PRBool ReplaceChildAt (nsIContent *  aContent, int aIndex);

  virtual PRBool RemoveChildAt (int aIndex);

protected:
  virtual PRBool SetInternalAttribute (nsString *aName, nsString *aValue);  

  virtual nsString *GetInternalAttribute (nsString *aName);

  virtual PRBool UnsetInternalAttribute (nsString *aName);

  virtual int GetInternalAttributeState (nsString *aName);

  virtual PRBool IsInternalAttribute (nsString *aName);

  virtual nsString * GetAllInternalAttributeNames ();

  
};

#endif



