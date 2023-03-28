"use strict";

// The following check is one possible way to identify
// if this script is loaded as a ES Module or the classic way.
const isLoadedAsEsModule = this != globalThis;

// We expect the debugger script to always be loaded as classic
if (!isLoadedAsEsModule) {
  postMessage("debugger script ran");
}
