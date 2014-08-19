/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim:set tw=80 expandtab softtabstop=2 ts=2 sw=2: */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "nsGenericHTMLFrameElement.h"

#include "mozilla/Preferences.h"
#include "mozilla/ErrorResult.h"
#include "GeckoProfiler.h"
#include "mozIApplication.h"
#include "nsAttrValueInlines.h"
#include "nsContentUtils.h"
#include "nsIAppsService.h"
#include "nsIDocShell.h"
#include "nsIDOMDocument.h"
#include "nsIFrame.h"
#include "nsIInterfaceRequestorUtils.h"
#include "nsIPermissionManager.h"
#include "nsIPresShell.h"
#include "nsIScrollable.h"
#include "nsPresContext.h"
#include "nsServiceManagerUtils.h"
#include "nsSubDocumentFrame.h"

using namespace mozilla;
using namespace mozilla::dom;

NS_IMPL_CYCLE_COLLECTION_CLASS(nsGenericHTMLFrameElement)

NS_IMPL_CYCLE_COLLECTION_TRAVERSE_BEGIN_INHERITED(nsGenericHTMLFrameElement,
                                                  nsGenericHTMLElement)
  NS_IMPL_CYCLE_COLLECTION_TRAVERSE(mFrameLoader)
NS_IMPL_CYCLE_COLLECTION_TRAVERSE_END

NS_IMPL_ADDREF_INHERITED(nsGenericHTMLFrameElement, nsGenericHTMLElement)
NS_IMPL_RELEASE_INHERITED(nsGenericHTMLFrameElement, nsGenericHTMLElement)

NS_INTERFACE_TABLE_HEAD_CYCLE_COLLECTION_INHERITED(nsGenericHTMLFrameElement)
  NS_INTERFACE_TABLE_INHERITED(nsGenericHTMLFrameElement,
                               nsIDOMMozBrowserFrame,
                               nsIMozBrowserFrame,
                               nsIFrameLoaderOwner)
NS_INTERFACE_TABLE_TAIL_INHERITING(nsGenericHTMLElement)
NS_IMPL_BOOL_ATTR(nsGenericHTMLFrameElement, Mozbrowser, mozbrowser)

int32_t
nsGenericHTMLFrameElement::TabIndexDefault()
{
  return 0;
}

nsresult
nsGenericHTMLFrameElement::CreateRemoteFrameLoader(nsITabParent* aTabParent)
{
  MOZ_ASSERT(!mFrameLoader);
  EnsureFrameLoader();
  NS_ENSURE_STATE(mFrameLoader);
  mFrameLoader->SetRemoteBrowser(aTabParent);

  if (nsSubDocumentFrame* subdocFrame = do_QueryFrame(GetPrimaryFrame())) {
    // The reflow for this element already happened while we were waiting
    // for the iframe creation. Therefore the subdoc frame didn't have a
    // frameloader when UpdatePositionAndSize was supposed to be called in
    // ReflowFinished, and we need to do it properly now.
    mFrameLoader->UpdatePositionAndSize(subdocFrame);
  }
  return NS_OK;
}

nsresult
nsGenericHTMLFrameElement::BindToTree(nsIDocument* aDocument,
                                      nsIContent* aParent,
                                      nsIContent* aBindingParent,
                                      bool aCompileEventHandlers)
{
  nsresult rv = nsGenericHTMLElement::BindToTree(aDocument, aParent,
                                                 aBindingParent,
                                                 aCompileEventHandlers);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aDocument) {
    NS_ASSERTION(!nsContentUtils::IsSafeToRunScript(),
                 "Missing a script blocker!");

    PROFILER_LABEL("nsGenericHTMLFrameElement", "BindToTree",
      js::ProfileEntry::Category::OTHER);

    // We're in a document now.  Kick off the frame load.
    LoadSrc();
  }

  // We're now in document and scripts may move us, so clear
  // the mNetworkCreated flag.
  mNetworkCreated = false;
  return rv;
}

