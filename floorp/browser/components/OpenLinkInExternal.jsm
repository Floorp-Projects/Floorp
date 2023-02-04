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

function getBrowsersPath() {
    let browsersPath = [];
    let key = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
        Ci.nsIWindowsRegKey
    );
    key.open(
        Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
        "Software\\Clients\\StartMenuInternet",
        Ci.nsIWindowsRegKey.ACCESS_READ
    );
    for (let i = 0; i < key.childCount; i++) {
        let keyname = key.getChildName(i);
        let keysub = Cc["@mozilla.org/windows-registry-key;1"].createInstance(
            Ci.nsIWindowsRegKey
        );
        keysub.open(
            Ci.nsIWindowsRegKey.ROOT_KEY_CURRENT_USER,
            `Software\\Clients\\StartMenuInternet\\${keyname}\\shell\\open\\command`,
            Ci.nsIWindowsRegKey.ACCESS_READ
        );
        let regValue = keysub.readStringValue("");
        let browserPath = regValue.replace(/^\"/, "").replace(/\"$/, "");
        browsersPath.push(browserPath);
        keysub.close();
    }
    key.close();
    return browsersPath;
}

function OpenLinkInExternal(url) {
    const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(FileUtils.File(getBrowsersPath()[0]));
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
