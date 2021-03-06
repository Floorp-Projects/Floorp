/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*-
 * vim: sw=2 ts=8 et :
 */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#include "X11Util.h"
#include "nsDebug.h"          // for NS_ASSERTION, etc
#include "MainThreadUtils.h"  // for NS_IsMainThread

namespace mozilla {

void FindVisualAndDepth(Display* aDisplay, VisualID aVisualID, Visual** aVisual,
                        int* aDepth) {
  const Screen* screen = DefaultScreenOfDisplay(aDisplay);

  for (int d = 0; d < screen->ndepths; d++) {
    Depth* d_info = &screen->depths[d];
    for (int v = 0; v < d_info->nvisuals; v++) {
      Visual* visual = &d_info->visuals[v];
      if (visual->visualid == aVisualID) {
        *aVisual = visual;
        *aDepth = d_info->depth;
        return;
      }
    }
  }

  NS_ASSERTION(aVisualID == X11None, "VisualID not on Screen.");
  *aVisual = nullptr;
  *aDepth = 0;
}

void FinishX(Display* aDisplay) {
  unsigned long lastRequest = NextRequest(aDisplay) - 1;
  if (lastRequest == LastKnownRequestProcessed(aDisplay)) return;

  XSync(aDisplay, X11False);
}

}  // namespace mozilla
