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

#include "nsHTMLContainer.h"
#include "nsLeafFrame.h"
#include "nsHTMLContainerFrame.h"
#include "nsIWebWidget.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsHTMLIIDs.h"
#include "nsRepository.h"
#include "nsIStreamListener.h"
#include "nsIURL.h"
#include "nsIDocument.h"
#include "nsIWebFrame.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsHTMLAtoms.h"
#include "nsIScrollableView.h"
#include "nsStyleCoord.h"
#include "nsIStyleContext.h"
#include "nsCSSLayout.h"

#include "nsIDocumentWidget.h"
#include "nsIDocumentLoader.h"
class nsHTMLIFrame;
static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebWidgetIID, NS_IWEBWIDGET_IID);
static NS_DEFINE_IID(kIWebFrameIID, NS_IWEBFRAME_IID);
static NS_DEFINE_IID(kCWebWidgetCID, NS_WEBWIDGET_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kCViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kCChildCID, NS_CHILD_CID);


/*******************************************************************************
 * TempObserver XXX temporary until doc manager/loader is in place
 ******************************************************************************/
class TempObserver : public nsIStreamObserver
{
public:
  TempObserver() { NS_INIT_REFCNT(); }

  ~TempObserver() {}
  // nsISupports
  NS_DECL_ISUPPORTS

  // nsIStreamObserver
  NS_IMETHOD OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg);
  NS_IMETHOD OnStartBinding(nsIURL* aURL, const char *aContentType);
  NS_IMETHOD OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg);

protected:

  nsString mURL;
  nsString mOverURL;
  nsString mOverTarget;
};


/*******************************************************************************
 * nsHTMLIFrameOuterFrame
 ******************************************************************************/
class nsHTMLIFrameOuterFrame : public nsHTMLContainerFrame {

public:
  nsHTMLIFrameOuterFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  nscoord GetBorderWidth(nsIPresContext& aPresContext);

protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);
  virtual PRIntn GetSkipSides() const;
};

/*******************************************************************************
 * nsHTMLIFrameInnerFrame
 ******************************************************************************/
class nsHTMLIFrameInnerFrame : public nsLeafFrame, public nsIWebFrame {

public:

  nsHTMLIFrameInnerFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  /**
    * @see nsIFrame::Paint
    */
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  /**
    * @see nsIFrame::Reflow
    */
  NS_IMETHOD Reflow(nsIPresContext&      aCX,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);

  NS_IMETHOD MoveTo(nscoord aX, nscoord aY);
  NS_IMETHOD SizeTo(nscoord aWidth, nscoord aHeight);

  virtual nsIWebWidget* GetWebWidget();

  float GetTwipsToPixels();

  NS_IMETHOD GetParentContent(nsHTMLIFrame*& aContent);

  NS_DECL_ISUPPORTS

protected:

  virtual ~nsHTMLIFrameInnerFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  nsresult CreateWebWidget(const nsSize& aSize);

  nsIWebWidget* mWebWidget;
  PRBool mCreatingViewer;

  // XXX fix these
  TempObserver* mTempObserver;
};

/*******************************************************************************
 * nsHTMLIFrame
 ******************************************************************************/
class nsHTMLIFrame : public nsHTMLContainer {
public:
 
  virtual nsresult  CreateFrame(nsIPresContext*  aPresContext,
                                nsIFrame*        aParentFrame,
                                nsIStyleContext* aStyleContext,
                                nsIFrame*&       aResult);

  void GetSize(float aPixelsToTwips, nsSize& aSize);
  PRBool GetURL(nsString& aURLSpec);
  PRBool GetName(nsString& aName);
  nsScrollPreference GetScrolling();
  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);
  PRBool GetFrameBorder();

#if 0
  virtual nsContentAttr GetAttribute(nsIAtom* aAttribute,
                                     nsHTMLValue& aValue) const;
#endif

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

protected:
  nsHTMLIFrame(nsIAtom* aTag, nsIWebWidget* aParentWebWidget);
  virtual  ~nsHTMLIFrame();

  // this is held for a short time until the frame uses it, so it is not ref counted
  nsIWebWidget* mParentWebWidget;  

  friend nsresult NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                                   nsIAtom* aTag, nsIWebWidget* aWebWidget);
  friend class nsHTMLIFrameInnerFrame;

};

