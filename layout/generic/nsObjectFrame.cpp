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
#include "nsINodeInfo.h"
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
#include "nsIScrollableView.h"
#include "nsIScrollPositionListener.h"
#include "nsIStringStream.h" // for NS_NewCharInputStream
#include "nsITimer.h"
#include "nsLayoutAtoms.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIWebBrowserChrome.h"
#include "nsIDOMElement.h"
#include "nsIDOMNodeList.h"
#include "nsIDOMHTMLObjectElement.h"
#include "nsIDOMHTMLAppletElement.h"
#include "nsContentPolicyUtils.h"
#include "nsIDOMWindow.h"
#include "nsIDOMMouseListener.h"
#include "nsIDOMMouseMotionListener.h"
#include "nsIDOMFocusListener.h"
#include "nsIDOMContextMenuListener.h"
#include "nsIDOMDragListener.h"
#include "nsIDOMEventReceiver.h"
#include "nsIDOMNSEvent.h"
#include "nsIPrivateDOMEvent.h"
#include "nsIDocumentEncoder.h"
#include "nsXPIDLString.h"
#include "nsIDOMRange.h"
#include "nsIPrintContext.h"
#include "nsIPrintPreviewContext.h"
#include "nsIWidget.h"
#include "nsGUIEvent.h"
#include "nsIRenderingContext.h"
#include "nsIContentViewer.h"
#include "nsIDocShell.h"
#include "npapi.h"
#include "nsIPrintSettings.h"
#include "nsGfxCIID.h"
#include "nsHTMLUtils.h"
#include "nsUnicharUtils.h"
#include "nsTransform2D.h"

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
#include "imgILoader.h"

#include "nsObjectFrame.h"
#include "nsIObjectFrame.h"
#include "nsPluginNativeWindow.h"
#include "nsPIPluginHost.h"

// accessibility support
#ifdef ACCESSIBILITY
#include "nsIAccessibilityService.h"
#endif

#include "nsContentCID.h"
static NS_DEFINE_CID(kRangeCID,     NS_RANGE_CID);

// XXX temporary for Mac double buffering pref
static NS_DEFINE_CID(kPrefServiceCID, NS_PREF_CID);

/* X headers suck */
#ifdef KeyPress
#undef KeyPress
#endif

// special class for handeling DOM context menu events
// because for some reason it starves other mouse events if implemented on the same class
class nsPluginDOMContextMenuListener : public nsIDOMContextMenuListener,
                                       public nsIEventListener
{
public:
  nsPluginDOMContextMenuListener();
  virtual ~nsPluginDOMContextMenuListener();

  NS_DECL_ISUPPORTS

  NS_IMETHOD ContextMenu(nsIDOMEvent* aContextMenuEvent) { return NS_ERROR_FAILURE; /* means consume event */ }
  
  nsresult Init(nsObjectFrame *aFrame);
  nsresult Destroy(nsObjectFrame *aFrame);
  
  NS_IMETHOD HandleEvent(nsIDOMEvent* aEvent) { return NS_OK; }
  nsEventStatus ProcessEvent(const nsGUIEvent& anEvent) { return nsEventStatus_eConsumeNoDefault; }
};


class nsPluginInstanceOwner : public nsIPluginInstanceOwner,
                              public nsIPluginTagInfo2,
                              public nsIJVMPluginTagInfo,
                              public nsIEventListener,
                              public nsITimerCallback,
                              public nsIDOMMouseListener,
                              public nsIDOMMouseMotionListener,
                              public nsIDOMKeyListener,
                              public nsIDOMFocusListener,
                              public nsIScrollPositionListener,
                              public nsIDOMDragListener

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
                    PRUint32 aHeadersDataLen, PRBool isFile = PR_FALSE);

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
  
  // nsIDOMDragListener interfaces
  NS_IMETHOD DragEnter(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragOver(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragExit(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragDrop(nsIDOMEvent* aMouseEvent);
  NS_IMETHOD DragGesture(nsIDOMEvent* aMouseEvent);
  

  nsresult Destroy();  

  //nsIEventListener interface
  nsEventStatus ProcessEvent(const nsGUIEvent & anEvent);
  
  void Paint(const nsRect& aDirtyRect, PRUint32 ndc = nsnull);

  // nsITimerCallback interface
  NS_DECL_NSITIMERCALLBACK
  
  void CancelTimer();
  void StartTimer();

  // nsIScrollPositionListener interface
  NS_IMETHOD ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);
  NS_IMETHOD ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY);

  //locals

  NS_IMETHOD Init(nsIPresContext *aPresContext, nsObjectFrame *aFrame);

  nsPluginPort* GetPluginPort();
  void ReleasePluginPort(nsPluginPort * pluginPort);

  void SetPluginHost(nsIPluginHost* aHost);

#if defined(XP_MAC) || defined(XP_MACOSX)
  nsPluginPort* FixUpPluginWindow();
  void GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent);
  void Composite();
#endif

private:
  nsPluginNativeWindow *mPluginWindow;
  nsIPluginInstance *mInstance;
  nsObjectFrame     *mOwner;
  nsCString          mDocumentBase;
  char              *mTagText;
  nsIWidget         *mWidget;
  nsIPresContext    *mContext;
  nsCOMPtr<nsITimer> mPluginTimer;
  nsIPluginHost     *mPluginHost;
  PRPackedBool       mContentFocused;
  PRPackedBool       mWidgetVisible;    // used on Mac to store our widget's visible state
  PRUint16          mNumCachedAttrs;
  PRUint16          mNumCachedParams;
  char              **mCachedAttrParamNames;
  char              **mCachedAttrParamValues;
  
  nsPluginDOMContextMenuListener * mCXMenuListener;  // pointer to wrapper for nsIDOMContextMenuListener
  
  nsresult DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent);
  nsresult DispatchMouseToPlugin(nsIDOMEvent* aMouseEvent);
  nsresult DispatchFocusToPlugin(nsIDOMEvent* aFocusEvent);

  nsresult EnsureCachedAttrParamArrays();
};

static void ConvertTwipsToPixels(nsIPresContext& aPresContext, nsRect& aTwipsRect, nsRect& aPixelRect);

  // Mac specific code to fix up port position and clip during paint
#if defined(XP_MAC) || defined(XP_MACOSX)
  // get the absolute widget position and clip
  static void GetWidgetPosClipAndVis(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY, nsRect& aClipRect, PRBool& aIsVisible); 
  // convert relative coordinates to absolute
  static void ConvertRelativeToWindowAbsolute(nsIFrame* aFrame, nsIPresContext* aPresContext, nsPoint& aRel, nsPoint& aAbs, nsIWidget *&aContainerWidget);
#endif // XP_MAC

nsObjectFrame::~nsObjectFrame()
{
  if (nsnull != mInstanceOwner) {
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

#ifdef ACCESSIBILITY
NS_IMETHODIMP nsObjectFrame::GetAccessible(nsIAccessible** aAccessible)
{
  nsCOMPtr<nsIAccessibilityService> accService = do_GetService("@mozilla.org/accessibilityService;1");

  if (accService) {
    nsCOMPtr<nsIDOMNode> node = do_QueryInterface(mContent);
    return accService->CreateIFrameAccessible(node, aAccessible);
  }

  return NS_ERROR_FAILURE;
}

#ifdef XP_WIN
NS_IMETHODIMP nsObjectFrame::GetPluginPort(HWND *aPort)
{
  *aPort = (HWND) mInstanceOwner->GetPluginPort();
  return NS_OK;
}
#endif
#endif


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
#define IMAGE_EXT_CUR "cur"
#define IMAGE_EXT_MNG "mng"
#define IMAGE_EXT_JNG "jng"

// #define DO_DIRTY_INTERSECT 1   // enable dirty rect intersection during paint

void nsObjectFrame::IsSupportedImage(nsIContent* aContent, PRBool* aImage)
{
  *aImage = PR_FALSE;

  if(aContent == NULL)
    return;

  nsAutoString type;
  nsresult rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (type.Length() > 0)) 
  {
    nsCOMPtr<imgILoader> loader(do_GetService("@mozilla.org/image/loader;1"));
    loader->SupportImageWithMimeType(NS_LossyConvertUCS2toASCII(type).get(), aImage);
    return;
  }

  nsAutoString data;
  rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, data);

  PRBool havedata = (rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0);

  if(!havedata)
  {// try it once more for SRC attrubute
    rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, data);
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
         ext.EqualsIgnoreCase(IMAGE_EXT_ICO) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_CUR) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_MNG) ||
         ext.EqualsIgnoreCase(IMAGE_EXT_JNG))
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
  rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);
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
  rv = aContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, data);
  if((rv == NS_CONTENT_ATTR_HAS_VALUE) && (data.Length() > 0)) 
  {
    nsCOMPtr<nsIURI> uri;
    nsCOMPtr<nsIURI> baseURL;

    if (NS_FAILED(GetBaseURL(*getter_AddRefs(baseURL)))) return; // XXX NS_NewURI fails without base
    rv = NS_NewURI(getter_AddRefs(uri), data, nsnull, baseURL);
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
    nsCOMPtr<nsIPluginInstance> inst;
    if(NS_SUCCEEDED(mInstanceOwner->GetInstance(*getter_AddRefs(inst))))
    {
      nsPluginWindow *win;
      mInstanceOwner->GetWindow(win);
      nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;
      nsCOMPtr<nsIPluginInstance> nullinst;

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
          
          if (window) 
            window->CallSetWindow(nullinst);
          else 
            inst->SetWindow(nsnull);
        }
        else {
          if (window) 
            window->CallSetWindow(nullinst);
          else 
            inst->SetWindow(nsnull);

          inst->Stop();
          inst->Destroy();
        }
      }
      else {
        if (window) 
          window->CallSetWindow(nullinst);
        else 
          inst->SetWindow(nsnull);

        inst->Stop();
      }

      nsCOMPtr<nsIPluginHost> pluginHost = do_GetService(kCPluginManagerCID);
      if(pluginHost)
        pluginHost->StopPluginInstance(inst);
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
nsObjectFrame::GetFrameName(nsAString& aResult) const
{
  return MakeFrameName(NS_LITERAL_STRING("ObjectFrame"), aResult);
}
#endif

