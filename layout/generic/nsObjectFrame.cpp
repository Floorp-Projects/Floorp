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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is Netscape Communications
 * Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Pierre Phaneuf <pp@ludusdesign.com>
 */
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsString.h"
#include "prmem.h"
#include "nsHTMLAtoms.h"
#include "nsIDocument.h"
#include "nsIURL.h"
#include "nsNetUtil.h"
#include "nsIPluginInstanceOwner.h"
#include "nsIHTMLContent.h"
#include "nsISupportsArray.h"
#include "plstr.h"
#include "nsILinkHandler.h"
#include "nsIJVMPluginTagInfo.h"
#include "nsIWebShell.h"
#include "nsINameSpaceManager.h"
#include "nsIEventListener.h"
#include "nsIStringStream.h" // for NS_NewCharInputStream
#include "nsITimer.h"
#include "nsITimerCallback.h"
#include "nsLayoutAtoms.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMElement.h"
#include "nsContentPolicyUtils.h"

// XXX For temporary paint code
#include "nsIStyleContext.h"

//~~~ For image mime types
#include "nsMimeTypes.h"

#include "nsObjectFrame.h"

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

  NS_IMETHOD GetURL(const char *aURL, const char *aTarget, void *aPostData, 
                    PRUint32 aPostDataLen, void *aHeadersData, 
                    PRUint32 aHeadersDataLen);

  NS_IMETHOD ShowStatus(const char *aStatusMsg);
  
  NS_IMETHOD GetDocument(nsIDocument* *aDocument);

  //nsIPluginTagInfo interface

  NS_IMETHOD GetAttributes(PRUint16& n, const char*const*& names,
                           const char*const*& values);

  NS_IMETHOD GetAttribute(const char* name, const char* *result);

  NS_IMETHOD GetDOMElement(nsIDOMElement* *result);

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
  
  void Paint(const nsRect& aDirtyRect, PRUint32 ndc = nsnull);

  // nsITimerCallback interface
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  
  void CancelTimer();
  
  //locals

  NS_IMETHOD Init(nsIPresContext *aPresContext, nsObjectFrame *aFrame);

  nsresult GetContainingBlock(nsIFrame *aFrame, nsIFrame **aContainingBlock);
  
  nsPluginPort* GetPluginPort();
  void ReleasePluginPort(nsPluginPort * pluginPort);//~~~

  void SetPluginHost(nsIPluginHost* aHost);

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
  char              *mDocumentBase;
  nsIWidget         *mWidget;
  nsIPresContext    *mContext;
  nsCOMPtr<nsITimer> mPluginTimer;
  nsIPluginHost     *mPluginHost;
};

  // Mac specific code to fix up port position and clip during paint
#ifdef XP_MAC
  static void DoMacFixUp(nsPluginWindow *pluginWindow, nsIWidget* aWidget);

  static void GetWidgetPosAndClip(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY, nsRect& aClipRect); 
#endif	// XP_MAC

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
static NS_DEFINE_IID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_IID(kIPluginHostIID, NS_IPLUGINHOST_IID);
static NS_DEFINE_IID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

PRIntn
nsObjectFrame::GetSkipSides() const
{
  return 0;
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
    PRInt32 iPointOffset = data.RFindChar('.');

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

NS_IMETHODIMP nsObjectFrame::SetInitialChildList(nsIPresContext* aPresContext,
                                                 nsIAtom*        aListName,
                                                 nsIFrame*       aChildList)
{
  // we don't want to call this if it is already set (image)
  nsresult rv = NS_OK;
  if(mFrames.IsEmpty())
    rv = nsObjectFrameSuper::SetInitialChildList(aPresContext, aListName, aChildList);
  return rv;
}

NS_IMETHODIMP 
nsObjectFrame::Init(nsIPresContext*  aPresContext,
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
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    nsIFrame * aNewFrame = nsnull;
    rv = NS_NewImageFrame(shell, &aNewFrame);
    if(rv != NS_OK)
      return rv;

    rv = aNewFrame->Init(aPresContext, aContent, this, aContext, aPrevInFlow);
    if(rv == NS_OK)
    {
      nsHTMLContainerFrame::CreateViewForFrame(aPresContext, aNewFrame, aContext, nsnull, PR_FALSE);
      mFrames.AppendFrame(this, aNewFrame);
    }
    else
      aNewFrame->Destroy(aPresContext);
  }

  return rv;
}

NS_IMETHODIMP
nsObjectFrame::Destroy(nsIPresContext* aPresContext)
{
  // we need to finish with the plugin before native window is destroyed
  // doing this in the destructor is too late.
  if(mInstanceOwner != nsnull)
  {
    nsIPluginInstance *inst;
    if(NS_OK == mInstanceOwner->GetInstance(inst))
    {
      PRBool doCache = PR_TRUE;
      PRBool doCallSetWindowAfterDestroy = PR_FALSE;

      // first, determine if the plugin wants to be cached
      inst->GetValue(nsPluginInstanceVariable_DoCacheBool, 
                     (void *) &doCache);
      if (!doCache) {
        // then determine if the plugin wants Destroy to be called after
        // Set Window.  This is for bug 50547.
        inst->GetValue(nsPluginInstanceVariable_CallSetWindowAfterDestroyBool, 
                       (void *) &doCallSetWindowAfterDestroy);
        if (doCallSetWindowAfterDestroy) {
          inst->Stop();
          inst->Destroy();
          inst->SetWindow(nsnull);
        }
        else {
          inst->SetWindow(nsnull);
          inst->Stop();
          inst->Destroy();
        }
      }
      else {
        inst->SetWindow(nsnull);
        inst->Stop();
      }
      NS_RELEASE(inst);
    }
  }
  return nsObjectFrameSuper::Destroy(aPresContext);
}

