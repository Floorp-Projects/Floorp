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

// YY need to pass isMultiple before create called

#include "nsInputFrame.h"
#include "nsIContent.h"
#include "prtypes.h"
#include "nsIFrame.h"
#include "nsISupports.h"
#include "nsIAtom.h"
#include "nsIPresContext.h"
#include "nsIHTMLContent.h"
#include "nsHTMLIIDs.h"
#include "nsIRadioButton.h"
#include "nsWidgetsCID.h"
#include "nsSize.h"
#include "nsHTMLAtoms.h"
#include "nsIFormManager.h"
#include "nsIView.h"
#include "nsIListWidget.h"
#include "nsIComboBox.h"
#include "nsIListBox.h"
#include "nsInput.h"
#include "nsHTMLForms.h"

static NS_DEFINE_IID(kListWidgetIID, NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kComboBoxIID, NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kListBoxIID, NS_ILISTBOX_IID);
 

class nsSelectFrame : public nsInputFrame {
public:
  nsSelectFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  virtual nsInputWidgetData* GetWidgetInitData();

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  PRBool IsComboBox();

protected:

  virtual ~nsSelectFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

class nsSelect : public nsInput 
{
public:
  typedef nsInput super;
  nsSelect (nsIAtom* aTag, nsIFormManager* aFormMan);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  virtual PRInt32 GetMaxNumValues();
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  PRBool IsMultiple() { return mMultiple; }

  PRBool  IsComboBox();

  virtual void Reset();

protected:
  virtual ~nsSelect();

  virtual void GetType(nsString& aResult) const;

  PRBool mMultiple;
};

class nsOption : public nsInput 
{
public:
  typedef nsInput super;

  nsOption (nsIAtom* aTag);

  virtual nsresult CreateFrame(nsIPresContext*  aPresContext,
                               nsIFrame*        aParentFrame,
                               nsIStyleContext* aStyleContext,
                               nsIFrame*&       aResult);

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const;

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  PRBool GetText(nsString& aString) const;

  void SetText(nsString& aString);

protected:
  virtual ~nsOption();

  virtual void GetType(nsString& aResult) const;

  PRBool mSelected;
  nsString* mText;
};


nsSelectFrame::nsSelectFrame(nsIContent* aContent,
                             nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsSelectFrame::~nsSelectFrame()
{
}

PRBool
nsSelectFrame::IsComboBox()
{
  nsSelect* content = (nsSelect *) mContent;
  return content->IsComboBox();
}

const nsIID&
nsSelectFrame::GetIID()
{
  if (IsComboBox()) {
    return kComboBoxIID;
  }
  else {
    return kListBoxIID;
  }
}
  
const nsIID&
nsSelectFrame::GetCID()
{
  static NS_DEFINE_IID(kComboCID, NS_COMBOBOX_CID);
  static NS_DEFINE_IID(kListCID, NS_LISTBOX_CID);

  if (IsComboBox()) {
    return kComboCID;
  }
  else {
    return kListCID;
  }
}

void 
nsSelectFrame::GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize)
{
  nsSelect* select;
  GetContent((nsIContent *&)select);

  // get the css size 
  nsSize styleSize;
  GetStyleSize(*aPresContext, aMaxSize, styleSize);

  // get the size of the longest child
  PRInt32 maxWidth = 1;
  PRInt32 numChildren = select->ChildCount();
  for (int i = 0; i < numChildren; i++) {
    nsOption* option = (nsOption*) select->ChildAt(i);  // YYY this had better be an option 
    nsString text;
    if (PR_FALSE == option->GetText(text)) {
      continue;
    }
    nsSize textSize;
    // use the style for the select rather that the option, since widgets don't support it
    nsInputFrame::GetTextSize(*aPresContext, this, text, textSize); 
    if (textSize.width > maxWidth) {
      maxWidth = textSize.width;
    }
  }

  PRInt32 rowHeight = 0;
  nsSize calcSize, charSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  PRInt32 numRows = CalculateSize(aPresContext, this, styleSize, textSpec, 
                                  calcSize, widthExplicit, heightExplicit, rowHeight);

  if (!heightExplicit) {
    calcSize.height += 100;
  }

  PRBool isCombo = IsComboBox();
  // account for vertical scrollbar, if present
  aDesiredLayoutSize.width = ((numRows < select->ChildCount()) || isCombo) 
    ? calcSize.width + 350 : calcSize.width + 100;
  aDesiredLayoutSize.height = calcSize.height;
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = 
    (isCombo && !heightExplicit) ? aDesiredLayoutSize.height + (rowHeight * numChildren) + 100 
                                 : aDesiredLayoutSize.height;

  NS_RELEASE(select);
}

nsInputWidgetData*
nsSelectFrame::GetWidgetInitData()
{
  nsInputWidgetData* data = nsnull;
  nsSelect* content;
  GetContent((nsIContent *&) content);
  if (content->IsMultiple()) {
    data = new nsInputWidgetData();
    data->arg1 = 1;
  }
  NS_RELEASE(content);

  return data;
}

void 
nsSelectFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsSelect* select;
  GetContent((nsIContent *&)select);

