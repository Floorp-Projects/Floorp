//https://github.com/Floorp-Projects/Floorp-core/blob/b759e6eca79449389b5806b5b7bd6d9bbd718339/browser/base/content/browser-command.js#L46
/******************************************** StyleSheetService (userContent.css) ******************************/

const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);
const ios = Services.io;

export function loadStyleSheetWithNsStyleSheetService(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL);
  sss.loadAndRegisterSheet(uri, sss.USER_SHEET);
}

export function checkProvidedStyleSheetLoaded(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL);
  if (!sss.sheetRegistered(uri, sss.USER_SHEET)) {
    sss.loadAndRegisterSheet(uri, sss.USER_SHEET);
  }
}

export function unloadStyleSheetWithNsStyleSheetService(styleSheetURL) {
  const uri = ios.newURI(styleSheetURL);
  sss.unregisterSheet(uri, sss.USER_SHEET);
}
