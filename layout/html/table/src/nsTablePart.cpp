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
#include "nsTablePart.h"
#include "nsTableOuterFrame.h"
#include "nsITableContent.h"
#include "nsTableCol.h"
#include "nsTableColGroup.h"
#include "nsTableRowGroup.h"
#include "nsTableCaption.h"
#include "nsTableRow.h"
#include "nsTableCell.h"
#include "nsCellMap.h"
#include "nsHTMLParts.h"
#include "nsIPresContext.h"
#include "nsContainerFrame.h"
#include "nsIRenderingContext.h"
#include "nsHTMLIIDs.h"
#include "nsIAtom.h"
#include "nsIStyleContext.h"
#include "nsHTMLAtoms.h"
#include "nsStyleConsts.h"
#include "nsUnitConversion.h"

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);
static NS_DEFINE_IID(kStylePositionSID, NS_STYLEPOSITION_SID);
static NS_DEFINE_IID(kStyleSpacingSID, NS_STYLESPACING_SID);
static NS_DEFINE_IID(kStyleDisplaySID, NS_STYLEDISPLAY_SID);

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
#endif

const char *nsTablePart::kCaptionTagString="CAPTION";
const char *nsTablePart::kRowGroupBodyTagString="TBODY";
const char *nsTablePart::kRowGroupHeadTagString="THEAD";
const char *nsTablePart::kRowGroupFootTagString="TFOOT";
const char *nsTablePart::kRowTagString="TR";
const char *nsTablePart::kColGroupTagString="COLGROUP";
const char *nsTablePart::kColTagString="COL";
const char *nsTablePart::kDataCellTagString="TD";
const char *nsTablePart::kHeaderCellTagString="TH";

CellData::CellData()
{
  mCell = nsnull;
  mRealCell = nsnull;
  mOverlap = nsnull;
}

CellData::~CellData()
{}


/*---------- nsTablePart implementation -----------*/

/**
 *
 * BUGS:
 * <ul>
 * <li>centering (etc.) is leaking into table cell's from the outer environment
 * </ul>
 *
 * TODO:
 * <ul>
 * <li>colspan creating empty cells on rows w/o matching cells
 * <li>draw table borders ala navigator
 * <li>border sizing
 * <li>bordercolor
 * <li>draw cell borders ala navigator
 * <li>cellspacing
 * <li>colspan not the last cell
 * <li>floaters in table cells
 * <ul>
 * <li>rowspan truncation (rowspan sticking out past the bottom of the table)
 * <li>effect on subsequent row structure
 * </ul>
 * <li>inherit table border color ala nav4 (use inherited text color) when
 * the border is enabled and the bordercolor value nsnull.
 * </ul>
 *
 *
 * CSS1:
 * <ul>
 * <li>table margin
 * <li>table border-width
 * <li>table padding
 * <li>row margin
 * <li>row border-width
 * <li>row padding
 * <li>cell margin
 * <li>cell border-width
 * <li>cell padding
 * <li>what about colgroup, thead, tbody, tfoot margin/border-width/padding?
 * <li>map css1 style (borders, padding, margins) in some sensible manner
 * </ul>
 *
 * CSS2:
 * <ul>
 * <li>watch it
 * </ul>
 *
 */

/** constructor
  * I do not check or addref aTag because my superclass does that for me
  */
nsTablePart::nsTablePart(nsIAtom* aTag)
  : nsHTMLContainer(aTag),
    mColCount(0),
    mSpecifiedColCount(0),
    mCellMap(0)
{ 
}

/** constructor
  * I do not check or addref aTag because my superclass does that for me
  */
nsTablePart::nsTablePart (nsIAtom* aTag, PRInt32 aColumnCount)
  : nsHTMLContainer(aTag),
    mColCount(aColumnCount),
    mSpecifiedColCount(0),
    mCellMap(0)
{
}

/**
  */
nsTablePart::~nsTablePart()
{
  if (nsnull!=mCellMap)
  {
    delete mCellMap;
  }
}

/**
  */
/*
nsTablePart::void compact() {
  compact();
}
*/

// for debugging only
nsrefcnt nsTablePart::AddRef(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Add Ref: nsTablePart cnt = %d \n",mRefCnt+1);
  return ++mRefCnt;
}

// for debugging only
nsrefcnt nsTablePart::Release(void)
{
  if (gsNoisyRefs==PR_TRUE) printf("Release: nsTablePart cnt = %d \n",mRefCnt-1);
  if (--mRefCnt == 0) {
    if (gsNoisyRefs==PR_TRUE) printf("Delete: nsTablePart \n");
    delete this;
    return 0;
  }
  return mRefCnt;
}

PRInt32 nsTablePart::GetMaxColumns ()
{
  if (nsnull == mCellMap)
  {
    BuildCellMap ();
  }
  return mColCount;
}

  // XXX what do rows with no cells turn into?