NS_IMETHODIMP
nsObjectFrame::GetFrameType(nsIAtom** aType) const
{
  NS_PRECONDITION(nsnull != aType, "null OUT parameter pointer");
  *aType = nsLayoutAtoms::objectFrame; 
  NS_ADDREF(*aType);
  return NS_OK;
}

#ifdef DEBUG
NS_IMETHODIMP
nsObjectFrame::GetFrameName(nsString& aResult) const
{
  return MakeFrameName("ObjectFrame", aResult);
}
#endif

nsresult
nsObjectFrame::CreateWidget(nsIPresContext* aPresContext,
                            nscoord aWidth,
                            nscoord aHeight,
                            PRBool aViewOnly)
{
#ifndef XP_MAC
  // Do not create a widget if 'hidden' (except for Mac, where we
  // always create a widget...)
  if (IsHidden())
    return NS_OK;
#endif

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

  GetParentWithView(aPresContext, &parWithView);
  parWithView->GetView(aPresContext, &parView);

  if (NS_OK == parView->GetViewManager(viewMan))
  {
  //  nsWidgetInitData* initData = GetWidgetInitData(aPresContext); // needs to be deleted
    // initialize the view as hidden since we don't know the (x,y) until Paint
    result = view->Init(viewMan, boundBox, parView, nsViewVisibility_kHide);
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

    // Turn off double buffering on the Mac. This depends on bug 49743 and partially
    // fixes 32327, 19931 amd 51787
#ifdef XP_MAC
    viewMan->AllowDoubleBuffering(PR_FALSE);
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
    GetOffsetFromView(aPresContext, origin, &parentWithView);
    viewMan->ResizeView(view, mRect.width, mRect.height);
    viewMan->MoveViewTo(view, origin.x, origin.y);
  }

  SetView(aPresContext, view);

exit:
  NS_IF_RELEASE(viewMan);  

  return result;
}

#define EMBED_DEF_WIDTH 240
#define EMBED_DEF_HEIGHT 200

