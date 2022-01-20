"use strict";

/**
 * Bug 1746883 - disable OffscreenCanvas for Zoom
 *
 * When OffscreenCanvas is enabled, Zoom breaks due to Canvas2D not being
 * supported yet. As such, we disable OffscreenCanvas for now on Zoom.
 */

if (window.OffscreenCanvas) {
  console.info(
    "OffscreenCanvas has been disabled for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1746883 for details."
  );

  Object.defineProperty(window.wrappedJSObject, "OffscreenCanvas", {
    get: undefined,
    set: undefined,
  });

  Object.defineProperty(
    HTMLCanvasElement.wrappedJSObject.prototype,
    "transferControlToOffscreen",
    {
      get: undefined,
      set: undefined,
    }
  );
}
