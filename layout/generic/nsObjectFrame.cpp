/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License.  You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied.  See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are Copyright (C) 1998
 * Netscape Communications Corporation.  All Rights Reserved.
 */
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsIContentConnector.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsString.h"
#include "nsIContentViewerContainer.h"
#include "prmem.h"
#include "nsHTMLAtoms.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsIURLGroup.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIHTMLContent.h"
#include "nsISupportsArray.h"
#include "plstr.h"
#include "nsILinkHandler.h"
#include "nsIJVMPluginTagInfo.h"
#include "nsIWebShell.h"
#include "nsIBrowserWindow.h"
#include "nsINameSpaceManager.h"
#include "nsIEventListener.h"
#include "nsITimer.h"
#include "nsITimerCallback.h"

// XXX For temporary paint code
#include "nsIStyleContext.h"

//~~~ For image mime types
#include "net.h"

//#define REFLOW_MODS

class nsObjectFrame;

class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIPluginTagInfo2,
                              public nsIJVMPluginTagInfo,
                              public nsIEventListener,
                              public nsITimerCallback
{
public:
  nsPluginInstanceOwner();
  virtual ~nsPluginInstanceOwner();

  NS_DECL_ISUPPORTS

  //nsIPluginInstanceOwner interface

  NS_IMETHOD SetInstance(nsIPluginInstance *aInstance);

  NS_IMETHOD GetInstance(nsIPluginInstance *&aInstance);

  NS_IMETHOD GetWindow(nsPluginWindow *&aWindow);

  NS_IMETHOD GetMode(nsPluginMode *aMode);

  NS_IMETHOD CreateWidget(void);

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget, void *aPostData);

  NS_IMETHOD ShowStatus(const char *aStatusMsg);
  
  NS_IMETHOD GetDocument(nsIDocument* *aDocument);

  //nsIPluginTagInfo interface

  NS_IMETHOD GetAttributes(PRUint16& n, const char*const*& names,
                           const char*const*& values);

  NS_IMETHOD GetAttribute(const char* name, const char* *result);

  //nsIPluginTagInfo2 interface

  NS_IMETHOD GetTagType(nsPluginTagType *result);

  NS_IMETHOD GetTagText(const char* *result);

  NS_IMETHOD GetParameters(PRUint16& n, const char*const*& names, const char*const*& values);

  NS_IMETHOD GetParameter(const char* name, const char* *result);
  
  NS_IMETHOD GetDocumentBase(const char* *result);
  
  NS_IMETHOD GetDocumentEncoding(const char* *result);
  
  NS_IMETHOD GetAlignment(const char* *result);
  
  NS_IMETHOD GetWidth(PRUint32 *result);
  
  NS_IMETHOD GetHeight(PRUint32 *result);
  
  NS_IMETHOD GetBorderVertSpace(PRUint32 *result);
  
  NS_IMETHOD GetBorderHorizSpace(PRUint32 *result);

  NS_IMETHOD GetUniqueID(PRUint32 *result);

  //nsIJVMPluginTagInfo interface

  NS_IMETHOD GetCode(const char* *result);

  NS_IMETHOD GetCodeBase(const char* *result);

  NS_IMETHOD GetArchive(const char* *result);

  NS_IMETHOD GetName(const char* *result);

  NS_IMETHOD GetMayScript(PRBool *result);
  
  //nsIEventListener interface
  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);
  
  void Paint(const nsRect& aDirtyRect);

  // nsITimerCallback interface
  virtual void Notify(nsITimer *timer);
  
  void CancelTimer();
  
  //locals

  NS_IMETHOD Init(nsIPresContext *aPresContext, nsObjectFrame *aFrame);
  
  nsPluginPort* GetPluginPort();

private:
  nsPluginWindow    mPluginWindow;
  nsIPluginInstance *mInstance;
  nsObjectFrame     *mOwner;
  PRInt32           mNumAttrs;
  char              **mAttrNames;
  char              **mAttrVals;
  PRInt32           mNumParams;
  char              **mParamNames;
  char              **mParamVals;
  nsIWidget         *mWidget;
  nsIPresContext    *mContext;
  nsITimer			*mPluginTimer;
};

#define nsObjectFrameSuper nsHTMLContainerFrame

class nsObjectFrame : public nsObjectFrameSuper {
public:
  //~~~
  NS_IMETHOD SetInitialChildList(nsIPresContext& aPresContext,
                                 nsIAtom*        aListName,
                                 nsIFrame*       aChildList);
  //~~~
  NS_IMETHOD Init(nsIPresContext&  aPresContext,
                  nsIContent*      aContent,
                  nsIFrame*        aParent,
                  nsIStyleContext* aContext,
                  nsIFrame*        aPrevInFlow);
  NS_IMETHOD Reflow(nsIPresContext&          aPresContext,
                    nsHTMLReflowMetrics&     aDesiredSize,
                    const nsHTMLReflowState& aReflowState,
                    nsReflowStatus&          aStatus);
  NS_IMETHOD DidReflow(nsIPresContext& aPresContext,
                       nsDidReflowStatus aStatus);
  NS_IMETHOD Paint(nsIPresContext& aPresContext,
                   nsIRenderingContext& aRenderingContext,
                   const nsRect& aDirtyRect,
                   nsFramePaintLayer aWhichLayer);

  NS_IMETHOD  HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent*     aEvent,
                          nsEventStatus&  aEventStatus);

  NS_IMETHOD Scrolled(nsIView *aView);
  NS_IMETHOD GetFrameName(nsString& aResult) const;

  //~~~
  NS_IMETHOD ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent);
  //local methods
  nsresult CreateWidget(nscoord aWidth, nscoord aHeight, PRBool aViewOnly);
  nsresult GetFullURL(nsIURL*& aFullURL);

  nsresult GetPluginInstance(nsIPluginInstance*& aPluginInstance);
  
  //~~~
  void IsSupportedImage(nsIContent* aContent, PRBool* aImage);

protected:
  virtual ~nsObjectFrame();

  //~~~
  virtual PRIntn GetSkipSides() const;

  virtual void GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aDesiredSize);


  nsresult SetFullURL(nsIURL* aURL);

#ifdef REFLOW_MODS
  nsresult InstantiateWidget(nsIPresContext&          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsCID aWidgetCID);

  nsresult InstantiatePlugin(nsIPresContext&          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsIPluginHost* aPluginHost, 
							const char* aMimetype,
							nsIURL* aURL);

  nsresult HandleImage(nsIPresContext&          aPresContext,
					   nsHTMLReflowMetrics&     aMetrics,
					   const nsHTMLReflowState& aReflowState,
					   nsReflowStatus&          aStatus,
					   nsIFrame* child);
 
  nsresult GetBaseURL(nsIURL* &aURL);
#endif

private:
  nsPluginInstanceOwner *mInstanceOwner;
  nsIURL                *mFullURL;
  nsIFrame              *mFirstChild;
  nsIWidget				*mWidget;
};

nsObjectFrame::~nsObjectFrame()
{
  // beard: stop the timer explicitly to reduce reference count.
  if (nsnull != mInstanceOwner)
    mInstanceOwner->CancelTimer();

  NS_IF_RELEASE(mWidget);
  NS_IF_RELEASE(mInstanceOwner);
  NS_IF_RELEASE(mFullURL);
}

static NS_DEFINE_IID(kISupportsIID, NS_ISUPPORTS_IID);
static NS_DEFINE_IID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_IID(kIViewIID, NS_IVIEW_IID);
static NS_DEFINE_IID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_IID(kIHTMLContentIID, NS_IHTMLCONTENT_IID);
static NS_DEFINE_IID(kILinkHandlerIID, NS_ILINKHANDLER_IID);
static NS_DEFINE_IID(kCTreeViewCID, NS_TREEVIEW_CID);
static NS_DEFINE_IID(kCToolbarCID, NS_TOOLBAR_CID);
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kIContentViewerContainerIID, NS_ICONTENT_VIEWER_CONTAINER_IID);

//~~~
PRIntn
nsObjectFrame::GetSkipSides() const
{
  return 0;
}

//~~~
NS_IMETHODIMP
nsObjectFrame::SetInitialChildList(nsIPresContext& aPresContext,
                                      nsIAtom*        aListName,
                                      nsIFrame*       aChildList)
{
  return NS_OK;
}

