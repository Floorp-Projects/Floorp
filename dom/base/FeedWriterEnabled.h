/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "js/TypeDecls.h"
#include "nsGlobalWindow.h"
#include "nsIPrincipal.h"
#include "nsIURI.h"
#include "nsString.h"
#include "xpcpublic.h"

namespace mozilla {

struct FeedWriterEnabled {
  static bool IsEnabled(JSContext* cx, JSObject* aGlobal)
  {
    // Make sure the global is a window
    nsGlobalWindow* win = xpc::WindowGlobalOrNull(aGlobal);
    if (!win) {
      return false;
    }

    // Make sure that the principal is about:feeds.
    nsCOMPtr<nsIPrincipal> principal = win->GetPrincipal();
    NS_ENSURE_TRUE(principal, false);
    nsCOMPtr<nsIURI> uri;
    principal->GetURI(getter_AddRefs(uri));
    if (!uri) {
      return false;
    }

    // First check the scheme to avoid getting long specs in the common case.
    bool isAbout = false;
    uri->SchemeIs("about", &isAbout);
    if (!isAbout) {
      return false;
    }

    // Now check the spec itself
    nsAutoCString spec;
    uri->GetSpec(spec);
    return spec.EqualsLiteral("about:feeds");
  }
};

}