PRInt32 nsTablePart::GetRowCount ()
{
  // if we've already built the cellMap, ask it for the row count
  if (nsnull != mCellMap)
    return mCellMap->GetRowCount();

  // otherwise, we need to compute it by walking our children
  int rowCount = 0;
  int index = ChildCount ();
  while (0 < index)
  {
    nsIContent *child = ChildAt (--index);  // child: REFCNT++
    nsTableContent *tableContent = (nsTableContent *)child;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableRowGroupType)
      rowCount += ((nsTableRowGroup *)tableContent)->GetRowCount ();
    NS_RELEASE(child);                      // child: REFCNT--
  }
  return rowCount;
}
  
/* counts columns in column groups */
PRInt32 nsTablePart::GetSpecifiedColumnCount ()
{
  if (mSpecifiedColCount < 0)
  {
    mSpecifiedColCount = 0;
    int count = ChildCount ();
    for (int index = 0; index < count; index++)
    {
      nsIContent *child = ChildAt (index);  // child: REFCNT++
      nsTableContent *tableContent = (nsTableContent *)child;
      const int contentType = tableContent->GetType();
      if (contentType == nsITableContent::kTableColGroupType)
      {
        ((nsTableColGroup *)tableContent)->SetStartColumnIndex (mSpecifiedColCount);
        mSpecifiedColCount += ((nsTableColGroup *)tableContent)->GetColumnCount ();
      }
      NS_RELEASE(child);                    // child: REFCNT--
    }
  }
  return mSpecifiedColCount;
}

// returns the actual cell map, not a copy, so don't mess with it!
nsCellMap*  nsTablePart::GetCellMap() const
{
  return mCellMap;
}


/* call when the cell structure has changed.  mCellMap will be rebuilt on demand. */
void nsTablePart::ResetCellMap ()
{
  if (nsnull==mCellMap)
    delete mCellMap;
  mCellMap = nsnull; // for now, will rebuild when needed
}

/* call when column structure has changed. */
void nsTablePart::ResetColumns ()
{
  mSpecifiedColCount = -1;
  if (nsnull != mCellMap)
  {
    int colCount = GetSpecifiedColumnCount ();
    GrowCellMap (colCount); // make sure we're at least as big as specified columns
  }
  else
    mColCount = 0;  // we'll compute later (as part of building cell map)
}

/** sum the columns represented by all nsTableColGroup objects
  * if the cell map says there are more columns than this, 
  * add extra implicit columns to the content tree.
  */
void nsTablePart::EnsureColumns()
{
  if (nsnull!=mCellMap)
  {
    PRInt32 actualColumns = 0;
    PRInt32 numColGroups = ChildCount();
    nsTableColGroup *lastColGroup = nsnull;
    for (PRInt32 colGroupIndex = 0; colGroupIndex < numColGroups; colGroupIndex++)
    {
      nsTableContent *colGroup = (nsTableContent*)ChildAt(colGroupIndex); // colGroup: REFCNT++
      const int contentType = colGroup->GetType();
      if (contentType==nsTableContent::kTableColGroupType)
      {
        PRInt32 numCols = ((nsTableColGroup *)colGroup)->GetColumnCount();
        actualColumns += numCols;
        lastColGroup = (nsTableColGroup *)colGroup;
        NS_RELEASE(colGroup);
      }
      else if (contentType==nsTableContent::kTableRowGroupType)
      {
        NS_RELEASE(colGroup);
        break;
      }
    }    
    if (actualColumns < mCellMap->GetColCount())
    {
      if (nsnull==lastColGroup)
      {
        lastColGroup = new nsTableColGroup (PR_TRUE);
        AppendColGroup(lastColGroup);
      }
      PRInt32 excessColumns = mCellMap->GetColCount() - actualColumns;
      for ( ; excessColumns > 0; excessColumns--)
      {
        nsTableCol *col = new nsTableCol(PR_TRUE);
        lastColGroup->AppendChild (col);
      }
    }
  }
}

/**
  */
void nsTablePart::ReorderChildren()
{
  NS_ASSERTION(PR_FALSE, "not yet implemented.");
}

void nsTablePart::NotifyContentComplete()
{
  // set the children in order
  ReorderChildren();
}

/** add a child to the table content.
  * tables are special because they require the content to be normalized, in order.
  * so this function doesn't really "append" the content, but adds it in the proper place,
  * possibly nested inside of implicit parents.
  * order is:
  *   Captions (optional)
  *   Column Groups (1 or more, possibly implicit)
  *   Row Groups 
  *     THEADs (optional)
  *     TFOOTs (optional)
  *     TBODY (at least 1, possibly implicit)
  * 
  * should be broken out into separate functions!
  */