nsresult
nsObjectFrame::CreateWidget(nsIPresContext* aPresContext,
                            nscoord aWidth,
                            nscoord aHeight,
                            PRBool aViewOnly)
{
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
#if defined(XP_MAC) || defined(XP_MACOSX)
    nsCOMPtr<nsIPref> prefs(do_GetService(kPrefServiceCID));
    PRBool doubleBuffer = PR_FALSE;
    prefs ? prefs->GetBoolPref("plugin.enable_double_buffer", &doubleBuffer) : 0;
    
    viewMan->AllowDoubleBuffering(doubleBuffer);
#endif

    // XXX Put this last in document order
    // XXX Should we be setting the z-index here?
    viewMan->InsertChild(parView, view, nsnull, PR_TRUE);

    if(aViewOnly != PR_TRUE) {

      result = view->CreateWidget(kWidgetCID);

      if (NS_OK != result) {
        result = NS_OK;       //XXX why OK? MMP
        goto exit;            //XXX sue me. MMP
      }
    }
  }

  {
    // Here we set the background color for this widget because some plugins will use 
    // the child window background color when painting. If it's not set, it may default to gray
    // Sometimes, a frame doesn't have a background color or is transparent. In this
    // case, walk up the frame tree until we do find a frame with a background color
    for (nsIFrame* frame = this; frame; frame->GetParent(&frame)) {
      const nsStyleBackground*  color;
      frame->GetStyleData(eStyleStruct_Background, (const nsStyleStruct*&)color);
      if (!color->BackgroundIsTransparent()) {  // make sure we got an actual color
        nsCOMPtr<nsIWidget> win;
        view->GetWidget(*getter_AddRefs(win));
        if (win)
          win->SetBackgroundColor(color->mBackgroundColor);
        break;
      }
    }

    
    //this is ugly. it was ripped off from didreflow(). MMP
    // Position and size view relative to its parent, not relative to our
    // parent frame (our parent frame may not have a view).

    nsIView* parentWithView;
    nsPoint origin;
    nsRect r(0, 0, mRect.width, mRect.height);

    viewMan->SetViewVisibility(view, nsViewVisibility_kShow);
    GetOffsetFromView(aPresContext, origin, &parentWithView);
    viewMan->ResizeView(view, r);
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

  if (IsHidden(PR_FALSE))
    return;

  // for EMBED and APPLET, default to 240x200 for compatibility
  nsCOMPtr<nsIAtom> atom;
  mContent->GetTag(*getter_AddRefs(atom));
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
    else // unconstrained percent case
      aMetrics.width = (NS_UNCONSTRAINEDSIZE == aReflowState.mComputedWidth) ? 0 : aReflowState.mComputedWidth;
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
    else {// unconstrained percent case
      nsCOMPtr<nsIAtom>  fType;
      const nsHTMLReflowState* cbrs = aReflowState.parentReflowState;
      nsIFrame* f = cbrs->parentReflowState->frame;
      f->GetFrameType(getter_AddRefs(fType));
      if (fType.get() == nsLayoutAtoms::tableCellFrame || fType.get() == nsLayoutAtoms::bcTableCellFrame) {
        if (aReflowState.mComputedHeight != NS_UNCONSTRAINEDSIZE)
          aMetrics.height = aReflowState.mComputedHeight;
      }
      else {
        // Make it nicely fit in the viewport considering margins
        nsRect rect;
        aPresContext->GetVisibleArea(rect);
        nscoord h = rect.height;
        // Get the containing block reflow state
        const nsHTMLReflowState* containingBlockRS = aReflowState.parentReflowState;
        for ( ; containingBlockRS; containingBlockRS = containingBlockRS->parentReflowState) {
          PRBool isPercentBase;
          containingBlockRS->frame->IsPercentageBase(isPercentBase);
          if (isPercentBase)
            break;
        }
        // Substract out top and bottom margins from the containing block up to the canvas
        if (containingBlockRS) {
          for (const nsHTMLReflowState* rs = containingBlockRS; rs; rs = rs->parentReflowState) {
            nsCOMPtr<nsIAtom> fType;
            rs->frame->GetFrameType(getter_AddRefs(fType));
            if (nsLayoutAtoms::canvasFrame == fType) 
              break;
            h -= rs->mComputedMargin.top + rs->mComputedMargin.bottom;
          }
        }
        else NS_ASSERTION(containingBlockRS, "no containing block");

        aMetrics.height = NSToCoordRound (factor * h);
      }
    }
  }
  
  // accent
  aMetrics.ascent = aMetrics.height;

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
  aSrc.Trim(" \n\r\t\b", PR_TRUE, PR_TRUE, PR_FALSE);

  // get document charset
  nsAutoString originCharset;
  if (document && NS_FAILED(document->GetDocumentCharacterSet(originCharset)))
    originCharset.Truncate();
 
  return NS_NewURI(aFullURI, aSrc, NS_LossyConvertUCS2toASCII(originCharset).get(),
                   aBaseURI, nsHTMLUtils::IOService);
}

