/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

if (
  Services.prefs.getBoolPref("floorp.browser.native.downloadbar.enabled", true)
) {
  window.setTimeout(() => {
    function changeXULElementTagName(oldElementId, newTagName) {
      const oldElement = document.getElementById(oldElementId);
      const newElement = document.createElement(newTagName);

      const attrs = oldElement.attributes;
      for (let i = 0; i < attrs.length; i++) {
        newElement.setAttribute(attrs[i].name, attrs[i].value);
      }

      while (oldElement.firstChild) {
        newElement.appendChild(oldElement.firstChild);
      }
      oldElement.parentNode.replaceChild(newElement, oldElement);
    }

    document
      .getElementById("appcontent")
      .appendChild(document.getElementById("downloadsPanel"));
    document.getElementById("downloadsPanel").style.display = "block";
    document.getElementById("downloadsPanel").hidden = false;

    changeXULElementTagName("downloadsPanel", "vbox");
    changeXULElementTagName("downloadsPanel-multiView", "vbox");
    changeXULElementTagName("downloadsPanel-mainView", "vbox");

    const elem = document.createElement("div");
    elem.id = "close";
    elem.textContent = `X`;
    elem.setAttribute("flex", "1");
    document.head.appendChild(elem);

    const Tag = document.createElement("style");
    Tag.textContent = `
    :root[inDOMFullscreen] :is(#downloadsPanel), :root[customizing] :is(#downloadsPanel),
    :root[inFullscreen]:not([macOSNativeFullscreen]) :is(#downloadsPanel) {
      display: none !important;
    }
    .downloadDetails {
      width: 170px;
      white-space: nowrap;
      overflow: hidden;
      text-overflow: ellipsis;
    }
    #downloadsListBox > richlistitem {
      margin: 3px 6px 3px 6px !important;
      max-width: min-content !important;
    }
    #emptyDownloads {
      display: none !important;
    }
    #downloadsFooterButtons{
      display: none !important;
    }
    #downloadsListBox {
      background: var(--toolbar-bgcolor);
      display: flex !important;
      width: auto !important;
      flex-direction: initial;
    }
    #downloadsPanel-blockedSubview, #downloadsPanel-mainView {
      padding: 0 !important;
    }
    #downloadsPanel {
      margin: 0 !important;
      background-color: var(--toolbar-bgcolor) !important;
    }
    #downloads-button {
      display: none !important;
    }
    #downloadsPanel-mainView > .panel-view-body-unscrollable {
      display: flex !important;
    }
    #downloadsPanel:not([hasdownloads="true"]) #downloadsListBox > .toolbarbutton-1 {
      display: none !important;
    }
    #hide-downloads-button {
      list-style-image: url('chrome://browser/skin/close.svg');
      margin-right: 0.5em;
      margin-top: auto;
      margin-bottom: auto;
      appearance: inherit;
      border-radius: 5px;
      padding: 10px;
    }
    #hide-downloads-button:hover {
      background-color: var(--toolbarbutton-hover-background);
    }
    #hide-downloads-button:active {
      background-color: var(--toolbarbutton-active-background);
    }
    #hide-downloads-button > .toolbarbutton-text {
      display: none !important;
    }
    #show-downloads-button {
      list-style-image: url('chrome://browser/skin/downloads/downloads.svg');
      margin-left: auto;
      margin-right: 1em;
      margin-top: auto;
      margin-bottom: auto;
      appearance: inherit;
      border-radius: 5px;
      padding: 10px;
    }
    #show-downloads-button > .toolbarbutton-icon {
      margin-right: 0.5em !important;
      width: 15px;
      height: 15px;
    }
    #show-downloads-button:hover {
      background-color: var(--toolbarbutton-hover-background);
    }
    #show-downloads-button:active {
      background-color: var(--toolbarbutton-active-background);
    }
    `;
    document.head.appendChild(Tag);

    //delete all downloads button

    const hideAllDownloadButtonElem = window.MozXULElement.parseXULToFragment(`
    <toolbarbutton id="hide-downloads-button" class="toolbarbutton-1" command="downloadsCmd_clearList" data-l10n-id="floorp-delete-all-downloads" />
    `);
    const showAllDownloadTextAndButtonElem = window.MozXULElement
      .parseXULToFragment(`
    <toolbarbutton id="show-downloads-button" class="toolbarbutton-1" oncommand="BrowserDownloadsUI();" data-l10n-id="floorp-show-all-downloads" />
    `);

    document
      .getElementById("downloadsListBox")
      .appendChild(showAllDownloadTextAndButtonElem);
    document
      .getElementById("downloadsListBox")
      .appendChild(hideAllDownloadButtonElem);
  }, 1000);
}
