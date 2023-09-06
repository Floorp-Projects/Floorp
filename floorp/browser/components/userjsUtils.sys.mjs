/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";
import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

export const EXPORTED_SYMBOLS = ["userjsUtils"];

/**
 * An object containing a list of user.js scripts with their corresponding URLs.
 *
 * @typedef  {object} UserJsList
 * @property {string[]} BetterfoxDefault - The URL for the BetterfoxDefault user.js script.
 * @property {string[]} Securefox - The URL for the Securefox user.js script.
 * @property {string[]} Fastfox - The URL for the Fastfox user.js script.
 * @property {string[]} Peskyfox - The URL for the Peskyfox user.js script.
 * @property {string[]} Smoothfox - The URL for the Smoothfox user.js script.
 */

/**
 * An object containing a list of user.js scripts with their corresponding URLs.
 *
 * @type {UserJsList}
 */
export const userJsList = {
  BetterfoxDefault: ["https://raw.githubusercontent.com/yokoffing/Betterfox/115.0/user.js"],
  Securefox: ["https://raw.githubusercontent.com/yokoffing/Betterfox/115.0/Securefox.js"],
  Fastfox: ["https://raw.githubusercontent.com/yokoffing/Betterfox/115.0/Fastfox.js"],
  Peskyfox: ["https://raw.githubusercontent.com/yokoffing/Betterfox/115.0/Peskyfox.js"],
  Smoothfox: ["https://raw.githubusercontent.com/yokoffing/Betterfox/115.0/Smoothfox.js"],
};


const PROFILE_DIR = Services.dirsvc.get("ProfD", Ci.nsIFile).path;
const userjs = PathUtils.join(PROFILE_DIR, "user.js");

export const userjsUtilsFunctions = {
    userJsNameList() {
        let list = [];
        for (let name in userJsList) {
            list.push(name);
        }
    },

    /**
     * Sets the user.js file with the contents of the file at the given URL.
     * 
     * @async
     * @param {string} url - The URL of the file to set as the user.js file.
     * @returns {Promise<void>} - A Promise that resolves when the user.js file has been set.
     */
    async setUserJSWithURL(url) {
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
    },

    async getFileContent(url) {
        const response = await fetch(url);
        const text = await response.text();
        return text;
    },

    async setUserJSWithName(name) {
        const url = userJsList[name][0];
        await userjsUtilsFunctions.setUserJSWithURL(url);
    },

    /**
     * Resets preferences to Floorp's default values.
     * 
     * @async
     * @returns {Promise<void>}
     */
    async resetPreferencesWithUserJsContents() {
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
};
