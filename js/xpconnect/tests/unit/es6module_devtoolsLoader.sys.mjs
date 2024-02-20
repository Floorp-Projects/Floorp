export let x = 0;

export function increment() {
  x++;
};

import { object } from "resource://test/es6module_devtoolsLoader.js";
export const importedObject = object;

const importDevTools = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { global: "devtools" });
export const importESModuleDevTools = importDevTools.object;

const importShared = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { global: "shared" });
export const importESModuleShared = importShared.object;

const importCurrent = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { global: "current" });
export const importESModuleCurrent = importCurrent.object;

const importContextual = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { global: "contextual" });
export const importESModuleContextual = importContextual.object;

let caught = false;
try {
  ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js");
} catch (e) {
  caught = true;
}
export const importESModuleNoOptionFailed1 = caught;

caught = false;
try {
  ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", {});
} catch (e) {
  caught = true;
}
export const importESModuleNoOptionFailed2 = caught;

const lazyDevTools = {};
ChromeUtils.defineESModuleGetters(lazyDevTools, {
  object: "resource://test/es6module_devtoolsLoader.js",
}, { global: "devtools" });

export function importLazyDevTools() {
  return lazyDevTools.object;
}

const lazyShared = {};
ChromeUtils.defineESModuleGetters(lazyShared, {
  object: "resource://test/es6module_devtoolsLoader.js",
}, { global: "shared" });

export function importLazyShared() {
  return lazyShared.object;
}

const lazyCurrent = {};
ChromeUtils.defineESModuleGetters(lazyCurrent, {
  object: "resource://test/es6module_devtoolsLoader.js",
}, { global: "current" });

export function importLazyCurrent() {
  return lazyCurrent.object;
}

const lazyContextual = {};
ChromeUtils.defineESModuleGetters(lazyContextual, {
  object: "resource://test/es6module_devtoolsLoader.js",
}, { global: "contextual" });

export function importLazyContextual() {
  return lazyContextual.object;
}

caught = false;
try {
  let lazy = {};
  ChromeUtils.defineESModuleGetters({}, {
    object: "resource://test/es6module_devtoolsLoader.js",
  });
} catch (e) {
  caught = true;
}
export const importLazyNoOptionFailed1 = caught;

caught = false;
try {
  let lazy = {};
  ChromeUtils.defineESModuleGetters({}, {
    object: "resource://test/es6module_devtoolsLoader.js",
  }, {});
} catch (e) {
  caught = true;
}
export const importLazyNoOptionFailed2 = caught;
