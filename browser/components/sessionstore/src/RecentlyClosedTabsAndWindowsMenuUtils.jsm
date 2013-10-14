/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

this.EXPORTED_SYMBOLS = ["RecentlyClosedTabsAndWindowsMenuUtils"];

const kNSXUL = "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul";

var Ci = Components.interfaces;
var Cc = Components.classes;
var Cr = Components.results;
var Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");

let navigatorBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");
let ss = Cc["@mozilla.org/browser/sessionstore;1"].getService(Ci.nsISessionStore);

this.RecentlyClosedTabsAndWindowsMenuUtils = {

  /**
  * Builds up a document fragment of UI items for the recently closed tabs.
  * @param   aWindow
  *          The window that the tabs were closed in.
  * @param   aTagName
  *          The tag name that will be used when creating the UI items.
  * @returns A document fragment with UI items for each recently closed tab.
  */
  getTabsFragment: function(aWindow, aTagName) {
    let doc = aWindow.document;
    let fragment = doc.createDocumentFragment();
    if (ss.getClosedTabCount(aWindow) != 0) {
      let closedTabs = JSON.parse(ss.getClosedTabData(aWindow));
      for (let i = 0; i < closedTabs.length; i++) {
        let element = doc.createElementNS(kNSXUL, aTagName);
        element.setAttribute("label", closedTabs[i].title);
        if (closedTabs[i].image) {
          setImage(closedTabs[i], element);
        }
        element.setAttribute("value", i);
        if (aTagName == "menuitem") {
          element.setAttribute("class", "menuitem-iconic bookmark-item menuitem-with-favicon");
        }
        element.setAttribute("oncommand", "undoCloseTab(" + i + ");");

        // Set the targetURI attribute so it will be shown in tooltip and trigger
        // onLinkHovered. SessionStore uses one-based indexes, so we need to
        // normalize them.
        let tabData = closedTabs[i].state;
        let activeIndex = (tabData.index || tabData.entries.length) - 1;
        if (activeIndex >= 0 && tabData.entries[activeIndex]) {
          element.setAttribute("targetURI", tabData.entries[activeIndex].url);
        }

        element.addEventListener("click", this._undoCloseMiddleClick, false);
        if (i == 0)
          element.setAttribute("key", "key_undoCloseTab");
        fragment.appendChild(element);
      }

      fragment.appendChild(doc.createElementNS(kNSXUL, "menuseparator"));
      let restoreAllTabs = fragment.appendChild(doc.createElementNS(kNSXUL, aTagName));
      restoreAllTabs.setAttribute("label", navigatorBundle.GetStringFromName("menuRestoreAllTabs.label"));
      restoreAllTabs.addEventListener("command", function() {
        for (var i = 0; i < closedTabs.length; i++)
          undoCloseTab(0);
      }, false);
    }
    return fragment;
  },

  /**
  * Builds up a document fragment of UI items for the recently closed windows.
  * @param   aWindow
  *          A window that can be used to create the elements and document fragment.
  * @param   aTagName
  *          The tag name that will be used when creating the UI items.
  * @returns A document fragment with UI items for each recently closed window.
  */
  getWindowsFragment: function(aWindow, aTagName) {
    let closedWindowData = JSON.parse(ss.getClosedWindowData());
    let fragment = aWindow.document.createDocumentFragment();
    if (closedWindowData.length != 0) {
      let menuLabelString = navigatorBundle.GetStringFromName("menuUndoCloseWindowLabel");
      let menuLabelStringSingleTab =
        navigatorBundle.GetStringFromName("menuUndoCloseWindowSingleTabLabel");

      let doc = aWindow.document;
      for (let i = 0; i < closedWindowData.length; i++) {
        let undoItem = closedWindowData[i];
        let otherTabsCount = undoItem.tabs.length - 1;
        let label = (otherTabsCount == 0) ? menuLabelStringSingleTab
                                          : PluralForm.get(otherTabsCount, menuLabelString);
        let menuLabel = label.replace("#1", undoItem.title)
                             .replace("#2", otherTabsCount);
        let item = doc.createElementNS(kNSXUL, aTagName);
        item.setAttribute("label", menuLabel);
        let selectedTab = undoItem.tabs[undoItem.selected - 1];
        if (selectedTab.image) {
          setImage(selectedTab, item);
        }
        if (aTagName == "menuitem") {
          item.setAttribute("class", "menuitem-iconic bookmark-item menuitem-with-favicon");
        }
        item.setAttribute("oncommand", "undoCloseWindow(" + i + ");");

        // Set the targetURI attribute so it will be shown in tooltip.
        // SessionStore uses one-based indexes, so we need to normalize them.
        let activeIndex = (selectedTab.index || selectedTab.entries.length) - 1;
        if (activeIndex >= 0 && selectedTab.entries[activeIndex])
          item.setAttribute("targetURI", selectedTab.entries[activeIndex].url);

        if (i == 0)
          item.setAttribute("key", "key_undoCloseWindow");
        fragment.appendChild(item);
      }

      // "Open All in Windows"
      fragment.appendChild(doc.createElementNS(kNSXUL, "menuseparator"));
      let restoreAllWindows = fragment.appendChild(doc.createElementNS(kNSXUL, aTagName));
      restoreAllWindows.setAttribute("label", navigatorBundle.GetStringFromName("menuRestoreAllWindows.label"));
      restoreAllWindows.setAttribute("oncommand",
        "for (var i = 0; i < " + closedWindowData.length + "; i++) undoCloseWindow();");
    }
    return fragment;
  },
};

function setImage(aItem, aElement) {
  let iconURL = aItem.image;
  // don't initiate a connection just to fetch a favicon (see bug 467828)
  if (/^https?:/.test(iconURL))
    iconURL = "moz-anno:favicon:" + iconURL;
  aElement.setAttribute("image", iconURL);
}