PRBool nsTablePart::AppendChild (nsIContent * aContent)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg");
  PRBool result = PR_FALSE;
  PRBool newCells = PR_FALSE;
  PRBool contentHandled = PR_FALSE;

  // wait, stop!  need to check to see if this is really tableContent or not!
  nsITableContent *tableContentInterface = nsnull;
  nsresult rv = aContent->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
  if (NS_FAILED(rv))
  { // create an implicit cell and return the result of adding it to the table
    if (gsDebug==PR_TRUE)
      printf ("*** bad HTML in table, not yet implemented.\n");
    return PR_FALSE;
  }
  else
  {
    nsTableContent *tableContent = (nsTableContent *)tableContentInterface;
    const int contentType = tableContent->GetType();
    if ((contentType == nsITableContent::kTableCellType) ||
        (contentType==nsITableContent::kTableRowType))
    {
      if (gsDebug==PR_TRUE)
      {
        if (contentType == nsITableContent::kTableRowType)
          printf ("nsTablePart::AppendChild -- adding a row.\n");
        else
          printf ("nsTablePart::AppendChild -- adding a cell.\n");
      }
      // find last row group, if ! implicit, make one, append there
      nsTableRowGroup *group = nsnull;
      int index = ChildCount ();
      while ((0 < index) && (nsnull==group))
      {
        nsIContent *child = ChildAt (--index);    // child: REFCNT++
        if (nsnull != child)
        {
          nsTableContent * content = (nsTableContent *)child;
          if (content->GetType()==nsITableContent::kTableRowGroupType)
          {  
            group = (nsTableRowGroup *)content;
            NS_ADDREF(group);                     // group: REFCNT++
            // SEC: this code might be a space leak for tables >1 row group
          }
        }
        NS_RELEASE(child);                        // child: REFCNT--
      }
      if ((nsnull == group) || (! group->IsImplicit ()))
      {
        if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChild -- creating an implicit row group.\n");
        nsIAtom * rowGroupTag = NS_NewAtom(kRowGroupBodyTagString); // rowGroupTag: REFCNT++
        group = new nsTableRowGroup (rowGroupTag, PR_TRUE);
        NS_ADDREF(group);                         // group: REFCNT++
        AppendChild (group);
        group->SetTable(this);
        NS_RELEASE(rowGroupTag);                                  // rowGroupTag: REFCNT--
      }
      // group is guaranteed to be allocated at this point
      result = group->AppendChild(aContent);
      newCells = result;
      contentHandled = PR_TRUE;
      NS_RELEASE(group);                          // group: REFCNT--
    }
    else if (contentType == nsITableContent::kTableColType)
    {
      result = AppendColumn((nsTableCol *)aContent);
      newCells = result;
      contentHandled = PR_TRUE;
    }
    else if (contentType == nsITableContent::kTableCaptionType)
    {
      result = AppendCaption((nsTableCaption *)aContent);
      contentHandled = PR_TRUE; // whether we succeeded or not, we've "handled" this request
    }
    else if (contentType == nsITableContent::kTableRowGroupType)
    {
      result = AppendRowGroup((nsTableRowGroup *)aContent);
      if (PR_TRUE==result)
      {
        newCells = PR_TRUE;
      }
      contentHandled = PR_TRUE; // whether we succeeded or not, we've "handled" this request
    }
    else if (contentType == nsITableContent::kTableColGroupType)
    {
      result = AppendColGroup((nsTableColGroup *)aContent);
      if (PR_TRUE==result)
      {
        newCells = PR_TRUE;
      }
      contentHandled = PR_TRUE; // whether we succeeded or not, we've "handled" this request
    }

    // Remember to set the table variable -- gpk
    if (tableContentInterface)
      tableContentInterface->SetTable(this);

    /* if aContent is not a known content type, make a capion out of it */
    // SEC the logic here is very suspicious!!!!
    if (PR_FALSE==contentHandled)
    {
      if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChild -- content not handled!!!\n");
      nsTableCaption *caption = nsnull;
      nsIContent *lastChild = ChildAt (ChildCount() - 1); // lastChild: REFCNT++
      if (nsnull != lastChild)
      {
        nsTableContent * content = (nsTableContent *)lastChild;
        if (content->GetType()==nsITableContent::kTableCaptionType)
          caption = (nsTableCaption *)content;
        NS_RELEASE(lastChild);                            // lastChild: REFCNT--
      }
      if ((nsnull == caption) || (! caption->IsImplicit ()))
      {
        if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChild -- adding an implicit caption.\n");
        caption = new nsTableCaption (PR_TRUE);
        AppendCaption (caption);
      }
      result = caption->AppendChild (aContent);
    }
    /* if we added new cells, we need to fix up the cell map */
    if (newCells)
    {
      ResetCellMap ();
    }
  }
  NS_RELEASE(tableContentInterface);                                        // tableContentInterface: REFCNT--
  return result;
}

/* SEC: why can we only insertChildAt (captions or groups) ? */
PRBool nsTablePart::InsertChildAt(nsIContent * aContent, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg");
  // aIndex checked in nsHTMLContainer

  PRBool result = PR_FALSE;
  nsTableContent *tableContent = (nsTableContent *)aContent;
  const int contentType = tableContent->GetType();
  if ((contentType == nsITableContent::kTableCaptionType) ||
      (contentType == nsITableContent::kTableColGroupType) ||
      (contentType == nsITableContent::kTableRowGroupType))
  {
    result = nsHTMLContainer::InsertChildAt (aContent, aIndex);
    if (result)
    {
      tableContent->SetTable (this);
      ResetCellMap ();
    }
  }
  return result;
}

