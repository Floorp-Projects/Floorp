/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

function enableRestMode() {
    if (Services.prefs.getBoolPref("floorp.browser.rest.mode", false)) {
        let selectedTab = gBrowser.selectedTab;
        let selectedTabLocation = selectedTab.linkedBrowser.documentURI.spec;
        for (let tab of gBrowser.tabs) {
            gBrowser.discardBrowser(tab);
        }
        if (selectedTabLocation) {
            openTrustedLinkIn("about:blank", "current");
        }
        var Tag = document.createElement("style");
        Tag.textContent = `
        * {
            display:none !important;
        }`;
        Tag.setAttribute("id", "floorp-rest-mode");
        document.head.appendChild(Tag);

        let l10n = new Localization(["browser/floorp.ftl"], true);
        Services.prompt.alert(
            null,
            l10n.formatValueSync("rest-mode"),
            l10n.formatValueSync("rest-mode-description")
        );

        document.getElementById("floorp-rest-mode").remove();
        if (selectedTabLocation) {
            openTrustedLinkIn(selectedTabLocation, "current");
        }
    }
}
