/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const CustomKeyboardShortcutUtils = ChromeUtils.importESModule(
  "resource:///modules/CustomKeyboardShortcutUtils.sys.mjs"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const keyboradShortcutConfig = JSON.parse(
  Services.prefs.getStringPref(
    CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_PREF,
    ""
  )
);

const buildShortCutkeyFunctions = {
  init() {
    if (
      Services.prefs.getBoolPref(
        CustomKeyboardShortcutUtils.SHORTCUT_KEY_DISABLE_FX_DEFAULT_SCKEY_PREF,
        false
      )
    ) {
      SessionStore.promiseInitialized.then(() => {
        buildShortCutkeyFunctions.disableAllCustomKeyShortcut();
        console.info("Remove already exist shortcut keys");
      });
    }

    const keyboradShortcutConfig = JSON.parse(
      Services.prefs.getStringPref(
        CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_PREF,
        ""
      )
    );

    if (
      keyboradShortcutConfig.length === 0 &&
      CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_ENABLED_PREF
    ) {
      return;
    }

    for (let shortcutObj of keyboradShortcutConfig) {
      let name = shortcutObj.actionName;
      let key = shortcutObj.key;
      let keyCode = shortcutObj.keyCode;
      let modifiers = shortcutObj.modifiers;

      if ((key && name) || (keyCode && name)) {
        buildShortCutkeyFunctions.buildShortCutkeyFunction(
          name,
          key,
          keyCode,
          modifiers
        );
      } else {
        console.error("Invalid shortcut key config: " + shortcutObj);
      }
    }

    function reloadCurrentTab() {
      BrowserReloadOrDuplicate("reload");
    }
    
    SessionStore.promiseInitialized.then(() => {
      Services.obs.addObserver(
        reloadCurrentTab,
        "reload-current-tab",
      );
    });
  },

  buildShortCutkeyFunction(name, key, keyCode, modifiers) {
    let functionCode =
      CustomKeyboardShortcutUtils.keyboradShortcutActions[name][0];
    if (!functionCode) {
      return;
    }

    let keyElement = window.MozXULElement.parseXULToFragment(`
            <key id="${name}" class="floorpCustomShortcutKey"
                 modifiers="${modifiers}"
                 key="${key}"
                 oncommand="${functionCode}"
             />
         `);

    if (keyCode) {
      keyElement = window.MozXULElement.parseXULToFragment(`
           <key id="${name}" class="floorpCustomShortcutKey"
                oncommand="${functionCode}"
                keycode="${keyCode}"
             />`);
    }

    document.getElementById("mainKeyset").appendChild(keyElement);
  },

  removeAlreadyExistShortCutkeys() {
    let mainKeyset = document.getElementById("mainKeyset");
    while (mainKeyset.firstChild) {
      mainKeyset.firstChild.remove();
    }
  },

  disableAllCustomKeyShortcut() {
    let keyElems = document.querySelector("#mainKeyset").childNodes;
    for (let keyElem of keyElems) {
      if (!keyElem.classList.contains("floorpCustomShortcutKey")) {
        keyElem.setAttribute("disabled", "true");
      }
    }
  },

  disableAllCustomKeyShortcutElemets() {
    let keyElems = document.querySelectorAll(".floorpCustomShortcutKey");
    for (let keyElem of keyElems) {
      keyElem.disabled = true;
    }
  },

  enableAllCustomKeyShortcutElemets() {
    let keyElems = document.querySelectorAll(".floorpCustomShortcutKey");
    for (let keyElem of keyElems) {
      keyElem.disabled = false;
    }
  },

  removeCustomKeyShortcutElemets() {
    let keyElems = document.querySelectorAll(".floorpCustomShortcutKey");
    for (let keyElem of keyElems) {
      keyElem.remove();
    }
  },
};

buildShortCutkeyFunctions.init();
