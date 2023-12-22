/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdarg.h>

#include "nsICanvasRenderingContextInternal.h"
#include "nsIHTMLCollection.h"
#include "mozilla/dom/BrowserChild.h"
#include "mozilla/dom/Document.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/OffscreenCanvas.h"
#include "mozilla/dom/UserActivation.h"
#include "mozilla/dom/WorkerCommon.h"
#include "mozilla/dom/WorkerPrivate.h"
#include "mozilla/gfx/gfxVars.h"
#include "mozilla/BasePrincipal.h"
#include "mozilla/StaticPrefs_gfx.h"
#include "mozilla/StaticPrefs_privacy.h"
#include "mozilla/StaticPrefs_webgl.h"
#include "nsIPrincipal.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "WebGL2Context.h"

#include "nsIScriptError.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "jsapi.h"

#define TOPIC_CANVAS_PERMISSIONS_PROMPT "canvas-permissions-prompt"
#define TOPIC_CANVAS_PERMISSIONS_PROMPT_HIDE_DOORHANGER \
  "canvas-permissions-prompt-hide-doorhanger"
#define PERMISSION_CANVAS_EXTRACT_DATA "canvas"_ns

using namespace mozilla::gfx;

static bool IsUnrestrictedPrincipal(nsIPrincipal& aPrincipal) {
  // The system principal can always extract canvas data.
  if (aPrincipal.IsSystemPrincipal()) {
    return true;
  }

  // Allow chrome: and resource: (this especially includes PDF.js)
  if (aPrincipal.SchemeIs("chrome") || aPrincipal.SchemeIs("resource")) {
    return true;
  }

  // Allow extension principals.
  return aPrincipal.GetIsAddonOrExpandedAddonPrincipal();
}