/*******************************************************************************
 * nsHTMLIFrameOuterFrame
 ******************************************************************************/
nsHTMLIFrameOuterFrame::nsHTMLIFrameOuterFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

nscoord
nsHTMLIFrameOuterFrame::GetBorderWidth(nsIPresContext& aPresContext)
{
  if (nsnull == mStyleContext) {// make sure the style context is set
    GetStyleContext(&aPresContext, mStyleContext);
  }
  const nsStyleSpacing* spacing =
    (const nsStyleSpacing*)mStyleContext->GetStyleData(eStyleStruct_Spacing);
  nsStyleCoord leftBorder;
  spacing->mBorder.GetLeft(leftBorder);
  nsStyleUnit unit = leftBorder.GetUnit(); 
  if (eStyleUnit_Coord == unit) {
    return leftBorder.GetCoordValue();
  }
  return 0;
}

PRIntn
nsHTMLIFrameOuterFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLIFrameOuterFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsReflowState& aReflowState,
                                  nsReflowMetrics& aDesiredSize)
{
  float p2t = aPresContext->GetPixelsToTwips();

  nsSize size;
  PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState, size);

  if (0 == (ss & NS_SIZE_HAS_WIDTH)) {
    size.width = (nscoord)(200.0 * p2t + 0.5);
  }
  if (0 == (ss & NS_SIZE_HAS_HEIGHT)) {
    size.height = (nscoord)(200 * p2t + 0.5);
  }

  aDesiredSize.width  = size.width;
  aDesiredSize.height = size.height;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

NS_IMETHODIMP
nsHTMLIFrameOuterFrame::Paint(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect)
{
  printf("outer paint %d %d %d %d \n", aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  if (nsnull != mFirstChild) {
    mFirstChild->Paint(aPresContext, aRenderingContext, aDirtyRect);
  }
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

NS_IMETHODIMP
nsHTMLIFrameOuterFrame::Reflow(nsIPresContext&      aPresContext,
                               nsReflowMetrics&     aDesiredSize,
                               const nsReflowState& aReflowState,
                               nsReflowStatus&      aStatus)
{
  // Always get the size so that the caller knows how big we are
  GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);

  if (nsnull == mFirstChild) {
    mFirstChild = new nsHTMLIFrameInnerFrame(mContent, this);
    mChildCount = 1;
  }
  
  nscoord borderWidth  = GetBorderWidth(aPresContext);
  nscoord borderWidth2 = 2 * borderWidth;
  nsSize innerSize(aDesiredSize.width - borderWidth2, aDesiredSize.height - borderWidth2);

  // Reflow the child and get its desired size
  nsReflowState kidReflowState(mFirstChild, aReflowState, innerSize);
  mFirstChild->WillReflow(aPresContext);
  nsReflowMetrics ignore(nsnull);
  aStatus = ReflowChild(mFirstChild, &aPresContext, ignore, kidReflowState);
  NS_ASSERTION(NS_FRAME_IS_COMPLETE(aStatus), "bad status");
  
  // Place and size the child
  nsRect rect(borderWidth, borderWidth, innerSize.width, innerSize.height);
  mFirstChild->SetRect(rect);
  mFirstChild->DidReflow(aPresContext, NS_FRAME_REFLOW_FINISHED);

  // XXX what should the max-element-size of an iframe be? Shouldn't
  // iframe's normally shrink wrap around their content when they
  // don't have a specified width/height?
  if (nsnull != aDesiredSize.maxElementSize) {
    aDesiredSize.maxElementSize->width = aDesiredSize.width;
    aDesiredSize.maxElementSize->height = aDesiredSize.height;
  }

  return NS_OK;
}

/*******************************************************************************
 * nsHTMLIFrameInnerFrame
 ******************************************************************************/
nsHTMLIFrameInnerFrame::nsHTMLIFrameInnerFrame(nsIContent* aContent, nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
  NS_INIT_REFCNT();

  // Addref this frame because it supports interfaces which require ref-counting!
  NS_ADDREF(this);
  
  mWebWidget = nsnull;
  mCreatingViewer = PR_FALSE;
  mTempObserver = new TempObserver();
  NS_ADDREF(mTempObserver);
}

nsHTMLIFrameInnerFrame::~nsHTMLIFrameInnerFrame()
{
  NS_IF_RELEASE(mWebWidget);
  NS_RELEASE(mTempObserver);
}

NS_IMPL_ADDREF(nsHTMLIFrameInnerFrame);
NS_IMPL_RELEASE(nsHTMLIFrameInnerFrame);

nsresult
nsHTMLIFrameInnerFrame::QueryInterface(const nsIID& aIID,
                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIWebFrameIID)) {
    *aInstancePtrResult = (void*) ((nsIWebFrame*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIWebFrame*)this));
    AddRef();
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtrResult);
}

