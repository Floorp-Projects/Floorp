/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

var gPageStyleMenu = {
  _getStyleSheetInfo(browser) {
    let actor =
      browser.browsingContext.currentWindowGlobal?.getActor("PageStyle");
    let styleSheetInfo;
    if (actor) {
      styleSheetInfo = actor.getSheetInfo();
    } else {
      // Fallback if the actor is missing or we don't have a window global.
      // It's unlikely things will work well but let's be optimistic,
      // rather than throwing exceptions immediately.
      styleSheetInfo = {
        filteredStyleSheets: [],
        preferredStyleSheetSet: true,
      };
    }
    return styleSheetInfo;
  },

  fillPopup(menuPopup) {
    let styleSheetInfo = this._getStyleSheetInfo(gBrowser.selectedBrowser);
    var noStyle = menuPopup.firstElementChild;
    var persistentOnly = noStyle.nextElementSibling;
    var sep = persistentOnly.nextElementSibling;
    while (sep.nextElementSibling) {
      menuPopup.removeChild(sep.nextElementSibling);
    }

    let styleSheets = styleSheetInfo.filteredStyleSheets;
    var currentStyleSheets = {};
    var styleDisabled =
      !!gBrowser.selectedBrowser.browsingContext?.authorStyleDisabledDefault;
    var haveAltSheets = false;
    var altStyleSelected = false;

    for (let currentStyleSheet of styleSheets) {
      if (!currentStyleSheet.disabled) {
        altStyleSelected = true;
      }

      haveAltSheets = true;

      let lastWithSameTitle = null;
      if (currentStyleSheet.title in currentStyleSheets) {
        lastWithSameTitle = currentStyleSheets[currentStyleSheet.title];
      }

      if (!lastWithSameTitle) {
        let menuItem = document.createXULElement("menuitem");
        menuItem.setAttribute("type", "radio");
        menuItem.setAttribute("label", currentStyleSheet.title);
        menuItem.setAttribute("data", currentStyleSheet.title);
        menuItem.setAttribute(
          "checked",
          !currentStyleSheet.disabled && !styleDisabled
        );
        menuItem.setAttribute(
          "oncommand",
          "gPageStyleMenu.switchStyleSheet(this.getAttribute('data'));"
        );
        menuPopup.appendChild(menuItem);
        currentStyleSheets[currentStyleSheet.title] = menuItem;
      } else if (currentStyleSheet.disabled) {
        lastWithSameTitle.removeAttribute("checked");
      }
    }

    noStyle.setAttribute("checked", styleDisabled);
    persistentOnly.setAttribute("checked", !altStyleSelected && !styleDisabled);
    persistentOnly.hidden = styleSheetInfo.preferredStyleSheetSet
      ? haveAltSheets
      : false;
    sep.hidden = (noStyle.hidden && persistentOnly.hidden) || !haveAltSheets;
  },

  /**
   * Send a message to all PageStyleParents by walking the BrowsingContext tree.
   * @param message
   *        The string message to send to each PageStyleChild.
   * @param data
   *        The data to send to each PageStyleChild within the message.
   */
  _sendMessageToAll(message, data) {
    let contextsToVisit = [gBrowser.selectedBrowser.browsingContext];
    while (contextsToVisit.length) {
      let currentContext = contextsToVisit.pop();
      let global = currentContext.currentWindowGlobal;

      if (!global) {
        continue;
      }

      let actor = global.getActor("PageStyle");
      actor.sendAsyncMessage(message, data);

      contextsToVisit.push(...currentContext.children);
    }
  },

  /**
   * Switch the stylesheet of all documents in the current browser.
   * @param title The title of the stylesheet to switch to.
   */
  switchStyleSheet(title) {
    let sheetData = this._getStyleSheetInfo(gBrowser.selectedBrowser);
    for (let sheet of sheetData.filteredStyleSheets) {
      sheet.disabled = sheet.title !== title;
    }
    this._sendMessageToAll("PageStyle:Switch", { title });
  },

  /**
   * Disable all stylesheets. Called with View > Page Style > No Style.
   */
  disableStyle() {
    this._sendMessageToAll("PageStyle:Disable", {});
  },
};
