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
#include "nsIHTMLAttributes.h"
#include "nsGenericHTMLElement.h"

static NS_DEFINE_IID(kITableContentIID, NS_ITABLECONTENT_IID);

#ifdef NS_DEBUG
static PRBool gsDebug = PR_FALSE;
static PRBool gsNoisyRefs = PR_FALSE;
static PRBool gsDebugWarnings = PR_TRUE;
#else
static const PRBool gsDebug = PR_FALSE;
static const PRBool gsNoisyRefs = PR_FALSE;
static const PRBool gsDebugWarnings = PR_FALSE;
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

//QQQ can remove mColCount?

/** constructor
  * I do not check or addref aTag because my superclass does that for me
  */
nsTablePart::nsTablePart(nsIAtom* aTag)
  : nsHTMLContainer(aTag)
{ 
}

/**
  */
nsTablePart::~nsTablePart()
{
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
  */
NS_IMETHODIMP
nsTablePart::AppendChildTo (nsIContent * aContent, PRBool aNotify)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg");
  PRBool contentHandled = PR_FALSE;

  // wait, stop!  need to check to see if this is really tableContent or not!
  nsITableContent *tableContentInterface = nsnull;
  PRBool result = PR_TRUE;  // to be removed when called methods are COM-ized
  nsresult rv = aContent->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
  if (NS_FAILED(rv))
  { // hold onto the non-table content, but do nothing with it
    if (PR_TRUE==gsDebugWarnings)
      printf ("non-table content inserted into table.");
    rv = nsHTMLContainer::AppendChildTo(aContent, aNotify);
    return rv;
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
          printf ("nsTablePart::AppendChildTo -- adding a row.\n");
        else
          printf ("nsTablePart::AppendChildTo -- adding a cell.\n");
      }
      // find last row group, if ! implicit, make one, append there
      nsTableRowGroup *group = nsnull;
      int index;
      ChildCount (index);
      while ((0 < index) && (nsnull==group))
      {
        nsIContent *child;
        ChildAt (--index, child);    // child: REFCNT++
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
      PRBool groupIsImplicit = PR_FALSE;
      if (nsnull!=group)
        group->IsSynthetic(groupIsImplicit);
      if ((nsnull == group) || (PR_FALSE==groupIsImplicit))
      {
        if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChildTo -- creating an implicit row group.\n");
        nsIAtom * rowGroupTag = NS_NewAtom(kRowGroupBodyTagString); // rowGroupTag: REFCNT++
        group = new nsTableRowGroup (rowGroupTag, PR_TRUE);
        NS_ADDREF(group);                         // group: REFCNT++
        rv = AppendChildTo (group, PR_FALSE);
        if (NS_OK==rv)
          group->SetTable(this);
        NS_RELEASE(rowGroupTag);                                  // rowGroupTag: REFCNT--
      }
      // group is guaranteed to be allocated at this point
      rv = group->AppendChildTo(aContent, PR_FALSE);
      contentHandled = PR_TRUE;
      NS_RELEASE(group);                          // group: REFCNT--
    }
    else if (contentType == nsITableContent::kTableColType)
    {
      // TODO: switch Append* to COM interfaces
      result = AppendColumn((nsTableCol *)aContent);
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
      if (PR_FALSE==result)
        rv=NS_ERROR_FAILURE;
      contentHandled = PR_TRUE; // whether we succeeded or not, we've "handled" this request
    }
    else if (contentType == nsITableContent::kTableColGroupType)
    {
      result = AppendColGroup((nsTableColGroup *)aContent);
      if (PR_FALSE==result)
        rv = NS_ERROR_FAILURE;
      contentHandled = PR_TRUE; // whether we succeeded or not, we've "handled" this request
    }

    /* if aContent is not a known content type, make a capion out of it */
    // SEC the logic here is very suspicious!!!!
    if (PR_FALSE==contentHandled)
    {
      if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChildTo -- content not handled!!!\n");
      nsTableCaption *caption = nsnull;
      nsIContent *lastChild;
      PRInt32 numKids;
      ChildCount(numKids);
      ChildAt (numKids - 1, lastChild); // lastChild: REFCNT++
      if (nsnull != lastChild)
      {
        nsTableContent * content = (nsTableContent *)lastChild;
        if (content->GetType()==nsITableContent::kTableCaptionType)
          caption = (nsTableCaption *)content;
        NS_RELEASE(lastChild);                            // lastChild: REFCNT--
      }
      PRBool captionIsImplicit = PR_FALSE;
      if (nsnull!=caption)
        caption->IsSynthetic(captionIsImplicit);
      if ((nsnull == caption) || (PR_FALSE==captionIsImplicit))
      {
        if (gsDebug==PR_TRUE) printf ("nsTablePart::AppendChildTo -- adding an implicit caption.\n");
        caption = new nsTableCaption (PR_TRUE);
        result = AppendCaption (caption);
        // check result
      }
      result = caption->AppendChildTo (aContent, PR_FALSE);
    }
  }
  NS_RELEASE(tableContentInterface);                                        // tableContentInterface: REFCNT--

  return rv;
}