NS_IMETHODIMP
nsObjectFrame::Reflow(nsIPresContext*          aPresContext,
                      nsHTMLReflowMetrics&     aMetrics,
                      const nsHTMLReflowState& aReflowState,
                      nsReflowStatus&          aStatus)
{
  DO_GLOBAL_REFLOW_COUNT("nsObjectFrame", aReflowState.reason);
  DISPLAY_REFLOW(aPresContext, this, aReflowState, aMetrics, aStatus);
  nsresult rv = NS_OK;

  // Get our desired size
  GetDesiredSize(aPresContext, aReflowState, aMetrics);

  // handle an image or iframes elsewhere
  nsIFrame * child = mFrames.FirstChild();
  if(child != nsnull)
    return HandleChild(aPresContext, aMetrics, aReflowState, aStatus, child);
  // if we are printing or print previewing, bail for now
  nsCOMPtr<nsIPrintContext> thePrinterContext = do_QueryInterface(aPresContext);
  nsCOMPtr<nsIPrintPreviewContext> thePrintPreviewContext = do_QueryInterface(aPresContext);
  if (thePrinterContext || thePrintPreviewContext) {
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

    if (NS_SUCCEEDED(rv = GetBaseURL(*getter_AddRefs(baseURL)))) {
      nsAutoString codeBase;
      if ((NS_CONTENT_ATTR_HAS_VALUE ==
            mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::codebase, codeBase)) &&
          !codeBase.IsEmpty()) {
        nsCOMPtr<nsIURI> codeBaseURL;
        rv = MakeAbsoluteURL(getter_AddRefs(codeBaseURL), codeBase, baseURL);
        if (NS_SUCCEEDED(rv)) {
          baseURL = codeBaseURL;
        }
      }
    }

    // if we have a clsid, we're either an internal widget, an ActiveX control, or an applet
    if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::classid, classid)) {
      PRBool bJavaObject;

      bJavaObject = !nsCRT::strncmp(classid.get(), NS_LITERAL_STRING("java:").get(), 5);

      // if we find "java:" in the class id, we have a java applet
      if(bJavaObject) {
        if (!baseURL) return NS_ERROR_FAILURE;

        fullURL = baseURL;

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
        if (classid.Equals(NS_LITERAL_STRING("browser"))) {
          rv = InstantiateWidget(aPresContext, aMetrics,
                                 aReflowState, kCAppShellCID);
        }
        else {
          // if we haven't matched to an internal type, check to see if
          // we have an ActiveX handler
          // if not, create the default plugin
          if (!baseURL) return NS_ERROR_FAILURE;

          fullURL = baseURL;

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
      if (!baseURL) return NS_ERROR_FAILURE;

      // get the nsIPluginHost interface
      pluginHost = do_GetService(kCPluginManagerCID);
      if (!pluginHost) return NS_ERROR_FAILURE;

      mInstanceOwner->SetPluginHost(pluginHost);

      nsCOMPtr<nsIAtom> tag;
      mContent->GetTag(*getter_AddRefs(tag));
      if (tag.get() == nsHTMLAtoms::applet) {
        if (NS_CONTENT_ATTR_HAS_VALUE == mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::code, src)) {
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
        mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::type, type);

        if (type.Length()) {
          mimeTypeStr.Adopt(ToNewCString(type));
        }
        //stream in the object source if there is one...
        if (NS_CONTENT_ATTR_HAS_VALUE ==
              mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::src, src) ||
            NS_CONTENT_ATTR_HAS_VALUE ==
              mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, src)) {
          // Create an absolute URL
          rv = MakeAbsoluteURL(getter_AddRefs(fullURL), src, baseURL);
          if (NS_FAILED(rv)) {
            // Failed to create URI, maybe because we didn't
            // reconize the protocol handler ==> treat like
            // no 'src'/'data' was specified in the embed/object tag
            fullURL = baseURL;
          }
        }
        else {
          // we didn't find a src or data param, so just set the url
          // to the base

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
    // if we got an error, we are probably going to be replaced

    // for a replaced object frame, clear our vertical alignment style info, see bug 36997
    nsStyleTextReset* text = NS_STATIC_CAST(nsStyleTextReset*,
      mStyleContext->GetUniqueStyleData(mPresContext,  eStyleStruct_TextReset));
    text->mVerticalAlign.SetNormalValue();

    //check for alternative content with CantRenderReplacedElement()
    nsIPresShell* presShell;
    aPresContext->GetShell(&presShell);
    rv = presShell->CantRenderReplacedElement(aPresContext, this);
    NS_RELEASE(presShell);
  } else {
    NotifyContentObjectWrapper();
  }
  aStatus = NS_FRAME_COMPLETE;

  NS_FRAME_SET_TRUNCATION(aStatus, aReflowState, aMetrics);
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

  nsCOMPtr<nsIWidget> parent;
  parentWithView->GetOffsetFromWidget(nsnull, nsnull, *getter_AddRefs(parent));
  mWidget->Create(NS_STATIC_CAST(nsIWidget*,parent), r, nsnull, nsnull);

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

  NS_ENSURE_TRUE(window, NS_ERROR_NULL_POINTER);

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
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
#else
  window->clipRect.bottom = 0;
  window->clipRect.right = 0;
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
#if defined(XP_MAC) || defined(XP_MACOSX)
  window->clipRect.top = 0;
  window->clipRect.left = 0;
  window->clipRect.bottom = NSTwipsToIntPixels(aMetrics.height, t2p);
  window->clipRect.right = NSTwipsToIntPixels(aMetrics.width, t2p);
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

  if(kidReflowState.mStylePosition->mWidth.GetUnit() == eStyleUnit_Coord ||
       kidReflowState.mStylePosition->mWidth.GetUnit() == eStyleUnit_Percent) {
    //the object frame has already calculated the constraints
    kidReflowState.mComputedWidth = aMetrics.width;
  }

  if(kidReflowState.mStylePosition->mHeight.GetUnit() == eStyleUnit_Coord ||
       kidReflowState.mStylePosition->mHeight.GetUnit() == eStyleUnit_Percent) {
    //the object frame has already calculated the constraints
    kidReflowState.mComputedHeight = aMetrics.height;
  }

  nsReflowStatus status;

  kidDesiredSize.width = NS_UNCONSTRAINEDSIZE;
  kidDesiredSize.height = NS_UNCONSTRAINEDSIZE;

  ReflowChild(child, aPresContext, kidDesiredSize, kidReflowState, 0, 0, 0, status);
  FinishReflowChild(child, aPresContext, &kidReflowState, kidDesiredSize, 0, 0, 0);

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
nsObjectFrame::IsHidden(PRBool aCheckVisibilityStyle) const
{
  if (aCheckVisibilityStyle) {
    const nsStyleVisibility* vis = (const nsStyleVisibility*)mStyleContext->GetStyleData(eStyleStruct_Visibility);
    if (vis && !vis->IsVisibleOrCollapsed())
      return PR_TRUE;    
  }

  nsCOMPtr<nsIAtom> tag;
  mContent->GetTag(*getter_AddRefs(tag));

  // only <embed> tags support the HIDDEN attribute
  if (tag.get() == nsHTMLAtoms::embed) {
    nsAutoString hidden;
    mContent->GetAttr(kNameSpaceID_None, nsHTMLAtoms::hidden, hidden);

    // Yes, these are really the kooky ways that you could tell 4.x
    // not to hide the <embed> once you'd put the 'hidden' attribute
    // on the tag...
      // these |NS_ConvertASCIItoUCS2|s can't be |NS_LITERAL_STRING|s until |EqualsIgnoreCase| get's fixed
    if (!hidden.IsEmpty() &&
        !hidden.Equals(NS_LITERAL_STRING("false"), nsCaseInsensitiveStringComparator()) &&
        !hidden.Equals(NS_LITERAL_STRING("no"), nsCaseInsensitiveStringComparator()) &&
        !hidden.Equals(NS_LITERAL_STRING("off"), nsCaseInsensitiveStringComparator())) {
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
    nsHTMLReflowCommand* reflowCmd;
    rv = NS_NewHTMLReflowCommand(&reflowCmd, this,
                                 eReflowType_ContentChanged);
    if (NS_SUCCEEDED(rv))
      shell->AppendReflowCommand(reflowCmd);
  }
  return rv;
}

nsresult nsObjectFrame::GetWindowOriginInPixels(nsIPresContext * aPresContext, PRBool aWindowless, nsPoint * aOrigin)
{
  NS_ENSURE_ARG_POINTER(aPresContext);
  NS_ENSURE_ARG_POINTER(aOrigin);

  nsresult rv = NS_OK;
  nsIView * parentWithView;
  nsPoint origin(0,0);

  GetOffsetFromView(aPresContext, origin, &parentWithView);

  // if it's windowless, let's make sure we have our origin set right
  // it may need to be corrected, like after scrolling
  if (aWindowless && parentWithView) {
    nsPoint correction(0,0);
    nsCOMPtr<nsIViewManager> parentVM;
    parentWithView->GetViewManager(*getter_AddRefs(parentVM));

    // Walk up all the views and add up their positions. This will give us our
    // absolute position which is what we want to give the plugin
    nsIView* theView = parentWithView;
    while (theView) {
      nsCOMPtr<nsIViewManager> vm;
      theView->GetViewManager(*getter_AddRefs(vm));
      if (vm != parentVM)
        break;

      theView->GetPosition(&correction.x, &correction.y);
      origin += correction;
      
      theView->GetParent(theView);
    }  
  }

  float t2p;
  aPresContext->GetTwipsToPixels(&t2p);
  aOrigin->x = NSTwipsToIntPixels(origin.x, t2p);
  aOrigin->y = NSTwipsToIntPixels(origin.y, t2p);

  return rv;
}

NS_IMETHODIMP
nsObjectFrame::DidReflow(nsIPresContext*           aPresContext,
                         const nsHTMLReflowState*  aReflowState,
                         nsDidReflowStatus         aStatus)
{
  nsresult rv = nsObjectFrameSuper::DidReflow(aPresContext, aReflowState, aStatus);

  // The view is created hidden; once we have reflowed it and it has been
  // positioned then we show it.
  if (aStatus != NS_FRAME_REFLOW_FINISHED) 
    return rv;

  PRBool bHidden = IsHidden();

  nsIView* view = nsnull;
  GetView(aPresContext, &view);
  if (view) {
    nsCOMPtr<nsIViewManager> vm;
    view->GetViewManager(*getter_AddRefs(vm));
    if (vm)
      vm->SetViewVisibility(view, bHidden ? nsViewVisibility_kHide : nsViewVisibility_kShow);
  }

  nsPluginWindow *win = nsnull;
 
  nsCOMPtr<nsIPluginInstance> pi; 
  if (!mInstanceOwner ||
      NS_FAILED(rv = mInstanceOwner->GetWindow(win)) || 
      NS_FAILED(rv = mInstanceOwner->GetInstance(*getter_AddRefs(pi))) ||
      !pi ||
      !win)
    return rv;

  nsPluginNativeWindow *window = (nsPluginNativeWindow *)win;

#if defined(XP_MAC) || defined(XP_MACOSX) 
  mInstanceOwner->FixUpPluginWindow();
#endif // XP_MAC || XP_MACOSX

  if (bHidden)
    return rv;

  PRBool windowless = (window->type == nsPluginWindowType_Drawable);
  if(windowless)
    return rv;

  nsPoint origin;
  GetWindowOriginInPixels(aPresContext, windowless, &origin);

  window->x = origin.x;
  window->y = origin.y;

  // refresh the plugin port as well
  window->window = mInstanceOwner->GetPluginPort();

  // this will call pi->SetWindow and take care of window subclassing
  // if needed, see bug 132759
  window->CallSetWindow(pi);

  mInstanceOwner->ReleasePluginPort((nsPluginPort *)window->window);

  if (mWidget) {
    PRInt32 x = origin.x;
    PRInt32 y = origin.y;
    mWidget->Move(x, y);
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
  if ((vis != nsnull) && !vis->IsVisibleOrCollapsed())
    return NS_OK;
  
  nsIFrame * child = mFrames.FirstChild();
  if (child) {    // if we have children, we are probably not really a plugin
    nsObjectFrameSuper::Paint(aPresContext, aRenderingContext, aDirtyRect, aWhichLayer);
    return NS_OK;
  }

  // If we are painting in Print Preview do nothing....
  nsCOMPtr<nsIPrintPreviewContext> thePrintPreviewContext = do_QueryInterface(aPresContext);
  if (thePrintPreviewContext) {
    return NS_OK;
  }

  // determine if we are printing
  nsCOMPtr<nsIPrintContext> thePrinterContext = do_QueryInterface(aPresContext);
  if  (thePrinterContext) {
    // UNIX Plugins can't PP at this time, so draw an empty box
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
    PRBool windowless = PR_FALSE;
    pi->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);
    window.type  =  windowless ? nsPluginWindowType_Drawable : nsPluginWindowType_Window;

    // get a few things
    nsCOMPtr<nsIPrintSettings> printSettings;
    if (thePrinterContext) {
      thePrinterContext->GetPrintSettings(getter_AddRefs(printSettings));
      NS_ENSURE_TRUE(printSettings, NS_ERROR_FAILURE);
      printSettings->GetMarginInTwips(margin);
    }
    
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

#elif defined (XP_UNIX) && !defined(XP_MACOSX)
    // UNIX does things completely differently
    PRUnichar *printfile = nsnull;
    if (printSettings) {
      printSettings->GetToFileName(&printfile);
    }
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
    frame->DidReflow(screenPcx, nsnull, status);  // DidReflow will take care of it

    return rv;  // done with printing
  }

// Screen painting code
#if defined (XP_MAC) || defined(XP_MACOSX)
  // delegate all painting to the plugin instance.
  if ((NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) && mInstanceOwner)
      mInstanceOwner->Paint(aDirtyRect);
#elif defined (XP_PC)
  if (NS_FRAME_PAINT_LAYER_FOREGROUND == aWhichLayer) {
    nsIPluginInstance * inst;
    if (NS_OK == GetPluginInstance(inst)) {
      // Look if it's windowless
      nsPluginWindow * window;
      mInstanceOwner->GetWindow(window);

      if (window->type == nsPluginWindowType_Drawable) {
        // check if we need to call SetWindow with updated parameters
        PRBool doupdatewindow = PR_FALSE;

        // check if we need to update hdc
        PRUint32 hdc;
        aRenderingContext.RetrieveCurrentNativeGraphicData(&hdc);
        if(NS_REINTERPRET_CAST(PRUint32, window->window) != hdc) {
          window->window = NS_REINTERPRET_CAST(nsPluginPort*, hdc);
          doupdatewindow = PR_TRUE;
        }

        /*
         * Layout now has an optimized way of painting. Now we always get
         * a new drawing surface, sized to be just what's needed. Windowsless
         * plugins need a transform applied to their origin so they paint
         * in the right place. Since |SetWindow| is no longer being used
         * to tell the plugin where it is, we dispatch a NPWindow through
         * |HandleEvent| to tell the plugin when its window moved
         */

        // Get the offset of the DC
        nsTransform2D* rcTransform;
        aRenderingContext.GetCurrentTransform(rcTransform);
        nsPoint origin;
        rcTransform->GetTranslationCoord(&origin.x, &origin.y);

        if((window->x != origin.x) || (window->y != origin.y)) {
          window->x = origin.x;
          window->y = origin.y;
          doupdatewindow = PR_TRUE;
        }

        // if our location or visible area has changed, we need to tell the plugin
        if(doupdatewindow) {
#ifdef XP_WIN    // Windowless plugins on windows need a special event to update their location, see bug 135737

          // first, lets find out how big the window is, in pixels
          nsCOMPtr<nsIPresShell> shell;
          nsCOMPtr<nsIViewManager> vm;
          aPresContext->GetShell(getter_AddRefs(shell));
          if (shell) {
            shell->GetViewManager(getter_AddRefs(vm));
            if (vm) {  
              nsIView* view;
              vm->GetRootView(view);
              if (view) {
                nsCOMPtr<nsIWidget> win;
                view->GetWidget(*getter_AddRefs(win));
                if (win) {
                  nsRect visibleRect;
                  win->GetBounds(visibleRect);         
                    
                  // next, get our plugin's rect so we can intersect it with the visible rect so we
                  // can tell the plugin where and how much to paint
                  GetWindowOriginInPixels(aPresContext, window->type, &origin);
                  nsRect winlessRect = nsRect(origin, nsSize(window->width, window->height));
                  winlessRect.IntersectRect(winlessRect, visibleRect);

                  // now check our cached window and only update plugin if something has changed
                  if (mWindowlessRect != winlessRect) {
                    mWindowlessRect = winlessRect;

                    WINDOWPOS winpos;
                    memset(&winpos, 0, sizeof(winpos));
                    winpos.x = mWindowlessRect.x;
                    winpos.y = mWindowlessRect.y;
                    winpos.cx = mWindowlessRect.width;
                    winpos.cy = mWindowlessRect.height;

                    // finally, update the plugin by sending it a WM_WINDOWPOSCHANGED event
                    nsPluginEvent pluginEvent;
                    pluginEvent.event = 0x0047;
                    pluginEvent.wParam = 0;
                    pluginEvent.lParam = (uint32)&winpos;
                    PRBool eventHandled = PR_FALSE;

                    inst->HandleEvent(&pluginEvent, &eventHandled);
                  }
                }
              }
            }
          }
#endif

          inst->SetWindow(window);        
        }

        mInstanceOwner->Paint(aDirtyRect, hdc);
      }
      NS_RELEASE(inst);
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


// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener()
{
  NS_INIT_ISUPPORTS();
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener()
{
}

NS_IMPL_ISUPPORTS2(nsPluginDOMContextMenuListener, nsIDOMContextMenuListener, nsIEventListener);

nsresult nsPluginDOMContextMenuListener::Init(nsObjectFrame *aFrame)
{
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));

  // Register context menu listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMContextMenuListener> cxMenuListener;
      QueryInterface(NS_GET_IID(nsIDOMContextMenuListener), getter_AddRefs(cxMenuListener));
      if (cxMenuListener) {
        receiver->AddEventListener(NS_LITERAL_STRING("contextmenu"), cxMenuListener, PR_TRUE);
        return NS_OK;
      }
    }
  }

  return NS_ERROR_NO_INTERFACE;
}

nsresult nsPluginDOMContextMenuListener::Destroy(nsObjectFrame *aFrame)
{
  nsCOMPtr<nsIContent> content;
  aFrame->GetContent(getter_AddRefs(content));

  // Unregister context menu listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMContextMenuListener> cxMenuListener;
      QueryInterface(NS_GET_IID(nsIDOMContextMenuListener), getter_AddRefs(cxMenuListener));
      if (cxMenuListener) { 
        receiver->RemoveEventListenerByIID(cxMenuListener, NS_GET_IID(nsIDOMContextMenuListener));
      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
    }
    else NS_ASSERTION(PR_FALSE, "plugin was not an event listener");
  }
  else NS_ASSERTION(PR_FALSE, "plugin had no content");

  return NS_OK;
}

//plugin instance owner

nsPluginInstanceOwner::nsPluginInstanceOwner()
{
  NS_INIT_ISUPPORTS();

  // create nsPluginNativeWindow object, it is derived from nsPluginWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph)
    pph->NewPluginNativeWindow(&mPluginWindow);
  else
    mPluginWindow = nsnull;

  mInstance = nsnull;
  mOwner = nsnull;
  mWidget = nsnull;
  mContext = nsnull;
  mTagText = nsnull;
  mPluginHost = nsnull;
  mContentFocused = PR_FALSE;
  mWidgetVisible = PR_TRUE;
  mNumCachedAttrs = 0;
  mNumCachedParams = 0;
  mCachedAttrParamNames = nsnull;
  mCachedAttrParamValues = nsnull;
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

  for (cnt = 0; cnt < (mNumCachedAttrs + 1 + mNumCachedParams); cnt++) {
    if (mCachedAttrParamNames && mCachedAttrParamNames[cnt]) {
      PR_Free(mCachedAttrParamNames[cnt]);
      mCachedAttrParamNames[cnt] = nsnull;
    }

    if (mCachedAttrParamValues && mCachedAttrParamValues[cnt]) {
      PR_Free(mCachedAttrParamValues[cnt]);
      mCachedAttrParamValues[cnt] = nsnull;
    }
  }

  if (mCachedAttrParamNames) {
    PR_Free(mCachedAttrParamNames);
    mCachedAttrParamNames = nsnull;
  }

  if (mCachedAttrParamValues) {
    PR_Free(mCachedAttrParamValues);
    mCachedAttrParamValues = nsnull;
  }

  if (mTagText) {
    nsCRT::free(mTagText);
    mTagText = nsnull;
  }

  NS_IF_RELEASE(mWidget);
  mContext = nsnull;

#if defined(XP_UNIX) && !defined(XP_MACOSX)
  // the mem for this struct is allocated
  // by PR_MALLOC in ns4xPluginInstance.cpp:ns4xPluginInstance::SetWindow()
  if (mPluginWindow && mPluginWindow->ws_info) {
    PR_Free(mPluginWindow->ws_info);
    mPluginWindow->ws_info = nsnull;
  }
#endif

  // clean up plugin native window object
  nsCOMPtr<nsIPluginHost> ph = do_GetService(kCPluginManagerCID);
  nsCOMPtr<nsPIPluginHost> pph(do_QueryInterface(ph));
  if (pph) {
    pph->DeletePluginNativeWindow(mPluginWindow);
    mPluginWindow = nsnull;
  }
}

/*
 * nsISupports Implementation
 */

NS_IMPL_ADDREF(nsPluginInstanceOwner)
NS_IMPL_RELEASE(nsPluginInstanceOwner)

NS_INTERFACE_MAP_BEGIN(nsPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginInstanceOwner)
  NS_INTERFACE_MAP_ENTRY(nsIPluginTagInfo)
  NS_INTERFACE_MAP_ENTRY(nsIPluginTagInfo2)
  NS_INTERFACE_MAP_ENTRY(nsIJVMPluginTagInfo)
  NS_INTERFACE_MAP_ENTRY(nsIEventListener)
  NS_INTERFACE_MAP_ENTRY(nsITimerCallback)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMMouseMotionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMKeyListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMFocusListener)
  NS_INTERFACE_MAP_ENTRY(nsIScrollPositionListener)
  NS_INTERFACE_MAP_ENTRY(nsIDOMDragListener)
  NS_INTERFACE_MAP_ENTRY_AMBIGUOUS(nsISupports, nsIPluginInstanceOwner)
