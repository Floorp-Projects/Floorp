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
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"

#define NS_IOPTION_IID   \
{ 0xfa6a8b11, 0x2af2, 0x11d2, \
  { 0x80, 0x3a, 0x0, 0x60, 0x8, 0x15, 0xa7, 0x91 } }

static NS_DEFINE_IID(kListWidgetIID, NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kComboBoxIID, NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kListBoxIID, NS_ILISTBOX_IID);
static NS_DEFINE_IID(kOptionIID, NS_IOPTION_IID);
 
class nsOption;

class nsSelectFrame : public nsInputFrame {
public:
  nsSelectFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
protected:

  virtual ~nsSelectFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};

class nsSelect : public nsInput 
{
public:
  typedef nsInput nsSelectSuper;
  nsSelect (nsIAtom* aTag, nsIFormManager* aFormMan);

  nsOption* GetNthOption(PRInt32 aIndex);

  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify);

  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aResult) const;

  virtual PRInt32 GetMaxNumValues();
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  PRBool IsMultiple() { return mMultiple; }

  PRBool  GetMultiple() const { return mMultiple; }

  virtual void Reset();

  PRBool mIsComboBox;

protected:
  virtual ~nsSelect();

  virtual void GetType(nsString& aResult) const;

  PRBool mMultiple;
};

class nsOption : public nsInput 
{
public:
  typedef nsInput nsOptionSuper;

  nsOption (nsIAtom* aTag);

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

  NS_IMETHOD SetAttribute(nsIAtom* aAttribute, const nsString& aValue,
                          PRBool aNotify);

  NS_IMETHOD GetAttribute(nsIAtom* aAttribute,
                          nsHTMLValue& aResult) const;

  virtual PRInt32 GetMaxNumValues();

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  virtual PRBool GetContent(nsString& aString) const;

  virtual void SetContent(const nsString& aValue);

  void CompressContent();

protected:
  virtual ~nsOption();

  virtual void GetType(nsString& aResult) const;

  PRBool mSelected;
  nsString* mContent;
};

nsresult
NS_NewHTMLSelectFrame(nsIContent* aContent,
                      nsIFrame*   aParent,
                      nsIFrame*&  aResult)
{
  aResult = new nsSelectFrame(aContent, aParent);
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsSelectFrame::nsSelectFrame(nsIContent* aContent,
                             nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsSelectFrame::~nsSelectFrame()
{
}


nscoord nsSelectFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(1, aPixToTwip);
}

nscoord nsSelectFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord nsSelectFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
#ifdef XP_PC
  return (nscoord)NSToIntRound(float(aInnerHeight) * 0.15f);
#endif
#ifdef XP_UNIX
  return NSIntPixelsToTwips(1, aPixToTwip); // XXX this is probably wrong
#endif
}

PRInt32 nsSelectFrame::GetHorizontalInsidePadding(float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
#ifdef XP_PC
  nscoord padding = (nscoord)NSToIntRound(float(aCharWidth) * 0.40f);
  nscoord min = NSIntPixelsToTwips(3, aPixToTwip);
  if (padding > min) {
    return padding;
  } else {
    return min;
  }
#endif
#ifdef XP_UNIX
  return NSIntPixelsToTwips(7, aPixToTwip); // XXX this is probably wrong
#endif
}