/* SEC: why can we only insertChildAt (captions or groups) ? */
NS_IMETHODIMP
nsTablePart::InsertChildAt(nsIContent * aContent, PRInt32 aIndex,
                           PRBool aNotify)
{
  NS_PRECONDITION(nsnull!=aContent, "bad arg");
  // aIndex checked in nsHTMLContainer

  nsresult rv = NS_OK;
  nsTableContent *tableContent = (nsTableContent *)aContent;
  const int contentType = tableContent->GetType();
  if ((contentType == nsITableContent::kTableCaptionType) ||
      (contentType == nsITableContent::kTableColGroupType) ||
      (contentType == nsITableContent::kTableRowGroupType))
  {
    rv = nsHTMLContainer::InsertChildAt (aContent, aIndex, PR_FALSE);
    if (NS_OK == rv)
    {
      tableContent->SetTable (this);
    }
  }

  return rv;
}

NS_IMETHODIMP
nsTablePart::ReplaceChildAt (nsIContent *aContent, PRInt32 aIndex,
                             PRBool aNotify)
{
  PRInt32 numKids;
  ChildCount(numKids);
  NS_PRECONDITION(nsnull!=aContent, "bad aContent arg to ReplaceChildAt");
  NS_PRECONDITION(0<=aIndex && aIndex<numKids, "bad aIndex arg to ReplaceChildAt");
  if ((nsnull==aContent) || !(0<=aIndex && aIndex<numKids))
    return NS_OK;

  nsresult rv = NS_OK;
  nsTableContent *tableContent = (nsTableColGroup *)aContent;
  const int contentType = tableContent->GetType();
  if ( (contentType == nsITableContent::kTableCaptionType) ||
       (contentType == nsITableContent::kTableColGroupType) ||
       (contentType == nsITableContent::kTableRowGroupType))
  {
    nsIContent *lastChild;
    ChildAt (aIndex, lastChild);// lastChild: REFCNT++
    rv = nsHTMLContainer::ReplaceChildAt (aContent, aIndex, PR_FALSE);
    if (NS_OK == rv)
    {
      if (nsnull != lastChild)
        tableContent->SetTable (nsnull);
      tableContent->SetTable (this);
    }
    NS_IF_RELEASE(lastChild);               // lastChild: REFCNT--
  }

  return rv;
}

/**
 * Remove a child at the given position. The method is a no-op if
 * the index is invalid (too small or too large).
 */
