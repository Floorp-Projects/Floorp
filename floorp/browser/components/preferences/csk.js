/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } =  ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const CustomKeyboardShortcutUtils = ChromeUtils.importESModule("resource:///modules/CustomKeyboardShortcutUtils.sys.mjs");

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp", ]);
});
 
const gCSKPane = {
  _pane: null,
  init() {
   // const l10n = new Localization(["browser/floorp.ftl"], true);
   this._pane = document.getElementById("panCSK");
   document.getElementById("backtogeneral-CSK").addEventListener("command", function () {
      gotoPref("general")
    });

    const needreboot = document.getElementsByClassName("needreboot");
    for (let i = 0; i < needreboot.length; i++) {
      needreboot[i].addEventListener("click", function () {
        if (!Services.prefs.getBoolPref("floorp.enable.auto.restart", false)) {
          (async () => {
            let userConfirm = await confirmRestartPrompt(null)
            if (userConfirm == CONFIRM_RESTART_PROMPT_RESTART_NOW) {
              Services.startup.quit(
                Ci.nsIAppStartup.eAttemptQuit | Ci.nsIAppStartup.eRestart
              );
            }
          })()
        } else {
          window.setTimeout(function () {
            Services.startup.quit(Services.startup.eAttemptQuit | Services.startup.eRestart);
          }, 500);
        }
      });
    }

    const utils = CustomKeyboardShortcutUtils.keyboradShortcutFunctions;
    const allActionType = utils.getInfoFunctions.getAllActionType();

    function buildCustomShortcutkeyPreferences() {
      const shortcutkeyPreferences = document.getElementById("csks-box");
      for (let i = 0; i < allActionType.length; i++) {
        const type = allActionType[i];
        const actions = utils.getInfoFunctions.getkeyboradShortcutActionsByType(type);
        
        const box = window.MozXULElement.parseXULToFragment(`
          <vbox class="csks-content-box" id="${type}">
            <html:h2 class="csks-box-title" data-l10n-id="${utils.getInfoFunctions.getTypeLocalization(type)}"></html:h2>
          </vbox>
        `);
        shortcutkeyPreferences.appendChild(box);         

        for (actionName of actions) {
          (function (action) {
            const actionL10nId = utils.getInfoFunctions.getFluentLocalization(action);
            const parentBox = document.getElementById(`${type}`);
        
            const boxItem = window.MozXULElement.parseXULToFragment(`
              <hbox class="csks-box-item">
                <label class="csks-box-item-label" data-l10n-id="${actionL10nId}"/>
                <spacer flex="1"/> 
                <button class="csks-button" value="${action}" label="Customize Action"/>
              </hbox>
            `);
            parentBox.appendChild(boxItem);
        
            // add event listener
            const button = document.querySelector(`.csks-button[value="${action}"]`);
            button.addEventListener("click", function () {
              CustomKeyboardShortcutUtils.keyboradShortcutFunctions.openDialog(action);
            });
          })(actionName);
        }
      }
    }
    buildCustomShortcutkeyPreferences();
  },
};
