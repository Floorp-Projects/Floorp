/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
// vim:set ts=2 sts=2 sw=2 et cin:
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifdef MOZ_X11
#  include <cairo-xlib.h>
#  include "gfxXlibSurface.h"
/* X headers suck */
enum { XKeyPress = KeyPress };
#  include "mozilla/X11Util.h"
using mozilla::DefaultXDisplay;
#endif

#include "nsPluginInstanceOwner.h"

#include "gfxUtils.h"
#include "nsIRunnable.h"
#include "nsContentUtils.h"
#include "nsRect.h"
#include "nsSize.h"
#include "nsDisplayList.h"
#include "ImageLayers.h"
#include "GLImages.h"
#include "nsIPluginDocument.h"
#include "nsIStringStream.h"
#include "nsNetUtil.h"
#include "mozilla/Preferences.h"
#include "nsLayoutUtils.h"
#include "nsViewManager.h"
#include "nsIAppShell.h"
#include "nsIObjectLoadingContent.h"
#include "nsObjectLoadingContent.h"
#include "nsAttrName.h"
#include "nsIFocusManager.h"
#include "nsFocusManager.h"
#include "nsIProtocolHandler.h"
#include "nsIScrollableFrame.h"
#include "nsIDocShell.h"
#include "ImageContainer.h"
#include "GLContext.h"
#include "nsIContentInlines.h"
#include "mozilla/MiscEvents.h"
#include "mozilla/MouseEvents.h"
#include "mozilla/NullPrincipal.h"
#include "mozilla/PresShell.h"
#include "mozilla/ProfilerLabels.h"
#include "mozilla/TextEvents.h"
#include "mozilla/dom/DocumentInlines.h"
#include "mozilla/dom/DragEvent.h"
#include "mozilla/dom/Element.h"
#include "mozilla/dom/Event.h"
#include "mozilla/dom/HTMLObjectElementBinding.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/WheelEventBinding.h"
#include "nsFrameSelection.h"
#include "PuppetWidget.h"
#include "nsPIWindowRoot.h"
#include "mozilla/IMEStateManager.h"
#include "mozilla/TextComposition.h"
#include "mozilla/AutoRestore.h"

#include "nsContentCID.h"
#include "nsWidgetsCID.h"
static NS_DEFINE_CID(kAppShellCID, NS_APPSHELL_CID);

#ifdef XP_WIN
#  include <wtypes.h>
#  include <winuser.h>
#  include "mozilla/widget/WinMessages.h"
#endif  // #ifdef XP_WIN

#ifdef MOZ_WIDGET_GTK
#  include <gdk/gdk.h>
#  include <gtk/gtk.h>
#endif

using namespace mozilla;
using namespace mozilla::dom;
using namespace mozilla::layers;

// special class for handeling DOM context menu events because for
// some reason it starves other mouse events if implemented on the
// same class
class nsPluginDOMContextMenuListener : public nsIDOMEventListener {
  virtual ~nsPluginDOMContextMenuListener();

 public:
  explicit nsPluginDOMContextMenuListener(nsIContent* aContent);

  NS_DECL_ISUPPORTS
  NS_DECL_NSIDOMEVENTLISTENER

  void Destroy(nsIContent* aContent);

  nsEventStatus ProcessEvent(const WidgetGUIEvent& anEvent) {
    return nsEventStatus_eConsumeNoDefault;
  }
};

class AsyncPaintWaitEvent : public Runnable {
 public:
  AsyncPaintWaitEvent(nsIContent* aContent, bool aFinished)
      : Runnable("AsyncPaintWaitEvent"),
        mContent(aContent),
        mFinished(aFinished) {}

  NS_IMETHOD Run() override {
    nsContentUtils::DispatchEventOnlyToChrome(
        mContent->OwnerDoc(), mContent,
        mFinished ? u"MozPaintWaitFinished"_ns : u"MozPaintWait"_ns,
        CanBubble::eYes, Cancelable::eYes);
    return NS_OK;
  }

 private:
  nsCOMPtr<nsIContent> mContent;
  bool mFinished;
};

void nsPluginInstanceOwner::NotifyPaintWaiter(nsDisplayListBuilder* aBuilder) {
  // This is notification for reftests about async plugin paint start
  if (!mWaitingForPaint && !IsUpToDate() &&
      aBuilder->ShouldSyncDecodeImages()) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, false);
    // Run this event as soon as it's safe to do so, since listeners need to
    // receive it immediately
    nsContentUtils::AddScriptRunner(event);
    mWaitingForPaint = true;
  }
}

bool nsPluginInstanceOwner::NeedsScrollImageLayer() {
#if defined(XP_WIN)
  // If this is a windowed plugin and we're doing layout in the content
  // process, force the creation of an image layer for the plugin. We'll
  // paint to this when scrolling.
  return XRE_IsContentProcess() && mPluginWindow &&
         mPluginWindow->type == NPWindowTypeWindow;
#else
  return false;
#endif
}

already_AddRefed<ImageContainer> nsPluginInstanceOwner::GetImageContainer() {
  if (!mInstance) return nullptr;

  RefPtr<ImageContainer> container;

  if (NeedsScrollImageLayer()) {
    // windowed plugin under e10s
#if defined(XP_WIN)
    mInstance->GetScrollCaptureContainer(getter_AddRefs(container));
#endif
  } else {
    // async windowless rendering
    mInstance->GetImageContainer(getter_AddRefs(container));
  }

  return container.forget();
}

void nsPluginInstanceOwner::DidComposite() {
  if (mInstance) {
    mInstance->DidComposite();
  }
}

void nsPluginInstanceOwner::SetBackgroundUnknown() {
  if (mInstance) {
    mInstance->SetBackgroundUnknown();
  }
}

already_AddRefed<mozilla::gfx::DrawTarget>
nsPluginInstanceOwner::BeginUpdateBackground(const nsIntRect& aRect) {
  nsIntRect rect = aRect;
  RefPtr<DrawTarget> dt;
  if (mInstance && NS_SUCCEEDED(mInstance->BeginUpdateBackground(
                       &rect, getter_AddRefs(dt)))) {
    return dt.forget();
  }
  return nullptr;
}

void nsPluginInstanceOwner::EndUpdateBackground(const nsIntRect& aRect) {
  nsIntRect rect = aRect;
  if (mInstance) {
    mInstance->EndUpdateBackground(&rect);
  }
}

bool nsPluginInstanceOwner::UseAsyncRendering() {
#ifdef XP_MACOSX
  if (mUseAsyncRendering) {
    return true;
  }
#endif

  bool isOOP;
  bool result =
      (mInstance && NS_SUCCEEDED(mInstance->GetIsOOP(&isOOP)) && isOOP
#ifndef XP_MACOSX
       && (!mPluginWindow || mPluginWindow->type == NPWindowTypeDrawable)
#endif
      );

#ifdef XP_MACOSX
  if (result) {
    mUseAsyncRendering = true;
  }
#endif

  return result;
}

nsIntSize nsPluginInstanceOwner::GetCurrentImageSize() {
  nsIntSize size(0, 0);
  if (mInstance) {
    mInstance->GetImageSize(&size);
  }
  return size;
}

