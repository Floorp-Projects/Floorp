/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * This file contains the globals for the Gecko Profiler frame script environment.
 */

interface ContentWindow {
  wrappedJSObject: {
    connectToGeckoProfiler?: (
      interface: import("./perf").GeckoProfilerFrameScriptInterface
    ) => void;
  };
}

declare var content: ContentWindow;
