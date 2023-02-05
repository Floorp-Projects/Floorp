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

function getBrowsers() {
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
            try {
                keyUrlAssociations.open(
                    ROOT_KEY,
                    `Software\\Clients\\StartMenuInternet\\${keyname}\\Capabilities\\URLAssociations`,
                    Ci.nsIWindowsRegKey.ACCESS_READ
                );
                for (let j = 0; j < keyUrlAssociations.valueCount; j++) {
                    let valuename = keyUrlAssociations.getValueName(j);
                    let urlAssociationRegValue = keyUrlAssociations.readStringValue(valuename);
                    urlAssociations[valuename] = urlAssociationRegValue;
                }
            } catch (e) {
                console.error(e);
            }
            keyUrlAssociations.close();

            browsers.push({
                keyName: keyname,
                path: browserPath,
                urlAssociations: urlAssociations
            });
        }
        key.close();
    }
    return browsers;
}

function OpenLinkInExternal(url) {
    const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(FileUtils.File(getBrowsers()[0]["path"]));
    process.runAsync([url], 1) 
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
                openLinkInExternal.label = "Vivaldiで開く";
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