namespace mozilla::CanvasUtils {

bool IsImageExtractionAllowed(dom::Document* aDocument, JSContext* aCx,
                              nsIPrincipal& aPrincipal) {
  if (NS_WARN_IF(!aDocument)) {
    return false;
  }

  /*
   * There are three RFPTargets that change the behavior here, and they can be
   * in any combination
   * - CanvasImageExtractionPrompt - whether or not to prompt the user for
   * canvas extraction. If enabled, before canvas is extracted we will ensure
   * the user has granted permission.
   * - CanvasExtractionBeforeUserInputIsBlocked - if enabled, canvas extraction
   * before user input has occurred is always blocked, regardless of any other
   * Target behavior
   * - CanvasExtractionFromThirdPartiesIsBlocked - if enabled, canvas extraction
   * by third parties is always blocked, regardless of any other Target behavior
   *
   * There are two odd cases:
   * 1) When CanvasImageExtractionPrompt=false but
   *    CanvasExtractionBeforeUserInputIsBlocked=true Conceptually this is
   *    "Always allow canvas extraction in response to user input, and never
   *     allow it otherwise"
   *
   *    That's fine as a concept, but it might be a little confusing, so we
   *    still want to show the permission icon in the address bar, but never
   *    the permission doorhanger.
   * 2) When CanvasExtractionFromThirdPartiesIsBlocked=false - we will prompt
   *    the user for permission _for the frame_ (maybe with the doorhanger,
   *    maybe not).  The prompt shows the frame's origin, but it's easy to
   *    mistake that for the origin of the top-level page and grant it when you
   *    don't mean to.  This combination isn't likely to be used by anyone
   *    except those opting in, so that's alright.
   */

  // We can improve this mechanism when we have this implemented as a bitset
  if (!aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasImageExtractionPrompt) &&
      !aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked) &&
      !aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionFromThirdPartiesIsBlocked)) {
    return true;
  }

  // -------------------------------------------------------------------
  // General Exemptions

  // Don't proceed if we don't have a document or JavaScript context.
  if (!aCx) {
    return false;
  }

  // The system and extension principals can always extract canvas data.
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return true;
  }

  // Get the document URI and its spec.
  nsIURI* docURI = aDocument->GetDocumentURI();
  nsCString docURISpec;
  docURI->GetSpec(docURISpec);

  // Allow local files to extract canvas data.
  if (docURI->SchemeIs("file")) {
    return true;
  }

  // -------------------------------------------------------------------
  // Possibly block third parties

  if (aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionFromThirdPartiesIsBlocked)) {
    MOZ_ASSERT(aDocument->GetWindowContext());
    bool isThirdParty =
        aDocument->GetWindowContext()
            ? aDocument->GetWindowContext()->GetIsThirdPartyWindow()
            : false;
    if (isThirdParty) {
      nsAutoString message;
      message.AppendPrintf(
          "Blocked third party %s from extracting canvas data.",
          docURISpec.get());
      nsContentUtils::ReportToConsoleNonLocalized(
          message, nsIScriptError::warningFlag, "Security"_ns, aDocument);
      return false;
    }
  }

  // -------------------------------------------------------------------
  // Check if we will do any further blocking

  if (!aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasImageExtractionPrompt) &&
      !aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked)) {
    return true;
  }

  // -------------------------------------------------------------------
  // Check a site's permission

  // If the user has previously granted or not granted permission, we can return
  // immediately. Load Permission Manager service.
  nsresult rv;
  nsCOMPtr<nsIPermissionManager> permissionManager =
      do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
  NS_ENSURE_SUCCESS(rv, false);

  // Check if the site has permission to extract canvas data.
  // Either permit or block extraction if a stored permission setting exists.
  uint32_t permission;
  rv = permissionManager->TestPermissionFromPrincipal(
      &aPrincipal, PERMISSION_CANVAS_EXTRACT_DATA, &permission);
  NS_ENSURE_SUCCESS(rv, false);
  switch (permission) {
    case nsIPermissionManager::ALLOW_ACTION:
      return true;
    case nsIPermissionManager::DENY_ACTION:
      return false;
    default:
      break;
  }

  // -------------------------------------------------------------------
  // At this point, there's only one way to return true: if we are always
  // allowing canvas in response to user input, and not prompting
  bool hidePermissionDoorhanger = false;
  if (!aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasImageExtractionPrompt) &&
      StaticPrefs::
          privacy_resistFingerprinting_autoDeclineNoUserInputCanvasPrompts() &&
      aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked)) {
    // If so, see if this is in response to user input.
    if (dom::UserActivation::IsHandlingUserInput()) {
      return true;
    }

    hidePermissionDoorhanger = true;
  }

  // -------------------------------------------------------------------
  // Now we know we're going to block it, and log something to the console,
  // and show some sort of prompt maybe with the doorhanger, maybe not

  hidePermissionDoorhanger |=
      StaticPrefs::
          privacy_resistFingerprinting_autoDeclineNoUserInputCanvasPrompts() &&
      aDocument->ShouldResistFingerprinting(
          RFPTarget::CanvasExtractionBeforeUserInputIsBlocked) &&
      !dom::UserActivation::IsHandlingUserInput();

  if (hidePermissionDoorhanger) {
    nsAutoString message;
    message.AppendPrintf(
        "Blocked %s from extracting canvas data because no user input was "
        "detected.",
        docURISpec.get());
    nsContentUtils::ReportToConsoleNonLocalized(
        message, nsIScriptError::warningFlag, "Security"_ns, aDocument);
  } else {
    // It was in response to user input, so log and display the prompt.
    nsAutoString message;
    message.AppendPrintf(
        "Blocked %s from extracting canvas data, but prompting the user.",
        docURISpec.get());
    nsContentUtils::ReportToConsoleNonLocalized(
        message, nsIScriptError::warningFlag, "Security"_ns, aDocument);
  }

  // Show the prompt to the user (asynchronous) - maybe with the doorhanger,
  // maybe not
  nsPIDOMWindowOuter* win = aDocument->GetWindow();
  nsAutoCString origin;
  rv = aPrincipal.GetOrigin(origin);
  NS_ENSURE_SUCCESS(rv, false);

  if (XRE_IsContentProcess()) {
    dom::BrowserChild* browserChild = dom::BrowserChild::GetFrom(win);
    if (browserChild) {
      browserChild->SendShowCanvasPermissionPrompt(origin,
                                                   hidePermissionDoorhanger);
    }
  } else {
    nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
    if (obs) {
      obs->NotifyObservers(win,
                           hidePermissionDoorhanger
                               ? TOPIC_CANVAS_PERMISSIONS_PROMPT_HIDE_DOORHANGER
                               : TOPIC_CANVAS_PERMISSIONS_PROMPT,
                           NS_ConvertUTF8toUTF16(origin).get());
    }
  }

  // We don't extract the image for now -- user may override at prompt.
  return false;
}

