/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 */

// YY need to pass isMultiple before create called
#include "nsCOMPtr.h"
#include "nsNativeFormControlFrame.h"
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
#include "nsISelectControlFrame.h"
#include "nsIStatefulFrame.h"
#include "nsISupportsArray.h"
#include "nsISupportsPrimitives.h"
#include "nsIComponentManager.h"
#include "nsIPresState.h"

static NS_DEFINE_IID(kIDOMHTMLSelectElementIID, NS_IDOMHTMLSELECTELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLOptionElementIID, NS_IDOMHTMLOPTIONELEMENT_IID);
static NS_DEFINE_IID(kIDOMHTMLCollectionIID, NS_IDOMHTMLCOLLECTION_IID);
static NS_DEFINE_IID(kIDOMNodeIID, NS_IDOMNODE_IID);
static NS_DEFINE_IID(kListWidgetIID, NS_ILISTWIDGET_IID);
static NS_DEFINE_IID(kComboBoxIID, NS_ICOMBOBOX_IID);
static NS_DEFINE_IID(kListBoxIID, NS_ILISTBOX_IID);
static NS_DEFINE_IID(kComboCID, NS_COMBOBOX_CID);
static NS_DEFINE_IID(kListCID, NS_LISTBOX_CID);
static NS_DEFINE_IID(kISelectControlFrameIID, NS_ISELECTCONTROLFRAME_IID);
 
class nsOption;

class nsNativeSelectControlFrame : public nsNativeFormControlFrame,
                                   public nsISelectControlFrame,
				   public nsIStatefulFrame
{
public:
  nsNativeSelectControlFrame();

  NS_IMETHOD QueryInterface(const nsIID& aIID, void** aInstancePtr);

#ifdef DEBUG
  NS_IMETHOD GetFrameName(nsString& aResult) const {
    return MakeFrameName("SelectControl", aResult);
  }
#endif

  virtual nsWidgetInitData* GetWidgetInitData(nsIPresContext* aPresContext);

  virtual void PostCreateWidget(nsIPresContext* aPresContext,
                                nscoord& aWidth,
                                nscoord& aHeight);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  virtual nscoord GetVerticalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetHorizontalBorderWidth(float aPixToTwip) const;
  virtual nscoord GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                           float aPixToTwip,
                                           nscoord aInnerHeight) const;
  virtual nscoord GetHorizontalInsidePadding(nsIPresContext* aPresContext,
                                             float aPixToTwip, 
                                             nscoord aInnerWidth,
                                             nscoord aCharWidth) const;
  virtual PRInt32 GetMaxNumValues();
  
  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                                nsString* aValues, nsString* aNames);

  NS_METHOD GetMultiple(PRBool* aResult, nsIDOMHTMLSelectElement* aSelect = nsnull);
  virtual void Reset(nsIPresContext* aPresContext);

  NS_IMETHOD  AppendFrames(nsIPresContext* aPresContext,
                           nsIPresShell&   aPresShell,
                           nsIAtom*        aListName,
                           nsIFrame*       aFrameList);

  //
  // XXX: The following paint methods are TEMPORARY. It is being used to get printing working
  // under windows. Later it may be used to GFX-render the controls to the display. 
  // Expect this code to repackaged and moved to a new location in the future.
  //
  NS_IMETHOD Paint(nsIPresContext* aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  virtual void PaintOption(PRBool aPaintSelected, nsIPresContext* aPresContext,
                 nsIRenderingContext& aRenderingContext, nsString aText, nscoord aX, nscoord aY,
                 const nsRect& aInside, nscoord aTextHeight);

  virtual void PaintSelectControl(nsIPresContext* aPresContext,
                                  nsIRenderingContext& aRenderingContext,
                                  const nsRect& aDirtyRect);

  ///XXX: End o the temporary methods

  virtual void MouseClicked(nsIPresContext* aPresContext);
  virtual void ControlChanged(nsIPresContext* aPresContext);

  // nsIFormControLFrame
  NS_IMETHOD SetProperty(nsIPresContext* aPresContext, nsIAtom* aName, const nsAReadableString& aValue);
  NS_IMETHOD GetProperty(nsIAtom* aName, nsAWritableString& aValue);

  NS_IMETHOD AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     PRInt32         aNameSpaceID,
                                     nsIAtom*        aAttribute,
                                     PRInt32         aHint);

  // nsISelectControlFrame
  NS_IMETHOD AddOption(nsIPresContext* aPresContext, PRInt32 aIndex);
  NS_IMETHOD RemoveOption(nsIPresContext* aPresContext, PRInt32 aIndex);
  NS_IMETHOD SetOptionSelected(PRInt32 aIndex, PRBool aValue);
  NS_IMETHOD GetOptionSelected(PRInt32 aIndex, PRBool* aValue);
  NS_IMETHOD DoneAddingContent(PRBool aIsDone);

  //nsIStatefulFrame
  NS_IMETHOD GetStateType(nsIPresContext* aPresContext, nsIStatefulFrame::StateType* aStateType);
  NS_IMETHOD SaveState(nsIPresContext* aPresContext, nsIPresState** aState);
  NS_IMETHOD RestoreState(nsIPresContext* aPresContext, nsIPresState* aState);

