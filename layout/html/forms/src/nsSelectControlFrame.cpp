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
#include "nsCOMPtr.h"
#include "nsFormControlFrame.h"
#include "nsFormFrame.h"
#include "nsIDOMNode.h"
#include "nsIDOMHTMLSelectElement.h"
#include "nsIDOMHTMLOptionElement.h"
#include "nsIDOMHTMLCollection.h"
#include "nsDOMEvent.h"
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
#include "nsIDeviceContext.h"
#include "nsIFontMetrics.h"
#include "nsILookAndFeel.h"
#include "nsIComponentManager.h"

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kListWidgetIID, NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kComboBoxIID, NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kListBoxIID, NS_ILISTBOX_IID);
static NS_DEFINE_IID(kComboCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kListCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kLookAndFeelCID,  NS_LOOKANDFEEL_CID);
static NS_DEFINE_IID(kILookAndFeelIID, NS_ILOOKANDFEEL_IID);

 
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

  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintOption(PRBool aPaintSelected, nsIPresContext& aPresContext,
                 nsIRenderingContext& aRenderingContext, nsString aText, nscoord aX, nscoord aY,
                 const nsRect& aInside, nscoord aTextHeight);

  virtual void PaintSelectControl(nsIPresContext& aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect);

  ///XXX: End o the temporary methods

  virtual void MouseClicked(nsIPresContext* aPresContext);

  // nsIFormControLFrame
  NS_IMETHOD SetProperty(nsIAtom* aName, const nsString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsString& aValue);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint);
protected:
  PRUint32 mNumRows;

  nsIDOMHTMLSelectElement* GetSelect();
  nsIDOMHTMLCollection* GetOptions(nsIDOMHTMLSelectElement* aSelect = nsnull);
  nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  PRBool GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);
  PRUint32 GetSelectedIndex();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
  
  void GetWidgetSize(nsIPresContext& aPresContext, nscoord& aWidth, nscoord& aHeight);

  PRBool mIsComboBox;
  PRBool mOptionsAdded;

  // Store the state of the options in a local array whether the widget is 
  // GFX-rendered or not.  This is used to detect changes in MouseClicked
  PRUint32 mNumOptions;
  PRBool* mOptionSelected;

  // Accessor methods for mOptionsSelected and mNumOptions
  void GetOptionSelected(PRUint32 index, PRBool* aValue);
  void SetOptionSelected(PRUint32 index, PRBool aValue);
  void GetOptionSelectedFromWidget(PRInt32 index, PRBool* aValue);

  // Free our locally cached option array
  ~nsSelectControlFrame();
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
  mNumRows      = 0;
  mNumOptions   = 0;
  mOptionSelected = nsnull;
}

// XXX is this the right way to clean up?
nsSelectControlFrame::~nsSelectControlFrame()
{
  if (mOptionSelected)
    delete[] mOptionSelected;
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
  // XXX NOTE: the enums eMetric_ListVerticalInsidePadding and eMetric_ListShouldUseVerticalInsidePadding
  // are ONLY needed because GTK is not using the "float" padding values and wants to only use an 
  // integer value for the padding instead of calculating like the other platforms.
  //
  // If GTK decides to start calculating the value, PLEASE remove these two enum from nsILookAndFeel and
  // all the platforms nsLookAndFeel impementations so we don't have these extra values remaining in the code.
  // The two enums are:
  //    eMetric_ListVerticalInsidePadding
  //    eMetric_ListShouldUseVerticalInsidePadding
  //
  float   pad;
  PRInt32 padInside;
  PRInt32 shouldUsePadInside;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ListVerticalInsidePadding,  pad);
   // These two (below) are really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListVerticalInsidePadding,  padInside);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListShouldUseVerticalInsidePadding,  shouldUsePadInside);
   NS_RELEASE(lookAndFeel);
  }

  if (1 == shouldUsePadInside) {
    return NSIntPixelsToTwips(padInside, aPixToTwip); // XXX this is probably wrong (GTK)
  } else {
    return (nscoord)NSToIntRound(float(aInnerHeight) * pad);
  }
}