const nsIID&
nsSelectFrame::GetIID()
{
  nsSelect* content = (nsSelect *)mContent;
  if (content->mIsComboBox) {
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

  nsSelect* content = (nsSelect *)mContent;
  if (content->mIsComboBox) {
    return kComboCID;
  }
  else {
    return kListCID;
  }
}

void 
nsSelectFrame::GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize)
{
  nsSelect* select;
  GetContent((nsIContent *&)select);

  // get the css size 
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  // get the size of the longest option child
  PRInt32 maxWidth = 1;
  PRInt32 numChildren;
  select->ChildCount(numChildren);
  for (int childX = 0; childX < numChildren; childX++) {
    nsIContent* child;
    select->ChildAt(childX, child);
    nsOption* option;
    nsresult result = child->QueryInterface(kOptionIID, (void**)&option);
    if (NS_OK == result) {
      option->CompressContent();
      nsString text;
      if (PR_FALSE == option->GetContent(text)) {
        continue;
      }
      nsSize textSize;
      // use the style for the select rather that the option, since widgets don't support it
      nsInputFrame::GetTextSize(*aPresContext, this, text, textSize); 
      if (textSize.width > maxWidth) {
        maxWidth = textSize.width;
      }
      NS_RELEASE(option);
    }
    NS_RELEASE(child);  
  }

  PRInt32 rowHeight = 0;
  nsSize calcSize, charSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, nsnull,
                                maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  PRInt32 numRows = CalculateSize(aPresContext, this, styleSize, textSpec, 
                                  calcSize, widthExplicit, heightExplicit, rowHeight);

  // here it is determined whether we are a combo box
  PRInt32 sizeAttr = select->GetSize();
  if (!select->GetMultiple() && ((1 >= sizeAttr) || ((ATTR_NOTSET == sizeAttr) && (1 >= numRows)))) {
    select->mIsComboBox = PR_TRUE;
  }

  aDesiredLayoutSize.width = calcSize.width;
  // account for vertical scrollbar, if present  
  if (!widthExplicit && ((numRows < numChildren) || select->mIsComboBox)) {
    float p2t = aPresContext->GetPixelsToTwips();
    aDesiredLayoutSize.width += GetScrollbarWidth(p2t);
  }

  // XXX put this in widget library, combo boxes are fixed height (visible part)
  aDesiredLayoutSize.height = (select->mIsComboBox) ? 350 : calcSize.height; 
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
  if (select->mIsComboBox) {  // add in pull down size
    aDesiredWidgetSize.height += (rowHeight * (numChildren > 20 ? 20 : numChildren)) + 100;
  }

  NS_RELEASE(select);
}

nsWidgetInitData*
nsSelectFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  nsSelect* select = (nsSelect *)mContent;
  if (select->mIsComboBox) {
    nsComboBoxInitData* initData = new nsComboBoxInitData();
    initData->clipChildren = PR_TRUE;
    float twipToPix = aPresContext.GetTwipsToPixels();
    initData->mDropDownHeight = NSTwipsToIntPixels(mWidgetSize.height, twipToPix);
    return initData;
  }
  else {
    nsListBoxInitData* initData = nsnull;
    nsSelect* content;
    GetContent((nsIContent *&) content);
    if (content->IsMultiple()) {
      initData = new nsListBoxInitData();
      initData->clipChildren = PR_TRUE;
      initData->mMultiSelect = PR_TRUE;
    }
    NS_RELEASE(content);
    return initData;
  }
}

void 
nsSelectFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsSelect* select;
  GetContent((nsIContent *&)select);

  nsIView* view;
  GetView(view);

  nsIWidget*     widget;
  view->GetWidget(widget);
  nsIListWidget* list;
  if ((nsnull == widget) || NS_FAILED(widget->QueryInterface(kListWidgetIID, (void **) &list))) {
    NS_ASSERTION(PR_FALSE, "invalid widget");
    return;
  }
  widget->SetBackgroundColor(NS_RGB(0xFF, 0xFF, 0xFF));

  const nsStyleFont* styleFont = (const nsStyleFont*)mStyleContext->GetStyleData(eStyleStruct_Font);
  if ((styleFont->mFlags & NS_STYLE_FONT_FACE_EXPLICIT) || 
      (styleFont->mFlags & NS_STYLE_FONT_SIZE_EXPLICIT)) {
    nsFont  widgetFont(styleFont->mFixedFont);
    widgetFont.weight = NS_FONT_WEIGHT_NORMAL;  // always normal weight
    widgetFont.size = styleFont->mFont.size;    // normal font size
    if (0 == (styleFont->mFlags & NS_STYLE_FONT_FACE_EXPLICIT)) {
      widgetFont.name = "Arial";  // XXX windows specific font
    }
    widget->SetFont(widgetFont);
  }
  else {
    // use arial, scaled down one HTML size
    // italics, decoration & variant(?) get used
    nsFont  widgetFont(styleFont->mFont);
    widgetFont.name = "Arial";  // XXX windows specific font
    widgetFont.weight = NS_FONT_WEIGHT_NORMAL; 
    const nsFont& normal = aPresContext->GetDefaultFont();
    PRInt32 scaler = aPresContext->GetFontScaler();
    float scaleFactor = nsStyleUtil::GetScalingFactor(scaler);
    PRInt32 fontIndex = nsStyleUtil::FindNextSmallerFontSize(widgetFont.size, (PRInt32)normal.size, scaleFactor);
    widgetFont.size = nsStyleUtil::CalcFontPointSize(fontIndex, (PRInt32)normal.size, scaleFactor);
    widget->SetFont(widgetFont);
  }

  PRInt32 numChildren;
  select->ChildCount(numChildren);
  int optionX = -1;
  for (int childX = 0; childX < numChildren; childX++) {
    nsIContent* child;
    select->ChildAt(childX, child);
    nsOption* option;
    nsresult result = child->QueryInterface(kOptionIID, (void**)&option);
    if (NS_OK == result) {
      optionX++;
      nsString text;
      if (PR_TRUE != option->GetContent(text)) {
        text = " ";
      }
      list->AddItemAt(text, optionX);
      NS_RELEASE(option);
    }
    NS_RELEASE(child);  
  }

  NS_RELEASE(list);
  NS_RELEASE(widget);

  select->Reset();  // initializes selections 
}

