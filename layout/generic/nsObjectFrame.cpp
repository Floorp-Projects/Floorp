/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Pierre Phaneuf <pp@ludusdesign.com>
 *   Jacek Piskozub <piskozub@iopan.gda.pl>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */
#include "nscore.h"
#include "nsCOMPtr.h"
#include "nsHTMLParts.h"
#include "nsHTMLContainerFrame.h"
#include "nsIPresContext.h"
#include "nsIPresShell.h"
#include "nsWidgetsCID.h"
#include "nsViewsCID.h"
#include "nsIView.h"
#include "nsIViewManager.h"
#include "nsIDOMKeyListener.h"
#include "nsIPluginHost.h"
#include "nsplugin.h"
#include "nsString.h"
#include "nsReadableUtils.h"
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
#include "nsIDOMWindow.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDocumentEncoder.h"
#include "nsXPIDLString.h"
#include "nsIDOMRange.h"
#include "nsIPrintContext.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "npapi.h"
#include "nsIPrintOptions.h"
#include "nsGfxCIID.h"
#include "nsHTMLUtils.h"
static NS_DEFINE_CID(kPrintOptionsCID, NS_PRINTOPTIONS_CID);

// headers for plugin scriptability
#include "nsIScriptGlobalObject.h"
#include "nsIScriptContext.h"
#include "nsIXPConnect.h"
#include "nsIXPCScriptable.h"
#include "nsIClassInfo.h"
#include "jsapi.h"

// XXX temporary for Mac double buffering pref
#include "nsIPref.h"

// XXX For temporary paint code
#include "nsIStyleContext.h"

// For mime types
#include "nsMimeTypes.h"
#include "nsIMIMEService.h"
#include "nsCExternalHandlerService.h"
#include "nsICategoryManager.h"

#include "nsObjectFrame.h"
#include "nsIObjectFrame.h"
#include "nsIJVMManager.h"

#include "nsContentCID.h"
static NS_DEFINE_CID(kRangeCID,     NS_RANGE_CID);

// XXX temporary for Mac double buffering pref
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

/* X headers suck */
#ifdef KeyPress
#undef KeyPress
#endif

static nsresult GetContainingBlock(nsIFrame *aFrame, nsIFrame **aContainingBlock);

class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIPluginTagInfo2,
                              public nsIJVMPluginTagInfo,
                              public nsIEventListener,
                              public nsITimerCallback,
                              public nsIDOMMouseListener,
                              public nsIDOMMouseMotionListener,
                              public nsIDOMKeyListener,
                              public nsIDOMFocusListener
                              
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

  NS_IMETHOD ShowStatus(const PRUnichar *aStatusMsg);
  
  NS_IMETHOD GetDocument(nsIDocument* *aDocument);

  NS_IMETHOD InvalidateRect(nsPluginRect *invalidRect);

  NS_IMETHOD InvalidateRegion(nsPluginRegion invalidRegion);

  NS_IMETHOD ForceRedraw();

  NS_IMETHOD GetValue(nsPluginInstancePeerVariable variable, void *value);

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

 /** nsIDOMMouseListener interfaces 
  * @see nsIDOMMouseListener
  */
  NS_IMETHOD MouseDown(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseUp(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseDblClick(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD MouseOut(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent);     
  /* END interfaces from nsIDOMMouseListener*/

  // nsIDOMMouseListener intefaces
  NS_IMETHOD MouseMove(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragMove(nsIDOMEvent* aMouseEvent) { return NS_OK; }

  // nsIDOMKeyListener interfaces
  NS_IMETHOD KeyDown(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyUp(nsIDOMEvent* aKeyEvent);
  NS_IMETHOD KeyPress(nsIDOMEvent* aKeyEvent);
  // end nsIDOMKeyListener interfaces

  // nsIDOMFocuListener interfaces
  NS_IMETHOD Focus(nsIDOMEvent * aFocusEvent);
  NS_IMETHOD Blur(nsIDOMEvent * aFocusEvent);

  nsresult Destroy();  

  //nsIEventListener interface
  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);
  
  void Paint(const nsRect& aDirtyRect, PRUint32 ndc = nsnull);

  // nsITimerCallback interface
  NS_IMETHOD_(void) Notify(nsITimer *timer);
  
  void CancelTimer();
  
  //locals

  NS_IMETHOD Init(nsIPresContext *aPresContext, nsObjectFrame *aFrame);

  nsPluginPort* GetPluginPort();
  void ReleasePluginPort(nsPluginPort * pluginPort);

  void SetPluginHost(nsIPluginHost* aHost);

#ifdef XP_MAC
  void FixUpPluginWindow();
  void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent);
#endif

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
  char              *mTagText;
  nsIWidget         *mWidget;
  nsIPresContext    *mContext;
  nsCOMPtr<nsITimer> mPluginTimer;
  nsIPluginHost     *mPluginHost;
  PRPackedBool       mContentFocused;
  
  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);
};

  // Mac specific code to fix up port position and clip during paint
#ifdef XP_MAC
  // get the absolute widget position and clip
  static void GetWidgetPosAndClip(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY, nsRect& aClipRect); 
  static void ConvertTwipsToPixels(nsIPresContext& aPresContext, nsRect& aTwipsRect, nsRect& aPixelRect);
  // convert relative coordinates to absolute
  static void ConvertRelativeToWindowAbsolute(nsIFrame* aFrame, nsIPresContext* aPresContext, nsPoint& aRel, nsPoint& aAbs, nsIWidget *&aContainerWidget);
#endif // XP_MAC

nsObjectFrame::~nsObjectFrame()
{
  // beard: stop the timer explicitly to reduce reference count.
  if (nsnull != mInstanceOwner) {
    mInstanceOwner->CancelTimer();
    mInstanceOwner->Destroy();
  }

  NS_IF_RELEASE(mWidget);
  NS_IF_RELEASE(mInstanceOwner);
  NS_IF_RELEASE(mFullURL);

}

NS_IMETHODIMP
nsObjectFrame::QueryInterface(const nsIID& aIID, void** aInstancePtr)
{
  NS_ENSURE_ARG_POINTER(aInstancePtr);
  *aInstancePtr = nsnull;

#ifdef DEBUG
  if (aIID.Equals(NS_GET_IID(nsIFrameDebug))) {
    *aInstancePtr = NS_STATIC_CAST(nsIFrameDebug*,this);
    return NS_OK;
  }
#endif

  if (aIID.Equals(NS_GET_IID(nsIObjectFrame))) {
    *aInstancePtr = NS_STATIC_CAST(nsIObjectFrame*,this);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsIFrame))) {
    *aInstancePtr = NS_STATIC_CAST(nsIFrame*,this);
    return NS_OK;
  } else if (aIID.Equals(NS_GET_IID(nsISupports))) {
    *aInstancePtr = NS_STATIC_CAST(nsIObjectFrame*,this);
    return NS_OK;
  }
  return NS_NOINTERFACE;
}

NS_IMETHODIMP_(nsrefcnt) nsObjectFrame::AddRef(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

NS_IMETHODIMP_(nsrefcnt) nsObjectFrame::Release(void)
{
  NS_WARNING("not supported for frames");
  return 1;
}

static NS_DEFINE_CID(kViewCID, NS_VIEW_CID);
static NS_DEFINE_CID(kWidgetCID, NS_CHILD_CID);
static NS_DEFINE_CID(kCAppShellCID, NS_APPSHELL_CID);
static NS_DEFINE_CID(kCPluginManagerCID, NS_PLUGINMANAGER_CID);

PRIntn
nsObjectFrame::GetSkipSides() const
{
  return 0;
}

#define IMAGE_EXT_GIF "gif"
#define IMAGE_EXT_JPG "jpg"
#define IMAGE_EXT_PNG "png"
#define IMAGE_EXT_XBM "xbm"
#define IMAGE_EXT_BMP "bmp"
#define IMAGE_EXT_ICO "ico"

// #define DO_DIRTY_INTERSECT 1   // enable dirty rect intersection during paint

void nsObjectFrame::IsSupportedImage(nsIContent* aContent, PRBool* aImage)
{
  *aImage = PR_FALSE;

  if(aContent == NULL)
    return;

  nsAutoString type;
  nsresult rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::type, type);
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
       type.EqualsIgnoreCase(IMAGE_XBM3)||
       type.EqualsIgnoreCase(IMAGE_BMP) ||
       type.EqualsIgnoreCase(IMAGE_ICO))
    {
      *aImage = PR_TRUE;
    }
    return;
  }

  nsAutoString data;
  rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::data, data);

  PRBool havedata = (rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0);

  if(!havedata)
  {// try it once more for SRC attrubute
    rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::src, data);
    havedata = (rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0);
  }

  if(havedata) 
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
         ext.EqualsIgnoreCase(IMAGE_EXT_XBM) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_BMP) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_ICO))
      {
        *aImage = PR_TRUE;
      }
    }
    return;
  }
}