NS_INTERFACE_MAP_END

NS_IMETHODIMP nsPluginInstanceOwner::SetInstance(nsIPluginInstance *aInstance)
{
  NS_IF_RELEASE(mInstance);
  mInstance = aInstance;
  NS_IF_ADDREF(mInstance);

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(nsPluginWindow *&aWindow)
{
  NS_ASSERTION(mPluginWindow, "the plugin window object being returned is null");
  aWindow = mPluginWindow;
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
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  n = mNumCachedAttrs;
  names  = (const char **)mCachedAttrParamNames;
  values = (const char **)mCachedAttrParamValues;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAttribute(const char* name, const char* *result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);
  
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nsnull;

  for (int i = 0; i < mNumCachedAttrs; i++) {
    if (0 == PL_strcasecmp(mCachedAttrParamNames[i], name)) {
      *result = mCachedAttrParamValues[i];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDOMElement(nsIDOMElement* *result)
{
  NS_ENSURE_ARG_POINTER(result);
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
                                            PRUint32 aHeadersDataLen, PRBool isFile)
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

  nsCOMPtr<nsIInputStream> postDataStream;
  nsCOMPtr<nsIInputStream> headersDataStream;

  // deal with post data, either in a file or raw data, and any headers
  if (aPostData) {

    rv = NS_NewPluginPostDataStream(getter_AddRefs(postDataStream), (const char *)aPostData, aPostDataLen, isFile);

    NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin post data stream");
    if (NS_FAILED(rv)) return rv;

    if (aHeadersData) {
      rv = NS_NewPluginPostDataStream(getter_AddRefs(headersDataStream), 
                                      (const char *) aHeadersData, 
                                      aHeadersDataLen,
                                      PR_FALSE,
                                      PR_TRUE);  // last arg says we are headers, no /r/n/r/n fixup!

      NS_ASSERTION(NS_SUCCEEDED(rv),"failed in creating plugin header data stream");
      if (NS_FAILED(rv)) return rv;
    }
  }

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
    if (shell)
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
      // get the document's widget from the view manager
      // get the view manager from the pres shell, not from the view!
      // we may not have a view if we are hidden
      if (mContext) {
        nsCOMPtr<nsIPresShell> shell;
        mContext->GetShell(getter_AddRefs(shell));
        if (shell) {
          nsCOMPtr<nsIViewManager> vm;
          shell->GetViewManager(getter_AddRefs(vm));
          if (vm) {
            nsCOMPtr<nsIWidget> widget;
            rv = vm->GetWidget(getter_AddRefs(widget));            
            if(widget) {

              void** pvalue = (void**)value;
              *pvalue = (void*)widget->GetNativeData(NS_NATIVE_WINDOW);

            } else NS_ASSERTION(widget, "couldn't get doc's widget in getting doc's window handle");
          } else NS_ASSERTION(vm, "couldn't get view manager in getting doc's window handle");
        } else NS_ASSERTION(shell, "couldn't get pres shell in getting doc's window handle");
      } else NS_ASSERTION(mContext, "plugin owner has no pres context in getting doc's window handle");

      break;
    }
  }

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetTagType(nsPluginTagType *result)
{
  NS_ENSURE_ARG_POINTER(result);
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
    NS_ENSURE_ARG_POINTER(result);
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
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  n = mNumCachedParams;
  if (n) {
    names  = (const char **)(mCachedAttrParamNames + mNumCachedAttrs + 1);
    values = (const char **)(mCachedAttrParamValues + mNumCachedAttrs + 1);
  } else
    names = values = nsnull;

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetParameter(const char* name, const char* *result)
{
  NS_ENSURE_ARG_POINTER(name);
  NS_ENSURE_ARG_POINTER(result);
  
  nsresult rv = EnsureCachedAttrParamArrays();
  NS_ENSURE_SUCCESS(rv, rv);

  *result = nsnull;

  for (int i = mNumCachedAttrs + 1; i < (mNumCachedParams + 1 + mNumCachedAttrs); i++) {
    if (0 == PL_strcasecmp(mCachedAttrParamNames[i], name)) {
      *result = mCachedAttrParamValues[i];
      return NS_OK;
    }
  }

  return NS_ERROR_FAILURE;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentBase(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsresult rv = NS_OK;
  if (mDocumentBase.IsEmpty()) {
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

    rv = docURL->GetSpec(mDocumentBase);
  }
  if (rv == NS_OK)
    *result = ToNewCString(mDocumentBase);
  return rv;
}

static nsHashtable *gCharsetMap = nsnull;
typedef struct {
    char *mozName;
    char *javaName;
} moz2javaCharset;

static const moz2javaCharset charsets[] = 
{
    {"windows-1252",    "Cp1252"},
    {"IBM850",          "Cp850"},
    {"IBM852",          "Cp852"},
    {"IBM855",          "Cp855"},
    {"IBM857",          "Cp857"},
    {"IBM828",          "Cp862"},
    {"IBM864",          "Cp864"},
    {"IBM866",          "Cp866"},
    {"windows-1250",    "Cp1250"},
    {"windows-1251",    "Cp1251"},
    {"windows-1253",    "Cp1253"},
    {"windows-1254",    "Cp1254"},
    {"windows-1255",    "Cp1255"},
    {"windows-1256",    "Cp1256"},
    {"windows-1257",    "Cp1257"},
    {"windows-1258",    "Cp1258"},
    {"EUC-JP",          "EUC_JP"},
    {"EUC-KR",          "EUC_KR"},
    {"x-euc-tw",        "EUC_TW"},
    {"gb18030",         "GB18030"},
    {"x-gbk",           "GBK"},
    {"ISO-2022-JP",     "ISO2022JP"},
    {"ISO-2022-KR",     "ISO2022KR"},
    {"ISO-8859-2",      "ISO8859_2"},
    {"ISO-8859-3",      "ISO8859_3"},
    {"ISO-8859-4",      "ISO8859_4"},
    {"ISO-8859-5",      "ISO8859_5"},
    {"ISO-8859-6",      "ISO8859_6"},
    {"ISO-8859-7",      "ISO8859_7"},
    {"ISO-8859-8",      "ISO8859_8"},
    {"ISO-8859-9",      "ISO8859_9"},
    {"ISO-8859-13",     "ISO8859_13"},
    {"x-johab",         "Johab"},
    {"KOI8-R",          "KOI8_R"},
    {"TIS-620",         "MS874"},
    {"windows-936",     "MS936"},
    {"x-windows-949",   "MS949"},
    {"x-mac-arabic",    "MacArabic"},
    {"x-mac-croatian",  "MacCroatia"},
    {"x-mac-cyrillic",  "MacCyrillic"},
    {"x-mac-greek",     "MacGreek"},
    {"x-mac-hebrew",    "MacHebrew"},
    {"x-mac-icelandic", "MacIceland"},
    {"x-mac-roman",     "MacRoman"},
    {"x-mac-romanian",  "MacRomania"},
    {"x-mac-ukrainian", "MacUkraine"},
    {"Shift_JIS",       "SJIS"},
    {"TIS-620",         "TIS620"}
};
  
NS_IMETHODIMP nsPluginInstanceOwner::GetDocumentEncoding(const char* *result)
{
  NS_ENSURE_ARG_POINTER(result);
  *result = nsnull;

  nsresult rv;
  nsCOMPtr<nsIDocument> doc;
  rv = GetDocument(getter_AddRefs(doc));
  NS_ASSERTION(NS_SUCCEEDED(rv), "failed to get document");
  if (NS_FAILED(rv)) return rv;

  nsString charset;
  rv = doc->GetDocumentCharacterSet(charset);
  NS_ASSERTION(NS_SUCCEEDED(rv), "can't get charset");
  if (NS_FAILED(rv)) return rv;

  if (charset.IsEmpty()) return NS_OK;

  // common charsets and those not requiring conversion first
  if (charset == NS_LITERAL_STRING("us-acsii")) {
    *result = PL_strdup("US_ASCII");
  } else if (charset == NS_LITERAL_STRING("ISO-8859-1") ||
      !nsCRT::strncmp(charset.get(), NS_LITERAL_STRING("UTF").get(), 3)) {
    *result = ToNewUTF8String(charset);
  } else {
    if (!gCharsetMap) {
      gCharsetMap = new nsHashtable(sizeof(charsets)/sizeof(moz2javaCharset));
      if (!gCharsetMap) return NS_ERROR_OUT_OF_MEMORY;

      for (PRUint16 i = 0; i < sizeof(charsets)/sizeof(moz2javaCharset); i++) {
        nsCStringKey key(charsets[i].mozName);
        gCharsetMap->Put(&key, (void *)(charsets[i].javaName));
      }
    }
    nsCStringKey mozKey(NS_LossyConvertUCS2toASCII(charset).get());
    // if found mapping, return it; otherwise return original charset
    char *mapping = (char *)gCharsetMap->Get(&mozKey);
    *result = mapping ? PL_strdup(mapping) : ToNewUTF8String(charset);
  }

  return (*result) ? NS_OK : NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetAlignment(const char* *result)
{
  return GetAttribute("ALIGN", result);
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetWidth(PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->width;

  return NS_OK;
}
  
NS_IMETHODIMP nsPluginInstanceOwner::GetHeight(PRUint32 *result)
{
  NS_ENSURE_ARG_POINTER(result);

  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  *result = mPluginWindow->height;

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
  NS_ENSURE_ARG_POINTER(result);
  *result = NS_PTR_TO_INT32(mContext);
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetCode(const char* *result)
{
  nsresult rv;
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(rv = GetTagType(&tagType), rv);

  rv = NS_ERROR_FAILURE;
  if (nsPluginTagType_Object != tagType)
    rv = GetAttribute("CODE", result);
  if (NS_FAILED(rv))
    rv = GetParameter("CODE", result);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetCodeBase(const char* *result)
{
  nsresult rv;
  if (NS_FAILED(rv = GetAttribute("CODEBASE", result)))
    rv = GetParameter("CODEBASE", result);
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetArchive(const char* *result)
{
  nsresult rv;
  if (NS_FAILED(rv = GetAttribute("ARCHIVE", result)))
    rv = GetParameter("ARCHIVE", result);
  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetName(const char* *result)
{
  nsresult rv;
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(rv = GetTagType(&tagType), rv);

  rv = NS_ERROR_FAILURE;
  if (nsPluginTagType_Object != tagType)
    rv = GetAttribute("NAME", result);
  if (NS_FAILED(rv))
    rv = GetParameter("NAME", result);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMayScript(PRBool *result)
{
  NS_ENSURE_ARG_POINTER(result);
  nsPluginTagType tagType;  
  NS_ENSURE_SUCCESS(GetTagType(&tagType), NS_ERROR_FAILURE);

  const char* unused;
  if (nsPluginTagType_Object == tagType)
    *result = NS_SUCCEEDED(GetParameter("MAYSCRIPT", &unused)); 
  else
    *result = NS_SUCCEEDED(GetAttribute("MAYSCRIPT", &unused));

  return NS_OK;
}


// Cache the attributes and/or parameters of our tag into a single set
// of arrays to be compatible with 4.x. The attributes go first,
// followed by a PARAM/null and then any PARAM tags. Also, hold the
// cached array around for the duration of the life of the instance
// because 4.x did. See bug 111008.

nsresult nsPluginInstanceOwner::EnsureCachedAttrParamArrays()
{
  if (mCachedAttrParamValues)
    return NS_OK;

  NS_PRECONDITION(((mNumCachedAttrs + mNumCachedParams) == 0) &&
                  !mCachedAttrParamNames,
                  "re-cache of attrs/params not implemented! use the DOM "
                  "node directy instead");
  NS_ENSURE_TRUE(mOwner, NS_ERROR_NULL_POINTER);

  // first, we need to find out how much we need to allocate for our
  // arrays count up attributes
  mNumCachedAttrs = 0;

  nsCOMPtr<nsIContent> content; 
  nsresult rv = mOwner->GetContent(getter_AddRefs(content));
  NS_ENSURE_TRUE(content, rv);

  PRInt32 cattrs;
  rv = content->GetAttrCount(cattrs);
  NS_ENSURE_SUCCESS(rv, rv);

  if (cattrs < 0x0000FFFF) {
    // signed 32 bits to unsigned 16 bits conversion
    mNumCachedAttrs = NS_STATIC_CAST(PRUint16, cattrs);
  } else { 
    mNumCachedAttrs = 0xFFFE;  // minus one in case we add an extra "src" entry below
  }

  // now, we need to find all the PARAM tags that are children of us
  // however, be carefull NOT to include any PARAMs that don't have us
  // as a direct parent. For nested object (or applet) tags, be sure
  // to only round up the param tags that coorespond with THIS
  // instance. And also, weed out any bogus tags that may get in the
  // way, see bug 39609. Then, with any param tag that meet our
  // qualification, temporarly cache them in an nsISupportsArray until
  // we can figure out what size to make our fixed char* array.

  mNumCachedParams = 0;
  nsCOMPtr<nsISupportsArray> ourParams;
  rv = NS_NewISupportsArray(getter_AddRefs(ourParams));
  NS_ENSURE_SUCCESS(rv, rv);
 
  // use the DOM to get us ALL our dependant PARAM tags, even if not
  // ours
  nsCOMPtr<nsIDOMElement> mydomElement = do_QueryInterface(content);
  NS_ENSURE_TRUE(mydomElement, NS_ERROR_NO_INTERFACE);

  nsCOMPtr<nsIDOMNodeList> allParams; 

  nsCOMPtr<nsINodeInfo> ni;
  content->GetNodeInfo(*getter_AddRefs(ni));

  if (ni->NamespaceEquals(kNameSpaceID_XHTML)) {
    // For XHTML elements we need to take the namespace URI into
    // account when looking for param elements.

    NS_NAMED_LITERAL_STRING(xhtml_ns, "http://www.w3.org/1999/xhtml");

    mydomElement->GetElementsByTagNameNS(xhtml_ns, NS_LITERAL_STRING("param"),
                                         getter_AddRefs(allParams));
  } else {
    // If content is not XHTML, it must be HTML, no need to worry
    // about namespaces then...

    mydomElement->GetElementsByTagName(NS_LITERAL_STRING("param"),
                                       getter_AddRefs(allParams));
  }    

  if (allParams) {
    PRUint32 numAllParams; 
    allParams->GetLength(&numAllParams);
    // loop through every so called dependant PARAM tag to check if it
    // "belongs" to us

    for (PRUint32 i = 0; i < numAllParams; i++) {
      nsCOMPtr<nsIDOMNode> pnode;
      allParams->Item(i, getter_AddRefs(pnode));

      nsCOMPtr<nsIDOMElement> domelement = do_QueryInterface(pnode);
      if (domelement) {
        // let's NOT count up param tags that don't have a name attribute
        nsAutoString name;
        domelement->GetAttribute(NS_LITERAL_STRING("name"), name);
        if (name.Length() > 0) {
          nsCOMPtr<nsIDOMNode> parent;
          nsCOMPtr<nsIDOMHTMLObjectElement> domobject;
          nsCOMPtr<nsIDOMHTMLAppletElement> domapplet;
          pnode->GetParentNode(getter_AddRefs(parent));
          // walk up the parents of this PARAM until we find an object
          // (or applet) tag

          while (!(domobject || domapplet) && parent) {
            domobject = do_QueryInterface(parent);
            domapplet = do_QueryInterface(parent);
            nsCOMPtr<nsIDOMNode> temp;
            parent->GetParentNode(getter_AddRefs(temp));
            parent = temp;
          }

          if (domapplet || domobject) {
            if (domapplet)
              parent = do_QueryInterface(domapplet);
            else
              parent = do_QueryInterface(domobject);

            // now check to see if this PARAM's parent is us. if so,
            // cache it for later

            nsCOMPtr<nsIDOMNode> mydomNode = do_QueryInterface(mydomElement);
            if (parent == mydomNode) {
              ourParams->AppendElement(pnode);
            }
          }
        }
      }
    }
  }

  PRUint32 cparams;
  ourParams->Count(&cparams); // unsigned 32 bits to unsigned 16 bits conversion
  if (cparams < 0x0000FFFF)
    mNumCachedParams = NS_STATIC_CAST(PRUint16, cparams);
  else 
    mNumCachedParams = 0xFFFF;

  // Some plugins were never written to understand the "data" attribute of the OBJECT tag.
  // Real and WMP will not play unless they find a "src" attribute, see bug 152334.
  // Nav 4.x would simply replace the "data" with "src". Because some plugins correctly
  // look for "data", lets instead copy the "data" attribute and add another entry
  // to the bottom of the array if there isn't already a "src" specified.
  PRInt16 numRealAttrs = mNumCachedAttrs;
  nsAutoString data;
  nsCOMPtr<nsIAtom> tag;
  content->GetTag(*getter_AddRefs(tag));
  if (nsHTMLAtoms::object == tag.get() 
    && !content->HasAttr(kNameSpaceID_None, nsHTMLAtoms::src)
    && NS_CONTENT_ATTR_NOT_THERE != content->GetAttr(kNameSpaceID_None, nsHTMLAtoms::data, data)) {
      mNumCachedAttrs++;
  }

  // now lets make the arrays
  mCachedAttrParamNames  = (char **)PR_Calloc(sizeof(char *) * (mNumCachedAttrs + 1 + mNumCachedParams), 1);
  NS_ENSURE_TRUE(mCachedAttrParamNames,  NS_ERROR_OUT_OF_MEMORY);
  mCachedAttrParamValues = (char **)PR_Calloc(sizeof(char *) * (mNumCachedAttrs + 1 + mNumCachedParams), 1);
  NS_ENSURE_TRUE(mCachedAttrParamValues, NS_ERROR_OUT_OF_MEMORY);

  // let's fill in our attributes
  PRInt16 c = 0;
  for (PRInt16 index = 0; index < numRealAttrs; index++) {
    PRInt32 nameSpaceID;
    nsCOMPtr<nsIAtom> atom;
    nsCOMPtr<nsIAtom> prefix;
    content->GetAttrNameAt(index, nameSpaceID,
                           *getter_AddRefs(atom),
                           *getter_AddRefs(prefix));
    nsAutoString value;
    if (NS_CONTENT_ATTR_NOT_THERE != content->GetAttr(nameSpaceID, atom, value)) {
      nsAutoString name;
      atom->ToString(name);
      mCachedAttrParamNames [c] = ToNewUTF8String(name);
      mCachedAttrParamValues[c] = ToNewUTF8String(value);
      c++;
    }
  }

  // if the conditions above were met, copy the "data" attribute to a "src" array entry
  if (data.Length()) {
    mCachedAttrParamNames [mNumCachedAttrs-1] = ToNewUTF8String(NS_LITERAL_STRING("SRC"));
    mCachedAttrParamValues[mNumCachedAttrs-1] = ToNewUTF8String(data);
  }

  // add our PARAM and null seperator
  mCachedAttrParamNames [mNumCachedAttrs] = ToNewUTF8String(NS_LITERAL_STRING("PARAM"));
  mCachedAttrParamValues[mNumCachedAttrs] = nsnull;

  // now fill in the PARAM name/value pairs from the cached DOM nodes
  c = 0;
  for (PRInt16 idx = 0; idx < mNumCachedParams; idx++) {
    nsCOMPtr<nsIDOMElement> param = do_QueryElementAt(ourParams, idx);
    if (param) {
     nsAutoString name;
     nsAutoString value;
     param->GetAttribute(NS_LITERAL_STRING("name"), name); // check for empty done above
     param->GetAttribute(NS_LITERAL_STRING("value"), value);
     /*
      * According to the HTML 4.01 spec, at
      * http://www.w3.org/TR/html4/types.html#type-cdata
      * ''User agents may ignore leading and trailing
      * white space in CDATA attribute values (e.g., "
      * myval " may be interpreted as "myval"). Authors
      * should not declare attribute values with
      * leading or trailing white space.''
      * However, do not trim consecutive spaces as in bug 122119
      */            
     name.Trim(" \n\r\t\b", PR_TRUE, PR_TRUE, PR_FALSE);
     value.Trim(" \n\r\t\b", PR_TRUE, PR_TRUE, PR_FALSE);
     mCachedAttrParamNames [mNumCachedAttrs + 1 + c] = ToNewUTF8String(name);
     mCachedAttrParamValues[mNumCachedAttrs + 1 + c] = ToNewUTF8String(value);
     c++;                                                      // rules!
    }
  }

  return NS_OK;
}


// Here's where we forward events to plugins.

#if defined(XP_MAC) || defined(XP_MACOSX)

#if TARGET_CARBON
static void InitializeEventRecord(EventRecord* event)
{
    memset(event, 0, sizeof(EventRecord));
    ::GetGlobalMouse(&event->where);
    event->when = ::TickCount();
    event->modifiers = ::GetCurrentKeyModifiers();
}
#else
inline void InitializeEventRecord(EventRecord* event) { ::OSEventAvail(0, event); }
#endif

void nsPluginInstanceOwner::GUItoMacEvent(const nsGUIEvent& anEvent, EventRecord& aMacEvent)
{
    InitializeEventRecord(&aMacEvent);
    switch (anEvent.message) {
    case NS_FOCUS_EVENT_START:   // this is the same as NS_FOCUS_CONTENT
        aMacEvent.what = nsPluginEventType_GetFocusEvent;
        if (mOwner && mOwner->mPresContext) {
            nsCOMPtr<nsIContent> content;
            mOwner->GetContent(getter_AddRefs(content));
            if (content)
                content->SetFocus(mOwner->mPresContext);
        }
        break;
    case NS_BLUR_CONTENT:
        aMacEvent.what = nsPluginEventType_LoseFocusEvent;
        if (mOwner && mOwner->mPresContext) {
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

nsresult nsPluginInstanceOwner::ScrollPositionWillChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    if (mInstance != NULL) {
        EventRecord scrollEvent;
        InitializeEventRecord(&scrollEvent);
        scrollEvent.what = nsPluginEventType_ScrollingBeginsEvent;

        nsPluginPort* pluginPort = GetPluginPort();
        if (pluginPort) {
          nsPluginEvent pluginEvent = { &scrollEvent, nsPluginPlatformWindowRef(GetWindowFromPort(pluginPort->port)) };
        
          PRBool eventHandled = PR_FALSE;
          mInstance->HandleEvent(&pluginEvent, &eventHandled);
        }
    }
#endif
    return NS_OK;
}

nsresult nsPluginInstanceOwner::ScrollPositionDidChange(nsIScrollableView* aScrollable, nscoord aX, nscoord aY)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    if (mInstance != NULL) {
        EventRecord scrollEvent;
        InitializeEventRecord(&scrollEvent);
        scrollEvent.what = nsPluginEventType_ScrollingEndsEvent;

        nsPluginPort* pluginPort = FixUpPluginWindow();
        if (pluginPort) {
          nsPluginEvent pluginEvent = { &scrollEvent, nsPluginPlatformWindowRef(GetWindowFromPort(pluginPort->port)) };
          
          PRBool eventHandled = PR_FALSE;
          mInstance->HandleEvent(&pluginEvent, &eventHandled);
#if defined(XP_MACOSX)
          // FIXME - Only invalidate the newly revealed amount.
          // mWidget->Invalidate(PR_TRUE);
          Composite(); 
#else
          if (!eventHandled) {
            nsRect bogus(0,0,0,0);
            Paint(bogus, 0);     // send an update event to the plugin
          }
#endif
        }
    }
#endif
    return NS_OK;
}

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
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
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

        nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aFocusEvent));

        if (nsevent) {
          nsevent->PreventBubble();
        }

        return NS_ERROR_FAILURE; // means consume event
      }
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, focusEvent null");   
  }
  else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchFocusToPlugin failed, privateEvent null");   
  
  return NS_OK;
}    

/*=============== nsIDOMDragListener ======================*/
nsresult nsPluginInstanceOwner::DragEnter(nsIDOMEvent* aMouseEvent)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending drag events to the plugin, since we didn't before.
    aMouseEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
#endif
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DragOver(nsIDOMEvent* aMouseEvent)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending drag events to the plugin, since we didn't before.
    aMouseEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
#endif
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DragExit(nsIDOMEvent* aMouseEvent)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending drag events to the plugin, since we didn't before.
    aMouseEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
#endif
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DragDrop(nsIDOMEvent* aMouseEvent)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending drag events to the plugin, since we didn't before.
    aMouseEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
#endif
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DragGesture(nsIDOMEvent* aMouseEvent)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending drag events to the plugin, since we didn't before.
    aMouseEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
#endif
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
#if defined(XP_MAC) || defined(XP_MACOSX)  // send KeyPress events only on Mac
  return DispatchKeyToPlugin(aKeyEvent);
#else
  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending keypress to the plugin, since we didn't before.
    aKeyEvent->PreventDefault();
    nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aKeyEvent));
    if (nsevent) {
      nsevent->PreventBubble();
    }
    return NS_ERROR_FAILURE; // means consume event                             
  }
  return NS_OK;
#endif
}

nsresult nsPluginInstanceOwner::DispatchKeyToPlugin(nsIDOMEvent* aKeyEvent)
{
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  if (mInstance) {
    nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aKeyEvent));
    if (privateEvent) {
      nsKeyEvent* keyEvent = nsnull;
      privateEvent->GetInternalNSEvent((nsEvent**)&keyEvent);
      if (keyEvent) {
        nsEventStatus rv = ProcessEvent(*keyEvent);
        if (nsEventStatus_eConsumeNoDefault == rv) {
          aKeyEvent->PreventDefault();

          nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aKeyEvent));

          if (nsevent) {
            nsevent->PreventBubble();
          }

          return NS_ERROR_FAILURE; // means consume event
        }
      }
      else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, keyEvent null");   
    }
    else NS_ASSERTION(PR_FALSE, "nsPluginInstanceOwner::DispatchKeyToPlugin failed, privateEvent null");   
  }

  return NS_OK;
}    

