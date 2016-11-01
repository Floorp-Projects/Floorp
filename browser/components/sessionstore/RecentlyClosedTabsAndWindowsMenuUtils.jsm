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
Cu.import("resource://gre/modules/PlacesUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "PluralForm",
                                  "resource://gre/modules/PluralForm.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "SessionStore",
                                  "resource:///modules/sessionstore/SessionStore.jsm");

var navigatorBundle = Services.strings.createBundle("chrome://browser/locale/browser.properties");

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
    if (SessionStore.getClosedTabCount(aWindow) != 0) {
      let closedTabs = SessionStore.getClosedTabData(aWindow, false);
      for (let i = 0; i < closedTabs.length; i++) {
        createEntry(aTagName, false, i, closedTabs[i], doc,
                    closedTabs[i].title, fragment);
      }

    createRestoreAllEntry(doc, fragment, aPrefixRestoreAll, false,
                          aRestoreAllLabel, closedTabs.length, aTagName)
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
    let closedWindowData = SessionStore.getClosedWindowData(false);
    let doc = aWindow.document;
    let fragment = doc.createDocumentFragment();
    if (closedWindowData.length != 0) {
      let menuLabelString = navigatorBundle.GetStringFromName("menuUndoCloseWindowLabel");
      let menuLabelStringSingleTab =
        navigatorBundle.GetStringFromName("menuUndoCloseWindowSingleTabLabel");

      for (let i = 0; i < closedWindowData.length; i++) {
        let undoItem = closedWindowData[i];
        let otherTabsCount = undoItem.tabs.length - 1;
        let label = (otherTabsCount == 0) ? menuLabelStringSingleTab
                                          : PluralForm.get(otherTabsCount, menuLabelString);
        let menuLabel = label.replace("#1", undoItem.title)
                             .replace("#2", otherTabsCount);
        let selectedTab = undoItem.tabs[undoItem.selected - 1];

        createEntry(aTagName, true, i, selectedTab, doc, menuLabel,
                    fragment);
      }

      createRestoreAllEntry(doc, fragment, aPrefixRestoreAll, true,
                            aRestoreAllLabel, closedWindowData.length,
                            aTagName);
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

/**
 * Create a UI entry for a recently closed tab or window.
 * @param aTagName
 *        the tag name that will be used when creating the UI entry
 * @param aIsWindowsFragment
 *        whether or not this entry will represent a closed window
 * @param aIndex
 *        the index of the closed tab
 * @param aClosedTab
 *        the closed tab
 * @param aDocument
 *        a document that can be used to create the entry
 * @param aMenuLabel
 *        the label the created entry will have
 * @param aFragment
 *        the fragment the created entry will be in
 */
function createEntry(aTagName, aIsWindowsFragment, aIndex, aClosedTab,
                     aDocument, aMenuLabel, aFragment) {
  let element = aDocument.createElementNS(kNSXUL, aTagName);

  element.setAttribute("label", aMenuLabel);
  if (aClosedTab.image) {
    setImage(aClosedTab, element);
  }
  if (!aIsWindowsFragment) {
    element.setAttribute("value", aIndex);
  }

  if (aTagName == "menuitem") {
    element.setAttribute("class", "menuitem-iconic bookmark-item menuitem-with-favicon");
  }

  element.setAttribute("oncommand", "undoClose" + (aIsWindowsFragment ? "Window" : "Tab") +
                       "(" + aIndex + ");");

  // Set the targetURI attribute so it will be shown in tooltip.
  // SessionStore uses one-based indexes, so we need to normalize them.
  let tabData;
  tabData = aIsWindowsFragment ? aClosedTab
                     : aClosedTab.state;
  let activeIndex = (tabData.index || tabData.entries.length) - 1;
  if (activeIndex >= 0 && tabData.entries[activeIndex]) {
    element.setAttribute("targetURI", tabData.entries[activeIndex].url);
  }

  if (!aIsWindowsFragment) {
    element.addEventListener("click", RecentlyClosedTabsAndWindowsMenuUtils._undoCloseMiddleClick, false);
  }
  if (aIndex == 0) {
    element.setAttribute("key", "key_undoClose" + (aIsWindowsFragment? "Window" : "Tab"));
  }

  aFragment.appendChild(element);
}

/**
 * Create an entry to restore all closed windows or tabs.
 * @param aDocument
 *        a document that can be used to create the entry
 * @param aFragment
 *        the fragment the created entry will be in
 * @param aPrefixRestoreAll
 *        whether the 'restore all windows' item is suffixed or prefixed to the list
 *        If suffixed a separator will be inserted before it.
 * @param aIsWindowsFragment
 *        whether or not this entry will represent a closed window
 * @param aRestoreAllLabel
 *        which localizable string to use for the entry
 * @param aEntryCount
 *        the number of elements to be restored by this entry
 * @param aTagName
 *        the tag name that will be used when creating the UI entry
 */
function createRestoreAllEntry(aDocument, aFragment, aPrefixRestoreAll,
                                aIsWindowsFragment, aRestoreAllLabel,
                                aEntryCount, aTagName) {
  let restoreAllElements = aDocument.createElementNS(kNSXUL, aTagName);
  restoreAllElements.classList.add("restoreallitem");
  restoreAllElements.setAttribute("label", navigatorBundle.GetStringFromName(aRestoreAllLabel));
  restoreAllElements.setAttribute("oncommand",
                                  "for (var i = 0; i < " + aEntryCount + "; i++) undoClose" +
                                    (aIsWindowsFragment? "Window" : "Tab") + "();");
  if (aPrefixRestoreAll) {
    aFragment.insertBefore(restoreAllElements, aFragment.firstChild);
  } else {
    aFragment.appendChild(aDocument.createElementNS(kNSXUL, "menuseparator"));
    aFragment.appendChild(restoreAllElements);
  }
}