nsIWebWidget* nsHTMLIFrameInnerFrame::GetWebWidget()
{
  NS_IF_ADDREF(mWebWidget);
  return mWebWidget;
}

float nsHTMLIFrameInnerFrame::GetTwipsToPixels()
{
  nsISupports* parentSup;
  if (mWebWidget) {
    mWebWidget->GetContainer(&parentSup);
    if (parentSup) {
      nsIWebWidget* parentWidget;
      nsresult res = parentSup->QueryInterface(kIWebWidgetIID, (void**)&parentWidget);
      if (NS_OK == res) {
        nsIPresContext* presContext = parentWidget->GetPresContext();
        NS_RELEASE(parentWidget);
        if (presContext) {
          float ret = presContext->GetTwipsToPixels();
          NS_RELEASE(presContext);
          return ret;
        } 
      } else {
        NS_ASSERTION(0, "invalid web widget container");
      }
    } else {
      NS_ASSERTION(0, "invalid web widget container");
    }
  }
  return (float)0.05;  // this should not be reached
}


NS_METHOD
nsHTMLIFrameInnerFrame::MoveTo(nscoord aX, nscoord aY)
{
  return nsLeafFrame::MoveTo(aX, aY);
}

NS_METHOD
nsHTMLIFrameInnerFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  return nsLeafFrame::SizeTo(aWidth, aWidth);
}

NS_IMETHODIMP
nsHTMLIFrameInnerFrame::Paint(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect)
{
  printf("inner paint %d %d %d %d \n", aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  if (nsnull != mWebWidget) {
    //mWebWidget->Show();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameInnerFrame::GetParentContent(nsHTMLIFrame*& aContent)
{
  nsHTMLIFrameOuterFrame* parent;
  GetGeometricParent((nsIFrame*&)parent);
  return parent->GetContent((nsIContent*&)aContent);
}


void TempMakeAbsURL(nsIContent* aContent, nsString& aRelURL, nsString& aAbsURL)
{
  nsIURL* docURL = nsnull;
  nsIDocument* doc = nsnull;
  aContent->GetDocument(doc);
  if (nsnull != doc) {
    docURL = doc->GetDocumentURL();
    NS_RELEASE(doc);
  }

  nsAutoString base;
  nsresult rv = NS_MakeAbsoluteURL(docURL, base, aRelURL, aAbsURL);
  NS_IF_RELEASE(docURL);
}

nsresult nsHTMLIFrameInnerFrame::CreateWebWidget(const nsSize& aSize)
{
  nsresult rv;
  nsHTMLIFrame* content;
  GetParentContent(content);

  rv = NSRepository::CreateInstance(kCWebWidgetCID, nsnull, kIWebWidgetIID, (void**)&mWebWidget);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "could not create web widget");
    return rv;
  }

  nsString frameName;
  if (content->GetName(frameName)) {
    mWebWidget->SetName(frameName);
  }

  // set the web widget parentage
  nsIWebWidget* parentWebWidget = content->mParentWebWidget;
  parentWebWidget->AddChild(mWebWidget);


  // Get the view manager, conversion
  float t2p = 0.0f;
  nsIViewManager* viewMan = nsnull;

  nsIPresContext* presContext = parentWebWidget->GetPresContext();
  t2p = presContext->GetTwipsToPixels();
  nsIPresShell *presShell = presContext->GetShell();     
	viewMan = presShell->GetViewManager();  
  NS_RELEASE(presShell);
  NS_RELEASE(presContext);
  //NS_RELEASE(parentWebWidget);


  // create, init, set the parent of the view
  nsIView* view;
  rv = NSRepository::CreateInstance(kCViewCID, nsnull, kIViewIID,
                                        (void **)&view);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "Could not create view for nsHTMLIFrame"); 
    return rv;
  }

  nsIView* parView;
  nsPoint origin;
  GetOffsetFromView(origin, parView);  
  nsRect viewBounds(origin.x, origin.y, aSize.width, aSize.height);

  rv = view->Init(viewMan, viewBounds, parView, &kCChildCID);
  viewMan->InsertChild(parView, view, 0);
  NS_RELEASE(viewMan);
  SetView(view);
  NS_RELEASE(parView);

  // init the web widget
  mWebWidget->SetUAStyleSheet(parentWebWidget->GetUAStyleSheet());

  nsIWidget* widget = view->GetWidget();
  NS_RELEASE(view);
  nsRect webBounds(0, 0, NS_TO_INT_ROUND(aSize.width * t2p), 
                   NS_TO_INT_ROUND(aSize.height * t2p));
  mWebWidget->Init(widget->GetNativeData(NS_NATIVE_WIDGET), webBounds, 
                   content->GetScrolling());
  NS_RELEASE(content);
  NS_RELEASE(widget);

