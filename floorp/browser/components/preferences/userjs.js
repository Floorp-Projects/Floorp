/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } = ChromeUtils.import(
  "resource://gre/modules/AppConstants.jsm"
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
      button.addEventListener("click", function (event) {
        let url = event.target.getAttribute("data-url");
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
            try{FileUtils.getFile("ProfD", ["user.js"]).remove(false);} catch(e) {}
          } else {
            setUserJSWithURL(url);
          }
        }
      });
    }
  },
};

const userjs = FileUtils.getFile("ProfD", ["user.js"]);
const outputStream = FileUtils.openFileOutputStream(
  userjs,
  FileUtils.MODE_WRONLY | FileUtils.MODE_CREATE | FileUtils.MODE_APPEND
);
function setUserJSWithURL(url) {
  fetch(url)
    .then(response => response.text())
    .then(data => {
      try{userjs.remove(false);} catch(e) {}
      outputStream.write(data, data.length);
      outputStream.close();
      Services.obs.notifyObservers([], "floorp-restart-browser");
    });
}
