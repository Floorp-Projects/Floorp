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

#include "nsInput.h"
#include "nsInputFrame.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainer.h"
#include "nsIRenderingContext.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsIStyleContext.h"
#include "nsLeafFrame.h"
#include "nsCSSRendering.h"
#include "nsHTMLIIDs.h"
#include "nsIButton.h"
#include "nsIViewManager.h"
#include "nsISupports.h"
#include "nsHTMLAtoms.h"
#include "nsIButton.h"
#include "nsIView.h"
#include "nsViewsCID.h"
#include "nsWidgetsCID.h"
#include "nsIDeviceContext.h"
#include "nsIFontCache.h"
#include "nsIFontMetrics.h"
#include "nsIFormManager.h"
#include "nsIImage.h"
#include "nsHTMLForms.h"

enum nsButtonTagType {
  kButtonTag_Button,
  kButtonTag_Input
};

enum nsButtonType {
  kButton_Button,
  kButton_Reset,
  kButton_Submit,
  kButton_Image,
  kButton_Hidden
};
	
static NS_DEFINE_IID(kIFormControlIID, NS_IFORMCONTROL_IID);

class nsInputButton : public nsInput {
public:
  typedef nsInput nsInputButtonSuper;
  nsInputButton (nsIAtom* aTag, nsIFormManager* aManager,
                 nsButtonType aType);

  virtual nsIFrame* CreateFrame(nsIPresContext* aPresContext,
                                nsIFrame* aParentFrame);

  virtual void GetDefaultLabel(nsString& aLabel);

  nsButtonType GetButtonType() { return mType; }
  nsButtonTagType GetButtonTagType() { return mTagType; }

  virtual PRInt32 GetMaxNumValues(); 

  virtual PRBool GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues, 
                                nsString* aValues, nsString* aNames);

  virtual PRBool IsHidden();

  virtual PRBool IsSuccessful(nsIFormControl* aSubmitter) const;

 protected:
  virtual ~nsInputButton();

  virtual void GetType(nsString& aResult) const;

  nsButtonType    mType;
  nsButtonTagType mTagType;  
};

class nsInputButtonFrame : public nsInputFrame {
public:
  typedef nsInputFrame nsInputButtonFrameSuper;
  nsInputButtonFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                  nsIRenderingContext& aRenderingContext,
                  const nsRect& aDirtyRect);

  NS_IMETHOD ResizeReflow(nsIPresContext* aPresContext,
                         nsReflowMetrics& aDesiredSize,
                         const nsSize& aMaxSize,
                         nsSize* aMaxElementSize,
                         ReflowStatus& aStatus);

  virtual void PostCreateWidget(nsIPresContext* aPresContext, nsIView* aView);

  virtual void MouseClicked(nsIPresContext* aPresContext);

  virtual const nsIID& GetCID();

  virtual const nsIID& GetIID();

  nsButtonType GetButtonType() const;
  nsButtonTagType GetButtonTagType() const;

  nsIImage* GetImage(nsIPresContext& aPresContext);


protected:
  virtual  ~nsInputButtonFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsSize& aMaxSize,
                              nsReflowMetrics& aDesiredLayoutSize,
                              nsSize& aDesiredWidgetSize);
};


// nsInputButton Implementation

nsInputButton::nsInputButton(nsIAtom* aTag, nsIFormManager* aManager,
                             nsButtonType aType)
  : nsInput(aTag, aManager), mType(aType) 
{
  nsAutoString tagName;
  aTag->ToString(tagName);
  mTagType = (tagName.EqualsIgnoreCase("input")) ? kButtonTag_Input : kButtonTag_Button;
}

nsInputButton::~nsInputButton()
{
  if (nsnull != mValue) {
    delete mValue;
  }
}

PRBool nsInputButton::IsSuccessful(nsIFormControl* aSubmitter) const
{
  if ((void*)&mControl == (void*)aSubmitter) {
    return nsInputButtonSuper::IsSuccessful(aSubmitter);
  }
  return PR_FALSE;
}

PRBool 
nsInputButton::IsHidden()
{
  if (kButton_Hidden == mType) {
    return PR_TRUE;
  }
  else {
    return PR_FALSE;
  }
}

void nsInputButton::GetType(nsString& aResult) const
{
  aResult.SetLength(0);

  if (kButtonTag_Button == mTagType) {
    aResult.Append("button");
    return;
  }

  switch (mType) {
    case kButton_Button:
      aResult.Append("button");
      break;
    case kButton_Reset:
      aResult.Append("reset");
      break;
    case kButton_Image:
      aResult.Append("image");
      break;
    case kButton_Hidden:
      aResult.Append("hidden");
      break;
    case kButton_Submit:
    default:
      aResult.Append("submit");
      break;
  }
}

void
nsInputButton::GetDefaultLabel(nsString& aString) 
{
  if (kButton_Reset == mType) {
    aString = "Reset";
  } else if (kButton_Submit == mType) {
    aString = "Submit";
  } else {
    aString = "noname";
  }
}

