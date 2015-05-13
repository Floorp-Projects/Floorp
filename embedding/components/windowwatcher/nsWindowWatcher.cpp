/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

//#define USEWEAKREFS // (haven't quite figured that out yet)

#include "nsWindowWatcher.h"
#include "nsAutoWindowStateHelper.h"

#include "nsCRT.h"
#include "nsNetUtil.h"
#include "nsJSUtils.h"
#include "plstr.h"

#include "nsIBaseWindow.h"
#include "nsIBrowserDOMWindow.h"
#include "nsIDocShell.h"
#include "nsIDocShellLoadInfo.h"
#include "nsIDocShellTreeItem.h"
#include "nsIDocShellTreeOwner.h"
#include "nsIDocumentLoader.h"
#include "nsIDocument.h"
#include "nsIDOMDocument.h"
#include "nsIDOMWindow.h"
#include "nsIDOMChromeWindow.h"
#include "nsIDOMModalContentWindow.h"
#include "nsIPrompt.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIScreen.h"
#include "nsIScreenManager.h"
#include "nsIScriptContext.h"
#include "nsIObserverService.h"
#include "nsIScriptSecurityManager.h"
#include "nsXPCOM.h"
#include "nsIURI.h"
#include "nsIWebBrowser.h"
#include "nsIWebBrowserChrome.h"
#include "nsIWebNavigation.h"
#include "nsIWindowCreator.h"
#include "nsIWindowCreator2.h"
#include "nsIXPConnect.h"
#include "nsIXULRuntime.h"
#include "nsPIDOMWindow.h"
#include "nsIContentViewer.h"
#include "nsIWindowProvider.h"
#include "nsIMutableArray.h"
#include "nsISupportsArray.h"
#include "nsIDOMStorageManager.h"
#include "nsIWidget.h"
#include "nsFocusManager.h"
#include "nsIPresShell.h"
#include "nsPresContext.h"
#include "nsContentUtils.h"
#include "nsIPrefBranch.h"
#include "nsIPrefService.h"
#include "nsSandboxFlags.h"
#include "mozilla/Preferences.h"
#include "mozilla/dom/DOMStorage.h"
#include "mozilla/dom/ScriptSettings.h"

#ifdef USEWEAKREFS
#include "nsIWeakReference.h"
#endif

using namespace mozilla;
using namespace mozilla::dom;

/****************************************************************
 ******************** nsWatcherWindowEntry **********************
 ****************************************************************/

class nsWindowWatcher;

struct nsWatcherWindowEntry
{

  nsWatcherWindowEntry(nsIDOMWindow* aWindow, nsIWebBrowserChrome* aChrome)
  {
#ifdef USEWEAKREFS
    mWindow = do_GetWeakReference(aWindow);
#else
    mWindow = aWindow;
#endif
    nsCOMPtr<nsISupportsWeakReference> supportsweak(do_QueryInterface(aChrome));
    if (supportsweak) {
      supportsweak->GetWeakReference(getter_AddRefs(mChromeWeak));
    } else {
      mChrome = aChrome;
      mChromeWeak = 0;
    }
    ReferenceSelf();
  }
  ~nsWatcherWindowEntry() {}

  void InsertAfter(nsWatcherWindowEntry* aOlder);
  void Unlink();
  void ReferenceSelf();

#ifdef USEWEAKREFS
  nsCOMPtr<nsIWeakReference> mWindow;
#else // still not an owning ref
  nsIDOMWindow* mWindow;
#endif
  nsIWebBrowserChrome* mChrome;
  nsWeakPtr mChromeWeak;
  // each struct is in a circular, doubly-linked list
  nsWatcherWindowEntry* mYounger; // next younger in sequence
  nsWatcherWindowEntry* mOlder;
};

void
nsWatcherWindowEntry::InsertAfter(nsWatcherWindowEntry* aOlder)
{
  if (aOlder) {
    mOlder = aOlder;
    mYounger = aOlder->mYounger;
    mOlder->mYounger = this;
    if (mOlder->mOlder == mOlder) {
      mOlder->mOlder = this;
    }
    mYounger->mOlder = this;
    if (mYounger->mYounger == mYounger) {
      mYounger->mYounger = this;
    }
  }
}

void
nsWatcherWindowEntry::Unlink()
{
  mOlder->mYounger = mYounger;
  mYounger->mOlder = mOlder;
  ReferenceSelf();
}

void
nsWatcherWindowEntry::ReferenceSelf()
{

  mYounger = this;
  mOlder = this;
}

/****************************************************************
 ****************** nsWatcherWindowEnumerator *******************
 ****************************************************************/

class nsWatcherWindowEnumerator : public nsISimpleEnumerator
{

public:
  explicit nsWatcherWindowEnumerator(nsWindowWatcher* aWatcher);
  NS_IMETHOD HasMoreElements(bool* aResult) override;
  NS_IMETHOD GetNext(nsISupports** aResult) override;

  NS_DECL_ISUPPORTS

protected:
  virtual ~nsWatcherWindowEnumerator();

private:
  friend class nsWindowWatcher;

  nsWatcherWindowEntry* FindNext();
  void WindowRemoved(nsWatcherWindowEntry* aInfo);

  nsWindowWatcher* mWindowWatcher;
  nsWatcherWindowEntry* mCurrentPosition;
};

NS_IMPL_ADDREF(nsWatcherWindowEnumerator)
NS_IMPL_RELEASE(nsWatcherWindowEnumerator)
NS_IMPL_QUERY_INTERFACE(nsWatcherWindowEnumerator, nsISimpleEnumerator)

nsWatcherWindowEnumerator::nsWatcherWindowEnumerator(nsWindowWatcher* aWatcher)
  : mWindowWatcher(aWatcher)
  , mCurrentPosition(aWatcher->mOldestWindow)
{
  mWindowWatcher->AddEnumerator(this);
  mWindowWatcher->AddRef();
}

nsWatcherWindowEnumerator::~nsWatcherWindowEnumerator()
{
  mWindowWatcher->RemoveEnumerator(this);
  mWindowWatcher->Release();
}

NS_IMETHODIMP
nsWatcherWindowEnumerator::HasMoreElements(bool* aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = !!mCurrentPosition;
  return NS_OK;
}

NS_IMETHODIMP
nsWatcherWindowEnumerator::GetNext(nsISupports** aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = nullptr;

#ifdef USEWEAKREFS
  while (mCurrentPosition) {
    CallQueryReferent(mCurrentPosition->mWindow, aResult);
    if (*aResult) {
      mCurrentPosition = FindNext();
      break;
    } else { // window is gone!
      mWindowWatcher->RemoveWindow(mCurrentPosition);
    }
  }
  NS_IF_ADDREF(*aResult);
#else
  if (mCurrentPosition) {
    CallQueryInterface(mCurrentPosition->mWindow, aResult);
    mCurrentPosition = FindNext();
  }
#endif
  return NS_OK;
}

nsWatcherWindowEntry*
nsWatcherWindowEnumerator::FindNext()
{
  nsWatcherWindowEntry* info;

  if (!mCurrentPosition) {
    return 0;
  }

  info = mCurrentPosition->mYounger;
  return info == mWindowWatcher->mOldestWindow ? 0 : info;
}

// if a window is being removed adjust the iterator's current position
void
nsWatcherWindowEnumerator::WindowRemoved(nsWatcherWindowEntry* aInfo)
{

  if (mCurrentPosition == aInfo) {
    mCurrentPosition =
      mCurrentPosition != aInfo->mYounger ? aInfo->mYounger : 0;
  }
}

/****************************************************************
 *********************** nsWindowWatcher ************************
 ****************************************************************/

NS_IMPL_ADDREF(nsWindowWatcher)
NS_IMPL_RELEASE(nsWindowWatcher)
NS_IMPL_QUERY_INTERFACE(nsWindowWatcher,
                        nsIWindowWatcher,
                        nsIPromptFactory,
                        nsPIWindowWatcher)

nsWindowWatcher::nsWindowWatcher()
  : mEnumeratorList()
  , mOldestWindow(0)
  , mListLock("nsWindowWatcher.mListLock")
{
}

nsWindowWatcher::~nsWindowWatcher()
{
  // delete data
  while (mOldestWindow) {
    RemoveWindow(mOldestWindow);
  }
}

nsresult
nsWindowWatcher::Init()
{
  return NS_OK;
}

/**
 * Convert aArguments into either an nsIArray or nullptr.
 *
 *  - If aArguments is nullptr, return nullptr.
 *  - If aArguments is an nsArray, return nullptr if it's empty, or otherwise
 *    return the array.
 *  - If aArguments is an nsISupportsArray, return nullptr if it's empty, or
 *    otherwise add its elements to an nsArray and return the new array.
 *  - Otherwise, return an nsIArray with one element: aArguments.
 */
static already_AddRefed<nsIArray>
ConvertArgsToArray(nsISupports* aArguments)
{
  if (!aArguments) {
    return nullptr;
  }

  nsCOMPtr<nsIArray> array = do_QueryInterface(aArguments);
  if (array) {
    uint32_t argc = 0;
    array->GetLength(&argc);
    if (argc == 0) {
      return nullptr;
    }

    return array.forget();
  }

  nsCOMPtr<nsISupportsArray> supArray = do_QueryInterface(aArguments);
  if (supArray) {
    uint32_t argc = 0;
    supArray->Count(&argc);
    if (argc == 0) {
      return nullptr;
    }

    nsCOMPtr<nsIMutableArray> mutableArray =
      do_CreateInstance(NS_ARRAY_CONTRACTID);
    NS_ENSURE_TRUE(mutableArray, nullptr);

    for (uint32_t i = 0; i < argc; i++) {
      nsCOMPtr<nsISupports> elt;
      supArray->GetElementAt(i, getter_AddRefs(elt));
      nsresult rv = mutableArray->AppendElement(elt, /* aWeak = */ false);
      NS_ENSURE_SUCCESS(rv, nullptr);
    }

    return mutableArray.forget();
  }

  nsCOMPtr<nsIMutableArray> singletonArray =
    do_CreateInstance(NS_ARRAY_CONTRACTID);
  NS_ENSURE_TRUE(singletonArray, nullptr);

  nsresult rv = singletonArray->AppendElement(aArguments, /* aWeak = */ false);
  NS_ENSURE_SUCCESS(rv, nullptr);

  return singletonArray.forget();
}