// nsSelect

nsSelect::nsSelect(nsIAtom* aTag, nsIFormManager* aFormMan)
  : nsInput(aTag, aFormMan), mIsComboBox(PR_FALSE) 
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

NS_IMETHODIMP
nsSelect::SetAttribute(nsIAtom* aAttribute,
                       const nsString& aValue,
                       PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::multiple) {
    mMultiple = PR_TRUE;
  }
  return nsSelectSuper::SetAttribute(aAttribute, aValue, aNotify);
}

NS_IMETHODIMP
nsSelect::GetAttribute(nsIAtom* aAttribute,
                       nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::multiple) {
    return GetCacheAttribute(mMultiple, aResult, eHTMLUnit_Empty);
  }
  else {
    return nsSelectSuper::GetAttribute(aAttribute, aResult);
  }
}


PRInt32 
nsSelect::GetMaxNumValues()
{
  if (mMultiple) {
    PRInt32 n;
    ChildCount(n);
    return n;
  }
  else {
    return 1;
  }
}

nsOption* nsSelect::GetNthOption(PRInt32 aIndex)
{
  int optionX = -1;
  PRInt32 numChildren;
  ChildCount(numChildren);
  for (int childX = 0; childX < numChildren; childX++) {
    nsIContent* child;
    ChildAt(childX, child);
    nsOption* option;
    nsresult result = child->QueryInterface(kOptionIID, (void**)&option);
    if (NS_OK == result) {
      optionX++;
      if (aIndex == optionX) {
        NS_RELEASE(child);
        return option;
      }
      NS_RELEASE(option);
    }
    NS_RELEASE(child);  
  }
  return nsnull;
}

PRBool
nsSelect::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                         nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    //NS_ASSERTION(0, "invalid max num values"); // XXX remove this in branch
    return PR_FALSE;
  }

  if (!IsMultiple()) {
    NS_ASSERTION(aMaxNumValues > 0, "invalid max num values");
    nsIListWidget* list;
    nsresult stat = mWidget->QueryInterface(kListWidgetIID, (void **) &list);
    NS_ASSERTION((NS_OK == stat), "invalid widget");
    PRInt32 index = list->GetSelectedIndex();
    NS_RELEASE(list);
    if (index >= 0) {
      nsOption* selected = GetNthOption(index);
      if (selected) {
        selected->GetNamesValues(aMaxNumValues, aNumValues, aValues, aNames);
        aNames[0] = *mName;
        NS_RELEASE(selected); // YYY remove this comment if ok
        return PR_TRUE;
      }
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
      NS_RELEASE(list);
      PRInt32 numValues;
      aNumValues = 0;
      for (int i = 0; i < numSelections; i++) {
        nsOption* selected = GetNthOption(selections[i]);
        if (selected) {
          selected->GetNamesValues(aMaxNumValues - i, numValues, 
                                   aValues + i, aNames + i);  // options can only have 1 value
          aNames[i] = *mName;
          aNumValues += 1;
          NS_RELEASE(selected); // YYY remove this comment if ok
        }
      }
      delete [] selections;
      return PR_TRUE;
    }
    else {
      aNumValues = 0;
      return PR_FALSE;
    }
  }
  aNumValues = 0;
  return PR_FALSE;
}