nsPluginInstanceOwner::nsPluginInstanceOwner()
    : mPluginWindow(nullptr), mLastEventloopNestingLevel(0) {
  // create nsPluginNativeWindow object, it is derived from NPWindow
  // struct and allows to manipulate native window procedure
  nsCOMPtr<nsIPluginHost> pluginHostCOM =
      do_GetService(MOZ_PLUGIN_HOST_CONTRACTID);
  mPluginHost = static_cast<nsPluginHost*>(pluginHostCOM.get());
  if (mPluginHost) mPluginHost->NewPluginNativeWindow(&mPluginWindow);

  mWidgetCreationComplete = false;
#ifdef XP_MACOSX
  mSentInitialTopLevelWindowEvent = false;
  mLastWindowIsActive = false;
  mLastContentFocused = false;
  mLastScaleFactor = 1.0;
  mShouldBlurOnActivate = false;
#endif
  mLastCSSZoomFactor = 1.0;
  mContentFocused = false;
  mWidgetVisible = true;
  mPluginWindowVisible = false;
  mPluginDocumentActiveState = true;
  mLastMouseDownButtonType = -1;

#ifdef XP_MACOSX
#  ifndef NP_NO_CARBON
  // We don't support Carbon, but it is still the default model for i386 NPAPI.
  mEventModel = NPEventModelCarbon;
#  else
  mEventModel = NPEventModelCocoa;
#  endif
  mUseAsyncRendering = false;
#endif

  mWaitingForPaint = false;

#ifdef XP_WIN
  mGotCompositionData = false;
  mSentStartComposition = false;
  mPluginDidNotHandleIMEComposition = false;
  // 3 is the Windows default for these values.
  mWheelScrollLines = 3;
  mWheelScrollChars = 3;
#endif
}

nsPluginInstanceOwner::~nsPluginInstanceOwner() {
  if (mWaitingForPaint) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    if (content) {
      // We don't care when the event is dispatched as long as it's "soon",
      // since whoever needs it will be waiting for it.
      nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, true);
      NS_DispatchToMainThread(event);
    }
  }

  PLUG_DeletePluginNativeWindow(mPluginWindow);
  mPluginWindow = nullptr;

  if (mInstance) {
    mInstance->SetOwner(nullptr);
  }
}

NS_IMPL_ISUPPORTS(nsPluginInstanceOwner, nsIPluginInstanceOwner,
                  nsIDOMEventListener, nsIPrivacyTransitionObserver,
                  nsISupportsWeakReference)