void
nsObjectFrame::GetDesiredSize(nsIPresContext* aPresContext,
                              const nsHTMLReflowState& aReflowState,
                              nsHTMLReflowMetrics& aMetrics)
{
  // Determine our size stylistically
  PRBool haveWidth = PR_FALSE;
  PRBool haveHeight = PR_FALSE;
  PRUint32 width = EMBED_DEF_WIDTH;
  PRUint32 height = EMBED_DEF_HEIGHT;

  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedWidth) {
    aMetrics.width = aReflowState.mComputedWidth;
    haveWidth = PR_TRUE;
  }
  if (NS_UNCONSTRAINEDSIZE != aReflowState.mComputedHeight) {
    aMetrics.height = aReflowState.mComputedHeight;
    haveHeight = PR_TRUE;
  }

  if (IsHidden()) {
    // If we're hidden, then width and height are zero.
    haveWidth = haveHeight = PR_TRUE;
    aMetrics.width = aMetrics.height = 0;
  }
  else if(mInstanceOwner != nsnull)
  {
    // the first time, mInstanceOwner will be null, so we a temporary default

    // if no width and height attributes specified use embed_def_*.
    if(NS_OK != mInstanceOwner->GetWidth(&width))
    {
      width = EMBED_DEF_WIDTH;
  	  haveWidth = PR_FALSE;
    }
    else
	    haveWidth = PR_FALSE;

    if(NS_OK != mInstanceOwner->GetHeight(&height))
    {
      height = EMBED_DEF_HEIGHT;
  	  haveHeight = PR_FALSE;
    }
    else
	    haveHeight = PR_FALSE;
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


#define JAVA_CLASS_ID "8AD9C840-044E-11D1-B3E9-00805F499D93"

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsObjectFrame", aReflowState.reason);
  nsresult rv = NS_OK;
  char* mimeType = nsnull;
  PRUint32 buflen;

  // Get our desired size
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

  // could be an image
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
    mInstanceOwner->Init(aPresContext, this);

    nsCOMPtr<nsISupports>     container;
    nsCOMPtr<nsIPluginHost>   pluginHost;
    nsIURI* baseURL = nsnull;
    nsIURI* fullURL = nsnull;

    nsAutoString classid;
    PRInt32 nameSpaceID;

    // if we have a clsid, we're either an internal widget, an ActiveX control, or an applet
    mContent->GetNameSpaceID(nameSpaceID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(nameSpaceID, nsHTMLAtoms::classid, classid))
    {
      nsCID widgetCID;
	
	    PRBool bJavaObject = PR_FALSE;
	    PRBool bJavaPluginClsid = PR_FALSE;

	    if (classid.Find("java:") != -1)
	    {
	      classid.Cut(0, 5); // Strip off the "java:". What's left is the class file.
	      bJavaObject = PR_TRUE;
	    }

	    if (classid.Find("clsid:") != -1)
	    {
        classid.Cut(0, 6); // Strip off the "clsid:". What's left is the class ID.
	      bJavaPluginClsid = (classid.EqualsWithConversion(JAVA_CLASS_ID));
	    }

      // if we find "java:" in the class id, or we match the Java classid number, we have a java applet
      if(bJavaObject || bJavaPluginClsid)
      {
        mimeType = (char *)PR_Malloc(PL_strlen("application/x-java-vm") + 1);
        PL_strcpy(mimeType, "application/x-java-vm");

        if((rv = GetBaseURL(baseURL)) != NS_OK)
	        return rv;

        nsAutoString codeBase;
        if(NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase) &&
           !bJavaPluginClsid) 
		    {
          nsIURI* codeBaseURL = nsnull;
          rv = NS_NewURI(&codeBaseURL, codeBase, baseURL);
          if(rv == NS_OK)
          {
            NS_IF_RELEASE(baseURL);
            baseURL = codeBaseURL;
          }
        }

        // Create an absolute URL
        if(bJavaPluginClsid) {
          rv = NS_NewURI(&fullURL, classid, baseURL);
        }
        else
        {
          fullURL = baseURL;
          NS_IF_ADDREF(fullURL);
        }

        // get the nsIPluginHost interface
        pluginHost = do_GetService(kCPluginManagerCID);
        if (!pluginHost)
          return NS_ERROR_FAILURE;

        mInstanceOwner->SetPluginHost(pluginHost);
        rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, mimeType, fullURL);
      }
      else // otherwise, we're either an ActiveX control or an internal widget
      {
        // These are some builtin types that we know about for now.
        // (Eventually this will move somewhere else.)
        if (classid.EqualsWithConversion("browser"))
        {
          widgetCID = kCAppShellCID;
	        rv = InstantiateWidget(aPresContext, aMetrics, aReflowState, widgetCID);
        }
        else
        {
	      // if we haven't matched to an internal type, check to see if we have an ActiveX handler
	      // if not, create the default plugin
	        if((rv = GetBaseURL(baseURL)) != NS_OK)
	          return rv;

	        // if we have a codebase, add it to the fullURL
	        nsAutoString codeBase;
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) 
	        {
            nsIURI* codeBaseURL = nsnull;
            rv = NS_NewURI(&fullURL, codeBase, baseURL);
            if(rv == NS_OK)
            {
              NS_IF_RELEASE(baseURL);
              baseURL = codeBaseURL;
            }
          } else {
	          fullURL = baseURL;
              NS_IF_ADDREF(fullURL);
          }

          // get the nsIPluginHost interface
          pluginHost = do_GetService(kCPluginManagerCID);
          if (!pluginHost)
            return NS_ERROR_FAILURE;

          mInstanceOwner->SetPluginHost(pluginHost);
          if(pluginHost->IsPluginEnabledForType("application/x-oleobject") == NS_OK)
	          rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, "application/x-oleobject", fullURL);
          else if(pluginHost->IsPluginEnabledForType("application/oleobject") == NS_OK)
	          rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, "application/oleobject", fullURL);
	        else
	          rv = NS_ERROR_FAILURE;
        }
      }

	    NS_IF_RELEASE(baseURL);
	    NS_IF_RELEASE(fullURL);

      // finish up
      if(rv == NS_OK)
      {
        aStatus = NS_FRAME_COMPLETE;
        return NS_OK;
      }

      // if we got an error, we'll check for alternative content with CantRenderReplacedElement()
      nsIPresShell* presShell;
      aPresContext->GetShell(&presShell);
      presShell->CantRenderReplacedElement(aPresContext, this);
      NS_RELEASE(presShell);
      aStatus = NS_FRAME_COMPLETE;
      return NS_OK;
    }

	  // if we're here, the object is either an applet or a plugin

    nsIAtom* atom = nsnull;
    nsAutoString    src;

	  if((rv = GetBaseURL(baseURL)) != NS_OK)
		  return rv;

    // get the nsIPluginHost interface
    pluginHost = do_GetService(kCPluginManagerCID);
    if (!pluginHost)
      return NS_ERROR_FAILURE;

    mInstanceOwner->SetPluginHost(pluginHost);

    mContent->GetTag(atom);
	  // check if it's an applet
    if (atom == nsHTMLAtoms::applet && atom) 
	  {
      mimeType = (char *)PR_Malloc(PL_strlen("application/x-java-vm") + 1);
      PL_strcpy(mimeType, "application/x-java-vm");

      if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::code, src)) 
	    {

        nsAutoString codeBase;
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) 
		    {
          nsIURI* codeBaseURL = nsnull;
          rv = NS_NewURI(&codeBaseURL, codeBase, baseURL);
          if(rv == NS_OK)
          {
            NS_IF_RELEASE(baseURL);
            baseURL = codeBaseURL;
          }
        }

        // Create an absolute URL
        rv = NS_NewURI(&fullURL, src, baseURL);
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
        // Create an absolute URL
        rv = NS_NewURI(&fullURL, src, baseURL);
        if ( rv != NS_OK )
          // Failed to create URI, maybe because we didn't
          // reconize the protocol handler ==> treat like
          // no 'src' was specified in the embed tag
          fullURL = baseURL;
	    }
	    else if(NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttribute(kNameSpaceID_HTML, nsHTMLAtoms::data, src)) 
	    {
        // Create an absolute URL
        rv = NS_NewURI(&fullURL, src, baseURL);
        if ( rv != NS_OK )
          // Failed to create URI, maybe because we didn't
          // reconize the protocol handler ==> treat like
          // no 'data' was specified in the object tag
          fullURL = baseURL;

	    } else {// we didn't find a src or data param, so just set the url to the base
		  fullURL = baseURL;
		  NS_IF_ADDREF(fullURL);
		}

	    // if we didn't find the type, but we do have a src, we can determine the mimetype
	    // based on the file extension
      if(!mimeType && src.GetUnicode())
	    {
		    char* extension;
		    char* cString = src.ToNewCString();
		    extension = PL_strrchr(cString, '.');
		    if(extension)
			    ++extension;

		    pluginHost->IsPluginEnabledForExtension((const char *)extension, (const char *&)mimeType);

      delete [] cString;
	    }

	    rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, mimeType, fullURL);
    }

	  NS_IF_RELEASE(atom);
	  NS_IF_RELEASE(baseURL);
	  NS_IF_RELEASE(fullURL);
  }
  else
    rv = ReinstantiatePlugin(aPresContext, aMetrics, aReflowState);

  // finish up
  if(rv == NS_OK)
  {
    aStatus = NS_FRAME_COMPLETE;
    return NS_OK;
  }

  // if we got an error, we'll check for alternative content with CantRenderReplacedElement()
  nsIPresShell* presShell;
  aPresContext->GetShell(&presShell);
  presShell->CantRenderReplacedElement(aPresContext, this);
  NS_RELEASE(presShell);
  aStatus = NS_FRAME_COMPLETE;
  return NS_OK;
}

