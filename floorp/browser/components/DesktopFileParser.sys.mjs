/* eslint-disable no-undef */
/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

export const EXPORTED_SYMBOLS = ["DesktopFileParser"];


export const { FileUtils } = ChromeUtils.import(
    "resource://gre/modules/FileUtils.jsm"
);

export const env = Services.env;

export const DesktopFileParser = {
    async parseFromPath(path) {
        let file = FileUtils.File(path);
        if (!file.isFile()) {
            throw new Error("This is not a file.");
        }
        if (file.fileSize > 1048576) {
            throw new Error("File size is too large.");
        }
        return this.parseFromText(await IOUtils.readUTF8(path));
    },
    parseFromText(text) {
        let lines = text.replaceAll("\r\n", "\n")
                        .replaceAll("\r", "\n")
                        .split("\n");
        let parsed = {};
        let currentSection = null;
        for (let i = 0; i < lines.length; i++) {
            let line = lines[i];
            if (line.trim() === "" || line.trim().startsWith("#")) { continue };
            if (/^\[.*\]\s*$/.test(line)) {
                currentSection = line.match(/^\[(.*)\]\s*$/)[1];
                continue;
            }
            if (currentSection !== null) {
                let line_parsed = line.split("=");
                if (line_parsed.length < 2) {throw new Error (`No value is set at line ${i + 1}`);}
                if (!parsed[currentSection]) {
                    parsed[currentSection] = {};
                }
                parsed[currentSection][line_parsed[0]] = line_parsed.slice(1).join("=");
            } else {
                throw new Error ("The value must be present in the section.");
            }
        }
        return parsed;
    },
    getCurrentLanguageNameProperty(desktopFileInfo) {
        let lang_env = env.get("LANG");
        if (lang_env !== "") {
            let lang_env_without_codeset;
            let lang_env_without_codeset_and_modifier;
            let lang_env_without_country_and_codeset;
            let lang_env_without_country_and_codeset_and_modifier;
            {
                let lang = lang_env.match(/^[a-zA-Z\_]+/);
                let modifier = lang_env.match(/@[a-zA-Z\_]+$/);
                if (lang) {
                    lang_env_without_codeset = lang[0];
                    lang_env_without_codeset_and_modifier = lang[0];
                    lang_env_without_country_and_codeset = lang[0].split("_")[0];
                    lang_env_without_country_and_codeset_and_modifier = lang[0].split("_")[0];
                    if (modifier) {
                        lang_env_without_codeset += modifier[0];
                        lang_env_without_country_and_codeset += modifier[0];
                    }
                }
            }
            let desktopEntry = desktopFileInfo.fileInfo["Desktop Entry"];
            if (lang_env_without_codeset) {
                let name_value = desktopEntry[`Name[${lang_env_without_codeset}]`];
                if (name_value) {
                    return name_value;
                }
            }
            if (lang_env_without_codeset_and_modifier) {
                let name_value = desktopEntry[`Name[${lang_env_without_codeset_and_modifier}]`];
                if (name_value) {
                    return name_value;
                }
            }
            if (lang_env_without_country_and_codeset) {
                let name_value = desktopEntry[`Name[${lang_env_without_country_and_codeset}]`];
                if (name_value) {
                    return name_value;
                }
            }
            if (lang_env_without_country_and_codeset_and_modifier) {
                let name_value = desktopEntry[`Name[${lang_env_without_country_and_codeset_and_modifier}]`];
                if (name_value) {
                    return name_value;
                }
            }
        }
        return desktopEntry.Name;
    }
};
