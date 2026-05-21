// SPDX-License-Identifier: MPL-2.0

//https://github.com/Floorp-Projects/Floorp-core/blob/b759e6eca79449389b5806b5b7bd6d9bbd718339/browser/base/content/browser-command.js#L46
/******************************************** StyleSheetService (userContent.css) ******************************/

const sss = Cc["@mozilla.org/content/style-sheet-service;1"].getService(
  Ci.nsIStyleSheetService,
);
const ios = Services.io;

// deno-lint-ignore no-namespace
export namespace StyleSheetServiceUtils {
  /**
   * load styleSheet with NsStyleSheetService
   * @param styleSheetURL
   */
  export function loadStyleSheetWith(styleSheetURL: string) {
    const sheetType = sss.USER_SHEET;
    if (sheetType == null) {
      console.error("[StyleSheetService] USER_SHEET is not available");
      return;
    }
    const uri = ios.newURI(styleSheetURL);
    sss.loadAndRegisterSheet(uri, sheetType);
  }

  /**
   * check styleSheet loaded with NsStyleSheetService
   * @param styleSheetURL
   */
  export function checkStyleSheetLoaded(styleSheetURL: string): boolean {
    const sheetType = sss.USER_SHEET;
    if (sheetType == null) {
      console.error("[StyleSheetService] USER_SHEET is not available");
      return false;
    }
    const uri = ios.newURI(styleSheetURL);
    return sss.sheetRegistered(uri, sheetType);
  }

  /**
   * unregister styleSheet loaded with NsStyleSheetService
   * @param styleSheetURL
   */
  export function unloadStyleSheet(styleSheetURL: string) {
    const sheetType = sss.USER_SHEET;
    if (sheetType == null) {
      console.error("[StyleSheetService] USER_SHEET is not available");
      return;
    }
    const uri = ios.newURI(styleSheetURL);
    sss.unregisterSheet(uri, sheetType);
  }
}
