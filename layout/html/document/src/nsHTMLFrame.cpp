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
#include "nsIWebShell.h"
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
#include "nsIDocumentLoader.h"
//#include "nsIDocumentWidget.h"
#include "nsHTMLFrameset.h"
class nsHTMLFrame;

static NS_DEFINE_IID(kIStreamObserverIID, NS_ISTREAMOBSERVER_IID);
static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kIWebShellIID, NS_IWEB_SHELL_IID);
static NS_DEFINE_IID(kIWebFrameIID, NS_IWEBFRAME_IID);
static NS_DEFINE_IID(kWebShellCID, NS_WEB_SHELL_CID);
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
 * FrameLoadingInfo 
 ******************************************************************************/
class FrameLoadingInfo : public nsISupports
{
public:
  FrameLoadingInfo(const nsSize& aSize);

  // nsISupports interface...
  NS_DECL_ISUPPORTS

protected:
  virtual ~FrameLoadingInfo() {}

public:
  nsSize mFrameSize;
};

/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
class nsHTMLFrameOuterFrame : public nsHTMLContainerFrame {

public:
  nsHTMLFrameOuterFrame(nsIContent* aContent, nsIFrame* aParent);

  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect);

  NS_IMETHOD Reflow(nsIPresContext&      aPresContext,
                    nsReflowMetrics&     aDesiredSize,
                    const nsReflowState& aReflowState,
                    nsReflowStatus&      aStatus);
  nscoord GetBorderWidth(nsIPresContext& aPresContext);
  PRBool IsInline();

protected:
  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);
  virtual PRIntn GetSkipSides() const;
};

/*******************************************************************************
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
class nsHTMLFrameInnerFrame : public nsLeafFrame, public nsIWebFrame {

public:

  nsHTMLFrameInnerFrame(nsIContent* aContent, nsIFrame* aParentFrame);

  NS_IMETHOD QueryInterface(REFNSIID aIID, void** aInstancePtr);

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

  NS_IMETHOD GetWebShell(nsIWebShell*& aResult);

//  float GetTwipsToPixels();

  NS_IMETHOD GetParentContent(nsHTMLFrame*& aContent);

protected:
  nsresult CreateWebShell(nsIPresContext& aPresContext, const nsSize& aSize);

  virtual ~nsHTMLFrameInnerFrame();

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsReflowState& aReflowState,
                              nsReflowMetrics& aDesiredSize);

  nsIWebShell* mWebShell;
  PRBool mCreatingViewer;

  // XXX fix these
  TempObserver* mTempObserver;
};

/*******************************************************************************
 * nsHTMLFrame
 ******************************************************************************/
class nsHTMLFrame : public nsHTMLContainer {
public:
 
  virtual nsresult  CreateFrame(nsIPresContext*  aPresContext,
                                nsIFrame*        aParentFrame,
                                nsIStyleContext* aStyleContext,
                                nsIFrame*&       aResult);

  PRBool GetURL(nsString& aURLSpec);
  PRBool GetName(nsString& aName);
  nsScrollPreference GetScrolling();
  virtual void MapAttributesInto(nsIStyleContext* aContext,
                                 nsIPresContext* aPresContext);
  PRBool GetFrameBorder();
  PRBool IsInline() { return mInline; }

  virtual void SetAttribute(nsIAtom* aAttribute, const nsString& aValue);

protected:
  nsHTMLFrame(nsIAtom* aTag, PRBool aInline, nsIWebShell* aParentWebWidget);
  virtual  ~nsHTMLFrame();

  PRBool mInline; // true for <IFRAME>, false for <FRAME>
  // this is held for a short time until the frame uses it, so it is not ref counted
  nsIWebShell* mParentWebWidget;  

  friend nsresult NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                                   nsIAtom* aTag, nsIWebShell* aWebWidget);
  friend nsresult NS_NewHTMLFrame(nsIHTMLContent** aInstancePtrResult,
                                   nsIAtom* aTag, nsIWebShell* aWebWidget);
  friend class nsHTMLFrameInnerFrame;

};

/*******************************************************************************
 * nsHTMLFrameOuterFrame
 ******************************************************************************/
