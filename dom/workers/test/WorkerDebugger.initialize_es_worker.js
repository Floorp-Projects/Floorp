"use strict";

// The following check is one possible way to identify
// if this script is loaded as a ES Module or the classic way.
const isLoadedAsEsModule = this != globalThis;

// Here we expect the worker to be loaded a ES Module
if (isLoadedAsEsModule) {
  postMessage("worker");
}