nsresult
nsObjectFrame::InstantiateWidget(nsIPresContext*          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsCID aWidgetCID)
{
  nsresult rv;

  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  nsIView *parentWithView;
  nsPoint origin;
  GetOffsetFromView(aPresContext, origin, &parentWithView);
  // Just make the frigging widget

  float           t2p;
  aPresContext->GetTwipsToPixels(&t2p);
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

  mWidget->Show(PR_TRUE);
  return rv;
}

nsresult
nsObjectFrame::InstantiatePlugin(nsIPresContext*          aPresContext,
							nsHTMLReflowMetrics&     aMetrics,
							const nsHTMLReflowState& aReflowState,
							nsIPluginHost* aPluginHost, 
							const char* aMimetype,
							nsIURI* aURL)
{
  nsIView *parentWithView;
  nsPoint origin;
  nsPluginWindow  *window;
  float           t2p;
  aPresContext->GetTwipsToPixels(&t2p);

  SetFullURL(aURL);

  // we need to recalculate this now that we have access to the nsPluginInstanceOwner
  // and its size info (as set in the tag)
  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  if (nsnull != aMetrics.maxElementSize) 
  {
    //XXX              AddBorderPaddingToMaxElementSize(borderPadding);
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  mInstanceOwner->GetWindow(window);

  GetOffsetFromView(aPresContext, origin, &parentWithView);
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

  // Check to see if content-policy wants to veto this
  if(aURL != nsnull)
  {
    PRBool shouldLoad = PR_TRUE; // default permit
    nsresult rv;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mContent, &rv);

    // For pinkerton: a symphony for string conversion, in 3 parts.
    nsXPIDLCString urlCString;
    aURL->GetSpec(getter_Copies(urlCString));
    nsAutoString url;
    url.AssignWithConversion((const char *)urlCString);
  
    if (NS_SUCCEEDED(rv) &&
        NS_SUCCEEDED(NS_CheckContentLoadPolicy(nsIContentPolicy::CONTENT_OBJECT,
                                               url, element, &shouldLoad)) &&
        !shouldLoad) {
      return NS_OK;
    }
  }

  return aPluginHost->InstantiateEmbededPlugin(aMimetype, aURL, mInstanceOwner);
}

// This is called when the page containing plugin is resized, and plugin has its dimensions
// specified in percentage, so it needs to follow the page on the fly.
nsresult
nsObjectFrame::ReinstantiatePlugin(nsIPresContext* aPresContext, nsHTMLReflowMetrics& aMetrics, const nsHTMLReflowState& aReflowState)
{
  nsIView *parentWithView;
  nsPoint origin;
  nsPluginWindow  *window;
  float           t2p;
  aPresContext->GetTwipsToPixels(&t2p);

  // we need to recalculate this now that we have access to the nsPluginInstanceOwner
  // and its size info (as set in the tag)
  GetDesiredSize(aPresContext, aReflowState, aMetrics);
  if (nsnull != aMetrics.maxElementSize) 
  {
    //XXX              AddBorderPaddingToMaxElementSize(borderPadding);
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }

  mInstanceOwner->GetWindow(window);

  GetOffsetFromView(aPresContext, origin, &parentWithView);

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

  return NS_OK;
}

nsresult
nsObjectFrame::HandleImage(nsIPresContext*          aPresContext,
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

  // adjust kidReflowState
  nsIHTMLContent* hc = nsnull;
  mContent->QueryInterface(kIHTMLContentIID, (void**) &hc);

  if(hc != nsnull)
  {
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);

    nsHTMLValue val;
    if(NS_CONTENT_ATTR_HAS_VALUE == hc->GetHTMLAttribute(nsHTMLAtoms::width, val))
    {
      if(eHTMLUnit_Pixel == val.GetUnit())
      {
        nscoord width = val.GetPixelValue();
        kidReflowState.mComputedWidth = NSIntPixelsToTwips(width, p2t);
      }
    }
    if(NS_CONTENT_ATTR_HAS_VALUE == hc->GetHTMLAttribute(nsHTMLAtoms::height, val))
    {
      if(eHTMLUnit_Pixel == val.GetUnit())
      {
        nscoord height = val.GetPixelValue();
        kidReflowState.mComputedHeight = NSIntPixelsToTwips(height, p2t);
      }
    }
  }

	ReflowChild(child, aPresContext, kidDesiredSize, kidReflowState, 0, 0, 0, status);
  FinishReflowChild(child, aPresContext, kidDesiredSize, 0, 0, 0);

	aMetrics.width = kidDesiredSize.width;
	aMetrics.height = kidDesiredSize.height;
	aMetrics.ascent = kidDesiredSize.height;
	aMetrics.descent = 0;

  aStatus = NS_FRAME_COMPLETE;
	return NS_OK;
}


nsresult
nsObjectFrame::GetBaseURL(nsIURI* &aURL)
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


