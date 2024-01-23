import { getCounter, setCounter } from "./es6module_dynamic_import_static.js";

let resolve;

export const result = new Promise(r => { resolve = r; });

import("./es6module_dynamic_import2.js").then(ns => {
  resolve(ns);
});

export function doImport() {
  return import("./es6module_dynamic_import3.js");
}

export function callGetCounter() {
  return getCounter();
}

export function callSetCounter(v) {
  setCounter(v);
}

export function doImportStatic() {
  return import("./es6module_dynamic_import_static.js");
}
