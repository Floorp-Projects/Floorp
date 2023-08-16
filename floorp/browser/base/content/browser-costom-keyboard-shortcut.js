/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let CustomKeyboardShortcutUtils = ChromeUtils.importESModule("resource:///modules/CustomKeyboardShortcutUtils.sys.mjs");
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const keyboradShortcutConfig = JSON.parse(Services.prefs.getStringPref(
    CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_PREF,
    ""
));

const buildShortCutkeyFunctions = {
    init() {
        if (keyboradShortcutConfig.length === 0 && CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_ENABLED_PREF) {
            return;
        }

        for (let shortcutObj of keyboradShortcutConfig) {
            let name = shortcutObj.actionName;
            let key = shortcutObj.key;
            let modifiers = shortcutObj.modifiers ? shortcutObj.modifiers : "";

            if (key && name) {
                buildShortCutkeyFunctions.buildShortCutkeyFunction(name, key, modifiers);
            }
        }
    },

    buildShortCutkeyFunction(name, key, modifiers) {
        let functionCode = CustomKeyboardShortcutUtils.keyboradShortcutActions[name][0];
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
        document.getElementById("mainKeyset").appendChild(keyElement);
    },

    removeAlreadyExistShortCutkeys() {        
        let mainKeyset = document.getElementById("mainKeyset");
        while (mainKeyset.firstChild) {
            mainKeyset.firstChild.remove();
        }
    },

    rebuildShortCutkeyFunctions() {
        buildShortCutkeyFunctions.removeAlreadyExistShortCutkeys();
        buildShortCutkeyFunctions.init();
    },
}

buildShortCutkeyFunctions.init();