///  mWebWidget->Show();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLIFrameInnerFrame::Reflow(nsIPresContext&      aPresContext,
                               nsReflowMetrics&     aDesiredSize,
                               const nsReflowState& aReflowState,
                               nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;

  // use the max size set in aReflowState by the nsHTMLIFrameOuterFrame as our size
  if (!mCreatingViewer) {
    nsHTMLIFrame* content;
    GetParentContent(content);

    nsAutoString url;
    content->GetURL(url);
    nsSize size;

    if (nsnull == mWebWidget) {
      rv = CreateWebWidget(aReflowState.maxSize);
    }
    if (nsnull != mWebWidget) {
      mCreatingViewer=PR_TRUE;

      // load the document
      nsString absURL;
      TempMakeAbsURL(content, url, absURL);

      rv = mWebWidget->LoadURL(absURL,          // URL string
                               mTempObserver,   // Observer
                               nsnull);         // Post Data
    }
    NS_RELEASE(content);
  }

  aDesiredSize.width  = aReflowState.maxSize.width;
  aDesiredSize.height = aReflowState.maxSize.height;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
  return rv;
}

void 
nsHTMLIFrameInnerFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                  const nsReflowState& aReflowState,
                                  nsReflowMetrics& aDesiredSize)
{
  nsSize size;
  ((nsHTMLIFrame*)mContent)->GetSize(aPresContext->GetPixelsToTwips(), size);
  aDesiredSize.width  = size.width;
  aDesiredSize.height = size.height;
  aDesiredSize.ascent = aDesiredSize.height;
  aDesiredSize.descent = 0;
}

/*******************************************************************************
 * nsHTMLIFrame
 ******************************************************************************/
nsHTMLIFrame::nsHTMLIFrame(nsIAtom* aTag, nsIWebWidget* aParentWebWidget)
  : nsHTMLContainer(aTag), mParentWebWidget(aParentWebWidget)
{
}

nsHTMLIFrame::~nsHTMLIFrame()
{
  mParentWebWidget = nsnull;
}

void nsHTMLIFrame::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  nsHTMLValue val;
  if (ParseImageProperty(aAttribute, aString, val)) { // convert width or height to pixels
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  nsHTMLContainer::SetAttribute(aAttribute, aString);
}

void nsHTMLIFrame::MapAttributesInto(nsIStyleContext* aContext, 
                                     nsIPresContext* aPresContext)
{
  MapImagePropertiesInto(aContext, aPresContext);
  MapImageBorderInto(aContext, aPresContext, nsnull);
}

nsresult
nsHTMLIFrame::CreateFrame(nsIPresContext*  aPresContext,
                          nsIFrame*        aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*&       aResult)
{
  nsIFrame* frame = new nsHTMLIFrameOuterFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  frame->SetStyleContext(aPresContext, aStyleContext);
  return NS_OK;
}