//~~~
#define IMAGE_EXT_GIF "gif"
#define IMAGE_EXT_JPG "jpg"
#define IMAGE_EXT_PNG "png"
#define IMAGE_EXT_XBM "xbm"

void nsObjectFrame::IsSupportedImage(nsIContent* aContent, PRBool* aImage)
{
  *aImage = PR_FALSE;

  if(aContent == NULL)
    return;

  nsAutoString type;
  nsresult rv = aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, type);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (type.Length() > 0)) 
  {
    // should be really call to imlib
    if(type.EqualsIgnoreCase(IMAGE_GIF) ||
       type.EqualsIgnoreCase(IMAGE_JPG) ||
       type.EqualsIgnoreCase(IMAGE_PJPG)||
       type.EqualsIgnoreCase(IMAGE_PNG) ||
       type.EqualsIgnoreCase(IMAGE_PPM) ||
       type.EqualsIgnoreCase(IMAGE_XBM) ||
       type.EqualsIgnoreCase(IMAGE_XBM2)||
       type.EqualsIgnoreCase(IMAGE_XBM3))
    {
      *aImage = PR_TRUE;
    }
    return;
  }

  nsAutoString data;
  rv = aContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, data);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0)) 
  {
    // should be really call to imlib
    nsAutoString ext;
    
    PRInt32 iLastCharOffset = data.Length() - 1;
    PRInt32 iPointOffset = data.RFind(".");

    if(iPointOffset != -1)
    {
      data.Mid(ext, iPointOffset + 1, iLastCharOffset - iPointOffset);

      if(ext.EqualsIgnoreCase(IMAGE_EXT_GIF) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_JPG) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_PNG) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_XBM))
      {
        *aImage = PR_TRUE;
      }
    }
    return;
  }
}

//~~~
NS_IMETHODIMP 
nsObjectFrame::Init(nsIPresContext&  aPresContext,
                    nsIContent*      aContent,
                    nsIFrame*        aParent,
                    nsIStyleContext* aContext,
                    nsIFrame*        aPrevInFlow)
{
  nsresult rv = nsObjectFrameSuper::Init(aPresContext, aContent, aParent, aContext, aPrevInFlow);

  if(rv != NS_OK)
    return rv;

  PRBool bImage = PR_FALSE;

  //Ideally should be call to imlib, when it is available
  // and even move this code to Reflow when the stream starts to come
  IsSupportedImage(aContent, &bImage);
  
  if(bImage)
  {
    nsIFrame * aNewFrame = nsnull;
    rv = NS_NewImageFrame(aNewFrame);
    if(rv != NS_OK)
      return rv;

    rv = aNewFrame->Init(aPresContext, aContent, this, aContext, aPrevInFlow);
    if(rv == NS_OK)
    {
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame, aContext, PR_FALSE);
      mFrames.AppendFrame(this, aNewFrame);
    }
    else
      aNewFrame->DeleteFrame(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP
nsObjectFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ObjectFrame", aResult);
}

nsresult
nsObjectFrame::CreateWidget(nscoord aWidth, nscoord aHeight, PRBool aViewOnly)
{
  nsIView* view;

  // Create our view and widget

  nsresult result = 
    nsComponentManager::CreateInstance(kViewCID, nsnull, kIViewIID,
                                 (void **)&view);
  if (NS_OK != result) {
    return result;
  }
  nsIViewManager *viewMan;    // need to release

  nsRect boundBox(0, 0, aWidth, aHeight); 

  nsIFrame* parWithView;
  nsIView *parView;

  GetParentWithView(&parWithView);
  parWithView->GetView(&parView);

  if (NS_OK == parView->GetViewManager(viewMan))
  {
  //  nsWidgetInitData* initData = GetWidgetInitData(*aPresContext); // needs to be deleted
    // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, nsnull,
                        nsViewVisibility_kHide);
  //  if (nsnull != initData) {
  //    delete(initData);
  //  }

    if (NS_OK != result) {
      result = NS_OK;       //XXX why OK? MMP
      goto exit;            //XXX sue me. MMP
    }

#if 0
    // set the content's widget, so it can get content modified by the widget
    nsIWidget *widget;
    result = GetWidget(view, &widget);
    if (NS_OK == result) {
      nsInput* content = (nsInput *)mContent; // change this cast to QueryInterface 
      content->SetWidget(widget);
      NS_IF_RELEASE(widget);
    } else {
      NS_ASSERTION(0, "could not get widget");
    }
#endif

    viewMan->InsertChild(parView, view, 0);

    result = view->CreateWidget(kWidgetCID);

    if (NS_OK != result) {
      result = NS_OK;       //XXX why OK? MMP
      goto exit;            //XXX sue me. MMP
    }
  }

  {
    //this is ugly. it was ripped off from didreflow(). MMP
    // Position and size view relative to its parent, not relative to our
    // parent frame (our parent frame may not have a view).

    nsIView* parentWithView;
    nsPoint origin;
    view->SetVisibility(nsViewVisibility_kShow);
    GetOffsetFromView(origin, &parentWithView);
    viewMan->ResizeView(view, mRect.width, mRect.height);
    viewMan->MoveViewTo(view, origin.x, origin.y);
  }

  SetView(view);

exit:
  NS_IF_RELEASE(viewMan);  

  return result;
}

#define EMBED_DEF_DIM 50

void
nsObjectFrame::GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  // Determine our size stylistically
  PRBool haveWidth = PR_FALSE;
  PRBool haveHeight = PR_FALSE;
  PRUint32 width = EMBED_DEF_DIM;
  PRUint32 height = EMBED_DEF_DIM;

  if (aReflowState.HaveFixedContentWidth()) {
    aMetrics.width = aReflowState.computedWidth;
    haveWidth = PR_TRUE;
  }
  if (aReflowState.HaveFixedContentHeight()) {
    aMetrics.height = aReflowState.computedHeight;
    haveHeight = PR_TRUE;
  }

  // the first time, mInstanceOwner will be null, so we a temporary default
  if(mInstanceOwner != nsnull)
  {
    mInstanceOwner->GetWidth(&width);
    mInstanceOwner->GetHeight(&height);
	// XXX this is temporary fix so plugins display until we support padding
	haveHeight = PR_FALSE;
	haveWidth = PR_FALSE;
  }


  // XXX Temporary auto-sizing logic
  if (!haveWidth) {
    if (haveHeight) {
      aMetrics.width = aMetrics.height;
    }
    else {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      aMetrics.width = NSIntPixelsToTwips(width, p2t);
    }
  }
  if (!haveHeight) {
    if (haveWidth) {
      aMetrics.height = aMetrics.width;
    }
    else {
      float p2t;
      aPresContext->GetScaledPixelsToTwips(&p2t);
      aMetrics.height = NSIntPixelsToTwips(height, p2t);
    }
  }
  aMetrics.ascent = aMetrics.height;
  aMetrics.descent = 0;
}