void
nsGenericHTMLFrameElement::UnbindFromTree(bool aDeep, bool aNullParent)
{
  if (mFrameLoader) {
    // This iframe is being taken out of the document, destroy the
    // iframe's frame loader (doing that will tear down the window in
    // this iframe).
    // XXXbz we really want to only partially destroy the frame
    // loader... we don't want to tear down the docshell.  Food for
    // later bug.
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::UnbindFromTree(aDeep, aNullParent);
}

nsresult
nsGenericHTMLFrameElement::SetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                   nsIAtom* aPrefix, const nsAString& aValue,
                                   bool aNotify)
{
  nsresult rv = nsGenericHTMLElement::SetAttr(aNameSpaceID, aName, aPrefix,
                                              aValue, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::src &&
      (Tag() != nsGkAtoms::iframe || 
       !HasAttr(kNameSpaceID_None,nsGkAtoms::srcdoc))) {
    // Don't propagate error here. The attribute was successfully set, that's
    // what we should reflect.
    LoadSrc();
  } else if (aNameSpaceID == kNameSpaceID_None && aName == nsGkAtoms::name) {
    // Propagate "name" to the docshell to make browsing context names live,
    // per HTML5.
    nsIDocShell *docShell = mFrameLoader ? mFrameLoader->GetExistingDocShell()
                                         : nullptr;
    if (docShell) {
      docShell->SetName(aValue);
    }
  }

  return NS_OK;
}

nsresult
nsGenericHTMLFrameElement::UnsetAttr(int32_t aNameSpaceID, nsIAtom* aAttribute,
                                     bool aNotify)
{
  // Invoke on the superclass.
  nsresult rv = nsGenericHTMLElement::UnsetAttr(aNameSpaceID, aAttribute, aNotify);
  NS_ENSURE_SUCCESS(rv, rv);

  if (aNameSpaceID == kNameSpaceID_None && aAttribute == nsGkAtoms::name) {
    // Propagate "name" to the docshell to make browsing context names live,
    // per HTML5.
    nsIDocShell *docShell = mFrameLoader ? mFrameLoader->GetExistingDocShell()
                                         : nullptr;
    if (docShell) {
      docShell->SetName(EmptyString());
    }
  }

  return NS_OK;
}

/* static */ int32_t
nsGenericHTMLFrameElement::MapScrollingAttribute(const nsAttrValue* aValue)
{
  int32_t mappedValue = nsIScrollable::Scrollbar_Auto;
  if (aValue && aValue->Type() == nsAttrValue::eEnum) {
    switch (aValue->GetEnumValue()) {
      case NS_STYLE_FRAME_OFF:
      case NS_STYLE_FRAME_NOSCROLL:
      case NS_STYLE_FRAME_NO:
        mappedValue = nsIScrollable::Scrollbar_Never;
        break;
    }
  }
  return mappedValue;
}

/* virtual */ nsresult
nsGenericHTMLFrameElement::AfterSetAttr(int32_t aNameSpaceID, nsIAtom* aName,
                                        const nsAttrValue* aValue,
                                        bool aNotify)
{
  if (aName == nsGkAtoms::scrolling && aNameSpaceID == kNameSpaceID_None) {
    if (mFrameLoader) {
      nsIDocShell* docshell = mFrameLoader->GetExistingDocShell();
      nsCOMPtr<nsIScrollable> scrollable = do_QueryInterface(docshell);
      if (scrollable) {
        int32_t cur;
        scrollable->GetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X, &cur);
        int32_t val = MapScrollingAttribute(aValue);
        if (cur != val) {
          scrollable->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_X, val);
          scrollable->SetDefaultScrollbarPreferences(nsIScrollable::ScrollOrientation_Y, val);
          nsRefPtr<nsPresContext> presContext;
          docshell->GetPresContext(getter_AddRefs(presContext));
          nsIPresShell* shell = presContext ? presContext->GetPresShell() : nullptr;
          nsIFrame* rootScroll = shell ? shell->GetRootScrollFrame() : nullptr;
          if (rootScroll) {
            shell->FrameNeedsReflow(rootScroll, nsIPresShell::eStyleChange,
                                    NS_FRAME_IS_DIRTY);
          }
        }
      }
    }
  }

  return nsGenericHTMLElement::AfterSetAttr(aNameSpaceID, aName, aValue,
                                            aNotify);
}

void
nsGenericHTMLFrameElement::DestroyContent()
{
  if (mFrameLoader) {
    mFrameLoader->Destroy();
    mFrameLoader = nullptr;
  }

  nsGenericHTMLElement::DestroyContent();
}

