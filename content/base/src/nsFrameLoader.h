/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Johnny Stenback <jst@netscape.com> (original author)
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

/*
 * Class for managing loading of a subframe (creation of the docshell,
 * handling of loads in it, recursion-checking).
 */

#ifndef nsFrameLoader_h_
#define nsFrameLoader_h_

#include "nsIDocShell.h"
#include "nsStringFwd.h"
#include "nsIFrameLoader.h"
#include "nsPoint.h"
#include "nsSize.h"
#include "nsIURI.h"
#include "nsAutoPtr.h"
#include "nsFrameMessageManager.h"
#include "Layers.h"

class nsIContent;
class nsIURI;
class nsSubDocumentFrame;
class nsIView;
class nsIInProcessContentFrameMessageManager;
class AutoResetInShow;

namespace mozilla {
namespace dom {
class PBrowserParent;
class TabParent;
}

namespace layout {
class RenderFrameParent;
}
}

#ifdef MOZ_WIDGET_GTK2
typedef struct _GtkWidget GtkWidget;
#endif
#ifdef MOZ_WIDGET_QT
class QX11EmbedContainer;
#endif

/**
 * Defines a target configuration for this <browser>'s content
 * document's view.  If the content document's actual view
 * doesn't match this nsIContentView, then on paints its pixels
 * are transformed to compensate for the difference.
 *
 * Used to support asynchronous re-paints of content pixels; see
 * nsIContentView.
 */
class nsContentView : public nsIContentView
{
public:
  typedef mozilla::layers::FrameMetrics::ViewID ViewID;
  NS_DECL_ISUPPORTS
  NS_DECL_NSICONTENTVIEW
 
  struct ViewConfig {
    ViewConfig()
      : mScrollOffset(0, 0)
      , mXScale(1.0)
      , mYScale(1.0)
    {}

    // Default copy ctor and operator= are fine

    PRBool operator==(const ViewConfig& aOther) const
    {
      return (mScrollOffset == aOther.mScrollOffset &&
              mXScale == aOther.mXScale &&
              mYScale == aOther.mYScale);
    }

    // This is the scroll offset the <browser> user wishes or expects
    // its enclosed content document to have.  "Scroll offset" here
    // means the document pixel at pixel (0,0) within the CSS
    // viewport.  If the content document's actual scroll offset
    // doesn't match |mScrollOffset|, the difference is used to define
    // a translation transform when painting the content document.
    nsPoint mScrollOffset;
    // The scale at which the <browser> user wishes to paint its
    // enclosed content document.  If content-document layers have a
    // lower or higher resolution than the desired scale, then the
    // ratio is used to define a scale transform when painting the
    // content document.
    float mXScale;
    float mYScale;
  };

  nsContentView(nsIContent* aOwnerContent, ViewID aScrollId,
                ViewConfig aConfig = ViewConfig())
    : mViewportSize(0, 0)
    , mContentSize(0, 0)
    , mOwnerContent(aOwnerContent)
    , mScrollId(aScrollId)
    , mConfig(aConfig)
  {}

  bool IsRoot() const;

  ViewID GetId() const
  {
    return mScrollId;
  }

  ViewConfig GetViewConfig() const
  {
    return mConfig;
  }

  nsSize mViewportSize;
  nsSize mContentSize;

  nsIContent *mOwnerContent; // WEAK

private:
  nsresult Update(const ViewConfig& aConfig);

  ViewID mScrollId;
  ViewConfig mConfig;
};


