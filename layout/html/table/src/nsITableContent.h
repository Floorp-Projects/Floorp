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
 * nsITableContent is a concrete subclass for all content nodes contained directly 
 * within a table.
 *
 * @author  sclark
 * @see nsTablePart
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
  /** protected constructor.  Never create an object of type nsITableContent directly.*/
  nsITableContent ();

public:
  /** returns the TablePart that contains this content node.
    * every table content has one and only one parent, 
    * and may not be replicated within different hierarchies.
    */
  virtual nsTablePart *GetTable ()=0;
  
  /** set the parent of this table content object.
    * @param aTable the new parent.  May be null to disconnect this object from
    *               the hierarchy it is in.  Note that we don't have formalized
    *               relationship objects, so the caller must also remove this 
    *               from it's prarent's child list.
    */
  virtual void SetTable (nsTablePart *aTable)=0;

  /** returns PR_TRUE if there is an actual input tag corresponding to
    * this content object.
    */
  virtual PRBool IsImplicit () const =0;

  /** returns PR_TRUE if this content object should NOT be written to the output stream.
    * for example, we don't generally want to output implicit tags when saving.
    */
  virtual PRBool SkipSelfForSaving ()=0; 

  /** return the type of TableContent this object represents. */
  virtual int GetType()=0;

};

#endif