PRBool nsTablePart::ReplaceChildAt (nsIContent *aContent, PRInt32 aIndex)
{
  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;

  PRBool result = PR_FALSE;
  nsTableContent *tableContent = (nsTableColGroup *)aContent;
  const int contentType = tableContent->GetType();
  if ( (contentType == nsITableContent::kTableCaptionType) ||
       (contentType == nsITableContent::kTableColGroupType) ||
       (contentType == nsITableContent::kTableRowGroupType))
  {
    nsIContent *lastChild = ChildAt (aIndex);// lastChild: REFCNT++
    result = nsHTMLContainer::ReplaceChildAt (aContent, aIndex);
    if (result)
    {
      if (nsnull != lastChild)
        tableContent->SetTable (nsnull);
      tableContent->SetTable (this);
      ResetCellMap ();
    }
    NS_IF_RELEASE(lastChild);               // lastChild: REFCNT--
  }
  return result;
}

/**
 * Remove a child at the given position. The method is a no-op if
 * the index is invalid (too small or too large).
 */
PRBool nsTablePart::RemoveChildAt (PRInt32 aIndex)
{
  NS_PRECONDITION(0<=aIndex && aIndex<ChildCount(), "bad aIndex arg to RemoveChildAt");
  if (!(0<=aIndex && aIndex<ChildCount()))
    return PR_FALSE;

  nsIContent * lastChild = ChildAt (aIndex);    // lastChild: REFCNT++
  PRBool result = nsHTMLContainer::RemoveChildAt (aIndex);
  if (result)
  {
    nsTableContent *tableContent = (nsTableColGroup *)lastChild;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableRowType)
    {
      if (nsnull != lastChild)
        ((nsTableRow *)tableContent)->SetRowGroup (nsnull);
      tableContent->SetTable(nsnull);
      ResetCellMap ();
    }
  }
  NS_IF_RELEASE(lastChild);
  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendRowGroup (nsTableRowGroup *aContent)
{
  PRInt32 childIndex;
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  PRBool result = PR_TRUE;
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendRowGroup -- adding a row group.\n");
  // find the last row group of this kind and insert this row group after it
  // if there is no row group of this type already in the table, 
  // find the appropriate slot depending on this row group tag
  nsIAtom * rowGroupTag = aContent->GetTag();
  int childCount = ChildCount ();
  nsIAtom * tHeadTag = NS_NewAtom(kRowGroupHeadTagString); // tHeadTag: REFCNT++
  nsIAtom * tFootTag = NS_NewAtom(kRowGroupFootTagString); // tFootTag: REFCNT++
  nsIAtom * tBodyTag = NS_NewAtom(kRowGroupBodyTagString); // tBodyTag: REFCNT++
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsTableContent *tableChild = (nsTableContent *)ChildAt(childIndex); // tableChild: REFCNT++
    const int tableChildType = tableChild->GetType();
    // if we've found caption or colgroup, then just skip it and keep going
    if ((tableChildType == nsITableContent::kTableCaptionType) ||
        (tableChildType == nsITableContent::kTableColGroupType))
    {
      NS_RELEASE(tableChild);                                             // tableChild: REFCNT-- (a)
      continue;
    }
    // if we've found a row group, our action depends on what kind of row group
    else if (tableChildType == nsITableContent::kTableRowGroupType)
    {
      nsIAtom * tableChildTag = tableChild->GetTag();
      NS_RELEASE(tableChild);                                             // tableChild: REFCNT-- (b)
      // if aContent is a header and the current child is a header, keep going
      if (tHeadTag==rowGroupTag && tHeadTag==tableChildTag)
      {
        continue;
      }
      // if aContent is a footer and the current child is either a header or a footer, keep going
      else if (tFootTag==rowGroupTag &&
               (tHeadTag==tableChildTag || 
                tFootTag==tableChildTag))
      {
        continue;
      }
      // if aContent is a body and the current child is a footer, stop, we've found the spot
      else if (tBodyTag==rowGroupTag && tFootTag==tableChildTag)
      {
        continue;
      }
      // if aContent is a body and we've gotten this far, keep going
      else if (tBodyTag==rowGroupTag)
      {
        continue;
      }
      // otherwise, we must have found the right spot so stop
      else
      {
        break;
      }
    }
    // otherwise we're already at the right spot, so stop
    else
    {
      NS_RELEASE(tableChild);                                           // tableChild: REFCNT-- (c)
      break;
    }
  }
  NS_RELEASE(tFootTag);
  NS_RELEASE(tHeadTag);
  NS_RELEASE(tBodyTag);
  result = nsHTMLContainer::InsertChildAt(aContent, childIndex);
  if (result)
    ((nsTableContent *)aContent)->SetTable (this);
  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendColGroup(nsTableColGroup *aContent)
{
  PRInt32 childIndex;
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendColGroup -- adding a column group.\n");
  // find the last column group and insert this column group after it.
  // if there is no column group already in the table, make this the first child
  // after any caption
  int childCount = ChildCount ();
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsTableContent *tableChild = (nsTableContent *)ChildAt(childIndex); // tableChild: REFCNT++
    const int tableChildType = tableChild->GetType();
    NS_RELEASE(tableChild);                                             // tableChild: REFCNT--
    if (!((tableChildType == nsITableContent::kTableCaptionType) ||
         (tableChildType == nsITableContent::kTableColGroupType)))
      break;
  }
  PRBool result = nsHTMLContainer::InsertChildAt(aContent, childIndex);
  if (result)
  {
    ((nsTableContent *)aContent)->SetTable (this);
  }
  // if col group has a SPAN attribute, create implicit columns for the value of SPAN
  // what sucks is if we then get a COL for this COLGROUP, we have to delete all the
  // COLs we created for SPAN, and just contain the explicit COLs.
  PRInt32 span = 0; // SEC: TODO find a way to really get this
  for (PRInt32 i=0; i<span; i++)
  {
    nsTableCol *col = new nsTableCol(PR_TRUE);
    aContent->AppendChild (col);
  }

  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendColumn(nsTableCol *aContent)
{
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendColumn -- adding a column.\n");
  // find last col group, if ! implicit, make one, append there
  nsTableColGroup *group = nsnull;
  PRBool foundColGroup = PR_FALSE;
  int index = ChildCount ();
  while ((0 < index) && (PR_FALSE==foundColGroup))
  {
    nsIContent *child = ChildAt (--index);    // child: REFCNT++
    if (nsnull != child)
    {
      group = (nsTableColGroup *)child;
      foundColGroup = (PRBool)
        (group->GetType()==nsITableContent::kTableColGroupType);
      NS_RELEASE(child);                      // child: REFCNT--
    }
  }
  if ((PR_FALSE == foundColGroup) || (! group->IsImplicit ()))
  {
    if (gsDebug==PR_TRUE) 
      printf ("nsTablePart::AppendChild -- creating an implicit column group.\n");
    group = new nsTableColGroup (PR_TRUE);
    AppendChild (group);
    group->SetTable(this);
  }
  PRBool result = group->AppendChild (aContent);
  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendCaption(nsTableCaption *aContent)
{
  PRInt32 childIndex;
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendCaption -- adding a caption.\n");
  // find the last caption and insert this caption after it.
  // if there is no caption already in the table, make this the first child
  int childCount = ChildCount ();
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsTableContent *tableChild = (nsTableContent *)ChildAt(childIndex);
    const int tableChildType = tableChild->GetType();
    NS_RELEASE(tableChild);
    if (tableChildType != nsITableContent::kTableCaptionType)
      break;
  }
  PRBool result = nsHTMLContainer::InsertChildAt(aContent, childIndex);
  if (PR_TRUE==result)
  {
    ((nsTableContent *)aContent)->SetTable (this);
  }
  return result;
}

