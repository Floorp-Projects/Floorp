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

#include "nsFormControlFrame.h"
#include "nsFormFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLCollection.h"
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
#include "nsIView.h"
#include "nsIListWidget.h"
#include "nsIComboBox.h"
#include "nsIListBox.h"
#include "nsIStyleContext.h"
#include "nsStyleConsts.h"
#include "nsStyleUtil.h"
#include "nsFont.h"

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kListWidgetIID, NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kComboBoxIID, NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kListBoxIID, NS_ILISTBOX_IID);
static NS_DEFINE_IID(kComboCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kListCID, NS_LISTBOX_CID);

 
class nsOption;

class nsSelectControlFrame : public nsFormControlFrame {
public:
  nsSelectControlFrame();

  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("SelectControl", aResult);
  }

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext& aPresContext);

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual PRInt32 GetMaxNumValues();
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  NS_METHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);
  virtual void Reset();

protected:
  nsIDOMHTMLSelectElement* GetSelect();
  nsIDOMHTMLCollection* GetOptions(nsIDOMHTMLSelectElement* aSelect = nsnull);
  nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  PRBool GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
  
  void GetWidgetSize(nsIPresContext& aPresContext, nscoord& aWidth, nscoord& aHeight);

  PRBool mIsComboBox;
  PRBool mOptionsAdded;
};

nsresult
NS_NewSelectControlFrame(nsIFrame*& aResult)
{
  aResult = new nsSelectControlFrame;
  if (nsnull == aResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

nsSelectControlFrame::nsSelectControlFrame()
  : nsFormControlFrame()
{
  mIsComboBox   = PR_FALSE;
  mOptionsAdded = PR_FALSE;
}

nscoord 
nsSelectControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(1, aPixToTwip);
}

nscoord 
nsSelectControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord 
nsSelectControlFrame::GetVerticalInsidePadding(float aPixToTwip, 
                                               nscoord aInnerHeight) const
{
#ifdef XP_PC
  return (nscoord)NSToIntRound(float(aInnerHeight) * 0.10f);
#endif
#ifdef XP_UNIX
  return NSIntPixelsToTwips(1, aPixToTwip); // XXX this is probably wrong
#endif
}

PRInt32 
nsSelectControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                                 float aPixToTwip, 
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
nsSelectControlFrame::GetIID()
{
  if (mIsComboBox) {
    return kComboBoxIID;
  } else {
    return kListBoxIID;
  }
}
  
const nsIID&
nsSelectControlFrame::GetCID()
{
  if (mIsComboBox) {
    return kComboCID;
  } else {
    return kListCID;
  }
}

void 
nsSelectControlFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                     const nsHTMLReflowState& aReflowState,
                                     nsHTMLReflowMetrics& aDesiredLayoutSize,
                                     nsSize& aDesiredWidgetSize)
{
  nsIDOMHTMLSelectElement* select = GetSelect();
  if (!select) {
    return;
  }
  nsIDOMHTMLCollection* options = GetOptions(select);
  if (!options) {
    NS_RELEASE(select);
    return;
  }

  // get the css size 
  nsSize styleSize;
  GetStyleSize(*aPresContext, aReflowState, styleSize);

  // get the size of the longest option 
  PRInt32 maxWidth = 1;
  PRUint32 numOptions;
  options->GetLength(&numOptions);
  for (PRUint32 i = 0; i < numOptions; i++) {
    nsIDOMHTMLOptionElement* option = GetOption(*options, i);
    if (option) {
      //option->CompressContent();
       nsAutoString text;
      if (NS_CONTENT_ATTR_HAS_VALUE != option->GetText(text)) {
        continue;
      }
      nsSize textSize;
      // use the style for the select rather that the option, since widgets don't support it
      nsFormControlFrame::GetTextSize(*aPresContext, this, text, textSize, aReflowState.rendContext); 
      if (textSize.width > maxWidth) {
        maxWidth = textSize.width;
      }
      NS_RELEASE(option);
    }
  }

  PRInt32 rowHeight = 0;
  nsSize calcSize, charSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, nsnull,
                                maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  // XXX fix CalculateSize to return PRUint32
  PRUint32 numRows = (PRUint32)CalculateSize(aPresContext, this, styleSize, textSpec, 
                                             calcSize, widthExplicit, heightExplicit, rowHeight,
                                             aReflowState.rendContext);

  // here it is determined whether we are a combo box
  PRInt32 sizeAttr;
  GetSize(&sizeAttr);
  PRBool multiple;
  if (!GetMultiple(&multiple) && 
      ((1 >= sizeAttr) || ((ATTR_NOTSET == sizeAttr) && (1 >= numRows)))) {
    mIsComboBox = PR_TRUE;
  }
    
  float p2t = aPresContext->GetPixelsToTwips();

  aDesiredLayoutSize.width = calcSize.width;
  // account for vertical scrollbar, if present  
  if (!widthExplicit && ((numRows < numOptions) || mIsComboBox)) {
    aDesiredLayoutSize.width += GetScrollbarWidth(p2t);
  }

  // XXX put this in widget library, combo boxes are fixed height (visible part)
  aDesiredLayoutSize.height = (mIsComboBox)
    ? rowHeight + (2 * GetVerticalInsidePadding(p2t, rowHeight))
    : calcSize.height; 
  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
  if (mIsComboBox) {  // add in pull down size
    PRInt32 extra = NSIntPixelsToTwips(10, p2t);
    aDesiredWidgetSize.height += (rowHeight * (numOptions > 20 ? 20 : numOptions)) + extra;
  }

  // override the width and height for a combo box that has already got a widget
  if (mWidget && mIsComboBox) {
    nscoord ignore;
    GetWidgetSize(*aPresContext, ignore, aDesiredLayoutSize.height);
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  }

  NS_RELEASE(select);
  NS_RELEASE(options);
}