NS_IMETHODIMP
nsTablePart::RemoveChildAt (PRInt32 aIndex, PRBool aNotify)
{
  PRInt32 numKids;
  ChildCount(numKids);
  NS_PRECONDITION(0<=aIndex && aIndex<numKids, "bad aIndex arg to RemoveChildAt");
  if (!(0<=aIndex && aIndex<numKids))
    return NS_OK;

  nsIContent * lastChild;
  ChildAt (aIndex, lastChild);    // lastChild: REFCNT++
  nsresult rv = nsHTMLContainer::RemoveChildAt (aIndex, PR_FALSE);
  if (NS_OK == rv)
  {
    nsTableContent *tableContent = (nsTableColGroup *)lastChild;
    const int contentType = tableContent->GetType();
    if (contentType == nsITableContent::kTableRowType)
    {
      if (nsnull != lastChild)
        ((nsTableRow *)tableContent)->SetRowGroup (nsnull);
      tableContent->SetTable(nsnull);
    }
  }
  NS_IF_RELEASE(lastChild);

  return rv;
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
  nsIAtom * rowGroupTag;
  aContent->GetTag(rowGroupTag);
  PRInt32 childCount;
  ChildCount (childCount);
  nsIAtom * tHeadTag = NS_NewAtom(kRowGroupHeadTagString); // tHeadTag: REFCNT++
  nsIAtom * tFootTag = NS_NewAtom(kRowGroupFootTagString); // tFootTag: REFCNT++
  nsIAtom * tBodyTag = NS_NewAtom(kRowGroupBodyTagString); // tBodyTag: REFCNT++
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsITableContent *tableContentInterface = nsnull;
    nsIContent * child;
    ChildAt(childIndex, child); // tableChild: REFCNT++
    nsresult rv = child->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
    if (NS_FAILED(rv))
    {
      NS_RELEASE(child);                                             // tableChild: REFCNT-- (a)
      continue;
    }
    nsTableContent *tableChild = (nsTableContent *)tableContentInterface;
    const int tableChildType = tableChild->GetType();
    // if we've found caption or colgroup, then just skip it and keep going
    if ((tableChildType == nsITableContent::kTableCaptionType) ||
        (tableChildType == nsITableContent::kTableColGroupType))
    {
      NS_RELEASE(child);                                             // tableChild: REFCNT-- (b)
      NS_RELEASE(tableContentInterface);                                    // tableContentInterface: REFCNT--
      continue;
    }
    // if we've found a row group, our action depends on what kind of row group
    else if (tableChildType == nsITableContent::kTableRowGroupType)
    {
      // XXX we are leaking tableChildTag
      nsIAtom * tableChildTag;
      tableChild->GetTag(tableChildTag);
      NS_RELEASE(child);                                             // tableChild: REFCNT-- (c)
      NS_RELEASE(tableContentInterface);                                    // tableContentInterface: REFCNT--
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
      NS_RELEASE(child);                                           // tableChild: REFCNT-- (d)
      NS_RELEASE(tableContentInterface);                                    // tableContentInterface: REFCNT--
      break;
    }
  }
  NS_RELEASE(tFootTag);
  NS_RELEASE(tHeadTag);
  NS_RELEASE(tBodyTag);

  nsresult rv = nsHTMLContainer::InsertChildAt(aContent, childIndex, PR_FALSE);
  if (NS_OK==rv)
    ((nsTableContent *)aContent)->SetTable (this);
  else
    result = PR_FALSE;

  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendColGroup(nsTableColGroup *aContent)
{
  PRBool result = PR_TRUE;  // to be removed when I'm COM-ized
  PRInt32 childIndex;
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendColGroup -- adding a column group.\n");
  // find the last column group and insert this column group after it.
  // if there is no column group already in the table, make this the first child
  // after any caption
  PRInt32 childCount;
  ChildCount (childCount);
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsIContent *child;
    ChildAt(childIndex, child);                         // child: REFCNT++
    nsITableContent *tableContentInterface = nsnull;
    nsresult rv = child->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
    if (NS_FAILED(rv))
    {
      NS_RELEASE(child);
      continue;
    }
    const int tableChildType = tableContentInterface->GetType();
    NS_RELEASE(child);                                                  // tableChild: REFCNT--
    NS_RELEASE(tableContentInterface); 
    if (!((tableChildType == nsITableContent::kTableCaptionType) ||
         (tableChildType == nsITableContent::kTableColGroupType)))
      break;
  }
  nsresult rv = nsHTMLContainer::InsertChildAt(aContent, childIndex, PR_FALSE);
  if (NS_OK==rv)
    ((nsTableContent *)aContent)->SetTable (this);
  else
    result = PR_FALSE;

  // if col group has a SPAN attribute, create implicit columns for
  // the value of SPAN what sucks is if we then get a COL for this
  // COLGROUP, we have to delete all the COLs we created for SPAN, and
  // just contain the explicit COLs.
  
  // TODO: move this into the OK clause above

  PRInt32 span = 0; // SEC: TODO find a way to really get this
  for (PRInt32 i=0; i<span; i++)
  {
    nsTableCol *col = new nsTableCol(PR_TRUE);
    rv = aContent->AppendChildTo (col, PR_FALSE);
    if (NS_OK!=rv)
    {
      result = PR_FALSE;
      break;
    }
  }

  return result;
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendColumn(nsTableCol *aContent)
{
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  nsresult rv = NS_OK;
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendColumn -- adding a column.\n");
  // find last col group, if ! implicit, make one, append there
  nsTableColGroup *group = nsnull;
  PRBool foundColGroup = PR_FALSE;
  PRInt32 index;
  ChildCount (index);
  while ((0 < index) && (PR_FALSE==foundColGroup))
  {
    nsITableContent *tableContentInterface = nsnull;
    nsIContent *child;
    ChildAt (--index, child);                          // child: REFCNT++
    nsresult rv = child->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
    NS_RELEASE(child);                                            // tableChild: REFCNT-- (a)
    if (NS_FAILED(rv))
    {
      continue;
    }

    group = (nsTableColGroup *)tableContentInterface;
    foundColGroup = (PRBool)
      (group->GetType()==nsITableContent::kTableColGroupType);
    if (PR_FALSE==foundColGroup)
    { // release all the table contents that are not a col group
      NS_RELEASE(tableContentInterface);                                    // tableContentInterface: REFCNT-- (a)
    }
  }
  PRBool groupIsImplicit = PR_FALSE;
  if (nsnull!=group)
    group->IsSynthetic(groupIsImplicit);
  if ((PR_FALSE == foundColGroup) || (PR_FALSE==groupIsImplicit))
  {
    if (gsDebug==PR_TRUE) 
      printf ("nsTablePart::AppendChildTo -- creating an implicit column group.\n");
    group = new nsTableColGroup (PR_TRUE);
    rv = AppendChildTo (group, PR_FALSE);
    if (NS_OK==rv)
      group->SetTable(this);
  }
  if (NS_OK==rv)
    rv = group->AppendChildTo (aContent, PR_FALSE);

  NS_IF_RELEASE(group);                                    // tableContentInterface: REFCNT-- (b)
  return (PRBool)(NS_OK==rv);
}