/* return the index of the first row group after aStartIndex */
PRInt32 nsTablePart::NextRowGroup (PRInt32 aStartIndex)
{
  int index = aStartIndex;
  int count = ChildCount ();

  while (++index < count)
  {
    nsIContent * child = ChildAt (index); // child: REFCNT++
    nsTableContent *tableContent = (nsTableColGroup *)child;
    const int contentType = tableContent->GetType();
    NS_RELEASE(child);                    // child: REFCNT--
    if (contentType == nsITableContent::kTableRowGroupType)
      return index;
  }
  return count;
}

// XXX This should be computed incrementally and updated
// automatically when rows are added/deleted. To do this, however,
// we need to change the way the ContentSink works and somehow
// notify this table when a row is modified. We can give the rows
// parent pointers, but that would bite.

// XXX nuke this; instead pretend the content sink is working
// incrementally like we want it to and build up the data a row at a
// time.

void nsTablePart::BuildCellMap ()
{
  if (gsDebug==PR_TRUE) printf("Build Cell Map...\n");
  int rowCount = GetRowCount ();
  if (0 == rowCount)
  {
    if (gsDebug==PR_TRUE) printf("0 row count.  Returning.\n");
    mColCount = GetSpecifiedColumnCount ();  // at least set known column count
    EnsureColumns();
    return;
  }

  // Make an educated guess as to how many columns we have. It's
  // only a guess because we can't know exactly until we have
  // processed the last row.

  int childCount = ChildCount ();
  int groupIndex = NextRowGroup (-1);
  if (0 == mColCount)
    mColCount = GetSpecifiedColumnCount ();
  if (0 == mColCount) // no column parts
  {
    nsTableRowGroup *rowGroup = (nsTableRowGroup*)(ChildAt (groupIndex)); // rowGroup: REFCNT++
    nsTableRow *row = (nsTableRow *)(rowGroup->ChildAt (0));              // row: REFCNT++
    mColCount = row->GetMaxColumns ();
    if (gsDebug==PR_TRUE) printf("mColCount=0 at start.  Guessing col count to be %d from a row.\n", mColCount);
    NS_RELEASE(rowGroup);                                                 // rowGroup: REFCNT--
    NS_RELEASE(row);                                                      // row: REFCNT--
  }
  if (nsnull==mCellMap)
    mCellMap = new nsCellMap(rowCount, mColCount);
  else
    mCellMap->Reset(rowCount, mColCount);
  if (gsDebug==PR_TRUE) printf("mCellMap set to (%d, %d)\n", rowCount, mColCount);
  int rowStart = 0;
  if (gsDebug==PR_TRUE) printf("childCount is %d\n", childCount);
  while (groupIndex < childCount)
  {
    if (gsDebug==PR_TRUE) printf("  groupIndex is %d\n", groupIndex);
    if (gsDebug==PR_TRUE) printf("  rowStart is %d\n", rowStart);
    nsTableRowGroup *rowGroup = (nsTableRowGroup *)ChildAt (groupIndex);  // rowGroup: REFCNT++
    int groupRowCount = rowGroup->ChildCount ();
    if (gsDebug==PR_TRUE) printf("  groupRowCount is %d\n", groupRowCount);
    for (int rowIndex = 0; rowIndex < groupRowCount; rowIndex++)
    {
      nsTableRow *row = (nsTableRow *)(rowGroup->ChildAt (rowIndex));     // row: REFCNT++
      int cellCount = row->ChildCount ();
      int cellIndex = 0;
      int colIndex = 0;
      if (gsDebug==PR_TRUE) 
        DumpCellMap();
      if (gsDebug==PR_TRUE) printf("    rowIndex is %d, row->SetRowIndex(%d)\n", rowIndex, rowIndex + rowStart);
      row->SetRowIndex (rowIndex + rowStart);
      while (colIndex < mColCount)
      {
        if (gsDebug==PR_TRUE) printf("      colIndex = %d, with mColCount = %d\n", colIndex, mColCount);
        CellData *data =mCellMap->GetCellAt(rowIndex + rowStart, colIndex);
        if (nsnull == data)
        {
          if (gsDebug==PR_TRUE) printf("      null data from GetCellAt(%d,%d)\n", rowIndex+rowStart, colIndex);
          if (gsDebug==PR_TRUE) printf("      cellIndex=%d, cellCount=%d)\n", cellIndex, cellCount);
          if (cellIndex < cellCount)
          {
            nsTableCell* cell = (nsTableCell *) row->ChildAt (cellIndex); // cell: REFCNT++
            if (gsDebug==PR_TRUE) printf("      calling BuildCellIntoMap(cell, %d, %d), and incrementing cellIndex\n", rowIndex + rowStart, colIndex);
            BuildCellIntoMap (cell, rowIndex + rowStart, colIndex);
            NS_RELEASE(cell);                                             // cell: REFCNT--
            cellIndex++;
          }
        }
        colIndex++;
      }

      if (cellIndex < cellCount)  
      {
        // We didn't use all the cells in this row up. Grow the cell
        // data because we now know that we have more columns than we
        // originally thought we had.
        if (gsDebug==PR_TRUE) printf("   calling GrowCellMap because cellIndex < %d\n", cellIndex, cellCount);
        GrowCellMap (cellCount);
        while (cellIndex < cellCount)
        {
          if (gsDebug==PR_TRUE) printf("     calling GrowCellMap again because cellIndex < %d\n", cellIndex, cellCount);
          GrowCellMap (colIndex + 1); // ensure enough cols in map, may be low due to colspans
          CellData *data =mCellMap->GetCellAt(rowIndex + rowStart, colIndex);
          if (data == nsnull)
          {
            nsTableCell* cell = (nsTableCell *) row->ChildAt (cellIndex); // cell: REFCNT++
            BuildCellIntoMap (cell, rowIndex + rowStart, colIndex);
            cellIndex++;
            NS_RELEASE(cell);                                             // cell: REFCNT--
          }
          colIndex++;
        }
      }
      NS_RELEASE(row);      // row: REFCNT--
    }
    NS_RELEASE(rowGroup);   // rowGroup: REFCNT--
    rowStart += groupRowCount;
    groupIndex = NextRowGroup (groupIndex);
  }
  if (gsDebug==PR_TRUE)
    DumpCellMap ();
  EnsureColumns();
}

