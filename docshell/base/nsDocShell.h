/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Mozilla browser.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Travis Bogard <travis@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef nsDocShell_h__
#define nsDocShell_h__

#include "nsIPresShell.h"
#include "nsIDOMNode.h"
#include "nsIDOMNodeList.h"
#include "nsIViewManager.h"
#include "nsIScrollableView.h"
#include "nsIContentViewer.h"
#include "nsIPref.h"
#include "nsVoidArray.h"
#include "nsIScriptContext.h"
#include "nsITimer.h"

#include "nsCDocShell.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIContentViewerContainer.h"
#include "nsIDeviceContext.h"

#include "nsIDocumentLoader.h"
#include "nsIURILoader.h"
#include "nsIEditorDocShell.h"

#include "nsWeakReference.h"

// Local Includes
#include "nsDSURIContentListener.h"
#include "nsDocShellEditorData.h"

// Helper Classes
#include "nsCOMPtr.h"
#include "nsPoint.h" // mCurrent/mDefaultScrollbarPreferences
#include "nsString.h"

// Threshold value in ms for META refresh based redirects
#define REFRESH_REDIRECT_TIMER 15000

// Interfaces Needed
#include "nsIDocumentCharsetInfo.h"
#include "nsIDocCharset.h"
#include "nsIGlobalHistory2.h"
#include "nsIInterfaceRequestor.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPrompt.h"
#include "nsIRefreshURI.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptGlobalObjectOwner.h"
#include "nsISHistory.h"
#include "nsILayoutHistoryState.h"
#include "nsIStringBundle.h"
#include "nsISupportsArray.h"
#include "nsIWebNavigation.h"
#include "nsIWebPageDescriptor.h"
#include "nsIWebProgressListener.h"
#include "nsISHContainer.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellHistory.h"
#include "nsIURIFixup.h"
#include "nsIWebBrowserFind.h"
#include "nsIHttpChannel.h"
#include "nsDocShellTransferableHooks.h"


#define MAKE_LOAD_TYPE(type, flags) ((type) | ((flags) << 16))

/* load commands were moved to nsIDocShell.h */

/* load types are legal combinations of load commands and flags 
 *  
 * NOTE:
 *  Remember to update the IsValidLoadType function below if you change this
 *  enum to ensure bad flag combinations will be rejected.
 */
enum LoadType {
    LOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_NORMAL_REPLACE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_REPLACE_HISTORY),
    LOAD_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_HISTORY, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_RELOAD_NORMAL = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_NONE),
    LOAD_RELOAD_BYPASS_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE),
    LOAD_RELOAD_BYPASS_PROXY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_RELOAD_BYPASS_PROXY_AND_CACHE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_BYPASS_CACHE | nsIWebNavigation::LOAD_FLAGS_BYPASS_PROXY),
    LOAD_LINK = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_LINK),
    LOAD_REFRESH = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_IS_REFRESH),
    LOAD_RELOAD_CHARSET_CHANGE = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_RELOAD, nsIWebNavigation::LOAD_FLAGS_CHARSET_CHANGE),
    LOAD_BYPASS_HISTORY = MAKE_LOAD_TYPE(nsIDocShell::LOAD_CMD_NORMAL, nsIWebNavigation::LOAD_FLAGS_BYPASS_HISTORY)
    // NOTE: Adding a new value? Remember to update IsValidLoadType!
};
static inline PRBool IsValidLoadType(PRUint32 aLoadType)
{
    switch (aLoadType)
    {
    case LOAD_NORMAL:
    case LOAD_NORMAL_REPLACE:
    case LOAD_HISTORY:
    case LOAD_RELOAD_NORMAL:
    case LOAD_RELOAD_BYPASS_CACHE:
    case LOAD_RELOAD_BYPASS_PROXY:
    case LOAD_RELOAD_BYPASS_PROXY_AND_CACHE:
    case LOAD_LINK:
    case LOAD_REFRESH:
    case LOAD_RELOAD_CHARSET_CHANGE:
    case LOAD_BYPASS_HISTORY:
        return PR_TRUE;
    }
    return PR_FALSE;
}

/* internally used ViewMode types */
enum ViewMode {
    viewNormal = 0x0,
    viewSource = 0x1
};

//*****************************************************************************
//***    nsRefreshTimer
//*****************************************************************************

