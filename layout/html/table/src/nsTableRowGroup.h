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
#ifndef nsTableRowGroup_h__
#define nsTableRowGroup_h__

#include "nscore.h"
#include "nsTableContent.h"

class nsIPresContext;

/**
 * TableRowGroup
 *
 * @author  sec   11-20-97 6:12pm
 * @version $Revision: 3.1 $
 * @see
 */
class nsTableRowGroup : public nsTableContent
{

public:

  nsTableRowGroup (nsIAtom* aTag);

  nsTableRowGroup (nsIAtom* aTag, PRBool aImplicit);

  virtual ~nsTableRowGroup();

  virtual PRInt32 GetMaxColumns();

    // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                PRInt32 aIndexInParent,
                                nsIFrame* aParentFrame);

  virtual int GetRowCount ()
  {
    return ChildCount ();
  };

  virtual int GetType()
  {
    return nsITableContent::kTableRowGroupType;
  };

  virtual void ResetCellMap ();

  /* ----------- overrides from nsTableContent ---------- */

  virtual PRBool AppendChild (nsIContent * aContent);

  virtual PRBool InsertChildAt (nsIContent * aContent, int aIndex);

  virtual PRBool ReplaceChildAt (nsIContent * aContent, int aIndex);

  /**
   * Remove a child at the given position. The method is ignored if
   * the index is invalid (too small or too large).
   */
  virtual PRBool RemoveChildAt (int aIndex);

protected:

  virtual PRBool IsRow(nsIContent * aContent) const;

};

#endif

