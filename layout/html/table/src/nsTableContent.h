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
#ifndef nsTableContent_h__
#define nsTableContent_h__

#include "nscore.h"
#include "nsITableContent.h"
#include "nsHTMLContainer.h"
#include "nsTablePart.h"
#include "nsIAtom.h"

/**
 * TableContent is a concrete base class for all content nodes contained directly 
 * within a table.
 *
 * @author  sclark
 * @version $Revision: 3.3 $
 * @see
 */
class nsTableContent : public nsHTMLContainer, public nsITableContent
{

public:

protected:
  /** the table to which this content belongs */
  nsTablePart *mTable;

  /** PR_TRUE if this content was generated in response to incomplete input,
    * meaning there is no actual input tag matching this container.
    */
  PRBool       mImplicit;

public:

  /** constructor
    * @param aTag  the HTML tag causing this caption to get constructed.
    */
  nsTableContent (nsIAtom* aTag);

  /** constructor
    * @param aTag  the HTML tag causing this caption to get constructed.
    * @param aImplicit  PR_TRUE if there is no actual input tag corresponding to
    *                   this caption.
    */
  nsTableContent (nsIAtom* aTag, PRBool aImplicit);

  /** supports implementation */
  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtrResult);

  // For debugging purposes only
  NS_IMETHOD_(nsrefcnt) AddRef();
  NS_IMETHOD_(nsrefcnt) Release();

  /** @see nsITableContent::GetTable */
  nsTablePart* GetTable ();

  /** @see nsITableContent::SetTable 
    * Note: Since mColGroup is the parent of the table column,
    * reference counting should NOT be done.
    * see /ns/raptor/doc/MemoryModel.html
    **/
  void SetTable (nsTablePart *aTable);


  /** Set the children of this piece of content to
    * be aTable 
    **/
  void SetTableForChildren(nsTablePart *aTable);


  /** @see nsITableContent::IsImplicit */
  virtual PRBool IsImplicit () const;

  /** @see nsITableContent::SkipSelfForSaving */
  virtual PRBool SkipSelfForSaving ();

  /** @see nsITableContent::GetType */
  virtual int GetType()=0;

  /** debug method prints out this and all child frames */
  void List(FILE* out, PRInt32 aIndent) const;

  PRBool InsertChildAt(nsIContent* aKid, PRInt32 aIndex);
  PRBool ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex);
  PRBool AppendChild(nsIContent* aKid);
  PRBool RemoveChildAt(PRInt32 aIndex);


private:
  /**
    *
    * If the content is a nsTableContent then call SetTable on 
    * aContent, otherwise, do nothing.
    *
    */
  void SetTableForTableContent(nsIContent* aContent, nsTablePart *aTable);

};

#endif

