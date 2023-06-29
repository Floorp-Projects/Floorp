/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

/* import-globals-from preferences.js */

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } =  ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
let l10n = new Localization(["browser/floorp.ftl"], true);

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp", ]);
});

const gUserjsPane = {
  _pane: null,
  init() {
    this._pane = document.getElementById("paneUserjs");
    document.getElementById("backtogeneral___").addEventListener("command", function () {
      gotoPref("general")
    });
    
    let buttons = document.getElementsByClassName("apply-userjs-button");
    for (let button of buttons){
        button.addEventListener("click", function(event){
            let url = event.target.getAttribute("data-url");
            const prompts = Services.prompt;
            const check = { value: false };
            const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
            let result = prompts.confirmEx(null, l10n.formatValueSync("userjs-prompt"), `${l10n.formatValueSync("apply-userjs-attention")}\n${l10n.formatValueSync("apply-userjs-attention2")}`, flags, "", null, "", null, check);
            if (result == 0) {
              if (!url){
                OS.File.remove(userjs);
              } else {
                setUserJSWithURL(url);
              }
            }
       });
    }
  },
};

let userjs = OS.Path.join(OS.Constants.Path.profileDir, "user.js");
function setUserJSWithURL(url){
    fetch(url).then(response => response.text()).then(data => {
        OS.File.writeAtomic(userjs, data, {
            encoding: "utf-8"
        });
    });
}