/**
  */
void nsTablePart::DumpCellMap () const
{
  printf("dumping CellMap:\n");
  if (nsnull != mCellMap)
  {
    int rowCount = mCellMap->GetRowCount();
    int cols = mCellMap->GetColCount();
    for (int r = 0; r < rowCount; r++)
    {
      if (gsDebug==PR_TRUE)
      { printf("row %d", r);
        printf(": ");
      }
      for (int c = 0; c < cols; c++)
      {
        CellData *cd =mCellMap->GetCellAt(r, c);
        if (cd != nsnull)
        {
          if (cd->mCell != nsnull)
          {
            printf("C%d,%d ", r, c);
            printf("     ");
          }
          else
          {
            nsTableCell *cell = cd->mRealCell->mCell;
            nsTableRow *row = cell->GetRow();
            int rr = row->GetRowIndex ();
            int cc = cell->GetColIndex ();
            printf("S%d,%d ", rr, cc);
            if (cd->mOverlap != nsnull)
            {
              cell = cd->mOverlap->mCell;
              nsTableRow* row2 = cell->GetRow();
              rr = row2->GetRowIndex ();
              cc = cell->GetColIndex ();
              printf("O%d,%c ", rr, cc);
              NS_RELEASE(row2);
            }
            else
              printf("     ");
            NS_RELEASE(row);
          }
        }
        else
          printf("----      ");
      }
      printf("\n");
    }
  }
  else
    printf ("[nsnull]");
}

