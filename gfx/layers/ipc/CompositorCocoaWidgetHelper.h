/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* vim: set sw=4 ts=8 et tw=80 : */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_CompositorCocoaWidgetHelper_h
#define mozilla_layers_CompositorCocoaWidgetHelper_h

// Note we can't include IPDL-generated headers here, since this file is being
// used as a workaround for Bug 719036.

namespace mozilla {
namespace layers {

class CompositorParent;
class LayerManager;

namespace compositor {

// Needed when we cannot directly include CompositorParent.h since it includes
// an IPDL-generated header (e.g. in widget/cocoa/nsChildView.mm; see Bug 719036).
LayerManager* GetLayerManager(CompositorParent* aParent);

}
}
}
#endif // mozilla_layers_CompositorCocoaWidgetHelper_h