protected:
  PRInt32 mNumRows;

  nsIDOMHTMLSelectElement* GetSelect();
  nsIDOMHTMLCollection* GetOptions(nsIDOMHTMLSelectElement* aSelect = nsnull);
  nsIDOMHTMLOptionElement* GetOption(nsIDOMHTMLCollection& aOptions, PRUint32 aIndex);
  PRBool GetOptionValue(nsIDOMHTMLCollection& aCollecton, PRUint32 aIndex, nsString& aValue);
  PRInt32 GetSelectedIndex();
  nsresult UpdateWidgetToCache(PRBool aDeselectFirst = PR_TRUE);
  nsresult Deselect();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize)
 				{ nsFormControlFrame::GetDesiredSize(aPresContext, aReflowState, aDesiredSize); }
  
  void GetWidgetSize(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight);

  PRBool mIsComboBox;
  PRBool mOptionsAdded;

  // Store the state of the options in a local array whether the widget is 
  // GFX-rendered or not.  This is used to detect changes in MouseClicked
  PRInt32 mNumOptions;
  PRBool* mOptionSelected;
  PRBool mCachedState; // A flag meaning "Don't reset state on PostCreateWidget"

  // Accessor methods for mOptionsSelected and mNumOptions
  void GetOptionSelectedCache(PRInt32 index, PRBool* aValue);
  void SetOptionSelectedCache(PRInt32 index, PRBool aValue);
  void GetOptionSelectedWidget(PRInt32 index, PRBool* aValue);
  nsresult GetOptionText(nsIDOMHTMLCollection*& aOptions,
                              PRUint32 aIndex,
                              nsString& aText);

  // Free our locally cached option array
  ~nsNativeSelectControlFrame();

private:
  NS_IMETHOD_(nsrefcnt) AddRef() { return NS_OK; }
  NS_IMETHOD_(nsrefcnt) Release() { return NS_OK; }
};

nsresult
NS_NewNativeSelectControlFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsNativeSelectControlFrame* it = new (aPresShell) nsNativeSelectControlFrame;
  if (!it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

nsNativeSelectControlFrame::nsNativeSelectControlFrame()
  : nsNativeFormControlFrame()
{
  mIsComboBox   = PR_FALSE;
  mOptionsAdded = PR_FALSE;
  mNumRows      = 0;
  mNumOptions   = 0;
  mOptionSelected = nsnull;
  mCachedState  = PR_FALSE;
}

// XXX is this the right way to clean up?
nsNativeSelectControlFrame::~nsNativeSelectControlFrame()
{
  if (mOptionSelected)
    delete[] mOptionSelected;
}

// Frames are not refcounted, no need to AddRef
NS_IMETHODIMP
nsNativeSelectControlFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_PRECONDITION(0 != aInstancePtr, "null ptr");
  if (NULL == aInstancePtr) {
    return NS_ERROR_NULL_POINTER;
  } else if (aIID.Equals(NS_GET_IID(nsISelectControlFrame))) {
    *aInstancePtr = (void*)(nsISelectControlFrame*) this;
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIStatefulFrame))) {
    *aInstancePtr = (void*)(nsIStatefulFrame*) this;
    return NS_OK;
  }

  return nsNativeFormControlFrame::QueryInterface(aIID, aInstancePtr);
}

nscoord 
nsNativeSelectControlFrame::GetVerticalBorderWidth(float aPixToTwip) const
{
   return NSIntPixelsToTwips(1, aPixToTwip);
}

nscoord 
nsNativeSelectControlFrame::GetHorizontalBorderWidth(float aPixToTwip) const
{
  return GetVerticalBorderWidth(aPixToTwip);
}

nscoord 
nsNativeSelectControlFrame::GetVerticalInsidePadding(nsIPresContext* aPresContext,
                                                     float aPixToTwip, 
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
  nsCOMPtr<nsILookAndFeel> lookAndFeel;
  if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(getter_AddRefs(lookAndFeel)))) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ListVerticalInsidePadding,  pad);
   // These two (below) are really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListVerticalInsidePadding,  padInside);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListShouldUseVerticalInsidePadding,  shouldUsePadInside);
  }

  if (1 == shouldUsePadInside) {
    return NSIntPixelsToTwips(padInside, aPixToTwip); // XXX this is probably wrong (GTK)
  } else {
    return (nscoord)NSToIntRound(float(aInnerHeight) * pad);
  }
}

