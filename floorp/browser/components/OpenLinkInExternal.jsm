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
const { Subprocess } = ChromeUtils.import(
    "resource://gre/modules/Subprocess.jsm"
);
const platform = AppConstants.platform;
const env = Cc["@mozilla.org/process/environment;1"].getService(
    Ci.nsIEnvironment
);
const { DesktopFileParser } = ChromeUtils.import(
    "resource:///modules/DesktopFileParser.jsm"
);
const { EscapeShell } = ChromeUtils.import(
    "resource:///modules/EscapeShell.jsm"
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
    let targets = browsers.filter(browser => browser["urlAssociations"][protocol] === regValue);
    if (targets.length === 0) return null;
    return targets[0];
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

    let desktopFilesInfo = [];
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
            if (desktopFilesInfo.filter(desktopFileInfo => desktopFileInfo.filename === desktopFile.leafName).length === 0) {
                let desktopFileInfo = {};
                desktopFileInfo["filename"] = desktopFile.leafName;
                try {
                    desktopFileInfo["fileInfo"] = 
                        await DesktopFileParser.parseFromPath(desktopFile.path);
                } catch (e) {
                    console.log(`Failed to load ${desktopFile.path}`);
                    console.error(e);
                    continue;
                }
                if (desktopFileInfo["fileInfo"]["Desktop Entry"]) {
                    let mimetypes = desktopFileInfo["fileInfo"]["Desktop Entry"]["MimeType"];
                    if (mimetypes &&
                        (
                            mimetypes.split(";").includes("x-scheme-handler/http") ||
                            mimetypes.split(";").includes("x-scheme-handler/https")
                        )
                    ) {
                        desktopFilesInfo.push(desktopFileInfo);
                    }
                }
            }
        }
    }
    console.log(desktopFilesInfo);
    return desktopFilesInfo;
}

async function getDefaultBrowserOnLinux(protocol, desktopFilesInfo = null) {
    if (desktopFilesInfo === null) {
        desktopFilesInfo = await getBrowsersOnLinux();
    }
    let output;
    try {
        output = await Subprocess.call({
            command: "/usr/bin/xdg-mime",
            arguments: ["query", "default", `x-scheme-handler/${protocol}`],
        }).then(async (proc) => {
            proc.stdin.close().catch(() => {
                // It's possible that the process exists before we close stdin.
                // In that case, we should ignore the errors.
            });
            await proc.wait();
            return proc.stdout.readString();
        });
    } catch (e) {
        console.error(e);
        return null;
    }
    if (output === "") return null;
    let filename = output.replace(/\n$/, "");
    let targets = desktopFilesInfo.filter(desktopFileInfo => desktopFileInfo.filename === filename);
    if (targets.length === 0) return null;
    return targets[0];
}

async function OpenLinkInExternal(url) {
    let userSelectedBrowserId = Services.prefs.getStringPref("floorp.openInExternal.browserId", "");
    let protocol;
    if (url.startsWith("http")) protocol = "http";
    if (url.startsWith("https")) protocol = "https";
    if (platform === "linux") {
        let desktopFilesInfo = await getBrowsersOnLinux();
        let browser;
        if (userSelectedBrowserId === "") {
            browser = await getDefaultBrowserOnLinux(protocol, desktopFilesInfo);
            if (browser === null) {
                Services.prompt.asyncAlert(null, null, "Error", "Default browser does not exist or is not configured.");
                return;
            }
        } else {
            let targets = desktopFilesInfo.filter(desktopFileInfo => desktopFileInfo.filename === userSelectedBrowserId + ".desktop");
            if (targets.length === 0) {
                Services.prompt.asyncAlert(null, null, "Error", "The selected browser does not exist.");
                return;
            }
            browser = targets[0];
        }
        let shellscript = "#!/bin/sh\n";
        shellscript += browser["fileInfo"]["Desktop Entry"]["Exec"].replace(
            "%u",
            EscapeShell(url)
        );
        let randomized = Math.random().toString(32).substring(2);
        let outputFilePath = `/tmp/floorp_openInExternal_${randomized}.sh`;
        await IOUtils.writeUTF8(outputFilePath, shellscript);
        const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
        process.init(FileUtils.File("/bin/sh"));
        process.runAsync([outputFilePath], 1, async () => {
            await IOUtils.remove(outputFilePath);
        });
    } else if (platform === "win") {
        let browsers = getBrowsersOnWindows();
        let browser;
        if (userSelectedBrowserId === "") {
            browser = getDefaultBrowserOnWindows(protocol, browsers);
            if (browser === null) {
                Services.prompt.asyncAlert(null, null, "Error", "Default browser does not exist or is not configured.");
                return;
            }
        } else {
            let targets = browsers.filter(browser => browser.keyName === userSelectedBrowserId);
            if (targets.length === 0) {
                Services.prompt.asyncAlert(null, null, "Error", "The selected browser does not exist.");
                return;
            }
            browser = targets[0];
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
                let openLinkInExternal = document_.createXULElement("menuitem");
                openLinkInExternal.id = "open-link-in-external";
                openLinkInExternal.label = "デフォルトのブラウザーで開く";
                openLinkInExternal.addEventListener("command", function(e) {
                    let window_ = e.currentTarget.ownerGlobal;
                    OpenLinkInExternal(window_.TabContextMenu.contextTab.linkedBrowser.currentURI.spec);
                });
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
