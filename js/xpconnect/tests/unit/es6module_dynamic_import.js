let resolve;

export const result = new Promise(r => { resolve = r; });

import("./es6module_dynamic_import2.js").then(ns => {}, e => {
  resolve(e);
});
