/* eslint-disable no-undef */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var { AppConstants } =  ChromeUtils.import("resource://gre/modules/AppConstants.jsm");
const CustomKeyboardShortcutUtils = ChromeUtils.importESModule("resource:///modules/CustomKeyboardShortcutUtils.sys.mjs");

XPCOMUtils.defineLazyGetter(this, "L10n", () => {
  return new Localization(["branding/brand.ftl", "browser/floorp", ]);
});

Preferences.addAll([
  { id: "floorp.custom.shortcutkeysAndActions.remove.fx.actions", type: "bool" },
])
 
const gCSKPane = {
  _pane: null,
  init() {
   // const l10n = new Localization(["browser/floorp.ftl"], true);
   this._pane = document.getElementById("panCSK");

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
    const restoreDefaultButton = document.getElementById("reset-CSK-button");

    async function restoreDefault() {
      let l10n = new Localization(["browser/floorp.ftl"], true);
      const prompts = Services.prompt;
      const check = {
        value: false
      };
      const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
      let result = prompts.confirmEx(null, l10n.formatValueSync("CSK-restore-default"), l10n.formatValueSync("CSK-restore-default-description"), flags, "", null, "", null, check);
      if (result == 0) {
        utils.preferencesFunctions.removeAllKeyboradShortcut();
      }
    };

    async function removeShortcutKey(actionName){
      let l10n = new Localization(["browser/floorp.ftl"], true);
      const prompts = Services.prompt;
      const check = {
        value: false
      };
      const flags = prompts.BUTTON_POS_0 * prompts.BUTTON_TITLE_OK + prompts.BUTTON_POS_1 * prompts.BUTTON_TITLE_CANCEL;
      let result = prompts.confirmEx(null, l10n.formatValueSync("CSK-remove-shortcutkey"), l10n.formatValueSync("CSK-remove-shortcutkey-description"), flags, "", null, "", null, check);
      if (result == 0) {
        utils.preferencesFunctions.removeKeyboradShortcutByActionName(actionName);

        const removeButton = document.querySelector(`.csks-remove-button[value="${actionName}"]`);
        const button = document.querySelector(`.csks-button[value="${actionName}"]`);
        const descriptionItem = document.querySelector(`.csks-box-item-description[value="${actionName}"]`);
        button.setAttribute("disabled", "false");
        removeButton.setAttribute("hidden", "true");
        descriptionItem.remove();
        window.location.reload();
      }
    }

    restoreDefaultButton.addEventListener("click", restoreDefault);

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
          (async function (action) {
            const actionL10nId = utils.getInfoFunctions.getFluentLocalization(action);
            const parentBox = document.getElementById(`${type}`);
            const CSKIsExist = utils.getInfoFunctions.actionIsExsit(action);
 
            const boxItem = window.MozXULElement.parseXULToFragment(`
              <hbox class="csks-box-item" id="${action}">
                <label class="csks-box-item-label" value="${action}" data-l10n-id="${actionL10nId}"/>
                <spacer flex="1"/> 
                <button class="csks-remove-button" value="${action}" data-l10n-id="remove-Action" hidden="${!CSKIsExist}"/>
                <button class="csks-button" value="${action}" data-l10n-id="customize-Action"/>
              </hbox>
            `);
            parentBox.appendChild(boxItem);

            if (CSKIsExist) {
              const keyboradShortcutObj = utils.getInfoFunctions.getActionKey(action);
              const key = keyboradShortcutObj.key;
              const modifiers = keyboradShortcutObj.modifiers ? keyboradShortcutObj.modifiers : undefined;

              // Disable button if the keyborad shortcut is exist
              const button = document.querySelector(`.csks-button[value="${action}"]`);
              button.setAttribute("disabled", "true");

              // Add keyborad shortcut info
              const boxItem = document.querySelector(`.csks-box-item[id="${action}"]`);
              const keyboradShortcutInfo = window.MozXULElement.parseXULToFragment(`
                <description value="${action}" class="indent tip-caption csks-box-item-description">
                </description>
              `);
              boxItem.after(keyboradShortcutInfo);

              // add l10n
              if(!keyboradShortcutObj.keyCode || keyboradShortcutObj.keyCode === ""){
                const descriptionItem = document.querySelector(`.csks-box-item-description[value="${action}"]`);
                document.l10n.setAttributes(
                  descriptionItem,
                  "CSK-keyborad-shortcut-info",
                  { key, modifiers }
                );  
              } else {
                const descriptionItem = document.querySelector(`.csks-box-item-description[value="${action}"]`);
                document.l10n.setAttributes(
                  descriptionItem,
                  "CSK-keyborad-shortcut-info-with-keycode",
                  { key: keyboradShortcutObj.keyCode }
                );
              }

              // Add remove button event listener
              const removeButton = document.querySelector(`.csks-remove-button[value="${action}"]`);
              removeButton.addEventListener("click", function () {
                removeShortcutKey(action);
              });
            } else {
              // add event listener
              const button = document.querySelector(`.csks-button[value="${action}"]`);
              button.addEventListener("click", function () {
                CustomKeyboardShortcutUtils.keyboradShortcutFunctions.openDialog(action);
              });
            }
          })(actionName);
        }
      }
    }
    buildCustomShortcutkeyPreferences();
  },
};