PRBool
nsObjectFrame::IsHidden() const
{
	// check the style visibility first
  const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
	if (disp != nsnull)
  {
    if(!disp->IsVisibleOrCollapsed())
      return PR_TRUE;
  }

  nsCOMPtr<nsIAtom> tag;
  mContent->GetTag(*getter_AddRefs(tag));

  if (tag.get() != nsHTMLAtoms::object) {
    // The <object> tag doesn't support the 'hidden' attribute, but
    // everything else does...
    nsAutoString hidden;
    mContent->GetAttribute(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);

    // Yes, these are really the kooky ways that you could tell 4.x
    // not to hide the <embed> once you'd put the 'hidden' attribute
    // on the tag...
      // these |NS_ConvertASCIItoUCS2|s can't be |NS_LITERAL_STRING|s until |EqualsIgnoreCase| get's fixed
    if (hidden.Length() &&
        ! hidden.EqualsIgnoreCase(NS_ConvertASCIItoUCS2("false")) &&
        ! hidden.EqualsIgnoreCase(NS_ConvertASCIItoUCS2("no")) &&
        ! hidden.EqualsIgnoreCase(NS_ConvertASCIItoUCS2("off"))) {
      // The <embed> or <applet> is hidden.
      return PR_TRUE;
    }
  }

  return PR_FALSE;
}

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
nsObjectFrame::DidReflow(nsIPresContext* aPresContext,
                         nsDidReflowStatus aStatus)
{
  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (NS_FRAME_REFLOW_FINISHED == aStatus && !IsHidden()) {
    nsIView* view = nsnull;
    GetView(aPresContext, &view);
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
        aPresContext->GetTwipsToPixels(&t2p);
        nscoord           offx, offy;

        GetOffsetFromView(aPresContext, origin, &parentWithView);

#if 0
        // beard:  how do we get this?
        parentWithView->GetScrollOffset(&offx, &offy);
#else
        offx = offy = 0;
#endif

        window->x = NSTwipsToIntPixels(origin.x, t2p);
        window->y = NSTwipsToIntPixels(origin.y, t2p);
        // window->width = NSTwipsToIntPixels(aMetrics.width, t2p);
        // window->height = NSTwipsToIntPixels(aMetrics.height, t2p);

        // refresh the plugin port as well
        window->window = mInstanceOwner->GetPluginPort();

        // beard: to preserve backward compatibility with Communicator 4.X, the
        // clipRect must be in port coordinates.
#ifdef XP_MAC
        DoMacFixUp(window,mWidget);
#else
        // this is only well-defined on the Mac OS anyway, or perhaps for windowless plugins.
        window->clipRect.top = 0;
        window->clipRect.left = 0;
        window->clipRect.bottom = window->clipRect.top + window->height;
        window->clipRect.right = window->clipRect.left + window->width;
#endif

        if (NS_OK == mInstanceOwner->GetInstance(inst)) {
          inst->SetWindow(window);
          NS_RELEASE(inst);
        }

        //~~~
        mInstanceOwner->ReleasePluginPort((nsPluginPort *)window->window);

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
nsObjectFrame::Paint(nsIPresContext* aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect& aDirtyRect,
                     nsFramePaintLayer aWhichLayer)
{
	const nsStyleDisplay* disp = (const nsStyleDisplay*)mStyleContext->GetStyleData(eStyleStruct_Display);
	if ((disp != nsnull) && !disp->IsVisibleOrCollapsed()) {
		return NS_OK;
	}

	nsIFrame * child = mFrames.FirstChild();
	if (child != NULL) {	// This is an image
		nsObjectFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
		return NS_OK;
	}

#if defined (XP_MAC)
	// delegate all painting to the plugin instance.
	if ((NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) && (nsnull != mInstanceOwner)) {
		mInstanceOwner->Paint(aDirtyRect);
	}
#elif defined (XP_PC)
	if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) 
	{
		nsIPluginInstance * inst;
		if (NS_OK == GetPluginInstance(inst))
		{
			NS_RELEASE(inst);
			// Look if it's windowless
			nsPluginWindow * window;
			mInstanceOwner->GetWindow(window);
			if (window->type == nsPluginWindowType_Drawable)
			{
				PRUint32 hdc;
				aRenderingContext.RetrieveCurrentNativeGraphicData(&hdc);
				mInstanceOwner->Paint(aDirtyRect, hdc);
			}
		}
	}
#endif /* !XP_MAC */
	return NS_OK;
}

NS_IMETHODIMP
nsObjectFrame::HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent*     anEvent,
                          nsEventStatus*  anEventStatus)
{
   NS_ENSURE_ARG_POINTER(anEventStatus);
	nsresult rv = NS_OK;

//~~~
  //FIX FOR CRASHING WHEN NO INSTANCE OWVER
  if (!mInstanceOwner)
    return NS_ERROR_NULL_POINTER;
#ifdef XP_WIN
  nsPluginWindow * window;
  mInstanceOwner->GetWindow(window);
  if(window->type == nsPluginWindowType_Drawable)
  {
	  switch (anEvent->message) 
    {
      case NS_MOUSE_LEFT_BUTTON_DOWN:
      case NS_MOUSE_LEFT_BUTTON_UP:
      case NS_MOUSE_LEFT_DOUBLECLICK:
      case NS_MOUSE_RIGHT_BUTTON_DOWN:
      case NS_MOUSE_RIGHT_BUTTON_UP:
      case NS_MOUSE_RIGHT_DOUBLECLICK:
      case NS_MOUSE_MIDDLE_BUTTON_DOWN:
      case NS_MOUSE_MIDDLE_BUTTON_UP:
      case NS_MOUSE_MIDDLE_DOUBLECLICK:
      case NS_MOUSE_MOVE:
      case NS_KEY_UP:
      case NS_KEY_DOWN:
      case NS_GOTFOCUS:
      case NS_LOSTFOCUS:
      //case set cursor should be here too:
        *anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
        return rv;
      default:
        break;
    }
  }
  rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

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
		*anEventStatus = mInstanceOwner->ProcessEvent(*anEvent);
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
nsObjectFrame::SetFullURL(nsIURI* aURL)
{
  NS_IF_RELEASE(mFullURL);
  mFullURL = aURL;
  NS_IF_ADDREF(mFullURL);
  return NS_OK;
}

nsresult nsObjectFrame::GetFullURL(nsIURI*& aFullURL)
{
  aFullURL = mFullURL;
  NS_IF_ADDREF(aFullURL);
  return NS_OK;
}

nsresult nsObjectFrame::GetPluginInstance(nsIPluginInstance*& aPluginInstance)
{
  if(mInstanceOwner == nsnull)
    return NS_ERROR_NULL_POINTER;
  else
    return mInstanceOwner->GetInstance(aPluginInstance);
}

nsresult
NS_NewObjectFrame(nsIPresShell* aPresShell, nsIFrame** aNewFrame)
{
  NS_PRECONDITION(aNewFrame, "null OUT ptr");
  if (nsnull == aNewFrame) {
    return NS_ERROR_NULL_POINTER;
  }
  nsObjectFrame* it = new (aPresShell) nsObjectFrame;
  if (nsnull == it) {
    return NS_ERROR_OUT_OF_MEMORY;
  }
  *aNewFrame = it;
  return NS_OK;
}

// TODO: put this in a header file.
extern nsresult NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance);

