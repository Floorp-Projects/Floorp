/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
);

let userjsUtils = ChromeUtils.importESModule(
  "resource:///modules/userjsUtils.sys.mjs"
);

let l10n = new Localization(["browser/floorp.ftl"], true);

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp"]);
});

const gUserjsPane = {
  _pane: null,
  init() {
    this._pane = document.getElementById("paneUserjs");
    document
      .getElementById("backtogeneral___")
      .addEventListener("command", function () {
        gotoPref("general");
      });

    let buttons = document.getElementsByClassName("apply-userjs-button");
    for (let button of buttons) {
      button.addEventListener("click", async function (event) {
        let url = event.target.getAttribute("data-url");
        let id = event.target.getAttribute("id");
        const prompts = Services.prompt;
        const check = { value: false };
        const flags =
          prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK +
          prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
        let result = prompts.confirmEx(
          null,
          l10n.formatValueSync("userjs-prompt"),
          `${l10n.formatValueSync(
            "apply-userjs-attention"
          )}\n${l10n.formatValueSync("apply-userjs-attention2")}`,
          flags,
          "",
          null,
          "",
          null,
          check
        );
        if (result == 0) {
          if (!url) {
            await userjsUtils.userjsUtilsFunctions.resetPreferencesWithUserJsContents();
            window.setTimeout(async function () {
              try{FileUtils.getFile("ProfD", ["user.js"]).remove(false);} catch(e) {};
              Services.prefs.clearUserPref("floorp.user.js.customize");
              Services.obs.notifyObservers([], "floorp-restart-browser");
            }, 3000);
          } else {
            await userjsUtils.userjsUtilsFunctions.resetPreferencesWithUserJsContents();
            window.setTimeout(async function () {
              await userjsUtils.userjsUtilsFunctions.setUserJSWithURL(url);
              Services.prefs.setStringPref("floorp.user.js.customize", id);
            }, 3000);
          }
        }
      });
    }
  },
};