ImageExtraction ImageExtractionResult(dom::HTMLCanvasElement* aCanvasElement,
                                      JSContext* aCx,
                                      nsIPrincipal& aPrincipal) {
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return ImageExtraction::Unrestricted;
  }

  nsCOMPtr<dom::Document> ownerDoc = aCanvasElement->OwnerDoc();
  if (!IsImageExtractionAllowed(ownerDoc, aCx, aPrincipal)) {
    return ImageExtraction::Placeholder;
  }

  if (ownerDoc->ShouldResistFingerprinting(RFPTarget::CanvasRandomization)) {
    return ImageExtraction::Randomize;
  }

  return ImageExtraction::Unrestricted;
}

ImageExtraction ImageExtractionResult(dom::OffscreenCanvas* aOffscreenCanvas,
                                      JSContext* aCx,
                                      nsIPrincipal& aPrincipal) {
  if (IsUnrestrictedPrincipal(aPrincipal)) {
    return ImageExtraction::Unrestricted;
  }

  if (aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasImageExtractionPrompt)) {
    return ImageExtraction::Placeholder;
  }

  if (aOffscreenCanvas->ShouldResistFingerprinting(
          RFPTarget::CanvasRandomization)) {
    return ImageExtraction::Randomize;
  }

  return ImageExtraction::Unrestricted;
}

bool GetCanvasContextType(const nsAString& str,
                          dom::CanvasContextType* const out_type) {
  if (str.EqualsLiteral("2d")) {
    *out_type = dom::CanvasContextType::Canvas2D;
    return true;
  }

  if (str.EqualsLiteral("webgl") || str.EqualsLiteral("experimental-webgl")) {
    *out_type = dom::CanvasContextType::WebGL1;
    return true;
  }

  if (StaticPrefs::webgl_enable_webgl2()) {
    if (str.EqualsLiteral("webgl2")) {
      *out_type = dom::CanvasContextType::WebGL2;
      return true;
    }
  }

  if (gfxVars::AllowWebGPU()) {
    if (str.EqualsLiteral("webgpu")) {
      *out_type = dom::CanvasContextType::WebGPU;
      return true;
    }
  }

  if (str.EqualsLiteral("bitmaprenderer")) {
    *out_type = dom::CanvasContextType::ImageBitmap;
    return true;
  }

  return false;
}

/**
 * This security check utility might be called from an source that never taints
 * others. For example, while painting a CanvasPattern, which is created from an
 * ImageBitmap, onto a canvas. In this case, the caller could set the CORSUsed
 * true in order to pass this check and leave the aPrincipal to be a nullptr
 * since the aPrincipal is not going to be used.
 */
void DoDrawImageSecurityCheck(dom::HTMLCanvasElement* aCanvasElement,
                              nsIPrincipal* aPrincipal, bool forceWriteOnly,
                              bool CORSUsed) {
  // Callers should ensure that mCanvasElement is non-null before calling this
  if (!aCanvasElement) {
    NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
    return;
  }

  if (aCanvasElement->IsWriteOnly() && !aCanvasElement->mExpandedReader) {
    return;
  }

  // If we explicitly set WriteOnly just do it and get out
  if (forceWriteOnly) {
    aCanvasElement->SetWriteOnly();
    return;
  }

  // No need to do a security check if the image used CORS for the load
  if (CORSUsed) return;

  if (NS_WARN_IF(!aPrincipal)) {
    MOZ_ASSERT_UNREACHABLE("Must have a principal here");
    aCanvasElement->SetWriteOnly();
    return;
  }

  if (aCanvasElement->NodePrincipal()->Subsumes(aPrincipal)) {
    // This canvas has access to that image anyway
    return;
  }

  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    // This is a resource from an extension content script principal.

    if (aCanvasElement->mExpandedReader &&
        aCanvasElement->mExpandedReader->Subsumes(aPrincipal)) {
      // This canvas already allows reading from this principal.
      return;
    }

    if (!aCanvasElement->mExpandedReader) {
      // Allow future reads from this same princial only.
      aCanvasElement->SetWriteOnly(aPrincipal);
      return;
    }

    // If we got here, this must be the *second* extension tainting
    // the canvas.  Fall through to mark it WriteOnly for everyone.
  }

  aCanvasElement->SetWriteOnly();
}

