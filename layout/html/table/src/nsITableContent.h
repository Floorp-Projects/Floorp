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
#ifndef nsITableContent_h__
#define nsITableContent_h__
 
#include "nsISupports.h"
#include "nscore.h"

class nsTablePart;

#define NS_ITABLECONTENT_IID \
 { /* 34a59b40-a71d-11d1-8f2f-006008159b0c */ \
    0x34a59b40, 0xa71d, 0x11d1, \
    {0x8f, 0x2f, 0x00, 0x60, 0x08, 0x15, 0x9b, 0x0c} }

/**
 * nsITableContent is a concrete subclass for all content nodes contained directly within a table.
 *
 * @author  sclark
 * @version $Revision: 1.1 $
 * @see
 */
class nsITableContent : public nsISupports
{

public:

  /** constants representing well-known table content types */
  static const int kTableRowGroupType;
  static const int kTableRowType;
  static const int kTableColGroupType;
  static const int kTableColType;
  static const int kTableCaptionType;
  static const int kTableCellType;

protected:
  nsITableContent ();

public:
  /** 
    * returns the TablePart that contains this content node.
    */
  virtual nsTablePart *GetTable ()=0;
  
  /**
    *
    */
  virtual void SetTable (nsTablePart *aTable)=0;

  /**
    *
    */
  virtual PRBool IsImplicit () const =0;

  /**
    * Don't want to put out implicit tags when saving.
    */
  virtual PRBool SkipSelfForSaving ()=0; 

  /**
    * return the type of TableContent this object represents.
    * implementing interfaces for all these objects seemed like overkill.
    */
  virtual int GetType()=0;

};

#endif