nsresult
nsGenericHTMLFrameElement::CopyInnerTo(Element* aDest)
{
  nsresult rv = nsGenericHTMLElement::CopyInnerTo(aDest);
  NS_ENSURE_SUCCESS(rv, rv);

  nsIDocument* doc = aDest->OwnerDoc();
  if (doc->IsStaticDocument() && mFrameLoader) {
    nsGenericHTMLFrameElement* dest =
      static_cast<nsGenericHTMLFrameElement*>(aDest);
    nsFrameLoader* fl = nsFrameLoader::Create(dest, false);
    NS_ENSURE_STATE(fl);
    dest->mFrameLoader = fl;
    static_cast<nsFrameLoader*>(mFrameLoader.get())->CreateStaticClone(fl);
  }

  return rv;
}

bool
nsGenericHTMLFrameElement::IsHTMLFocusable(bool aWithMouse,
                                           bool *aIsFocusable,
                                           int32_t *aTabIndex)
{
  if (nsGenericHTMLElement::IsHTMLFocusable(aWithMouse, aIsFocusable, aTabIndex)) {
    return true;
  }

  *aIsFocusable = nsContentUtils::IsSubDocumentTabbable(this);

  if (!*aIsFocusable && aTabIndex) {
    *aTabIndex = -1;
  }

  return false;
}

bool
nsGenericHTMLFrameElement::BrowserFramesEnabled()
{
  static bool sMozBrowserFramesEnabled = false;
  static bool sBoolVarCacheInitialized = false;

  if (!sBoolVarCacheInitialized) {
    sBoolVarCacheInitialized = true;
    Preferences::AddBoolVarCache(&sMozBrowserFramesEnabled,
                                 "dom.mozBrowserFramesEnabled");
  }

  return sMozBrowserFramesEnabled;
}

/**
 * Return true if this frame element really is a mozbrowser or mozapp.  (It
 * needs to have the right attributes, and its creator must have the right
 * permissions.)
 */