/** protected method for appending a column group to this table */
PRBool nsTablePart::AppendCaption(nsTableCaption *aContent)
{
  nsresult rv = NS_OK;
  PRInt32 childIndex;
  NS_PRECONDITION(nsnull!=aContent, "null arg.");
  if (gsDebug==PR_TRUE)
    printf ("nsTablePart::AppendCaption -- adding a caption.\n");
  // find the last caption and insert this caption after it.
  // if there is no caption already in the table, make this the first child
  PRInt32 childCount;
  ChildCount (childCount);
  for (childIndex = 0; childIndex < childCount; childIndex++)
  {
    nsITableContent *tableContentInterface = nsnull;
    nsIContent *child;
    ChildAt (childIndex, child);                      // child: REFCNT++
    nsresult rv = child->QueryInterface(kITableContentIID, 
                                         (void **)&tableContentInterface);  // tableContentInterface: REFCNT++
    NS_RELEASE(child);                                            // tableChild: REFCNT-- (a)
    if (NS_FAILED(rv))
    {
      continue;
    }
    const int tableChildType = tableContentInterface->GetType();
    NS_RELEASE(tableContentInterface);
    if (tableChildType != nsITableContent::kTableCaptionType)
      break;
  }
  rv = nsHTMLContainer::InsertChildAt(aContent, childIndex, PR_FALSE);
  if (NS_OK==rv)
    ((nsTableContent *)aContent)->SetTable (this);

  return (PRBool)(NS_OK==rv);
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

NS_IMETHODIMP
nsTablePart::SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify)
{
  nsHTMLValue val;

  
  if (aAttribute == nsHTMLAtoms::width) 
  {
    ParseValueOrPercent(aValue, val, eHTMLUnit_Pixel);
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if ( aAttribute == nsHTMLAtoms::cols)
  { // it'll either be empty, or have an integer value
    nsAutoString tmp(aValue);
    tmp.StripWhitespace();
    if (0 == tmp.Length()) {
      val.SetEmptyValue();
    }
    else 
    {
      ParseValue(aValue, 0, val, eHTMLUnit_Integer);
    }
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if ( aAttribute == nsHTMLAtoms::border)
  {
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
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if (aAttribute == nsHTMLAtoms::cellspacing ||
           aAttribute == nsHTMLAtoms::cellpadding) 
  {
    ParseValue(aValue, 0, val, eHTMLUnit_Pixel);
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if (aAttribute == nsHTMLAtoms::bgcolor) 
  {
    ParseColor(aValue, val);
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }
  else if (aAttribute == nsHTMLAtoms::align) {
    if (ParseTableAlignParam(aValue, val)) {
      return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
    }
  }
  /* HTML 4 attributes */
  // TODO: only partially implemented
  else if (aAttribute == nsHTMLAtoms::summary) {
    val.SetStringValue(aValue);
    return nsHTMLTagContent::SetAttribute(aAttribute, val, aNotify);
  }

  // Use default attribute catching code
  return nsHTMLTagContent::SetAttribute(aAttribute, aValue, aNotify);
}

static void 
MapTableBorderInto(nsIHTMLAttributes* aAttributes,
                   nsIStyleContext* aContext,
                   nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  nsHTMLValue value;

  aAttributes->GetAttribute(nsHTMLAtoms::border, value);
  if ((value.GetUnit() == eHTMLUnit_Pixel) || 
      (value.GetUnit() == eHTMLUnit_Empty)) {
    nsStyleSpacing* spacing = (nsStyleSpacing*)
      aContext->GetMutableStyleData(eStyleStruct_Spacing);
    float p2t = aPresContext->GetPixelsToTwips();
    nsStyleCoord twips;
    if (value.GetUnit() == eHTMLUnit_Empty) {
      twips.SetCoordValue(NSIntPixelsToTwips(1, p2t));
    }
    else {
      twips.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
    }

#if 0
    nsStyleCoord  two(NSIntPixelsToTwips(2, p2t));

    spacing->mPadding.SetTop(two);
    spacing->mPadding.SetRight(two);
    spacing->mPadding.SetBottom(two);
    spacing->mPadding.SetLeft(two);
#endif

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

static void
MapAttributesInto(nsIHTMLAttributes* aAttributes,
                  nsIStyleContext* aContext,
                  nsIPresContext* aPresContext)
{
  NS_PRECONDITION(nsnull!=aContext, "bad style context arg");
  NS_PRECONDITION(nsnull!=aPresContext, "bad presentation context arg");

  float p2t = aPresContext->GetPixelsToTwips();
  nsHTMLValue value;

  // width
  aAttributes->GetAttribute(nsHTMLAtoms::width, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    nsStylePosition* position = (nsStylePosition*)
      aContext->GetMutableStyleData(eStyleStruct_Position);
    switch (value.GetUnit()) {
    case eHTMLUnit_Percent:
      position->mWidth.SetPercentValue(value.GetPercentValue());
      break;

    case eHTMLUnit_Pixel:
      position->mWidth.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
      break;
    }
  }
  // border
  MapTableBorderInto(aAttributes, aContext, aPresContext);

  // align
  aAttributes->GetAttribute(nsHTMLAtoms::align, value);
  if (value.GetUnit() == eHTMLUnit_Enumerated) {  // it may be another type if illegal
    nsStyleDisplay* display = (nsStyleDisplay*)aContext->GetMutableStyleData(eStyleStruct_Display);
    switch (value.GetIntValue()) {
    case NS_STYLE_TEXT_ALIGN_LEFT:
      display->mFloats = NS_STYLE_FLOAT_LEFT;
      break;

    case NS_STYLE_TEXT_ALIGN_RIGHT:
      display->mFloats = NS_STYLE_FLOAT_RIGHT;
      break;
    }
  }

  // cellpadding
  nsStyleTable* tableStyle=nsnull;
  aAttributes->GetAttribute(nsHTMLAtoms::cellpadding, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
    tableStyle->mCellPadding.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
  }

  // cellspacing  (reuses tableStyle if already resolved)
  aAttributes->GetAttribute(nsHTMLAtoms::cellspacing, value);
  if (value.GetUnit() == eHTMLUnit_Pixel) {
    if (nsnull==tableStyle)
      tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
    tableStyle->mCellSpacing.SetCoordValue(NSIntPixelsToTwips(value.GetPixelValue(), p2t));
  }
  else
  { // XXX: remove me as soon as we get this from the style sheet
    if (nsnull==tableStyle)
      tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
    tableStyle->mCellSpacing.SetCoordValue(NSIntPixelsToTwips(2, p2t));
  }

  // cols
  aAttributes->GetAttribute(nsHTMLAtoms::cols, value);
  if (value.GetUnit() != eHTMLUnit_Null) {
    if (nsnull==tableStyle)
      tableStyle = (nsStyleTable*)aContext->GetMutableStyleData(eStyleStruct_Table);
    if (value.GetUnit() == eHTMLUnit_Integer)
      tableStyle->mCols = value.GetIntValue();
    else // COLS had no value, so it refers to all columns
      tableStyle->mCols = NS_STYLE_TABLE_COLS_ALL;
  }

  //background: color
  nsGenericHTMLElement::MapBackgroundAttributesInto(aAttributes, aContext, aPresContext);
  nsGenericHTMLElement::MapCommonAttributesInto(aAttributes, aContext, aPresContext);
}

NS_IMETHODIMP
nsTablePart::GetAttributeMappingFunction(nsMapAttributesFunc& aMapFunc) const
{
  aMapFunc = &MapAttributesInto;
  return NS_OK;
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