void nsTablePart::BuildCellIntoMap (nsTableCell *aCell, PRInt32 aRowIndex, PRInt32 aColIndex)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (aColIndex < mColCount, "bad column index arg");
  NS_PRECONDITION (aRowIndex < GetRowCount(), "bad row index arg");

  // Setup CellMap for this cell
  int rowSpan = GetEffectiveRowSpan (aRowIndex, aCell);
  int colSpan = aCell->GetColSpan ();
  if (gsDebug==PR_TRUE) printf("        BuildCellIntoMap. rowSpan = %d, colSpan = %d\n", rowSpan, colSpan);

  // Grow the mCellMap array if we will end up addressing
  // some new columns.
  if (mColCount < (aColIndex + colSpan))
  {
    if (gsDebug==PR_TRUE) printf("        mColCount=%d<aColIndex+colSpan so calling GrowCellMap(%d)\n", mColCount, aColIndex+colSpan);
    GrowCellMap (aColIndex + colSpan);
  }

  // Setup CellMap for this cell in the table
  CellData *data = new CellData ();
  data->mCell = aCell;
  data->mRealCell = data;
  if (gsDebug==PR_TRUE) printf("        calling mCellMap->SetCellAt(data, %d, %d)\n", aRowIndex, aColIndex);
  mCellMap->SetCellAt(data, aRowIndex, aColIndex);
  aCell->SetColIndex (aColIndex);

  // Create CellData objects for the rows that this cell spans. Set
  // their mCell to nsnull and their mRealCell to point to data. If
  // there were no column overlaps then we could use the same
  // CellData object for each row that we span...
  if ((1 < rowSpan) || (1 < colSpan))
  {
    if (gsDebug==PR_TRUE) printf("        spans\n");
    for (int rowIndex = 0; rowIndex < rowSpan; rowIndex++)
    {
      if (gsDebug==PR_TRUE) printf("          rowIndex = %d\n", rowIndex);
      int workRow = aRowIndex + rowIndex;
      if (gsDebug==PR_TRUE) printf("          workRow = %d\n", workRow);
      for (int colIndex = 0; colIndex < colSpan; colIndex++)
      {
        if (gsDebug==PR_TRUE) printf("            colIndex = %d\n", colIndex);
        int workCol = aColIndex + colIndex;
        if (gsDebug==PR_TRUE) printf("            workCol = %d\n", workCol);
        CellData *testData = mCellMap->GetCellAt(workRow, workCol);
        if (nsnull == testData)
        {
          CellData *spanData = new CellData ();
          spanData->mRealCell = data;
          if (gsDebug==PR_TRUE) printf("            null GetCellAt(%d, %d) so setting to spanData\n", workRow, workCol);
          mCellMap->SetCellAt(spanData, workRow, workCol);
        }
        else if ((0 < rowIndex) || (0 < colIndex))
        { // we overlap, replace existing data, it might be shared
          if (gsDebug==PR_TRUE) printf("            overlapping Cell from GetCellAt(%d, %d) so setting to spanData\n", workRow, workCol);
          CellData *overlap = new CellData ();
          overlap->mCell = testData->mCell;
          overlap->mRealCell = testData->mRealCell;
          overlap->mOverlap = data;
          mCellMap->SetCellAt(overlap, workRow, workCol);
        }
      }
    }
  }
}

void nsTablePart::GrowCellMap (PRInt32 aColCount)
{
  if (nsnull!=mCellMap)
  {
    if (mColCount < aColCount)
    {
      mCellMap->GrowTo(aColCount);
    }
    mColCount = aColCount;
  }
}

PRInt32 nsTablePart::GetEffectiveRowSpan (PRInt32 aRowIndex, nsTableCell *aCell)
{
  NS_PRECONDITION (nsnull!=aCell, "bad cell arg");
  NS_PRECONDITION (0<=aRowIndex && aRowIndex<GetRowCount(), "bad row index arg");

  int rowSpan = aCell->GetRowSpan ();
  int rowCount = GetRowCount ();
  if (rowCount < (aRowIndex + rowSpan))
    return (rowCount - aRowIndex);
  return rowSpan;
}