nsWidgetInitData*
nsSelectControlFrame::GetWidgetInitData(nsIPresContext& aPresContext)
{
  if (mIsComboBox) {
    nsComboBoxInitData* initData = new nsComboBoxInitData();
    initData->clipChildren = PR_TRUE;
    float twipToPix = aPresContext.GetTwipsToPixels();
    initData->mDropDownHeight = NSTwipsToIntPixels(mWidgetSize.height, twipToPix);
    return initData;
  } else {
    PRBool multiple;
    GetMultiple(&multiple);
    nsListBoxInitData* initData = nsnull;
    if (multiple) {
      initData = new nsListBoxInitData();
      initData->clipChildren = PR_TRUE;
      initData->mMultiSelect = PR_TRUE;
    }
    return initData;
  }
}

NS_IMETHODIMP 
nsSelectControlFrame::GetMultiple(PRBool* aMultiple, nsIDOMHTMLSelectElement* aSelect)
{
  if (!aSelect) {
    nsIDOMHTMLSelectElement* select = nsnull;
    nsresult result = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
    if ((NS_OK == result) && select) {
      result = select->GetMultiple(aMultiple);
      NS_RELEASE(select);
    } 
    return result;
  } else {
    return aSelect->GetMultiple(aMultiple);
  }
}

nsIDOMHTMLSelectElement* 
nsSelectControlFrame::GetSelect()
{
  nsIDOMHTMLSelectElement* select = nsnull;
  nsresult result = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select);
  if ((NS_OK == result) && select) {
    return select;
  } else {
    return nsnull;
  }
}

nsIDOMHTMLCollection* 
nsSelectControlFrame::GetOptions(nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsIDOMHTMLSelectElement* select = GetSelect();
    if (select) {
      select->GetOptions(&options);
      NS_RELEASE(select);
      return options;
    } else {
      return nsnull;
    }
  } else {
    aSelect->GetOptions(&options);
    return options;
  }
}

void
nsSelectControlFrame::GetWidgetSize(nsIPresContext& aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  nsRect bounds;
  mWidget->GetBounds(bounds);
  float p2t = aPresContext.GetPixelsToTwips();
  aWidth  = NSIntPixelsToTwips(bounds.width, p2t);
  aHeight = NSTwipsToIntPixels(bounds.height, p2t);
}

void 
nsSelectControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                       nscoord& aWidth,
                                       nscoord& aHeight)
{
  if (!mWidget) {
    return;
  }

  nsIListWidget* listWidget;
  if (NS_OK != mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget)) {
    NS_ASSERTION(PR_FALSE, "invalid widget");
    return;
  }

  mWidget->Enable(!nsFormFrame::GetDisabled(this));
  nsFont font(aPresContext->GetDefaultFixedFont()); 
  GetFont(aPresContext, font);
  mWidget->SetFont(font);
  SetColors(*aPresContext);

  // add the options 
  if (!mOptionsAdded) {
    nsIDOMHTMLCollection* options = GetOptions();
    if (options) {
      PRUint32 numOptions;
      options->GetLength(&numOptions);
      nsIDOMNode* node;
      nsIDOMHTMLOptionElement* option;
      for (PRUint32 i = 0; i < numOptions; i++) {
        options->Item(i, &node);
        if (node) {
          nsresult result = node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
          if ((NS_OK == result) && option) {
            nsString text;
            // XXX need to compress whitespace
            if (NS_CONTENT_ATTR_HAS_VALUE != option->GetText(text)) {
                text = " ";
            }
            listWidget->AddItemAt(text, i);
            NS_RELEASE(option);
          }
          NS_RELEASE(node);
        }
      }
      NS_RELEASE(options);
    }
    mOptionsAdded = PR_TRUE;
  }

  NS_RELEASE(listWidget);

  // get the size of the combo box and let Reflow change its desired size 
  // XXX this technique should be considered for other widgets as well
  if (mIsComboBox) {  
    nscoord ignore;
    nscoord height;
    GetWidgetSize(*aPresContext, ignore, height);
    if (height > aHeight) {
      aHeight = height;
    }
  }

  Reset();  // initializes selections 
}


