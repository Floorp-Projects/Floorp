/* -*- Mode: C++; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include <stdlib.h>
#include <stdarg.h>

#include "prprf.h"

#include "nsIServiceManager.h"

#include "nsIConsoleService.h"
#include "nsIDOMCanvasRenderingContext2D.h"
#include "nsICanvasRenderingContextInternal.h"
#include "nsIHTMLCollection.h"
#include "mozilla/dom/HTMLCanvasElement.h"
#include "nsIPrincipal.h"

#include "nsGfxCIID.h"

#include "nsTArray.h"

#include "CanvasUtils.h"
#include "mozilla/gfx/Matrix.h"
#include "WebGL2Context.h"

using namespace mozilla::gfx;

namespace mozilla {
namespace CanvasUtils {

bool
GetCanvasContextType(const nsAString& str, dom::CanvasContextType* const out_type)
{
  if (str.EqualsLiteral("2d")) {
    *out_type = dom::CanvasContextType::Canvas2D;
    return true;
  }

  if (str.EqualsLiteral("experimental-webgl")) {
    *out_type = dom::CanvasContextType::WebGL1;
    return true;
  }

#ifdef MOZ_WEBGL_CONFORMANT
  if (str.EqualsLiteral("webgl")) {
    /* WebGL 1.0, $2.1 "Context Creation":
     *   If the user agent supports both the webgl and experimental-webgl
     *   canvas context types, they shall be treated as aliases.
     */
    *out_type = dom::CanvasContextType::WebGL1;
    return true;
  }
#endif

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
CoerceDouble(JS::Value v, double* d)
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

} // namespace CanvasUtils
} // namespace mozilla