#ifndef REFLOW_MODS

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  // Get our desired size
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);

  // Handle the clsid instantiation of arbitrary widgets
  nsAutoString classid;
  PRInt32 nameSpaceID;
  mContent->GetNameSpaceID(nameSpaceID);
  if (NS_CONTENT_ATTR_HAS_VALUE == 
			 mContent->GetAttribute(nameSpaceID, nsHTMLAtoms::classid, classid))
  {
		// A widget with a certain classID may need to be instantiated.
		nsString protocol;
		classid.Left(protocol, 5);
		if (protocol == "clsid")
		{
			// We are looking at a class ID for an XPCOM object. We need to create the
			// widget that corresponds to this object.
			classid.Cut(0, 6); // Strip off the clsid:. What's left is the class ID.
			static NS_DEFINE_IID(kCTreeViewCID, NS_TREEVIEW_CID);
			static NS_DEFINE_IID(kCToolbarCID, NS_TOOLBAR_CID);
			static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
			static NS_DEFINE_IID(kIContentConnectorIID, NS_ICONTENTCONNECTOR_IID);
			
			nsCID aWidgetCID;
			// These are some builtin types that we know about for now.
			// (Eventually this will move somewhere else.)
			if (classid == "treeview")
				aWidgetCID = kCTreeViewCID;
			else if (classid == "toolbar")
				aWidgetCID = kCToolbarCID;
			else if (classid == "browser")
				aWidgetCID = kCAppShellCID;
			else
			{
				// parse it.
				char* buffer;
				PRInt32 buflen = classid.Length();

                if (buflen > 0) {
                  buffer = (char *)PR_Malloc(buflen + 1);

                  if (nsnull != buffer)
                    classid.ToCString(buffer, buflen + 1);
              
				  aWidgetCID.Parse(buffer);
				  PR_Free((void*)buffer);
				}
			}

			// let's try making a widget.
				
			if (mInstanceOwner == nsnull)
			{
				mInstanceOwner = new nsPluginInstanceOwner();
				NS_ADDREF(mInstanceOwner);
				mInstanceOwner->Init(&aPresContext, this);
				
			}

			if (mWidget == nsnull)
			{
				GetDesiredSize(&aPresContext, aReflowState, aMetrics);
				nsIView *parentWithView;
				nsPoint origin;
				GetOffsetFromView(origin, &parentWithView);
				// Just make the frigging widget.

				float           t2p;
        aPresContext.GetTwipsToPixels(&t2p);

				PRInt32 x = NSTwipsToIntPixels(origin.x, t2p);
				PRInt32 y = NSTwipsToIntPixels(origin.y, t2p);
				PRInt32 width = NSTwipsToIntPixels(aMetrics.width, t2p);
				PRInt32 height = NSTwipsToIntPixels(aMetrics.height, t2p);
				nsRect r = nsRect(x, y, width, height);
				
				static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
				nsresult rv = nsComponentManager::CreateInstance(aWidgetCID, nsnull, kIWidgetIID,
									   (void**)&mWidget);
				if(rv != NS_OK)
					return rv;

				nsIWidget *parent;
				parentWithView->GetOffsetFromWidget(nsnull, nsnull, parent);
				mWidget->Create(parent, r, nsnull, nsnull);

				// See if the widget implements the CONTENT CONNECTOR interface.  If it
				// does, we can hand it the content subtree for further processing.
				nsIContentConnector* cc;
	
				if (NS_OK == mWidget->QueryInterface(kIContentConnectorIID, (void**)&cc))
				{
				   cc->SetContentRoot(mContent);
				   NS_IF_RELEASE(cc);
				}
				mWidget->Show(PR_TRUE);
			}	
		}

		aStatus = NS_FRAME_COMPLETE;
		return NS_OK;
  }

  //~~~
  //This could be an image
  nsIFrame * child = mFrames.FirstChild();
  if(child != nsnull)
  {
    nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
    nsHTMLReflowMetrics kidDesiredSize(nsnull);

    nsReflowReason reflowReason;
    nsFrameState frameState;
    child->GetFrameState(&frameState);
    if (frameState & NS_FRAME_FIRST_REFLOW)
      reflowReason = eReflowReason_Initial;
    else
      reflowReason = eReflowReason_Resize;

    nsHTMLReflowState kidReflowState(aPresContext, aReflowState, child,
                                     availSize, reflowReason);

    nsReflowStatus status;

    ReflowChild(child, aPresContext, kidDesiredSize, kidReflowState, status);

    nsRect rect(0, 0, kidDesiredSize.width, kidDesiredSize.height);
    child->SetRect(rect);

    aMetrics.width = kidDesiredSize.width;
    aMetrics.height = kidDesiredSize.height;
    aMetrics.ascent = kidDesiredSize.height;
    aMetrics.descent = 0;

    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  // XXX deal with border and padding the usual way...wrap it up!

  nsresult rv = NS_OK;

  nsIAtom* atom;
  mContent->GetTag(atom);
  if ((nsnull != atom) && (nsnull == mInstanceOwner)) 
  {
    static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
    static NS_DEFINE_IID(kIContentViewerContainerIID, NS_ICONTENT_VIEWER_CONTAINER_IID);

    nsISupports               *container;
    nsIPluginHost             *pm;
    nsIContentViewerContainer *cv;

    mInstanceOwner = new nsPluginInstanceOwner();

    if (nsnull != mInstanceOwner) {
      NS_ADDREF(mInstanceOwner);
      mInstanceOwner->Init(&aPresContext, this);

      rv = aPresContext.GetContainer(&container);

      if (NS_OK == rv) {
        rv = container->QueryInterface(kIContentViewerContainerIID, (void **)&cv);

        if (NS_OK == rv) {
          rv = cv->QueryCapability(kIPluginHostIID, (void **)&pm);

          if (NS_OK == rv) {
            nsAutoString    type;
            char            *buf = nsnull;
            PRInt32         buflen;
            nsPluginWindow  *window;
            float           t2p;
            aPresContext.GetTwipsToPixels(&t2p);
            nsAutoString    src;
            nsIURL* fullURL = nsnull;
            nsIURL* baseURL = nsnull;

            nsIHTMLContent* htmlContent;
            if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) {
              htmlContent->GetBaseURL(baseURL);
              NS_RELEASE(htmlContent);
            }
            else {
              nsIDocument* doc = nsnull;
              if (NS_SUCCEEDED(mContent->GetDocument(doc))) {
                doc->GetBaseURL(baseURL);
                NS_RELEASE(doc);
              }
            }

            mInstanceOwner->GetWindow(window);

            if (atom == nsHTMLAtoms::applet) {
              buf = (char *)PR_Malloc(PL_strlen("application/x-java-vm") + 1);
              PL_strcpy(buf, "application/x-java-vm");

              if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::code, src)) {
                nsIURLGroup* group = nsnull;
                if (nsnull != baseURL) {
                  baseURL->GetURLGroup(&group);
                }

                nsAutoString codeBase;
                if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) {
                  nsIURL* codeBaseURL = nsnull;
                  rv = NS_NewURL(&codeBaseURL, codeBase, baseURL, nsnull, group);
                  NS_IF_RELEASE(baseURL);
                  baseURL = codeBaseURL;
                }

                // Create an absolute URL
                rv = NS_NewURL(&fullURL, src, baseURL, nsnull, group);

                SetFullURL(fullURL);

                NS_IF_RELEASE(group);
              }
            }
            else {
              mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, type);

              buflen = type.Length();

              if (buflen > 0) {
                buf = (char *)PR_Malloc(buflen + 1);

                if (nsnull != buf)
                  type.ToCString(buf, buflen + 1);
              }

              //stream in the object source if there is one...

              if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src) ||
		  NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src)) 
	      {
                nsIURLGroup* group = nsnull;
                if (nsnull != baseURL) 
		    baseURL->GetURLGroup(&group);

                // Create an absolute URL
                rv = NS_NewURL(&fullURL, src, baseURL, nsnull, group);

                SetFullURL(fullURL);

                NS_IF_RELEASE(group);
	      }
	    }

			// if there's no fullURL at this point, we need to set one
	    if(!fullURL && baseURL)
	    {
		SetFullURL(baseURL); 
		fullURL = baseURL;
	    }
	    nsIView *parentWithView;
            nsPoint origin;

            // we need to recalculate this now that we have access to the nsPluginInstanceOwner
            // and its size info (as set in the tag)
            GetDesiredSize(&aPresContext, aReflowState, aMetrics);
            if (nsnull != aMetrics.maxElementSize) {
//XXX              AddBorderPaddingToMaxElementSize(borderPadding);
              aMetrics.maxElementSize->width = aMetrics.width;
              aMetrics.maxElementSize->height = aMetrics.height;
            }

            GetOffsetFromView(origin, &parentWithView);

            window->x = NSTwipsToIntPixels(origin.x, t2p);
            window->y = NSTwipsToIntPixels(origin.y, t2p);
            window->width = NSTwipsToIntPixels(aMetrics.width, t2p);
            window->height = NSTwipsToIntPixels(aMetrics.height, t2p);
            window->clipRect.top = 0;
            window->clipRect.left = 0;

//              window->clipRect.top = NSTwipsToIntPixels(origin.y, t2p);
//              window->clipRect.left = NSTwipsToIntPixels(origin.x, t2p);

            window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
            window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