void nsObjectFrame::IsSupportedDocument(nsIContent* aContent, PRBool* aDoc)
{
  *aDoc = PR_FALSE;
  nsresult rv;
  
  if(aContent == nsnull)
    return;

  nsCOMPtr<nsICategoryManager> catman = do_GetService(NS_CATEGORYMANAGER_CONTRACTID, &rv);
  if (NS_FAILED(rv)) return;

  nsAutoString type;
  rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::type, type);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (type.Length() > 0)) 
  {
    nsXPIDLCString value;
    char * buf = ToNewCString(type);
    rv = catman->GetCategoryEntry("Gecko-Content-Viewers",buf, getter_Copies(value));
    nsMemory::Free(buf);

    if (NS_SUCCEEDED(rv) && value && *value && (value.Length() > 0))
      *aDoc = PR_TRUE;
    return;
  }

  // if we don't have a TYPE= try getting the mime-type via the DATA= url
  nsAutoString data;
  rv = aContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::data, data);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0)) 
  {
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIURI> baseURL;

    if (NS_FAILED(GetBaseURL(*getter_AddRefs(baseURL)))) return; // XXX NS_NewURI fails without base
    rv = NS_NewURI(getter_AddRefs(uri), data, baseURL);
    if (NS_FAILED(rv)) return;

    nsCOMPtr<nsIMIMEService> mimeService = do_GetService(NS_MIMESERVICE_CONTRACTID, &rv);
    if (NS_FAILED(rv)) return;
    
    char * contentType = nsnull;
    rv = mimeService->GetTypeFromURI(uri, &contentType);
    if (NS_FAILED(rv)) {
      if (contentType)
        nsMemory::Free(contentType);
      return;
    }

    nsXPIDLCString value;
    rv = catman->GetCategoryEntry("Gecko-Content-Viewers",contentType, getter_Copies(value));

    if (NS_SUCCEEDED(rv) && value && *value && (value.Length() > 0))
      *aDoc = PR_TRUE;

    if (contentType)
      nsMemory::Free(contentType);
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

  mPresContext = aPresContext; // weak ref

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

    return rv; // bail at this point
  }

  
  // only do the following for the object tag
  nsCOMPtr<nsIAtom> tag;
  aContent->GetTag(*getter_AddRefs(tag));
  if (tag.get() != nsHTMLAtoms::object) return rv;

  // for now, we should try to do the same for "document" types and create
  // and IFrame-like sub-frame
  PRBool bDoc = PR_FALSE;
  IsSupportedDocument(aContent, &bDoc);

  if(bDoc)
  {
    nsCOMPtr<nsIPresShell> shell;
    aPresContext->GetShell(getter_AddRefs(shell));
    nsIFrame * aNewFrame = nsnull;
    rv = NS_NewHTMLFrameOuterFrame(shell, &aNewFrame);
    if(NS_FAILED(rv))
      return rv;

    rv = aNewFrame->Init(aPresContext, aContent, this, aContext, aPrevInFlow);
    if(NS_SUCCEEDED(rv))
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

      nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(kCPluginManagerCID);
      if(pluginHost)
        pluginHost->StopPluginInstance(inst);

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
    nsComponentManager::CreateInstance(kViewCID, nsnull, NS_GET_IID(nsIView),
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
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
    PRBool doubleBuffer = PR_FALSE;
    prefs ? prefs->GetBoolPref("plugin.enable_double_buffer", &doubleBuffer) : 0;
    
    viewMan->AllowDoubleBuffering(doubleBuffer);
#endif

    viewMan->InsertChild(parView, view, 0);

    if(aViewOnly != PR_TRUE) {

      result = view->CreateWidget(kWidgetCID);

      if (NS_OK != result) {
        result = NS_OK;       //XXX why OK? MMP
        goto exit;            //XXX sue me. MMP
      }
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
  // By default, we have no area
  aMetrics.width = 0;
  aMetrics.height = 0;
  aMetrics.ascent = 0;
  aMetrics.descent = 0;

  // for EMBED and APPLET, default to 240x200 for compatibility
  nsIAtom *atom;
  mContent->GetTag(atom);
  if ( nsnull != atom &&
     ((atom == nsHTMLAtoms::applet) || (atom == nsHTMLAtoms::embed))) {
    
    float p2t;
    aPresContext->GetScaledPixelsToTwips(&p2t);
    aMetrics.width  = NSIntPixelsToTwips(EMBED_DEF_WIDTH,  p2t);
    aMetrics.height = NSIntPixelsToTwips(EMBED_DEF_HEIGHT, p2t);
  }

  // now find out size stylisticly
  const nsStylePosition*  position;
  GetStyleData(eStyleStruct_Position, (const nsStyleStruct*&)position);

  // width
  nsStyleUnit unit = position->mWidth.GetUnit();
  if (eStyleUnit_Coord == unit) {
    aMetrics.width = position->mWidth.GetCoordValue();
  }
  else if (eStyleUnit_Percent == unit) 
  {
    float factor = position->mWidth.GetPercentValue();

    if (NS_UNCONSTRAINEDSIZE != aReflowState.availableWidth)
      aMetrics.width = NSToCoordRound (factor * aReflowState.availableWidth);
    else {  // unconstrained percent case
      nsRect rect;
      aPresContext->GetVisibleArea(rect);
      nscoord w = rect.width;
      // now make it nicely fit counting margins
      nsIFrame *containingBlock = nsnull;
      if (NS_SUCCEEDED(GetContainingBlock(this, &containingBlock)) && containingBlock) {
        containingBlock->GetRect(rect);
        w -= 2*rect.x;
        // XXX: this math seems suspect to me.  Is the parent's margin really twice the x-offset?
        //      in CSS, a frame can have independent top and bottom margins
        // but for now, let's just copy what we had before
      }
      aMetrics.width = NSToCoordRound (factor * w);
    }
  }

  // height
  unit = position->mHeight.GetUnit();
  if (eStyleUnit_Coord == unit) {
    aMetrics.height = position->mHeight.GetCoordValue();
  }
  else if (eStyleUnit_Percent == unit) 
  {
    float factor = position->mHeight.GetPercentValue();

    if (NS_UNCONSTRAINEDSIZE != aReflowState.availableHeight)
      aMetrics.height = NSToCoordRound (factor * aReflowState.availableHeight);
    else {  // unconstrained percent case
      nsRect rect;
      aPresContext->GetVisibleArea(rect);
      nscoord h = rect.height;
      // now make it nicely fit counting margins
      nsIFrame *containingBlock = nsnull;
      if (NS_SUCCEEDED(GetContainingBlock(this, &containingBlock)) && containingBlock) {
        containingBlock->GetRect(rect);
        h -= 2*rect.y;
        // XXX: this math seems suspect to me.  Is the parent's margin really twice the x-offset?
        //      in CSS, a frame can have independent top and bottom margins
        // but for now, let's just copy what we had before
      }
      aMetrics.height = NSToCoordRound (factor * h);
    }
  }
  // accent
  aMetrics.ascent = aMetrics.height;
  
  if (aMetrics.width || aMetrics.height) {
    // Make sure that the other dimension is non-zero
    if (!aMetrics.width) aMetrics.width = 1;
    if (!aMetrics.height) aMetrics.height = 1;
  }

  if (nsnull != aMetrics.maxElementSize) {
    aMetrics.maxElementSize->width = aMetrics.width;
    aMetrics.maxElementSize->height = aMetrics.height;
  }
}

nsresult 
nsObjectFrame::MakeAbsoluteURL(nsIURI* *aFullURI, 
                              nsString aSrc,
                              nsIURI* aBaseURI)
{
  nsresult rv;
  nsCOMPtr<nsIDocument> document;
  rv = mInstanceOwner->GetDocument(getter_AddRefs(document));

  //trim leading and trailing whitespace
  aSrc.Trim("\b\t\r\n ", PR_TRUE, PR_TRUE, PR_FALSE);

  //create properly encoded absolute URI using the document charset
  nsXPIDLCString encodedURI;
  rv = NS_MakeAbsoluteURIWithCharset(getter_Copies(encodedURI),
                                     aSrc, document, aBaseURI,
                                     nsHTMLUtils::IOService,
                                     nsHTMLUtils::CharsetMgr);
  rv = NS_NewURI(aFullURI, encodedURI, aBaseURI);
  return rv;
}

#define JAVA_CLASS_ID "8AD9C840-044E-11D1-B3E9-00805F499D93"

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsObjectFrame", aReflowState.reason);
  DISPLAY_REFLOW(this, aReflowState, aMetrics, aStatus);
  nsresult rv = NS_OK;

  // Get our desired size
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

  // handle an image or iframes elsewhere
  nsIFrame * child = mFrames.FirstChild();
  if(child != nsnull)
    return HandleChild(aPresContext, aMetrics, aReflowState, aStatus, child);
  // if we are printing, bail for now
  nsCOMPtr<nsIPrintContext> thePrinterContext = do_QueryInterface(aPresContext);
  if (thePrinterContext) {
    aStatus = NS_FRAME_COMPLETE;
    return rv;
  }

  // if mInstance is null, we need to determine what kind of object we are and instantiate ourselves
  if (!mInstanceOwner) {
    // XXX - do we need to create this for widgets as well?
    mInstanceOwner = new nsPluginInstanceOwner();
    if(!mInstanceOwner) return NS_ERROR_OUT_OF_MEMORY;

    NS_ADDREF(mInstanceOwner);
    mInstanceOwner->Init(aPresContext, this);

    nsCOMPtr<nsISupports>     container;
    nsCOMPtr<nsIPluginHost>   pluginHost;
    nsCOMPtr<nsIURI> baseURL;
    nsCOMPtr<nsIURI> fullURL;

    nsAutoString classid;
    PRInt32 nameSpaceID;

    // if we have a clsid, we're either an internal widget, an ActiveX control, or an applet
    mContent->GetNameSpaceID(nameSpaceID);
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(nameSpaceID, nsHTMLAtoms::classid, classid)) {
      PRBool bJavaObject = PR_FALSE;
      PRBool bJavaPluginClsid = PR_FALSE;

      if (classid.Find("java:") != -1) {
        classid.Cut(0, 5); // Strip off the "java:". What's left is the class file.
        bJavaObject = PR_TRUE;
      }

      if (classid.Find("clsid:") != -1) {
        classid.Cut(0, 6); // Strip off the "clsid:". What's left is the class ID.
        bJavaPluginClsid = (classid.EqualsWithConversion(JAVA_CLASS_ID));
      }

      // if we find "java:" in the class id, or we match the Java
      // classid number, we have a java applet
      if(bJavaObject || bJavaPluginClsid) {
        if (NS_FAILED(rv = GetBaseURL(*getter_AddRefs(baseURL)))) return rv;

        nsAutoString codeBase;
        if ((NS_CONTENT_ATTR_HAS_VALUE == 
             mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) && 
            !bJavaPluginClsid) {
          nsCOMPtr<nsIURI> codeBaseURL;
          rv = MakeAbsoluteURL(getter_AddRefs(codeBaseURL), codeBase, baseURL);
          if (NS_SUCCEEDED(rv)) {
            baseURL = codeBaseURL;
          }
        }

        // Create an absolute URL
        if(bJavaPluginClsid) {
          rv = MakeAbsoluteURL(getter_AddRefs(fullURL), classid, baseURL);
        }
        else fullURL = baseURL;

        // get the nsIPluginHost interface
        pluginHost = do_GetService(kCPluginManagerCID);
        if (!pluginHost) return NS_ERROR_FAILURE;

        mInstanceOwner->SetPluginHost(pluginHost);
        rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState,
                               pluginHost, "application/x-java-vm", fullURL);
      }
      else { // otherwise, we're either an ActiveX control or an internal widget
        // These are some builtin types that we know about for now.
        // (Eventually this will move somewhere else.)
        if (classid.EqualsWithConversion("browser")) {
          rv = InstantiateWidget(aPresContext, aMetrics,
                                 aReflowState, kCAppShellCID);
        }
        else {
          // if we haven't matched to an internal type, check to see if
          // we have an ActiveX handler
          // if not, create the default plugin
          if (NS_FAILED(rv = GetBaseURL(*getter_AddRefs(baseURL)))) return rv;

          // if we have a codebase, add it to the fullURL
          nsAutoString codeBase;
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) {
            nsCOMPtr<nsIURI> codeBaseURL;
            rv = MakeAbsoluteURL(getter_AddRefs(fullURL), codeBase, baseURL);
            if (NS_SUCCEEDED(rv)) {
              baseURL = codeBaseURL;
            }
          } 
          else fullURL = baseURL;

          // get the nsIPluginHost interface
          pluginHost = do_GetService(kCPluginManagerCID);
          if (!pluginHost) return NS_ERROR_FAILURE;

          mInstanceOwner->SetPluginHost(pluginHost);
          if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForType("application/x-oleobject"))) {
            rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, 
                                   "application/x-oleobject", fullURL);
          }
          else if (NS_SUCCEEDED(pluginHost->IsPluginEnabledForType("application/oleobject"))) {
            rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState, pluginHost, 
                                   "application/oleobject", fullURL);
          }
          else rv = NS_ERROR_FAILURE;
        }
      }

      // finish up
      if (NS_SUCCEEDED(rv)) {
        aStatus = NS_FRAME_COMPLETE;
        return NS_OK;
      }
    }
    else { // the object is either an applet or a plugin
      nsAutoString    src;
      if (NS_FAILED(rv = GetBaseURL(*getter_AddRefs(baseURL)))) return rv;

      // get the nsIPluginHost interface
      pluginHost = do_GetService(kCPluginManagerCID);
      if (!pluginHost) return NS_ERROR_FAILURE;

      mInstanceOwner->SetPluginHost(pluginHost);

      nsCOMPtr<nsIAtom> tag;
      mContent->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsHTMLAtoms::applet) { 
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::code, src)) {
          nsAutoString codeBase;
          if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::codebase, codeBase)) {
            nsCOMPtr<nsIURI> codeBaseURL;
            rv = MakeAbsoluteURL(getter_AddRefs(codeBaseURL), codeBase, baseURL);
            if(rv == NS_OK) {
              baseURL = codeBaseURL;
            }
          }
          // Create an absolute URL
          rv = MakeAbsoluteURL(getter_AddRefs(fullURL), src, baseURL);
        }
        else
          fullURL = baseURL;

        rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState,
                               pluginHost, "application/x-java-vm", fullURL);
      } else { // traditional plugin
        nsXPIDLCString mimeTypeStr;
        nsAutoString type;
        mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::type, type);

        if (type.Length()) {
          mimeTypeStr.Adopt(ToNewCString(type));
        }
        //stream in the object source if there is one...
        if (NS_CONTENT_ATTR_HAS_VALUE ==
            mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::src, src)) {
          // Create an absolute URL
          rv = MakeAbsoluteURL(getter_AddRefs(fullURL), src, baseURL);
          if (NS_FAILED(rv)) {
            // Failed to create URI, maybe because we didn't
            // reconize the protocol handler ==> treat like
            // no 'src' was specified in the embed tag
            fullURL = baseURL;
          }
        }
        else if (NS_CONTENT_ATTR_HAS_VALUE ==
                 mContent->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::data, src)) {
          // Create an absolute URL
          rv = MakeAbsoluteURL(getter_AddRefs(fullURL), src, baseURL);
          if (NS_FAILED(rv)) {
            // Failed to create URI, maybe because we didn't
            // reconize the protocol handler ==> treat like
            // no 'data' was specified in the object tag
            fullURL = baseURL;
          }

        } 
        else {
          // we didn't find a src or data param, so just set the url to the base
          fullURL = baseURL;
        }

        // now try to instantiate a plugin instance based on a mime type
        const char* mimeType = mimeTypeStr.get();
        if (mimeType || (src.Length() > 0)) {
          if (!mimeType) {
            // we don't have a mime type, try to figure it out from extension
            nsXPIDLCString extension;
            PRInt32 offset = src.RFindChar(PRUnichar('.'));
            if (offset != kNotFound)
              *getter_Copies(extension) = ToNewCString(Substring(src, offset+1, src.Length()));
            pluginHost->IsPluginEnabledForExtension(extension, mimeType);
          }
          // if we fail to get a mime type from extension we can still try to 
          // instantiate plugin as it can be possible to determine it later
          rv = InstantiatePlugin(aPresContext, aMetrics, aReflowState,
                                 pluginHost, mimeType, fullURL);
        }
        else // if we have neither we should not bother
          rv = NS_ERROR_FAILURE;

      }
    }
  }
  else { // if (!mInstanceOwner)
    rv = ReinstantiatePlugin(aPresContext, aMetrics, aReflowState);
  }
  // finish up
  if (NS_FAILED(rv)) {
    // if we got an error, we'll check for alternative content with
    // CantRenderReplacedElement()
    nsIPresShell* presShell;
    aPresContext->GetShell(&presShell);
    rv = presShell->CantRenderReplacedElement(aPresContext, this);
    NS_RELEASE(presShell);
  } else {
    NotifyContentObjectWrapper();
  }
  aStatus = NS_FRAME_COMPLETE;

  return rv;
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

  if((rv = nsComponentManager::CreateInstance(aWidgetCID, nsnull, NS_GET_IID(nsIWidget), (void**)&mWidget)) != NS_OK)
    return rv;

  nsIWidget *parent;
  parentWithView->GetOffsetFromWidget(nsnull, nsnull, parent);
  mWidget->Create(parent, r, nsnull, nsnull);

  mWidget->Show(PR_TRUE);
  return rv;
}