class nsFrameLoader : public nsIFrameLoader,
                      public nsIContentViewManager
{
  friend class AutoResetInShow;
  typedef mozilla::dom::PBrowserParent PBrowserParent;
  typedef mozilla::dom::TabParent TabParent;
  typedef mozilla::layout::RenderFrameParent RenderFrameParent;

protected:
  nsFrameLoader(nsIContent *aOwner, PRBool aNetworkCreated);

public:
  ~nsFrameLoader() {
    mNeedsAsyncDestroy = PR_TRUE;
    if (mMessageManager) {
      mMessageManager->Disconnect();
    }
    nsFrameLoader::Destroy();
  }

  PRBool AsyncScrollEnabled() const
  {
    return !!(mRenderMode & RENDER_MODE_ASYNC_SCROLL);
  }

  static nsFrameLoader* Create(nsIContent* aOwner, PRBool aNetworkCreated);

  NS_DECL_CYCLE_COLLECTING_ISUPPORTS
  NS_DECL_CYCLE_COLLECTION_CLASS_AMBIGUOUS(nsFrameLoader, nsIFrameLoader)
  NS_DECL_NSIFRAMELOADER
  NS_DECL_NSICONTENTVIEWMANAGER
  NS_HIDDEN_(nsresult) CheckForRecursiveLoad(nsIURI* aURI);
  nsresult ReallyStartLoading();
  void Finalize();
  nsIDocShell* GetExistingDocShell() { return mDocShell; }
  nsPIDOMEventTarget* GetTabChildGlobalAsEventTarget();
  nsresult CreateStaticClone(nsIFrameLoader* aDest);

  /**
   * Called from the layout frame associated with this frame loader;
   * this notifies us to hook up with the widget and view.
   */
  PRBool Show(PRInt32 marginWidth, PRInt32 marginHeight,
              PRInt32 scrollbarPrefX, PRInt32 scrollbarPrefY,
              nsSubDocumentFrame* frame);

  /**
   * Called from the layout frame associated with this frame loader, when
   * the frame is being torn down; this notifies us that out widget and view
   * are going away and we should unhook from them.
   */
  void Hide();

  nsresult CloneForStatic(nsIFrameLoader* aOriginal);

  // The guts of an nsIFrameLoaderOwner::SwapFrameLoader implementation.  A
  // frame loader owner needs to call this, and pass in the two references to
  // nsRefPtrs for frame loaders that need to be swapped.
  nsresult SwapWithOtherLoader(nsFrameLoader* aOther,
                               nsRefPtr<nsFrameLoader>& aFirstToSwap,
                               nsRefPtr<nsFrameLoader>& aSecondToSwap);

  // When IPC is enabled, destroy any associated child process.
  void DestroyChild();

  /**
   * Return the primary frame for our owning content, or null if it
   * can't be found.
   */
  nsIFrame* GetPrimaryFrameOfOwningContent() const
  {
    return mOwnerContent ? mOwnerContent->GetPrimaryFrame() : nsnull;
  }

  /** 
   * Return the document that owns this, or null if we don't have
   * an owner.
   */
  nsIDocument* GetOwnerDoc() const
  { return mOwnerContent ? mOwnerContent->GetOwnerDoc() : nsnull; }

  PBrowserParent* GetRemoteBrowser();

  /**
   * The "current" render frame is the one on which the most recent
   * remote layer-tree transaction was executed.  If no content has
   * been drawn yet, or the remote browser doesn't have any drawn
   * content for whatever reason, return NULL.  The returned render
   * frame has an associated shadow layer tree.
   *
   * Note that the returned render frame might not be a frame
   * constructed for this->GetURL().  This can happen, e.g., if the
   * <browser> was just navigated to a new URL, but hasn't painted the
   * new page yet.  A render frame for the previous page may be
   * returned.  (In-process <browser> behaves similarly, and this
   * behavior seems desirable.)
   */
  RenderFrameParent* GetCurrentRemoteFrame() const
  {
    return mCurrentRemoteFrame;
  }

  /**
   * |aFrame| can be null.  If non-null, it must be the remote frame
   * on which the most recent layer transaction completed for this's
   * <browser>.
   */
  void SetCurrentRemoteFrame(RenderFrameParent* aFrame)
  {
    mCurrentRemoteFrame = aFrame;
  }
  nsFrameMessageManager* GetFrameMessageManager() { return mMessageManager; }

  nsIContent* GetOwnerContent() { return mOwnerContent; }
  void SetOwnerContent(nsIContent* aContent);

private:

  bool ShouldUseRemoteProcess();

  /**
   * If we are an IPC frame, set mRemoteFrame. Otherwise, create and
   * initialize mDocShell.
   */
  nsresult MaybeCreateDocShell();
  nsresult EnsureMessageManager();
  NS_HIDDEN_(void) GetURL(nsString& aURL);

  // Properly retrieves documentSize of any subdocument type.
  NS_HIDDEN_(nsIntSize) GetSubDocumentSize(const nsIFrame *aIFrame);

  // Updates the subdocument position and size. This gets called only
  // when we have our own in-process DocShell.
  NS_HIDDEN_(nsresult) UpdateBaseWindowPositionAndSize(nsIFrame *aIFrame);
  nsresult CheckURILoad(nsIURI* aURI);
  void FireErrorEvent();
  nsresult ReallyStartLoadingInternal();

  // Return true if remote browser created; nothing else to do
  bool TryRemoteBrowser();

  // Tell the remote browser that it's now "virtually visible"
  bool ShowRemoteFrame(const nsIntSize& size);

  nsCOMPtr<nsIDocShell> mDocShell;
  nsCOMPtr<nsIURI> mURIToLoad;
  nsIContent *mOwnerContent; // WEAK
public:
  // public because a callback needs these.
  nsRefPtr<nsFrameMessageManager> mMessageManager;
  nsCOMPtr<nsIInProcessContentFrameMessageManager> mChildMessageManager;
private:
  PRPackedBool mDepthTooGreat : 1;
  PRPackedBool mIsTopLevelContent : 1;
  PRPackedBool mDestroyCalled : 1;
  PRPackedBool mNeedsAsyncDestroy : 1;
  PRPackedBool mInSwap : 1;
  PRPackedBool mInShow : 1;
  PRPackedBool mHideCalled : 1;
  // True when the object is created for an element which the parser has
  // created using NS_FROM_PARSER_NETWORK flag. If the element is modified,
  // it may lose the flag.
  PRPackedBool mNetworkCreated : 1;

  PRPackedBool mDelayRemoteDialogs : 1;
  PRPackedBool mRemoteBrowserShown : 1;
  bool mRemoteFrame;
  // XXX leaking
  nsCOMPtr<nsIObserver> mChildHost;
  RenderFrameParent* mCurrentRemoteFrame;
  TabParent* mRemoteBrowser;

  // See nsIFrameLoader.idl.  Short story, if !(mRenderMode &
  // RENDER_MODE_ASYNC_SCROLL), all the fields below are ignored in
  // favor of what content tells.
  PRUint32 mRenderMode;
};

#endif