nsresult
NS_GetObjectFramePluginInstance(nsIFrame* aFrame, nsIPluginInstance*& aPluginInstance)
{
  if(aFrame == nsnull)
    return NS_ERROR_NULL_POINTER;

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
  mDocumentBase = nsnull;
  mPluginHost = nsnull;
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  // shut off the timer.
  if (mPluginTimer != nsnull) {
    mPluginTimer->Cancel();
  }

  if (nsnull != mInstance)
  {
    mPluginHost->StopPluginInstance(mInstance);
    NS_RELEASE(mInstance);
  }

  NS_RELEASE(mPluginHost);
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

  if (nsnull != mDocumentBase)
  {
    nsCRT::free(mDocumentBase);
    mDocumentBase = nsnull;
  }

  NS_IF_RELEASE(mWidget);
  mContext = nsnull;
}

NS_IMPL_ADDREF(nsPluginInstanceOwner);
NS_IMPL_RELEASE(nsPluginInstanceOwner);

nsresult nsPluginInstanceOwner::QueryInterface(const nsIID& aIID,
                                                 void** aInstancePtrResult)
{
  NS_PRECONDITION(nsnull != aInstancePtrResult, "null pointer");

  if (nsnull == aInstancePtrResult)
    return NS_ERROR_NULL_POINTER;

  if (aIID.Equals(NS_GET_IID(nsIPluginInstanceOwner)))
  {
    *aInstancePtrResult = (void *)((nsIPluginInstanceOwner *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIPluginTagInfo)) || aIID.Equals(NS_GET_IID(nsIPluginTagInfo2)))
  {
    *aInstancePtrResult = (void *)((nsIPluginTagInfo2 *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIJVMPluginTagInfo)))
  {
    *aInstancePtrResult = (void *)((nsIJVMPluginTagInfo *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsIEventListener)))
  {
    *aInstancePtrResult = (void *)((nsIEventListener *)this);
    AddRef();
    return NS_OK;
  }

  if (aIID.Equals(NS_GET_IID(nsITimerCallback)))
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
            nsCOMPtr<nsIAtom> prefix;
            iContent->GetAttributeNameAt(index, nameSpaceID, atom,
                                         *getter_AddRefs(prefix));
            nsAutoString  value;
            if (NS_CONTENT_ATTR_HAS_VALUE == iContent->GetAttribute(nameSpaceID, atom, value)) {
              nsAutoString  name;
              atom->ToString(name);

/*       Changing to ToNewUTF8String addressing 17169, 39789

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
*/
              mAttrNames[mNumAttrs] = name.ToNewUTF8String();
              mAttrVals[mNumAttrs] = value.ToNewUTF8String();
              mNumAttrs++;
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

NS_IMETHODIMP nsPluginInstanceOwner::GetDOMElement(nsIDOMElement* *result)
{
  nsresult rv = NS_ERROR_FAILURE;

  *result = nsnull;

  if (nsnull != mOwner)
  {
    nsIContent  *cont;

    mOwner->GetContent(&cont);

    if (nsnull != cont)
    {
      rv = cont->QueryInterface(NS_GET_IID(nsIDOMElement), (void **)result);
      NS_RELEASE(cont);
    }
  }

  return rv;
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

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(const char *aURL, const char *aTarget, void *aPostData, PRUint32 aPostDataLen, void *aHeadersData, 
                    PRUint32 aHeadersDataLen)
{
  nsISupports     *container;
  nsILinkHandler  *lh;
  nsresult        rv;
  nsIView         *view;
  nsPoint         origin;

  if ((nsnull != mOwner) && (nsnull != mContext))
  {
    rv = mOwner->GetOffsetFromView(mContext, origin, &view);

    if (NS_OK == rv)
    {
      rv = mContext->GetContainer(&container);

      if (NS_OK == rv)
      {
        rv = container->QueryInterface(kILinkHandlerIID, (void **)&lh);

        if (NS_OK == rv)
        {
          nsAutoString  uniurl; uniurl.AssignWithConversion(aURL);
          nsAutoString  unitarget; unitarget.AssignWithConversion(aTarget);
          nsAutoString  fullurl;
          nsIURI* baseURL;

          nsCOMPtr<nsIDocument> doc;
          rv = GetDocument(getter_AddRefs(doc));
          if (NS_SUCCEEDED(rv) && doc)
          {
            baseURL = doc->GetDocumentURL(); // gets the document's url
          }
          else
          {
            mOwner->GetFullURL(baseURL); // gets the plugin's content url
          }

          // Create an absolute URL
          rv = NS_MakeAbsoluteURI(fullurl, uniurl, baseURL);

          NS_IF_RELEASE(baseURL);

          if (NS_OK == rv) {
            nsIContent* content = nsnull;
            mOwner->GetContent(&content);
            nsCOMPtr<nsISupports> result = nsnull;
            nsCOMPtr<nsIInputStream> postDataStream = nsnull;
            nsCOMPtr<nsIInputStream> headersDataStream = nsnull;
            if (aPostData) {
              NS_NewByteInputStream(getter_AddRefs(result),
                                    (const char *) aPostData, aPostDataLen);
              if (result) {
                postDataStream = do_QueryInterface(result, &rv);
              }
            }
            if (aHeadersData) {
              NS_NewByteInputStream(getter_AddRefs(result),
                                    (const char *) aHeadersData, aHeadersDataLen);
              if (result) {
                headersDataStream = do_QueryInterface(result, &rv);
              }
            }
            rv = lh->OnLinkClick(content, eLinkVerb_Replace, 
                                 fullurl.GetUnicode(), 
                                 unitarget.GetUnicode(), 
                                 postDataStream, headersDataStream);
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
    nsCOMPtr<nsISupports> cont;

    rv = mContext->GetContainer(getter_AddRefs(cont));

    if ((NS_OK == rv) && (nsnull != cont))
    {
      nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont));
      if (docShellItem)
      {
        nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
        docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));

        if(treeOwner)
        {
        nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner));

        if(browserChrome)
          {
          nsAutoString  msg; msg.AssignWithConversion(aStatusMsg);
          browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, msg.GetUnicode());
          }
        }
      }
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

/*       Changing to ToNewUTF8String addressing 17169, 39789

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
*/
                      mParamNames[mNumParams] = name.ToNewUTF8String();
                      mParamVals[mNumParams]  = val.ToNewUTF8String();
                      mNumParams++;
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
  nsresult rv = NS_OK;
  if (nsnull == mDocumentBase) {
    if (nsnull == mContext) {
      *result = nsnull;
      return NS_ERROR_FAILURE;
    }
    
    nsCOMPtr<nsIPresShell> shell;
    mContext->GetShell(getter_AddRefs(shell));

    nsCOMPtr<nsIDocument> doc;
    shell->GetDocument(getter_AddRefs(doc));

    nsCOMPtr<nsIURI> docURL( dont_AddRef(doc->GetDocumentURL()) );

    rv = docURL->GetSpec(&mDocumentBase);
  }
  if (rv == NS_OK)
    *result = mDocumentBase;
  return rv;
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
    {
      *result = 0;

      PRInt32 attr = atol(width);

      if(nsnull == strchr(width, '%'))
        *result = (PRUint32)attr;
      else
      {
        if(mContext == nsnull)
          return NS_ERROR_FAILURE;
        
        attr = (attr > 100) ? 100 : attr;
        attr = (attr < 0) ? 0 : attr;

        float t2p;

        mContext->GetTwipsToPixels(&t2p);

        nsRect rect;
        mContext->GetVisibleArea(rect);

        nscoord w = rect.width;

        if(mOwner == nsnull)
        {
          *result = NSTwipsToIntPixels(attr*w/100, t2p);
          return NS_OK;
        }

        // now make it nicely fit counting margins
        nsIFrame *parent;
        mOwner->GetParent(&parent);
        parent->GetRect(rect);

        w -= 2*rect.x;

        *result = NSTwipsToIntPixels(attr*w/100, t2p);
      }
    }
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
    {
      *result = 0;

      PRInt32 attr = atol(height);

      if(nsnull == strchr(height, '%'))
        *result = (PRUint32)attr;
      else
      {
        if(mContext == nsnull)
          return NS_ERROR_FAILURE;
        
        attr = (attr > 100) ? 100 : attr;
        attr = (attr < 0) ? 0 : attr;

        float t2p;

        mContext->GetTwipsToPixels(&t2p);

        nsRect rect;
        mContext->GetVisibleArea(rect);

        nscoord h = rect.height;

        if(mOwner == nsnull)
        {
          *result = NSTwipsToIntPixels(attr*h/100, t2p);
          return NS_OK;
        }

        // now make it nicely fit counting margins
        nsIFrame *containingBlock=nsnull;
        rv = GetContainingBlock(mOwner, &containingBlock);
        if (NS_SUCCEEDED(rv) && containingBlock)
        {
          containingBlock->GetRect(rect);
          h -= 2*rect.y;
        }
        *result = NSTwipsToIntPixels(attr*h/100, t2p);
      }
    }
    else
      *result = 0;
  }
  else
    *result = 0;

  return rv;
}

nsresult
nsPluginInstanceOwner::GetContainingBlock(nsIFrame *aFrame, 
                                          nsIFrame **aContainingBlock)
{
  NS_ENSURE_ARG_POINTER(aFrame);
  NS_ENSURE_ARG_POINTER(aContainingBlock);

  *aContainingBlock = nsnull;
  nsIFrame *containingBlock = aFrame;
  while (containingBlock) {
    PRBool isContainingBlock=PR_FALSE;
    nsresult rv = containingBlock->IsPercentageBase(isContainingBlock);
    NS_ENSURE_SUCCESS(rv, rv);
    if (isContainingBlock) 
    {
      *aContainingBlock = containingBlock;
      break;
    }
    containingBlock->GetParent(&containingBlock);
  }
  return NS_OK;
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

#if TARGET_CARBON
inline Boolean OSEventAvail(EventMask mask, EventRecord* event) { return EventAvail(mask, event); }
#endif

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

//~~~
#ifdef XP_WIN
		nsPluginEvent * pPluginEvent = (nsPluginEvent *)anEvent.nativeMsg;
		PRBool eventHandled = PR_FALSE;
		mInstance->HandleEvent(pPluginEvent, &eventHandled);
		if (eventHandled)
			rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}

// Paints are handled differently, so we just simulate an update event.

void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect, PRUint32 ndc)
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

    DoMacFixUp(&mPluginWindow,mWidget);
    
    mInstance->HandleEvent(&pluginEvent, &eventHandled);
	}