nsIFrame* 
nsInputButton::CreateFrame(nsIPresContext* aPresContext,
                           nsIFrame* aParentFrame)
{
  if (kButton_Hidden == mType) {
    nsIFrame* frame;
    nsFrame::NewFrame(&frame, this, aParentFrame);
    return frame;
  } 
  else {
   return new nsInputButtonFrame(this, aParentFrame);
  }
 
}

PRInt32
nsInputButton::GetMaxNumValues() 
{
  if ((kButton_Submit == mType) || (kButton_Hidden == mType)) {
    return 1;
  } else if ((kButton_Image == mType) && (kButtonTag_Input == mTagType)) {
    return 2;
  } else {
	  return 0;
  }
}


PRBool
nsInputButton::GetNamesValues(PRInt32 aMaxNumValues, PRInt32& aNumValues,
                              nsString* aValues, nsString* aNames)
{
  if ((aMaxNumValues <= 0) || (nsnull == mName)) {
    return PR_FALSE;
  }

  if ((kButton_Image == mType) && (kButtonTag_Input == mTagType)) {
    char buf[20];
    aNumValues = 2;

    aValues[0].SetLength(0);
    sprintf(&buf[0], "%d", mLastClickPoint.x);
    aValues[0].Append(&buf[0]);

    aNames[0] = *mName;
    aNames[0].Append(".x");

    aValues[1].SetLength(0);
    sprintf(&buf[0], "%d", mLastClickPoint.y);
    aValues[1].Append(&buf[0]);

    aNames[1] = *mName;
    aNames[1].Append(".y");

    return PR_TRUE;
  }
  else if ((kButton_Submit == mType) || (kButton_Hidden == mType) && (nsnull != mValue)) {
    aValues[0] = *mValue;
    aNames[0]  = *mName;
    aNumValues = 1;
    return PR_TRUE;
  } else {
    aNumValues = 0;
    return PR_FALSE;
  }
}

//----------------------------------------------------------------------
// nsInputButtonFrame Implementation

nsInputButtonFrame::nsInputButtonFrame(nsIContent* aContent,
                           nsIFrame* aParentFrame)
  : nsInputFrame(aContent, aParentFrame)
{
}

nsInputButtonFrame::~nsInputButtonFrame()
{
}

nsButtonType 
nsInputButtonFrame::GetButtonType() const
{
  nsInputButton* button = (nsInputButton *)mContent;
  return button->GetButtonType();
}

nsButtonTagType 
nsInputButtonFrame::GetButtonTagType() const
{
  nsInputButton* button = (nsInputButton *)mContent;
  return button->GetButtonTagType();
}

nsIImage* nsInputButtonFrame::GetImage(nsIPresContext& aPresContext)
{
  if (kButton_Image != GetButtonType()) {
    return nsnull;
  }

  nsAutoString src;
  if (eContentAttr_HasValue == mContent->GetAttribute("SRC", src)) {
    return aPresContext.LoadImage(src, this);
  }
  return nsnull;
}

NS_METHOD nsInputButtonFrame::Paint(nsIPresContext& aPresContext,
                              nsIRenderingContext& aRenderingContext,
                              const nsRect& aDirtyRect)
{
  // let super do processing if there is no image
  if (kButton_Image != GetButtonType()) {
    return nsInputButtonFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect);
  }

  nsIImage* image = GetImage(aPresContext);
  if (nsnull == image) {
    return NS_OK;
  }

  // First paint background and borders
  nsLeafFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);

  // Now render the image into our inner area (the area without the
  nsRect inner;
  GetInnerArea(&aPresContext, inner);
  aRenderingContext.DrawImage(image, inner);

  return NS_OK;
}

void
nsInputButtonFrame::MouseClicked(nsIPresContext* aPresContext) 
{
  nsInputButton* button = (nsInputButton *)mContent;
  nsIFormManager* formMan = button->GetFormManager();
  if (nsnull != formMan) {
    nsButtonType    butType    = button->GetButtonType();
    nsButtonTagType butTagType = button->GetButtonTagType();
    if (kButton_Reset == butType) {
      formMan->OnReset();
    } else if ((kButton_Submit == butType) ||
               ((kButton_Image == butType) && (kButtonTag_Input == butTagType))) {
      //NS_ADDREF(this);
      nsIFormControl* control;
      mContent->QueryInterface(kIFormControlIID, (void**)&control);
      formMan->OnSubmit(aPresContext, this, control);
      //NS_RELEASE(this);
    }
    NS_RELEASE(formMan);
  }
}