void nsHTMLIFrame::GetSize(float aPixelsToTwips, nsSize& aSize)
{
  aSize.width  = 200;
  aSize.height = 200;

  PRInt32 status;
  nsHTMLValue  htmlVal;
  nsAutoString stringVal;
  PRInt32 intVal;

  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::width, htmlVal))) {
    if (eHTMLUnit_String == htmlVal.GetUnit()) {
      htmlVal.GetStringValue(stringVal);
      intVal = stringVal.ToInteger(&status);
      if (NS_OK == status) {
        aSize.width = intVal;
      }
    }
  }
  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::height, htmlVal))) {
    if (eHTMLUnit_String == htmlVal.GetUnit()) {
      htmlVal.GetStringValue(stringVal);
      intVal = stringVal.ToInteger(&status);
      if (NS_OK == status) {
        aSize.height = intVal;
      }
    }
  }

  aSize.width  = (nscoord)(aSize.width * aPixelsToTwips + 0.5);
  aSize.height = (nscoord)(aSize.height * aPixelsToTwips + 0.5);
}

PRBool nsHTMLIFrame::GetURL(nsString& aURLSpec)
{
  nsHTMLValue value;
  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::src, value))) {
    if (eHTMLUnit_String == value.GetUnit()) {
      value.GetStringValue(aURLSpec);
      return PR_TRUE;
    }
  }
  aURLSpec.SetLength(0);
  return PR_FALSE;
}

PRBool nsHTMLIFrame::GetName(nsString& aName)
{
  nsHTMLValue value;
  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::name, value))) {
    if (eHTMLUnit_String == value.GetUnit()) {
      value.GetStringValue(aName);
      return PR_TRUE;
    }
  }
  aName.SetLength(0);
  return PR_FALSE;
}

nsScrollPreference nsHTMLIFrame::GetScrolling()
{
  nsHTMLValue value;
  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::scrolling, value))) {
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString scrolling;
      value.GetStringValue(scrolling);
      if (scrolling.EqualsIgnoreCase("yes")) {
        return nsScrollPreference_kAlwaysScroll;
      } 
      else if (scrolling.EqualsIgnoreCase("no")) {
        return nsScrollPreference_kNeverScroll;
      }
    }
  }
  return nsScrollPreference_kAuto;
}

PRBool nsHTMLIFrame::GetFrameBorder()
{
  nsHTMLValue value;
  if (eContentAttr_HasValue == (GetAttribute(nsHTMLAtoms::frameborder, value))) {
    if (eHTMLUnit_String == value.GetUnit()) {
      nsAutoString frameborder;
      value.GetStringValue(frameborder);
      if (frameborder.EqualsIgnoreCase("0")) {
        return PR_FALSE;
      } 
    }
  }
  return PR_TRUE;
}

nsresult
NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIWebWidget* aWebWidget)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsHTMLIFrame(aTag, aWebWidget);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

// XXX temp implementation
/*******************************************************************************
 * TempObserver
 ******************************************************************************/

NS_IMPL_ADDREF(TempObserver);
NS_IMPL_RELEASE(TempObserver);

nsresult
TempObserver::QueryInterface(const nsIID& aIID,
                            void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIStreamObserverIID)) {
    *aInstancePtrResult = (void*) ((nsIStreamObserver*)this);
    AddRef();
    return NS_OK;
  }
  if (aIID.Equals(kISupportsIID)) {
    *aInstancePtrResult = (void*) ((nsISupports*)((nsIDocumentObserver*)this));
    AddRef();
    return NS_OK;
  }
  return NS_NOINTERFACE;
}


NS_IMETHODIMP
TempObserver::OnProgress(nsIURL* aURL, PRInt32 aProgress, PRInt32 aProgressMax,
                        const nsString& aMsg)
{
  fputs("[progress ", stdout);
  fputs(mURL, stdout);
  printf(" %d %d ", aProgress, aProgressMax);
  fputs(aMsg, stdout);
  fputs("]\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
TempObserver::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
  fputs("Loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
  return NS_OK;
}

NS_IMETHODIMP
TempObserver::OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg)
{
  fputs("Done loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
  return NS_OK;
}


