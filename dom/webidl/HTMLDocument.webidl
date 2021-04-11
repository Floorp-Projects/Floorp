/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[LegacyOverrideBuiltIns,
 Exposed=Window,
 InstrumentedProps=(adoptedStyleSheets,
                    caretRangeFromPoint,
                    clear,
                    exitPictureInPicture,
                    featurePolicy,
                    onbeforecopy,
                    onbeforecut,
                    onbeforepaste,
                    oncancel,
                    onfreeze,
                    onmousewheel,
                    onresume,
                    onsearch,
                    onsecuritypolicyviolation,
                    onwebkitfullscreenchange,
                    onwebkitfullscreenerror,
                    pictureInPictureElement,
                    pictureInPictureEnabled,
                    registerElement,
                    wasDiscarded,
                    webkitCancelFullScreen,
                    webkitCurrentFullScreenElement,
                    webkitExitFullscreen,
                    webkitFullscreenElement,
                    webkitFullscreenEnabled,
                    webkitHidden,
                    webkitIsFullScreen,
                    webkitVisibilityState,
                    xmlEncoding,
                    xmlStandalone,
                    xmlVersion)]
interface HTMLDocument : Document {
  // DOM tree accessors
  [Throws]
  getter object (DOMString name);
};