  nsIView* view;
  GetView(view);

  nsIListWidget* list;
  nsresult stat = view->QueryInterface(kListWidgetIID, (void **) &list);
  NS_ASSERTION((NS_OK == stat), "invalid widget");

  PRBool isCombo = IsComboBox();
  PRInt32 numChildren = select->ChildCount();
  for (int i = 0; i < numChildren; i++) {
    nsOption* option = (nsOption*) select->ChildAt(i);  // YYY this had better be an option
    nsString text;
    if (PR_TRUE != option->GetText(text)) {
      text = " ";
    }
//    if (isCombo) {
//      printf("\n ** text = %s", text.ToNewCString());
//      list->AddItemAt(text, 1);
//    }
    list->AddItemAt(text, i);
  }

  NS_RELEASE(view);

  select->Reset();  // initializes selections 
}

// nsSelect

nsSelect::nsSelect(nsIAtom* aTag, nsIFormManager* aFormMan)
  : nsInput(aTag, aFormMan) 
{
  mMultiple = PR_FALSE;
}

nsSelect::~nsSelect()
{
}

void nsSelect::GetType(nsString& aResult) const
{
  aResult = "select";
}

nsresult
nsSelect::CreateFrame(nsIPresContext* aPresContext,
                      nsIFrame* aParentFrame,
                      nsIStyleContext* aStyleContext,
                      nsIFrame*& aResult)
{
  nsIFrame* frame = new nsSelectFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsSelect::SetAttribute(nsIAtom* aAttribute,
                                const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::multiple) {
    mMultiple = PR_TRUE;
  }
  else {
    super::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsSelect::GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::multiple) {
    return GetCacheAttribute(mMultiple, aResult, eHTMLUnit_Empty);
  }
  else {
    return super::GetAttribute(aAttribute, aResult);
  }
}

PRBool
nsSelect::IsComboBox()
{
//  PRBool  multiple;
//  PRInt32 size;

//  GetAttribute(nsHTMLAtoms::size, size);
//  GetAttribute(nsHTMLAtoms::multiple, multiple);

  PRBool result = (!mMultiple && (mSize <= 1)) ? PR_TRUE : PR_FALSE;
  return result;
}

PRInt32 
nsSelect::GetMaxNumValues()
{
//  PRBool isMultiple;
//  GetAttribute(nsHTMLAtoms::multiple, isMultiple);

  if (mMultiple) {
    return ChildCount();
  }
  else {
    return 1;
  }
}

PRBool
nsSelect::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                         nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    NS_ASSERTION(0, "invalid max num values");
    return PR_FALSE;
  }

  if (!IsMultiple()) {
    NS_ASSERTION(aMaxNumValues > 0, "invalid max num values");
    nsIListWidget* list;
    nsresult stat = mWidget->QueryInterface(kListWidgetIID, (void **) &list);
    NS_ASSERTION((NS_OK == stat), "invalid widget");
    PRInt32 index = list->GetSelectedIndex();
    if (index >= 0) {
      nsOption* selected = (nsOption*)ChildAt(index);
      selected->GetNamesValues(aMaxNumValues, aNumValues, aValues, aNames);
      aNames[0] = *mName;
      return PR_TRUE;
    }
    else {
      aNumValues = 0;
      return PR_FALSE;
    }
  }
  else {
    nsIListBox* list;
    nsresult stat = mWidget->QueryInterface(kListBoxIID, (void **) &list);
    NS_ASSERTION((NS_OK == stat), "invalid widget");
    PRInt32 numSelections = list->GetSelectedCount();
    NS_ASSERTION(aMaxNumValues >= numSelections, "invalid max num values");
    if (numSelections >= 0) {
      PRInt32* selections = new PRInt32[numSelections];
      list->GetSelectedIndices(selections, numSelections);
      PRInt32 numValues;
      aNumValues = 0;
      for (int i = 0; i < numSelections; i++) {
        nsOption* selected = (nsOption*)ChildAt(selections[i]);
        selected->GetNamesValues(aMaxNumValues - i, numValues, 
                                 aValues + i, aNames + i);  // options can only have 1 value
        aNames[i] = *mName;
        aNumValues += 1;
      }
      return PR_TRUE;
    }
    else {
      aNumValues = 0;
      return PR_FALSE;
    }
  }
}