#ifdef XP_UNIX
            window->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif
            rv = pm->InstantiateEmbededPlugin(buf, fullURL, mInstanceOwner);
            NS_IF_RELEASE(fullURL);
            NS_IF_RELEASE(baseURL);

            PR_Free((void *)buf);

            NS_RELEASE(pm);
          }
          NS_RELEASE(cv);
        }
        NS_RELEASE(container);
      }
    }
    NS_RELEASE(atom);
  }

  if(rv == NS_OK)//~~~
  {
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  //~~~
  nsIPresShell* presShell;
  aPresContext.GetShell(&presShell);
  presShell->CantRenderReplacedElement(&aPresContext, this);
  NS_RELEASE(presShell);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

#else

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext&          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  nsresult rv;
  char* mimeType = nsnull;
  PRUint32 buflen;

  // Get our desired size
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);

  //~~~ could be an image
  nsIFrame * child = mFrames.FirstChild();
  if(child != nsnull)
  	return HandleImage(aPresContext, aMetrics, aReflowState, aStatus, child);

  // if mInstance is null, we need to determine what kind of object we are and instantiate ourselves
  if(!mInstanceOwner)
  {
	// XXX - do we need to create this for widgets as well?
    mInstanceOwner = new nsPluginInstanceOwner();
	  if(!mInstanceOwner)
		  return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mInstanceOwner);
    mInstanceOwner->Init(&aPresContext, this);

	  nsISupports               *container;
	  nsIPluginHost             *pluginHost;
	  nsIContentViewerContainer *cv;

	  nsAutoString classid;
	  PRInt32 nameSpaceID;
	  // if we have a clsid, we're either an internal widget or an ActiveX control
	  mContent->GetNameSpaceID(nameSpaceID);
	  if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(nameSpaceID, nsHTMLAtoms::classid, classid))
	  {
	    nsCID widgetCID;
	    classid.Cut(0, 6); // Strip off the clsid:. What's left is the class ID.

	    // These are some builtin types that we know about for now.
	    // (Eventually this will move somewhere else.)
	    if (classid == "treeview")
	    {
		    widgetCID = kCTreeViewCID;
		    rv = InstantiateWidget(aPresContext, aMetrics, aReflowState, widgetCID);
	    }
	    else if (classid == "toolbar")
	    {
	      widgetCID = kCToolbarCID;
		    rv = InstantiateWidget(aPresContext, aMetrics, aReflowState, widgetCID);
	    }
	    else if (classid == "browser")
	    {
	      widgetCID = kCAppShellCID;
		    rv = InstantiateWidget(aPresContext, aMetrics, aReflowState, widgetCID);
	    }
	    else
	    {
		  // if we haven't matched to an internal type, check to see if we have an ActiveX handler
		  // if not, create the default plugin
        nsIURL* baseURL;
		    nsIURL* fullURL;

		    if((rv = GetBaseURL(baseURL)) != NS_OK)
		      return rv;

		    nsIURLGroup* group = nsnull;
        if(nsnull != baseURL)
          baseURL->GetURLGroup(&group);

		    // if we have a codebase, add it to the fullURL
		    nsAutoString codeBase;
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) 
		    {
          nsIURL* codeBaseURL = nsnull;
          rv = NS_NewURL(&fullURL, codeBase, baseURL, nsnull, group);
        }
		    else
		      fullURL = baseURL;

		    NS_IF_RELEASE(group);

        // get the nsIPluginHost interface
     	  if((rv = aPresContext.GetContainer(&container)) != NS_OK)
		      return rv;
	      if((rv = container->QueryInterface(kIContentViewerContainerIID, (void **)&cv)) != NS_OK)
        {
			    NS_RELEASE(container); 
          return rv;
        }
		    if((rv = cv->QueryCapability(kIPluginHostIID, (void **)&pluginHost)) != NS_OK)
        {
			    NS_RELEASE(container); 
          NS_RELEASE(cv); 
          return rv;
        }

	      if(pluginHost->IsPluginEnabledForType("application/x-oleobject") == NS_OK)
		      rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, "application/x-oleobject", fullURL);
     	  else if(pluginHost->IsPluginEnabledForType("application/oleobject") == NS_OK)
		      rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, "application/oleobject", fullURL);
		    else if(pluginHost->IsPluginEnabledForType("*") == NS_OK)
		      rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, "*", fullURL);
	  	  else
		      rv = NS_ERROR_FAILURE;
  
		    NS_RELEASE(pluginHost);
		    NS_RELEASE(cv);
		    NS_RELEASE(container);
	    }

	    aStatus = NS_FRAME_COMPLETE;
	    return rv;
    }

	  // if we're here, the object is either an applet or a plugin

    nsIAtom* atom = nsnull;
	  nsIURL* baseURL = nsnull;
	  nsIURL* fullURL = nsnull;
    nsAutoString    src;

	  if((rv = GetBaseURL(baseURL)) != NS_OK)
		  return rv;

    // get the nsIPluginHost interface
    if((rv = aPresContext.GetContainer(&container)) != NS_OK)
		  return rv;
	  if((rv = container->QueryInterface(kIContentViewerContainerIID, (void **)&cv)) != NS_OK)
    {
		  NS_RELEASE(container); 
      return rv;
    }
    if((rv = cv->QueryCapability(kIPluginHostIID, (void **)&pluginHost)) != NS_OK)
    {
		  NS_RELEASE(container); 
      NS_RELEASE(cv); 
      return rv;
    }

    mContent->GetTag(atom);
	  // check if it's an applet
    if (atom == nsHTMLAtoms::applet && atom) 
	  {
      mimeType = (char *)PR_Malloc(PL_strlen("application/x-java-vm") + 1);
      PL_strcpy(mimeType, "application/x-java-vm");

      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::code, src)) 
	    {
        nsIURLGroup* group = nsnull;
        if (nsnull != baseURL)
          baseURL->GetURLGroup(&group);

        nsAutoString codeBase;
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) 
		    {
          nsIURL* codeBaseURL = nsnull;
          rv = NS_NewURL(&codeBaseURL, codeBase, baseURL, nsnull, group);
          NS_IF_RELEASE(baseURL);
          baseURL = codeBaseURL;
        }

        // Create an absolute URL
        rv = NS_NewURL(&fullURL, src, baseURL, nsnull, group);
        NS_IF_RELEASE(group);
      }

      rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, mimeType, fullURL);
    }
    else // traditional plugin
	  {
	    nsAutoString type;
      mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::type, type);

      buflen = type.Length();
      if (buflen > 0) 
	    {
        mimeType = (char *)PR_Malloc(buflen + 1);
        if (nsnull != mimeType)
          type.ToCString(mimeType, buflen + 1);
      }

      //stream in the object source if there is one...
      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::src, src)) 
	    {
        nsIURLGroup* group = nsnull;
        if (nsnull != baseURL)
          baseURL->GetURLGroup(&group);

        // Create an absolute URL
        rv = NS_NewURL(&fullURL, src, baseURL, nsnull, group);
        NS_IF_RELEASE(group);
	    }
	    else if(NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src)) 
	    {
        nsIURLGroup* group = nsnull;
        if (nsnull != baseURL)
          baseURL->GetURLGroup(&group);

        // Create an absolute URL
        rv = NS_NewURL(&fullURL, src, baseURL, nsnull, group);
        NS_IF_RELEASE(group);
	    }
	    else // we didn't find a src or data param, so just set the url to the base
		  fullURL = baseURL;

	    // if we didn't find the type, but we do have a src, we can determine the mimetype
	    // based on the file extension
      if(!mimeType && src)
	    {
		    char* extension;
		    char* cString = src.ToNewCString();
		    extension = PL_strrchr(cString, '.');
		    if(extension)
			    ++extension;

		    delete [] cString;

		    pluginHost->IsPluginEnabledForExtension(extension, mimeType);
	    }

	    rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, mimeType, fullURL);
    }

	  NS_IF_RELEASE(atom);
	  NS_IF_RELEASE(baseURL);
	  NS_IF_RELEASE(fullURL);
	  NS_IF_RELEASE(pluginHost);
	  NS_IF_RELEASE(cv);
	  NS_IF_RELEASE(container);

    if(rv == NS_OK)
    {
      aStatus = NS_FRAME_COMPLETE;
      return NS_OK;
    }
  }

  nsIPresShell* presShell;
  aPresContext.GetShell(&presShell);
  presShell->CantRenderReplacedElement(&aPresContext, this);
  NS_RELEASE(presShell);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