PRInt32 
nsSelectControlFrame::GetHorizontalInsidePadding(nsIPresContext& aPresContext,
                                                 float aPixToTwip, 
                                                 nscoord aInnerWidth,
                                                 nscoord aCharWidth) const
{
  // XXX NOTE: the enum eMetric_ListShouldUseHorizontalInsideMinimumPadding
  // is ONLY needed because GTK is not using the "float" padding values and wants to only use the 
  // "minimum" integer value for the padding instead of calculating and comparing like the other platforms.
  //
  // If GTK decides to start calculating and comparing the values, 
  // PLEASE remove these the enum from nsILookAndFeel and
  // all the platforms nsLookAndFeel impementations so we don't have these extra values remaining in the code.
  // The enum is:
  //    eMetric_ListShouldUseHorizontalInsideMinimumPadding
  //
  float pad;
  PRInt32 padMin;
  PRInt32 shouldUsePadMin;
  nsILookAndFeel * lookAndFeel;
  if (NS_OK == nsComponentManager::CreateInstance(kLookAndFeelCID, nsnull, kILookAndFeelIID, (void**)&lookAndFeel)) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ListHorizontalInsidePadding,  pad);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListHorizontalInsideMinimumPadding,  padMin);
   // This one (below) is really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListShouldUseHorizontalInsideMinimumPadding,  shouldUsePadMin);
   NS_RELEASE(lookAndFeel);
  }

  if (1 == shouldUsePadMin) {
    return NSIntPixelsToTwips(padMin, aPixToTwip); // XXX this is probably wrong (GTK)
  } else {
    nscoord padding = (nscoord)NSToIntRound(float(aCharWidth) * pad);
    nscoord min = NSIntPixelsToTwips(padMin, aPixToTwip);
    if (padding > min) {
      return padding;
    } else {
      return min;
    }
  }
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
nsSelectControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                     const nsHTMLReflowState& aReflowState,
                                     nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                     nsSize&                  aDesiredWidgetSize)
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
      nsFormControlHelper::GetTextSize(*aPresContext, this, text, textSize, aReflowState.rendContext); 
      if (textSize.width > maxWidth) {
        maxWidth = textSize.width;
      }
      NS_RELEASE(option);
    }
  }

  PRInt32 rowHeight = 0;
  nsSize desiredSize;
  nsSize minSize;
  PRBool widthExplicit, heightExplicit;
  nsInputDimensionSpec textSpec(nsnull, PR_FALSE, nsnull, nsnull,
                                maxWidth, PR_TRUE, nsHTMLAtoms::size, 1);
  // XXX fix CalculateSize to return PRUint32
  mNumRows = 
    (PRUint32)nsFormControlHelper::CalculateSize(aPresContext, aReflowState.rendContext, this, styleSize, 
                                                 textSpec, desiredSize, minSize, widthExplicit, 
                                                 heightExplicit, rowHeight);

  // here it is determined whether we are a combo box
  PRInt32 sizeAttr;
  GetSizeFromContent(&sizeAttr);
  PRBool multiple;
  if (!GetMultiple(&multiple) && 
      ((1 >= sizeAttr) || ((ATTR_NOTSET == sizeAttr) && (1 >= mNumRows)))) {
    mIsComboBox = PR_TRUE;
  }

  float sp2t;
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  aPresContext->GetScaledPixelsToTwips(&sp2t);

  nscoord scrollbarWidth  = 0;
  nscoord scrollbarHeight = 0;
  float   scale;
  nsCOMPtr<nsIDeviceContext> dx;
  aPresContext->GetDeviceContext(getter_AddRefs(dx));
  if (dx) {
    float sbWidth;
    float sbHeight;
    dx->GetCanonicalPixelScale(scale);
    dx->GetScrollBarDimensions(sbWidth, sbHeight);
    scrollbarWidth  = PRInt32(sbWidth * scale);
    scrollbarHeight = PRInt32(sbHeight * scale);
  } else {
    scrollbarWidth  = GetScrollbarWidth(sp2t);
    scrollbarHeight = scrollbarWidth;
  }

  aDesiredLayoutSize.width = desiredSize.width;
  // account for vertical scrollbar, if present  
  if (!widthExplicit && ((mNumRows < numOptions) || mIsComboBox)) {
    aDesiredLayoutSize.width += scrollbarWidth;
  }
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width = minSize.width;
    if (!widthExplicit && ((mNumRows < numOptions) || mIsComboBox)) {
      aDesiredLayoutSize.maxElementSize->width += scrollbarWidth;
    }
  }

  // XXX put this in widget library, combo boxes are fixed height (visible part)
  aDesiredLayoutSize.height = (mIsComboBox)
    ? rowHeight + (2 * GetVerticalInsidePadding(p2t, rowHeight))
    : desiredSize.height; 
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->height = (mIsComboBox)
      ? rowHeight + (2 * GetVerticalInsidePadding(p2t, rowHeight))
      : minSize.height; 
  }

  aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
  aDesiredLayoutSize.descent = 0;

  aDesiredWidgetSize.width  = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height = aDesiredLayoutSize.height;
  if (mIsComboBox) {  // add in pull down size
    PRInt32 extra = NSIntPixelsToTwips(10, p2t*scale);
    aDesiredWidgetSize.height += (rowHeight * (numOptions > 20 ? 20 : numOptions)) + extra;
  }

  // override the width and height for a combo box that has already got a widget
  if (mWidget && mIsComboBox) {
    nscoord ignore;
    GetWidgetSize(*aPresContext, ignore, aDesiredLayoutSize.height);
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    if (aDesiredLayoutSize.maxElementSize) {
      aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
    }
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
    float twipToPix;
    aPresContext.GetTwipsToPixels(&twipToPix);
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

PRUint32
nsSelectControlFrame::GetSelectedIndex()
{
  PRInt32 index = -1; // No selection
  nsIDOMHTMLSelectElement* select = nsnull;
  if (NS_SUCCEEDED(mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&select))) {
    select->GetSelectedIndex(&index);
    NS_RELEASE(select);
  }
  return(index); 
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
  float p2t;
  aPresContext.GetPixelsToTwips(&p2t);
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
  nsFont font(aPresContext->GetDefaultFixedFontDeprecated()); 
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

      // Initialize the locally cached numOptions and optionSelected array.
      // The values of the optionsSelected array are filled in by Reset()
      mNumOptions = numOptions;
      if ((numOptions > 0) && (mOptionSelected == nsnull)) {
        mOptionSelected = new PRBool[numOptions];
      }

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
      NS_RELEASE(options);
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

        // Cache the state of each option locally
        option->GetDefaultSelected(&selected);
        SetOptionSelected(i, selected);

        // Store the index of each option in the DOM
        option->SetIndex(i);
        if (selected) {
          listWidget->SelectItem(i);
          if (selectedIndex < 0)
            selectedIndex = i;
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
  if ((NS_OK == aCollection.Item(aIndex, &node)) && node) {
    nsIDOMHTMLOptionElement* option = nsnull;
    node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
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


void
nsSelectControlFrame::PaintOption(PRBool aPaintSelected, nsIPresContext& aPresContext,
                 nsIRenderingContext& aRenderingContext, nsString aText, nscoord aX, nscoord aY,
                 const nsRect& aInside,
                 nscoord aTextHeight)
{
  nscolor foreground =  NS_RGB(0, 0, 0);
  nscolor background =  NS_RGB(255, 255, 255);
  if (PR_TRUE == aPaintSelected) {
    background =  NS_RGB(0, 0, 0);
    foreground =  NS_RGB(255, 255, 255);
  }
 
  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);
  nsRect rect(aInside.x, aY-onePixel, mRect.width-onePixel, aTextHeight+onePixel);
  nscolor currentColor;
  aRenderingContext.GetColor(currentColor);
  aRenderingContext.SetColor(background);
  aRenderingContext.FillRect(rect); 
  aRenderingContext.SetColor(foreground);
  aRenderingContext.DrawString(aText, aX, aY); 
  aRenderingContext.SetColor(currentColor);
}


void
nsSelectControlFrame::PaintSelectControl(nsIPresContext& aPresContext,
                                         nsIRenderingContext& aRenderingContext,
                                         const nsRect& aDirtyRect)
{
  aRenderingContext.PushState();

  /**
   * Resolve style for a pseudo frame within the given aParentContent & aParentContext.
   * The tag should be lowercase and inclue the colon.
   * ie: NS_NewAtom(":first-line");
   */
  nsIAtom * sbAtom = NS_NewAtom(":scrollbar-look");
  nsIStyleContext* scrollbarStyle;
  aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext,
                                            PR_FALSE, &scrollbarStyle);
  NS_RELEASE(sbAtom);

  sbAtom = NS_NewAtom(":scrollbar-arrow-look");
  nsIStyleContext* arrowStyle;
  aPresContext.ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext,
                                            PR_FALSE, &arrowStyle);
  NS_RELEASE(sbAtom);


  nsIDOMHTMLCollection* options = GetOptions();
  if (!options) {
    return;
  }
  PRUint32 numOptions;
  options->GetLength(&numOptions);

  float scale;
  nsIDeviceContext * context;
  aRenderingContext.GetDeviceContext(context);
  context->GetCanonicalPixelScale(scale);

  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsMargin border;
  spacing->CalcBorderFor(this, border);

  float p2t;
  aPresContext.GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  nsRect outside(0, 0, mRect.width, mRect.height);
  outside.Deflate(border);
  outside.Deflate(onePixel, onePixel);

  nsRect inside(outside);

  aRenderingContext.SetColor(NS_RGB(0,0,0));

  nsFont font(aPresContext.GetDefaultFixedFontDeprecated()); 
  GetFont(&aPresContext, font);

  aRenderingContext.SetFont(font);

  //nscoord textWidth;
  nscoord textHeight;
  nsString text;

  // Calculate the height of the text
  nsIFontMetrics* metrics;
  context->GetMetricsFor(font, metrics);
  metrics->GetHeight(textHeight);

  // Calculate the width of the scrollbar
  PRInt32 scrollbarWidth;
  if (numOptions > mNumRows) {
    float sbWidth;
    float sbHeight;
    context->GetCanonicalPixelScale(scale);
    context->GetScrollBarDimensions(sbWidth, sbHeight);
    scrollbarWidth = PRInt32(sbWidth * scale);
  } else {
    scrollbarWidth = 0;
  }

  // shrink the inside rect's width for the scrollbar
  inside.width  -= scrollbarWidth;
  PRBool clipEmpty;
  aRenderingContext.PushState();
  nsRect clipRect(inside); 
  clipRect.Inflate(onePixel, onePixel);

  aRenderingContext.SetClipRect(clipRect, nsClipCombine_kReplace, clipEmpty);

  nscoord x = inside.x + onePixel;
  nscoord y;
  if (mIsComboBox) {
    y = ((inside.height  - textHeight) / 2)  + inside.y;
  } else {
    y = inside.y;
  }

    // Get Selected index out of Content model
  PRUint32 selectedIndex = GetSelectedIndex();
  PRBool multiple = PR_FALSE;
  GetMultiple(&multiple);

  nsIDOMNode* node;
  nsIDOMHTMLOptionElement* option;
  for (PRUint32 i = 0; i < numOptions; i++) {
    options->Item(i, &node);
    if (node) {
      nsresult result = node->QueryInterface(kIDOMHTMLOptionElementIID, (void**)&option);
      if ((NS_OK == result) && option) {
        // XXX need to compress whitespace
        if (NS_CONTENT_ATTR_HAS_VALUE != option->GetText(text)) {
            text = " ";
        }

        if (mIsComboBox) {
            // Paint a non-selected option
          if ((selectedIndex == -1) && (i == 0)) {
            PaintOption(PR_FALSE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
          }
          else if (selectedIndex == i) {
            PaintOption(PR_FALSE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
          }

          if ((-1 == selectedIndex && (0 == i)) || (selectedIndex == i)) {
            i = numOptions;
          }

        } else {
          // Its a list box
          if (!multiple) {
             // Single select list box
            if (selectedIndex == i)
              PaintOption(PR_TRUE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
            else
              PaintOption(PR_FALSE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
          }
          else {
            PRBool selected = PR_FALSE;
            option->GetSelected(&selected);
             // Multi-selection list box
            if (selected) 
              PaintOption(PR_TRUE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
            else
              PaintOption(PR_FALSE, aPresContext, aRenderingContext, text, x, y, inside, textHeight);
          }     
          y += textHeight;
          if (i == mNumRows-1) {
            i = numOptions;
          }   
        }
      }
         
      NS_RELEASE(option);
    }
    NS_RELEASE(node);
  }
 

  aRenderingContext.PopState(clipEmpty);
  // Draw Scrollbars
  if (numOptions > mNumRows) {
    //const nsStyleColor* myColor =
    //  (const nsStyleColor*)mStyleContext->GetStyleData(eStyleStruct_Color);

    if (mIsComboBox) {
      // Get the Scrollbar's Arrow's Style structs
      const nsStyleSpacing* arrowSpacing = (const nsStyleSpacing*)arrowStyle->GetStyleData(eStyleStruct_Spacing);
//XXX      const nsStyleColor*   arrowColor   = (const nsStyleColor*)arrowStyle->GetStyleData(eStyleStruct_Color);

      nsRect srect(mRect.width-scrollbarWidth-onePixel, onePixel, scrollbarWidth, mRect.height-(onePixel*2));
      nsFormControlHelper::PaintArrow(nsFormControlHelper::eArrowDirection_Down, aRenderingContext,aPresContext, 
                      aDirtyRect, srect, onePixel, arrowStyle, *arrowSpacing, this, mRect);
    } else {
      nsRect srect(mRect.width-scrollbarWidth-onePixel, onePixel, scrollbarWidth, mRect.height-(onePixel*2));

      nsFormControlHelper::PaintScrollbar(aRenderingContext,aPresContext, aDirtyRect, srect, PR_FALSE, onePixel, 
                                                                    scrollbarStyle, arrowStyle, this, mRect);   
    }
  }


  NS_RELEASE(context);

  NS_RELEASE(options);
  aRenderingContext.PopState(clipEmpty);

  NS_RELEASE(scrollbarStyle);
  NS_RELEASE(arrowStyle);

}

NS_METHOD 
nsSelectControlFrame::Paint(nsIPresContext& aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  nsFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintSelectControl(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}

void
nsSelectControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
  if ((nsnull != mFormFrame) && !nsFormFrame::GetDisabled(this)) {

    PRBool changed = PR_FALSE;
    PRBool multiple;
    GetMultiple(&multiple);
    if (!multiple) {
      nsIListWidget* listWidget;
      nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
      if ((NS_OK == result) && listWidget) {
        PRInt32 viewIndex = listWidget->GetSelectedIndex();
        NS_RELEASE(listWidget);

        PRBool wasSelected = PR_FALSE;
        GetOptionSelected(viewIndex, &wasSelected);

        if (wasSelected == PR_FALSE) {
          changed = PR_TRUE;
        }
        // Update the locally cached array
        for (PRUint32 i=0; i < mNumOptions; i++)
          SetOptionSelected(i, PR_FALSE);
        SetOptionSelected(viewIndex, PR_TRUE);
        
      }
    } else {
      // Get the selected option from the widget
      nsIListBox* listBox;
      nsresult result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
      if (!(NS_OK == result) || !listBox) {
        return;
      }
      PRUint32 numSelected = listBox->GetSelectedCount();
      PRInt32* selOptions = nsnull;
      if (numSelected >= 0) {
        selOptions = new PRInt32[numSelected];
        listBox->GetSelectedIndices(selOptions, numSelected);
      }
      NS_RELEASE(listBox);

      // XXX Assume options are sorted in selOptions

      PRUint32 selIndex = 0;
      PRUint32 nextSel = 0;
      if ((nsnull != selOptions) && (numSelected > 0)) {
        nextSel = selOptions[selIndex];
      }

      // Step through each option in local cache
      for (PRUint32 i=0; i < mNumOptions; i++) {
        PRBool selectedInLocalCache = PR_FALSE;
        PRBool selectedInView = (i == nextSel);
        GetOptionSelected(i, &selectedInLocalCache);
        if (selectedInView != selectedInLocalCache) {
          changed = PR_TRUE;
          SetOptionSelected(i, selectedInView);

          if (selectedInView) {
            // Get the next selected option in the view
            selIndex++; 
            if (selIndex < numSelected) {
              nextSel = selOptions[selIndex];
            }
          }
        }
      }
      delete[] selOptions;
    }

    if (changed) {
      nsEventStatus status = nsEventStatus_eIgnore;
      nsEvent event;
      event.eventStructType = NS_EVENT;

      event.message = NS_FORM_CHANGE;
      if (nsnull != mContent) {
        mContent->HandleDOMEvent(*aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, status);
      }
    }
  }
}

// Get the selected state from the local cache (not widget)
void nsSelectControlFrame::GetOptionSelected(PRUint32 index, PRBool* aValue)
{
  if (nsnull != mOptionSelected) {
    if (mNumOptions >= index) {
      *aValue = mOptionSelected[index];
      return;
    }
  }
  *aValue = PR_FALSE;
}       

// Get the selected state from the widget
void nsSelectControlFrame::GetOptionSelectedFromWidget(PRInt32 index, PRBool* aValue)
{
  nsIListBox* listBox;
  *aValue = PR_FALSE;
  nsresult result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
  if ((NS_OK == result) && (nsnull != listBox)) {
    PRUint32 numSelected = listBox->GetSelectedCount();
    PRInt32* selOptions = nsnull;
    if (numSelected > 0) {
      // Could we set numSelected to 1 here? (memory, speed optimization)
      selOptions = new PRInt32[numSelected];
      listBox->GetSelectedIndices(selOptions, numSelected);
      PRUint32 i;
      for (i = 0; i < numSelected; i++) {
        if (selOptions[i] == index) {
          *aValue = PR_TRUE;
          break;
        }
      }
      delete[] selOptions;
    }
    NS_RELEASE(listBox);
  }
}

// Update the locally cached selection state (not widget)
void nsSelectControlFrame::SetOptionSelected(PRUint32 index, PRBool aValue)
{
  if (nsnull != mOptionSelected) {
    if (mNumOptions >= index) {
      mOptionSelected[index] = aValue;
    }
  }
}         

NS_IMETHODIMP
nsSelectControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint)
{
  if (nsnull != mWidget) {
    if (nsHTMLAtoms::disabled == aAttribute) {
      mWidget->Enable(!nsFormFrame::GetDisabled(this));
    }
  }
  return NS_OK;
}


NS_IMETHODIMP nsSelectControlFrame::SetProperty(nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.
  } else if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 error = 0;
    PRInt32 selectedIndex = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error) {
      return NS_ERROR_INVALID_ARG; // Couldn't convert to integer
    } else {
      // Update local cache of selected values
      for (PRUint32 i=0; i < mNumOptions; i++)
        SetOptionSelected(i, PR_FALSE);
      SetOptionSelected(selectedIndex, PR_TRUE);

      // Update widget
      nsIListWidget* listWidget;
      nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
      if ((NS_OK == result) && (nsnull != listWidget)) {
        listWidget->Deselect();
        listWidget->SelectItem(selectedIndex);
        NS_RELEASE(listWidget);
      }
    }
  } else {
    return nsFormControlFrame::SetProperty(aName, aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP nsSelectControlFrame::GetProperty(nsIAtom* aName, nsString& aValue)
{
#ifndef XP_MAC
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsHTMLAtoms::selected == aName) {
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 index = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error == 0)
      GetOptionSelectedFromWidget(index, &selected);
    nsFormControlHelper::GetBoolString(selected, aValue);
    
  // For selectedIndex, get the value from the widget
  } else  if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 selectedIndex = -1;
    PRBool multiple;
    GetMultiple(&multiple);
    if (!multiple) {
      nsIListWidget* listWidget;
      nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
      if ((NS_OK == result) && (nsnull != listWidget)) {
        selectedIndex = listWidget->GetSelectedIndex();
        NS_RELEASE(listWidget);
      }
    } else {
      // Listboxes don't do GetSelectedIndex on windows.  Use GetSelectedIndices
      nsIListBox* listBox;
      nsresult result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
      if ((NS_OK == result) && (nsnull != listBox)) {
        PRUint32 numSelected = listBox->GetSelectedCount();
        PRInt32* selOptions = nsnull;
        if (numSelected > 0) {
          // Could we set numSelected to 1 here? (memory, speed optimization)
          selOptions = new PRInt32[numSelected];
          listBox->GetSelectedIndices(selOptions, numSelected);
          selectedIndex = selOptions[0];
          delete[] selOptions;
        }
        NS_RELEASE(listBox);
      }
    }
    aValue.Append(selectedIndex, 10);
  } else {
    return nsFormControlFrame::GetProperty(aName, aValue);
  }
#endif
  return NS_OK;
}
