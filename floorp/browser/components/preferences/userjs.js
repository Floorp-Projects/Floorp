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
      button.addEventListener("click", async function (event) {
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
            await resetPreferences();
            window.setTimeout(async function () {
              try{FileUtils.getFile("ProfD", ["user.js"]).remove(false);} catch(e) {};
              Services.prefs.clearUserPref("floorp.user.js.customize");
              Services.obs.notifyObservers([], "floorp-restart-browser");
            }, 3000);
          } else {
            await resetPreferences();
            window.setTimeout(async function () {
              await setUserJSWithURL(url);
              Services.prefs.setStringPref("floorp.user.js.customize", url);
            }, 3000);
          }
        }
      });
    }
  },
};


async function setUserJSWithURL(url) {
  try{userjs.remove(false);} catch(e) {}
  fetch(url)
    .then(response => response.text())
    .then(async data => {
      const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
      const userjs = PathUtils.join(PROFILE_DIR, "user.js");
      const encoder = new TextEncoder("UTF-8");
      const writeData = encoder.encode(data);

      await IOUtils.write(userjs, writeData);
      Services.obs.notifyObservers([], "floorp-restart-browser");
    });
}


async function resetPreferences() {
  const FileUtilsPath = FileUtils.getFile("ProfD", ["user.js"]);
  const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;

  if (!FileUtilsPath.exists()) {
    console.warn("user.js does not exist");
    const path = PathUtils.join(PROFILE_DIR, "user.js");
    const encoder = new TextEncoder("UTF-8");
    const data = encoder.encode("fake data");
    await IOUtils.write(path, data);
  }

  const decoder = new TextDecoder("UTF-8");
  const path = PathUtils.join(PROFILE_DIR, "user.js");
  let read = await IOUtils.read(path);
  let inputStream = decoder.decode(read);

  const prefPattern = /user_pref\("([^"]+)",\s*(true|false|\d+|"[^"]*")\);/g;
  let match;
  while ((match = prefPattern.exec(inputStream)) !== null) {
    if (!match[0].startsWith("//")) {
      const settingName = match[1];
      await new Promise(resolve => {
        setTimeout(() => {
          Services.prefs.clearUserPref(settingName);
          console.info("resetting " + settingName);
        }, 100);
        resolve();
      });
    }
  }

}
