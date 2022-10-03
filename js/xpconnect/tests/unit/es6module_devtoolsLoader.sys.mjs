export let x = 0;

export function increment() {
  x++;
};

import { object } from "resource://test/es6module_devtoolsLoader.js";
export const importedObject = object;

const importTrue = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { loadInDevToolsLoader : true });
export const importESModuleTrue = importTrue.object;

const importFalse = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", { loadInDevToolsLoader : false });
export const importESModuleFalse = importFalse.object;

const importNull = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js", {});
export const importESModuleNull = importNull.object;

const importNull2 = ChromeUtils.importESModule("resource://test/es6module_devtoolsLoader.js");
export const importESModuleNull2 = importNull2.object;

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  object: "resource://test/es6module_devtoolsLoader.js",
});

export function importLazy() {
  return lazy.object;
}
