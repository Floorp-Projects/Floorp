let resolve;

export const result = new Promise(r => { resolve = r; });

import("./es6module_dynamic_import_missing2.js").then(ns => {}, e => {
  resolve(e);
});

export async function doImport() {
  try {
    await import("./es6module_dynamic_import_missing3.js");
  } catch (e) {
    return e;
  }

  return null;
}