nsresult
nsObjectFrame::InstantiateWidget(nsIPresContext&          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsCID aWidgetCID)
{
  nsresult rv;

  GetDesiredSize(&aPresContext, aReflowState, aMetrics);
  nsIView *parentWithView;
  nsPoint origin;
  GetOffsetFromView(origin, &parentWithView);
  // Just make the frigging widget

  float           t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  PRInt32 x = NSTwipsToIntPixels(origin.x, t2p);
  PRInt32 y = NSTwipsToIntPixels(origin.y, t2p);
  PRInt32 width = NSTwipsToIntPixels(aMetrics.width, t2p);
  PRInt32 height = NSTwipsToIntPixels(aMetrics.height, t2p);
  nsRect r = nsRect(x, y, width, height);

  static NS_DEFINE_IID(kIWidgetIID, NS_IWIDGET_IID);
  if((rv = nsComponentManager::CreateInstance(aWidgetCID, nsnull, kIWidgetIID, (void**)&mWidget)) != NS_OK)
    return rv;

  nsIWidget *parent;
  parentWithView->GetOffsetFromWidget(nsnull, nsnull, parent);
  mWidget->Create(parent, r, nsnull, nsnull);

  // See if the widget implements the CONTENT CONNECTOR interface.  If it
  // does, we can hand it the content subtree for further processing.
  nsIContentConnector* cc;
  if ((rv = mWidget->QueryInterface(kIContentConnectorIID, (void**)&cc)) == NS_OK)
  {
    cc->SetContentRoot(mContent);
    NS_IF_RELEASE(cc); 
  }
  mWidget->Show(PR_TRUE);
  return rv;
}

nsresult
nsObjectFrame::InstantiatePlugin(nsIPresContext&          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsIPluginHost* aPluginHost, 
							const char* aMimetype,
							nsIURL* aURL)
{
  nsIView *parentWithView;
  nsPoint origin;
  nsPluginWindow  *window;
  float           t2p;
  aPresContext.GetTwipsToPixels(&t2p);

  SetFullURL(aURL);

  // we need to recalculate this now that we have access to the nsPluginInstanceOwner
  // and its size info (as set in the tag)
  GetDesiredSize(&aPresContext, aReflowState, aMetrics);
  if (nsnull != aMetrics.maxElementSize) 
  {
    //XXX              AddBorderPaddingToMaxElementSize(borderPadding);
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  mInstanceOwner->GetWindow(window);

  GetOffsetFromView(origin, &parentWithView);
  window->x = NSTwipsToIntPixels(origin.x, t2p);
  window->y = NSTwipsToIntPixels(origin.y, t2p);
  window->width = NSTwipsToIntPixels(aMetrics.width, t2p);
  window->height = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.top = 0;
  window->clipRect.left = 0;
  window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);

#ifdef XP_UNIX
  window->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif
  return aPluginHost->InstantiateEmbededPlugin(aMimetype, aURL, mInstanceOwner);
}

nsresult
nsObjectFrame::HandleImage(nsIPresContext&          aPresContext,
						   nsHTMLReflowMetrics&     aMetrics,
						   const nsHTMLReflowState& aReflowState,
						   nsReflowStatus&          aStatus,
						   nsIFrame* child)
{
	nsSize availSize(NS_UNCONSTRAINEDSIZE, NS_UNCONSTRAINEDSIZE);
	nsHTMLReflowMetrics kidDesiredSize(nsnull);

	nsReflowReason reflowReason;
	nsFrameState frameState;
	child->GetFrameState(&frameState);
	if (frameState & NS_FRAME_FIRST_REFLOW)
	  reflowReason = eReflowReason_Initial;
	else
	  reflowReason = eReflowReason_Resize;

	nsHTMLReflowState kidReflowState(aPresContext, aReflowState, child,
									 availSize, reflowReason);

	nsReflowStatus status;

	if(PR_TRUE)//reflowReason == eReflowReason_Initial)
	{
	  kidDesiredSize.width = NS_UNCONSTRAINEDSIZE;
	  kidDesiredSize.height = NS_UNCONSTRAINEDSIZE;
	}

	ReflowChild(child, aPresContext, kidDesiredSize, kidReflowState, status);

	nsRect rect(0, 0, kidDesiredSize.width, kidDesiredSize.height);
	child->SetRect(rect);

	aMetrics.width = kidDesiredSize.width;
	aMetrics.height = kidDesiredSize.height;
	aMetrics.ascent = kidDesiredSize.height;
	aMetrics.descent = 0;

    aStatus = NS_FRAME_COMPLETE;
	return NS_OK;
}


nsresult
nsObjectFrame::GetBaseURL(nsIURL* &aURL)
{
  nsIHTMLContent* htmlContent;
  if (NS_SUCCEEDED(mContent->QueryInterface(kIHTMLContentIID, (void**)&htmlContent))) 
  {
    htmlContent->GetBaseURL(aURL);
    NS_RELEASE(htmlContent);
  }
  else 
  {
    nsIDocument* doc = nsnull;
    if (NS_SUCCEEDED(mContent->GetDocument(doc))) 
	{
      doc->GetBaseURL(aURL);
      NS_RELEASE(doc);
	}
	else
	  return NS_ERROR_FAILURE;
  }
  return NS_OK;
}

#endif
//~~~
NS_IMETHODIMP
nsObjectFrame::ContentChanged(nsIPresContext* aPresContext,
                            nsIContent*     aChild,
                            nsISupports*    aSubContent)
{
  // Generate a reflow command with this frame as the target frame
  nsCOMPtr<nsIPresShell> shell;
  nsresult rv = aPresContext->GetShell(getter_AddRefs(shell));
  if (NS_SUCCEEDED(rv) && shell) {
    nsIReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 nsIReflowCommand::ContentChanged);
    if (NS_SUCCEEDED(rv)) {
      shell->AppendReflowCommand(reflowCmd);
      NS_RELEASE(reflowCmd);
    }
  }
  return rv;
}

NS_IMETHODIMP
nsObjectFrame::DidReflow(nsIPresContext& aPresContext,
                         nsDidReflowStatus aStatus)
{
  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus) {
    nsIView* view = nsnull;
    GetView(&view);
    if (nsnull != view) {
      view->SetVisibility(nsViewVisibility_kShow);
    }

    if (nsnull != mInstanceOwner) {
      nsPluginWindow    *window;

      if (NS_OK == mInstanceOwner->GetWindow(window)) {
        nsIView           *parentWithView;
        nsPoint           origin;
        nsIPluginInstance *inst;
        float             t2p;
        aPresContext.GetTwipsToPixels(&t2p);
        nscoord           offx, offy;

        GetOffsetFromView(origin, &parentWithView);

#if 0
        parentWithView->GetScrollOffset(&offx, &offy);
#else
        offx = offy = 0;
#endif

        window->x = 0;
        window->y = 0;

        window->clipRect.top = 0;
        window->clipRect.left = 0;
//        window->clipRect.top = NSTwipsToIntPixels(origin.y, t2p);
//        window->clipRect.left = NSTwipsToIntPixels(origin.x, t2p);
//        window->clipRect.top = NSTwipsToIntPixels(origin.y + offy, t2p);
//        window->clipRect.left = NSTwipsToIntPixels(origin.x + offx, t2p);
        window->clipRect.bottom = window->clipRect.top + window->height;
        window->clipRect.right = window->clipRect.left + window->width;
        
        // refresh the plugin port as well
        window->window = mInstanceOwner->GetPluginPort();

        if (NS_OK == mInstanceOwner->GetInstance(inst)) {
          inst->SetWindow(window);
          NS_RELEASE(inst);
        }

		if (mWidget)
		{
			PRInt32 x = NSTwipsToIntPixels(origin.x, t2p);
			PRInt32 y = NSTwipsToIntPixels(origin.y, t2p);
			mWidget->Move(x, y);
		}
      }
    }
  }
  return rv;
}