NS_METHOD
nsInputButtonFrame::ResizeReflow(nsIPresContext* aPresContext,
                                 nsReflowMetrics& aDesiredSize,
                                 const nsSize& aMaxSize,
                                 nsSize* aMaxElementSize,
                                 ReflowStatus& aStatus)
{
  if ((kButtonTag_Input == GetButtonTagType()) && (kButton_Image == GetButtonType())) {
    nsSize ignore;
    GetDesiredSize(aPresContext, aMaxSize, aDesiredSize, ignore);
    AddBordersAndPadding(aPresContext, aDesiredSize);
    if (nsnull != aMaxElementSize) {
      aMaxElementSize->width = aDesiredSize.width;
      aMaxElementSize->height = aDesiredSize.height;
    }
    mCacheBounds.width  = aDesiredSize.width;
    mCacheBounds.height = aDesiredSize.height;
    aStatus = frComplete;
    return NS_OK;
  }
  else {
    return nsInputButtonFrameSuper::
      ResizeReflow(aPresContext, aDesiredSize, aMaxSize, aMaxElementSize, aStatus);
  }
}

void 
nsInputButtonFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                   const nsSize& aMaxSize,
                                   nsReflowMetrics& aDesiredLayoutSize,
                                   nsSize& aDesiredWidgetSize)
{

  if (kButton_Hidden == GetButtonType()) { // there is no physical rep
    aDesiredLayoutSize.width  = 0;
    aDesiredLayoutSize.height = 0;
    aDesiredLayoutSize.ascent = 0;
    aDesiredLayoutSize.descent = 0;
  }
  else {
    nsSize styleSize;
    GetStyleSize(*aPresContext, aMaxSize, styleSize);

    if (kButton_Image == GetButtonType()) { // there is an image
      float p2t = aPresContext->GetPixelsToTwips();
      if ((0 < styleSize.width) && (0 < styleSize.height)) {
        // Use dimensions from style attributes
        aDesiredLayoutSize.width  = nscoord(styleSize.width  * p2t);
        aDesiredLayoutSize.height = nscoord(styleSize.height * p2t);
      } else {
        nsIImage* image = GetImage(*aPresContext);
        if (nsnull == image) {
          // XXX Here is where we trigger a resize-reflow later on; or block
          // layout or whatever our policy wants to be
          aDesiredLayoutSize.width  = nscoord(50 * p2t);
          aDesiredLayoutSize.height = nscoord(50 * p2t);
        } else {
          aDesiredLayoutSize.width  = nscoord(image->GetWidth()  * p2t);
          aDesiredLayoutSize.height = nscoord(image->GetHeight() * p2t);
        }
      }
    }
    else {  // there is a widget
      nsSize size;
      PRBool widthExplicit, heightExplicit;
      PRInt32 ignore;
      nsInputDimensionSpec spec(nsHTMLAtoms::size, PR_TRUE, nsHTMLAtoms::value, 1,
                                PR_FALSE, nsnull, 1);
      CalculateSize(aPresContext, this, styleSize, spec, size, 
                    widthExplicit, heightExplicit, ignore);

      if (!widthExplicit) {
        size.width += 100;
      } 
      if (!heightExplicit) {
        size.height += 100;
      } 

      aDesiredLayoutSize.width = size.width;
      aDesiredLayoutSize.height= size.height;
    }
    aDesiredLayoutSize.ascent = aDesiredLayoutSize.height;
    aDesiredLayoutSize.descent = 0;
  }

  aDesiredWidgetSize.width = aDesiredLayoutSize.width;
  aDesiredWidgetSize.height= aDesiredLayoutSize.height;
}


void 
nsInputButtonFrame::PostCreateWidget(nsIPresContext* aPresContext, nsIView *aView)
{
  nsIButton* button;
  if (NS_OK == GetWidget(aView, (nsIWidget **)&button)) {
	  button->SetFont(GetFont(aPresContext));
	} 
  else {
    NS_ASSERTION(0, "no widget in button control");
  }

  nsInputButton* content;
  GetContent((nsIContent*&) content);
  nsString value;
  nsContentAttr status = content->GetAttribute(nsHTMLAtoms::value, value);
  if (eContentAttr_HasValue == status) {  
	  button->SetLabel(value);
  } 
  else {
    button->SetLabel(" ");
  }

  NS_RELEASE(content);
  NS_RELEASE(button);
}

const nsIID&
nsInputButtonFrame::GetIID()
{
  static NS_DEFINE_IID(kButtonIID, NS_IBUTTON_IID);
  return kButtonIID;
}
  
const nsIID&
nsInputButtonFrame::GetCID()
{
  static NS_DEFINE_IID(kButtonCID, NS_BUTTON_CID);
  return kButtonCID;
}

nsresult
CreateButton(nsIHTMLContent** aInstancePtrResult,
             nsIAtom* aTag, nsIFormManager* aManager,
             nsButtonType aType)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsInputButton(aTag, aManager, aType);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLInputButton(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Button);
}

nsresult
NS_NewHTMLButton(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Button);
}

nsresult
NS_NewHTMLInputSubmit(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Submit);
}

nsresult
NS_NewHTMLInputReset(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Reset);
}

nsresult
NS_NewHTMLInputImage(nsIHTMLContent** aInstancePtrResult,
                     nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Image);
}

nsresult
NS_NewHTMLInputHidden(nsIHTMLContent** aInstancePtrResult,
                      nsIAtom* aTag, nsIFormManager* aManager)
{
  return CreateButton(aInstancePtrResult, aTag, aManager, kButton_Hidden);
}