PRInt32 
nsSelectControlFrame::GetMaxNumValues()
{
  PRBool multiple;
  GetMultiple(&multiple);
  if (multiple) {
    PRUint32 length = 0;
    nsIDOMHTMLCollection* options = GetOptions();
    if (options) {
      options->GetLength(&length);
    }
    return (PRInt32)length; // XXX fix return on GetMaxNumValues
  } else {
    return 1;
  }
}

PRBool
nsSelectControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                     nsString* aValues, nsString* aNames)
{
  aNumValues = 0;
  nsAutoString name;
  nsresult result = GetName(&name);
  if ((aMaxNumValues <= 0) || (NS_CONTENT_ATTR_NOT_THERE == result)) {
    return PR_FALSE;
  }

  nsIDOMHTMLCollection* options = GetOptions();
  if (!options) {
    return PR_FALSE;
  }

  PRBool status = PR_FALSE;
  PRBool multiple;
  GetMultiple(&multiple);
  if (!multiple) {
    nsIListWidget* listWidget;
    result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
    if ((NS_OK == result) && listWidget) {
      PRInt32 index = listWidget->GetSelectedIndex();
      NS_RELEASE(listWidget);
      if (index >= 0) {
        nsAutoString value;
        GetOptionValue(*options, index, value);
        aNumValues = 1;
        aNames[0]  = name;
        aValues[0] = value;
        status = PR_TRUE;
      }
    } 
  } else {
    nsIListBox* listBox;
    nsresult result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
    if ((NS_OK == result) && listBox) {
      PRInt32 numSelections = listBox->GetSelectedCount();
      NS_ASSERTION(aMaxNumValues >= numSelections, "invalid max num values");
      if (numSelections >= 0) {
        PRInt32* selections = new PRInt32[numSelections];
        listBox->GetSelectedIndices(selections, numSelections);
        aNumValues = 0;
        for (int i = 0; i < numSelections; i++) {
          nsAutoString value;
          GetOptionValue(*options, selections[i], value);
          aNames[i]  = name;
          aValues[i] = value;
          aNumValues++;
        }
        delete[] selections;
        status = PR_TRUE;
      }
      NS_RELEASE(listBox);
    }
  }

  NS_RELEASE(options);

  return status;
}


void 
nsSelectControlFrame::Reset() 
{
  nsIDOMHTMLCollection* options = GetOptions();
  if (!options) {
    return;
  }

  PRBool multiple;
  GetMultiple(&multiple);

  PRUint32 numOptions;
  options->GetLength(&numOptions);

  PRInt32 selectedIndex = -1;
  nsIListWidget* listWidget;
  nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
  if ((NS_OK == result) && listWidget) {
    listWidget->Deselect();
    for (PRUint32 i = 0; i < numOptions; i++) {
      nsIDOMHTMLOptionElement* option = GetOption(*options, i);
      if (option) {
        PRBool selected = PR_FALSE;
        option->GetSelected(&selected);
        if (selected) {
          listWidget->SelectItem(i);
          selectedIndex = i;
          if (!multiple) {
            break;  
          }
        }
        NS_RELEASE(option);
      }
    }
  }

  // if none were selected, select 1st one if we are a combo box
  if (mIsComboBox && (numOptions > 0) && (selectedIndex < 0)) {
    listWidget->SelectItem(0);
  }
  NS_RELEASE(listWidget);
  NS_RELEASE(options);
}  



/* XXX add this to nsHTMLOptionElement.cpp
void
nsOption::CompressContent()
{
  if (nsnull != mContent) {
    mContent->CompressWhitespace(PR_TRUE, PR_TRUE);
  }
}*/

nsIDOMHTMLOptionElement* 
nsSelectControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex)
{
  nsIDOMNode* node = nsnull;
  PRBool status = PR_FALSE;
  if ((NS_OK == aCollection.Item(aIndex, &node)) && node) {
    nsIDOMHTMLOptionElement* option = nsnull;
    nsresult result = node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
    NS_RELEASE(node);
    return option;
  }
  return nsnull;
}

PRBool
nsSelectControlFrame::GetOptionValue(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex, nsString& aValue)
{
  PRBool status = PR_FALSE;
  nsIDOMHTMLOptionElement* option = GetOption(aCollection, aIndex);
  if (option) {
    nsresult result = option->GetValue(aValue);
    if (aValue.Length() > 0) {
      status = PR_TRUE;
    } else {
      result = option->GetText(aValue);
      if (aValue.Length() > 0) {
        status = PR_TRUE;
      }
    }
    NS_RELEASE(option);
  }
  return status;
}