/*=============== nsIMouseMotionListener ======================*/

nsresult
nsPluginInstanceOwner::MouseMove(nsIDOMEvent* aMouseEvent)
{
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  // don't send mouse events if we are hiddden
  if (!mWidgetVisible)
    return NS_OK;

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
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  // if the plugin is windowless, we need to set focus ourselves
  // otherwise, we might not get key events
  if (mPluginWindow && mPluginWindow->type == nsPluginWindowType_Drawable) {
    nsCOMPtr<nsIContent> content;
    mOwner->GetContent(getter_AddRefs(content));
    if (content)
      content->SetFocus(mContext);
  }

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
#if !(defined(XP_MAC) || defined(XP_MACOSX))
  if (!mPluginWindow || nsPluginWindowType_Window == mPluginWindow->type)
    return NS_ERROR_FAILURE; // means consume event
  // continue only for cases without child window
#endif

  // don't send mouse events if we are hiddden
  if (!mWidgetVisible)
    return NS_OK;

  nsCOMPtr<nsIPrivateDOMEvent> privateEvent(do_QueryInterface(aMouseEvent));
  if (privateEvent) {
    nsMouseEvent* mouseEvent = nsnull;
    privateEvent->GetInternalNSEvent((nsEvent**)&mouseEvent);
    if (mouseEvent) {
      nsEventStatus rv = ProcessEvent(*mouseEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aMouseEvent->PreventDefault();

        nsCOMPtr<nsIDOMNSEvent> nsevent(do_QueryInterface(aMouseEvent));

        if (nsevent) {
          nsevent->PreventBubble();
        }

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
  // printf("nsGUIEvent.message: %d\n", anEvent.message);
  nsEventStatus rv = nsEventStatus_eIgnore;
  if (!mInstance)   // if mInstance is null, we shouldn't be here
    return rv;

#if defined(XP_MAC) || defined(XP_MACOSX)
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
        PRBool eventHandled = PR_FALSE;
        nsPluginPort* pluginPort = FixUpPluginWindow();
        if (pluginPort) {
          nsPluginEvent pluginEvent = { event, nsPluginPlatformWindowRef(GetWindowFromPort(pluginPort->port)) };
          mInstance->HandleEvent(&pluginEvent, &eventHandled);
        }
        if (eventHandled && !(anEvent.message == NS_MOUSE_LEFT_BUTTON_DOWN && !mContentFocused))
            rv = nsEventStatus_eConsumeNoDefault;
    }
#endif

#ifdef XP_WIN
        // this code supports windowless plugins
        nsPluginEvent * pPluginEvent = (nsPluginEvent *)anEvent.nativeMsg;
        // we can get synthetic events from the nsEventStateManager... these have no nativeMsg
        if (pPluginEvent != nsnull) {
            PRBool eventHandled = PR_FALSE;
            mInstance->HandleEvent(pPluginEvent, &eventHandled);
            if (eventHandled)
                rv = nsEventStatus_eConsumeNoDefault;
        }
#endif

  return rv;
}

nsresult
nsPluginInstanceOwner::Destroy()
{
  nsCOMPtr<nsIContent> content;
  mOwner->GetContent(getter_AddRefs(content));

  // stop the timer explicitly to reduce reference count.
  CancelTimer();

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(mOwner);    
    NS_RELEASE(mCXMenuListener);
  }

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

  // Unregister drag event listener;
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMDragListener> dragListener;
      QueryInterface(NS_GET_IID(nsIDOMDragListener), getter_AddRefs(dragListener));
      if (dragListener) { 
        receiver->RemoveEventListener(NS_LITERAL_STRING("dragdrop"), dragListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("dragover"), dragListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("dragexit"), dragListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("dragenter"), dragListener, PR_TRUE);
        receiver->RemoveEventListener(NS_LITERAL_STRING("draggesture"), dragListener, PR_TRUE);
      }
      else NS_ASSERTION(PR_FALSE, "Unable to remove event listener for plugin");
    }
    else NS_ASSERTION(PR_FALSE, "plugin was not an event listener");
  }
  else NS_ASSERTION(PR_FALSE, "plugin had no content");

  // Unregister scroll position listener
  if (mContext) {
    nsCOMPtr<nsIPresShell> presShell;
    mContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsIScrollableView* scrollingView = nsnull;
        vm->GetRootScrollableView(&scrollingView);
        if (scrollingView) {
          scrollingView->RemoveScrollPositionListener((nsIScrollPositionListener *)this);
        }
      }
    }
  }

  mOwner = nsnull; // break relationship between frame and plugin instance owner

  return NS_OK;
}

