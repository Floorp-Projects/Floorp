/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdarg.h>

#include "nsIServiceManager.h"

#include "nsIConsoleService.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIHTMLCollection.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "mozilla/dom/TabChild.h"
#include "nsIPrincipal.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "WebGL2Context.h"

#include "nsIScriptObjectPrincipal.h"
#include "nsIPermissionManager.h"
#include "nsIObserverService.h"
#include "mozilla/Services.h"
#include "mozIThirdPartyUtil.h"
#include "nsContentUtils.h"
#include "nsUnicharUtils.h"
#include "nsPrintfCString.h"
#include "nsIConsoleService.h"
#include "jsapi.h"

#define TOPIC_CANVAS_PERMISSIONS_PROMPT "canvas-permissions-prompt"
#define PERMISSION_CANVAS_EXTRACT_DATA "canvas"

using namespace mozilla::gfx;

namespace mozilla {
namespace CanvasUtils {

bool IsImageExtractionAllowed(nsIDocument *aDocument, JSContext *aCx)
{
    // Do the rest of the checks only if privacy.resistFingerprinting is on.
    if (!nsContentUtils::ShouldResistFingerprinting()) {
        return true;
    }

    // Don't proceed if we don't have a document or JavaScript context.
    if (!aDocument || !aCx) {
        return false;
    }

    // Documents with system principal can always extract canvas data.
    nsPIDOMWindowOuter *win = aDocument->GetWindow();
    nsCOMPtr<nsIScriptObjectPrincipal> sop(do_QueryInterface(win));
    if (sop && nsContentUtils::IsSystemPrincipal(sop->GetPrincipal())) {
        return true;
    }

    // Always give permission to chrome scripts (e.g. Page Inspector).
    if (nsContentUtils::ThreadsafeIsCallerChrome()) {
        return true;
    }

    // Get the document URI and its spec.
    nsIURI *docURI = aDocument->GetDocumentURI();
    nsCString docURISpec;
    docURI->GetSpec(docURISpec);

    // Allow local files to extract canvas data.
    bool isFileURL;
    (void) docURI->SchemeIs("file", &isFileURL);
    if (isFileURL) {
        return true;
    }

    // Get calling script file and line for logging.
    JS::AutoFilename scriptFile;
    unsigned scriptLine = 0;
    bool isScriptKnown = false;
    if (JS::DescribeScriptedCaller(aCx, &scriptFile, &scriptLine)) {
        isScriptKnown = true;
        // Don't show canvas prompt for PDF.js
        if (scriptFile.get() &&
                strcmp(scriptFile.get(), "resource://pdf.js/build/pdf.js") == 0) {
            return true;
        }
    }

    nsIDocument* topLevelDocument = aDocument->GetTopLevelContentDocument();
    nsIURI *topLevelDocURI = topLevelDocument ? topLevelDocument->GetDocumentURI() : nullptr;
    nsCString topLevelDocURISpec;
    if (topLevelDocURI) {
        topLevelDocURI->GetSpec(topLevelDocURISpec);
    }

    // Load Third Party Util service.
    nsresult rv;
    nsCOMPtr<mozIThirdPartyUtil> thirdPartyUtil =
        do_GetService(THIRDPARTYUTIL_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, false);

    // Block all third-party attempts to extract canvas.
    bool isThirdParty = true;
    rv = thirdPartyUtil->IsThirdPartyURI(topLevelDocURI, docURI, &isThirdParty);
    NS_ENSURE_SUCCESS(rv, false);
    if (isThirdParty) {
        nsAutoCString message;
        message.AppendPrintf("Blocked third party %s in page %s from extracting canvas data.",
                             docURISpec.get(), topLevelDocURISpec.get());
        if (isScriptKnown) {
            message.AppendPrintf(" %s:%u.", scriptFile.get(), scriptLine);
        }
        nsContentUtils::LogMessageToConsole(message.get());
        return false;
    }

    // Load Permission Manager service.
    nsCOMPtr<nsIPermissionManager> permissionManager =
        do_GetService(NS_PERMISSIONMANAGER_CONTRACTID, &rv);
    NS_ENSURE_SUCCESS(rv, false);

    // Check if the site has permission to extract canvas data.
    // Either permit or block extraction if a stored permission setting exists.
    uint32_t permission;
    rv = permissionManager->TestPermission(topLevelDocURI,
                                           PERMISSION_CANVAS_EXTRACT_DATA,
                                           &permission);
    NS_ENSURE_SUCCESS(rv, false);
    switch (permission) {
    case nsIPermissionManager::ALLOW_ACTION:
        return true;
    case nsIPermissionManager::DENY_ACTION:
        return false;
    default:
        break;
    }

    // At this point, permission is unknown (nsIPermissionManager::UNKNOWN_ACTION).
    nsAutoCString message;
    message.AppendPrintf("Blocked %s in page %s from extracting canvas data.",
                         docURISpec.get(), topLevelDocURISpec.get());
    if (isScriptKnown) {
        message.AppendPrintf(" %s:%u.", scriptFile.get(), scriptLine);
    }
    nsContentUtils::LogMessageToConsole(message.get());

    // Prompt the user (asynchronous).
    if (XRE_IsContentProcess()) {
        TabChild* tabChild = TabChild::GetFrom(win);
        if (tabChild) {
            tabChild->SendShowCanvasPermissionPrompt(topLevelDocURISpec);
        }
    } else {
        nsCOMPtr<nsIObserverService> obs = mozilla::services::GetObserverService();
        if (obs) {
            obs->NotifyObservers(win, TOPIC_CANVAS_PERMISSIONS_PROMPT,
                                 NS_ConvertUTF8toUTF16(topLevelDocURISpec).get());
        }
    }

    // We don't extract the image for now -- user may override at prompt.
    return false;
}

bool
GetCanvasContextType(const nsAString& str, dom::CanvasContextType* const out_type)
{
  if (str.EqualsLiteral("2d")) {
    *out_type = dom::CanvasContextType::Canvas2D;
    return true;
  }

  if (str.EqualsLiteral("webgl") ||
      str.EqualsLiteral("experimental-webgl"))
  {
    *out_type = dom::CanvasContextType::WebGL1;
    return true;
  }

  if (WebGL2Context::IsSupported()) {
    if (str.EqualsLiteral("webgl2")) {
      *out_type = dom::CanvasContextType::WebGL2;
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
void
DoDrawImageSecurityCheck(dom::HTMLCanvasElement *aCanvasElement,
                         nsIPrincipal *aPrincipal,
                         bool forceWriteOnly,
                         bool CORSUsed)
{
    // Callers should ensure that mCanvasElement is non-null before calling this
    if (!aCanvasElement) {
        NS_WARNING("DoDrawImageSecurityCheck called without canvas element!");
        return;
    }

    if (aCanvasElement->IsWriteOnly())
        return;

    // If we explicitly set WriteOnly just do it and get out
    if (forceWriteOnly) {
        aCanvasElement->SetWriteOnly();
        return;
    }

    // No need to do a security check if the image used CORS for the load
    if (CORSUsed)
        return;

    NS_PRECONDITION(aPrincipal, "Must have a principal here");

    if (aCanvasElement->NodePrincipal()->Subsumes(aPrincipal)) {
        // This canvas has access to that image anyway
        return;
    }

    aCanvasElement->SetWriteOnly();
}

bool
CoerceDouble(const JS::Value& v, double* d)
{
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

bool
HasDrawWindowPrivilege(JSContext* aCx, JSObject* /* unused */)
{
  return nsContentUtils::CallerHasPermission(aCx, nsGkAtoms::all_urlsPermission);
}

} // namespace CanvasUtils
} // namespace mozilla
