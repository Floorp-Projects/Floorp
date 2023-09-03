/* eslint-disable no-undef */
"use strict";

let sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);
let ios = Services.io;

let sheetsMap = new WeakMap();


this.stylesheet = class extends ExtensionAPI {

  onShutdown(reason) {
    let {extension} = this;
    for (let sheet of sheetsMap.get(extension)) {
      let uriObj = ios.newURI(sheet.uri);
      sss.unregisterSheet(uriObj, sss[sheet.type]);
    }
    sheetsMap.delete(extension);
  }

  getAPI(context) {
    let {extension} = context;
    if (!sheetsMap.has(extension)) {
      sheetsMap.set(extension, []);
    }
    let loadedSheets = sheetsMap.get(extension);
    return {
      stylesheet: {
        async load(uri, type) {
          let uriObj = ios.newURI(uri);
          if (!sss.sheetRegistered(uriObj, sss[type])) {
            sss.loadAndRegisterSheet(uriObj, sss[type]);
            loadedSheets.push({uri, type});
          }
        },
        async unload(uri, type) {
          let uriObj = ios.newURI(uri);
          if (sss.sheetRegistered(uriObj, sss[type])) {
            sss.unregisterSheet(uriObj, sss[type]);
            let index = loadedSheets.findIndex(s => s.uri == uri && s.type == type);
            loadedSheets.splice(index, 1);
          }
        },
        async isLoaded(uri, type) {
          let uriObj = ios.newURI(uri);
          return sss.sheetRegistered(uriObj, sss[type]);
        }
      }
    };
  }
}