// Paints are handled differently, so we just simulate an update event.

void nsPluginInstanceOwner::Paint(const nsRect& aDirtyRect, PRUint32 ndc)
{
  if(!mInstance)
    return;
 
#if defined(XP_MAC) || defined(XP_MACOSX)
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

  nsPluginPort* pluginPort = FixUpPluginWindow();
  if (pluginPort) {
    EventRecord updateEvent;
    InitializeEventRecord(&updateEvent);
    updateEvent.what = updateEvt;
    updateEvent.message = UInt32(GetWindowFromPort(pluginPort->port));

    GrafPtr oldPort;
    ::GetPort(&oldPort);
    ::SetPort((GrafPtr)pluginPort->port);

    nsPluginEvent pluginEvent = { &updateEvent, nsPluginPlatformWindowRef(GetWindowFromPort(pluginPort->port)) };
    PRBool eventHandled = PR_FALSE;
    mInstance->HandleEvent(&pluginEvent, &eventHandled);

    ::SetPort(oldPort);
  }
#endif

#ifdef XP_WIN
  nsPluginWindow * window;
  GetWindow(window);
  nsRect relDirtyRect = nsRect(aDirtyRect.x, aDirtyRect.y, aDirtyRect.width, aDirtyRect.height);
  nsRect relDirtyRectInPixels;
  ConvertTwipsToPixels(*mContext, relDirtyRect, relDirtyRectInPixels);

  // we got dirty rectangle in relative window coordinates, but we
  // need it in absolute units and in the (left, top, right, bottom) form
  RECT drc;
  drc.left   = relDirtyRectInPixels.x + window->x;
  drc.top    = relDirtyRectInPixels.y + window->y;
  drc.right  = drc.left + relDirtyRectInPixels.width;
  drc.bottom = drc.top + relDirtyRectInPixels.height;

  nsPluginEvent pluginEvent;
  pluginEvent.event = 0x000F; //!!! This is bad, but is it better to include <windows.h> for WM_PAINT only?
  pluginEvent.wParam = (uint32)ndc;
  pluginEvent.lParam = (uint32)&drc;
  PRBool eventHandled = PR_FALSE;
  mInstance->HandleEvent(&pluginEvent, &eventHandled);
#endif
}