NS_IMETHODIMP
nsWindowWatcher::OpenWindow(nsIDOMWindow* aParent,
                            const char* aUrl,
                            const char* aName,
                            const char* aFeatures,
                            nsISupports* aArguments,
                            nsIDOMWindow** aResult)
{
  nsCOMPtr<nsIArray> argv = ConvertArgsToArray(aArguments);

  uint32_t argc = 0;
  if (argv) {
    argv->GetLength(&argc);
  }
  bool dialog = (argc != 0);

  return OpenWindowInternal(aParent, aUrl, aName, aFeatures,
                            /* calledFromJS = */ false, dialog,
                            /* navigate = */ true, nullptr, argv, aResult);
}

struct SizeSpec
{
  SizeSpec()
    : mLeftSpecified(false)
    , mTopSpecified(false)
    , mOuterWidthSpecified(false)
    , mOuterHeightSpecified(false)
    , mInnerWidthSpecified(false)
    , mInnerHeightSpecified(false)
    , mUseDefaultWidth(false)
    , mUseDefaultHeight(false)
  {
  }

  int32_t mLeft;
  int32_t mTop;
  int32_t mOuterWidth;  // Total window width
  int32_t mOuterHeight; // Total window height
  int32_t mInnerWidth;  // Content area width
  int32_t mInnerHeight; // Content area height

  bool mLeftSpecified;
  bool mTopSpecified;
  bool mOuterWidthSpecified;
  bool mOuterHeightSpecified;
  bool mInnerWidthSpecified;
  bool mInnerHeightSpecified;

  // If these booleans are true, don't look at the corresponding width values
  // even if they're specified -- they'll be bogus
  bool mUseDefaultWidth;
  bool mUseDefaultHeight;

  bool PositionSpecified() const
  {
    return mLeftSpecified || mTopSpecified;
  }

  bool SizeSpecified() const
  {
    return mOuterWidthSpecified || mOuterHeightSpecified ||
           mInnerWidthSpecified || mInnerHeightSpecified;
  }
};

NS_IMETHODIMP
nsWindowWatcher::OpenWindow2(nsIDOMWindow* aParent,
                             const char* aUrl,
                             const char* aName,
                             const char* aFeatures,
                             bool aCalledFromScript,
                             bool aDialog,
                             bool aNavigate,
                             nsITabParent* aOpeningTab,
                             nsISupports* aArguments,
                             nsIDOMWindow** aResult)
{
  nsCOMPtr<nsIArray> argv = ConvertArgsToArray(aArguments);

  uint32_t argc = 0;
  if (argv) {
    argv->GetLength(&argc);
  }

  // This is extremely messed up, but this behavior is necessary because
  // callers lie about whether they're a dialog window and whether they're
  // called from script.  Fixing this is bug 779939.
  bool dialog = aDialog;
  if (!aCalledFromScript) {
    dialog = argc > 0;
  }

  return OpenWindowInternal(aParent, aUrl, aName, aFeatures,
                            aCalledFromScript, dialog,
                            aNavigate, aOpeningTab, argv, aResult);
}

