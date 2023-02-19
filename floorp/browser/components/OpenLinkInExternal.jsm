/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = [];

const { FileUtils } = ChromeUtils.import(
    "resource://gre/modules/FileUtils.jsm"
);
const { Services } = ChromeUtils.import(
    "resource://gre/modules/Services.jsm"
);
const { ExtensionCommon } = ChromeUtils.import(
    "resource://gre/modules/ExtensionCommon.jsm"
);
const { AppConstants } = ChromeUtils.import(
    "resource://gre/modules/AppConstants.jsm"
);
const platform = AppConstants.platform;
const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
);
const { DesktopFileParser } = ChromeUtils.import(
    "resource:///modules/DesktopFileParser.jsm"
);

function getBrowsersOnWindows() {
    let browsers = [];
    let ROOT_KEYS = [
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        Ci.nsIWindowsRegKey.ROOT_KEY_LOCAL_MACHINE,
    ];

    for (let ROOT_KEY of ROOT_KEYS) {
        let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
            Ci.nsIWindowsRegKey
        );
        key.open(
            ROOT_KEY,
            "Software\\Clients\\StartMenuInternet",
            Ci.nsIWindowsRegKey.ACCESS_READ
        );
        for (let i = 0; i < key.childCount; i++) {
            let keyname = key.getChildName(i);
            if (browsers.filter(browser => browser.keyName === keyname).length >= 1) continue;

            let keyBrowserName = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
                Ci.nsIWindowsRegKey
            );
            keyBrowserName.open(
                ROOT_KEY,
                `Software\\Clients\\StartMenuInternet\\${keyname}`,
                Ci.nsIWindowsRegKey.ACCESS_READ
            );
            let browserName = keyBrowserName.readStringValue("");
            keyBrowserName.close();

            let keyPath = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
                Ci.nsIWindowsRegKey
            );
            keyPath.open(
                ROOT_KEY,
                `Software\\Clients\\StartMenuInternet\\${keyname}\\shell\\open\\command`,
                Ci.nsIWindowsRegKey.ACCESS_READ
            );
            let browserPathRegValue = keyPath.readStringValue("");
            let browserPath = browserPathRegValue.replace(/^\"/, "").replace(/\"$/, "");
            keyPath.close();

            let urlAssociations = {};
            let keyUrlAssociations = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
                Ci.nsIWindowsRegKey
            );
            let error = null;
            try {
                keyUrlAssociations.open(
                    ROOT_KEY,
                    `Software\\Clients\\StartMenuInternet\\${keyname}\\Capabilities\\URLAssociations`,
                    Ci.nsIWindowsRegKey.ACCESS_READ
                );
            } catch (e) {
                error = e;
                console.error(e);
            }
            if (error === null) {
                for (let j = 0; j < keyUrlAssociations.valueCount; j++) {
                    let valuename = keyUrlAssociations.getValueName(j);
                    let urlAssociationRegValue = keyUrlAssociations.readStringValue(valuename);
                    urlAssociations[valuename] = urlAssociationRegValue;
                }
            }
            keyUrlAssociations.close();

            let fileAssociations = {};
            let keyFileAssociations = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
                Ci.nsIWindowsRegKey
            );
            let error2 = null;
            try {
                keyFileAssociations.open(
                    ROOT_KEY,
                    `Software\\Clients\\StartMenuInternet\\${keyname}\\Capabilities\\FileAssociations`,
                    Ci.nsIWindowsRegKey.ACCESS_READ
                );
            } catch (e) {
                error2 = e;
                console.error(e);
            }
            if (error2 === null) {
                for (let j = 0; j < keyFileAssociations.valueCount; j++) {
                    let valuename = keyFileAssociations.getValueName(j);
                    let fileAssociationRegValue = keyFileAssociations.readStringValue(valuename);
                    fileAssociations[valuename] = fileAssociationRegValue;
                }
            }
            keyFileAssociations.close();

            browsers.push({
                name: browserName,
                keyName: keyname,
                path: browserPath,
                fileAssociations: fileAssociations,
                urlAssociations: urlAssociations,
            });
        }
        key.close();
    }
    return browsers.sort((a, b) => {
        if (a["name"] < b["name"]) {
            return -1;
        }
        if (a["name"] > b["name"]) {
            return 1;
        }
        return 0;
    });
}

function getDefaultBrowserOnWindows(protocol, browsers = null) {
    if (browsers === null) {
        browsers = getBrowsersOnWindows();
    }
    let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
        Ci.nsIWindowsRegKey
    );
    key.open(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        `Software\\Microsoft\\Windows\\Shell\\Associations\\UrlAssociations\\${protocol}\\UserChoice`,
        Ci.nsIWindowsRegKey.ACCESS_READ
    );
    let regValue = key.readStringValue("ProgID");
    let browser = browsers.filter(browser => browser["urlAssociations"][protocol] === regValue)[0];
    return browser;
}