// Here's how we give idle time to plugins.

NS_IMETHODIMP nsPluginInstanceOwner::Notify(nsITimer* /* timer */)
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    // validate the plugin clipping information by syncing the plugin window info to
    // reflect the current widget location. This makes sure that everything is updated
    // correctly in the event of scrolling in the window.
    if (mInstance != NULL) {
      nsPluginPort* pluginPort = FixUpPluginWindow();
      if (pluginPort) {
        EventRecord idleEvent;
        InitializeEventRecord(&idleEvent);
        idleEvent.what = nullEvent;

        // give a bogus 'where' field of our null event when hidden, so Flash
        // won't respond to mouse moves in other tabs, see bug 120875
        if (!mWidgetVisible)
          idleEvent.where.h = idleEvent.where.v = 20000;
        
        nsPluginEvent pluginEvent = { &idleEvent, nsPluginPlatformWindowRef(GetWindowFromPort(pluginPort->port)) };
        
        PRBool eventHandled = PR_FALSE;
        mInstance->HandleEvent(&pluginEvent, &eventHandled);
      }
    }
#endif
    return NS_OK;
}

void nsPluginInstanceOwner::StartTimer()
{
#if defined(XP_MAC) || defined(XP_MACOSX)
    nsresult rv;

    // start a periodic timer to provide null events to the plugin instance.
    if (!mPluginTimer) {
      mPluginTimer = do_CreateInstance("@mozilla.org/timer;1", &rv);
      if (rv == NS_OK)
        rv = mPluginTimer->InitWithCallback(this, 1020 / 60, nsITimer::TYPE_REPEATING_SLACK);
    }
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

  // register context menu listener
  mCXMenuListener = new nsPluginDOMContextMenuListener();
  if (mCXMenuListener) {    
    NS_ADDREF(mCXMenuListener);    
    mCXMenuListener->Init(aFrame);
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
      }
    }
  }

  // Register drag listener
  if (content) {
    nsCOMPtr<nsIDOMEventReceiver> receiver(do_QueryInterface(content));
    if (receiver) {
      nsCOMPtr<nsIDOMDragListener> dragListener;
      QueryInterface(NS_GET_IID(nsIDOMDragListener), getter_AddRefs(dragListener));
      if (dragListener) {
        receiver->AddEventListener(NS_LITERAL_STRING("dragdrop"), dragListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("dragover"), dragListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("dragexit"), dragListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("dragenter"), dragListener, PR_TRUE);
        receiver->AddEventListener(NS_LITERAL_STRING("draggesture"), dragListener, PR_TRUE);
      }
    }
  }
  
  // Register scroll position listener
  if (mContext) {
    nsCOMPtr<nsIPresShell> presShell;
    mContext->GetShell(getter_AddRefs(presShell));
    if (presShell) {
      nsCOMPtr<nsIViewManager> vm;
      presShell->GetViewManager(getter_AddRefs(vm));
      if (vm) {
        nsIScrollableView* scrollingView = nsnull;
        vm->GetRootScrollableView(&scrollingView);
        if (scrollingView) {
          scrollingView->AddScrollPositionListener((nsIScrollPositionListener *)this);
        }
      }
    }
  }

  return NS_OK; 
}

