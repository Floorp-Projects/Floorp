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

function OpenLinkInExternal(path, url) {
    const process = Cc["@mozilla.org/process/util;1"].createInstance(Ci.nsIProcess);
    process.init(path);
    process.runAsync([url], 1) 
}
//OpenLinkInExternal(FileUtils.File("C:\\Users\\user\\AppData\\Local\\Vivaldi\\Application\\vivaldi.exe"), "https://google.com")

let seenDocuments = new WeakSet();

// Listen for new windows to overlay.
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
                const openLinkInVivaldi = document_.createXULElement("toolbarbutton");
                openLinkInVivaldi.id = "open-link-in-vivaldi";
                openLinkInVivaldi.classList.add("subviewbutton");
                openLinkInVivaldi.label = "Vivaldiで開く";
                document_.querySelector("#tabContextMenu #context_sendTabToDevice").insertAdjacentElement("afterend", openLinkInVivaldi);
                //window_.PanelMultiView.getViewNode(document_, "appMenu-quit-button2").insertAdjacentElement("afterend", openLinkInVivaldi);
            }
        }
    },
};
Services.obs.addObserver(documentObserver, "chrome-document-interactive");