nsHTMLFrameOuterFrame::nsHTMLFrameOuterFrame(nsIContent* aContent, nsIFrame* aParent)
  : nsHTMLContainerFrame(aContent, aParent)
{
}

nscoord
nsHTMLFrameOuterFrame::GetBorderWidth(nsIPresContext& aPresContext)
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
nsHTMLFrameOuterFrame::GetSkipSides() const
{
  return 0;
}

void 
nsHTMLFrameOuterFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsReflowState& aReflowState,
                                      nsReflowMetrics& aDesiredSize)
{
  // <frame> processing does not use this routine, only <iframe>
  float p2t = aPresContext->GetPixelsToTwips();

  nsSize size;
  PRIntn ss = nsCSSLayout::GetStyleSize(aPresContext, aReflowState, size);

  // XXX this needs to be changed from (200,200) to a better default for inline frames
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

PRBool nsHTMLFrameOuterFrame::IsInline()
{ 
  return ((nsHTMLFrame*)mContent)->IsInline(); 
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Paint(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect)
{
  printf("outer paint %d (%d,%d,%d,%d) ", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  if (nsnull != mFirstChild) {
    mFirstChild->Paint(aPresContext, aRenderingContext, aDirtyRect);
  }
  return nsHTMLContainerFrame::Paint(aPresContext, aRenderingContext, aDirtyRect);
}

NS_IMETHODIMP
nsHTMLFrameOuterFrame::Reflow(nsIPresContext&      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
  if (IsInline()) {
    GetDesiredSize(&aPresContext, aReflowState, aDesiredSize);
  } else {
    aDesiredSize.width  = aReflowState.maxSize.width;
    aDesiredSize.height = aReflowState.maxSize.height;
  }

  if (nsnull == mFirstChild) {
    mFirstChild = new nsHTMLFrameInnerFrame(mContent, this);
    // XXX temporary! use style system to get correct style!
    mFirstChild->SetStyleContext(&aPresContext, mStyleContext);
    mChildCount = 1;
  }
  
  nscoord borderWidth  = GetBorderWidth(aPresContext);
  nscoord borderWidth2 = 2 * borderWidth;
  nsSize innerSize(aDesiredSize.width - borderWidth2, aDesiredSize.height - borderWidth2);

  // Reflow the child and get its desired size
  nsReflowState kidReflowState(mFirstChild, aReflowState, innerSize);
  mFirstChild->WillReflow(aPresContext);
  aStatus = ReflowChild(mFirstChild, &aPresContext, aDesiredSize, kidReflowState);
//  nsReflowMetrics ignore(nsnull);
//  aStatus = ReflowChild(mFirstChild, &aPresContext, ignore, kidReflowState);
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
 * nsHTMLFrameInnerFrame
 ******************************************************************************/
nsHTMLFrameInnerFrame::nsHTMLFrameInnerFrame(nsIContent* aContent,
                                             nsIFrame* aParentFrame)
  : nsLeafFrame(aContent, aParentFrame)
{
  mWebShell = nsnull;
  mCreatingViewer = PR_FALSE;
  mTempObserver = new TempObserver();
  NS_ADDREF(mTempObserver);
}

nsHTMLFrameInnerFrame::~nsHTMLFrameInnerFrame()
{
  NS_IF_RELEASE(mWebShell);
  NS_RELEASE(mTempObserver);
}

nsresult
nsHTMLFrameInnerFrame::QueryInterface(const nsIID& aIID,
                                      void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }
  if (aIID.Equals(kIWebFrameIID)) {
    *aInstancePtrResult = (void*) ((nsIWebFrame*)this);
    return NS_OK;
  }
  return nsLeafFrame::QueryInterface(aIID, aInstancePtrResult);
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetWebShell(nsIWebShell*& aResult)
{
  aResult = mWebShell;
  NS_IF_ADDREF(mWebShell);
  return NS_OK;
}

#if 0
float nsHTMLFrameInnerFrame::GetTwipsToPixels()
{
  nsISupports* parentSup;
  if (mWebShell) {
    mWebShell->GetContainer(&parentSup);
    if (parentSup) {
      nsIWebShell* parentWidget;
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
#endif


NS_METHOD
nsHTMLFrameInnerFrame::MoveTo(nscoord aX, nscoord aY)
{
  return nsLeafFrame::MoveTo(aX, aY);
}

NS_METHOD
nsHTMLFrameInnerFrame::SizeTo(nscoord aWidth, nscoord aHeight)
{
  return nsLeafFrame::SizeTo(aWidth, aWidth);
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Paint(nsIPresContext& aPresContext,
                         nsIRenderingContext& aRenderingContext,
                         const nsRect& aDirtyRect)
{
  printf("inner paint %d (%d,%d,%d,%d) ", this, aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  if (nsnull != mWebShell) {
    //mWebShell->Show();
  }
  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::GetParentContent(nsHTMLFrame*& aContent)
{
  nsHTMLFrameOuterFrame* parent;
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


nsresult
nsHTMLFrameInnerFrame::CreateWebShell(nsIPresContext& aPresContext,
                                      const nsSize& aSize)
{
  nsresult rv;
  nsHTMLFrame* content;
  GetParentContent(content);

  rv = NSRepository::CreateInstance(kWebShellCID, nsnull, kIWebShellIID,
                                    (void**)&mWebShell);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "could not create web widget");
    return rv;
  }

  nsString frameName;
  if (content->GetName(frameName)) {
    mWebShell->SetName(frameName);
  }

  // If our container is a web-shell, inform it that it has a new
  // child. If it's not a web-shell then some things will not operate
  // properly.
  nsISupports* container;
  aPresContext.GetContainer(&container);
  if (nsnull != container) {
    nsIWebShell* outerShell = nsnull;
    container->QueryInterface(kIWebShellIID, (void**) &outerShell);
    if (nsnull != outerShell) {
      outerShell->AddChild(mWebShell);
      NS_RELEASE(outerShell);
    }
    NS_RELEASE(container);
  }

  float t2p = aPresContext.GetTwipsToPixels();
  nsIViewManager* viewMan = nsnull;
  nsIPresShell *presShell = aPresContext.GetShell();     
	viewMan = presShell->GetViewManager();  
  NS_RELEASE(presShell);

  // create, init, set the parent of the view
  nsIView* view;
  rv = NSRepository::CreateInstance(kCViewCID, nsnull, kIViewIID,
                                        (void **)&view);
  if (NS_OK != rv) {
    NS_ASSERTION(0, "Could not create view for nsHTMLFrame"); 
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

  nsIWidget* widget = view->GetWidget();
  NS_RELEASE(view);
  nsRect webBounds(0, 0, NS_TO_INT_ROUND(aSize.width * t2p), 
                   NS_TO_INT_ROUND(aSize.height * t2p));

  mWebShell->Init(widget->GetNativeData(NS_NATIVE_WIDGET), webBounds, 
                   content->GetScrolling());
  NS_RELEASE(content);
  NS_RELEASE(widget);

  mWebShell->Show();

  return NS_OK;
}

NS_IMETHODIMP
nsHTMLFrameInnerFrame::Reflow(nsIPresContext&      aPresContext,
                              nsReflowMetrics&     aDesiredSize,
                              const nsReflowState& aReflowState,
                              nsReflowStatus&      aStatus)
{
  nsresult rv = NS_OK;

  // use the max size set in aReflowState by the nsHTMLFrameOuterFrame as our size
  if (!mCreatingViewer) {
    nsHTMLFrame* content;
    GetParentContent(content);

    nsAutoString url;
    content->GetURL(url);
    nsSize size;
 
    if (nsnull == mWebShell) {
      rv = CreateWebShell(aPresContext, aReflowState.maxSize);
    }

    if (nsnull != mWebShell) {
      mCreatingViewer=PR_TRUE;

      // load the document
      nsString absURL;
      TempMakeAbsURL(content, url, absURL);

      rv = mWebShell->LoadURL(absURL,          // URL string
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
nsHTMLFrameInnerFrame::GetDesiredSize(nsIPresContext* aPresContext,
                                      const nsReflowState& aReflowState,
                                      nsReflowMetrics& aDesiredSize)
{
  // it must be defined, but not called
  NS_ASSERTION(0, "this should never be called");
  aDesiredSize.width   = 0;
  aDesiredSize.height  = 0;
  aDesiredSize.ascent  = 0;
  aDesiredSize.descent = 0;
}

/*******************************************************************************
 * nsHTMLFrame
 ******************************************************************************/
nsHTMLFrame::nsHTMLFrame(nsIAtom* aTag, PRBool aInline, nsIWebShell* aParentWebWidget)
  : nsHTMLContainer(aTag), mInline(aInline), mParentWebWidget(aParentWebWidget)
{
}

nsHTMLFrame::~nsHTMLFrame()
{
  mParentWebWidget = nsnull;
}

void nsHTMLFrame::SetAttribute(nsIAtom* aAttribute, const nsString& aString)
{
  nsHTMLValue val;
  if (ParseImageProperty(aAttribute, aString, val)) { // convert width or height to pixels
    nsHTMLTagContent::SetAttribute(aAttribute, val);
    return;
  }
  nsHTMLContainer::SetAttribute(aAttribute, aString);
}

void nsHTMLFrame::MapAttributesInto(nsIStyleContext* aContext, 
                                     nsIPresContext* aPresContext)
{
  MapImagePropertiesInto(aContext, aPresContext);
  MapImageBorderInto(aContext, aPresContext, nsnull);
}

nsresult
nsHTMLFrame::CreateFrame(nsIPresContext*  aPresContext,
                          nsIFrame*        aParentFrame,
                          nsIStyleContext* aStyleContext,
                          nsIFrame*&       aResult)
{
  nsIFrame* frame = new nsHTMLFrameOuterFrame(this, aParentFrame);
  if (nsnull == frame) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  aResult = frame;
  frame->SetStyleContext(aPresContext, aStyleContext);
  return NS_OK;
}

PRBool nsHTMLFrame::GetURL(nsString& aURLSpec)
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

PRBool nsHTMLFrame::GetName(nsString& aName)
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

nsScrollPreference nsHTMLFrame::GetScrolling()
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

PRBool nsHTMLFrame::GetFrameBorder()
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
NS_NewHTMLFrame(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIWebShell* aWebWidget)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsHTMLFrame(aTag, PR_FALSE, aWebWidget);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

nsresult
NS_NewHTMLIFrame(nsIHTMLContent** aInstancePtrResult,
                 nsIAtom* aTag, nsIWebShell* aWebWidget)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null ptr");
  if (nsnull == aInstancePtrResult) {
    return NS_ERROR_NULL_POINTER;
  }

  nsIHTMLContent* it = new nsHTMLFrame(aTag, PR_TRUE, aWebWidget);

  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return it->QueryInterface(kIHTMLContentIID, (void**) aInstancePtrResult);
}

/*******************************************************************************
 * FrameLoadingInfo
 ******************************************************************************/
FrameLoadingInfo::FrameLoadingInfo(const nsSize& aSize)
{
  NS_INIT_REFCNT();

  mFrameSize = aSize;
}

/*
 * Implementation of ISupports methods...
 */
NS_IMPL_ISUPPORTS(FrameLoadingInfo,kISupportsIID);

// XXX temp implementation

NS_IMPL_ADDREF(TempObserver);
NS_IMPL_RELEASE(TempObserver);

/*******************************************************************************
 * TempObserver
 ******************************************************************************/
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
#if 0
  fputs("[progress ", stdout);
  fputs(mURL, stdout);
  printf(" %d %d ", aProgress, aProgressMax);
  fputs(aMsg, stdout);
  fputs("]\n", stdout);
#endif
  return NS_OK;
}

NS_IMETHODIMP
TempObserver::OnStartBinding(nsIURL* aURL, const char *aContentType)
{
#if 0
  fputs("Loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
#endif
  return NS_OK;
}

NS_IMETHODIMP
TempObserver::OnStopBinding(nsIURL* aURL, PRInt32 status, const nsString& aMsg)
{
#if 0
  fputs("Done loading ", stdout);
  fputs(mURL, stdout);
  fputs("\n", stdout);
#endif
  return NS_OK;
}


