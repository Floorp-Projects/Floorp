/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let { CustomizableUI } = ChromeUtils.import(
    "resource:///modules/CustomizableUI.jsm"
);
let { SessionStore } = ChromeUtils.import("resource:///modules/sessionstore/SessionStore.jsm")
async function UCTFirst(){
  let l10n = new Localization(["browser/floorp.ftl"])
  let l10n_text = await l10n.formatValue("undo-closed-tab") ?? "Undo close Tab"
  CustomizableUI.createWidget({
    id: 'undo-closed-tab',
    type: 'button',
    label: l10n_text,
    tooltiptext: l10n_text,
    onCreated(aNode) {
        aNode.appendChild(window.MozXULElement.parseXULToFragment(`<stack xmlns="http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul" class="toolbarbutton-badge-stack"><image class="toolbarbutton-icon" label="閉じたタブを開く"/><html:label xmlns:html="http://www.w3.org/1999/xhtml" class="toolbarbutton-badge" ></html:label></stack>`))

        let a = aNode.querySelector(".toolbarbutton-icon:not(stack > .toolbarbutton-icon)")
        if (a != null) a.remove()
    },
    onCommand() {
        if (SessionStore.getClosedTabCount(window)) SessionStore.undoCloseTab(window)
    }
  });
}
UCTFirst()