#endif

//~~~
#ifdef XP_WIN
	nsPluginEvent pluginEvent;
  pluginEvent.event = 0x000F; //!!! This is bad, but is it better to include <windows.h> for WM_PAINT only?
  pluginEvent.wParam = (uint32)ndc;
  pluginEvent.lParam = nsnull;
	PRBool eventHandled = PR_FALSE;
	mInstance->HandleEvent(&pluginEvent, &eventHandled);
#endif
}

// Here's how we give idle time to plugins.

NS_IMETHODIMP_(void) nsPluginInstanceOwner::Notify(nsITimer* /* timer */)
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

#ifndef REPEATING_TIMERS
  // reprime the timer? currently have to create a new timer for each call, which is
  // kind of wasteful. need to get periodic timers working on all platforms.
  nsresult rv;
  mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
  if (NS_SUCCEEDED(rv))
    mPluginTimer->Init(this, 1000 / 60);
#endif
}

void nsPluginInstanceOwner::CancelTimer()
{
	if (mPluginTimer) {
	    mPluginTimer->Cancel();
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
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  nsPluginPort* result = NULL;
	if (mWidget != NULL)
  {//~~~
#ifdef XP_WIN
    if(mPluginWindow.type == nsPluginWindowType_Drawable)
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_GRAPHIC);
    else
#endif
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
	}
	return result;
}