NS_IMETHODIMP
nsObjectFrame::Paint(nsIPresContext& aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect,
                     nsFramePaintLayer aWhichLayer)
{
#if !defined(XP_MAC)
  //~~~
  nsIFrame * child = mFrames.FirstChild();
  if(child != NULL) //This is an image
  {
    nsObjectFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
    return NS_OK;
  }

  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    const nsStyleFont* font = (const nsStyleFont*)
      mStyleContext->GetStyleData(eStyleStruct_Font);

    aRenderingContext.SetFont(font->mFont);
    aRenderingContext.SetColor(NS_RGB(192, 192, 192));
    aRenderingContext.FillRect(0, 0, mRect.width, mRect.height);
    aRenderingContext.SetColor(NS_RGB(0, 0, 0));
    aRenderingContext.DrawRect(0, 0, mRect.width, mRect.height);
    float p2t;
    aPresContext.GetPixelsToTwips(&p2t);
    nscoord px3 = NSIntPixelsToTwips(3, p2t);
    nsAutoString tmp;
    nsIAtom* atom;
    mContent->GetTag(atom);
    if (nsnull != atom) {
      atom->ToString(tmp);
      NS_RELEASE(atom);
      aRenderingContext.DrawString(tmp, px3, px3);
    }
  }
#else
  // delegate all painting to the plugin instance.
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    mInstanceOwner->Paint(aDirtyRect);
  }
#endif /* !XP_MAC */
  return NS_OK;
}

NS_IMETHODIMP
nsObjectFrame::HandleEvent(nsIPresContext& aPresContext,
                          nsGUIEvent*     anEvent,
                          nsEventStatus&  anEventStatus)
{
	nsresult rv = NS_OK;
	
	switch (anEvent->message) {
	case NS_DESTROY:
		mInstanceOwner->CancelTimer();
		break;
	case NS_GOTFOCUS:
	case NS_LOSTFOCUS:
	case NS_KEY_UP:
	case NS_KEY_DOWN:
	case NS_MOUSE_MOVE:
	case NS_MOUSE_ENTER:
	case NS_MOUSE_LEFT_BUTTON_UP:
	case NS_MOUSE_LEFT_BUTTON_DOWN:
		anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
		break;
		
	default:
		// instead of using an event listener, we can dispatch events to plugins directly.
		rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
	}
	
	return rv;
}

nsresult
nsObjectFrame::Scrolled(nsIView *aView)
{
  return NS_OK;
}

nsresult
nsObjectFrame::SetFullURL(nsIURL* aURL)
{
  NS_IF_RELEASE(mFullURL);
  mFullURL = aURL;
  NS_IF_ADDREF(mFullURL);
  return NS_OK;
}

nsresult nsObjectFrame::GetFullURL(nsIURL*& aFullURL)
{
  aFullURL = mFullURL;
  NS_IF_ADDREF(aFullURL);
  return NS_OK;
}

nsresult nsObjectFrame::GetPluginInstance(nsIPluginInstance*& aPluginInstance)
{
  return mInstanceOwner->GetInstance(aPluginInstance);
}

nsresult
NS_NewObjectFrame(nsIFrame*& aFrameResult)
{
  aFrameResult = new nsObjectFrame;
  if (nsnull == aFrameResult) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  return NS_OK;
}

// TODO: put this in a header file.
extern nsresult NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance);

nsresult
NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance)
{
	// TODO: any way to determine this cast is safe?
	nsObjectFrame* objectFrame = NS_STATIC_CAST(nsObjectFrame*, aFrame);
	return objectFrame->GetPluginInstance(aPluginInstance);
}

//plugin instance owner

nsPluginInstanceOwner::nsPluginInstanceOwner()
{
  NS_INIT_REFCNT();

  memset(&mPluginWindow, 0, sizeof(mPluginWindow));
  mInstance = nsnull;
  mOwner = nsnull;
  mNumAttrs = 0;
  mAttrNames = nsnull;
  mAttrVals = nsnull;
  mWidget = nsnull;
  mContext = nsnull;
  mNumParams = 0;
  mParamNames = nsnull;
  mParamVals = nsnull;
  mPluginTimer = nsnull;
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  // shut off the timer.
  if (mPluginTimer != nsnull) {
    mPluginTimer->Cancel();
    NS_RELEASE(mPluginTimer);
  }

  if (nsnull != mInstance)
  {
    mInstance->Stop();
    mInstance->Destroy();
    NS_RELEASE(mInstance);
  }

  mOwner = nsnull;

  for (cnt = 0; cnt < mNumAttrs; cnt++)
  {
    if ((nsnull != mAttrNames) && (nsnull != mAttrNames[cnt]))
    {
      PR_Free(mAttrNames[cnt]);
      mAttrNames[cnt] = nsnull;
    }

    if ((nsnull != mAttrVals) && (nsnull != mAttrVals[cnt]))
    {
      PR_Free(mAttrVals[cnt]);
      mAttrVals[cnt] = nsnull;
    }
  }

  if (nsnull != mAttrNames)
  {
    PR_Free(mAttrNames);
    mAttrNames = nsnull;
  }

  if (nsnull != mAttrVals)
  {
    PR_Free(mAttrVals);
    mAttrVals = nsnull;
  }

  for (cnt = 0; cnt < mNumParams; cnt++)
  {
    if ((nsnull != mParamNames) && (nsnull != mParamNames[cnt]))
    {
      PR_Free(mParamNames[cnt]);
      mParamNames[cnt] = nsnull;
    }

    if ((nsnull != mParamVals) && (nsnull != mParamVals[cnt]))
    {
      PR_Free(mParamVals[cnt]);
      mParamVals[cnt] = nsnull;
    }
  }

  if (nsnull != mParamNames)
  {
    PR_Free(mParamNames);
    mParamNames = nsnull;
  }

  if (nsnull != mParamVals)
  {
    PR_Free(mParamVals);
    mParamVals = nsnull;
  }

  NS_IF_RELEASE(mWidget);
  mContext = nsnull;
}

static NS_DEFINE_IID(kIBrowserWindowIID, NS_IBROWSER_WINDOW_IID); 

NS_IMPL_ADDREF(nsPluginInstanceOwner);
NS_IMPL_RELEASE(nsPluginInstanceOwner);

