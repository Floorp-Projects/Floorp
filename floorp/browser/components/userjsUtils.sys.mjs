/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import { setTimeout } from "resource://gre/modules/Timer.sys.mjs";
import { FileUtils } from "resource://gre/modules/FileUtils.sys.mjs";

export const EXPORTED_SYMBOLS = ["userjsUtils"];

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