async function getBrowsersOnLinux() {
    let checkDirs = [];

    let xdgDataHome = env.get("XDG_DATA_HOME");
    if (xdgDataHome === "") {
        xdgDataHome = "~/.local/share/";
    }
    checkDirs.push(xdgDataHome);
    let xdgDataDirs = env.get("XDG_DATA_DIRS").split(":");
    if (xdgDataDirs[0] === "") {
        xdgDataDirs = [
            "/usr/local/share/",
            "/usr/share/",
        ];
    }
    checkDirs.push(...xdgDataDirs);

    let desktopInfo = {};
    for (let checkDir of checkDirs) {
        let applications_dir_path = PathUtils.join(checkDir, "applications");
        let dir = FileUtils.File(applications_dir_path);
        if (!dir.exists()) continue;
        let desktopFiles = [];
        let dir_entries = dir.directoryEntries;
        while (dir_entries.hasMoreElements()) {
            let dir_entry = dir_entries.getNext().QueryInterface(Ci.nsIFile);
            if (dir_entry.isFile() && dir_entry.leafName.endsWith(".desktop")) {
                desktopFiles.push(dir_entry);
            }
        }
        for (let desktopFile of desktopFiles) {
            if (!desktopInfo[desktopFile.leafName]) {
                try {
                    desktopInfo[desktopFile.leafName] = 
                        await DesktopFileParser.parseFromPath(desktopFile.path);
                } catch (e) {
                    console.log(`Failed to load ${desktopFile.path}`);
                    console.error(e);
                }
            }
        }
    }
    console.log(desktopInfo);
    return desktopInfo;
}

async function OpenLinkInExternal(url) {
    let userSelectedBrowserId = Services.prefs.getStringPref("floorp.openInExternal.browserId", "");
    let protocol;
    if (url.startsWith("http")) protocol = "http";
    if (url.startsWith("https")) protocol = "https";
    if (platform === "linux") {
        let desktopInfo = await getBrowsersOnLinux();
        let browser;
        if (userSelectedBrowserId === "") {
            browser = await getDefaultBrowserOnLinux(desktopInfo);
        } else {
            browser = desktopInfo[userSelectedBrowserId + ".desktop"];
        }
        let shellscript = "#!/bin/sh\n";
        shellscript += browser["Desktop Entry"]["Exec"].replace(
            "%u",
            `"${url.replaceAll("\\", "\\\\").replaceAll('"', '\\"').replaceAll("`", "\\`").replaceAll("$", "\\$")}"`
        );
        await IOUtils.writeUTF8("/tmp/floorp_open_in_external.sh", shellscript);
        const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
        process.init(FileUtils.File("/bin/sh"));
        process.runAsync(["/tmp/floorp_open_in_external.sh"], 1);
    } else if (platform === "win") {
        let browsers = getBrowsersOnWindows();
        let browser;
        if (userSelectedBrowserId === "") {
            browser = getDefaultBrowserOnWindows(protocol, browsers);
        } else {
            browser = browsers.filter(browser => browser.keyName === userSelectedBrowserId);
        }
        let browserPath = browser["path"];
        const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
        process.init(FileUtils.File(browserPath));
        process.runAsync([url], 1);
    }
}

let seenDocuments = new WeakSet();
let documentObserver = {
    observe(doc) {
        if (
            ExtensionCommon.instanceOf(doc, "HTMLDocument") &&
            !seenDocuments.has(doc)
        ) {
            seenDocuments.add(doc);
            let window_ = doc.defaultView;
            let document_ = window_.document;
            if (window_.location.href == "chrome://browser/content/browser.xhtml") {
                let tabContextMenu = document_.querySelector("#tabContextMenu");
                window_.OpenLinkInExternalMenu = function (url) {
                    OpenLinkInExternal(url);
                }
                let openLinkInExternal = document_.createXULElement("menuitem");
                openLinkInExternal.id = "open-link-in-external";
                openLinkInExternal.label = "デフォルトのブラウザーで開く";
                openLinkInExternal.setAttribute(
                    "oncommand",
                    "OpenLinkInExternalMenu(TabContextMenu.contextTab.linkedBrowser.currentURI.spec);"
                );
                tabContextMenu.addEventListener("popupshowing", function(e) {
                    let window_ = e.currentTarget.ownerGlobal;
                    let scheme = window_.TabContextMenu.contextTab.linkedBrowser.currentURI.scheme;
                    e.currentTarget.querySelector("#open-link-in-external").hidden = !/https?/.test(scheme);
                });
                tabContextMenu.querySelector("#context_sendTabToDevice")
                    .insertAdjacentElement("afterend", openLinkInExternal);
            }
        }
    },
};
Services.obs.addObserver(documentObserver, "chrome-document-interactive");