nsresult nsPluginInstanceOwner::SetInstance(nsNPAPIPluginInstance* aInstance) {
  NS_ASSERTION(!mInstance || !aInstance,
               "mInstance should only be set or unset!");

  // If we're going to null out mInstance after use, be sure to call
  // mInstance->SetOwner(nullptr) here, since it now won't be called
  // from our destructor.  This fixes bug 613376.
  if (mInstance && !aInstance) {
    mInstance->SetOwner(nullptr);
  }

  mInstance = aInstance;

  nsCOMPtr<Document> doc;
  GetDocument(getter_AddRefs(doc));
  if (doc) {
    if (nsCOMPtr<nsPIDOMWindowOuter> domWindow = doc->GetWindow()) {
      nsCOMPtr<nsIDocShell> docShell = domWindow->GetDocShell();
      if (docShell) docShell->AddWeakPrivacyTransitionObserver(this);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetWindow(NPWindow*& aWindow) {
  NS_ASSERTION(mPluginWindow,
               "the plugin window object being returned is null");
  aWindow = mPluginWindow;
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetMode(int32_t* aMode) {
  nsCOMPtr<Document> doc;
  nsresult rv = GetDocument(getter_AddRefs(doc));
  nsCOMPtr<nsIPluginDocument> pDoc(do_QueryInterface(doc));

  if (pDoc) {
    *aMode = NP_FULL;
  } else {
    *aMode = NP_EMBED;
  }

  return rv;
}

void nsPluginInstanceOwner::GetAttributes(
    nsTArray<MozPluginParameter>& attributes) {
  nsCOMPtr<nsIObjectLoadingContent> content = do_QueryReferent(mContent);
  nsObjectLoadingContent* loadingContent =
      static_cast<nsObjectLoadingContent*>(content.get());

  loadingContent->GetPluginAttributes(attributes);
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDOMElement(Element** result) {
  return CallQueryReferent(mContent.get(), result);
}

nsNPAPIPluginInstance* nsPluginInstanceOwner::GetInstance() {
  return mInstance;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetURL(
    const char* aURL, const char* aTarget, nsIInputStream* aPostStream,
    void* aHeadersData, uint32_t aHeadersDataLen, bool aDoCheckLoadURIChecks) {
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!content) {
    return NS_ERROR_NULL_POINTER;
  }

  if (content->IsEditable()) {
    return NS_OK;
  }

  Document* doc = content->GetComposedDoc();
  if (!doc) {
    return NS_ERROR_FAILURE;
  }

  nsPresContext* presContext = doc->GetPresContext();
  if (!presContext) {
    return NS_ERROR_FAILURE;
  }

  // the container of the pres context will give us the link handler
  nsCOMPtr<nsIDocShell> container = presContext->GetDocShell();
  NS_ENSURE_TRUE(container, NS_ERROR_FAILURE);

  nsAutoString unitarget;
  if ((0 == PL_strcmp(aTarget, "newwindow")) ||
      (0 == PL_strcmp(aTarget, "_new"))) {
    unitarget.AssignLiteral("_blank");
  } else if (0 == PL_strcmp(aTarget, "_current")) {
    unitarget.AssignLiteral("_self");
  } else {
    unitarget.AssignASCII(aTarget);  // XXX could this be nonascii?
  }

  // Create an absolute URL
  nsCOMPtr<nsIURI> uri;
  nsresult rv = NS_NewURI(getter_AddRefs(uri), aURL, GetBaseURI());
  NS_ENSURE_SUCCESS(rv, NS_ERROR_FAILURE);

  nsCOMPtr<nsIInputStream> headersDataStream;
  if (aPostStream && aHeadersData) {
    if (!aHeadersDataLen) return NS_ERROR_UNEXPECTED;

    nsCOMPtr<nsIStringInputStream> sis =
        do_CreateInstance("@mozilla.org/io/string-input-stream;1");
    if (!sis) return NS_ERROR_OUT_OF_MEMORY;

    rv = sis->SetData((char*)aHeadersData, aHeadersDataLen);
    NS_ENSURE_SUCCESS(rv, rv);
    headersDataStream = sis;
  }

  int32_t blockPopups =
      Preferences::GetInt("privacy.popups.disable_from_plugins");
  AutoPopupStatePusher popupStatePusher(
      (PopupBlocker::PopupControlState)blockPopups);

  // if security checks (in particular CheckLoadURIWithPrincipal) needs
  // to be skipped we are creating a contentPrincipal from the target URI
  // to make sure that security checks succeed.
  // Please note that we do not want to fall back to using the
  // systemPrincipal, because that would also bypass ContentPolicy checks
  // which should still be enforced.
  nsCOMPtr<nsIPrincipal> triggeringPrincipal;
  if (!aDoCheckLoadURIChecks) {
    mozilla::OriginAttributes attrs =
        BasePrincipal::Cast(content->NodePrincipal())->OriginAttributesRef();
    triggeringPrincipal = BasePrincipal::CreateContentPrincipal(uri, attrs);
  } else {
    bool useParentContentPrincipal = false;
    nsCOMPtr<nsINetUtil> netUtil = do_GetNetUtil();
    // For protocols loadable by anyone, it doesn't matter what principal
    // we use for the security check. However, for external URIs, we check
    // whether the browsing context in which they load can be accessed by
    // the triggering principal that is doing the loading, to avoid certain
    // types of spoofing attacks. In this case, the load would never be
    // allowed with the newly minted null principal, when all the plugin is
    // trying to do is load a URL in its own browsing context. So we use
    // the content principal of the plugin's node in this case.
    netUtil->ProtocolHasFlags(uri,
                              nsIProtocolHandler::URI_LOADABLE_BY_ANYONE |
                                  nsIProtocolHandler::URI_DOES_NOT_RETURN_DATA,
                              &useParentContentPrincipal);
    if (useParentContentPrincipal) {
      triggeringPrincipal = content->NodePrincipal();
    } else {
      triggeringPrincipal = NullPrincipal::CreateWithInheritedAttributes(
          content->NodePrincipal());
    }
  }

  nsCOMPtr<nsIContentSecurityPolicy> csp = content->GetCsp();

  rv = nsDocShell::Cast(container)->OnLinkClick(
      content, uri, unitarget, VoidString(), aPostStream, headersDataStream,
      /* isUserTriggered */ false, /* isTrusted */ true, triggeringPrincipal,
      csp);

  return rv;
}

NS_IMETHODIMP nsPluginInstanceOwner::GetDocument(Document** aDocument) {
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!aDocument || !content) {
    return NS_ERROR_NULL_POINTER;
  }

  // XXX sXBL/XBL2 issue: current doc or owner doc?
  // But keep in mind bug 322414 comment 33
  NS_ADDREF(*aDocument = content->OwnerDoc());
  return NS_OK;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRect(NPRect* invalidRect) {
  // If our object frame has gone away, we won't be able to determine
  // up-to-date-ness, so just fire off the event.
  if (mWaitingForPaint) {
    nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
    // We don't care when the event is dispatched as long as it's "soon",
    // since whoever needs it will be waiting for it.
    nsCOMPtr<nsIRunnable> event = new AsyncPaintWaitEvent(content, true);
    NS_DispatchToMainThread(event);
    mWaitingForPaint = false;
  }

  return NS_ERROR_FAILURE;
}

NS_IMETHODIMP nsPluginInstanceOwner::InvalidateRegion(NPRegion invalidRegion) {
  return NS_ERROR_NOT_IMPLEMENTED;
}

NS_IMETHODIMP
nsPluginInstanceOwner::RedrawPlugin() { return NS_OK; }

#if defined(XP_WIN)
nsIWidget* nsPluginInstanceOwner::GetContainingWidgetIfOffset() {
  return nullptr;
}

#endif

NS_IMETHODIMP nsPluginInstanceOwner::GetNetscapeWindow(void* value) {
  NS_WARNING("plugin owner has no owner in getting doc's window handle");
  return NS_ERROR_FAILURE;
}

#if defined(XP_WIN)
void nsPluginInstanceOwner::SetWidgetWindowAsParent(HWND aWindowToAdopt) {
  if (!mWidget) {
    NS_ERROR("mWidget should exist before this gets called.");
    return;
  }

  mWidget->SetNativeData(NS_NATIVE_CHILD_WINDOW,
                         reinterpret_cast<uintptr_t>(aWindowToAdopt));
}

nsresult nsPluginInstanceOwner::SetNetscapeWindowAsParent(HWND aWindowToAdopt) {
  NS_WARNING("Plugin owner has no plugin frame.");
  return NS_ERROR_FAILURE;
}

bool nsPluginInstanceOwner::GetCompositionString(uint32_t aType,
                                                 nsTArray<uint8_t>* aDist,
                                                 int32_t* aLength) {
  // Mark pkugin calls ImmGetCompositionStringW correctly
  mGotCompositionData = true;

  RefPtr<TextComposition> composition = GetTextComposition();
  if (NS_WARN_IF(!composition)) {
    return false;
  }

  switch (aType) {
    case GCS_COMPSTR: {
      if (!composition->IsComposing()) {
        *aLength = 0;
        return true;
      }

      uint32_t len = composition->LastData().Length() * sizeof(char16_t);
      if (len) {
        aDist->SetLength(len);
        memcpy(aDist->Elements(), composition->LastData().get(), len);
      }
      *aLength = len;
      return true;
    }

    case GCS_RESULTSTR: {
      if (composition->IsComposing()) {
        *aLength = 0;
        return true;
      }

      uint32_t len = composition->LastData().Length() * sizeof(char16_t);
      if (len) {
        aDist->SetLength(len);
        memcpy(aDist->Elements(), composition->LastData().get(), len);
      }
      *aLength = len;
      return true;
    }

    case GCS_CURSORPOS: {
      *aLength = 0;
      TextRangeArray* ranges = composition->GetLastRanges();
      if (!ranges) {
        return true;
      }
      *aLength = ranges->GetCaretPosition();
      if (*aLength < 0) {
        return false;
      }
      return true;
    }

    case GCS_COMPATTR: {
      TextRangeArray* ranges = composition->GetLastRanges();
      if (!ranges || ranges->IsEmpty()) {
        *aLength = 0;
        return true;
      }

      aDist->SetLength(composition->LastData().Length());
      memset(aDist->Elements(), ATTR_INPUT, aDist->Length());

      for (TextRange& range : *ranges) {
        uint8_t type = ATTR_INPUT;
        switch (range.mRangeType) {
          case TextRangeType::eRawClause:
            type = ATTR_INPUT;
            break;
          case TextRangeType::eSelectedRawClause:
            type = ATTR_TARGET_NOTCONVERTED;
            break;
          case TextRangeType::eConvertedClause:
            type = ATTR_CONVERTED;
            break;
          case TextRangeType::eSelectedClause:
            type = ATTR_TARGET_CONVERTED;
            break;
          default:
            continue;
        }

        size_t minLen = std::min<size_t>(range.mEndOffset, aDist->Length());
        for (size_t i = range.mStartOffset; i < minLen; i++) {
          (*aDist)[i] = type;
        }
      }
      *aLength = aDist->Length();
      return true;
    }

    case GCS_COMPCLAUSE: {
      RefPtr<TextRangeArray> ranges = composition->GetLastRanges();
      if (!ranges || ranges->IsEmpty()) {
        aDist->SetLength(sizeof(uint32_t));
        memset(aDist->Elements(), 0, sizeof(uint32_t));
        *aLength = aDist->Length();
        return true;
      }
      AutoTArray<uint32_t, 16> clauses;
      clauses.AppendElement(0);
      for (TextRange& range : *ranges) {
        if (!range.IsClause()) {
          continue;
        }
        clauses.AppendElement(range.mEndOffset);
      }

      aDist->SetLength(clauses.Length() * sizeof(uint32_t));
      memcpy(aDist->Elements(), clauses.Elements(), aDist->Length());
      *aLength = aDist->Length();
      return true;
    }

    case GCS_RESULTREADSTR: {
      // When returning error causes unexpected error, so we return 0 instead.
      *aLength = 0;
      return true;
    }

    case GCS_RESULTCLAUSE: {
      // When returning error causes unexpected error, so we return 0 instead.
      *aLength = 0;
      return true;
    }

    default:
      NS_WARNING(
          nsPrintfCString(
              "Unsupported type %x of ImmGetCompositionStringW hook", aType)
              .get());
      break;
  }

  return false;
}

bool nsPluginInstanceOwner::RequestCommitOrCancel(bool aCommitted) {
  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (NS_WARN_IF(!widget)) {
    return false;
  }

  // Retrieve TextComposition for the widget with IMEStateManager instead of
  // using GetTextComposition() because we cannot know whether the method
  // failed due to no widget or no composition.
  RefPtr<TextComposition> composition =
      IMEStateManager::GetTextCompositionFor(widget);
  if (!composition) {
    // If there is composition, we should just ignore this request since
    // the composition may have been committed after the plugin process
    // sent this request.
    return true;
  }

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (content != composition->GetEventTargetNode()) {
    // If the composition is handled in different node, that means that
    // the composition for the plugin has gone and new composition has
    // already started.  So, request from the plugin should be ignored
    // since user inputs different text now.
    return true;
  }

  // If active composition is being handled in the plugin, let's request to
  // commit/cancel the composition via both IMEStateManager and TextComposition
  // for avoid breaking the status management of composition.  I.e., don't
  // call nsIWidget::NotifyIME() directly from here.
  IMEStateManager::NotifyIME(aCommitted ? widget::REQUEST_TO_COMMIT_COMPOSITION
                                        : widget::REQUEST_TO_CANCEL_COMPOSITION,
                             widget, composition->GetBrowserParent());
  // FYI: This instance may have been destroyed.  Be careful if you need to
  //      access members of this class.
  return true;
}

#endif  // #ifdef XP_WIN

NS_IMETHODIMP nsPluginInstanceOwner::SetEventModel(int32_t eventModel) {
#ifdef XP_MACOSX
  mEventModel = static_cast<NPEventModel>(eventModel);
  return NS_OK;
#else
  return NS_ERROR_NOT_IMPLEMENTED;
#endif
}

NPBool nsPluginInstanceOwner::ConvertPoint(double sourceX, double sourceY,
                                           NPCoordinateSpace sourceSpace,
                                           double* destX, double* destY,
                                           NPCoordinateSpace destSpace) {
  return false;
}

NPError nsPluginInstanceOwner::InitAsyncSurface(NPSize* size,
                                                NPImageFormat format,
                                                void* initData,
                                                NPAsyncSurface* surface) {
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
}

NPError nsPluginInstanceOwner::FinalizeAsyncSurface(NPAsyncSurface*) {
  return NPERR_INCOMPATIBLE_VERSION_ERROR;
}

void nsPluginInstanceOwner::SetCurrentAsyncSurface(NPAsyncSurface*, NPRect*) {}

NS_IMETHODIMP nsPluginInstanceOwner::GetTagType(nsPluginTagType* result) {
  NS_ENSURE_ARG_POINTER(result);

  *result = nsPluginTagType_Unknown;

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (content->IsHTMLElement(nsGkAtoms::embed))
    *result = nsPluginTagType_Embed;
  else if (content->IsHTMLElement(nsGkAtoms::object))
    *result = nsPluginTagType_Object;

  return NS_OK;
}

void nsPluginInstanceOwner::GetParameters(
    nsTArray<MozPluginParameter>& parameters) {
  nsCOMPtr<nsIObjectLoadingContent> content = do_QueryReferent(mContent);
  nsObjectLoadingContent* loadingContent =
      static_cast<nsObjectLoadingContent*>(content.get());

  loadingContent->GetPluginParameters(parameters);
}

#ifdef XP_MACOSX

static void InitializeNPCocoaEvent(NPCocoaEvent* event) {
  memset(event, 0, sizeof(NPCocoaEvent));
}

NPDrawingModel nsPluginInstanceOwner::GetDrawingModel() {
#  ifndef NP_NO_QUICKDRAW
  // We don't support the Quickdraw drawing model any more but it's still
  // the default model for i386 per NPAPI.
  NPDrawingModel drawingModel = NPDrawingModelQuickDraw;
#  else
  NPDrawingModel drawingModel = NPDrawingModelCoreGraphics;
#  endif

  if (!mInstance) return drawingModel;

  mInstance->GetDrawingModel((int32_t*)&drawingModel);
  return drawingModel;
}

bool nsPluginInstanceOwner::IsRemoteDrawingCoreAnimation() {
  if (!mInstance) return false;

  bool coreAnimation;
  if (!NS_SUCCEEDED(mInstance->IsRemoteDrawingCoreAnimation(&coreAnimation)))
    return false;

  return coreAnimation;
}

NPEventModel nsPluginInstanceOwner::GetEventModel() { return mEventModel; }

#  define DEFAULT_REFRESH_RATE 20  // 50 FPS
StaticRefPtr<nsITimer> nsPluginInstanceOwner::sCATimer;
nsTArray<nsPluginInstanceOwner*>* nsPluginInstanceOwner::sCARefreshListeners =
    nullptr;

void nsPluginInstanceOwner::CARefresh(nsITimer* aTimer, void* aClosure) {
  if (!sCARefreshListeners) {
    return;
  }
  for (size_t i = 0; i < sCARefreshListeners->Length(); i++) {
    nsPluginInstanceOwner* instanceOwner = (*sCARefreshListeners)[i];
    NPWindow* window;
    instanceOwner->GetWindow(window);
    if (!window) {
      continue;
    }
    NPRect r;
    r.left = 0;
    r.top = 0;
    r.right = window->width;
    r.bottom = window->height;
    instanceOwner->InvalidateRect(&r);
  }
}

void nsPluginInstanceOwner::AddToCARefreshTimer() {
  if (!mInstance) {
    return;
  }

  // Flash invokes InvalidateRect for us.
  const char* mime = nullptr;
  if (NS_SUCCEEDED(mInstance->GetMIMEType(&mime)) && mime &&
      nsPluginHost::GetSpecialType(nsDependentCString(mime)) ==
          nsPluginHost::eSpecialType_Flash) {
    return;
  }

  if (!sCARefreshListeners) {
    sCARefreshListeners = new nsTArray<nsPluginInstanceOwner*>();
  }

  if (sCARefreshListeners->Contains(this)) {
    return;
  }

  sCARefreshListeners->AppendElement(this);

  if (sCARefreshListeners->Length() == 1) {
    nsCOMPtr<nsITimer> timer;
    NS_NewTimerWithFuncCallback(
        getter_AddRefs(timer), CARefresh, nullptr, DEFAULT_REFRESH_RATE,
        nsITimer::TYPE_REPEATING_SLACK, "nsPluginInstanceOwner::CARefresh");
    sCATimer = timer.forget();
  }
}

void nsPluginInstanceOwner::RemoveFromCARefreshTimer() {
  if (!sCARefreshListeners || sCARefreshListeners->Contains(this) == false) {
    return;
  }

  sCARefreshListeners->RemoveElement(this);

  if (sCARefreshListeners->Length() == 0) {
    if (sCATimer) {
      sCATimer->Cancel();
      sCATimer = nullptr;
    }
    delete sCARefreshListeners;
    sCARefreshListeners = nullptr;
  }
}

void nsPluginInstanceOwner::SetPluginPort() {
  void* pluginPort = GetPluginPort();
  if (!pluginPort || !mPluginWindow) return;
  mPluginWindow->window = pluginPort;
}
#endif
#if defined(XP_MACOSX) || defined(XP_WIN)
nsresult nsPluginInstanceOwner::ContentsScaleFactorChanged(
    double aContentsScaleFactor) {
  if (!mInstance) {
    return NS_ERROR_NULL_POINTER;
  }
  return mInstance->ContentsScaleFactorChanged(aContentsScaleFactor);
}
#endif

// static
uint32_t nsPluginInstanceOwner::GetEventloopNestingLevel() {
  nsCOMPtr<nsIAppShell> appShell = do_GetService(kAppShellCID);
  uint32_t currentLevel = 0;
  if (appShell) {
    appShell->GetEventloopNestingLevel(&currentLevel);
#ifdef XP_MACOSX
    // Cocoa widget code doesn't process UI events through the normal
    // appshell event loop, so it needs an additional count here.
    currentLevel++;
#endif
  }

  // No idea how this happens... but Linux doesn't consistently
  // process UI events through the appshell event loop. If we get a 0
  // here on any platform we increment the level just in case so that
  // we make sure we always tear the plugin down eventually.
  if (!currentLevel) {
    currentLevel++;
  }

  return currentLevel;
}

nsresult nsPluginInstanceOwner::DispatchFocusToPlugin(Event* aFocusEvent) {
#ifndef XP_MACOSX
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow)) {
    // continue only for cases without child window
    aFocusEvent->PreventDefault();  // consume event
    return NS_OK;
  }
#endif

  WidgetEvent* theEvent = aFocusEvent->WidgetEventPtr();
  if (theEvent) {
    WidgetGUIEvent focusEvent(theEvent->IsTrusted(), theEvent->mMessage,
                              nullptr);
    nsEventStatus rv = ProcessEvent(focusEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      aFocusEvent->PreventDefault();
      aFocusEvent->StopPropagation();
    }
  }

  return NS_OK;
}

nsresult nsPluginInstanceOwner::ProcessKeyPress(Event* aKeyEvent) {
  // ProcessKeyPress() may be called twice with same eKeyPress event.  One is
  // by the event listener in the default event group and the other is by the
  // event listener in the system event group.  When this is called in the
  // latter case and the event must be fired in the default event group too,
  // we don't need to do nothing anymore.
  // XXX Do we need to check whether the document is in chrome?  In strictly
  //     speaking, it must be yes.  However, our UI must not use plugin in
  //     chrome.
  if (!aKeyEvent->WidgetEventPtr()->mFlags.mOnlySystemGroupDispatchInContent &&
      aKeyEvent->WidgetEventPtr()->mFlags.mInSystemGroup) {
    return NS_OK;
  }

#ifdef XP_MACOSX
  return DispatchKeyToPlugin(aKeyEvent);
#else
  if (SendNativeEvents()) DispatchKeyToPlugin(aKeyEvent);

  if (mInstance) {
    // If this event is going to the plugin, we want to kill it.
    // Not actually sending keypress to the plugin, since we didn't before.
    aKeyEvent->PreventDefault();
    aKeyEvent->StopPropagation();
  }
  return NS_OK;
#endif
}

nsresult nsPluginInstanceOwner::DispatchKeyToPlugin(Event* aKeyEvent) {
#if !defined(XP_MACOSX)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow)) {
    aKeyEvent->PreventDefault();  // consume event
    return NS_OK;
  }
  // continue only for cases without child window
#endif

  if (mInstance) {
    WidgetKeyboardEvent* keyEvent =
        aKeyEvent->WidgetEventPtr()->AsKeyboardEvent();
    if (keyEvent && keyEvent->mClass == eKeyboardEventClass) {
      nsEventStatus rv = ProcessEvent(*keyEvent);
      if (nsEventStatus_eConsumeNoDefault == rv) {
        aKeyEvent->PreventDefault();
        aKeyEvent->StopPropagation();
      }
    }
  }

  return NS_OK;
}

nsresult nsPluginInstanceOwner::ProcessMouseDown(Event* aMouseEvent) {
  return NS_OK;
}

nsresult nsPluginInstanceOwner::DispatchMouseToPlugin(Event* aMouseEvent,
                                                      bool aAllowPropagate) {
#if !defined(XP_MACOSX)
  if (!mPluginWindow || (mPluginWindow->type == NPWindowTypeWindow)) {
    aMouseEvent->PreventDefault();  // consume event
    return NS_OK;
  }
  // continue only for cases without child window
#endif
  // don't send mouse events if we are hidden
  if (!mWidgetVisible) return NS_OK;

  WidgetMouseEvent* mouseEvent = aMouseEvent->WidgetEventPtr()->AsMouseEvent();
  if (mouseEvent && mouseEvent->mClass == eMouseEventClass) {
    nsEventStatus rv = ProcessEvent(*mouseEvent);
    if (nsEventStatus_eConsumeNoDefault == rv) {
      aMouseEvent->PreventDefault();
      if (!aAllowPropagate) {
        aMouseEvent->StopPropagation();
      }
    }
    if (mouseEvent->mMessage == eMouseUp) {
      mLastMouseDownButtonType = -1;
    }
  }
  return NS_OK;
}

#ifdef XP_WIN
already_AddRefed<TextComposition> nsPluginInstanceOwner::GetTextComposition() {
  return nullptr;
}

void nsPluginInstanceOwner::HandleNoConsumedCompositionMessage(
    WidgetCompositionEvent* aCompositionEvent, const NPEvent* aPluginEvent) {
  nsCOMPtr<nsIWidget> widget = GetContainingWidgetIfOffset();
  if (NS_WARN_IF(!widget)) {
    return;
  }

  if (aPluginEvent->lParam & GCS_RESULTSTR) {
    return;
  }
  if (!mSentStartComposition) {
    mSentStartComposition = true;
  }
}
#endif

nsresult nsPluginInstanceOwner::DispatchCompositionToPlugin(Event* aEvent) {
#ifdef XP_WIN
  if (!mPluginWindow) {
    // CompositionEvent isn't cancellable.  So it is unnecessary to call
    // PreventDefaults() to consume event
    return NS_OK;
  }
  WidgetCompositionEvent* compositionEvent =
      aEvent->WidgetEventPtr()->AsCompositionEvent();
  if (NS_WARN_IF(!compositionEvent)) {
    return NS_ERROR_INVALID_ARG;
  }

  if (compositionEvent->mMessage == eCompositionChange) {
    RefPtr<TextComposition> composition = GetTextComposition();
    if (NS_WARN_IF(!composition)) {
      return NS_ERROR_FAILURE;
    }
    TextComposition::CompositionChangeEventHandlingMarker
        compositionChangeEventHandlingMarker(composition, compositionEvent);
  }

  const NPEvent* pPluginEvent =
      static_cast<const NPEvent*>(compositionEvent->mPluginEvent);
  if (pPluginEvent && pPluginEvent->event == WM_IME_COMPOSITION &&
      mPluginDidNotHandleIMEComposition) {
    // This is a workaround when running windowed and windowless Flash on
    // same process.
    // Flash with protected mode calls IMM APIs on own render process.  This
    // is a bug of Flash's protected mode.
    // ImmGetCompositionString with GCS_RESULTSTR returns *LAST* committed
    // string.  So when windowed mode Flash handles IME composition,
    // windowless plugin can get windowed mode's commited string by that API.
    // So we never post WM_IME_COMPOSITION when plugin doesn't call
    // ImmGetCompositionString() during WM_IME_COMPOSITION correctly.
    HandleNoConsumedCompositionMessage(compositionEvent, pPluginEvent);
    aEvent->StopImmediatePropagation();
    return NS_OK;
  }

  // Protected mode Flash returns noDefault by NPP_HandleEvent, but
  // composition information into plugin is invalid because plugin's bug.
  // So if plugin doesn't get composition data by WM_IME_COMPOSITION, we
  // recongnize it isn't handled
  AutoRestore<bool> restore(mGotCompositionData);
  mGotCompositionData = false;

  nsEventStatus status = ProcessEvent(*compositionEvent);
  aEvent->StopImmediatePropagation();

  // Composition event isn't handled by plugin, so we have to call default proc.

  if (NS_WARN_IF(!pPluginEvent)) {
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_STARTCOMPOSITION) {
    if (nsEventStatus_eConsumeNoDefault != status) {
      mSentStartComposition = true;
    } else {
      mSentStartComposition = false;
    }
    mPluginDidNotHandleIMEComposition = false;
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_ENDCOMPOSITION) {
    return NS_OK;
  }

  if (pPluginEvent->event == WM_IME_COMPOSITION && !mGotCompositionData) {
    // If plugin doesn't handle WM_IME_COMPOSITION correctly, we don't send
    // composition event until end composition.
    mPluginDidNotHandleIMEComposition = true;

    HandleNoConsumedCompositionMessage(compositionEvent, pPluginEvent);
  }
#endif  // #ifdef XP_WIN
  return NS_OK;
}

nsresult nsPluginInstanceOwner::HandleEvent(Event* aEvent) {
  NS_ASSERTION(mInstance,
               "Should have a valid plugin instance or not receive events.");

  nsAutoString eventType;
  aEvent->GetType(eventType);

#ifdef XP_MACOSX
  if (eventType.EqualsLiteral("activate") ||
      eventType.EqualsLiteral("deactivate")) {
    WindowFocusMayHaveChanged();
    return NS_OK;
  }
  if (eventType.EqualsLiteral("MozPerformDelayedBlur")) {
    if (mShouldBlurOnActivate) {
      WidgetGUIEvent blurEvent(true, eBlur, nullptr);
      ProcessEvent(blurEvent);
      mShouldBlurOnActivate = false;
    }
    return NS_OK;
  }
#endif

  if (eventType.EqualsLiteral("focus")) {
    mContentFocused = true;
    return DispatchFocusToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("blur")) {
    mContentFocused = false;
    return DispatchFocusToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("mousedown")) {
    return ProcessMouseDown(aEvent);
  }
  if (eventType.EqualsLiteral("mouseup")) {
    return DispatchMouseToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("mousemove")) {
    return DispatchMouseToPlugin(aEvent, true);
  }
  if (eventType.EqualsLiteral("click") || eventType.EqualsLiteral("dblclick") ||
      eventType.EqualsLiteral("mouseover") ||
      eventType.EqualsLiteral("mouseout")) {
    return DispatchMouseToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("keydown") || eventType.EqualsLiteral("keyup")) {
    return DispatchKeyToPlugin(aEvent);
  }
  if (eventType.EqualsLiteral("keypress")) {
    return ProcessKeyPress(aEvent);
  }
  if (eventType.EqualsLiteral("compositionstart") ||
      eventType.EqualsLiteral("compositionend") ||
      eventType.EqualsLiteral("text")) {
    return DispatchCompositionToPlugin(aEvent);
  }

  DragEvent* dragEvent = aEvent->AsDragEvent();
  if (dragEvent && mInstance) {
    WidgetEvent* ievent = aEvent->WidgetEventPtr();
    if (ievent && ievent->IsTrusted() && ievent->mMessage != eDragEnter &&
        ievent->mMessage != eDragOver) {
      aEvent->PreventDefault();
    }

    // Let the plugin handle drag events.
    aEvent->StopPropagation();
  }
  return NS_OK;
}

#ifdef XP_MACOSX

void nsPluginInstanceOwner::PerformDelayedBlurs() {
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  nsCOMPtr<EventTarget> windowRoot =
      content->OwnerDoc()->GetWindow()->GetTopWindowRoot();
  nsContentUtils::DispatchEventOnlyToChrome(
      content->OwnerDoc(), windowRoot, u"MozPerformDelayedBlur"_ns,
      CanBubble::eNo, Cancelable::eNo, nullptr);
}

#endif

nsEventStatus nsPluginInstanceOwner::ProcessEvent(
    const WidgetGUIEvent& anEvent) {
  return nsEventStatus_eIgnore;
}

nsresult nsPluginInstanceOwner::Destroy() {
#ifdef XP_MACOSX
  RemoveFromCARefreshTimer();
#endif

  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);

  // unregister context menu listener
  if (mCXMenuListener) {
    mCXMenuListener->Destroy(content);
    mCXMenuListener = nullptr;
  }

  content->RemoveEventListener(u"focus"_ns, this, false);
  content->RemoveEventListener(u"blur"_ns, this, false);
  content->RemoveEventListener(u"mouseup"_ns, this, false);
  content->RemoveEventListener(u"mousedown"_ns, this, false);
  content->RemoveEventListener(u"mousemove"_ns, this, false);
  content->RemoveEventListener(u"click"_ns, this, false);
  content->RemoveEventListener(u"dblclick"_ns, this, false);
  content->RemoveEventListener(u"mouseover"_ns, this, false);
  content->RemoveEventListener(u"mouseout"_ns, this, false);
  content->RemoveEventListener(u"keypress"_ns, this, true);
  content->RemoveSystemEventListener(u"keypress"_ns, this, true);
  content->RemoveEventListener(u"keydown"_ns, this, true);
  content->RemoveEventListener(u"keyup"_ns, this, true);
  content->RemoveEventListener(u"drop"_ns, this, true);
  content->RemoveEventListener(u"drag"_ns, this, true);
  content->RemoveEventListener(u"dragenter"_ns, this, true);
  content->RemoveEventListener(u"dragover"_ns, this, true);
  content->RemoveEventListener(u"dragleave"_ns, this, true);
  content->RemoveEventListener(u"dragexit"_ns, this, true);
  content->RemoveEventListener(u"dragstart"_ns, this, true);
  content->RemoveEventListener(u"dragend"_ns, this, true);
  content->RemoveSystemEventListener(u"compositionstart"_ns, this, true);
  content->RemoveSystemEventListener(u"compositionend"_ns, this, true);
  content->RemoveSystemEventListener(u"text"_ns, this, true);

  if (mWidget) {
    if (mPluginWindow) {
      mPluginWindow->SetPluginWidget(nullptr);
    }

    mWidget->Destroy();
  }

  return NS_OK;
}

// Paints are handled differently, so we just simulate an update event.

#ifdef XP_MACOSX
void nsPluginInstanceOwner::Paint(const gfxRect& aDirtyRect,
                                  CGContextRef cgContext) {
  return;
}

void nsPluginInstanceOwner::DoCocoaEventDrawRect(const gfxRect& aDrawRect,
                                                 CGContextRef cgContext) {
  return;
}
#endif

#ifdef XP_WIN
void nsPluginInstanceOwner::Paint(const RECT& aDirty, HDC aDC) { return; }
#endif

#if defined(MOZ_X11)
void nsPluginInstanceOwner::Paint(gfxContext* aContext,
                                  const gfxRect& aFrameRect,
                                  const gfxRect& aDirtyRect) {
  return;
}
nsresult nsPluginInstanceOwner::Renderer::DrawWithXlib(
    cairo_surface_t* xsurface, nsIntPoint offset, nsIntRect* clipRects,
    uint32_t numClipRects) {
  Screen* screen = cairo_xlib_surface_get_screen(xsurface);
  Colormap colormap;
  Visual* visual;
  if (!gfxXlibSurface::GetColormapAndVisual(xsurface, &colormap, &visual)) {
    NS_ERROR("Failed to get visual and colormap");
    return NS_ERROR_UNEXPECTED;
  }

  nsNPAPIPluginInstance* instance = mInstanceOwner->mInstance;
  if (!instance) return NS_ERROR_FAILURE;

  // See if the plugin must be notified of new window parameters.
  bool doupdatewindow = false;

  if (mWindow->x != offset.x || mWindow->y != offset.y) {
    mWindow->x = offset.x;
    mWindow->y = offset.y;
    doupdatewindow = true;
  }

  if (nsIntSize(mWindow->width, mWindow->height) != mPluginSize) {
    mWindow->width = mPluginSize.width;
    mWindow->height = mPluginSize.height;
    doupdatewindow = true;
  }

  // The clip rect is relative to drawable top-left.
  NS_ASSERTION(numClipRects <= 1, "We don't support multiple clip rectangles!");
  nsIntRect clipRect;
  if (numClipRects) {
    clipRect.x = clipRects[0].x;
    clipRect.y = clipRects[0].y;
    clipRect.width = clipRects[0].width;
    clipRect.height = clipRects[0].height;
    // NPRect members are unsigned, but clip rectangles should be contained by
    // the surface.
    NS_ASSERTION(clipRect.x >= 0 && clipRect.y >= 0,
                 "Clip rectangle offsets are negative!");
  } else {
    clipRect.x = offset.x;
    clipRect.y = offset.y;
    clipRect.width = mWindow->width;
    clipRect.height = mWindow->height;
    // Don't ask the plugin to draw outside the drawable.
    // This also ensures that the unsigned clip rectangle offsets won't be -ve.
    clipRect.IntersectRect(
        clipRect, nsIntRect(0, 0, cairo_xlib_surface_get_width(xsurface),
                            cairo_xlib_surface_get_height(xsurface)));
  }

  NPRect newClipRect;
  newClipRect.left = clipRect.x;
  newClipRect.top = clipRect.y;
  newClipRect.right = clipRect.XMost();
  newClipRect.bottom = clipRect.YMost();
  if (mWindow->clipRect.left != newClipRect.left ||
      mWindow->clipRect.top != newClipRect.top ||
      mWindow->clipRect.right != newClipRect.right ||
      mWindow->clipRect.bottom != newClipRect.bottom) {
    mWindow->clipRect = newClipRect;
    doupdatewindow = true;
  }

  NPSetWindowCallbackStruct* ws_info =
      static_cast<NPSetWindowCallbackStruct*>(mWindow->ws_info);
#  ifdef MOZ_X11
  if (ws_info->visual != visual || ws_info->colormap != colormap) {
    ws_info->visual = visual;
    ws_info->colormap = colormap;
    ws_info->depth = gfxXlibSurface::DepthOfVisual(screen, visual);
    doupdatewindow = true;
  }
#  endif

  {
    if (doupdatewindow) instance->SetWindow(mWindow);
  }

  // Translate the dirty rect to drawable coordinates.
  nsIntRect dirtyRect = mDirtyRect + offset;
  if (mInstanceOwner->mFlash10Quirks) {
    // Work around a bug in Flash up to 10.1 d51 at least, where expose event
    // top left coordinates within the plugin-rect and not at the drawable
    // origin are misinterpreted.  (We can move the top left coordinate
    // provided it is within the clipRect.)
    dirtyRect.SetRect(offset.x, offset.y, mDirtyRect.XMost(),
                      mDirtyRect.YMost());
  }
  // Intersect the dirty rect with the clip rect to ensure that it lies within
  // the drawable.
  if (!dirtyRect.IntersectRect(dirtyRect, clipRect)) return NS_OK;

  {
    XEvent pluginEvent = XEvent();
    XGraphicsExposeEvent& exposeEvent = pluginEvent.xgraphicsexpose;
    // set the drawing info
    exposeEvent.type = GraphicsExpose;
    exposeEvent.display = DisplayOfScreen(screen);
    exposeEvent.drawable = cairo_xlib_surface_get_drawable(xsurface);
    exposeEvent.x = dirtyRect.x;
    exposeEvent.y = dirtyRect.y;
    exposeEvent.width = dirtyRect.width;
    exposeEvent.height = dirtyRect.height;
    exposeEvent.count = 0;
    // information not set:
    exposeEvent.serial = 0;
    exposeEvent.send_event = X11False;
    exposeEvent.major_code = 0;
    exposeEvent.minor_code = 0;

    instance->HandleEvent(&pluginEvent, nullptr);
  }
  return NS_OK;
}
#endif

nsresult nsPluginInstanceOwner::Init(nsIContent* aContent) {
  mLastEventloopNestingLevel = GetEventloopNestingLevel();

  mContent = do_GetWeakReference(aContent);

  MOZ_ASSERT_UNREACHABLE("Should not be initializing plugin without a frame");
  return NS_ERROR_FAILURE;
}

void* nsPluginInstanceOwner::GetPluginPort() {
  void* result = nullptr;
  return result;
}

void nsPluginInstanceOwner::ReleasePluginPort(void* pluginPort) {}

NS_IMETHODIMP nsPluginInstanceOwner::CreateWidget(void) {
  NS_ENSURE_TRUE(mPluginWindow, NS_ERROR_NULL_POINTER);

  // Can't call this twice!
  if (mWidget) {
    NS_WARNING("Trying to create a plugin widget twice!");
    return NS_ERROR_FAILURE;
  }

  bool windowless = false;
  mInstance->IsWindowless(&windowless);
  if (!windowless) {
#ifndef XP_WIN
    // Only Windows supports windowed mode!
    MOZ_ASSERT_UNREACHABLE();
    return NS_ERROR_FAILURE;
#else
    // Try to get a parent widget, on some platforms widget creation will fail
    // without a parent.
    nsresult rv = NS_ERROR_FAILURE;

    nsCOMPtr<nsIWidget> parentWidget;

    // A failure here is terminal since we can't fall back on the non-e10s code
    // path below.
    if (!mWidget && XRE_IsContentProcess()) {
      return NS_ERROR_UNEXPECTED;
    }

    if (!mWidget) {
      // native (single process)
      mWidget = nsIWidget::CreateChildWindow();
      nsWidgetInitData initData;
      initData.mWindowType = eWindowType_plugin;
      initData.clipChildren = true;
      initData.clipSiblings = true;
      rv = mWidget->Create(parentWidget.get(), nullptr,
                           LayoutDeviceIntRect(0, 0, 0, 0), &initData);
      if (NS_FAILED(rv)) {
        mWidget->Destroy();
        mWidget = nullptr;
        return rv;
      }
    }

    mWidget->EnableDragDrop(true);
    mWidget->Show(false);
    mWidget->Enable(false);
#endif  // XP_WIN
  }

  if (windowless) {
    mPluginWindow->type = NPWindowTypeDrawable;

    // this needs to be a HDC according to the spec, but I do
    // not see the right way to release it so let's postpone
    // passing HDC till paint event when it is really
    // needed. Change spec?
    mPluginWindow->window = nullptr;
#ifdef MOZ_X11
    // Fill in the display field.
    NPSetWindowCallbackStruct* ws_info =
        static_cast<NPSetWindowCallbackStruct*>(mPluginWindow->ws_info);
    ws_info->display = DefaultXDisplay();

    nsAutoCString description;
    GetPluginDescription(description);
    constexpr auto flash10Head = "Shockwave Flash 10."_ns;
    mFlash10Quirks = StringBeginsWith(description, flash10Head);
#endif
  } else if (mWidget) {
    // mPluginWindow->type is used in |GetPluginPort| so it must
    // be initialized first
    mPluginWindow->type = NPWindowTypeWindow;
    mPluginWindow->window = GetPluginPort();
    // tell the plugin window about the widget
    mPluginWindow->SetPluginWidget(mWidget);
  }

#ifdef XP_MACOSX
  if (GetDrawingModel() == NPDrawingModelCoreAnimation) {
    AddToCARefreshTimer();
  }
#endif

  mWidgetCreationComplete = true;

  return NS_OK;
}

// Mac specific code to fix up the port location and clipping region
#ifdef XP_MACOSX

void nsPluginInstanceOwner::FixUpPluginWindow(int32_t inPaintState) { return; }

void nsPluginInstanceOwner::WindowFocusMayHaveChanged() {
  if (!mSentInitialTopLevelWindowEvent) {
    return;
  }

  bool isActive = WindowIsActive();
  if (isActive != mLastWindowIsActive) {
    SendWindowFocusChanged(isActive);
    mLastWindowIsActive = isActive;
  }
}

bool nsPluginInstanceOwner::WindowIsActive() { return false; }

void nsPluginInstanceOwner::SendWindowFocusChanged(bool aIsActive) {
  if (!mInstance) {
    return;
  }

  NPCocoaEvent cocoaEvent;
  InitializeNPCocoaEvent(&cocoaEvent);
  cocoaEvent.type = NPCocoaEventWindowFocusChanged;
  cocoaEvent.data.focus.hasFocus = aIsActive;
  mInstance->HandleEvent(&cocoaEvent, nullptr,
                         NS_PLUGIN_CALL_SAFE_TO_REENTER_GECKO);
}

void nsPluginInstanceOwner::HidePluginWindow() {
  if (!mPluginWindow || !mInstance) {
    return;
  }

  mPluginWindow->clipRect.bottom = mPluginWindow->clipRect.top;
  mPluginWindow->clipRect.right = mPluginWindow->clipRect.left;
  mWidgetVisible = false;
  if (UseAsyncRendering()) {
    mInstance->AsyncSetWindow(mPluginWindow);
  } else {
    mInstance->SetWindow(mPluginWindow);
  }
}

#else   // XP_MACOSX

void nsPluginInstanceOwner::UpdateWindowPositionAndClipRect(bool aSetWindow) {
  return;
}

void nsPluginInstanceOwner::UpdateWindowVisibility(bool aVisible) {
  mPluginWindowVisible = aVisible;
  UpdateWindowPositionAndClipRect(true);
}
#endif  // XP_MACOSX

void nsPluginInstanceOwner::ResolutionMayHaveChanged() {
#if defined(XP_MACOSX) || defined(XP_WIN)
  double scaleFactor = 1.0;
  GetContentsScaleFactor(&scaleFactor);
  if (scaleFactor != mLastScaleFactor) {
    ContentsScaleFactorChanged(scaleFactor);
    mLastScaleFactor = scaleFactor;
  }
#endif
  float zoomFactor = 1.0;
  GetCSSZoomFactor(&zoomFactor);
  if (zoomFactor != mLastCSSZoomFactor) {
    if (mInstance) {
      mInstance->CSSZoomFactorChanged(zoomFactor);
    }
    mLastCSSZoomFactor = zoomFactor;
  }
}

void nsPluginInstanceOwner::UpdateDocumentActiveState(bool aIsActive) {
  AUTO_PROFILER_LABEL("nsPluginInstanceOwner::UpdateDocumentActiveState",
                      OTHER);

  mPluginDocumentActiveState = aIsActive;
#ifndef XP_MACOSX
  UpdateWindowPositionAndClipRect(true);

  // We don't have a connection to PluginWidgetParent in the chrome
  // process when dealing with tab visibility changes, so this needs
  // to be forwarded over after the active state is updated. If we
  // don't hide plugin widgets in hidden tabs, the native child window
  // in chrome will remain visible after a tab switch.
  if (mWidget && XRE_IsContentProcess()) {
    mWidget->Show(aIsActive);
    mWidget->Enable(aIsActive);
  }
#endif  // #ifndef XP_MACOSX
}

NS_IMETHODIMP
nsPluginInstanceOwner::CallSetWindow() {
  if (!mWidgetCreationComplete) {
    // No widget yet, we can't run this code
    return NS_OK;
  }
  if (mInstance) {
    if (UseAsyncRendering()) {
      mInstance->AsyncSetWindow(mPluginWindow);
    } else {
      mInstance->SetWindow(mPluginWindow);
    }
  }

  return NS_OK;
}

NS_IMETHODIMP
nsPluginInstanceOwner::GetContentsScaleFactor(double* result) {
  NS_ENSURE_ARG_POINTER(result);
  double scaleFactor = 1.0;
  // On Mac, device pixels need to be translated to (and from) "display pixels"
  // for plugins. On other platforms, plugin coordinates are always in device
  // pixels.
#if defined(XP_MACOSX) || defined(XP_WIN)
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  PresShell* presShell =
      nsContentUtils::FindPresShellForDocument(content->OwnerDoc());
  if (presShell) {
    scaleFactor = double(AppUnitsPerCSSPixel()) /
                  presShell->GetPresContext()
                      ->DeviceContext()
                      ->AppUnitsPerDevPixelAtUnitFullZoom();
  }
#endif
  *result = scaleFactor;
  return NS_OK;
}

void nsPluginInstanceOwner::GetCSSZoomFactor(float* result) {
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  PresShell* presShell =
      nsContentUtils::FindPresShellForDocument(content->OwnerDoc());
  if (presShell) {
    *result = presShell->GetPresContext()->DeviceContext()->GetFullZoom();
  } else {
    *result = 1.0;
  }
}

NS_IMETHODIMP nsPluginInstanceOwner::PrivateModeChanged(bool aEnabled) {
  return mInstance ? mInstance->PrivateModeStateChanged(aEnabled) : NS_OK;
}

nsIURI* nsPluginInstanceOwner::GetBaseURI() const {
  nsCOMPtr<nsIContent> content = do_QueryReferent(mContent);
  if (!content) {
    return nullptr;
  }
  return content->GetBaseURI();
}

// static
void nsPluginInstanceOwner::GeneratePluginEvent(
    const WidgetCompositionEvent* aSrcCompositionEvent,
    WidgetCompositionEvent* aDistCompositionEvent) {
#ifdef XP_WIN
  NPEvent newEvent;
  switch (aDistCompositionEvent->mMessage) {
    case eCompositionChange: {
      newEvent.event = WM_IME_COMPOSITION;
      newEvent.wParam = 0;
      if (aSrcCompositionEvent &&
          (aSrcCompositionEvent->mMessage == eCompositionCommit ||
           aSrcCompositionEvent->mMessage == eCompositionCommitAsIs)) {
        newEvent.lParam = GCS_RESULTSTR;
      } else {
        newEvent.lParam = GCS_COMPSTR | GCS_COMPATTR | GCS_COMPCLAUSE;
      }
      TextRangeArray* ranges = aDistCompositionEvent->mRanges;
      if (ranges && ranges->HasCaret()) {
        newEvent.lParam |= GCS_CURSORPOS;
      }
      break;
    }

    case eCompositionStart:
      newEvent.event = WM_IME_STARTCOMPOSITION;
      newEvent.wParam = 0;
      newEvent.lParam = 0;
      break;

    case eCompositionEnd:
      newEvent.event = WM_IME_ENDCOMPOSITION;
      newEvent.wParam = 0;
      newEvent.lParam = 0;
      break;

    default:
      return;
  }
  aDistCompositionEvent->mPluginEvent.Copy(newEvent);
#endif
}

// nsPluginDOMContextMenuListener class implementation

nsPluginDOMContextMenuListener::nsPluginDOMContextMenuListener(
    nsIContent* aContent) {
  aContent->AddEventListener(u"contextmenu"_ns, this, true);
}

nsPluginDOMContextMenuListener::~nsPluginDOMContextMenuListener() = default;

NS_IMPL_ISUPPORTS(nsPluginDOMContextMenuListener, nsIDOMEventListener)

NS_IMETHODIMP
nsPluginDOMContextMenuListener::HandleEvent(Event* aEvent) {
  aEvent->PreventDefault();  // consume event

  return NS_OK;
}

void nsPluginDOMContextMenuListener::Destroy(nsIContent* aContent) {
  // Unregister context menu listener
  aContent->RemoveEventListener(u"contextmenu"_ns, this, true);
}