/**
 * This security check utility might be called from an source that never taints
 * others. For example, while painting a CanvasPattern, which is created from an
 * ImageBitmap, onto a canvas. In this case, the caller could set the aCORSUsed
 * true in order to pass this check and leave the aPrincipal to be a nullptr
 * since the aPrincipal is not going to be used.
 */
void DoDrawImageSecurityCheck(dom::OffscreenCanvas* aOffscreenCanvas,
                              nsIPrincipal* aPrincipal, bool aForceWriteOnly,
                              bool aCORSUsed) {
  // Callers should ensure that mCanvasElement is non-null before calling this
  if (NS_WARN_IF(!aOffscreenCanvas)) {
    return;
  }

  nsIPrincipal* expandedReader = aOffscreenCanvas->GetExpandedReader();
  if (aOffscreenCanvas->IsWriteOnly() && !expandedReader) {
    return;
  }

  // If we explicitly set WriteOnly just do it and get out
  if (aForceWriteOnly) {
    aOffscreenCanvas->SetWriteOnly();
    return;
  }

  // No need to do a security check if the image used CORS for the load
  if (aCORSUsed) {
    return;
  }

  // If we are on a worker thread, we might not have any principals at all.
  nsIGlobalObject* global = aOffscreenCanvas->GetOwnerGlobal();
  nsIPrincipal* canvasPrincipal = global ? global->PrincipalOrNull() : nullptr;
  if (!aPrincipal || !canvasPrincipal) {
    aOffscreenCanvas->SetWriteOnly();
    return;
  }

  if (canvasPrincipal->Subsumes(aPrincipal)) {
    // This canvas has access to that image anyway
    return;
  }

  if (BasePrincipal::Cast(aPrincipal)->AddonPolicy()) {
    // This is a resource from an extension content script principal.

    if (expandedReader && expandedReader->Subsumes(aPrincipal)) {
      // This canvas already allows reading from this principal.
      return;
    }

    if (!expandedReader) {
      // Allow future reads from this same princial only.
      aOffscreenCanvas->SetWriteOnly(aPrincipal);
      return;
    }

    // If we got here, this must be the *second* extension tainting
    // the canvas.  Fall through to mark it WriteOnly for everyone.
  }

  aOffscreenCanvas->SetWriteOnly();
}

bool CoerceDouble(const JS::Value& v, double* d) {
  if (v.isDouble()) {
    *d = v.toDouble();
  } else if (v.isInt32()) {
    *d = double(v.toInt32());
  } else if (v.isUndefined()) {
    *d = 0.0;
  } else {
    return false;
  }
  return true;
}

bool HasDrawWindowPrivilege(JSContext* aCx, JSObject* /* unused */) {
  return nsContentUtils::CallerHasPermission(aCx,
                                             nsGkAtoms::all_urlsPermission);
}

bool CheckWriteOnlySecurity(bool aCORSUsed, nsIPrincipal* aPrincipal,
                            bool aHadCrossOriginRedirects) {
  if (!aPrincipal) {
    return true;
  }

  if (!aCORSUsed) {
    if (aHadCrossOriginRedirects) {
      return true;
    }

    nsIGlobalObject* incumbentSettingsObject = dom::GetIncumbentGlobal();
    if (!incumbentSettingsObject) {
      return true;
    }

    nsIPrincipal* principal = incumbentSettingsObject->PrincipalOrNull();
    if (NS_WARN_IF(!principal) || !(principal->Subsumes(aPrincipal))) {
      return true;
    }
  }

  return false;
}

}  // namespace mozilla::CanvasUtils