nsresult
nsObjectFrame::InstantiatePlugin(nsIPresContext* aPresContext,
                                 nsHTMLReflowMetrics& aMetrics,
                                 const nsHTMLReflowState& aReflowState,
                                 nsIPluginHost* aPluginHost, 
                                 const char* aMimetype,
                                 nsIURI* aURI)
{
  nsIView *parentWithView;
  nsPoint origin;
  nsPluginWindow  *window;
  float           t2p;
  
  aPresContext->GetTwipsToPixels(&t2p);

  SetFullURL(aURI);

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

  // on the Mac we need to set the clipRect to { 0, 0, 0, 0 } for now. This will keep
  // us from drawing on screen until the widget is properly positioned, which will not
  // happen until we have finished the reflow process.
  window->clipRect.top = 0;
  window->clipRect.left = 0;
#ifndef XP_MAC
  window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
#else
  window->clipRect.bottom = 0;
  window->clipRect.right = 0;
#endif

#ifdef XP_UNIX
  window->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif

  // Check to see if content-policy wants to veto this
  if(aURI != nsnull)
  {
    PRBool shouldLoad = PR_TRUE; // default permit
    nsresult rv;
    nsCOMPtr<nsIDOMElement> element = do_QueryInterface(mContent, &rv);
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIPresShell> shell;
    rv = aPresContext->GetShell(getter_AddRefs(shell));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDocument> document;
    rv = shell->GetDocument(getter_AddRefs(document));
    if (NS_FAILED(rv)) return rv;

    if (! document)
      return NS_ERROR_FAILURE;

    nsCOMPtr<nsIScriptGlobalObject> globalScript;
    rv = document->GetScriptGlobalObject(getter_AddRefs(globalScript));
    if (NS_FAILED(rv)) return rv;

    nsCOMPtr<nsIDOMWindow> domWin(do_QueryInterface(globalScript));

    if (NS_SUCCEEDED(rv) &&
        NS_SUCCEEDED(NS_CheckContentLoadPolicy(nsIContentPolicy::OBJECT,
                                               aURI, element, domWin, &shouldLoad)) &&
        !shouldLoad) {
      return NS_OK;
    }
  }

#if defined(XP_UNIX) || defined(XP_OS2)
  // This is a work-around on Unix for a LiveConnect problem (bug 83698).
  // The problem:
  // The proxy JNI needs to be created by the browser. If it is created by
  // someone else (e.g., a plugin) on a different thread, the proxy JNI will
  // not work, and break LiveConnect.
  // Currently, on Unix, when instantiating a Java plugin instance (by calling
  // InstantiateEmbededPlugin() next), Java plugin will create the proxy JNI
  // if it is not created yet. If that happens, LiveConnect will be broken.
  // Before lazy start JVM was implemented, since at this point the browser
  // already created the proxy JNI buring startup, the problem did not happen.
  // But after the lazy start was implemented, at this point the proxy JNI was
  // not created yet, so the Java plugin created the proxy JNI, and broke
  // liveConnect.
  // On Windows and Mac, Java plugin does not create the proxy JNI, but lets
  // the browser to create it. Hence this is a Unix-only problem.
  //
  // The work-around:
  // The root cause of the problem is in Java plugin's Unix implementation,
  // which should not create the proxy JNI.
  // As a work-around, here we make sure the proxy JNI has been created by the
  // browser, before plugin gets a chance.
  //

  nsresult rv;
  // If Java is installed, get proxy JNI. Otherwise (for other plugins) keep
  // going.
  nsCOMPtr<nsIJVMManager> jvmManager = do_GetService(nsIJVMManager::GetCID(),
                                                     &rv);
  if (NS_SUCCEEDED(rv)) {
    JNIEnv* proxyEnv;
    // Get proxy JNI, if not created yet, create it.
    jvmManager->GetProxyJNI(&proxyEnv);
  }
#endif
  return aPluginHost->InstantiateEmbededPlugin(aMimetype, aURI, mInstanceOwner);
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

  // ignore this for now on the Mac because the widget is not properly positioned
  // yet and won't be until we have finished the reflow process.
#ifndef XP_MAC
  window->clipRect.top = 0;
  window->clipRect.left = 0;
  window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
#endif

#ifdef XP_UNIX
  window->ws_info = nsnull;   //XXX need to figure out what this is. MMP
#endif

  return NS_OK;
}