nsPluginPort* nsPluginInstanceOwner::GetPluginPort()
{
//!!! Port must be released for windowless plugins on Windows, because it is HDC !!!

  nsPluginPort* result = NULL;
    if (mWidget != NULL)
  {
#ifdef XP_WIN
    if(mPluginWindow && mPluginWindow->type == nsPluginWindowType_Drawable)
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
    if(mPluginWindow && mPluginWindow->type == nsPluginWindowType_Drawable)
      mWidget->FreeNativeData((HDC)pluginPort, NS_NATIVE_GRAPHIC);
  }
#endif
}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void)
{
  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  nsIView   *view;
  nsresult  rv = NS_ERROR_FAILURE;
  float p2t;

  if (nsnull != mOwner)
  {
    // Create view if necessary

    mOwner->GetView(mContext, &view);

    if (nsnull == view || nsnull == mWidget)
    {
      PRBool windowless = PR_FALSE;

      mInstance->GetValue(nsPluginInstanceVariable_WindowlessBool, (void *)&windowless);

      // always create widgets in Twips, not pixels
      mContext->GetScaledPixelsToTwips(&p2t);
      rv = mOwner->CreateWidget(mContext,
                                NSIntPixelsToTwips(mPluginWindow->width, p2t),
                                NSIntPixelsToTwips(mPluginWindow->height, p2t),
                                windowless);
      if (NS_OK == rv)
      {
        mOwner->GetView(mContext, &view);
        if (view)
        {
          view->GetWidget(mWidget);
          PRBool fTransparent = PR_FALSE;
          mInstance->GetValue(nsPluginInstanceVariable_TransparentBool, (void *)&fTransparent);
          
          nsCOMPtr<nsIViewManager> vm;
          view->GetViewManager(*getter_AddRefs(vm));
          if (vm) {
            vm->SetViewContentTransparency(view, fTransparent);
          }
        }

        if (PR_TRUE == windowless)
        {
          mPluginWindow->type = nsPluginWindowType_Drawable;
          mPluginWindow->window = nsnull; // this needs to be a HDC according to the spec,
                                         // but I do not see the right way to release it
                                         // so let's postpone passing HDC till paint event
                                         // when it is really needed. Change spec?
        }
        else if (mWidget)
        {
          mWidget->Resize(mPluginWindow->width, mPluginWindow->height, PR_FALSE);

          
          // mPluginWindow->type is used in |GetPluginPort| so it must be initilized first
          mPluginWindow->type = nsPluginWindowType_Window;
          mPluginWindow->window = GetPluginPort();

          // start the idle timer.
          StartTimer();
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

// convert frame coordinates from twips to pixels
static void ConvertTwipsToPixels(nsIPresContext& aPresContext, nsRect& aTwipsRect, nsRect& aPixelRect)
{
  float t2p;
  aPresContext.GetTwipsToPixels(&t2p);
  aPixelRect.x = NSTwipsToIntPixels(aTwipsRect.x, t2p);
  aPixelRect.y = NSTwipsToIntPixels(aTwipsRect.y, t2p);
  aPixelRect.width = NSTwipsToIntPixels(aTwipsRect.width, t2p);
  aPixelRect.height = NSTwipsToIntPixels(aTwipsRect.height, t2p);
}

  // Mac specific code to fix up the port location and clipping region
#if defined(XP_MAC) || defined(XP_MACOSX)
  // calculate the absolute position and clip for a widget 
  // and use other windows in calculating the clip
static void GetWidgetPosClipAndVis(nsIWidget* aWidget,nscoord& aAbsX, nscoord& aAbsY,
                                nsRect& aClipRect, PRBool& aIsVisible) 
{
  if (aIsVisible)
    aWidget->IsVisible(aIsVisible);

  aWidget->GetBounds(aClipRect); 
  aAbsX = aClipRect.x; 
  aAbsY = aClipRect.y; 
  
  nscoord ancestorX = -aClipRect.x, ancestorY = -aClipRect.y; 
  // Calculate clipping relative to the widget passed in 
  aClipRect.x = 0; 
  aClipRect.y = 0; 

  // Gather up the absolute position of the widget, clip window, and visibilty 
  nsCOMPtr<nsIWidget> widget = getter_AddRefs(aWidget->GetParent());
  while (widget != nsnull) { 
    if (aIsVisible)
      widget->IsVisible(aIsVisible);

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

  // if we are not visible, clear out the plugin's clip so it won't paint
  if (!aIsVisible)
    aClipRect.Empty();
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
     nscoord x, y;
     view->GetPosition(&x, &y);
     aAbs.x += x;
     aAbs.y += y;
    }
   
   nsRect widgetBounds;
   aContainerWidget->GetBounds(widgetBounds); 
  } else {
   NS_ASSERTION(PR_FALSE, "the object frame does not have a view");
  }

  // Add relative coordinate to the absolute coordinate that has been calculated
  aAbs += aRel;
}
#endif // DO_DIRTY_INTERSECT

inline PRUint16 COLOR8TOCOLOR16(PRUint8 color8)
{
	return (color8 << 8) | color8;	/* (color8 * 257) == (color8 * 0x0101) */
}

nsPluginPort* nsPluginInstanceOwner::FixUpPluginWindow()
{
  if (!mWidget || !mPluginWindow)
    return nsnull;

  nsPluginPort* pluginPort = GetPluginPort(); 
  nscoord absWidgetX = 0;
  nscoord absWidgetY = 0;
  nsRect widgetClip(0,0,0,0);

#if defined(MOZ_WIDGET_COCOA)
  if (!pluginPort)
    return nsnull;
#endif

  // first, check our view for CSS visibility style
  nsIView *view;
  mOwner->GetView(mContext, &view);
  nsViewVisibility vis;
  view->GetVisibility(vis);
  PRBool isVisible = (vis == nsViewVisibility_kShow) ? PR_TRUE : PR_FALSE;
  
  GetWidgetPosClipAndVis(mWidget,absWidgetX,absWidgetY,widgetClip,isVisible);

#if defined(MOZ_WIDGET_COCOA)
  // set the port coordinates
  mPluginWindow->x = -pluginPort->portx;
  mPluginWindow->y = -pluginPort->porty;
  widgetClip.x += mPluginWindow->x - absWidgetX;
  widgetClip.y += mPluginWindow->y - absWidgetY;
#else
  // set the port coordinates
  mPluginWindow->x = absWidgetX;
  mPluginWindow->y = absWidgetY;
#endif

  // fix up the clipping region
  mPluginWindow->clipRect.top = widgetClip.y;
  mPluginWindow->clipRect.left = widgetClip.x;
  mPluginWindow->clipRect.bottom =  mPluginWindow->clipRect.top + widgetClip.height;
  mPluginWindow->clipRect.right =  mPluginWindow->clipRect.left + widgetClip.width; 

  // the Mac widget doesn't set the background color right away!!
  // the background color needs to be set here on the plugin port
  GrafPtr savePort;
  ::GetPort(&savePort);  // save our current port
  ::SetPort((GrafPtr)pluginPort->port);

  nscolor color = mWidget->GetBackgroundColor();
  RGBColor macColor;
  macColor.red   = COLOR8TOCOLOR16(NS_GET_R(color));  // convert to Mac color
  macColor.green = COLOR8TOCOLOR16(NS_GET_G(color));
  macColor.blue  = COLOR8TOCOLOR16(NS_GET_B(color));
  ::RGBBackColor(&macColor);
  ::SetPort(savePort);  // restore port

  if (mWidgetVisible != isVisible) {
    mWidgetVisible = isVisible;
    // must do this to disable async Java Applet drawing
    if (isVisible) {
      mInstance->SetWindow(mPluginWindow);
    } else {
      mInstance->SetWindow(nsnull);
      // switching states, do not draw
      pluginPort = nsnull;
    }
  }
  
#if defined(MOZ_WIDGET_COCOA)
  if (!mWidgetVisible) {
    mPluginWindow->clipRect.right = mPluginWindow->clipRect.left;
    mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
  }
#endif

  return pluginPort;
}

void nsPluginInstanceOwner::Composite()
{
  //no reference count on view
  nsIView* view;
  nsresult rv = mOwner->GetView(mContext, &view);

  if (NS_SUCCEEDED(rv) && view) {
    nsIViewManager* manager;
    rv = view->GetViewManager(manager);

    //set flags to not do a synchronous update, force update does the redraw
    if (NS_SUCCEEDED(rv) && manager) {
      rv = manager->UpdateView(view, NS_VMREFRESH_IMMEDIATE);
      NS_RELEASE(manager);
    }
  }
}

#endif // XP_MAC || XP_MACOSX
 