void 
nsSelect::Reset() 
{
//  PRBool allowMultiple;
//  super::GetAttribute(nsHTMLAtoms::multiple, allowMultiple);
  PRBool haveSelection = PR_FALSE;
  PRInt32 numChildren = ChildCount();

  nsIListWidget* list;
  nsresult stat = mWidget->QueryInterface(kListWidgetIID, (void **) &list);
  NS_ASSERTION((NS_OK == stat), "invalid widget");

  list->Deselect();

  for (int i = 0; i < numChildren; i++) {
    nsOption* option = (nsOption*)ChildAt(i);  // YYY this had better be an option 
    PRInt32 selAttr;
    ((nsInput *)option)->GetAttribute(nsHTMLAtoms::selected, selAttr);
    if (ATTR_NOTSET != selAttr) {
      list->SelectItem(i);
      if (!mMultiple) {
        break;  
      }
    }
  }
}  


// nsOption

nsOption::nsOption(nsIAtom* aTag)
  : nsInput(aTag, nsnull) 
{
  mText     = nsnull;
  mSelected = PR_FALSE;
}

nsOption::~nsOption()
{
  if (nsnull != mText) {
    delete mText;
  }
}

void nsOption::GetType(nsString& aResult) const
{
  aResult = "select";
}

nsresult
nsOption::CreateFrame(nsIPresContext* aPresContext,
                      nsIFrame* aParentFrame,
                      nsIStyleContext* aStyleContext,
                      nsIFrame*& aResult)
{
  nsIFrame* frame;
  nsresult rv = nsFrame::NewFrame(&frame, this, aParentFrame);
  if (NS_OK != rv) {
    return rv;
  }
  frame->SetStyleContext(aPresContext, aStyleContext);
  aResult = frame;
  return NS_OK;
}

void nsOption::SetAttribute(nsIAtom* aAttribute,
                            const nsString& aValue)
{
  if (aAttribute == nsHTMLAtoms::selected) {
    mSelected = PR_TRUE;
  }
  else {
    super::SetAttribute(aAttribute, aValue);
  }
}

nsContentAttr nsOption::GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::selected) {
    return GetCacheAttribute(mSelected, aResult, eHTMLUnit_Empty);
  }
  else {
    return super::GetAttribute(aAttribute, aResult);
  }
}

PRInt32 
nsOption::GetMaxNumValues()
{
  return 1;
} 

PRBool nsOption::GetText(nsString& aString) const
{
  if (nsnull == mText) {
    return PR_FALSE;
  } 
  else {
    aString = *mText;
    return PR_TRUE;
  }
}

void nsOption::SetText(nsString& aString)
{
  if (nsnull == mText) {
    mText = new nsString(aString);
  }
  else {
    *mText = aString;
  }
}

PRBool
nsOption::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                         nsString* aValues, nsString* aNames)
{
  if (aMaxNumValues <= 0) {
    NS_ASSERTION(aMaxNumValues > 0, "invalid max num values");
    return PR_FALSE;
  }

  nsString valAttr;
  nsContentAttr stat = super::GetAttribute(nsHTMLAtoms::value, valAttr);
  if (eContentAttr_HasValue == stat) {
    aValues[0] = valAttr;
    aNumValues = 1;
    return PR_TRUE;
  }
  else if (nsnull != mText) {
    aValues[0] = *mText;
    aNumValues = 1;
    return PR_TRUE;
  }
  else {
    aNumValues = 0;
    return PR_FALSE;
  }
}

void HACK(nsSelect* aSel, PRInt32); 

// FACTORY functions

nsresult
NS_NewHTMLSelect(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIFormManager* aFormMan, PRInt32 aHackIndex)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsSelect(aTag, aFormMan);

  if (aHackIndex > 0) {
    HACK((nsSelect*)it, aHackIndex);
  }

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLOption(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsOption(aTag);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

void HACK(nsSelect* aSel, PRInt32 aIndex) 
{
  char buf[100];

  nsIAtom* nameAttr = NS_NewAtom("NAME");
  sprintf(&buf[0], "select %d", aIndex);
  nsString name(&buf[0]);
  aSel->SetAttribute(nameAttr, name);
  nsIAtom* sizeAttr = NS_NewAtom("SIZE");
  int numOpt = 2;
  if (aIndex == 1) {
    nsString size("2");
    aSel->SetAttribute(sizeAttr, size);
  } else if (aIndex == 2) {
    nsString size("4");
    aSel->SetAttribute(sizeAttr, size);
    nsIAtom* multAttr = NS_NewAtom("MULTIPLE");
    nsString mult("1");
    aSel->SetAttribute(multAttr, mult);
    numOpt = 8;
  } else {
    nsString size("1");
    aSel->SetAttribute(sizeAttr, size);
  }

  for (int i = 0; i < numOpt; i++) {
    nsIAtom* atom = NS_NewAtom("OPTION");
    nsOption* option;
    NS_NewHTMLOption((nsIHTMLContent**)&option, atom);
    sprintf(&buf[0], "option %d", i);
    nsString label(&buf[0]);
    option->SetText(label);
    aSel->AppendChild(option);
  }
}