/**
 * Create a frame object that will layout this table.
 */
nsresult
nsTablePart::CreateFrame(nsIPresContext* aPresContext,
                         nsIFrame* aParentFrame,
                         nsIStyleContext* aStyleContext,
                         nsIFrame*& aResult)
{
  NS_PRECONDITION(nsnull!=aPresContext, "bad arg");

  nsIFrame* frame;
  nsresult rv = nsTableOuterFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return rv;
}

void nsTablePart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue)
{
  nsHTMLValue val;

  if (aAttribute == nsHTMLAtoms::width) 
  {
    ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if ( aAttribute == nsHTMLAtoms::border)
  {
    nsHTMLValue val;
    nsAutoString tmp(aValue);
    tmp.StripWhitespace();
    if (0 == tmp.Length()) {
      // Just enable the border; same as border=1
      val.SetEmptyValue();/* XXX pixels? */
    }
    else 
    {
      ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
    }
    nsHTMLTagContent::SetAttribute(aAttribute, val);
  }
  else if (aAttribute == nsHTMLAtoms::cellspacing ||
           aAttribute == nsHTMLAtoms::cellpadding) 
  {
    ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) 
  {
    ParseColor(aValue, val);
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    nsHTMLValue val;
    if (ParseTableAlignParam(aValue, val)) {
      nsHTMLTagContent::SetAttribute(aAttribute, val);
    }
    return;
  }
}

void nsTablePart::MapAttributesInto(nsIStyleContext* aContext,
                                    nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  float p2t;
  nsHTMLValue value;

  // width
  GetAttribute(nsHTMLAtoms::width, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetData(kStylePositionSID);
    switch (value.GetUnit()) {
    case eHTMLUnit_Percent:
      position->mWidth.SetPercentValue(value.GetPercentValue());
      break;

    case eHTMLUnit_Pixel:
      p2t = aPresContext->GetPixelsToTwips();
      position->mWidth.SetCoordValue(nscoord(p2t * (float)value.GetPixelValue()));
      break;
    }
  }
  // border
  GetTableBorder(this, aContext, aPresContext, PR_FALSE);

  // align
  GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    NS_ASSERTION(value.GetUnit() == eHTMLUnit_Enumerated, "unexpected unit");
    nsStyleDisplay* display = (nsStyleDisplay*)aContext->GetData(kStyleDisplaySID);

    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    }
  }
}

void nsTablePart::GetTableBorder(nsIHTMLContent* aContent,
                                 nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext,
                                 PRBool aForCell)
{
  NS_PRECONDITION(nsnull!=aContent, "bad content arg");
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue value;

  aContent->GetAttribute(nsHTMLAtoms::border, value);
  if ((value.GetUnit() == eHTMLUnit_Pixel) || 
      (value.GetUnit() == eHTMLUnit_Empty)) {
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetData(kStyleSpacingSID);
    float p2t = aPresContext->GetPixelsToTwips();
    nsStyleCoord twips;
    if (aForCell || (value.GetUnit() == eHTMLUnit_Empty)) {
      twips.SetCoordValue(nscoord(NS_INT_PIXELS_TO_TWIPS(1, p2t)));
    }
    else {
      twips.SetCoordValue(nscoord(NS_INT_PIXELS_TO_TWIPS(value.GetPixelValue(), p2t)));
    }
    nsStyleCoord  two(nscoord(NS_INT_PIXELS_TO_TWIPS(2,p2t)));

    spacing->mPadding.SetTop(two);
    spacing->mPadding.SetRight(two);
    spacing->mPadding.SetBottom(two);
    spacing->mPadding.SetLeft(two);

    spacing->mBorder.SetTop(twips);
    spacing->mBorder.SetRight(twips);
    spacing->mBorder.SetBottom(twips);
    spacing->mBorder.SetLeft(twips);

    if (spacing->mBorderStyle[0] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[0] = NS_STYLE_BORDER_STYLE_SOLID;
    }
    if (spacing->mBorderStyle[1] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[1] = NS_STYLE_BORDER_STYLE_SOLID;
    }
    if (spacing->mBorderStyle[2] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[2] = NS_STYLE_BORDER_STYLE_SOLID;
    }
    if (spacing->mBorderStyle[3] == NS_STYLE_BORDER_STYLE_NONE) {
      spacing->mBorderStyle[3] = NS_STYLE_BORDER_STYLE_SOLID;
    }
  }
}

/* ---------- Global Functions ---------- */

/**
  */
nsresult
NS_NewTablePart(nsIHTMLContent** aInstancePtrResult,
               nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "nsnull ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  nsIHTMLContent* table = new nsTablePart(aTag);
  if (nsnull == table) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return table->QueryInterface(kIHTMLContentIID, (void **) aInstancePtrResult);
}