nsresult
nsObjectFrame::HandleChild(nsIPresContext*          aPresContext,
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
  mContent->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**) &hc);

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
  if (NS_SUCCEEDED(mContent->QueryInterface(NS_GET_IID(nsIHTMLContent), (void**)&htmlContent))) 
  {
    htmlContent->GetBaseURL(aURL);
    NS_RELEASE(htmlContent);
  }
  else 
  {
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    if (doc)
      doc->GetBaseURL(aURL);
    else
      return NS_ERROR_FAILURE;
  }
  return NS_OK;
}


PRBool
nsObjectFrame::IsHidden() const
{
    // check the style visibility first
  const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  if (vis != nsnull)
  {
    if(!vis->IsVisibleOrCollapsed())
      return PR_TRUE;
  }

  nsCOMPtr<nsIAtom> tag;
  mContent->GetTag(*getter_AddRefs(tag));

  if (tag.get() != nsHTMLAtoms::object) {
    // The <object> tag doesn't support the 'hidden' attribute, but
    // everything else does...
    nsAutoString hidden;
    mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);

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
        nscoord           offx = 0;
        nscoord           offy = 0;

        GetOffsetFromView(aPresContext, origin, &parentWithView);

        // if it's windowless we want to get the offset from the parent frame
        if (window->type == nsPluginWindowType_Drawable)
        {

          nsIWidget* aWidget;
          parentWithView->GetOffsetFromWidget(&offx, &offy, aWidget);

          nsIFrame* parentFrame;
          GetParentWithView(aPresContext, &parentFrame);

          if(parentFrame != nsnull)
            parentFrame->GetOffsetFromView(aPresContext, origin, &parentWithView);

        }

        window->x = NSTwipsToIntPixels(origin.x + offx, t2p);
        window->y = NSTwipsToIntPixels(origin.y + offy, t2p);
        // window->width = NSTwipsToIntPixels(aMetrics.width, t2p);
        // window->height = NSTwipsToIntPixels(aMetrics.height, t2p);

        // refresh the plugin port as well
        window->window = mInstanceOwner->GetPluginPort();

        // beard: to preserve backward compatibility with Communicator 4.X, the
        // clipRect must be in port coordinates.
#ifndef XP_MAC
       // this is only well-defined on the Mac OS anyway, or perhaps for windowless plugins.
        window->clipRect.top = 0;
        window->clipRect.left = 0;
        window->clipRect.bottom = window->clipRect.top + window->height;
        window->clipRect.right = window->clipRect.left + window->width;
#else
        // now that we have finished the reflow process, the widget is properly positioned
        // and we can validate the plugin clipping information by syncing the plugin
        // window info to reflect the current widget location.
        mInstanceOwner->FixUpPluginWindow();
#endif
        if (NS_OK == mInstanceOwner->GetInstance(inst)) {
          inst->SetWindow(window);
          NS_RELEASE(inst);
        }

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
nsObjectFrame::Paint(nsIPresContext*      aPresContext,
                     nsIRenderingContext& aRenderingContext,
                     const nsRect&        aDirtyRect,
                     nsFramePaintLayer    aWhichLayer,
                     PRUint32             aFlags)
{
  const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
  if ((vis != nsnull) && !vis->IsVisibleOrCollapsed()) {
    return NS_OK;
  }
  
  nsIFrame * child = mFrames.FirstChild();
  if (child) {    // if we have children, we are probably not really a plugin
    nsObjectFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
    return NS_OK;
  }

  // determine if we are printing
  nsCOMPtr<nsIPrintContext> thePrinterContext = do_QueryInterface(aPresContext);
  if  (thePrinterContext) {
    // we only want to print on the content layer pass
    if (eFramePaintLayer_Content != aWhichLayer)
      return NS_OK;

    // if we are printing, we need to get the correct nsIPluginInstance
    // for THIS content node in order to call ->Print() on the right plugin

    // first, we need to get the document
    nsCOMPtr<nsIDocument> doc;
    mContent->GetDocument(*getter_AddRefs(doc));
    NS_ENSURE_TRUE(doc, NS_ERROR_NULL_POINTER);

    // now we need to get the shell for the screen
    // XXX assuming that the shell at zero will always be the screen one
    nsCOMPtr<nsIPresShell> shell;
    doc->GetShellAt(0, getter_AddRefs(shell));
    NS_ENSURE_TRUE(shell, NS_ERROR_NULL_POINTER);

    // then the shell can give us the screen frame for this content node
    nsIFrame* frame = nsnull;
    shell->GetPrimaryFrameFor(mContent, &frame);
    NS_ENSURE_TRUE(frame, NS_ERROR_NULL_POINTER);

    // make sure this is REALLY an nsIObjectFrame
    // we may need to go through the children to get it
    nsIObjectFrame* objectFrame = nsnull;
    CallQueryInterface(frame,&objectFrame);
    if (!objectFrame)
      GetNextObjectFrame(aPresContext,frame,&objectFrame);
    NS_ENSURE_TRUE(objectFrame,NS_ERROR_FAILURE);

    // finally we can get our plugin instance
    nsCOMPtr<nsIPluginInstance> pi;
    if (NS_FAILED(objectFrame->GetPluginInstance(*getter_AddRefs(pi))) || !pi)
      return NS_ERROR_FAILURE;

    // now we need to setup the correct location for printing
    nsresult rv;
    nsPluginWindow    window;
    nsIView          *parentWithView;
    nsPoint           origin;
    float             t2p;
    nsMargin          margin(0,0,0,0);
    window.window =   nsnull;

    // prepare embedded mode printing struct
    nsPluginPrint npprint;
    npprint.mode = nsPluginMode_Embedded;

    // we need to find out if we are windowless or not
    PRBool    windowless;
    pi->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);
    window.type  =  windowless ? nsPluginWindowType_Drawable : nsPluginWindowType_Window;

    // get a few things
    nsCOMPtr<nsIPrintOptions> printService = do_GetService(kPrintOptionsCID, &rv);
    NS_ENSURE_TRUE(printService, NS_ERROR_FAILURE);
    printService->GetMarginInTwips(margin);

    aPresContext->GetTwipsToPixels(&t2p);
    GetOffsetFromView(aPresContext, origin, &parentWithView);

    // set it all up
    // XXX is windowless different?
    window.x = NSToCoordRound((origin.x + margin.left) * t2p);
    window.y = NSToCoordRound((origin.y + margin.top ) * t2p);
    window.width = NSToCoordRound(mRect.width * t2p);
    window.height= NSToCoordRound(mRect.height * t2p);
    window.clipRect.bottom = 0; window.clipRect.top = 0;
    window.clipRect.left = 0; window.clipRect.right = 0;
    
// XXX platform specific printing code
#if defined(XP_MAC) && !TARGET_CARBON
    // Mac does things a little differently.
    GrafPort *curPort;
    ::GetPort(&curPort);  // get the current port

    nsPluginPort port;
    port.port = (CGrafPort*)curPort;
    port.portx = window.x;
    port.porty = window.y;

    RgnHandle curClip = ::NewRgn();
    NS_ENSURE_TRUE(curClip, NS_ERROR_NULL_POINTER);
    ::GetClip(curClip); // save old clip
    
    ::SetOrigin(-window.x,-window.y); // port must be set correctly prior to telling plugin to print
    Rect r = {0,0,window.height,window.width};
    ::ClipRect(&r);  // also set the clip
    
    window.window = &port;
    npprint.print.embedPrint.platformPrint = (void*)window.window;

#elif defined (XP_UNIX)
    // UNIX does things completely differently
    PRUnichar *printfile = nsnull;
    printService->GetToFileName(&printfile);
    if (!printfile) 
      return NS_OK; // XXX what to do on UNIX when we don't have a PS file???? 

    NPPrintCallbackStruct npPrintInfo;
    npPrintInfo.type = NP_PRINT;
    npPrintInfo.fp = (FILE*)printfile;    
    npprint.print.embedPrint.platformPrint = (void*) &npPrintInfo;

#else  // Windows and non-UNIX, non-Mac(Classic) cases
    // we need the native printer device context to pass to plugin
    // On Windows, this will be the HDC
    PRUint32 pDC = 0;
    aRenderingContext.RetrieveCurrentNativeGraphicData(&pDC);

    if (!pDC)
      return NS_OK;  // no dc implemented so quit

    npprint.print.embedPrint.platformPrint = (void*)pDC;
#endif 


    npprint.print.embedPrint.window = window;
    // send off print info to plugin
    rv = pi->Print(&npprint);


#if defined(XP_MAC) && !TARGET_CARBON
    // Clean-up on Mac
    ::SetOrigin(0,0);
    ::SetClip(curClip);  // restore previous clip
    ::DisposeRgn(curClip);
#endif

    // XXX Nav 4.x always sent a SetWindow call after print. Should we do the same?
    nsCOMPtr<nsIPresContext> screenPcx;
    shell->GetPresContext(getter_AddRefs(screenPcx));
    nsDidReflowStatus status = NS_FRAME_REFLOW_FINISHED; // should we use a special status?
    frame->DidReflow(screenPcx, status);  // DidReflow will take care of it

    return rv;  // done with printing
  }


// ---------------------------------------------------------------------------
// Screen painting code

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
    DO_GLOBAL_REFLOW_COUNT_DSP("nsObjectFrame", &aRenderingContext);
    return NS_OK;
}