void 
nsSelect::Reset() 
{
//  PRBool allowMultiple;
//  super::GetAttribute(nsHTMLAtoms::multiple, allowMultiple);
  PRInt32 numChildren;
  ChildCount(numChildren);

  nsIListWidget* list;
  nsresult stat = mWidget->QueryInterface(kListWidgetIID, (void **) &list);
  NS_ASSERTION((NS_OK == stat), "invalid widget");

  list->Deselect();

  PRInt32 selIndex = -1;
  int optionX      = -1;
  for (int childX = 0; childX < numChildren; childX++) {
    nsIContent* child;
    ChildAt(childX, child);
    nsOption* option;
    nsresult result = child->QueryInterface(kOptionIID, (void**)&option);
    if (NS_OK == result) {
      optionX++;
      PRInt32 selAttr;
      ((nsInput *)option)->GetAttribute(nsHTMLAtoms::selected, selAttr);
      if (ATTR_NOTSET != selAttr) {
        list->SelectItem(optionX);
        selIndex = optionX;
        if (!mMultiple) {
          break;  
        }
      }
      NS_RELEASE(option);
    }
    NS_RELEASE(child);  
  }

  // if none were selected, select 1st one if we are a combo box
  if (mIsComboBox && (numChildren > 0) && (selIndex < 0)) {
    list->SelectItem(0);
  }
  NS_RELEASE(list);
}  


// nsOption

nsOption::nsOption(nsIAtom* aTag)
  : nsInput(aTag, nsnull) 
{
  mContent  = nsnull;
  mSelected = PR_FALSE;
}

nsOption::~nsOption()
{
  if (nsnull != mContent) {
    delete mContent;
  }
}

void nsOption::GetType(nsString& aResult) const
{
  aResult = "select";
}

nsresult nsOption::QueryInterface(REFNSIID aIID, void** aInstancePtr)
{
  if (aIID.Equals(kOptionIID)) {
    AddRef();
    *aInstancePtr = (void**) this;
    return NS_OK;
  }
  return nsInput::QueryInterface(aIID, aInstancePtr);
}

NS_IMETHODIMP
nsOption::SetAttribute(nsIAtom* aAttribute,
                       const nsString& aValue,
                       PRBool aNotify)
{
  if (aAttribute == nsHTMLAtoms::selected) {
    mSelected = PR_TRUE;
    // XXX aNotify
    return NS_OK;
  }
  else {
    return nsOptionSuper::SetAttribute(aAttribute, aValue, aNotify);
  }
}

NS_IMETHODIMP
nsOption::GetAttribute(nsIAtom* aAttribute,
                       nsHTMLValue& aResult) const
{
  aResult.Reset();
  if (aAttribute == nsHTMLAtoms::selected) {
    return GetCacheAttribute(mSelected, aResult, eHTMLUnit_Empty);
  }
  else {
    return nsOptionSuper::GetAttribute(aAttribute, aResult);
  }
}

PRInt32 
nsOption::GetMaxNumValues()
{
  return 1;
} 

PRBool nsOption::GetContent(nsString& aString) const
{
  if (nsnull == mContent) {
    aString.SetLength(0);
    return PR_FALSE;
  } 
  else {
    aString = *mContent;
    return PR_TRUE;
  }
}

void nsOption::SetContent(const nsString& aString)
{
  if (nsnull == mContent) {
    mContent = new nsString();
  }
  *mContent = aString;
}

void
nsOption::CompressContent()
{
  if (nsnull != mContent) {
    mContent->CompressWhitespace(PR_TRUE, PR_TRUE);
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
  nsresult stat = nsHTMLTagContent::GetAttribute(nsHTMLAtoms::value, valAttr);
  if (NS_CONTENT_ATTR_HAS_VALUE == stat) {
    aValues[0] = valAttr;
    aNumValues = 1;
    return PR_TRUE;
  }
  else if (nsnull != mContent) {
    aValues[0] = *mContent;
    aNumValues = 1;
    return PR_TRUE;
  }
  else {
    aNumValues = 0;
    return PR_FALSE;
  }
}

// FACTORY functions

nsresult
NS_NewHTMLSelect(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIFormManager* aFormMan)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsSelect(aTag, aFormMan);
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