nsresult nsPluginInstanceOwner::QueryInterface(const nsIID& aIID,
                                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(nsIPluginInstanceOwner::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIPluginInstanceOwner *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIPluginTagInfo::GetIID()) || aIID.Equals(nsIPluginTagInfo2::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIPluginTagInfo2 *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIJVMPluginTagInfo::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIJVMPluginTagInfo *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsIEventListener::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsIEventListener *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(nsITimerCallback::GetIID()))
  {
    *aInstancePtrResult = (void *)((nsITimerCallback *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(kISupportsIID))
  {
    *aInstancePtrResult = (void *)((nsISupports *)((nsIPluginTagInfo *)this));
    AddRef();
    return NS_OK;
  }

  return NS_NOINTERFACE;
}

NS_IMETHODIMP nsPluginInstanceOwner::SetInstance(nsIPluginInstance *aInstance)
{
  NS_IF_RELEASE(mInstance);
  mInstance = aInstance;
  NS_IF_ADDREF(mInstance);

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(nsPluginWindow *&aWindow)
{
  aWindow = &mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMode(nsPluginMode *aMode)
{
  *aMode = nsPluginMode_Embedded;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAttributes(PRUint16& n,
                                                     const char*const*& names,
                                                     const char*const*& values)
{
  nsresult    rv;
  nsIContent* iContent;

  if ((nsnull == mAttrNames) && (nsnull != mOwner)) {
    rv = mOwner->GetContent(&iContent);

    if (NS_SUCCEEDED(rv)) {
      PRInt32 count;

      if (NS_SUCCEEDED(iContent->GetAttributeCount(count))) {
        PRInt32 index;
        mAttrNames = (char **)PR_Calloc(sizeof(char *) * count, 1);
        mAttrVals = (char **)PR_Calloc(sizeof(char *) * count, 1);
        mNumAttrs = 0;

        if ((nsnull != mAttrNames) && (nsnull != mAttrVals)) {
          for (index = 0; index < count; index++) {
            PRInt32 nameSpaceID;
            nsIAtom* atom;
            iContent->GetAttributeNameAt(index, nameSpaceID, atom);
            nsAutoString  value;
            if (NS_CONTENT_ATTR_HAS_VALUE == iContent->GetAttribute(nameSpaceID, atom, value)) {
              nsAutoString  name;
              atom->ToString(name);

              mAttrNames[mNumAttrs] = (char *)PR_Malloc(name.Length() + 1);
              mAttrVals[mNumAttrs] = (char *)PR_Malloc(value.Length() + 1);

              if ((nsnull != mAttrNames[mNumAttrs]) &&
                  (nsnull != mAttrVals[mNumAttrs]))
              {
                name.ToCString(mAttrNames[mNumAttrs], name.Length() + 1);
                value.ToCString(mAttrVals[mNumAttrs], value.Length() + 1);

                mNumAttrs++;
              }
              else
              {
                if (nsnull != mAttrNames[mNumAttrs])
                {
                  PR_Free(mAttrNames[mNumAttrs]);
                  mAttrNames[mNumAttrs] = nsnull;
                }
                if (nsnull != mAttrVals[mNumAttrs])
                {
                  PR_Free(mAttrVals[mNumAttrs]);
                  mAttrVals[mNumAttrs] = nsnull;
                }
              }
            }
            NS_RELEASE(atom);
          }
        }
        else {
          rv = NS_ERROR_OUT_OF_MEMORY;
          if (nsnull != mAttrVals) {
            PR_Free(mAttrVals);
            mAttrVals = nsnull;
          }
          if (nsnull != mAttrNames) {
            PR_Free(mAttrNames);
            mAttrNames = nsnull;
          }
        }
      }
      NS_RELEASE(iContent);
    }
  }
  else {
    rv = NS_OK;
  }

  n = mNumAttrs;
  names = (const char **)mAttrNames;
  values = (const char **)mAttrVals;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAttribute(const char* name, const char* *result)
{
  if (nsnull == mAttrNames) {
    PRUint16  numattrs;
    const char * const *names, * const *vals;

    GetAttributes(numattrs, names, vals);
  }

  *result = NULL;

  for (int i = 0; i < mNumAttrs; i++) {
    if (0 == PL_strcasecmp(mAttrNames[i], name)) {
      *result = mAttrVals[i];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetInstance(nsIPluginInstance *&aInstance)
{
  if (nsnull != mInstance)
  {
    aInstance = mInstance;
    NS_ADDREF(mInstance);

    return NS_OK;
  }
  else
    return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(const char *aURL, const char *aTarget, void *aPostData)
{
  nsISupports     *container;
  nsILinkHandler  *lh;
  nsresult        rv;
  nsIView         *view;
  nsPoint         origin;

  if ((nsnull != mOwner) && (nsnull != mContext))
  {
    rv = mOwner->GetOffsetFromView(origin, &view);

    if (NS_OK == rv)
    {
      rv = mContext->GetContainer(&container);

      if (NS_OK == rv)
      {
        rv = container->QueryInterface(kILinkHandlerIID, (void **)&lh);

        if (NS_OK == rv)
        {
          nsAutoString  uniurl = nsAutoString(aURL);
          nsAutoString  unitarget = nsAutoString(aTarget);
          nsAutoString  fullurl;
          nsIURL* baseURL;

          mOwner->GetFullURL(baseURL);

          // Create an absolute URL
          rv = NS_MakeAbsoluteURL(baseURL, nsString(), uniurl, fullurl);

          NS_IF_RELEASE(baseURL);

          if (NS_OK == rv) {
            nsIContent* content = nsnull;
            mOwner->GetContent(&content);
            rv = lh->OnLinkClick(content, eLinkVerb_Replace, fullurl.GetUnicode(), unitarget.GetUnicode(), nsnull);
            NS_IF_RELEASE(content);
          }
          NS_RELEASE(lh);
        }

        NS_RELEASE(container);
      }
    }
  }
  else
    rv = NS_ERROR_FAILURE;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::ShowStatus(const char *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;

  if (nsnull != mContext)
  {
    nsISupports *cont;

    rv = mContext->GetContainer(&cont);

    if ((NS_OK == rv) && (nsnull != cont))
    {
      nsIWebShell *ws;

      rv = cont->QueryInterface(nsIWebShell::GetIID(), (void **)&ws);

      if (NS_OK == rv)
      {
        nsIWebShell *rootWebShell;

        ws->GetRootWebShell(rootWebShell);

        if (nsnull != rootWebShell)
        {
          nsIWebShellContainer *rootContainer;

          rv = rootWebShell->GetContainer(rootContainer);

          if (nsnull != rootContainer)
          {
            nsIBrowserWindow *browserWindow;

            if (NS_OK == rootContainer->QueryInterface(kIBrowserWindowIID, (void**)&browserWindow))
            {
              nsAutoString  msg = nsAutoString(aStatusMsg);

              rv = browserWindow->SetStatus(msg.GetUnicode());
              NS_RELEASE(browserWindow);
            }

            NS_RELEASE(rootContainer);
          }

          NS_RELEASE(rootWebShell);
        }

        NS_RELEASE(ws);
      }

      NS_RELEASE(cont);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDocument(nsIDocument* *aDocument)
{
  nsresult rv = NS_ERROR_FAILURE;
  if (nsnull != mContext) {
    nsCOMPtr<nsIPresShell> shell;
    mContext->GetShell(getter_AddRefs(shell));

    rv = shell->GetDocument(aDocument);
  }
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetTagType(nsPluginTagType *result)
{
  nsresult rv = NS_ERROR_FAILURE;

  *result = nsPluginTagType_Unknown;

  if (nsnull != mOwner)
  {
    nsIContent  *cont;

    mOwner->GetContent(&cont);

    if (nsnull != cont)
    {
      nsIAtom     *atom;

      cont->GetTag(atom);

      if (nsnull != atom)
      {
        if (atom == nsHTMLAtoms::applet)
          *result = nsPluginTagType_Applet;
        else if (atom == nsHTMLAtoms::embed)
          *result = nsPluginTagType_Embed;
        else if (atom == nsHTMLAtoms::object)
          *result = nsPluginTagType_Object;

        rv = NS_OK;
        NS_RELEASE(atom);
      }

      NS_RELEASE(cont);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetTagText(const char* *result)
{
printf("instance owner gettagtext called\n");
  *result = "";
  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameters(PRUint16& n, const char*const*& names, const char*const*& values)
{
  nsresult rv = NS_ERROR_FAILURE;

  if ((nsnull == mParamNames) && (nsnull != mOwner))
  {
    nsIContent  *cont;

    mOwner->GetContent(&cont);

    if (nsnull != cont)
    {
      PRBool  haskids = PR_FALSE;

      cont->CanContainChildren(haskids);

      if (PR_TRUE == haskids)
      {
        PRInt32 numkids, idx, numparams = 0;

        cont->ChildCount(numkids);

        //first pass, count number of param tags...

        for (idx = 0; idx < numkids; idx++)
        {
          nsIContent  *kid;

          cont->ChildAt(idx, kid);

          if (nsnull != kid)
          {
            nsIAtom     *atom;

            kid->GetTag(atom);

            if (nsnull != atom)
            {
              if (atom == nsHTMLAtoms::param)
                numparams++;

              NS_RELEASE(atom);
            }

            NS_RELEASE(kid);
          }
        }

        if (numparams > 0)
        {
          //now we need to create arrays
          //representing the parameter name/value pairs...

          mParamNames = (char **)PR_Calloc(sizeof(char *) * numparams, 1);
          mParamVals = (char **)PR_Calloc(sizeof(char *) * numparams, 1);

          if ((nsnull != mParamNames) && (nsnull != mParamVals))
          {
            for (idx = 0; idx < numkids; idx++)
            {
              nsIContent  *kid;

              cont->ChildAt(idx, kid);

              if (nsnull != kid)
              {
                nsIAtom     *atom;

                kid->GetTag(atom);

                if (nsnull != atom)
                {
                  if (atom == nsHTMLAtoms::param)
                  {
                    nsAutoString  val, name;

                    //add param to list...

                    if ((NS_CONTENT_ATTR_HAS_VALUE == kid->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::name, name)) &&
                        (NS_CONTENT_ATTR_HAS_VALUE == kid->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::value, val)))
                    {
                      mParamNames[mNumParams] = (char *)PR_Malloc(name.Length() + 1);
                      mParamVals[mNumParams] = (char *)PR_Malloc(val.Length() + 1);

                      if ((nsnull != mParamNames[mNumParams]) &&
                          (nsnull != mParamVals[mNumParams]))
                      {
                        name.ToCString(mParamNames[mNumParams], name.Length() + 1);
                        val.ToCString(mParamVals[mNumParams], val.Length() + 1);

                        mNumParams++;
                      }
                      else
                      {
                        if (nsnull != mParamNames[mNumParams])
                        {
                          PR_Free(mParamNames[mNumParams]);
                          mParamNames[mNumParams] = nsnull;
                        }

                        if (nsnull != mParamVals[mNumParams])
                        {
                          PR_Free(mParamVals[mNumParams]);
                          mParamVals[mNumParams] = nsnull;
                        }
                      }
                    }
                  }

                  NS_RELEASE(atom);
                }
              }

              NS_RELEASE(kid);
            }
          }
        }
      }

      rv = NS_OK;
      NS_RELEASE(cont);
    }
  }

  n = mNumParams;
  names = (const char **)mParamNames;
  values = (const char **)mParamVals;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameter(const char* name, const char* *result)
{
  PRInt32 count;

  if (nsnull == mParamNames)
  {
    PRUint16  numattrs;
    const char * const *names, * const *vals;

    GetParameters(numattrs, names, vals);
  }

  for (count = 0; count < mNumParams; count++)
  {
    if (0 == PL_strcasecmp(mParamNames[count], name))
    {
      *result = mParamVals[count];
      break;
    }
  }

  if (count >= mNumParams)
    *result = "";

  return NS_OK;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentBase(const char* *result)
{
  if (nsnull != mContext)
  {
    nsCOMPtr<nsIPresShell> shell;
    mContext->GetShell(getter_AddRefs(shell));

    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));

    nsCOMPtr<nsIURL> docURL( dont_AddRef(doc->GetDocumentURL()) );

    nsresult rv = docURL->GetSpec(result);

    return rv;
  }
  else
  {
    *result = "";
    return NS_ERROR_FAILURE;
  }
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentEncoding(const char* *result)
{
printf("instance owner getdocumentencoding called\n");
  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetAlignment(const char* *result)
{
  return GetAttribute("ALIGN", result);
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetWidth(PRUint32 *result)
{
  nsresult    rv;
  const char  *width;

  rv = GetAttribute("WIDTH", &width);

  if (NS_OK == rv)
  {
    if (*result != 0)
      *result = (PRUint32)atol(width);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetHeight(PRUint32 *result)
{
  nsresult    rv;
  const char  *height;

  rv = GetAttribute("HEIGHT", &height);

  if (NS_OK == rv)
  {
    if (*result != 0)
      *result = (PRUint32)atol(height);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderVertSpace(PRUint32 *result)
{
  nsresult    rv;
  const char  *vspace;

  rv = GetAttribute("VSPACE", &vspace);

  if (NS_OK == rv)
  {
    if (*result != 0)
      *result = (PRUint32)atol(vspace);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetBorderHorizSpace(PRUint32 *result)
{
  nsresult    rv;
  const char  *hspace;

  rv = GetAttribute("HSPACE", &hspace);

  if (NS_OK == rv)
  {
    if (*result != 0)
      *result = (PRUint32)atol(hspace);
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetUniqueID(PRUint32 *result)
{
  *result = (PRUint32)mContext;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetCode(const char* *result)
{
  return GetAttribute("CODE", result);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetCodeBase(const char* *result)
{
  return GetAttribute("CODEBASE", result);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetArchive(const char* *result)
{
  return GetAttribute("ARCHIVE", result);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetName(const char* *result)
{
  return GetAttribute("NAME", result);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMayScript(PRBool *result)
{
  const char* unused;
  *result = (GetAttribute("MAYSCRIPT", &unused) == NS_OK ? PR_TRUE : PR_FALSE);
  return NS_OK;
}

// Here's where we forward events to plugins.

#ifdef XP_MAC
static void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent)
{
	::OSEventAvail(0, &aMacEvent);
	
	switch (anEvent.message) {
	case NS_GOTFOCUS:
		aMacEvent.what = nsPluginEventType_GetFocusEvent;
		break;
	case NS_LOSTFOCUS:
		aMacEvent.what = nsPluginEventType_LoseFocusEvent;
		break;
	case NS_MOUSE_MOVE:
	case NS_MOUSE_ENTER:
		aMacEvent.what = nsPluginEventType_AdjustCursorEvent;
		break;
	default:
		aMacEvent.what = nullEvent;
		break;
	}
}
#endif

nsEventStatus nsPluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
{
	nsEventStatus rv = nsEventStatus_eIgnore;
#ifdef XP_MAC
	if (mInstance != NULL) {
		EventRecord* event = (EventRecord*)anEvent.nativeMsg;
		if (event == NULL || event->what == nullEvent) {
			EventRecord macEvent;
			GUItoMacEvent(anEvent, macEvent);
			event = &macEvent;
		}
		nsPluginPort* port = (nsPluginPort*)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
		nsPluginEvent pluginEvent = { event, nsPluginPlatformWindowRef(port->port) };
		PRBool eventHandled = PR_FALSE;
		mInstance->HandleEvent(&pluginEvent, &eventHandled);
		if (eventHandled)
			rv = nsEventStatus_eConsumeNoDefault;
	}
#endif
	return rv;
}

// Paints are handled differently, so we just simulate an update event.

void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect)
{
#ifdef XP_MAC
	if (mInstance != NULL) {
		nsPluginPort* pluginPort = GetPluginPort();

		EventRecord updateEvent;
		::OSEventAvail(0, &updateEvent);
		updateEvent.what = updateEvt;
		updateEvent.message = UInt32(pluginPort->port);
		
		nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(pluginPort->port) };
		PRBool eventHandled = PR_FALSE;
		mInstance->HandleEvent(&pluginEvent, &eventHandled);
	}
#endif
}

// Here's how we give idle time to plugins.

void nsPluginInstanceOwner::Notify(nsITimer* /* timer */)
{
#ifdef XP_MAC
	if (mInstance != NULL) {
		EventRecord idleEvent;
		::OSEventAvail(0, &idleEvent);
		idleEvent.what = nullEvent;
		
		nsPluginPort* pluginPort = GetPluginPort();
		nsPluginEvent pluginEvent = { &idleEvent, nsPluginPlatformWindowRef(pluginPort->port) };
		
		PRBool eventHandled = PR_FALSE;
		mInstance->HandleEvent(&pluginEvent, &eventHandled);
	}
#endif

  // reprime the timer? currently have to create a new timer for each call, which is
  // kind of wasteful. need to get periodic timers working on all platforms.
  NS_IF_RELEASE(mPluginTimer);
  if (NS_NewTimer(&mPluginTimer) == NS_OK)
    mPluginTimer->Init(this, 1000 / 60);
}

void nsPluginInstanceOwner::CancelTimer()
{
	if (mPluginTimer != NULL) {
	    mPluginTimer->Cancel();
    	NS_RELEASE(mPluginTimer);
    }
}

NS_IMETHODIMP nsPluginInstanceOwner::Init(nsIPresContext* aPresContext, nsObjectFrame *aFrame)
{
  //do not addref to avoid circular refs. MMP
  mContext = aPresContext;
  mOwner = aFrame;
  return NS_OK;
}

nsPluginPort* nsPluginInstanceOwner::GetPluginPort()
{
    // TODO: fix Windows widget code to support NS_NATIVE_PLUGIN_PORT selector.
	nsPluginPort* result = NULL;
	if (mWidget != NULL) {
		result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
	}
	return result;
}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void)
{
  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;

  if (nsnull != mOwner)
  {
    // Create view if necessary

    mOwner->GetView(&view);

    if (nsnull == view)
    {
      PRBool    windowless;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

      rv = mOwner->CreateWidget(mPluginWindow.width,
                                mPluginWindow.height,
                                windowless);
      if (NS_OK == rv)
      {
        nsIView   *view;

        mOwner->GetView(&view);
        view->GetWidget(mWidget);

        if (PR_TRUE == windowless)
        {
          mPluginWindow.window = nsnull;    //XXX this needs to be a HDC
          mPluginWindow.type = nsPluginWindowType_Drawable;
        }
        else
        {
          // mPluginWindow.window = (nsPluginPort *)mWidget->GetNativeData(NS_NATIVE_WINDOW);
          mPluginWindow.window = GetPluginPort();
          mPluginWindow.type = nsPluginWindowType_Window;

#if defined(XP_MAC)
          // start a periodic timer to provide null events to the plugin instance.
          rv = NS_NewTimer(&mPluginTimer);
          if (rv == NS_OK)
	        rv = mPluginTimer->Init(this, 1000 / 60);
#endif
        }
      }
    }
  }

  return rv;
}
