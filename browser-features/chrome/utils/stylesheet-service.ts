//https://github.com/Floorp-Projects/Floorp-core/blob/b759e6eca79449389b5806b5b7bd6d9bbd718339/browser/base/content/browser-command.js#L46
/******************************************** StyleSheetService (userContent.css) ******************************/

const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);
const ios = Services.io;

export namespace StyleSheetServiceUtils {
  /**
   * load styleSheet with NsStyleSheetService
   * @param styleSheetURL
   */
  export function loadStyleSheetWith(styleSheetURL: string) {
    const uri = ios.newURI(styleSheetURL);
    sss.loadAndRegisterSheet(uri, sss.USER_SHEET);
  }

  /**
   * check styleSheet loaded with NsStyleSheetService
   * @param styleSheetURL
   */
  export function checkStyleSheetLoaded(styleSheetURL: string): boolean {
    const uri = ios.newURI(styleSheetURL);
    return sss.sheetRegistered(uri, sss.USER_SHEET);
  }

  /**
   * unregister styleSheet loaded with NsStyleSheetService
   * @param styleSheetURL
   */
  export function unloadStyleSheet(styleSheetURL: string) {
    const uri = ios.newURI(styleSheetURL);
    sss.unregisterSheet(uri, sss.USER_SHEET);
  }
}
