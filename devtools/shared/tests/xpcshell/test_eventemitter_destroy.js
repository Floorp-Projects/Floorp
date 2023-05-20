/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

add_task(function () {
  const { DevToolsLoader, require } = ChromeUtils.importESModule(
    "resource://devtools/shared/loader/Loader.sys.mjs"
  );

  // Force-load the module once in the global loader to avoid Bug 1622718.
  require("resource://devtools/shared/event-emitter.js");

  const emitterRef = (function () {
    const loader = new DevToolsLoader();

    const ref = Cu.getWeakReference(
      loader.require("resource://devtools/shared/event-emitter.js")
    );

    loader.destroy();
    return ref;
  })();

  Cu.forceGC();
  Cu.forceCC();
  Cu.forceGC();
  Cu.forceCC();

  Assert.ok(!emitterRef.get(), "weakref has been cleared by gc");
});