/* [infallible] */ nsresult
nsGenericHTMLFrameElement::GetReallyIsBrowserOrApp(bool *aOut)
{
  *aOut = false;

  // Fail if browser frames are globally disabled.
  if (!nsGenericHTMLFrameElement::BrowserFramesEnabled()) {
    return NS_OK;
  }

  // Fail if this frame doesn't have the mozbrowser attribute.
  if (!GetBoolAttr(nsGkAtoms::mozbrowser)) {
    return NS_OK;
  }

  // Fail if the node principal isn't trusted.
  nsIPrincipal *principal = NodePrincipal();
  nsCOMPtr<nsIPermissionManager> permMgr =
    services::GetPermissionManager();
  NS_ENSURE_TRUE(permMgr, NS_OK);

  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permMgr->TestPermissionFromPrincipal(principal, "browser", &permission);
  NS_ENSURE_SUCCESS(rv, NS_OK);
  *aOut = permission == nsIPermissionManager::ALLOW_ACTION;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsGenericHTMLFrameElement::GetReallyIsApp(bool *aOut)
{
  nsAutoString manifestURL;
  GetAppManifestURL(manifestURL);

  *aOut = !manifestURL.IsEmpty();
  return NS_OK;
}

namespace {

bool WidgetsEnabled()
{
  static bool sMozWidgetsEnabled = false;
  static bool sBoolVarCacheInitialized = false;

  if (!sBoolVarCacheInitialized) {
    sBoolVarCacheInitialized = true;
    Preferences::AddBoolVarCache(&sMozWidgetsEnabled,
                                 "dom.enable_widgets");
  }

  return sMozWidgetsEnabled;
}

} // anonymous namespace

/* [infallible] */ NS_IMETHODIMP
nsGenericHTMLFrameElement::GetReallyIsWidget(bool *aOut)
{
  *aOut = false;
  if (!WidgetsEnabled()) {
    return NS_OK;
  }

  nsAutoString appManifestURL;
  GetManifestURLByType(nsGkAtoms::mozapp, appManifestURL);
  bool isApp = !appManifestURL.IsEmpty();

  nsAutoString widgetManifestURL;
  GetManifestURLByType(nsGkAtoms::mozwidget, widgetManifestURL);
  bool isWidget = !widgetManifestURL.IsEmpty();

  *aOut = isWidget && !isApp;
  return NS_OK;
}

/* [infallible] */ NS_IMETHODIMP
nsGenericHTMLFrameElement::GetIsExpectingSystemMessage(bool *aOut)
{
  *aOut = false;

  if (!nsIMozBrowserFrame::GetReallyIsApp()) {
    return NS_OK;
  }

  *aOut = HasAttr(kNameSpaceID_None, nsGkAtoms::expectingSystemMessage);
  return NS_OK;
}

/** Get manifest url of app or widget
 * @param AppType: nsGkAtoms::mozapp or nsGkAtoms::mozwidget
 */
void nsGenericHTMLFrameElement::GetManifestURLByType(nsIAtom *aAppType,
                                                     nsAString& aManifestURL)
{
  aManifestURL.Truncate();

  if (aAppType != nsGkAtoms::mozapp && aAppType != nsGkAtoms::mozwidget) {
    return;
  }

  nsAutoString manifestURL;
  GetAttr(kNameSpaceID_None, aAppType, manifestURL);
  if (manifestURL.IsEmpty()) {
    return;
  }

  // Check permission.
  nsCOMPtr<nsIPermissionManager> permMgr = services::GetPermissionManager();
  NS_ENSURE_TRUE_VOID(permMgr);
  nsIPrincipal *principal = NodePrincipal();
  const char* aPermissionType = (aAppType == nsGkAtoms::mozapp) ? "embed-apps"
                                                                : "embed-widgets";
  uint32_t permission = nsIPermissionManager::DENY_ACTION;
  nsresult rv = permMgr->TestPermissionFromPrincipal(principal,
                                                     aPermissionType,
                                                     &permission);
  NS_ENSURE_SUCCESS_VOID(rv);
  if (permission != nsIPermissionManager::ALLOW_ACTION) {
    return;
  }

  nsCOMPtr<nsIAppsService> appsService = do_GetService(APPS_SERVICE_CONTRACTID);
  NS_ENSURE_TRUE_VOID(appsService);

  nsCOMPtr<mozIApplication> app;
  appsService->GetAppByManifestURL(manifestURL, getter_AddRefs(app));

  if (!app) {
    return;
  }

  bool hasWidgetPage = false;
  nsAutoString src;
  if (aAppType == nsGkAtoms::mozwidget) {
    GetAttr(kNameSpaceID_None, nsGkAtoms::src, src);
    nsresult rv = app->HasWidgetPage(src, &hasWidgetPage);

    if (!NS_SUCCEEDED(rv) || !hasWidgetPage) {
      return;
    }
  }

  aManifestURL.Assign(manifestURL);
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::GetAppManifestURL(nsAString& aOut)
{
  aOut.Truncate();

  // At the moment, you can't be an app without being a browser.
  if (!nsIMozBrowserFrame::GetReallyIsBrowserOrApp()) {
    return NS_OK;
  }

  nsAutoString appManifestURL;
  nsAutoString widgetManifestURL;

  GetManifestURLByType(nsGkAtoms::mozapp, appManifestURL);

  if (WidgetsEnabled()) {
    GetManifestURLByType(nsGkAtoms::mozwidget, widgetManifestURL);
  }

  bool isApp = !appManifestURL.IsEmpty();
  bool isWidget = !widgetManifestURL.IsEmpty();

  if (!isApp && !isWidget) {
    // No valid case to get manifest
    return NS_OK;
  }

  if (isApp && isWidget) {
    NS_WARNING("Can not simultaneously be mozapp and mozwidget");
    return NS_OK;
  }

  nsAutoString manifestURL;
  if (isApp) {
    manifestURL.Assign(appManifestURL);
  } else if (isWidget) {
    manifestURL.Assign(widgetManifestURL);
  }

  aOut.Assign(manifestURL);

  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::DisallowCreateFrameLoader()
{
  MOZ_ASSERT(!mFrameLoader);
  MOZ_ASSERT(!mFrameLoaderCreationDisallowed);
  mFrameLoaderCreationDisallowed = true;
  return NS_OK;
}

NS_IMETHODIMP
nsGenericHTMLFrameElement::AllowCreateFrameLoader()
{
  MOZ_ASSERT(!mFrameLoader);
  MOZ_ASSERT(mFrameLoaderCreationDisallowed);
  mFrameLoaderCreationDisallowed = false;
  return NS_OK;
}
