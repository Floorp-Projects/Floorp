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
#include "nsTableContent.h"
#include "nsITableContent.h"
#include "nsString.h"

const int nsITableContent::kTableRowGroupType=1;
const int nsITableContent::kTableRowType=2;
const int nsITableContent::kTableColGroupType=3;
const int nsITableContent::kTableColType=4;
const int nsITableContent::kTableCaptionType=5;
const int nsITableContent::kTableCellType=6;

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);

/**
  * default contstructor
  */
nsTableContent::nsTableContent (nsIAtom* aTag)
  : nsHTMLContainer(aTag)
{
}

/**
  */
nsTableContent::nsTableContent (nsIAtom* aTag, PRBool aImplicit)
  : nsHTMLContainer(aTag),
    mImplicit(aImplicit)
{
}

nsrefcnt nsTableContent::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Add Ref: nsTableContent cnt = %d \n",mRefCnt+1);
  return ++mRefCnt;
}

nsrefcnt nsTableContent::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: nsTableContent cnt = %d \n",mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE)  printf("Delete: nsTableContent \n");
    delete this;
    return 0;
  }
  return mRefCnt;
}


nsresult nsTableContent::QueryInterface(const nsIID& aIID,
                                        void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kITableContentIID)) {
    *aInstancePtrResult = (void*) ((nsITableContent*)this);
    AddRef();
    return NS_OK;
  }
  return nsHTMLContent::QueryInterface(aIID, aInstancePtrResult);
}

/**
 * Don't want to put out implicit tags when saving.
 */
PRBool nsTableContent::SkipSelfForSaving() 
{
  return mImplicit;
}

nsTablePart* nsTableContent::GetTable ()
{
  NS_IF_ADDREF(mTable);
  return mTable;
}

void nsTableContent::SetTableForChildren(nsTablePart *aTable)
{
 if (aTable != nsnull)
  {
    PRInt32 count = ChildCount();
    for (PRInt32 index = 0; index < count; index++)
      SetTableForTableContent(ChildAt(index),aTable);
  }
}


void nsTableContent::SetTable (nsTablePart *aTable)
{
  // Check to see if the current table was null, if it was, then call
  // method to reset children
  if (mTable == nsnull)
    SetTableForChildren(aTable);
  mTable = aTable;
}

PRBool nsTableContent::IsImplicit () const
{
  return mImplicit;
}


/* ----- overridable methods from nsHTMLContainer ----- */

/**
*
* If the content is a nsTableContent then call SetTable on 
* aContent, otherwise, do nothing.
*
*/
void nsTableContent::SetTableForTableContent(nsIContent* aContent, nsTablePart *aTable)
{
  if (aContent != nsnull)
  {
    nsITableContent* tableContent;
    nsresult result = aContent->QueryInterface(kITableContentIID, (void**) &tableContent);
    if (NS_OK == result)
    {
      tableContent->SetTable(aTable);  
      NS_IF_RELEASE(tableContent);
    }
  }
}


void nsTableContent::List(FILE* out, PRInt32 aIndent) const
{
  PRInt32 i;
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  nsIAtom* tag = GetTag();
  if (tag != nsnull) {
    nsAutoString buf;
    tag->ToString(buf);
    fputs(buf, out);
    NS_RELEASE(tag);
  }

  char *isImplicitString = "";
  if (PR_TRUE==IsImplicit())
    isImplicitString = " (I)";

  ListAttributes(out);

  if (gsNoisyRefs == PR_TRUE)
    fprintf(out, " RefCount=%d<%s\n", mRefCnt, isImplicitString);

  if (CanContainChildren()) {
    PRInt32 kids = ChildCount();
    for (i = 0; i < kids; i++) {
      nsIContent* kid = ChildAt(i);
      kid->List(out, aIndent + 1);
      NS_RELEASE(kid);
    }
  }
  for (i = aIndent; --i >= 0; ) fputs("  ", out);
  fputs(">\n", out);
}

PRBool nsTableContent::InsertChildAt(nsIContent* aKid, PRInt32 aIndex)
{ 
  PRBool result = nsHTMLContainer::InsertChildAt(aKid, aIndex); 
  if (result == PR_TRUE)
    SetTableForTableContent(aKid,mTable);
  return result;
}

PRBool nsTableContent::ReplaceChildAt(nsIContent* aKid, PRInt32 aIndex)
{ 
  nsIContent* child = ChildAt(aIndex);
  PRBool result = nsHTMLContainer::ReplaceChildAt(aKid, aIndex); 
  if (result == PR_TRUE)
  {
    SetTableForTableContent(child,nsnull);
    SetTableForTableContent(aKid,mTable);
  }
  NS_IF_RELEASE(child);
  return result;
}

PRBool nsTableContent::AppendChild(nsIContent* aKid)
{ 
  PRBool  result = nsHTMLContainer::AppendChild(aKid);
  if (result == PR_TRUE)
    SetTableForTableContent(aKid,mTable);
  return result;
}

PRBool nsTableContent::RemoveChildAt(PRInt32 aIndex)
{ 
  nsTableContent* child = (nsTableContent*)ChildAt(aIndex);
  PRBool result = nsHTMLContainer::RemoveChildAt(aIndex); 
  if (result == PR_TRUE)
    SetTableForTableContent(child,nsnull);
  NS_IF_RELEASE(child);
  return result; 
}

/* ---------- nsITableContent implementations ----------- */

nsITableContent::nsITableContent () 
{}

