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
        const keyboradShortcutConfig = JSON.parse(Services.prefs.getStringPref(
            CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_PREF,
            ""
        ));

        if (keyboradShortcutConfig.length === 0 && CustomKeyboardShortcutUtils.SHORTCUT_KEY_AND_ACTION_ENABLED_PREF) {
            return;
        }

        for (let shortcutObj of keyboradShortcutConfig) {
            console.log(shortcutObj);
            let name = shortcutObj.actionName;
            let key = shortcutObj.key;
            let keyCode = shortcutObj.keyCode;
            let modifiers = shortcutObj.modifiers;
            
            if (key && name || keyCode && name) {
                console.log("buildShortCutkeyFunction: " + name + " " + key + " " + keyCode + " " + modifiers);
                buildShortCutkeyFunctions.buildShortCutkeyFunction(name, key, keyCode, modifiers);
            } else {
                console.error("Invalid shortcut key config: " + shortcutObj);
            }
        }
    },

    buildShortCutkeyFunction(name, key, keyCode, modifiers) {
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
};

buildShortCutkeyFunctions.init();