//~~~
void nsPluginInstanceOwner::ReleasePluginPort(nsPluginPort * pluginPort)
{
#ifdef XP_WIN
	if (mWidget != NULL)
  {
    if(mPluginWindow.type == nsPluginWindowType_Drawable)
      mWidget->FreeNativeData((HDC)pluginPort, NS_NATIVE_GRAPHIC);
  }
#endif
}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void)
{
  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;

  if (nsnull != mOwner)
  {
    // Create view if necessary

    mOwner->GetView(mContext, &view);

    if (nsnull == view)
    {
      PRBool    windowless;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

      rv = mOwner->CreateWidget(mContext,
                                mPluginWindow.width,
                                mPluginWindow.height,
                                windowless);
      if (NS_OK == rv)
      {
        mOwner->GetView(mContext, &view);
        if (view)
          view->GetWidget(mWidget);

        if (PR_TRUE == windowless)
        {
          mPluginWindow.type = nsPluginWindowType_Drawable;
          mPluginWindow.window = nsnull; //~~~ this needs to be a HDC according to the spec,
                                         // but I do not see the right way to release it
                                         // so let's postpone passing HDC till paint event
                                         // when it is really needed. Change spec?
        }
        else if (mWidget)
        {
          mWidget->Resize(mPluginWindow.width, mPluginWindow.height, PR_FALSE);
          mPluginWindow.window = GetPluginPort();
          mPluginWindow.type = nsPluginWindowType_Window;

#if defined(XP_MAC)
          DoMacFixUp(&mPluginWindow,mWidget);

          // start a periodic timer to provide null events to the plugin instance.
          mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
          if (rv == NS_OK)
	        rv = mPluginTimer->Init(this, 1000 / 60, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
#endif
        }
      }
    }
  }

  return rv;
}

void nsPluginInstanceOwner::SetPluginHost(nsIPluginHost* aHost)
{
  if(mPluginHost != nsnull)
    NS_RELEASE(mPluginHost);
 
  mPluginHost = aHost;
  NS_IF_ADDREF(mPluginHost);
}


  // Mac specific code to fix up the port location and clipping region
#ifdef XP_MAC
static void DoMacFixUp(nsPluginWindow *pluginWindow, nsIWidget* aWidget)
{
  if (aWidget == nsnull || pluginWindow == nsnull)
    return;

  nsPluginRect& clipRect = pluginWindow->clipRect;
  nscoord x,y; 
  nsRect rect(0,0,0,0);

  // find aWidget's absolute position (x,y) and the clipping region (rect)
  GetWidgetPosAndClip(aWidget,x,y,rect);

  // set the port location
  pluginWindow->x = x;
  pluginWindow->y = y;

  // fix up the clipping region
  clipRect.top = y;
  clipRect.left = x;
  clipRect.bottom = clipRect.top + rect.height;
  clipRect.right = clipRect.right + rect.width;
}

  // calculate the absolute position and clip for a widget 
  // and use other windows in calculating the clip
static void GetWidgetPosAndClip(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY,
                                nsRect& aClipRect) 
{
  aWidget->GetBounds(aClipRect); 
  aAbsX = aClipRect.x; 
  aAbsY = aClipRect.y; 
  
  nscoord ancestorX = -aClipRect.x, ancestorY = -aClipRect.y; 
  // Calculate clipping relative to the widget passed in 
  aClipRect.x = 0; 
  aClipRect.y = 0; 

   // Gather up the absolute position of the widget 
   // + clip window 
  nsCOMPtr<nsIWidget> widget = getter_AddRefs(aWidget->GetParent());
  while (widget != nsnull) { 
    nsRect wrect; 
    widget->GetClientBounds(wrect); 
    nscoord wx, wy; 
    wx = wrect.x; 
    wy = wrect.y; 
    wrect.x = ancestorX; 
    wrect.y = ancestorY; 
    aClipRect.IntersectRect(aClipRect, wrect); 
    aAbsX += wx; 
    aAbsY += wy; 
    widget = getter_AddRefs(widget->GetParent());
    if (widget == nsnull) { 
      // Don't include the top-level windows offset 
      // printf("Top level window offset %d %d\n", wx, wy); 
      aAbsX -= wx; 
      aAbsY -= wy; 
    } 
    ancestorX -=wx; 
    ancestorY -=wy; 
  } 

  aClipRect.x = aAbsX; 
  aClipRect.y = aAbsY; 

  //printf("--------------\n"); 
  //printf("Widget clip X %d Y %d rect %d %d %d %d\n", aAbsX, aAbsY,  aClipRect.x,  aClipRect.y, aClipRect.width,  aClipRect.height ); 
  //printf("--------------\n"); 
} 
#endif	// XP_MAC
