/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef mozilla_layers_APZCCallbackHelper_h
#define mozilla_layers_APZCCallbackHelper_h

#include "FrameMetrics.h"
#include "nsIDOMWindowUtils.h"

class nsIContent;
class nsIDocument;
template<class T> struct already_AddRefed;

namespace mozilla {
namespace layers {

/* This class contains some helper methods that facilitate implementing the
   GeckoContentController callback interface required by the AsyncPanZoomController.
   Since different platforms need to implement this interface in similar-but-
   not-quite-the-same ways, this utility class provides some helpful methods
   to hold code that can be shared across the different platform implementations.
 */
class APZCCallbackHelper
{
    typedef mozilla::layers::FrameMetrics FrameMetrics;
    typedef mozilla::layers::ScrollableLayerGuid ScrollableLayerGuid;

public:
    /* Checks to see if the pres shell that the given FrameMetrics object refers
       to is still the valid pres shell for the DOMWindowUtils. This can help
       guard against apply stale updates (updates meant for a pres shell that has
       since been torn down and replaced). */
    static bool HasValidPresShellId(nsIDOMWindowUtils* aUtils,
                                    const FrameMetrics& aMetrics);

    /* Applies the scroll and zoom parameters from the given FrameMetrics object to
       the root frame corresponding to the given DOMWindowUtils. If tiled thebes
       layers are enabled, this will align the displayport to tile boundaries.
       Setting the scroll position can cause some small adjustments to be made
       to the actual scroll position. aMetrics' display port and scroll position
       will be updated with any modifications made. */
    static void UpdateRootFrame(nsIDOMWindowUtils* aUtils,
                                FrameMetrics& aMetrics);

    /* Applies the scroll parameters from the given FrameMetrics object to the subframe
       corresponding to the given content object. If tiled thebes
       layers are enabled, this will align the displayport to tile boundaries.
       Setting the scroll position can cause some small adjustments to be made
       to the actual scroll position. aMetrics' display port and scroll position
       will be updated with any modifications made. */
    static void UpdateSubFrame(nsIContent* aContent,
                               FrameMetrics& aMetrics);

    /* Get the DOMWindowUtils for the window corresponding to the given document. */
    static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(const nsIDocument* aDoc);

    /* Get the DOMWindowUtils for the window corresponding to the givent content
       element. This might be an iframe inside the tab, for instance. */
    static already_AddRefed<nsIDOMWindowUtils> GetDOMWindowUtils(const nsIContent* aContent);

    /* Get the presShellId and view ID for the given content element.
     * If the view ID does not exist, one is created.
     * The pres shell ID should generally already exist; if it doesn't for some
     * reason, false is returned. */
    static bool GetOrCreateScrollIdentifiers(nsIContent* aContent,
                                             uint32_t* aPresShellIdOut,
                                             FrameMetrics::ViewID* aViewIdOut);

    /* Tell layout that we received the scroll offset update for the given view ID, so
       that it accepts future scroll offset updates from APZ. */
    static void AcknowledgeScrollUpdate(const FrameMetrics::ViewID& aScrollId,
                                        const uint32_t& aScrollGeneration);

    /* Save an "input transform" property on the content element corresponding to
       the scrollable content. This is needed because in some cases when the APZ code
       sends a paint request via the GeckoContentController interface, we don't always
       apply the scroll offset that was requested. Since the APZ code doesn't know
       that we didn't apply it, it will transform inputs assuming that we had applied
       it, and so we need to apply a fixup to the input to account for the fact that
       we didn't.
       The |aApzcMetrics| argument are the metrics that the APZ sent us, and the
       |aActualMetrics| argument are the metrics representing the gecko state after we
       applied some or all of the APZ metrics. */
    static void UpdateCallbackTransform(const FrameMetrics& aApzcMetrics,
                                        const FrameMetrics& aActualMetrics);

    /* Apply an "input transform" to the given |aInput| and return the transformed value.
       The input transform applied is the one for the content element corresponding to
       |aGuid|; this is populated in a previous call to UpdateCallbackTransform. See that
       method's documentations for details. */
    static CSSPoint ApplyCallbackTransform(const CSSPoint& aInput,
                                           const ScrollableLayerGuid& aGuid);

    /* Same as above, but operates on nsIntPoint that are assumed to be in LayoutDevice
       pixel space. Requires an additonal |aScale| parameter to convert between CSS and
       LayoutDevice space. */
    static nsIntPoint ApplyCallbackTransform(const nsIntPoint& aPoint,
                                             const ScrollableLayerGuid& aGuid,
                                             const CSSToLayoutDeviceScale& aScale);
};

}
}

#endif /* mozilla_layers_APZCCallbackHelper_h */
