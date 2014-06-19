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
  * @param   aPrefixRestoreAll (defaults to false)
  *          Whether the 'restore all tabs' item is suffixed or prefixed to the list.
  *          If suffixed (the default) a separator will be inserted before it.
  * @param   aRestoreAllLabel (defaults to "menuRestoreAllTabs.label")
  *          Which localizable string to use for the 'restore all tabs' item.
  * @returns A document fragment with UI items for each recently closed tab.
  */
  getTabsFragment: function(aWindow, aTagName, aPrefixRestoreAll=false,
                            aRestoreAllLabel="menuRestoreAllTabs.label") {
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

      let restoreAllTabs = doc.createElementNS(kNSXUL, aTagName);
      restoreAllTabs.classList.add("restoreallitem");
      restoreAllTabs.setAttribute("label", navigatorBundle.GetStringFromName(aRestoreAllLabel));
      restoreAllTabs.setAttribute("oncommand",
              "for (var i = 0; i < " + closedTabs.length + "; i++) undoCloseTab();");
      if (!aPrefixRestoreAll) {
        fragment.appendChild(doc.createElementNS(kNSXUL, "menuseparator"));
        fragment.appendChild(restoreAllTabs);
      } else {
        fragment.insertBefore(restoreAllTabs, fragment.firstChild);
      }
    }
    return fragment;
  },

  /**
  * Builds up a document fragment of UI items for the recently closed windows.
  * @param   aWindow
  *          A window that can be used to create the elements and document fragment.
  * @param   aTagName
  *          The tag name that will be used when creating the UI items.
  * @param   aPrefixRestoreAll (defaults to false)
  *          Whether the 'restore all windows' item is suffixed or prefixed to the list.
  *          If suffixed (the default) a separator will be inserted before it.
  * @param   aRestoreAllLabel (defaults to "menuRestoreAllWindows.label")
  *          Which localizable string to use for the 'restore all windows' item.
  * @returns A document fragment with UI items for each recently closed window.
  */
  getWindowsFragment: function(aWindow, aTagName, aPrefixRestoreAll=false,
                            aRestoreAllLabel="menuRestoreAllWindows.label") {
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
      let restoreAllWindows = doc.createElementNS(kNSXUL, aTagName);
      restoreAllWindows.classList.add("restoreallitem");
      restoreAllWindows.setAttribute("label", navigatorBundle.GetStringFromName(aRestoreAllLabel));
      restoreAllWindows.setAttribute("oncommand",
        "for (var i = 0; i < " + closedWindowData.length + "; i++) undoCloseWindow();");
      if (!aPrefixRestoreAll) {
        fragment.appendChild(doc.createElementNS(kNSXUL, "menuseparator"));
        fragment.appendChild(restoreAllWindows);
      } else {
        fragment.insertBefore(restoreAllWindows, fragment.firstChild);
      }
    }
    return fragment;
  },


  /**
    * Re-open a closed tab and put it to the end of the tab strip.
    * Used for a middle click.
    * @param aEvent
    *        The event when the user clicks the menu item
    */
  _undoCloseMiddleClick: function(aEvent) {
    if (aEvent.button != 1)
      return;

    aEvent.view.undoCloseTab(aEvent.originalTarget.getAttribute("value"));
    aEvent.view.gBrowser.moveTabToEnd();
  },
};

function setImage(aItem, aElement) {
  let iconURL = aItem.image;
  // don't initiate a connection just to fetch a favicon (see bug 467828)
  if (/^https?:/.test(iconURL))
    iconURL = "moz-anno:favicon:" + iconURL;
  aElement.setAttribute("image", iconURL);
}
