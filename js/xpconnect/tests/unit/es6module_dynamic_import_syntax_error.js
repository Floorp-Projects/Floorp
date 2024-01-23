let resolve;

export const result = new Promise(r => { resolve = r; });

import("./es6module_dynamic_import_syntax_error2.js").then(ns => {}, e => {
  resolve(e);
});

export async function doImport() {
  try {
    await import("./es6module_dynamic_import_syntax_error3.js");
  } catch (e) {
    return e;
  }

  return null;
}