class nsRefreshTimer : public nsITimerCallback
{
public:
    nsRefreshTimer();

    NS_DECL_ISUPPORTS
    NS_DECL_NSITIMERCALLBACK

    PRInt32 GetDelay() { return mDelay ;}

    nsCOMPtr<nsIDocShell> mDocShell;
    nsCOMPtr<nsIURI>      mURI;
    PRInt32               mDelay;
    PRPackedBool          mRepeat;
    PRPackedBool          mMetaRefresh;
    
protected:
    virtual ~nsRefreshTimer();
};

//*****************************************************************************
//***    nsDocShell
//*****************************************************************************

class nsDocShell : public nsIDocShell,
                   public nsIDocShellTreeItem, 
                   public nsIDocShellTreeNode,
                   public nsIDocShellHistory,
                   public nsIWebNavigation,
                   public nsIBaseWindow, 
                   public nsIScrollable, 
                   public nsITextScroll, 
                   public nsIDocCharset, 
                   public nsIContentViewerContainer,
                   public nsIInterfaceRequestor,
                   public nsIScriptGlobalObjectOwner,
                   public nsIRefreshURI,
                   public nsIWebProgressListener,
                   public nsIEditorDocShell,
                   public nsIWebPageDescriptor,
                   public nsSupportsWeakReference
{
friend class nsDSURIContentListener;

public:
    // Object Management
    nsDocShell();

    NS_DECL_ISUPPORTS

    NS_DECL_NSIDOCSHELL
    NS_DECL_NSIDOCSHELLTREEITEM
    NS_DECL_NSIDOCSHELLTREENODE
    NS_DECL_NSIDOCSHELLHISTORY
    NS_DECL_NSIWEBNAVIGATION
    NS_DECL_NSIBASEWINDOW
    NS_DECL_NSISCROLLABLE
    NS_DECL_NSITEXTSCROLL
    NS_DECL_NSIDOCCHARSET
    NS_DECL_NSIINTERFACEREQUESTOR
    NS_DECL_NSIWEBPROGRESSLISTENER
    NS_DECL_NSIREFRESHURI
    NS_DECL_NSICONTENTVIEWERCONTAINER
    NS_DECL_NSIEDITORDOCSHELL
    NS_DECL_NSIWEBPAGEDESCRIPTOR

    nsresult SetLoadCookie(nsISupports * aCookie);
    nsresult GetLoadCookie(nsISupports ** aResult);
    nsDocShellInfoLoadType ConvertLoadTypeToDocShellLoadInfo(PRUint32 aLoadType);
    PRUint32 ConvertDocShellLoadInfoToLoadType(nsDocShellInfoLoadType aDocShellLoadType);

    // nsIScriptGlobalObjectOwner methods
    virtual nsIScriptGlobalObject* GetScriptGlobalObject();
protected:
    // Object Management
    virtual ~nsDocShell();
    NS_IMETHOD DestroyChildren();

    // Content Viewer Management
    NS_IMETHOD EnsureContentViewer();
    NS_IMETHOD EnsureDeviceContext();
    NS_IMETHOD CreateAboutBlankContentViewer();
    NS_IMETHOD CreateContentViewer(const char * aContentType, 
        nsIRequest * request, nsIStreamListener ** aContentHandler);
    NS_IMETHOD NewContentViewerObj(const char * aContentType, 
        nsIRequest * request, nsILoadGroup * aLoadGroup, 
        nsIStreamListener ** aContentHandler, nsIContentViewer ** aViewer);
    NS_IMETHOD SetupNewViewer(nsIContentViewer * aNewViewer);

    NS_IMETHOD GetEldestPresContext(nsIPresContext** aPresContext);
    void GetCurrentDocumentOwner(nsISupports ** aOwner);
    virtual nsresult DoURILoad(nsIURI * aURI,
                               nsIURI * aReferrer,
                               nsISupports * aOwner,
                               const char * aTypeHint,
                               nsIInputStream * aPostData,
                               nsIInputStream * aHeadersData,
                               PRBool firstParty,
                               nsIDocShell ** aDocShell,
                               nsIRequest ** aRequest);
    NS_IMETHOD AddHeadersToChannel(nsIInputStream * aHeadersData, 
                                  nsIChannel * aChannel);
    virtual nsresult DoChannelLoad(nsIChannel * aChannel,
                                   nsIURILoader * aURILoader);
    NS_IMETHOD ScrollIfAnchor(nsIURI * aURI, PRBool * aWasAnchor, PRUint32 aLoadType, nscoord *cx, nscoord *cy);
    NS_IMETHOD OnLoadingSite(nsIChannel * aChannel);

    NS_IMETHOD OnNewURI(nsIURI * aURI, nsIChannel * aChannel, PRUint32 aLoadType);

    virtual void SetReferrerURI(nsIURI * aURI);

    // Session History
    virtual PRBool ShouldAddToSessionHistory(nsIURI * aURI);
    virtual nsresult AddToSessionHistory(nsIURI * aURI, nsIChannel * aChannel,
        nsISHEntry ** aNewEntry);
    nsresult DoAddChildSHEntry(nsISHEntry* aNewEntry, PRInt32 aChildOffset);

    NS_IMETHOD LoadHistoryEntry(nsISHEntry * aEntry, PRUint32 aLoadType);
    NS_IMETHOD PersistLayoutHistoryState();
    NS_IMETHOD CloneAndReplace(nsISHEntry * srcEntry, PRUint32 aCloneID,
        nsISHEntry * areplaceEntry, nsISHEntry ** destEntry);
    nsresult GetRootSessionHistory(nsISHistory ** aReturn);
    nsresult GetHttpChannel(nsIChannel * aChannel, nsIHttpChannel ** aReturn);
    PRBool ShouldDiscardLayoutState(nsIHttpChannel * aChannel);
    
    // Global History
    nsresult AddToGlobalHistory(nsIURI * aURI, PRBool aRedirect);

    // Helper Routines
    NS_IMETHOD GetPromptAndStringBundle(nsIPrompt ** aPrompt,
        nsIStringBundle ** aStringBundle);
    NS_IMETHOD GetChildOffset(nsIDOMNode * aChild, nsIDOMNode * aParent,
        PRInt32 * aOffset);
    NS_IMETHOD GetRootScrollableView(nsIScrollableView ** aOutScrollView);
    NS_IMETHOD EnsureContentListener();
    NS_IMETHOD EnsureScriptEnvironment();
    NS_IMETHOD EnsureEditorData();
    nsresult   EnsureTransferableHookData();
    NS_IMETHOD EnsureFind();
    NS_IMETHOD RefreshURIFromQueue();
    NS_IMETHOD DisplayLoadError(nsresult aError, nsIURI *aURI, const PRUnichar *aURL);
    NS_IMETHOD LoadErrorPage(nsIURI *aURI, const PRUnichar *aURL, const PRUnichar *aPage, const PRUnichar *aDescription);
    PRBool IsPrintingOrPP(PRBool aDisplayErrorDialog = PR_TRUE);

    nsresult SetBaseUrlForWyciwyg(nsIContentViewer * aContentViewer);

    static  inline  PRUint32
    PRTimeToSeconds(PRTime t_usec)
    {
      PRTime usec_per_sec;
      PRUint32 t_sec;
      LL_I2L(usec_per_sec, PR_USEC_PER_SEC);
      LL_DIV(t_usec, t_usec, usec_per_sec);
      LL_L2I(t_sec, t_usec);
      return t_sec;
    }



    virtual nsresult FindTarget(const PRUnichar *aTargetName,
                                PRBool *aIsNewWindow,
                                nsIDocShell **aResult);

    PRBool IsFrame();

    //
    // Helper method that is called when a new document (including any
    // sub-documents - ie. frames) has been completely loaded.
    //
    virtual nsresult EndPageLoad(nsIWebProgress * aProgress,
                                 nsIChannel * aChannel,
                                 nsresult aResult);

    nsresult CheckLoadingPermissions();

protected:
    PRPackedBool               mAllowSubframes;
    PRPackedBool               mAllowPlugins;
    PRPackedBool               mAllowJavascript;
    PRPackedBool               mAllowMetaRedirects;
    PRPackedBool               mAllowImages;
    PRPackedBool               mFocusDocFirst;
    PRPackedBool               mHasFocus;
    PRPackedBool               mCreatingDocument; // (should be) debugging only
    PRPackedBool               mUseErrorPages;
    PRPackedBool               mAllowAuth;

    PRPackedBool               mFiredUnloadEvent;

    // this flag is for bug #21358. a docshell may load many urls
    // which don't result in new documents being created (i.e. a new
    // content viewer) we want to make sure we don't call a on load
    // event more than once for a given content viewer.
    PRPackedBool               mEODForCurrentDocument; 
    PRPackedBool               mURIResultedInDocument;

    PRPackedBool               mIsBeingDestroyed;

    // used to keep track of whether user click links should be handle
    // by us or immediately kicked out to an external
    // application. mscott: eventually i'm going to try to fold this
    // up into the uriloader where it belongs but i haven't figured
    // out how to do that yet.
    PRPackedBool               mUseExternalProtocolHandler;

    // Disallow popping up new windows with target=
    PRPackedBool               mDisallowPopupWindows;

    // Validate window targets to prevent frameset spoofing
    PRPackedBool               mValidateOrigin;

    PRPackedBool               mIsExecutingOnLoadHandler;

    // Indicates that a DocShell in this "docshell tree" is printing
    PRPackedBool               mIsPrintingOrPP;

    PRUint32                   mAppType;

    // Offset in the parent's child list.
    PRInt32                    mChildOffset;

    PRUint32                   mBusyFlags;

    PRInt32                    mMarginWidth;
    PRInt32                    mMarginHeight;
    PRInt32                    mItemType;

    PRUint32                   mLoadType;

    nsString                   mName;
    nsString                   mTitle;
    /**
     * Content-Type Hint of the most-recently initiated load. Used for
     * session history entries.
     */
    nsCString                  mContentTypeHint;
    nsVoidArray                mChildren;
    nsCOMPtr<nsISupportsArray> mRefreshURIList;
    nsDSURIContentListener *   mContentListener;
    nsRect                     mBounds; // Dimensions of the docshell
    nsCOMPtr<nsIContentViewer> mContentViewer;
    nsCOMPtr<nsIDocumentCharsetInfo> mDocumentCharsetInfo;
    nsCOMPtr<nsIDeviceContext> mDeviceContext;
    nsCOMPtr<nsIDocumentLoader>mDocLoader;
    nsCOMPtr<nsIWidget>        mParentWidget;
    nsCOMPtr<nsIPref>          mPrefs;
    nsCOMPtr<nsIURI>           mCurrentURI;
    nsCOMPtr<nsIURI>           mReferrerURI;
    nsCOMPtr<nsIScriptGlobalObject> mScriptGlobal;
    nsCOMPtr<nsIScriptContext> mScriptContext;
    nsCOMPtr<nsISHistory>      mSessionHistory;
    nsCOMPtr<nsIGlobalHistory2> mGlobalHistory;
    nsCOMPtr<nsISupports>      mLoadCookie; // the load cookie associated with the window context.
    nsCOMPtr<nsIWebBrowserFind> mFind;
    nsPoint                    mCurrentScrollbarPref; // this document only
    nsPoint                    mDefaultScrollbarPref; // persistent across doc loads
    // Reference to the SHEntry for this docshell until the page is destroyed.
    // Somebody give me better name
    nsCOMPtr<nsISHEntry>       mOSHE; 
    // Reference to the SHEntry for this docshell until the page is loaded
    // Somebody give me better name
    nsCOMPtr<nsISHEntry>       mLSHE;

    // Editor stuff
    nsDocShellEditorData*      mEditorData;          // editor data, if any

    // Transferable hooks/callbacks
    nsCOMPtr<nsIClipboardDragDropHookList>  mTransferableHookData;

    // WEAK REFERENCES BELOW HERE.
    // Note these are intentionally not addrefd.  Doing so will create a cycle.
    // For that reasons don't use nsCOMPtr.

    nsIDocShellTreeItem *      mParent;  // Weak Reference
    nsIDocShellTreeOwner *     mTreeOwner; // Weak Reference
    nsIChromeEventHandler *    mChromeEventHandler; //Weak Reference

    static nsIURIFixup *sURIFixup;


public:
    class InterfaceRequestorProxy : public nsIInterfaceRequestor {
    public:
        InterfaceRequestorProxy(nsIInterfaceRequestor* p);
        virtual ~InterfaceRequestorProxy();
        NS_DECL_ISUPPORTS
        NS_DECL_NSIINTERFACEREQUESTOR
 
    protected:
        InterfaceRequestorProxy() {}
        nsWeakPtr mWeakPtr;
    };
};

#endif /* nsDocShell_h__ */
