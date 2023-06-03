"use strict";

const { classes: Cc, interfaces: Ci } = Components;

let sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(Ci.nsIStyleSheetService);
let ios = Cc["@mozilla.org/network/io-service;1"].getService(Ci.nsIIOService);

let sheetsMap = new WeakMap();


this.stylesheet = class extends ExtensionAPI {

  onShutdown(reason) {
    let {extension} = this;
    for (let sheet of sheetsMap.get(extension)) {
      let uriObj = ios.newURI(sheet.uri, null, null);
      sss.unregisterSheet(uriObj, sss[sheet.type]);
    }
    sheetsMap.delete(extension);
  }

  getAPI(context) {
    let {extension} = context;
    if (!sheetsMap.has(extension)) {
      sheetsMap.set(extension, new Array());
    }
    let loadedSheets = sheetsMap.get(extension);
    return {
      stylesheet: {
        async load(uri, type) {
          let uriObj = ios.newURI(uri, null, null);
          if (!sss.sheetRegistered(uriObj, sss[type])) {
            sss.loadAndRegisterSheet(uriObj, sss[type]);
            loadedSheets.push({uri: uri, type: type});
          }
        },
        async unload(uri, type) {
          let uriObj = ios.newURI(uri, null, null);
          if (sss.sheetRegistered(uriObj, sss[type])) {
            sss.unregisterSheet(uriObj, sss[type]);
            let index = loadedSheets.findIndex(s => s.uri == uri && s.type == type);
            loadedSheets.splice(index, 1);
          }
        },
        async isLoaded(uri, type) {
          let uriObj = ios.newURI(uri, null, null);
          return sss.sheetRegistered(uriObj, sss[type]);
        }
      }
    };
  }
}