nsresult
nsWindowWatcher::OpenWindowInternal(nsIDOMWindow* aParent,
                                    const char* aUrl,
                                    const char* aName,
                                    const char* aFeatures,
                                    bool aCalledFromJS,
                                    bool aDialog,
                                    bool aNavigate,
                                    nsITabParent* aOpeningTab,
                                    nsIArray* aArgv,
                                    nsIDOMWindow** aResult)
{
  nsresult rv = NS_OK;
  bool isNewToplevelWindow = false;
  bool windowIsNew = false;
  bool windowNeedsName = false;
  bool windowIsModal = false;
  bool uriToLoadIsChrome = false;
  bool windowIsModalContentDialog = false;

  // Opening tabs are only ever passed to OpenWindowInternal if we're opening
  // a window from a remote tab.
  bool openedFromRemoteTab = !!aOpeningTab;

  uint32_t chromeFlags;
  nsAutoString name;          // string version of aName
  nsAutoCString features;     // string version of aFeatures
  nsCOMPtr<nsIURI> uriToLoad; // from aUrl, if any
  nsCOMPtr<nsIDocShellTreeOwner> parentTreeOwner; // from the parent window, if any
  nsCOMPtr<nsIDocShellTreeItem> newDocShellItem; // from the new window

  nsCOMPtr<nsPIDOMWindow> parent = do_QueryInterface(aParent);
  if (parent && parent->IsInnerWindow()) {
    NS_ENSURE_STATE(parent->IsCurrentInnerWindow());
    aParent = parent->GetOuterWindow();
  }

  MOZ_ASSERT_IF(openedFromRemoteTab,
                XRE_GetProcessType() == GeckoProcessType_Default);
  NS_ENSURE_ARG_POINTER(aResult);
  *aResult = 0;

  if (!nsContentUtils::IsSafeToRunScript()) {
    nsContentUtils::WarnScriptWasIgnored(nullptr);
    return NS_ERROR_FAILURE;
  }

  GetWindowTreeOwner(aParent, getter_AddRefs(parentTreeOwner));

  if (aUrl) {
    rv = URIfromURL(aUrl, aParent, getter_AddRefs(uriToLoad));
    if (NS_FAILED(rv)) {
      return rv;
    }
    uriToLoad->SchemeIs("chrome", &uriToLoadIsChrome);
  }

  bool nameSpecified = false;
  if (aName) {
    CopyUTF8toUTF16(aName, name);
    nameSpecified = true;
  }

  bool featuresSpecified = false;
  if (aFeatures) {
    features.Assign(aFeatures);
    featuresSpecified = true;
    features.StripWhitespace();
  }

  // We only want to check for existing named windows if:
  // a) We're the child process
  // b) We're the parent process, and aOpeningTab wasn't passed
  //    in.
  // This is because when using child processes, the parent process shouldn't
  // know or care about names - unless we're opening named windows from chrome.
  if (!aOpeningTab) {
    // try to find an extant window with the given name
    nsCOMPtr<nsIDOMWindow> foundWindow = SafeGetWindowByName(name, aParent);
    GetWindowTreeItem(foundWindow, getter_AddRefs(newDocShellItem));
  }

  // Do sandbox checks here, instead of waiting until nsIDocShell::LoadURI.
  // The state of the window can change before this call and if we are blocked
  // because of sandboxing, we wouldn't want that to happen.
  nsCOMPtr<nsPIDOMWindow> parentWindow = do_QueryInterface(aParent);
  nsCOMPtr<nsIDocShell> parentDocShell;
  if (parentWindow) {
    parentDocShell = parentWindow->GetDocShell();
    if (parentDocShell) {
      nsCOMPtr<nsIDocShell> foundDocShell = do_QueryInterface(newDocShellItem);
      if (parentDocShell->IsSandboxedFrom(foundDocShell)) {
        return NS_ERROR_DOM_INVALID_ACCESS_ERR;
      }
    }
  }

  // no extant window? make a new one.

  // If no parent, consider it chrome.
  bool hasChromeParent = true;
  if (aParent) {
    // Check if the parent document has chrome privileges.
    nsCOMPtr<nsIDOMDocument> domdoc;
    aParent->GetDocument(getter_AddRefs(domdoc));
    nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);
    hasChromeParent =
      doc && nsContentUtils::IsChromeDoc(doc) && !openedFromRemoteTab;
  }

  // Make sure we call CalculateChromeFlags() *before* we push the
  // callee context onto the context stack so that
  // CalculateChromeFlags() sees the actual caller when doing its
  // security checks.
  chromeFlags = CalculateChromeFlags(aParent, features.get(), featuresSpecified,
                                     aDialog, uriToLoadIsChrome,
                                     hasChromeParent, openedFromRemoteTab);

  // If we are opening a window from a remote browser, the resulting window
  // should also be remote.
  MOZ_ASSERT_IF(openedFromRemoteTab,
                chromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

  // If we're not called through our JS version of the API, and we got
  // our internal modal option, treat the window we're opening as a
  // modal content window (and set the modal chrome flag).
  if (!aCalledFromJS && aArgv &&
      WinHasOption(features.get(), "-moz-internal-modal", 0, nullptr)) {
    windowIsModalContentDialog = true;

    // CHROME_MODAL gets inherited by dependent windows, which affects various
    // platform-specific window state (especially on OSX). So we need some way
    // to determine that this window was actually opened by nsGlobalWindow::
    // ShowModalDialog(), and that somebody is actually going to be watching
    // for return values and all that.
    chromeFlags |= nsIWebBrowserChrome::CHROME_MODAL_CONTENT_WINDOW;
    chromeFlags |= nsIWebBrowserChrome::CHROME_MODAL;
  }

  SizeSpec sizeSpec;
  CalcSizeSpec(features.get(), sizeSpec);

  nsCOMPtr<nsIScriptSecurityManager> sm(
    do_GetService(NS_SCRIPTSECURITYMANAGER_CONTRACTID));

  bool isCallerChrome =
    nsContentUtils::IsCallerChrome() && !openedFromRemoteTab;

  dom::AutoJSAPI jsapiChromeGuard;

  bool windowTypeIsChrome =
    chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
  if (isCallerChrome && !hasChromeParent && !windowTypeIsChrome) {
    // open() is called from chrome on a non-chrome window, initialize an
    // AutoJSAPI with the callee to prevent the caller's privileges from leaking
    // into code that runs while opening the new window.
    //
    // The reasoning for this is in bug 289204. Basically, chrome sometimes does
    // someContentWindow.open(untrustedURL), and wants to be insulated from nasty
    // javascript: URLs and such. But there are also cases where we create a
    // window parented to a content window (such as a download dialog), usually
    // directly with nsIWindowWatcher. In those cases, we want the principal of
    // the initial about:blank document to be system, so that the subsequent XUL
    // load can reuse the inner window and avoid blowing away expandos. As such,
    // we decide whether to load with the principal of the caller or of the parent
    // based on whether the docshell type is chrome or content.

    nsCOMPtr<nsIGlobalObject> parentGlobalObject = do_QueryInterface(aParent);
    if (NS_WARN_IF(!jsapiChromeGuard.Init(parentGlobalObject))) {
      return NS_ERROR_UNEXPECTED;
    }
  }

  uint32_t activeDocsSandboxFlags = 0;
  if (!newDocShellItem) {
    // We're going to either open up a new window ourselves or ask a
    // nsIWindowProvider for one.  In either case, we'll want to set the right
    // name on it.
    windowNeedsName = true;

    // If the parent trying to open a new window is sandboxed
    // without 'allow-popups', this is not allowed and we fail here.
    if (aParent) {
      nsCOMPtr<nsIDOMDocument> domdoc;
      aParent->GetDocument(getter_AddRefs(domdoc));
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(domdoc);

      if (doc) {
        // Save sandbox flags for copying to new browsing context (docShell).
        activeDocsSandboxFlags = doc->GetSandboxFlags();
        if (activeDocsSandboxFlags & SANDBOXED_AUXILIARY_NAVIGATION) {
          return NS_ERROR_DOM_INVALID_ACCESS_ERR;
        }
      }
    }

    // Now check whether it's ok to ask a window provider for a window.  Don't
    // do it if we're opening a dialog or if our parent is a chrome window or
    // if we're opening something that has modal, dialog, or chrome flags set.
    nsCOMPtr<nsIDOMChromeWindow> chromeWin = do_QueryInterface(aParent);
    if (!aDialog && !chromeWin &&
        !(chromeFlags & (nsIWebBrowserChrome::CHROME_MODAL |
                         nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
                         nsIWebBrowserChrome::CHROME_OPENAS_CHROME))) {
      nsCOMPtr<nsIWindowProvider> provider = do_GetInterface(parentTreeOwner);
      if (provider) {
        NS_ASSERTION(aParent, "We've _got_ to have a parent here!");

        nsCOMPtr<nsIDOMWindow> newWindow;
        rv = provider->ProvideWindow(aParent, chromeFlags, aCalledFromJS,
                                     sizeSpec.PositionSpecified(),
                                     sizeSpec.SizeSpecified(),
                                     uriToLoad, name, features, &windowIsNew,
                                     getter_AddRefs(newWindow));

        if (NS_SUCCEEDED(rv)) {
          GetWindowTreeItem(newWindow, getter_AddRefs(newDocShellItem));
          if (windowIsNew && newDocShellItem) {
            // Make sure to stop any loads happening in this window that the
            // window provider might have started.  Otherwise if our caller
            // manipulates the window it just opened and then the load
            // completes their stuff will get blown away.
            nsCOMPtr<nsIWebNavigation> webNav =
              do_QueryInterface(newDocShellItem);
            webNav->Stop(nsIWebNavigation::STOP_NETWORK);
          }
        } else if (rv == NS_ERROR_ABORT) {
          // NS_ERROR_ABORT means the window provider has flat-out rejected
          // the open-window call and we should bail.  Don't return an error
          // here, because our caller may propagate that error, which might
          // cause e.g. window.open to throw!  Just return null for our out
          // param.
          return NS_OK;
        }
      }
    }
  }

  bool newWindowShouldBeModal = false;
  bool parentIsModal = false;
  if (!newDocShellItem) {
    windowIsNew = true;
    isNewToplevelWindow = true;

    nsCOMPtr<nsIWebBrowserChrome> parentChrome(do_GetInterface(parentTreeOwner));

    // is the parent (if any) modal? if so, we must be, too.
    bool weAreModal = (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL) != 0;
    newWindowShouldBeModal = weAreModal;
    if (!weAreModal && parentChrome) {
      parentChrome->IsWindowModal(&weAreModal);
      parentIsModal = weAreModal;
    }

    if (weAreModal) {
      windowIsModal = true;
      // in case we added this because weAreModal
      chromeFlags |= nsIWebBrowserChrome::CHROME_MODAL |
                     nsIWebBrowserChrome::CHROME_DEPENDENT;
    }

    // Make sure to not create modal windows if our parent is invisible and
    // isn't a chrome window.  Otherwise we can end up in a bizarre situation
    // where we can't shut down because an invisible window is open.  If
    // someone tries to do this, throw.
    if (!hasChromeParent && (chromeFlags & nsIWebBrowserChrome::CHROME_MODAL)) {
      nsCOMPtr<nsIBaseWindow> parentWindow(do_GetInterface(parentTreeOwner));
      nsCOMPtr<nsIWidget> parentWidget;
      if (parentWindow) {
        parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
      }
      // NOTE: the logic for this visibility check is duplicated in
      // nsIDOMWindowUtils::isParentWindowMainWidgetVisible - if we change
      // how a window is determined "visible" in this context then we should
      // also adjust that attribute and/or any consumers of it...
      if (parentWidget && !parentWidget->IsVisible()) {
        return NS_ERROR_NOT_AVAILABLE;
      }
    }

    NS_ASSERTION(mWindowCreator,
                 "attempted to open a new window with no WindowCreator");
    rv = NS_ERROR_FAILURE;
    if (mWindowCreator) {
      nsCOMPtr<nsIWebBrowserChrome> newChrome;

      /* If the window creator is an nsIWindowCreator2, we can give it
         some hints. The only hint at this time is whether the opening window
         is in a situation that's likely to mean this is an unrequested
         popup window we're creating. However we're not completely honest:
         we clear that indicator if the opener is chrome, so that the
         downstream consumer can treat the indicator to mean simply
         that the new window is subject to popup control. */
      nsCOMPtr<nsIWindowCreator2> windowCreator2(
        do_QueryInterface(mWindowCreator));
      if (windowCreator2) {
        uint32_t contextFlags = 0;
        bool popupConditions = false;

        // is the parent under popup conditions?
        if (parentWindow) {
          popupConditions = parentWindow->IsLoadingOrRunningTimeout();
        }

        // chrome is always allowed, so clear the flag if the opener is chrome
        if (popupConditions) {
          popupConditions = !isCallerChrome;
        }

        if (popupConditions) {
          contextFlags |=
            nsIWindowCreator2::PARENT_IS_LOADING_OR_RUNNING_TIMEOUT;
        }

        bool cancel = false;
        rv = windowCreator2->CreateChromeWindow2(parentChrome, chromeFlags,
                                                 contextFlags, uriToLoad,
                                                 aOpeningTab, &cancel,
                                                 getter_AddRefs(newChrome));
        if (NS_SUCCEEDED(rv) && cancel) {
          newChrome = 0; // just in case
          rv = NS_ERROR_ABORT;
        }
      } else {
        rv = mWindowCreator->CreateChromeWindow(parentChrome, chromeFlags,
                                                getter_AddRefs(newChrome));
      }

      if (newChrome) {
        /* It might be a chrome nsXULWindow, in which case it won't have
            an nsIDOMWindow (primary content shell). But in that case, it'll
            be able to hand over an nsIDocShellTreeItem directly. */
        nsCOMPtr<nsIDOMWindow> newWindow(do_GetInterface(newChrome));
        if (newWindow) {
          GetWindowTreeItem(newWindow, getter_AddRefs(newDocShellItem));
        }
        if (!newDocShellItem) {
          newDocShellItem = do_GetInterface(newChrome);
        }
        if (!newDocShellItem) {
          rv = NS_ERROR_FAILURE;
        }
      }
    }
  }

  // better have a window to use by this point
  if (!newDocShellItem) {
    return rv;
  }

  nsCOMPtr<nsIDocShell> newDocShell(do_QueryInterface(newDocShellItem));
  NS_ENSURE_TRUE(newDocShell, NS_ERROR_UNEXPECTED);

  // Set up sandboxing attributes if the window is new.
  // The flags can only be non-zero for new windows.
  if (activeDocsSandboxFlags != 0) {
    newDocShell->SetSandboxFlags(activeDocsSandboxFlags);
    if (parentWindow) {
      newDocShell->SetOnePermittedSandboxedNavigator(
        parentWindow->GetDocShell());
    }
  }

  rv = ReadyOpenedDocShellItem(newDocShellItem, aParent, windowIsNew, aResult);
  if (NS_FAILED(rv)) {
    return rv;
  }

  /* disable persistence of size/position in popups (determined by
     determining whether the features parameter specifies width or height
     in any way). We consider any overriding of the window's size or position
     in the open call as disabling persistence of those attributes.
     Popup windows (which should not persist size or position) generally set
     the size. */
  if (isNewToplevelWindow) {
    /* at the moment, the strings "height=" or "width=" never happen
       outside a size specification, so we can do this the Q&D way. */

    if (PL_strcasestr(features.get(), "width=") ||
        PL_strcasestr(features.get(), "height=")) {

      nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
      newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
      if (newTreeOwner) {
        newTreeOwner->SetPersistence(false, false, false);
      }
    }
  }

  if ((aDialog || windowIsModalContentDialog) && aArgv) {
    // Set the args on the new window.
    nsCOMPtr<nsPIDOMWindow> piwin(do_QueryInterface(*aResult));
    NS_ENSURE_TRUE(piwin, NS_ERROR_UNEXPECTED);

    rv = piwin->SetArguments(aArgv);
    NS_ENSURE_SUCCESS(rv, rv);
  }

  /* allow a window that we found by name to keep its name (important for cases
     like _self where the given name is different (and invalid)).  Also, _blank
     is not a window name. */
  if (windowNeedsName) {
    if (nameSpecified && !name.LowerCaseEqualsLiteral("_blank")) {
      newDocShellItem->SetName(name);
    } else {
      newDocShellItem->SetName(EmptyString());
    }
  }

  // Now we have to set the right opener principal on the new window.  Note
  // that we have to do this _before_ starting any URI loads, thanks to the
  // sync nature of javascript: loads.
  //
  // Note: The check for the current JSContext isn't necessarily sensical.
  // It's just designed to preserve old semantics during a mass-conversion
  // patch.
  nsCOMPtr<nsIPrincipal> subjectPrincipal =
    nsContentUtils::GetCurrentJSContext() ? nsContentUtils::SubjectPrincipal() :
                                            nullptr;

  if (windowIsNew) {
    // Now set the opener principal on the new window.  Note that we need to do
    // this no matter whether we were opened from JS; if there is nothing on
    // the JS stack, just use the principal of our parent window.  In those
    // cases we do _not_ set the parent window principal as the owner of the
    // load--since we really don't know who the owner is, just leave it null.
    nsCOMPtr<nsPIDOMWindow> newWindow = do_QueryInterface(*aResult);
    NS_ASSERTION(newWindow == newDocShell->GetWindow(), "Different windows??");

    // The principal of the initial about:blank document gets set up in
    // nsWindowWatcher::AddWindow. Make sure to call it. In the common case
    // this call already happened when the window was created, but
    // SetInitialPrincipalToSubject is safe to call multiple times.
    if (newWindow) {
      newWindow->SetInitialPrincipalToSubject();
    }
  }

  // If all windows should be private, make sure the new window is also
  // private.  Otherwise, see if the caller has explicitly requested a
  // private or non-private window.
  bool isPrivateBrowsingWindow =
    Preferences::GetBool("browser.privatebrowsing.autostart") ||
    (!!(chromeFlags & nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW) &&
     !(chromeFlags & nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW));

  // Otherwise, propagate the privacy status of the parent window, if
  // available, to the child.
  if (!isPrivateBrowsingWindow &&
      !(chromeFlags & nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW)) {
    nsCOMPtr<nsIDocShellTreeItem> parentItem;
    GetWindowTreeItem(aParent, getter_AddRefs(parentItem));
    nsCOMPtr<nsILoadContext> parentContext = do_QueryInterface(parentItem);
    if (parentContext) {
      isPrivateBrowsingWindow = parentContext->UsePrivateBrowsing();
    }
  }

  // We rely on CalculateChromeFlags to decide whether remote (out-of-process)
  // tabs should be used.
  bool isRemoteWindow =
    !!(chromeFlags & nsIWebBrowserChrome::CHROME_REMOTE_WINDOW);

  if (isNewToplevelWindow) {
    nsCOMPtr<nsIDocShellTreeItem> childRoot;
    newDocShellItem->GetRootTreeItem(getter_AddRefs(childRoot));
    nsCOMPtr<nsILoadContext> childContext = do_QueryInterface(childRoot);
    if (childContext) {
      childContext->SetPrivateBrowsing(isPrivateBrowsingWindow);
      childContext->SetRemoteTabs(isRemoteWindow);
    }
  } else if (windowIsNew) {
    nsCOMPtr<nsILoadContext> childContext = do_QueryInterface(newDocShellItem);
    if (childContext) {
      childContext->SetPrivateBrowsing(isPrivateBrowsingWindow);
      childContext->SetRemoteTabs(isRemoteWindow);
    }
  }

  nsCOMPtr<nsIDocShellLoadInfo> loadInfo;
  if (uriToLoad && aNavigate) {
    newDocShell->CreateLoadInfo(getter_AddRefs(loadInfo));
    NS_ENSURE_TRUE(loadInfo, NS_ERROR_FAILURE);

    if (subjectPrincipal) {
      loadInfo->SetOwner(subjectPrincipal);
    }

    nsCOMPtr<nsPIDOMWindow> referrerWindow =
      do_QueryInterface(GetEntryGlobal());
    if (!referrerWindow) {
      referrerWindow = do_QueryInterface(aParent);
    }
    if (referrerWindow) {
      /* use the URL from the *extant* document, if any. The usual accessor
         GetDocument will synchronously create an about:blank document if
         it has no better answer, and we only care about a real document.
         Also using GetDocument to force document creation seems to
         screw up focus in the hidden window; see bug 36016.
      */
      nsCOMPtr<nsIDocument> doc = referrerWindow->GetExtantDoc();
      if (doc) {
        // Set the referrer
        loadInfo->SetReferrer(doc->GetDocumentURI());
        loadInfo->SetReferrerPolicy(doc->GetReferrerPolicy());
      }
    }
  }

  if (isNewToplevelWindow) {
    // Notify observers that the window is open and ready.
    // The window has not yet started to load a document.
    nsCOMPtr<nsIObserverService> obsSvc =
      mozilla::services::GetObserverService();
    if (obsSvc) {
      obsSvc->NotifyObservers(*aResult, "toplevel-window-ready", nullptr);
    }
  }

  if (uriToLoad && aNavigate) {
    newDocShell->LoadURI(
      uriToLoad,
      loadInfo,
      windowIsNew ?
        static_cast<uint32_t>(nsIWebNavigation::LOAD_FLAGS_FIRST_LOAD) :
        static_cast<uint32_t>(nsIWebNavigation::LOAD_FLAGS_NONE),
      true);
  }

  // Copy the current session storage for the current domain.
  if (subjectPrincipal && parentDocShell) {
    nsCOMPtr<nsIDOMStorageManager> parentStorageManager =
      do_QueryInterface(parentDocShell);
    nsCOMPtr<nsIDOMStorageManager> newStorageManager =
      do_QueryInterface(newDocShell);

    if (parentStorageManager && newStorageManager) {
      nsCOMPtr<nsIDOMStorage> storage;
      nsCOMPtr<nsPIDOMWindow> pWin = do_QueryInterface(aParent);
      nsCOMPtr<nsPIDOMWindow> pInnerWin =
        pWin->IsInnerWindow() ? pWin.get() : pWin->GetCurrentInnerWindow();

      parentStorageManager->GetStorage(pInnerWin, subjectPrincipal,
                                       isPrivateBrowsingWindow,
                                       getter_AddRefs(storage));
      if (storage) {
        newStorageManager->CloneStorage(storage);
      }
    }
  }

  if (isNewToplevelWindow) {
    SizeOpenedDocShellItem(newDocShellItem, aParent, isCallerChrome, sizeSpec);
  }

  // XXXbz isn't windowIsModal always true when windowIsModalContentDialog?
  if (windowIsModal || windowIsModalContentDialog) {
    nsCOMPtr<nsIDocShellTreeOwner> newTreeOwner;
    newDocShellItem->GetTreeOwner(getter_AddRefs(newTreeOwner));
    nsCOMPtr<nsIWebBrowserChrome> newChrome(do_GetInterface(newTreeOwner));

    // Throw an exception here if no web browser chrome is available,
    // we need that to show a modal window.
    NS_ENSURE_TRUE(newChrome, NS_ERROR_NOT_AVAILABLE);

    // Dispatch dialog events etc, but we only want to do that if
    // we're opening a modal content window (the helper classes are
    // no-ops if given no window), for chrome dialogs we don't want to
    // do any of that (it's done elsewhere for us).
    // Make sure we maintain the state on an outer window, because
    // that's where it lives; inner windows assert if you try to
    // maintain the state on them.
    nsAutoWindowStateHelper windowStateHelper(
      parentWindow ? parentWindow->GetOuterWindow() : nullptr);

    if (!windowStateHelper.DefaultEnabled()) {
      // Default to cancel not opening the modal window.
      NS_RELEASE(*aResult);

      return NS_OK;
    }

    if (!newWindowShouldBeModal && parentIsModal) {
      nsCOMPtr<nsIBaseWindow> parentWindow(do_GetInterface(newTreeOwner));
      if (parentWindow) {
        nsCOMPtr<nsIWidget> parentWidget;
        parentWindow->GetMainWidget(getter_AddRefs(parentWidget));
        if (parentWidget) {
          parentWidget->SetModal(true);
        }
      }
    } else {
      // Reset popup state while opening a modal dialog, and firing
      // events about the dialog, to prevent the current state from
      // being active the whole time a modal dialog is open.
      nsAutoPopupStatePusher popupStatePusher(openAbused);

      newChrome->ShowAsModal();
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::RegisterNotification(nsIObserver* aObserver)
{
  // just a convenience method; it delegates to nsIObserverService

  if (!aObserver) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_FAILURE;
  }

  nsresult rv = os->AddObserver(aObserver, "domwindowopened", false);
  if (NS_SUCCEEDED(rv)) {
    rv = os->AddObserver(aObserver, "domwindowclosed", false);
  }

  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::UnregisterNotification(nsIObserver* aObserver)
{
  // just a convenience method; it delegates to nsIObserverService

  if (!aObserver) {
    return NS_ERROR_INVALID_ARG;
  }

  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_FAILURE;
  }

  os->RemoveObserver(aObserver, "domwindowopened");
  os->RemoveObserver(aObserver, "domwindowclosed");

  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetWindowEnumerator(nsISimpleEnumerator** aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_ARG;
  }

  MutexAutoLock lock(mListLock);
  nsWatcherWindowEnumerator* enumerator = new nsWatcherWindowEnumerator(this);
  if (enumerator) {
    return CallQueryInterface(enumerator, aResult);
  }

  return NS_ERROR_OUT_OF_MEMORY;
}

NS_IMETHODIMP
nsWindowWatcher::GetNewPrompter(nsIDOMWindow* aParent, nsIPrompt** aResult)
{
  // This is for backwards compat only. Callers should just use the prompt
  // service directly.
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> factory =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return factory->GetPrompt(aParent, NS_GET_IID(nsIPrompt),
                            reinterpret_cast<void**>(aResult));
}

NS_IMETHODIMP
nsWindowWatcher::GetNewAuthPrompter(nsIDOMWindow* aParent,
                                    nsIAuthPrompt** aResult)
{
  // This is for backwards compat only. Callers should just use the prompt
  // service directly.
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> factory =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  return factory->GetPrompt(aParent, NS_GET_IID(nsIAuthPrompt),
                            reinterpret_cast<void**>(aResult));
}

NS_IMETHODIMP
nsWindowWatcher::GetPrompt(nsIDOMWindow* aParent, const nsIID& aIID,
                           void** aResult)
{
  // This is for backwards compat only. Callers should just use the prompt
  // service directly.
  nsresult rv;
  nsCOMPtr<nsIPromptFactory> factory =
    do_GetService("@mozilla.org/prompter;1", &rv);
  NS_ENSURE_SUCCESS(rv, rv);
  rv = factory->GetPrompt(aParent, aIID, aResult);

  // Allow for an embedding implementation to not support nsIAuthPrompt2.
  if (rv == NS_NOINTERFACE && aIID.Equals(NS_GET_IID(nsIAuthPrompt2))) {
    nsCOMPtr<nsIAuthPrompt> oldPrompt;
    rv = factory->GetPrompt(
      aParent, NS_GET_IID(nsIAuthPrompt), getter_AddRefs(oldPrompt));
    NS_ENSURE_SUCCESS(rv, rv);

    NS_WrapAuthPrompt(oldPrompt, reinterpret_cast<nsIAuthPrompt2**>(aResult));
    if (!*aResult) {
      rv = NS_ERROR_NOT_AVAILABLE;
    }
  }
  return rv;
}

NS_IMETHODIMP
nsWindowWatcher::SetWindowCreator(nsIWindowCreator* aCreator)
{
  mWindowCreator = aCreator;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::HasWindowCreator(bool* aResult)
{
  *aResult = mWindowCreator;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetActiveWindow(nsIDOMWindow** aActiveWindow)
{
  *aActiveWindow = nullptr;
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    return fm->GetActiveWindow(aActiveWindow);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::SetActiveWindow(nsIDOMWindow* aActiveWindow)
{
  nsCOMPtr<nsIFocusManager> fm = do_GetService(FOCUSMANAGER_CONTRACTID);
  if (fm) {
    return fm->SetActiveWindow(aActiveWindow);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::AddWindow(nsIDOMWindow* aWindow, nsIWebBrowserChrome* aChrome)
{
  if (!aWindow) {
    return NS_ERROR_INVALID_ARG;
  }

#ifdef DEBUG
  {
    nsCOMPtr<nsPIDOMWindow> win(do_QueryInterface(aWindow));

    NS_ASSERTION(win->IsOuterWindow(),
                 "Uh, the active window must be an outer window!");
  }
#endif

  {
    nsWatcherWindowEntry* info;
    MutexAutoLock lock(mListLock);

    // if we already have an entry for this window, adjust
    // its chrome mapping and return
    info = FindWindowEntry(aWindow);
    if (info) {
      nsCOMPtr<nsISupportsWeakReference> supportsweak(
        do_QueryInterface(aChrome));
      if (supportsweak) {
        supportsweak->GetWeakReference(getter_AddRefs(info->mChromeWeak));
      } else {
        info->mChrome = aChrome;
        info->mChromeWeak = 0;
      }
      return NS_OK;
    }

    // create a window info struct and add it to the list of windows
    info = new nsWatcherWindowEntry(aWindow, aChrome);
    if (!info) {
      return NS_ERROR_OUT_OF_MEMORY;
    }

    if (mOldestWindow) {
      info->InsertAfter(mOldestWindow->mOlder);
    } else {
      mOldestWindow = info;
    }
  } // leave the mListLock

  // a window being added to us signifies a newly opened window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (!os) {
    return NS_ERROR_FAILURE;
  }

  nsCOMPtr<nsISupports> domwin(do_QueryInterface(aWindow));
  return os->NotifyObservers(domwin, "domwindowopened", 0);
}

NS_IMETHODIMP
nsWindowWatcher::RemoveWindow(nsIDOMWindow* aWindow)
{
  // find the corresponding nsWatcherWindowEntry, remove it

  if (!aWindow) {
    return NS_ERROR_INVALID_ARG;
  }

  nsWatcherWindowEntry* info = FindWindowEntry(aWindow);
  if (info) {
    RemoveWindow(info);
    return NS_OK;
  }
  NS_WARNING("requested removal of nonexistent window");
  return NS_ERROR_INVALID_ARG;
}

nsWatcherWindowEntry*
nsWindowWatcher::FindWindowEntry(nsIDOMWindow* aWindow)
{
  // find the corresponding nsWatcherWindowEntry
  nsWatcherWindowEntry* info;
  nsWatcherWindowEntry* listEnd;
#ifdef USEWEAKREFS
  nsresult rv;
  bool found;
#endif

  info = mOldestWindow;
  listEnd = 0;
#ifdef USEWEAKREFS
  rv = NS_OK;
  found = false;
  while (info != listEnd && NS_SUCCEEDED(rv)) {
    nsCOMPtr<nsIDOMWindow> infoWindow(do_QueryReferent(info->mWindow));
    if (!infoWindow) { // clean up dangling reference, while we're here
      rv = RemoveWindow(info);
    } else if (infoWindow.get() == aWindow) {
      return info;
    }

    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
#else
  while (info != listEnd) {
    if (info->mWindow == aWindow) {
      return info;
    }
    info = info->mYounger;
    listEnd = mOldestWindow;
  }
  return 0;
#endif
}

nsresult
nsWindowWatcher::RemoveWindow(nsWatcherWindowEntry* aInfo)
{
  uint32_t count = mEnumeratorList.Length();

  {
    // notify the enumerators
    MutexAutoLock lock(mListLock);
    for (uint32_t ctr = 0; ctr < count; ++ctr) {
      mEnumeratorList[ctr]->WindowRemoved(aInfo);
    }

    // remove the element from the list
    if (aInfo == mOldestWindow) {
      mOldestWindow = aInfo->mYounger == mOldestWindow ? 0 : aInfo->mYounger;
    }
    aInfo->Unlink();
  }

  // a window being removed from us signifies a newly closed window.
  // send notifications.
  nsCOMPtr<nsIObserverService> os = mozilla::services::GetObserverService();
  if (os) {
#ifdef USEWEAKREFS
    nsCOMPtr<nsISupports> domwin(do_QueryReferent(aInfo->mWindow));
    if (domwin) {
      os->NotifyObservers(domwin, "domwindowclosed", 0);
    }
    // else bummer. since the window is gone, there's nothing to notify with.
#else
    nsCOMPtr<nsISupports> domwin(do_QueryInterface(aInfo->mWindow));
    os->NotifyObservers(domwin, "domwindowclosed", 0);
#endif
  }

  delete aInfo;
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetChromeForWindow(nsIDOMWindow* aWindow,
                                    nsIWebBrowserChrome** aResult)
{
  if (!aWindow || !aResult) {
    return NS_ERROR_INVALID_ARG;
  }
  *aResult = 0;

  MutexAutoLock lock(mListLock);
  nsWatcherWindowEntry* info = FindWindowEntry(aWindow);
  if (info) {
    if (info->mChromeWeak) {
      return info->mChromeWeak->QueryReferent(
        NS_GET_IID(nsIWebBrowserChrome), reinterpret_cast<void**>(aResult));
    }
    *aResult = info->mChrome;
    NS_IF_ADDREF(*aResult);
  }
  return NS_OK;
}

NS_IMETHODIMP
nsWindowWatcher::GetWindowByName(const char16_t* aTargetName,
                                 nsIDOMWindow* aCurrentWindow,
                                 nsIDOMWindow** aResult)
{
  if (!aResult) {
    return NS_ERROR_INVALID_ARG;
  }

  *aResult = nullptr;

  nsCOMPtr<nsIDocShellTreeItem> treeItem;

  nsCOMPtr<nsIDocShellTreeItem> startItem;
  GetWindowTreeItem(aCurrentWindow, getter_AddRefs(startItem));
  if (startItem) {
    // Note: original requestor is null here, per idl comments
    startItem->FindItemWithName(aTargetName, nullptr, nullptr,
                                getter_AddRefs(treeItem));
  } else {
    // Note: original requestor is null here, per idl comments
    FindItemWithName(aTargetName, nullptr, nullptr, getter_AddRefs(treeItem));
  }

  if (treeItem) {
    nsCOMPtr<nsIDOMWindow> domWindow = treeItem->GetWindow();
    domWindow.forget(aResult);
  }

  return NS_OK;
}

bool
nsWindowWatcher::AddEnumerator(nsWatcherWindowEnumerator* aEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.AppendElement(aEnumerator) != nullptr;
}

bool
nsWindowWatcher::RemoveEnumerator(nsWatcherWindowEnumerator* aEnumerator)
{
  // (requires a lock; assumes it's called by someone holding the lock)
  return mEnumeratorList.RemoveElement(aEnumerator);
}

nsresult
nsWindowWatcher::URIfromURL(const char* aURL,
                            nsIDOMWindow* aParent,
                            nsIURI** aURI)
{
  // Build the URI relative to the entry global.
  nsCOMPtr<nsIDOMWindow> baseWindow = do_QueryInterface(GetEntryGlobal());

  // failing that, build it relative to the parent window, if possible
  if (!baseWindow) {
    baseWindow = aParent;
  }

  // failing that, use the given URL unmodified. It had better not be relative.

  nsIURI* baseURI = nullptr;

  // get baseWindow's document URI
  if (baseWindow) {
    nsCOMPtr<nsIDOMDocument> domDoc;
    baseWindow->GetDocument(getter_AddRefs(domDoc));
    if (domDoc) {
      nsCOMPtr<nsIDocument> doc;
      doc = do_QueryInterface(domDoc);
      if (doc) {
        baseURI = doc->GetDocBaseURI();
      }
    }
  }

  // build and return the absolute URI
  return NS_NewURI(aURI, aURL, baseURI);
}

#define NS_CALCULATE_CHROME_FLAG_FOR(feature, flag)                            \
  prefBranch->GetBoolPref(feature, &forceEnable);                              \
  if (forceEnable && !(aDialog && isCallerChrome) &&                           \
      !(isCallerChrome && aHasChromeParent) && !aChromeURL) {                  \
    chromeFlags |= flag;                                                       \
  } else {                                                                     \
    chromeFlags |=                                                             \
      WinHasOption(aFeatures, feature, 0, &presenceFlag) ? flag : 0;           \
  }

/**
 * Calculate the chrome bitmask from a string list of features.
 * @param aParent the opener window
 * @param aFeatures a string containing a list of named chrome features
 * @param aNullFeatures true if aFeatures was a null pointer (which fact
 *                      is lost by its conversion to a string in the caller)
 * @param aDialog affects the assumptions made about unnamed features
 * @return the chrome bitmask
 */
// static
uint32_t
nsWindowWatcher::CalculateChromeFlags(nsIDOMWindow* aParent,
                                      const char* aFeatures,
                                      bool aFeaturesSpecified,
                                      bool aDialog,
                                      bool aChromeURL,
                                      bool aHasChromeParent,
                                      bool aOpenedFromRemoteTab)
{
  if (!aFeaturesSpecified || !aFeatures) {
    if (aDialog) {
      return nsIWebBrowserChrome::CHROME_ALL |
             nsIWebBrowserChrome::CHROME_OPENAS_DIALOG |
             nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
    } else {
      return nsIWebBrowserChrome::CHROME_ALL;
    }
  }

  /* This function has become complicated since browser windows and
     dialogs diverged. The difference is, browser windows assume all
     chrome not explicitly mentioned is off, if the features string
     is not null. Exceptions are some OS border chrome new with Mozilla.
     Dialogs interpret a (mostly) empty features string to mean
     "OS's choice," and also support an "all" flag explicitly disallowed
     in the standards-compliant window.(normal)open. */

  uint32_t chromeFlags = 0;
  bool presenceFlag = false;

  chromeFlags = nsIWebBrowserChrome::CHROME_WINDOW_BORDERS;
  if (aDialog && WinHasOption(aFeatures, "all", 0, &presenceFlag)) {
    chromeFlags = nsIWebBrowserChrome::CHROME_ALL;
  }

  /* Next, allow explicitly named options to override the initial settings */

  bool isCallerChrome =
    nsContentUtils::IsCallerChrome() && !aOpenedFromRemoteTab;

  // Determine whether the window is a private browsing window
  if (isCallerChrome) {
    chromeFlags |= WinHasOption(aFeatures, "private", 0, &presenceFlag) ?
      nsIWebBrowserChrome::CHROME_PRIVATE_WINDOW : 0;
    chromeFlags |= WinHasOption(aFeatures, "non-private", 0, &presenceFlag) ?
      nsIWebBrowserChrome::CHROME_NON_PRIVATE_WINDOW : 0;
  }

  // Determine whether the window should have remote tabs.
  if (isCallerChrome || aOpenedFromRemoteTab) {
    bool remote;
    if (BrowserTabsRemoteAutostart()) {
      remote = !WinHasOption(aFeatures, "non-remote", 0, &presenceFlag);
    } else {
      remote = WinHasOption(aFeatures, "remote", 0, &presenceFlag);
    }
    if (remote) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_REMOTE_WINDOW;
    }
  }

  nsresult rv;

  nsCOMPtr<nsIPrefBranch> prefBranch;
  nsCOMPtr<nsIPrefService> prefs =
    do_GetService(NS_PREFSERVICE_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, true);

  rv = prefs->GetBranch("dom.disable_window_open_feature.",
                        getter_AddRefs(prefBranch));
  NS_ENSURE_SUCCESS(rv, true);

  bool forceEnable = false;

  NS_CALCULATE_CHROME_FLAG_FOR("titlebar",
                               nsIWebBrowserChrome::CHROME_TITLEBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("close",
                               nsIWebBrowserChrome::CHROME_WINDOW_CLOSE);
  NS_CALCULATE_CHROME_FLAG_FOR("toolbar",
                               nsIWebBrowserChrome::CHROME_TOOLBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("location",
                               nsIWebBrowserChrome::CHROME_LOCATIONBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("personalbar",
                               nsIWebBrowserChrome::CHROME_PERSONAL_TOOLBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("status",
                               nsIWebBrowserChrome::CHROME_STATUSBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("menubar",
                               nsIWebBrowserChrome::CHROME_MENUBAR);
  NS_CALCULATE_CHROME_FLAG_FOR("scrollbars",
                               nsIWebBrowserChrome::CHROME_SCROLLBARS);
  NS_CALCULATE_CHROME_FLAG_FOR("resizable",
                               nsIWebBrowserChrome::CHROME_WINDOW_RESIZE);
  NS_CALCULATE_CHROME_FLAG_FOR("minimizable",
                               nsIWebBrowserChrome::CHROME_WINDOW_MIN);

  chromeFlags |= WinHasOption(aFeatures, "popup", 0, &presenceFlag) ?
    nsIWebBrowserChrome::CHROME_WINDOW_POPUP : 0;

  /* OK.
     Normal browser windows, in spite of a stated pattern of turning off
     all chrome not mentioned explicitly, will want the new OS chrome (window
     borders, titlebars, closebox) on, unless explicitly turned off.
     Dialogs, on the other hand, take the absence of any explicit settings
     to mean "OS' choice." */

  // default titlebar and closebox to "on," if not mentioned at all
  if (!(chromeFlags & nsIWebBrowserChrome::CHROME_WINDOW_POPUP)) {
    if (!PL_strcasestr(aFeatures, "titlebar")) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_TITLEBAR;
    }
    if (!PL_strcasestr(aFeatures, "close")) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_CLOSE;
    }
  }

  if (aDialog && !presenceFlag) {
    chromeFlags = nsIWebBrowserChrome::CHROME_DEFAULT;
  }

  /* Finally, once all the above normal chrome has been divined, deal
     with the features that are more operating hints than appearance
     instructions. (Note modality implies dependence.) */

  if (WinHasOption(aFeatures, "alwaysLowered", 0, nullptr) ||
      WinHasOption(aFeatures, "z-lock", 0, nullptr)) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_LOWERED;
  } else if (WinHasOption(aFeatures, "alwaysRaised", 0, nullptr)) {
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_RAISED;
  }

  chromeFlags |= WinHasOption(aFeatures, "macsuppressanimation", 0, nullptr) ?
    nsIWebBrowserChrome::CHROME_MAC_SUPPRESS_ANIMATION : 0;

  chromeFlags |= WinHasOption(aFeatures, "chrome", 0, nullptr) ?
    nsIWebBrowserChrome::CHROME_OPENAS_CHROME : 0;
  chromeFlags |= WinHasOption(aFeatures, "extrachrome", 0, nullptr) ?
    nsIWebBrowserChrome::CHROME_EXTRA : 0;
  chromeFlags |= WinHasOption(aFeatures, "centerscreen", 0, nullptr) ?
    nsIWebBrowserChrome::CHROME_CENTER_SCREEN : 0;
  chromeFlags |= WinHasOption(aFeatures, "dependent", 0, nullptr) ?
    nsIWebBrowserChrome::CHROME_DEPENDENT : 0;
  chromeFlags |= WinHasOption(aFeatures, "modal", 0, nullptr) ?
    (nsIWebBrowserChrome::CHROME_MODAL | nsIWebBrowserChrome::CHROME_DEPENDENT) : 0;

  /* On mobile we want to ignore the dialog window feature, since the mobile UI
     does not provide any affordance for dialog windows. This does not interfere
     with dialog windows created through openDialog. */
  bool disableDialogFeature = false;
  nsCOMPtr<nsIPrefBranch> branch = do_QueryInterface(prefs);
  branch->GetBoolPref("dom.disable_window_open_dialog_feature",
                      &disableDialogFeature);

  bool isFullScreen = false;
  if (aParent) {
    aParent->GetFullScreen(&isFullScreen);
  }
  if (isFullScreen && !isCallerChrome) {
    // If the parent window is in fullscreen & the caller context is content,
    // dialog feature is disabled. (see bug 803675)
    disableDialogFeature = true;
  }

  if (!disableDialogFeature) {
    chromeFlags |= WinHasOption(aFeatures, "dialog", 0, nullptr) ?
      nsIWebBrowserChrome::CHROME_OPENAS_DIALOG : 0;
  }

  /* and dialogs need to have the last word. assume dialogs are dialogs,
     and opened as chrome, unless explicitly told otherwise. */
  if (aDialog) {
    if (!PL_strcasestr(aFeatures, "dialog")) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_OPENAS_DIALOG;
    }
    if (!PL_strcasestr(aFeatures, "chrome")) {
      chromeFlags |= nsIWebBrowserChrome::CHROME_OPENAS_CHROME;
    }
  }

  /* missing
     chromeFlags->copy_history
   */

  // Check security state for use in determing window dimensions
  if (!isCallerChrome || !aHasChromeParent) {
    // If priv check fails (or if we're called from chrome, but the
    // parent is not a chrome window), set all elements to minimum
    // reqs., else leave them alone.
    chromeFlags |= nsIWebBrowserChrome::CHROME_TITLEBAR;
    chromeFlags |= nsIWebBrowserChrome::CHROME_WINDOW_CLOSE;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_LOWERED;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_RAISED;
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_WINDOW_POPUP;
    /* Untrusted script is allowed to pose modal windows with a chrome
       scheme. This check could stand to be better. But it effectively
       prevents untrusted script from opening modal windows in general
       while still allowing alerts and the like. */
    if (!aChromeURL)
      chromeFlags &= ~(nsIWebBrowserChrome::CHROME_MODAL |
                       nsIWebBrowserChrome::CHROME_OPENAS_CHROME);
  }

  if (!(chromeFlags & nsIWebBrowserChrome::CHROME_OPENAS_CHROME)) {
    // Remove the dependent flag if we're not opening as chrome
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_DEPENDENT;
  }

  // Disable CHROME_OPENAS_DIALOG if the window is inside <iframe mozbrowser>.
  // It's up to the embedder to interpret what dialog=1 means.
  nsCOMPtr<nsIDocShell> docshell = do_GetInterface(aParent);
  if (docshell && docshell->GetIsInBrowserOrApp()) {
    chromeFlags &= ~nsIWebBrowserChrome::CHROME_OPENAS_DIALOG;
  }

  return chromeFlags;
}

// static
int32_t
nsWindowWatcher::WinHasOption(const char* aOptions, const char* aName,
                              int32_t aDefault, bool* aPresenceFlag)
{
  if (!aOptions) {
    return 0;
  }

  char* comma;
  char* equal;
  int32_t found = 0;

#ifdef DEBUG
  nsAutoCString options(aOptions);
  NS_ASSERTION(options.FindCharInSet(" \n\r\t") == kNotFound,
               "There should be no whitespace in this string!");
#endif

  while (true) {
    comma = PL_strchr(aOptions, ',');
    if (comma) {
      *comma = '\0';
    }
    equal = PL_strchr(aOptions, '=');
    if (equal) {
      *equal = '\0';
    }
    if (nsCRT::strcasecmp(aOptions, aName) == 0) {
      if (aPresenceFlag) {
        *aPresenceFlag = true;
      }
      if (equal)
        if (*(equal + 1) == '*') {
          found = aDefault;
        } else if (nsCRT::strcasecmp(equal + 1, "yes") == 0) {
          found = 1;
        } else {
          found = atoi(equal + 1);
        }
      else {
        found = 1;
      }
    }
    if (equal) {
      *equal = '=';
    }
    if (comma) {
      *comma = ',';
    }
    if (found || !comma) {
      break;
    }
    aOptions = comma + 1;
  }
  return found;
}

/* try to find an nsIDocShellTreeItem with the given name in any
   known open window. a failure to find the item will not
   necessarily return a failure method value. check aFoundItem.
*/
NS_IMETHODIMP
nsWindowWatcher::FindItemWithName(const char16_t* aName,
                                  nsIDocShellTreeItem* aRequestor,
                                  nsIDocShellTreeItem* aOriginalRequestor,
                                  nsIDocShellTreeItem** aFoundItem)
{
  *aFoundItem = 0;

  /* special cases */
  if (!aName || !*aName) {
    return NS_OK;
  }

  nsDependentString name(aName);

  nsCOMPtr<nsISimpleEnumerator> windows;
  GetWindowEnumerator(getter_AddRefs(windows));
  if (!windows) {
    return NS_ERROR_FAILURE;
  }

  bool more;
  nsresult rv = NS_OK;

  do {
    windows->HasMoreElements(&more);
    if (!more) {
      break;
    }
    nsCOMPtr<nsISupports> nextSupWindow;
    windows->GetNext(getter_AddRefs(nextSupWindow));
    nsCOMPtr<nsIDOMWindow> nextWindow(do_QueryInterface(nextSupWindow));
    if (nextWindow) {
      nsCOMPtr<nsIDocShellTreeItem> treeItem;
      GetWindowTreeItem(nextWindow, getter_AddRefs(treeItem));
      if (treeItem) {
        // Get the root tree item of same type, since roots are the only
        // things that call into the treeowner to look for named items.
        nsCOMPtr<nsIDocShellTreeItem> root;
        treeItem->GetSameTypeRootTreeItem(getter_AddRefs(root));
        NS_ASSERTION(root, "Must have root tree item of same type");
        // Make sure not to call back into aRequestor
        if (root != aRequestor) {
          // Get the tree owner so we can pass it in as the requestor so
          // the child knows not to call back up, since we're walking
          // all windows already.
          nsCOMPtr<nsIDocShellTreeOwner> rootOwner;
          // Note: if we have no aRequestor, then we want to also look for
          // "special" window names, so pass a null requestor.  This will mean
          // that the treeitem calls back up to us, effectively (with a
          // non-null aRequestor), so break the loop immediately after the
          // call in that case.
          if (aRequestor) {
            root->GetTreeOwner(getter_AddRefs(rootOwner));
          }
          rv = root->FindItemWithName(aName, rootOwner, aOriginalRequestor,
                                      aFoundItem);
          if (NS_FAILED(rv) || *aFoundItem || !aRequestor) {
            break;
          }
        }
      }
    }
  } while (1);

  return rv;
}

already_AddRefed<nsIDocShellTreeItem>
nsWindowWatcher::GetCallerTreeItem(nsIDocShellTreeItem* aParentItem)
{
  nsCOMPtr<nsIWebNavigation> callerWebNav = do_GetInterface(GetEntryGlobal());
  nsCOMPtr<nsIDocShellTreeItem> callerItem = do_QueryInterface(callerWebNav);
  if (!callerItem) {
    callerItem = aParentItem;
  }

  return callerItem.forget();
}

nsPIDOMWindow*
nsWindowWatcher::SafeGetWindowByName(const nsAString& aName,
                                     nsIDOMWindow* aCurrentWindow)
{
  nsCOMPtr<nsIDocShellTreeItem> startItem;
  GetWindowTreeItem(aCurrentWindow, getter_AddRefs(startItem));

  nsCOMPtr<nsIDocShellTreeItem> callerItem = GetCallerTreeItem(startItem);

  const nsAFlatString& flatName = PromiseFlatString(aName);

  nsCOMPtr<nsIDocShellTreeItem> foundItem;
  if (startItem) {
    startItem->FindItemWithName(flatName.get(), nullptr, callerItem,
                                getter_AddRefs(foundItem));
  } else {
    FindItemWithName(flatName.get(), nullptr, callerItem,
                     getter_AddRefs(foundItem));
  }

  return foundItem ? foundItem->GetWindow() : nullptr;
}

/* Fetch the nsIDOMWindow corresponding to the given nsIDocShellTreeItem.
   This forces the creation of a script context, if one has not already
   been created. Note it also sets the window's opener to the parent,
   if applicable -- because it's just convenient, that's all. null aParent
   is acceptable. */
nsresult
nsWindowWatcher::ReadyOpenedDocShellItem(nsIDocShellTreeItem* aOpenedItem,
                                         nsIDOMWindow* aParent,
                                         bool aWindowIsNew,
                                         nsIDOMWindow** aOpenedWindow)
{
  nsresult rv = NS_ERROR_FAILURE;

  NS_ENSURE_ARG(aOpenedWindow);

  *aOpenedWindow = 0;
  nsCOMPtr<nsPIDOMWindow> piOpenedWindow = aOpenedItem->GetWindow();
  if (piOpenedWindow) {
    if (aParent) {
      piOpenedWindow->SetOpenerWindow(aParent, aWindowIsNew); // damnit

      if (aWindowIsNew) {
#ifdef DEBUG
        // Assert that we're not loading things right now.  If we are, when
        // that load completes it will clobber whatever principals we set up
        // on this new window!
        nsCOMPtr<nsIDocumentLoader> docloader = do_QueryInterface(aOpenedItem);
        NS_ASSERTION(docloader, "How can we not have a docloader here?");

        nsCOMPtr<nsIChannel> chan;
        docloader->GetDocumentChannel(getter_AddRefs(chan));
        NS_ASSERTION(!chan, "Why is there a document channel?");
#endif

        nsCOMPtr<nsIDocument> doc = piOpenedWindow->GetExtantDoc();
        if (doc) {
          doc->SetIsInitialDocument(true);
        }
      }
    }
    rv = CallQueryInterface(piOpenedWindow, aOpenedWindow);
  }
  return rv;
}

// static
void
nsWindowWatcher::CalcSizeSpec(const char* aFeatures, SizeSpec& aResult)
{
  // Parse position spec, if any, from aFeatures
  bool present;
  int32_t temp;

  present = false;
  if ((temp = WinHasOption(aFeatures, "left", 0, &present)) || present) {
    aResult.mLeft = temp;
  } else if ((temp = WinHasOption(aFeatures, "screenX", 0, &present)) ||
             present) {
    aResult.mLeft = temp;
  }
  aResult.mLeftSpecified = present;

  present = false;
  if ((temp = WinHasOption(aFeatures, "top", 0, &present)) || present) {
    aResult.mTop = temp;
  } else if ((temp = WinHasOption(aFeatures, "screenY", 0, &present)) ||
             present) {
    aResult.mTop = temp;
  }
  aResult.mTopSpecified = present;

  // Parse size spec, if any. Chrome size overrides content size.
  if ((temp = WinHasOption(aFeatures, "outerWidth", INT32_MIN, nullptr))) {
    if (temp == INT32_MIN) {
      aResult.mUseDefaultWidth = true;
    } else {
      aResult.mOuterWidth = temp;
    }
    aResult.mOuterWidthSpecified = true;
  } else if ((temp = WinHasOption(aFeatures, "width", INT32_MIN, nullptr)) ||
             (temp = WinHasOption(aFeatures, "innerWidth", INT32_MIN,
                                  nullptr))) {
    if (temp == INT32_MIN) {
      aResult.mUseDefaultWidth = true;
    } else {
      aResult.mInnerWidth = temp;
    }
    aResult.mInnerWidthSpecified = true;
  }

  if ((temp = WinHasOption(aFeatures, "outerHeight", INT32_MIN, nullptr))) {
    if (temp == INT32_MIN) {
      aResult.mUseDefaultHeight = true;
    } else {
      aResult.mOuterHeight = temp;
    }
    aResult.mOuterHeightSpecified = true;
  } else if ((temp = WinHasOption(aFeatures, "height", INT32_MIN,
                                  nullptr)) ||
             (temp = WinHasOption(aFeatures, "innerHeight", INT32_MIN,
                                  nullptr))) {
    if (temp == INT32_MIN) {
      aResult.mUseDefaultHeight = true;
    } else {
      aResult.mInnerHeight = temp;
    }
    aResult.mInnerHeightSpecified = true;
  }
}

/* Size and position the new window according to aSizeSpec. This method
   is assumed to be called after the window has already been given
   a default position and size; thus its current position and size are
   accurate defaults. The new window is made visible at method end.
*/
void
nsWindowWatcher::SizeOpenedDocShellItem(nsIDocShellTreeItem* aDocShellItem,
                                        nsIDOMWindow* aParent,
                                        bool aIsCallerChrome,
                                        const SizeSpec& aSizeSpec)
{
  // position and size of window
  int32_t left = 0, top = 0, width = 100, height = 100;
  // difference between chrome and content size
  int32_t chromeWidth = 0, chromeHeight = 0;
  // whether the window size spec refers to chrome or content
  bool sizeChromeWidth = true, sizeChromeHeight = true;

  // get various interfaces for aDocShellItem, used throughout this method
  nsCOMPtr<nsIDocShellTreeOwner> treeOwner;
  aDocShellItem->GetTreeOwner(getter_AddRefs(treeOwner));
  nsCOMPtr<nsIBaseWindow> treeOwnerAsWin(do_QueryInterface(treeOwner));
  if (!treeOwnerAsWin) { // we'll need this to actually size the docshell
    return;
  }

  double openerZoom = 1.0;
  if (aParent) {
    nsCOMPtr<nsIDOMDocument> openerDoc;
    aParent->GetDocument(getter_AddRefs(openerDoc));
    if (openerDoc) {
      nsCOMPtr<nsIDocument> doc = do_QueryInterface(openerDoc);
      nsIPresShell* shell = doc->GetShell();
      if (shell) {
        nsPresContext* presContext = shell->GetPresContext();
        if (presContext) {
          openerZoom = presContext->GetFullZoom();
        }
      }
    }
  }

  double scale;
  treeOwnerAsWin->GetUnscaledDevicePixelsPerCSSPixel(&scale);

  /* The current position and size will be unchanged if not specified
     (and they fit entirely onscreen). Also, calculate the difference
     between chrome and content sizes on aDocShellItem's window.
     This latter point becomes important if chrome and content
     specifications are mixed in aFeatures, and when bringing the window
     back from too far off the right or bottom edges of the screen. */

  treeOwnerAsWin->GetPositionAndSize(&left, &top, &width, &height);
  left = NSToIntRound(left / scale);
  top = NSToIntRound(top / scale);
  width = NSToIntRound(width / scale);
  height = NSToIntRound(height / scale);
  {
    // scope shellWindow why not
    nsCOMPtr<nsIBaseWindow> shellWindow(do_QueryInterface(aDocShellItem));
    if (shellWindow) {
      int32_t cox, coy;
      double shellScale;
      shellWindow->GetSize(&cox, &coy);
      shellWindow->GetUnscaledDevicePixelsPerCSSPixel(&shellScale);
      chromeWidth = width - NSToIntRound(cox / shellScale);
      chromeHeight = height - NSToIntRound(coy / shellScale);
    }
  }

  // Set up left/top
  if (aSizeSpec.mLeftSpecified) {
    left = NSToIntRound(aSizeSpec.mLeft * openerZoom);
  }

  if (aSizeSpec.mTopSpecified) {
    top = NSToIntRound(aSizeSpec.mTop * openerZoom);
  }

  // Set up width
  if (aSizeSpec.mOuterWidthSpecified) {
    if (!aSizeSpec.mUseDefaultWidth) {
      width = NSToIntRound(aSizeSpec.mOuterWidth * openerZoom);
    } // Else specified to default; just use our existing width
  } else if (aSizeSpec.mInnerWidthSpecified) {
    sizeChromeWidth = false;
    if (aSizeSpec.mUseDefaultWidth) {
      width = width - chromeWidth;
    } else {
      width = NSToIntRound(aSizeSpec.mInnerWidth * openerZoom);
    }
  }

  // Set up height
  if (aSizeSpec.mOuterHeightSpecified) {
    if (!aSizeSpec.mUseDefaultHeight) {
      height = NSToIntRound(aSizeSpec.mOuterHeight * openerZoom);
    } // Else specified to default; just use our existing height
  } else if (aSizeSpec.mInnerHeightSpecified) {
    sizeChromeHeight = false;
    if (aSizeSpec.mUseDefaultHeight) {
      height = height - chromeHeight;
    } else {
      height = NSToIntRound(aSizeSpec.mInnerHeight * openerZoom);
    }
  }

  bool positionSpecified = aSizeSpec.PositionSpecified();

  // Check security state for use in determing window dimensions
  bool enabled = false;
  if (aIsCallerChrome) {
    // Only enable special priveleges for chrome when chrome calls
    // open() on a chrome window
    nsCOMPtr<nsIDOMChromeWindow> chromeWin(do_QueryInterface(aParent));
    enabled = !aParent || chromeWin;
  }

  if (!enabled) {

    // Security check failed.  Ensure all args meet minimum reqs.

    int32_t oldTop = top, oldLeft = left;

    // We'll also need the screen dimensions
    nsCOMPtr<nsIScreen> screen;
    nsCOMPtr<nsIScreenManager> screenMgr(
      do_GetService("@mozilla.org/gfx/screenmanager;1"));
    if (screenMgr)
      screenMgr->ScreenForRect(left, top, width, height,
                               getter_AddRefs(screen));
    if (screen) {
      int32_t screenLeft, screenTop, screenWidth, screenHeight;
      int32_t winWidth = width + (sizeChromeWidth ? 0 : chromeWidth),
              winHeight = height + (sizeChromeHeight ? 0 : chromeHeight);

      screen->GetAvailRectDisplayPix(&screenLeft, &screenTop, &screenWidth,
                                     &screenHeight);

      if (aSizeSpec.SizeSpecified()) {
        /* Unlike position, force size out-of-bounds check only if
           size actually was specified. Otherwise, intrinsically sized
           windows are broken. */
        if (height < 100) {
          height = 100;
          winHeight = height + (sizeChromeHeight ? 0 : chromeHeight);
        }
        if (winHeight > screenHeight) {
          height = screenHeight - (sizeChromeHeight ? 0 : chromeHeight);
        }
        if (width < 100) {
          width = 100;
          winWidth = width + (sizeChromeWidth ? 0 : chromeWidth);
        }
        if (winWidth > screenWidth) {
          width = screenWidth - (sizeChromeWidth ? 0 : chromeWidth);
        }
      }

      if (left + winWidth > screenLeft + screenWidth ||
          left + winWidth < left) {
        left = screenLeft + screenWidth - winWidth;
      }
      if (left < screenLeft) {
        left = screenLeft;
      }
      if (top + winHeight > screenTop + screenHeight || top + winHeight < top) {
        top = screenTop + screenHeight - winHeight;
      }
      if (top < screenTop) {
        top = screenTop;
      }
      if (top != oldTop || left != oldLeft) {
        positionSpecified = true;
      }
    }
  }

  // size and position the window

  if (positionSpecified) {
    treeOwnerAsWin->SetPosition(left * scale, top * scale);
    // moving the window may have changed its scale factor
    treeOwnerAsWin->GetUnscaledDevicePixelsPerCSSPixel(&scale);
  }
  if (aSizeSpec.SizeSpecified()) {
    /* Prefer to trust the interfaces, which think in terms of pure
       chrome or content sizes. If we have a mix, use the chrome size
       adjusted by the chrome/content differences calculated earlier. */
    if (!sizeChromeWidth && !sizeChromeHeight) {
      treeOwner->SizeShellTo(aDocShellItem, width * scale, height * scale);
    } else {
      if (!sizeChromeWidth) {
        width += chromeWidth;
      }
      if (!sizeChromeHeight) {
        height += chromeHeight;
      }
      treeOwnerAsWin->SetSize(width * scale, height * scale, false);
    }
  }
  treeOwnerAsWin->SetVisibility(true);
}

void
nsWindowWatcher::GetWindowTreeItem(nsIDOMWindow* aWindow,
                                   nsIDocShellTreeItem** aResult)
{
  *aResult = 0;

  nsCOMPtr<nsPIDOMWindow> window(do_QueryInterface(aWindow));
  if (window) {
    nsIDocShell* docshell = window->GetDocShell();
    if (docshell) {
      CallQueryInterface(docshell, aResult);
    }
  }
}

void
nsWindowWatcher::GetWindowTreeOwner(nsIDOMWindow* aWindow,
                                    nsIDocShellTreeOwner** aResult)
{
  *aResult = 0;

  nsCOMPtr<nsIDocShellTreeItem> treeItem;
  GetWindowTreeItem(aWindow, getter_AddRefs(treeItem));
  if (treeItem) {
    treeItem->GetTreeOwner(aResult);
  }
}

/* static */
int32_t
nsWindowWatcher::GetWindowOpenLocation(nsIDOMWindow* aParent,
                                       uint32_t aChromeFlags,
                                       bool aCalledFromJS,
                                       bool aPositionSpecified,
                                       bool aSizeSpecified)
{
  bool isFullScreen = false;
  if (aParent) {
    aParent->GetFullScreen(&isFullScreen);
  }

  // Where should we open this?
  int32_t containerPref;
  if (NS_FAILED(Preferences::GetInt("browser.link.open_newwindow",
                                    &containerPref))) {
    // We couldn't read the user preference, so fall back on the default.
    return nsIBrowserDOMWindow::OPEN_NEWTAB;
  }

  bool isDisabledOpenNewWindow =
    isFullScreen &&
    Preferences::GetBool("browser.link.open_newwindow.disabled_in_fullscreen");

  if (isDisabledOpenNewWindow &&
      (containerPref == nsIBrowserDOMWindow::OPEN_NEWWINDOW)) {
    containerPref = nsIBrowserDOMWindow::OPEN_NEWTAB;
  }

  if (containerPref != nsIBrowserDOMWindow::OPEN_NEWTAB &&
      containerPref != nsIBrowserDOMWindow::OPEN_CURRENTWINDOW) {
    // Just open a window normally
    return nsIBrowserDOMWindow::OPEN_NEWWINDOW;
  }

  if (aCalledFromJS) {
    /* Now check our restriction pref.  The restriction pref is a power-user's
       fine-tuning pref. values:
       0: no restrictions - divert everything
       1: don't divert window.open at all
       2: don't divert window.open with features
    */
    int32_t restrictionPref =
      Preferences::GetInt("browser.link.open_newwindow.restriction", 2);
    if (restrictionPref < 0 || restrictionPref > 2) {
      restrictionPref = 2; // Sane default behavior
    }

    if (isDisabledOpenNewWindow) {
      // In browser fullscreen, the window should be opened
      // in the current window with no features (see bug 803675)
      restrictionPref = 0;
    }

    if (restrictionPref == 1) {
      return nsIBrowserDOMWindow::OPEN_NEWWINDOW;
    }

    if (restrictionPref == 2 &&
        // Only continue if there are no size/position features and no special
        // chrome flags.
        (aChromeFlags != nsIWebBrowserChrome::CHROME_ALL ||
         aPositionSpecified || aSizeSpecified)) {
      return nsIBrowserDOMWindow::OPEN_NEWWINDOW;
    }
  }

  return containerPref;
}