PRInt32 
nsNativeSelectControlFrame::GetHorizontalInsidePadding(nsIPresContext* aPresContext,
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
  nsCOMPtr<nsILookAndFeel> lookAndFeel;
  if (NS_SUCCEEDED(aPresContext->GetLookAndFeel(getter_AddRefs(lookAndFeel)))) {
   lookAndFeel->GetMetric(nsILookAndFeel::eMetricFloat_ListHorizontalInsidePadding,  pad);
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListHorizontalInsideMinimumPadding,  padMin);
   // This one (below) is really only needed for GTK
   lookAndFeel->GetMetric(nsILookAndFeel::eMetric_ListShouldUseHorizontalInsideMinimumPadding,  shouldUsePadMin);
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

/*
 * FIXME: this ::GetIID() method has no meaning in life and should be
 * removed.
 * Pierre Phaneuf <pp@ludusdesign.com>
 */
const nsIID&
nsNativeSelectControlFrame::GetIID()
{
  if (mIsComboBox) {
    return kComboBoxIID;
  } else {
    return kListBoxIID;
  }
}
  
const nsIID&
nsNativeSelectControlFrame::GetCID()
{
  if (mIsComboBox) {
    return kComboCID;
  } else {
    return kListCID;
  }
}

void 
nsNativeSelectControlFrame::GetDesiredSize(nsIPresContext*          aPresContext,
                                           const nsHTMLReflowState& aReflowState,
                                           nsHTMLReflowMetrics&     aDesiredLayoutSize,
                                           nsSize&                  aDesiredWidgetSize)
{
  nsIDOMHTMLCollection* options = GetOptions();
  if (!options)
    return;

  // get the css size 
  nsSize styleSize;
  GetStyleSize(aPresContext, aReflowState, styleSize);

  // get the size of the longest option 
  PRInt32 maxWidth = 1;
  PRUint32 numOptions;
  options->GetLength(&numOptions);
  
  nsString text;
  for (PRInt32 i = 0; i < (PRInt32)numOptions; i++) {
    nsresult res = GetOptionText(options, i, text);
    if (NS_CONTENT_ATTR_HAS_VALUE != res) {
      continue;
    }
    nsSize textSize;
    // use the style for the select rather that the option, since widgets don't support it
    nsFormControlHelper::GetTextSize(aPresContext, this, text, textSize, aReflowState.rendContext); 
    if (textSize.width > maxWidth) {
      maxWidth = textSize.width;
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
  GetMultiple(&multiple);
  if (!multiple && 
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
  PRBool needScrollbar;
#ifdef XP_MAC
  needScrollbar = PR_TRUE;
  scrollbarWidth = (mIsComboBox)
    ? NSIntPixelsToTwips(25, p2t*scale)
    : (scrollbarWidth + NSIntPixelsToTwips(2, p2t*scale));
#else
  needScrollbar = ((mNumRows < (PRInt32)numOptions) || mIsComboBox);
#endif
  // account for vertical scrollbar, if present  
  if (!widthExplicit && needScrollbar) {
    aDesiredLayoutSize.width += scrollbarWidth;
  }
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->width = minSize.width;
    if (!widthExplicit && needScrollbar) {
      aDesiredLayoutSize.maxElementSize->width += scrollbarWidth;
    }
  }

  // XXX put this in widget library, combo boxes are fixed height (visible part)
  aDesiredLayoutSize.height = (mIsComboBox)
    ? rowHeight + (2 * GetVerticalInsidePadding(aPresContext, p2t, rowHeight))
    : desiredSize.height; 
  if (aDesiredLayoutSize.maxElementSize) {
    aDesiredLayoutSize.maxElementSize->height = (mIsComboBox)
      ? rowHeight + (2 * GetVerticalInsidePadding(aPresContext, p2t, rowHeight))
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
    GetWidgetSize(aPresContext, ignore, aDesiredLayoutSize.height);
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    if (aDesiredLayoutSize.maxElementSize) {
      aDesiredLayoutSize.maxElementSize->height = aDesiredLayoutSize.height;
    }
  }

  NS_RELEASE(options);
}

nsWidgetInitData*
nsNativeSelectControlFrame::GetWidgetInitData(nsIPresContext* aPresContext)
{
  if (mIsComboBox) {
    nsComboBoxInitData* initData = new nsComboBoxInitData();
    if (initData) {
      initData->clipChildren = PR_TRUE;
      float twipToPix;
      aPresContext->GetTwipsToPixels(&twipToPix);
      initData->mDropDownHeight = NSTwipsToIntPixels(mWidgetSize.height, twipToPix);
    }
    return initData;
  } else {
    PRBool multiple;
    GetMultiple(&multiple);
    nsListBoxInitData* initData = nsnull;
    if (multiple) {
      initData = new nsListBoxInitData();
      if (initData) {
        initData->clipChildren = PR_TRUE;
        initData->mMultiSelect = PR_TRUE;
      }
    }
    return initData;
  }
}

NS_IMETHODIMP 
nsNativeSelectControlFrame::GetMultiple(PRBool* aMultiple, nsIDOMHTMLSelectElement* aSelect)
{
  if (!aSelect) {
    nsIDOMHTMLSelectElement* thisSelect = nsnull;
    nsresult result = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&thisSelect);
    if ((NS_OK == result) && thisSelect) {
      result = thisSelect->GetMultiple(aMultiple);
      NS_RELEASE(thisSelect);
    } 
    return result;
  } else {
    return aSelect->GetMultiple(aMultiple);
  }
}

nsIDOMHTMLSelectElement* 
nsNativeSelectControlFrame::GetSelect()
{
  nsIDOMHTMLSelectElement* thisSelect = nsnull;
  nsresult result = mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&thisSelect);
  if ((NS_OK == result) && thisSelect) {
    return thisSelect;
  } else {
    return nsnull;
  }
}

PRInt32
nsNativeSelectControlFrame::GetSelectedIndex()
{
  PRInt32 indx = -1; // No selection
  nsIDOMHTMLSelectElement* thisSelect = nsnull;
  if (NS_SUCCEEDED(mContent->QueryInterface(kIDOMHTMLSelectElementIID, (void**)&thisSelect))) {
    thisSelect->GetSelectedIndex(&indx);
    NS_RELEASE(thisSelect);
  }
  return(indx); 
}


nsIDOMHTMLCollection* 
nsNativeSelectControlFrame::GetOptions(nsIDOMHTMLSelectElement* aSelect)
{
  nsIDOMHTMLCollection* options = nsnull;
  if (!aSelect) {
    nsIDOMHTMLSelectElement* thisSelect = GetSelect();
    if (thisSelect) {
      thisSelect->GetOptions(&options);
      NS_RELEASE(thisSelect);
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
nsNativeSelectControlFrame::GetWidgetSize(nsIPresContext* aPresContext, nscoord& aWidth, nscoord& aHeight)
{
  nsRect bounds;
  mWidget->GetBounds(bounds);
  float p2t;
  aPresContext->GetPixelsToTwips(&p2t);
  aWidth  = NSIntPixelsToTwips(bounds.width, p2t);
  aHeight = NSTwipsToIntPixels(bounds.height, p2t);
}


void
nsNativeSelectControlFrame::PostCreateWidget(nsIPresContext* aPresContext,
                                       nscoord& aWidth,
                                       nscoord& aHeight)
{
  if (!mWidget) {
    NS_ASSERTION(PR_FALSE, "no widget");
    return; // XXX NS_ERROR_UNEXPECTED;
  }
  
  mWidget->Enable(!nsFormFrame::GetDisabled(this));
  const nsFont * font;
  nsresult res = GetFont(aPresContext, font);
  if (NS_SUCCEEDED(res) && font != nsnull) {
    mWidget->SetFont(*font);
  }
  SetColors(aPresContext);

  // add the options 
  if (!mOptionsAdded) {
    nsIListWidget* listWidget;
    // Grab the widget (we need to update it)
    nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
    if ((NS_FAILED(result)) || (nsnull == listWidget)) {
      return; // XXX NS_ERROR_UNEXPECTED;
    }

    nsIDOMHTMLCollection* options = GetOptions();
    if (options) {
      PRUint32 numOptions;
      options->GetLength(&numOptions);

      // Create the locally cached numOptions and optionSelected array.
      // The values of the mOptionsSelected array are filled in by Reset()
      mNumOptions = numOptions;
      if ((numOptions > 0) && (mOptionSelected == nsnull)) {
        mOptionSelected = new PRBool[numOptions];
	if (!mOptionSelected) {
	  return; // XXX NS_ERROR_OUT_OF_MEMORY;
	}
      }

      for (PRUint32 i = 0; i < numOptions; i++) {
        nsString text(" ");
        GetOptionText(options,i,text);
        listWidget->AddItemAt(text,i);
      }
    }

    NS_RELEASE(listWidget);
    mOptionsAdded = PR_TRUE;
  }

  // get the size of the combo box and let Reflow change its desired size 
  // XXX this technique should be considered for other widgets as well
  if (mIsComboBox) {  
    nscoord ignore;
    nscoord height;
    GetWidgetSize(aPresContext, ignore, height);
    if (height > aHeight) {
      aHeight = height;
    }
  }

  Reset(aPresContext);  // initializes selections
}

nsresult
nsNativeSelectControlFrame::GetOptionText(nsIDOMHTMLCollection*& aOptions,
                                          PRUint32 aIndex,
                                          nsString& aText)
{
  nsresult res = NS_OK;

  // Get the correct text of the option
  nsIDOMHTMLOptionElement* option = GetOption(*aOptions, aIndex);
  if (option) {
    // XXX need to compress whitespace
    // option->CompressContent();
    res = option->GetText(aText);
    if (NS_CONTENT_ATTR_HAS_VALUE != res) {
      aText = " "; // needed?
    }
    NS_RELEASE(option);
  } else {
    res = NS_ERROR_UNEXPECTED;
    aText = " ";
  }
  return res;
}


PRInt32 
nsNativeSelectControlFrame::GetMaxNumValues()
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
nsNativeSelectControlFrame::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
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
      PRInt32 indx = listWidget->GetSelectedIndex();
      NS_RELEASE(listWidget);
      if (indx >= 0) {
        nsAutoString value;
        GetOptionValue(*options, indx, value);
        aNumValues = 1;
        aNames[0]  = name;
        aValues[0] = value;
        status = PR_TRUE;
      }
    } 
  } else {
    nsIListBox* listBox;
    result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
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
nsNativeSelectControlFrame::Reset(nsIPresContext* aPresContext) 
{
  if (mCachedState) {
    mCachedState = PR_FALSE;
  } else {
    // Reset selection to default
    nsIDOMHTMLCollection* options = GetOptions();
    if (!options) return; // XXX NS_ERROR_UNEXPECTED;
    PRUint32 numOptions;
    options->GetLength(&numOptions);
    for (PRUint32 i = 0; i < numOptions; i++) {
      nsIDOMHTMLOptionElement* option = GetOption(*options, i);
      if (option) {
        // Cache the state of each option locally
        PRBool selected = PR_FALSE;
        option->GetDefaultSelected(&selected);
        SetOptionSelectedCache(i, selected);
	NS_RELEASE(option);
      }
    }
    NS_RELEASE(options);
  }

  UpdateWidgetToCache();
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
nsNativeSelectControlFrame::GetOption(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex)
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
nsNativeSelectControlFrame::GetOptionValue(nsIDOMHTMLCollection& aCollection, PRUint32 aIndex, nsString& aValue)
{
  PRBool status = PR_FALSE;
  nsIDOMHTMLOptionElement* option = GetOption(aCollection, aIndex);
  if (option) {
    nsIHTMLContent* content = nsnull;
    nsresult result = option->QueryInterface(kIHTMLContentIID, (void **)&content);
    if (NS_SUCCEEDED(result) && content) {
      nsHTMLValue value(aValue);
      result = content->GetHTMLAttribute(nsHTMLAtoms::value, value);
      if (NS_CONTENT_ATTR_HAS_VALUE == result) {
        value.GetStringValue(aValue);
        status = PR_TRUE;
      }
      NS_RELEASE(content);
    }
    if (!status) {
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
nsNativeSelectControlFrame::PaintOption(PRBool aPaintSelected, nsIPresContext* aPresContext,
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
  aPresContext->GetScaledPixelsToTwips(&p2t);
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
nsNativeSelectControlFrame::PaintSelectControl(nsIPresContext* aPresContext,
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
  aPresContext->ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext,
                                            PR_FALSE, &scrollbarStyle);
  NS_RELEASE(sbAtom);

  sbAtom = NS_NewAtom(":scrollbar-arrow-look");
  nsIStyleContext* arrowStyle;
  aPresContext->ResolvePseudoStyleContextFor(mContent, sbAtom, mStyleContext,
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
  aPresContext->GetScaledPixelsToTwips(&p2t);
  nscoord onePixel = NSIntPixelsToTwips(1, p2t);

  nsRect outside(0, 0, mRect.width, mRect.height);
  outside.Deflate(border);
  outside.Deflate(onePixel, onePixel);

  nsRect inside(outside);

  aRenderingContext.SetColor(NS_RGB(0,0,0));

  const nsFont * font = nsnull;
  nsresult res = GetFont(aPresContext, font);
  if (NS_SUCCEEDED(res) && font != nsnull) {
    mWidget->SetFont(*font);
    aRenderingContext.SetFont(*font);
  }

  //nscoord textWidth;
  nscoord textHeight;
  nsString text;

  // Calculate the height of the text
  nsIFontMetrics* metrics;
  if (font != nsnull) {
    context->GetMetricsFor(*font, metrics);
  }
  metrics->GetHeight(textHeight);

  // Calculate the width of the scrollbar
  PRInt32 scrollbarWidth;
  if ((PRInt32)numOptions > mNumRows) {
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
  PRInt32 selectedIndex = GetSelectedIndex();
  PRBool multiple = PR_FALSE;
  GetMultiple(&multiple);

  for (PRInt32 i = 0; i < (PRInt32)numOptions; i++) {
    if (NS_SUCCEEDED(GetOptionText(options,i,text))) {
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
        } else {
          PRBool selected = PR_FALSE;
          GetOptionSelectedCache(i, &selected);
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
  }

  aRenderingContext.PopState(clipEmpty);
  // Draw Scrollbars
  if ((PRInt32)numOptions > mNumRows) {
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
nsNativeSelectControlFrame::Paint(nsIPresContext* aPresContext,
                            nsIRenderingContext& aRenderingContext,
                            const nsRect& aDirtyRect,
                            nsFramePaintLayer aWhichLayer)
{
  nsNativeFormControlFrame::Paint(aPresContext, aRenderingContext, aDirtyRect,
                            aWhichLayer);
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    PaintSelectControl(aPresContext, aRenderingContext, aDirtyRect);
  }
  return NS_OK;
}

// Forward this on as a control changed event - this enables onChange for
// list boxes as ControlChanged is currently only sent for combos
void
nsNativeSelectControlFrame::MouseClicked(nsIPresContext* aPresContext)
{
  ControlChanged(aPresContext);
}

// Update the locally cached selection array.
// If different option(s) are selected, send a DOM onChange event.
// This routine is basically "UpdateCacheToWidget()"
void
nsNativeSelectControlFrame::ControlChanged(nsIPresContext* aPresContext)
{
  if (!nsFormFrame::GetDisabled(this)) {

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
        GetOptionSelectedCache(viewIndex, &wasSelected);

        if (wasSelected == PR_FALSE) {
          changed = PR_TRUE;
        }
        // Update the locally cached array
        for (PRInt32 i=0; i < mNumOptions; i++)
          SetOptionSelectedCache(i, PR_FALSE);
        SetOptionSelectedCache(viewIndex, PR_TRUE);
        
      }
    } else {
      // Get the selected option from the widget
      nsIListBox* listBox;
      nsresult result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
      if (!(NS_OK == result) || !listBox) {
        return;
      }
      PRInt32 numSelected = listBox->GetSelectedCount();
      PRInt32* selOptions = nsnull;
      if (numSelected > 0) {
        selOptions = new PRInt32[numSelected];
        listBox->GetSelectedIndices(selOptions, numSelected);
      }
      NS_RELEASE(listBox);

      // XXX Assume options are sorted in selOptions

      PRInt32 selIndex = 0;
      PRInt32 nextSel = 0;
      if ((nsnull != selOptions) && (numSelected > 0)) {
        nextSel = selOptions[selIndex];
      }

      // Step through each option in local cache
      for (PRInt32 i=0; i < mNumOptions; i++) {
        PRBool selectedInLocalCache = PR_FALSE;
        PRBool selectedInView = (i == nextSel);
        GetOptionSelectedCache(i, &selectedInLocalCache);
        if (selectedInView != selectedInLocalCache) {
          changed = PR_TRUE;
          SetOptionSelectedCache(i, selectedInView);

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
        mContent->HandleDOMEvent(aPresContext, &event, nsnull, NS_EVENT_FLAG_INIT, &status);
      }
    }
  }
}

NS_IMETHODIMP
nsNativeSelectControlFrame::DoneAddingContent(PRBool aIsDone)
{
  return NS_OK;
}

// Get the selected state from the local cache
void nsNativeSelectControlFrame::GetOptionSelectedCache(PRInt32 indx, PRBool* aValue)
{
  if (nsnull != mOptionSelected) {
    if (mNumOptions >= indx) {
      *aValue = mOptionSelected[indx];
      return;
    }
  }
  *aValue = PR_FALSE;
}       

// Get the selected state from the widget
void nsNativeSelectControlFrame::GetOptionSelectedWidget(PRInt32 indx, PRBool* aValue)
{
  *aValue = PR_FALSE;
  nsresult result;
  PRBool multiple;

  GetMultiple(&multiple);
  if (!multiple) {
    nsIListWidget* listWidget;
    result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
    
    if ((NS_OK == result) && (nsnull != listWidget)) {
      PRInt32 selIndex = listWidget->GetSelectedIndex();
      NS_RELEASE(listWidget);

      if (selIndex == indx) {
        *aValue = PR_TRUE;
      }
    }
  }
  else {
    nsIListBox* listBox;
    result = mWidget->QueryInterface(kListBoxIID, (void **) &listBox);
    if ((NS_OK == result) && (nsnull != listBox)) {
      PRUint32 numSelected = listBox->GetSelectedCount();
      PRInt32* selOptions = nsnull;
      if (numSelected > 0) {
        // Could we set numSelected to 1 here? (memory, speed optimization)
        selOptions = new PRInt32[numSelected];
        listBox->GetSelectedIndices(selOptions, numSelected);
        PRUint32 i;
        for (i = 0; i < numSelected; i++) {
          if (selOptions[i] == indx) {
            *aValue = PR_TRUE;
            break;
          }
        }
        delete[] selOptions;
      }
      NS_RELEASE(listBox);
    }
  }
}

// Update the locally cached selection state (not widget)
void nsNativeSelectControlFrame::SetOptionSelectedCache(PRInt32 indx, PRBool aValue)
{
  if (nsnull != mOptionSelected) {
    if ((-1 < indx) && (mNumOptions >= indx)) {
      mOptionSelected[indx] = aValue;
    }
  }
}         

NS_IMETHODIMP
nsNativeSelectControlFrame::AttributeChanged(nsIPresContext* aPresContext,
                                     nsIContent*     aChild,
                                     PRInt32         aNameSpaceID,
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


NS_IMETHODIMP nsNativeSelectControlFrame::SetProperty(nsIPresContext* aPresContext,
                                                      nsIAtom* aName, const nsString& aValue)
{
  if (nsHTMLAtoms::selected == aName) {
    return NS_ERROR_INVALID_ARG; // Selected is readonly according to spec.
  } else if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 error = 0;
    PRInt32 selectedIndex = aValue.ToInteger(&error, 10); // Get index from aValue
    if (error)
      return NS_ERROR_INVALID_ARG; // Couldn't convert to integer

    // Update local cache of selected values
    nsresult res = Deselect();
    if (NS_SUCCEEDED(res)) {
      SetOptionSelectedCache(selectedIndex, PR_TRUE);	// Select selectedIndex
    }

    // Update widget
    UpdateWidgetToCache();
  } else {
    return nsNativeFormControlFrame::SetProperty(aPresContext, aName, aValue);
  }
  return NS_OK;
}

NS_IMETHODIMP nsNativeSelectControlFrame::AddOption(nsIPresContext* aPresContext,
                                                    PRInt32 aIndex)
{
    // Grab the content model information
    nsIDOMHTMLCollection* options = GetOptions();
    if (!options) return NS_ERROR_UNEXPECTED;

    // Save the cache data structure
    PRBool* saveOptionSelected = mOptionSelected;
    PRBool selected = PR_FALSE;

    // Grab the widget (we need to update it)
    nsIListWidget* listWidget;
    nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
    if ((NS_FAILED(result)) || (nsnull == listWidget))
      return NS_ERROR_UNEXPECTED;

    mNumOptions++;
    mOptionSelected = new PRBool[mNumOptions];
    if (!mOptionSelected) return NS_ERROR_OUT_OF_MEMORY;

    // Copy saved local cache into new local cache
    for (PRInt32 i=0, j=0; j < mNumOptions; i++,j++) {
      if (i == aIndex) { // At the point of insertion
        // Add the option to the widget
        nsString text(" ");
        GetOptionText(options, i, text);
        listWidget->AddItemAt(text, i);

        // Get the correct selected value and text of the option
        nsIDOMHTMLOptionElement* option = GetOption(*options, i);
	if (option) {
          option->GetDefaultSelected(&selected);
	  NS_RELEASE(option);
	} else {
	  selected = PR_FALSE; // XXX failure case.
	}
        mOptionSelected[j]=selected;
        j++;
      }

      // Copy old selected value
      if (j < mNumOptions) {
        mOptionSelected[j]=saveOptionSelected[i];
      }
    }

    UpdateWidgetToCache();

    NS_RELEASE(options);
    NS_RELEASE(listWidget);
    if (saveOptionSelected)
      delete [] saveOptionSelected;
      
    return NS_OK;
}

NS_IMETHODIMP nsNativeSelectControlFrame::RemoveOption(nsIPresContext* aPresContext,
                                                       PRInt32 aIndex)
{
    // Grab the content model information
    nsIDOMHTMLCollection* options = GetOptions();
    if (!options) return NS_ERROR_UNEXPECTED;

    // Save the cache data structure
    PRUint32 saveNumOptions = mNumOptions;
    PRBool* saveOptionSelected = mOptionSelected;
    PRBool selected = PR_FALSE;

    // Grab the widget (we need to update it)
    nsIListWidget* listWidget;
    nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
    if ((NS_FAILED(result)) || (nsnull == listWidget)) {
      return NS_ERROR_UNEXPECTED;
    }

      mNumOptions--;
      if (mNumOptions) {
        mOptionSelected = new PRBool[mNumOptions];
        if (!mOptionSelected) return NS_ERROR_OUT_OF_MEMORY;
      } else {
        mOptionSelected = nsnull;
      }
      // If we got the index, remove just that one option, like this:
      if (aIndex >= 0) {
        // Copy saved local cache into new local cache
        for (PRInt32 i=0, j=0; j < mNumOptions; i++,j++) {
          if (i == aIndex) i++; // At the point of deletion
          mOptionSelected[j]=saveOptionSelected[i];
        }

        // Update widget
        listWidget->RemoveItemAt(aIndex);

      // No index, so we'll have to remove all options and recreate. (Performance hit)
      // We also loose the status of what has been selected.
      } else {
        // Remove all stale options
        PRInt32 i;
        for (i=saveNumOptions; i>=0; i--) {
          listWidget->RemoveItemAt(i);
        }

        // Add all the options back in
        for (i=0; i<mNumOptions; i++) {
          // Add the option to the widget
          nsString text(" ");
          GetOptionText(options, i, text);
          listWidget->AddItemAt(text, i);

          // Get the default (XXXincorrect) selected value and text of the option
          nsIDOMHTMLOptionElement* option = GetOption(*options, i);
	  if (option) {
            option->GetDefaultSelected(&selected); // Should be sel, not defsel :(
	    NS_RELEASE(option);
	  } else {
	    selected = PR_FALSE; // XXX failure case
	  }
          mOptionSelected[i]=selected;
        }
      }

    UpdateWidgetToCache();

    NS_RELEASE(options);
    NS_RELEASE(listWidget);
    if (saveOptionSelected)
      delete [] saveOptionSelected;

    return NS_OK;
}

NS_IMETHODIMP nsNativeSelectControlFrame::GetProperty(nsIAtom* aName, nsAWritableString& aValue)
{
  // Get the selected value of option from local cache (optimization vs. widget)
  if (nsHTMLAtoms::selected == aName) {
    PRInt32 error = 0;
    PRBool selected = PR_FALSE;
    PRInt32 indx = aValue.ToInteger(&error, 10); // Get index from aValue
    GetOptionSelectedCache(indx, &selected);
    nsFormControlHelper::GetBoolString(selected, aValue);
    
  // For selectedIndex, get the value from the widget
  } else  if (nsHTMLAtoms::selectedindex == aName) {
    PRInt32 selectedIndex = -1;
    if (mWidget) {
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
    }
    aValue.Append(selectedIndex, 10);
  } else {
    return nsNativeFormControlFrame::GetProperty(aName, aValue);
  }
  return NS_OK;
}

// Set the selection of option in the local selection cache, then update widget
NS_IMETHODIMP
nsNativeSelectControlFrame::SetOptionSelected(PRInt32 aIndex, PRBool aValue)
{
  PRBool multiple = PR_FALSE;
  GetMultiple(&multiple);

  if (PR_TRUE == multiple) {
    SetOptionSelectedCache(aIndex, aValue);
  } else {
    if (aValue) {
      // Get Selected index out of Content model
      PRInt32 selectedIndex = GetSelectedIndex();
      if (-1 < selectedIndex)
        SetOptionSelectedCache(selectedIndex, PR_FALSE);
      SetOptionSelectedCache(aIndex, PR_TRUE);
    } else {
      SetOptionSelectedCache(aIndex, PR_FALSE);
    }
  }
  // Note that UpdateWidgetToCache may return NS_ERROR_UNEXPECTED if the
  // widget is not created yet.  We can safely ignore this as when Reset is
  // called, it will update the widget to the cache's state.
  UpdateWidgetToCache(!aValue); // Don't deselect all if adding selection
  return NS_OK;
}

NS_IMETHODIMP
nsNativeSelectControlFrame::GetOptionSelected(PRInt32 aIndex, PRBool* aValue)
{
  // Determine if option is selected in local cache
  GetOptionSelectedCache(aIndex, aValue);
  return NS_OK;
}

nsresult
nsNativeSelectControlFrame::UpdateWidgetToCache(PRBool aDeselectFirst)
{
  if (!mWidget) {
    mCachedState = PR_TRUE; // Handle this update later when widget is created.
    return NS_OK;
  }

  // Grab the list widget
  nsIListWidget* listWidget;
  nsresult result = mWidget->QueryInterface(kListWidgetIID, (void **) &listWidget);
  if ((NS_FAILED(result)) || !listWidget) {
    return NS_ERROR_UNEXPECTED;
  }

  // Update widget selection to the cached values.
  // Normally, we have to deselect, then reselect all
  if (aDeselectFirst) {
    listWidget->Deselect();
  }
  PRInt32 selectedIndex = -1;
  PRBool selected = PR_FALSE;
  for (PRInt32 i=0; i < mNumOptions; i++) {
    GetOptionSelectedCache(i, &selected);
    if (selected) {
      listWidget->SelectItem(i);
      selectedIndex = i;
    }
  }
  
  // Select first option of combobox if needed
  if (mIsComboBox && (mNumOptions > 0) && (selectedIndex == -1)) {
    listWidget->SelectItem(0);
    SetOptionSelectedCache(0, PR_TRUE);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsNativeSelectControlFrame::AppendFrames(nsIPresContext* aPresContext,
                                          nsIPresShell&   aPresShell,
                                          nsIAtom*        aListName,
                                          nsIFrame*       aFrameList)
{
  //NS_PRECONDITION(PR_FALSE, "not a container");
  return NS_ERROR_UNEXPECTED;
}

nsresult
nsNativeSelectControlFrame::Deselect()
{
  nsresult res = NS_OK;
  for (PRInt32 i=0; (i<mNumOptions) && NS_SUCCEEDED(res); i++)
    res = SetOptionSelected(i, PR_FALSE);
  return res;
}

//----------------------------------------------------------------------
// nsIStatefulFrame
//----------------------------------------------------------------------
NS_IMETHODIMP
nsNativeSelectControlFrame::GetStateType(nsIPresContext* aPresContext,
                                         nsIStatefulFrame::StateType* aStateType)
{
  *aStateType = nsIStatefulFrame::eSelectType;
  return NS_OK;
}

NS_IMETHODIMP
nsNativeSelectControlFrame::SaveState(nsIPresContext* aPresContext, nsIPresState** aState)
{
  return NS_OK;
}

NS_IMETHODIMP
nsNativeSelectControlFrame::RestoreState(nsIPresContext* aPresContext, nsIPresState* aState)
{
  // Doesn't matter.
  return NS_OK;
}
