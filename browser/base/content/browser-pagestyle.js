/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

var gPageStyleMenu = {
  // This maps from a <browser> element (or, more specifically, a
  // browser's permanentKey) to an Object that contains the most recent
  // information about the browser content's stylesheets. That Object
  // is populated via the PageStyle:StyleSheets message from the content
  // process. The Object should have the following structure:
  //
  // filteredStyleSheets (Array):
  //   An Array of objects with a filtered list representing all stylesheets
  //   that the current page offers. Each object has the following members:
  //
  //   title (String):
  //     The title of the stylesheet
  //
  //   disabled (bool):
  //     Whether or not the stylesheet is currently applied
  //
  //   href (String):
  //     The URL of the stylesheet. Stylesheets loaded via a data URL will
  //     have this property set to null.
  //
  // authorStyleDisabled (bool):
  //   Whether or not the user currently has "No Style" selected for
  //   the current page.
  //
  // preferredStyleSheetSet (bool):
  //   Whether or not the user currently has the "Default" style selected
  //   for the current page.
  //
  _pageStyleSheets: new WeakMap(),

  /**
   * Add/append styleSheets to the _pageStyleSheets weakmap.
   * @param styleSheets
   *        The stylesheets to add, including the preferred
   *        stylesheet set for this document.
   * @param permanentKey
   *        The permanent key of the browser that
   *        these stylesheets come from.
   */
  addBrowserStyleSheets(styleSheets, permanentKey) {
    let sheetData = this._pageStyleSheets.get(permanentKey);
    if (!sheetData) {
      this._pageStyleSheets.set(permanentKey, styleSheets);
      return;
    }
    sheetData.filteredStyleSheets.push(...styleSheets.filteredStyleSheets);
    sheetData.preferredStyleSheetSet =
      sheetData.preferredStyleSheetSet || styleSheets.preferredStyleSheetSet;
  },

  clearBrowserStyleSheets(permanentKey) {
    this._pageStyleSheets.delete(permanentKey);
  },

  _getStyleSheetInfo(browser) {
    let data = this._pageStyleSheets.get(browser.permanentKey);
    if (!data) {
      return {
        filteredStyleSheets: [],
        authorStyleDisabled: false,
        preferredStyleSheetSet: true,
      };
    }

    return data;
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
    var styleDisabled = styleSheetInfo.authorStyleDisabled;
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
    let { permanentKey } = gBrowser.selectedBrowser;
    let sheetData = this._pageStyleSheets.get(permanentKey);
    if (sheetData && sheetData.filteredStyleSheets) {
      sheetData.authorStyleDisabled = false;
      for (let sheet of sheetData.filteredStyleSheets) {
        sheet.disabled = sheet.title !== title;
      }
    }
    this._sendMessageToAll("PageStyle:Switch", { title });
  },

  /**
   * Disable all stylesheets. Called with View > Page Style > No Style.
   */
  disableStyle() {
    let { permanentKey } = gBrowser.selectedBrowser;
    let sheetData = this._pageStyleSheets.get(permanentKey);
    if (sheetData) {
      sheetData.authorStyleDisabled = true;
    }
    this._sendMessageToAll("PageStyle:Disable", {});
  },
};
