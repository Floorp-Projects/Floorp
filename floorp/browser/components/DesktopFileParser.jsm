/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ["DesktopFileParser"];


const { FileUtils } = ChromeUtils.import(
  "resource://gre/modules/FileUtils.jsm"
);

const DesktopFileParser = {
    async parseFromPath(path) {
        let file = FileUtils.File(path);
        if (!file.isFile()) {
            throw "This is not a file.";
        }
        if (file.fileSize > 1048576) {
            throw "File size is too large.";
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
            if (line.trim() === "" || line.trim().startsWith("#")) continue;
            if (/^\[.*\]\s*$/.test(line)) {
                currentSection = line.match(/^\[(.*)\]\s*$/)[1];
                continue;
            }
            if (currentSection !== null) {
                let line_parsed = line.split("=");
                if (line_parsed.length < 2) throw `No value is set at line ${i + 1}`;
                if (!parsed[currentSection]) {
                    parsed[currentSection] = {};
                }
                parsed[currentSection][line_parsed[0]] = line_parsed.slice(1).join("=");
            } else {
                throw "The value must be present in the section.";
            }
        }
        return parsed;
    }
};