NS_IMETHODIMP
nsObjectFrame::HandleEvent(nsIPresContext* aPresContext,
                          nsGUIEvent*     anEvent,
                          nsEventStatus*  anEventStatus)
{
   NS_ENSURE_ARG_POINTER(anEventStatus);
    nsresult rv = NS_OK;

  //FIX FOR CRASHING WHEN NO INSTANCE OWVER
  if (!mInstanceOwner)
    return NS_ERROR_NULL_POINTER;

  if (anEvent->message == NS_PLUGIN_ACTIVATE)
  {
    nsCOMPtr<nsIContent> content;
    GetContent(getter_AddRefs(content));
    if (content)
    {
      content->SetFocus(aPresContext);
      return rv;
    }
  }

#ifdef XP_WIN
  rv = nsObjectFrameSuper::HandleEvent(aPresContext, anEvent, anEventStatus);
  return rv;
#endif

	switch (anEvent->message) {
	case NS_DESTROY:
		mInstanceOwner->CancelTimer();
		break;
	case NS_GOTFOCUS:
	case NS_LOSTFOCUS:
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
  aPluginInstance = nsnull;

  if(mInstanceOwner == nsnull)
    return NS_ERROR_NULL_POINTER;
  else
    return mInstanceOwner->GetInstance(aPluginInstance);
}

nsresult
nsObjectFrame::NotifyContentObjectWrapper()
{
  nsCOMPtr<nsIDocument> doc;

  mContent->GetDocument(*getter_AddRefs(doc));
  NS_ENSURE_TRUE(doc, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIScriptGlobalObject> sgo;
  doc->GetScriptGlobalObject(getter_AddRefs(sgo));
  NS_ENSURE_TRUE(sgo, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsIScriptContext> scx;
  sgo->GetContext(getter_AddRefs(scx));
  NS_ENSURE_TRUE(scx, NS_ERROR_UNEXPECTED);

  JSContext *cx = (JSContext *)scx->GetNativeContext();

  nsresult rv = NS_OK;
  nsCOMPtr<nsIXPConnect> xpc(do_GetService(nsIXPConnect::GetCID(), &rv));
  NS_ENSURE_SUCCESS(rv, rv);

  nsCOMPtr<nsIXPConnectWrappedNative> wrapper;
  xpc->GetWrappedNativeOfNativeObject(cx, ::JS_GetGlobalObject(cx), mContent,
                                      NS_GET_IID(nsISupports),
                                      getter_AddRefs(wrapper));

  if (!wrapper) {
    // Nothing to do here if there's no wrapper for mContent

    return NS_OK;
  }

  nsCOMPtr<nsIClassInfo> ci(do_QueryInterface(mContent));
  NS_ENSURE_TRUE(ci, NS_ERROR_UNEXPECTED);

  nsCOMPtr<nsISupports> s;
  ci->GetHelperForLanguage(nsIProgrammingLanguage::JAVASCRIPT,
                           getter_AddRefs(s));

  nsCOMPtr<nsIXPCScriptable> helper(do_QueryInterface(s));

  if (!helper) {
    // There's nothing we can do if there's no helper

    return NS_OK;
  }

  JSObject *obj = nsnull;
  rv = wrapper->GetJSObject(&obj);
  NS_ENSURE_SUCCESS(rv, rv);

  // Abuse the scriptable helper to trigger prototype setup for the
  // wrapper for mContent so that this plugin becomes part of the DOM
  // object.
  return helper->PostCreate(wrapper, cx, obj);
}

nsresult
nsObjectFrame::GetNextObjectFrame(nsIPresContext* aPresContext,
                                  nsIFrame* aRoot,
                                  nsIObjectFrame** outFrame)
{
  NS_ENSURE_ARG_POINTER(outFrame);

  nsIFrame * child;
  aRoot->FirstChild(aPresContext, nsnull, &child);

  while (child != nsnull) {
    *outFrame = nsnull;
    CallQueryInterface(child, outFrame);
    if (nsnull != *outFrame) {
      nsCOMPtr<nsIPluginInstance> pi;
      (*outFrame)->GetPluginInstance(*getter_AddRefs(pi));  // make sure we have a REAL plugin
      if (pi) return NS_OK;
    }

    GetNextObjectFrame(aPresContext, child, outFrame);
    child->GetNextSibling(&child);
  }

  return NS_ERROR_FAILURE;
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
  mTagText = nsnull;
  mPluginHost = nsnull;
  mContentFocused = PR_FALSE;
}

nsPluginInstanceOwner::~nsPluginInstanceOwner()
{
  PRInt32 cnt;

  // shut off the timer.
  if (mPluginTimer != nsnull) {
    CancelTimer();
  }

  NS_IF_RELEASE(mInstance);
  NS_IF_RELEASE(mPluginHost);
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
  
  if (nsnull != mTagText)
  {
    nsCRT::free(mTagText);
    mTagText = nsnull;
  }

  NS_IF_RELEASE(mWidget);
  mContext = nsnull;
}

NS_IMPL_ISUPPORTS10(nsPluginInstanceOwner,
                    nsIPluginInstanceOwner,
                    nsIPluginTagInfo,
                    nsIPluginTagInfo2,
                    nsIJVMPluginTagInfo,
                    nsIEventListener,
                    nsITimerCallback,
                    nsIDOMMouseListener,
                    nsIDOMMouseMotionListener,
                    nsIDOMKeyListener,
                    nsIDOMFocusListener)

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

      if (NS_SUCCEEDED(iContent->GetAttrCount(count))) {
        PRInt32 index;
        mAttrNames = (char **)PR_Calloc(sizeof(char *) * count, 1);
        mAttrVals = (char **)PR_Calloc(sizeof(char *) * count, 1);
        mNumAttrs = 0;

        if ((nsnull != mAttrNames) && (nsnull != mAttrVals)) {
          for (index = 0; index < count; index++) {
            PRInt32 nameSpaceID;
            nsCOMPtr<nsIAtom> atom;
            nsCOMPtr<nsIAtom> prefix;
            iContent->GetAttrNameAt(index, nameSpaceID,
                                    *getter_AddRefs(atom),
                                    *getter_AddRefs(prefix));
            nsAutoString  value;
            if (NS_CONTENT_ATTR_HAS_VALUE ==
                  iContent->GetAttr(nameSpaceID, atom, value)) {
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
              mAttrNames[mNumAttrs] = ToNewUTF8String(name);
              mAttrVals[mNumAttrs] = ToNewUTF8String(value);
              mNumAttrs++;
            }
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
  NS_ENSURE_TRUE(mOwner,NS_ERROR_NULL_POINTER);
  NS_ENSURE_TRUE(mContext,NS_ERROR_NULL_POINTER);

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsISupports> container;
  nsresult rv = mContext->GetContainer(getter_AddRefs(container));
  NS_ENSURE_TRUE(container,NS_ERROR_FAILURE);
  nsCOMPtr<nsILinkHandler> lh = do_QueryInterface(container);
  NS_ENSURE_TRUE(lh, NS_ERROR_FAILURE);

  nsAutoString  uniurl; uniurl.AssignWithConversion(aURL);
  nsAutoString  unitarget; unitarget.AssignWithConversion(aTarget);
  nsAutoString  fullurl;

  nsCOMPtr<nsIURI> baseURL;
  nsCOMPtr<nsIDocument> doc;
  rv = GetDocument(getter_AddRefs(doc));
  if (NS_SUCCEEDED(rv) && doc) {
    rv = doc->GetBaseURL(*getter_AddRefs(baseURL));  // gets the document's url
  } else {
    mOwner->GetFullURL(*getter_AddRefs(baseURL)); // gets the plugin's content url
  }

  // Create an absolute URL
  NS_MakeAbsoluteURI(fullurl, uniurl, baseURL);

  NS_ENSURE_TRUE(NS_SUCCEEDED(rv),NS_ERROR_FAILURE);
  nsCOMPtr<nsIContent> content;
  mOwner->GetContent(getter_AddRefs(content));
  NS_ENSURE_TRUE(content, NS_ERROR_FAILURE);

  nsCOMPtr<nsISupports> result;
  nsCOMPtr<nsIInputStream> postDataStream;
  nsCOMPtr<nsIInputStream> headersDataStream;
  if (aPostData) {
    rv = NS_NewPostDataStream(getter_AddRefs(postDataStream), PR_FALSE, (const char *) aPostData, 0);
  }
  if (aHeadersData) {
    rv = NS_NewByteInputStream(getter_AddRefs(result), (const char *) aHeadersData, aHeadersDataLen);
    if (result) {
      headersDataStream = do_QueryInterface(result, &rv);
    }
  }

  NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin post data stream");

  rv = lh->OnLinkClick(content, eLinkVerb_Replace, 
                       fullurl.get(), unitarget.get(), 
                       postDataStream, headersDataStream);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::ShowStatus(const char *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;
  
  rv = this->ShowStatus(NS_ConvertUTF8toUCS2(aStatusMsg).get());
  
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::ShowStatus(const PRUnichar *aStatusMsg)
{
  nsresult  rv = NS_ERROR_FAILURE;

  if (!mContext) {
    return rv;
  }
  nsCOMPtr<nsISupports> cont;
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  
  rv = mContext->GetContainer(getter_AddRefs(cont));
  if (NS_FAILED(rv) || !cont) {
    return rv;
  }

  nsCOMPtr<nsIDocShellTreeItem> docShellItem(do_QueryInterface(cont, &rv));
  if (NS_FAILED(rv) || !docShellItem) {
    return rv;
  }

  rv = docShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  if (NS_FAILED(rv) || !treeOwner) {
    return rv;
  }

  nsCOMPtr<nsIWebBrowserChrome> browserChrome(do_GetInterface(treeOwner, &rv));
  if (NS_FAILED(rv) || !browserChrome) {
    return rv;
  }
  rv = browserChrome->SetStatus(nsIWebBrowserChrome::STATUS_SCRIPT, 
                                aStatusMsg);

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

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(nsPluginRect *invalidRect)
{
  nsresult rv = NS_ERROR_FAILURE;

  if(invalidRect)
  {
    //no reference count on view
    nsIView* view;
    rv = mOwner->GetView(mContext, &view);

    if((rv == NS_OK) && view)
    {
      float ptot;
      mContext->GetPixelsToTwips(&ptot);

      nsRect rect((int)(ptot * invalidRect->left),
            (int)(ptot * invalidRect->top),
            (int)(ptot * (invalidRect->right - invalidRect->left)),
            (int)(ptot * (invalidRect->bottom - invalidRect->top)));

      nsIViewManager* manager;
      rv = view->GetViewManager(manager);

      //set flags to not do a synchronous update, force update does the redraw
      if((rv == NS_OK) && manager)
      {
        rv = manager->UpdateView(view, rect, NS_VMREFRESH_NO_SYNC);
        NS_RELEASE(manager);
      }
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(nsPluginRegion invalidRegion)
{
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP nsPluginInstanceOwner::ForceRedraw()
{
  //no reference count on view
  nsIView* view;
  nsresult rv = mOwner->GetView(mContext, &view);

  if((rv == NS_OK) && view)
  {
    nsIViewManager* manager;
    rv = view->GetViewManager(manager);

    if((rv == NS_OK) && manager)
    {
      rv = manager->Composite();
      NS_RELEASE(manager);
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetValue(nsPluginInstancePeerVariable variable, void *value)
{
  nsresult rv = NS_ERROR_FAILURE;

  switch(variable)
  {
    case nsPluginInstancePeerVariable_NetscapeWindow:
    {
      //no reference count on view
      nsIView* view;
      rv = mOwner->GetView(mContext, &view);

      if((rv == NS_OK) && view)
      {
        nsIViewManager* manager;
        rv = view->GetViewManager(manager);

        if((rv == NS_OK) && manager)
        {
          nsIWidget* widget;
          rv = manager->GetWidget(&widget);

          if((rv == NS_OK) && widget)
          {
            void** pvalue = (void**)value;
            *pvalue = (void*)widget->GetNativeData(NS_NATIVE_WINDOW);
            
            NS_RELEASE(widget);
          }
          NS_RELEASE(manager);
        }
      }

      break;
    }
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
    if (nsnull == mTagText) {
        nsresult rv;
        nsCOMPtr<nsIContent> content;
        rv = mOwner->GetContent(getter_AddRefs(content));
        if (NS_FAILED(rv))
            return rv;
        nsCOMPtr<nsIDOMNode> node(do_QueryInterface(content, &rv));
        if (NS_FAILED(rv))
            return rv;
        nsCOMPtr<nsIDocument> document;
        rv = GetDocument(getter_AddRefs(document));
        if (NS_FAILED(rv))
            return rv;
        nsCOMPtr<nsIDocumentEncoder> docEncoder(do_CreateInstance(NS_DOC_ENCODER_CONTRACTID_BASE "text/html", &rv));
        if (NS_FAILED(rv))
            return rv;
        rv = docEncoder->Init(document, NS_LITERAL_STRING("text/html"), nsIDocumentEncoder::OutputEncodeEntities);
        if (NS_FAILED(rv))
            return rv;

        nsCOMPtr<nsIDOMRange> range(do_CreateInstance(kRangeCID,&rv));
        if (NS_FAILED(rv))
            return rv;

        rv = range->SelectNode(node);
        if (NS_FAILED(rv))
            return rv;

        docEncoder->SetRange(range);
        nsString elementHTML;
        rv = docEncoder->EncodeToString(elementHTML);
        if (NS_FAILED(rv))
            return rv;

        mTagText = ToNewUTF8String(elementHTML);
        if (!mTagText)
            return NS_ERROR_OUT_OF_MEMORY;
    }
    *result = mTagText;
    return NS_OK;
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

                    if ((NS_CONTENT_ATTR_HAS_VALUE == kid->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::name, name)) &&
                        (NS_CONTENT_ATTR_HAS_VALUE == kid->GetAttr(kNameSpaceID_HTML, nsHTMLAtoms::value, val)))
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
                      /* According to the HTML 4.01 spec, at
                       * http://www.w3.org/TR/html4/types.html#type-cdata
                       * ''User agents may ignore leading and trailing
                       * white space in CDATA attribute values (e.g., "
                       * myval " may be interpreted as "myval"). Authors
                       * should not declare attribute values with
                       * leading or trailing white space.''
                       */
                      name.CompressWhitespace();
                      val.CompressWhitespace();
                      mParamNames[mNumParams] = ToNewUTF8String(name);
                      mParamVals[mNumParams]  = ToNewUTF8String(val);
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

    nsCOMPtr<nsIURI> docURL;
    doc->GetBaseURL(*getter_AddRefs(docURL));  // should return base + doc url

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
        nsIFrame *containingBlock=nsnull;
        rv = GetContainingBlock(mOwner, &containingBlock);
        if (NS_SUCCEEDED(rv) && containingBlock)
        {
          containingBlock->GetRect(rect);
          w -= 2*rect.x;
          // XXX: this math seems suspect to me.  Is the parent's margin really twice the x-offset?
          //      in CSS, a frame can have independent left and right margins
        }
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
          // XXX: this math seems suspect to me.  Is the parent's margin really twice the x-offset?
          //      in CSS, a frame can have independent top and bottom margins
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

// it would indicate a serious error in the frame model if aContainingBlock were null when 
// GetContainingBlock returns
static nsresult
GetContainingBlock(nsIFrame *aFrame, nsIFrame **aContainingBlock)
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
  NS_POSTCONDITION(*aContainingBlock, "bad frame model, this object frame has no containing block");
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
  *result = NS_PTR_TO_INT32(mContext);
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

void nsPluginInstanceOwner::GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent)
{
	::OSEventAvail(0, &aMacEvent);
	
	switch (anEvent.message) {
  case NS_FOCUS_EVENT_START:   // this is the same as NS_FOCUS_CONTENT
		aMacEvent.what = nsPluginEventType_GetFocusEvent;
		if (mOwner && mOwner->mPresContext)
		{
		  nsCOMPtr<nsIContent> content;
      mOwner->GetContent(getter_AddRefs(content));
      if (content)
        content->SetFocus(mOwner->mPresContext);
    }
		break;
	case NS_BLUR_CONTENT:
		aMacEvent.what = nsPluginEventType_LoseFocusEvent;
		if (mOwner && mOwner->mPresContext)
		{
		  nsCOMPtr<nsIContent> content;
      mOwner->GetContent(getter_AddRefs(content));
      if (content)
        content->RemoveFocus(mOwner->mPresContext);
    }
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

/*=============== nsIFocusListener ======================*/
nsresult nsPluginInstanceOwner::Focus(nsIDOMEvent * aFocusEvent)
{
  mContentFocused = PR_TRUE;
  return DispatchFocusToPlugin(aFocusEvent);
}

nsresult nsPluginInstanceOwner::Blur(nsIDOMEvent * aFocusEvent)
{
  mContentFocused = PR_FALSE;
  return DispatchFocusToPlugin(aFocusEvent);
}

nsresult nsPluginInstanceOwner::DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent)
{
#ifndef XP_MAC
  if (nsPluginWindowType_Window == mPluginWindow.type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aFocusEvent));
  if (privateEvent) {
    nsEvent * theEvent;
    privateEvent->GetInternalNSEvent(&theEvent);
    if (theEvent) {
      nsGUIEvent focusEvent;
      memset(&focusEvent, 0, sizeof(focusEvent));
      focusEvent.message = theEvent->message; // we only care about the message in ProcessEvent
      nsEventStatus rv = ProcessEvent(focusEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aFocusEvent->PreventDefault();
        aFocusEvent->PreventBubble();
        return NS_ERROR_FAILURE; // means consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, focusEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, privateEvent null");   
  
  return NS_OK;
}    



/*=============== nsIKeyListener ======================*/
nsresult nsPluginInstanceOwner::KeyDown(nsIDOMEvent* aKeyEvent)
{
  return DispatchKeyToPlugin(aKeyEvent);
}

nsresult nsPluginInstanceOwner::KeyUp(nsIDOMEvent* aKeyEvent)
{
  return DispatchKeyToPlugin(aKeyEvent);
}

nsresult nsPluginInstanceOwner::KeyPress(nsIDOMEvent* aKeyEvent)
{
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending keypress to the plugin, since we didn't before.
    aKeyEvent->PreventDefault();
    aKeyEvent->PreventBubble();
    return NS_ERROR_FAILURE; // means consume event
  }
  return NS_OK;
}
    
nsresult nsPluginInstanceOwner::DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent)
{
#ifdef XP_MAC   // on Mac, we don't have child windows so hook in through DOM events
  if (mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    if (privateEvent) {
      nsKeyEvent* keyEvent = nsnull;
      privateEvent->GetInternalNSEvent((nsEvent**)&keyEvent);
      if (keyEvent) {
        nsEventStatus rv = ProcessEvent(*keyEvent);
        if (nsEventStatus_eConsumeNoDefault == rv) {
          aKeyEvent->PreventDefault();
          aKeyEvent->PreventBubble();
          return NS_ERROR_FAILURE; // means consume event
        }
      }
      else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, keyEvent null");   
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, privateEvent null");   
  }
  return NS_OK;
#else
  return NS_ERROR_FAILURE; // means consume event
#endif
}    

/*=============== nsIMouseMotionListener ======================*/

nsresult
nsPluginInstanceOwner::MouseMove(nsIDOMEvent* aMouseEvent)
{
#ifndef XP_MAC
  if (nsPluginWindowType_Window == mPluginWindow.type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsMouseEvent* mouseEvent = nsnull;
    privateEvent->GetInternalNSEvent((nsEvent**)&mouseEvent);
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        return NS_ERROR_FAILURE; // means consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseMove failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseMove failed, privateEvent null");   
  
  return NS_OK;
}

/*=============== nsIMouseListener ======================*/

nsresult
nsPluginInstanceOwner::MouseDown(nsIDOMEvent* aMouseEvent)
{
#ifndef XP_MAC
  if (nsPluginWindowType_Window == mPluginWindow.type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsMouseEvent* mouseEvent = nsnull;
    privateEvent->GetInternalNSEvent((nsEvent**)&mouseEvent);
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        return NS_ERROR_FAILURE; // means consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseDown failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::MouseDown failed, privateEvent null");   
  
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::MouseUp(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseClick(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseDblClick(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseOver(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult
nsPluginInstanceOwner::MouseOut(nsIDOMEvent* aMouseEvent)
{
  return DispatchMouseToPlugin(aMouseEvent);
}

nsresult nsPluginInstanceOwner::DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent)
{
#ifndef XP_MAC
  if (nsPluginWindowType_Window == mPluginWindow.type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsMouseEvent* mouseEvent = nsnull;
    privateEvent->GetInternalNSEvent((nsEvent**)&mouseEvent);
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aMouseEvent->PreventDefault();
        aMouseEvent->PreventBubble();
        return NS_ERROR_FAILURE; // means consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, mouseEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchMouseToPlugin failed, privateEvent null");   
  
  return NS_OK;
}

nsresult
nsPluginInstanceOwner::HandleEvent(nsIDOMEvent* aEvent)
{
  return NS_OK;
}


nsEventStatus nsPluginInstanceOwner::ProcessEvent(const nsGUIEvent& anEvent)
{
  nsEventStatus rv = nsEventStatus_eIgnore;
  if (!mInstance)   // if mInstance is null, we shouldn't be here
    return rv;

#ifdef XP_MAC
    if (mWidget != NULL) {  // check for null mWidget
        EventRecord* event = (EventRecord*)anEvent.nativeMsg;
        if ((event == NULL) || (event->what == nullEvent)  || 
            (anEvent.message == NS_FOCUS_EVENT_START)      || 
            (anEvent.message == NS_BLUR_CONTENT)           || 
            (anEvent.message == NS_MOUSE_MOVE)             ||
            (anEvent.message == NS_MOUSE_ENTER))
        {
            EventRecord macEvent;
            GUItoMacEvent(anEvent, macEvent);
            event = &macEvent;
        }
        nsPluginPort* port = (nsPluginPort*)mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
        nsPluginEvent pluginEvent = { event, nsPluginPlatformWindowRef(port->port) };
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(&pluginEvent, &eventHandled);
        if (eventHandled && !(anEvent.message == NS_MOUSE_LEFT_BUTTON_DOWN && !mContentFocused))
            rv = nsEventStatus_eConsumeNoDefault;
    }
#endif

#ifdef XP_WIN
        nsPluginEvent * pPluginEvent = (nsPluginEvent *)anEvent.nativeMsg;
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(pPluginEvent, &eventHandled);
        if (eventHandled)
            rv = nsEventStatus_eConsumeNoDefault;
#endif

  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
  nsCOMPtr<nsIContent> content;
  mOwner->GetContent(getter_AddRefs(content));

  // Unregister focus event listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMFocusListener> focusListener;
      QueryInterface(NS_GET_IID(nsIDOMFocusListener), getter_AddRefs(focusListener));
      if (focusListener) { 
        receiver->RemoveEventListenerByIID(focusListener, NS_GET_IID(nsIDOMFocusListener));
      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
    }
    else NS_ASSERTION(PR_FALSE, "plugin was not an event listener");
  }
  else NS_ASSERTION(PR_FALSE, "plugin had no content");
  
  // Unregister mouse event listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMMouseListener> mouseListener;
      QueryInterface(NS_GET_IID(nsIDOMMouseListener), getter_AddRefs(mouseListener));
      if (mouseListener) { 
        receiver->RemoveEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));
      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
      // now for the mouse motion listener
      nsCOMPtr<nsIDOMMouseMotionListener> mouseMotionListener;
      QueryInterface(NS_GET_IID(nsIDOMMouseMotionListener), getter_AddRefs(mouseMotionListener));
      if (mouseMotionListener) { 
        receiver->RemoveEventListenerByIID(mouseMotionListener, NS_GET_IID(nsIDOMMouseMotionListener));
      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
    }
    else NS_ASSERTION(PR_FALSE, "plugin was not an event listener");
  }
  else NS_ASSERTION(PR_FALSE, "plugin had no content");
  
  // Unregister key event listener;
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMKeyListener> keyListener;
      QueryInterface(NS_GET_IID(nsIDOMKeyListener), getter_AddRefs(keyListener));
      if (keyListener) { 
        receiver->RemoveEventListener(NS_LITERAL_STRING("keypress"), keyListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("keydown"), keyListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("keyup"), keyListener, PR_TRUE);

      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
    }
    else NS_ASSERTION(PR_FALSE, "plugin was not an event listener");
  }
  else NS_ASSERTION(PR_FALSE, "plugin had no content");

  mOwner = nsnull; // break relationship between frame and plugin instance owner

  return NS_OK;
}

// Paints are handled differently, so we just simulate an update event.

void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect, PRUint32 ndc)
{
#ifdef XP_MAC
  if (mInstance != NULL) {

    nsPluginPort* pluginPort = GetPluginPort();

#ifdef DO_DIRTY_INTERSECT   // aDirtyRect isn't always correct, see bug 56128
    nsPoint rel(aDirtyRect.x, aDirtyRect.y);
    nsPoint abs(0,0);
    nsCOMPtr<nsIWidget> containerWidget;
  
    // Convert dirty rect relative coordinates to absolute and also get the containerWidget
    ConvertRelativeToWindowAbsolute(mOwner, mContext, rel, abs, *getter_AddRefs(containerWidget));

    nsRect absDirtyRect = nsRect(abs.x, abs.y, aDirtyRect.width, aDirtyRect.height);

    // Convert to absolute pixel values for the dirty rect
    nsRect absDirtyRectInPixels;
    ConvertTwipsToPixels(*mContext, absDirtyRect, absDirtyRectInPixels);
#endif
    FixUpPluginWindow();

    EventRecord updateEvent;
    ::OSEventAvail(0, &updateEvent);
    updateEvent.what = updateEvt;
    updateEvent.message = UInt32(pluginPort->port);

    nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(pluginPort->port) };
    PRBool eventHandled = PR_FALSE;
    mInstance->HandleEvent(&pluginEvent, &eventHandled);
    }
#endif

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
    // validate the plugin clipping information by syncing the plugin window info to
    // reflect the current widget location. This makes sure that everything is updated
    // correctly in the event of scrolling in the window.
    FixUpPluginWindow();
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
    mPluginTimer->Init(this, 1020 / 60);
#endif
}

void nsPluginInstanceOwner::CancelTimer()
{
    if (mPluginTimer) {
        mPluginTimer->Cancel();
        mPluginTimer = nsnull;
    }
}

NS_IMETHODIMP nsPluginInstanceOwner::Init(nsIPresContext* aPresContext, nsObjectFrame *aFrame)
{
  //do not addref to avoid circular refs. MMP
  mContext = aPresContext;
  mOwner = aFrame;
  
  nsCOMPtr<nsIContent> content;
  mOwner->GetContent(getter_AddRefs(content));
  
  // This is way of ensure the previous document is gone. Important when reloading either 
  // the page or refreshing plugins. In the case of an OBJECT frame,
  // we want to flush out the prevous content viewer which will cause the previous document
  // and plugins to be cleaned up. Then we can create our new plugin without the old instance
  // hanging around.

  nsCOMPtr<nsISupports> container;
  aPresContext->GetContainer(getter_AddRefs(container));
  if (container) {
    nsCOMPtr<nsIDocShell> cvc(do_QueryInterface(container));
    if (cvc) {
      nsCOMPtr<nsIContentViewer> cv;
      cvc->GetContentViewer(getter_AddRefs(cv));
      if (cv)
        cv->Show();
    }
  }

  // Register focus listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMFocusListener> focusListener;
      QueryInterface(NS_GET_IID(nsIDOMFocusListener), getter_AddRefs(focusListener));
      if (focusListener) {
        receiver->AddEventListenerByIID(focusListener, NS_GET_IID(nsIDOMFocusListener));
      }
    }
  }

  // Register mouse listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMMouseListener> mouseListener;
      QueryInterface(NS_GET_IID(nsIDOMMouseListener), getter_AddRefs(mouseListener));
      if (mouseListener) {
        receiver->AddEventListenerByIID(mouseListener, NS_GET_IID(nsIDOMMouseListener));
      }
      // now do the mouse motion listener
      nsCOMPtr<nsIDOMMouseMotionListener> mouseMotionListener;
      QueryInterface(NS_GET_IID(nsIDOMMouseMotionListener), getter_AddRefs(mouseMotionListener));
      if (mouseMotionListener) {
        receiver->AddEventListenerByIID(mouseMotionListener, NS_GET_IID(nsIDOMMouseMotionListener));
      }
    }
  }
  // Register key listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMKeyListener> keyListener;
      QueryInterface(NS_GET_IID(nsIDOMKeyListener), getter_AddRefs(keyListener));
      if (keyListener) {
        receiver->AddEventListener(NS_LITERAL_STRING("keypress"), keyListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("keydown"), keyListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("keyup"), keyListener, PR_TRUE);
        return NS_OK;
      }
    }
  }
  
  NS_ASSERTION(PR_FALSE, "plugin could not be added as a mouse listener");
  return NS_ERROR_NO_INTERFACE; 
}

nsPluginPort* nsPluginInstanceOwner::GetPluginPort()
{
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  nsPluginPort* result = NULL;
    if (mWidget != NULL)
  {
#ifdef XP_WIN
    if(mPluginWindow.type == nsPluginWindowType_Drawable)
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_GRAPHIC);
    else
#endif
      result = (nsPluginPort*) mWidget->GetNativeData(NS_NATIVE_PLUGIN_PORT);
    }
    return result;
}

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
  float p2t;

  if (nsnull != mOwner)
  {
    // Create view if necessary

    mOwner->GetView(mContext, &view);

    if (nsnull == view || nsnull == mWidget)
    {
      PRBool    windowless;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

      // always create widgets in Twips, not pixels
      mContext->GetScaledPixelsToTwips(&p2t);
      rv = mOwner->CreateWidget(mContext,
                                NSIntPixelsToTwips(mPluginWindow.width, p2t),
                                NSIntPixelsToTwips(mPluginWindow.height, p2t),
                                windowless);
      if (NS_OK == rv)
      {
        mOwner->GetView(mContext, &view);
        if (view)
        {
          view->GetWidget(mWidget);
          PRBool fTransparent;
          mInstance->GetValue(nsPluginInstanceVariable_TransparentBool, (void *)&fTransparent);
          view->SetContentTransparency(fTransparent);
        }

        if (PR_TRUE == windowless)
        {
          mPluginWindow.type = nsPluginWindowType_Drawable;
          mPluginWindow.window = nsnull; // this needs to be a HDC according to the spec,
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
          // Is this needed in the windowless case ???
          // start a periodic timer to provide null events to the plugin instance.
          mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
          if (rv == NS_OK)
            rv = mPluginTimer->Init(this, 1020 / 60, NS_PRIORITY_NORMAL, NS_TYPE_REPEATING_SLACK);
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

  aClipRect.x += aAbsX; 
  aClipRect.y += aAbsY; 

  //printf("--------------\n"); 
  //printf("Widget clip X %d Y %d rect %d %d %d %d\n", aAbsX, aAbsY,  aClipRect.x,  aClipRect.y, aClipRect.width,  aClipRect.height ); 
  //printf("--------------\n"); 
} 


#ifdef DO_DIRTY_INTERSECT
// Convert from a frame relative coordinate to a coordinate relative to its
// containing window
static void ConvertRelativeToWindowAbsolute(nsIFrame* aFrame, nsIPresContext* aPresContext, nsPoint& aRel, nsPoint& aAbs, nsIWidget *&
aContainerWidget)
{
  aAbs.x = 0;
  aAbs.y = 0;

  nsIView *view = nsnull;
  // See if this frame has a view
  aFrame->GetView(aPresContext, &view);
  if (nsnull == view) {
   // Calculate frames offset from its nearest view
   aFrame->GetOffsetFromView(aPresContext,
                   aAbs,
                   &view);
  } else {
   // Store frames offset from its view.
   nsRect rect;
   aFrame->GetRect(rect);
   aAbs.x = rect.x;
   aAbs.y = rect.y;
  }

  if (view != nsnull) {
    // Caclulate the views offset from its nearest widget

   nscoord viewx = 0; 
   nscoord viewy = 0;
   view->GetWidget(aContainerWidget);
   if (nsnull == aContainerWidget) {
     view->GetOffsetFromWidget(&viewx, &viewy, aContainerWidget/**getter_AddRefs(widget)*/);
     aAbs.x += viewx;
     aAbs.y += viewy;
   
   
   // GetOffsetFromWidget does not include the views offset, so we need to add
   // that in.
    nsRect bounds;
    view->GetBounds(bounds);
    aAbs.x += bounds.x;
    aAbs.y += bounds.y;
    }
   
   nsRect widgetBounds;
   aContainerWidget->GetBounds(widgetBounds); 
  } else {
   NS_ASSERTION(PR_FALSE, "the object frame does not have a view");
  }

  // Add relative coordinate to the absolute coordinate that has been calculated
  aAbs += aRel;
}

// Convert from a frame relative coordinate to a coordinate relative to its
// containing window
static void ConvertTwipsToPixels(nsIPresContext& aPresContext, nsRect& aTwipsRect, nsRect& aPixelRect)
{
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  aPixelRect.x = NSTwipsToIntPixels(aTwipsRect.x, t2p);
  aPixelRect.y = NSTwipsToIntPixels(aTwipsRect.y, t2p);
  aPixelRect.width = NSTwipsToIntPixels(aTwipsRect.width, t2p);
  aPixelRect.height = NSTwipsToIntPixels(aTwipsRect.height, t2p);
}
#endif // DO_DIRTY_INTERSECT

void nsPluginInstanceOwner::FixUpPluginWindow()
{
  if (mWidget) {
    nscoord absWidgetX = 0;
    nscoord absWidgetY = 0;
    nsRect widgetClip(0,0,0,0);
    GetWidgetPosAndClip(mWidget,absWidgetX,absWidgetY,widgetClip);

    // set the port coordinates
    mPluginWindow.x = absWidgetX;
    mPluginWindow.y = absWidgetY;

    // fix up the clipping region
    mPluginWindow.clipRect.top = widgetClip.y;
    mPluginWindow.clipRect.left = widgetClip.x;
    mPluginWindow.clipRect.bottom =  mPluginWindow.clipRect.top + widgetClip.height;
    mPluginWindow.clipRect.right =  mPluginWindow.clipRect.left + widgetClip.width; 
  }
}


#endif // XP_MAC
