/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var EXPORTED_SYMBOLS = ["RecentlyClosedTabsAndWindowsMenuUtils"];

const { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

const lazy = {};

ChromeUtils.defineModuleGetter(
  lazy,
  "PluralForm",
  "resource://gre/modules/PluralForm.jsm"
);
ChromeUtils.defineModuleGetter(
  lazy,
  "SessionStore",
  "resource:///modules/sessionstore/SessionStore.jsm"
);

ChromeUtils.defineModuleGetter(
  lazy,
  "PlacesUIUtils",
  "resource:///modules/PlacesUIUtils.jsm"
);

var navigatorBundle = Services.strings.createBundle(
  "chrome://browser/locale/browser.properties"
);

var RecentlyClosedTabsAndWindowsMenuUtils = {
  /**
   * Builds up a document fragment of UI items for the recently closed tabs.
   * @param   aWindow
   *          The window that the tabs were closed in.
   * @param   aTagName
   *          The tag name that will be used when creating the UI items.
   * @param   aPrefixRestoreAll (defaults to false)
   *          Whether the 'restore all tabs' item is suffixed or prefixed to the list.
   *          If suffixed (the default) a separator will be inserted before it.
   * @param   aRestoreAllLabel (defaults to "appmenu-reopen-all-tabs")
   *          Which localizable string to use for the 'restore all tabs' item.
   * @returns A document fragment with UI items for each recently closed tab.
   */
  getTabsFragment(
    aWindow,
    aTagName,
    aPrefixRestoreAll = false,
    aRestoreAllLabel = "appmenu-reopen-all-tabs"
  ) {
    let doc = aWindow.document;
    let fragment = doc.createDocumentFragment();
    if (lazy.SessionStore.getClosedTabCount(aWindow) != 0) {
      let closedTabs = lazy.SessionStore.getClosedTabData(aWindow);
      for (let i = 0; i < closedTabs.length; i++) {
        createEntry(
          aTagName,
          false,
          i,
          closedTabs[i],
          doc,
          closedTabs[i].title,
          fragment
        );
      }

      createRestoreAllEntry(
        doc,
        fragment,
        aPrefixRestoreAll,
        false,
        aRestoreAllLabel,
        closedTabs.length,
        aTagName
      );
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
   * @param   aRestoreAllLabel (defaults to "appmenu-reopen-all-windows")
   *          Which localizable string to use for the 'restore all windows' item.
   * @returns A document fragment with UI items for each recently closed window.
   */
  getWindowsFragment(
    aWindow,
    aTagName,
    aPrefixRestoreAll = false,
    aRestoreAllLabel = "appmenu-reopen-all-windows"
  ) {
    let closedWindowData = lazy.SessionStore.getClosedWindowData();
    let doc = aWindow.document;
    let fragment = doc.createDocumentFragment();
    if (closedWindowData.length) {
      let menuLabelString = navigatorBundle.GetStringFromName(
        "menuUndoCloseWindowLabel"
      );
      let menuLabelStringSingleTab = navigatorBundle.GetStringFromName(
        "menuUndoCloseWindowSingleTabLabel"
      );

      for (let i = 0; i < closedWindowData.length; i++) {
        let undoItem = closedWindowData[i];
        let otherTabsCount = undoItem.tabs.length - 1;
        let label =
          otherTabsCount == 0
            ? menuLabelStringSingleTab
            : lazy.PluralForm.get(otherTabsCount, menuLabelString);
        let menuLabel = label
          .replace("#1", undoItem.title)
          .replace("#2", otherTabsCount);
        let selectedTab = undoItem.tabs[undoItem.selected - 1];

        createEntry(aTagName, true, i, selectedTab, doc, menuLabel, fragment);
      }

      createRestoreAllEntry(
        doc,
        fragment,
        aPrefixRestoreAll,
        true,
        aRestoreAllLabel,
        closedWindowData.length,
        aTagName
      );
    }
    return fragment;
  },

  /**
   * Re-open a closed tab and put it to the end of the tab strip.
   * Used for a middle click.
   * @param aEvent
   *        The event when the user clicks the menu item
   */
  _undoCloseMiddleClick(aEvent) {
    if (aEvent.button != 1) {
      return;
    }

    aEvent.view.undoCloseTab(aEvent.originalTarget.getAttribute("value"));
    aEvent.view.gBrowser.moveTabToEnd();
    let ancestorPanel = aEvent.target.closest("panel");
    if (ancestorPanel) {
      ancestorPanel.hidePopup();
    }
  },

  get strings() {
    delete this.strings;
    return (this.strings = new Localization(
      ["branding/brand.ftl", "browser/menubar.ftl", "browser/appmenu.ftl"],
      true
    ));
  },
};

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
function createEntry(
  aTagName,
  aIsWindowsFragment,
  aIndex,
  aClosedTab,
  aDocument,
  aMenuLabel,
  aFragment
) {
  let element = aDocument.createXULElement(aTagName);

  element.setAttribute("label", aMenuLabel);
  if (aClosedTab.image) {
    const iconURL = lazy.PlacesUIUtils.getImageURL(aClosedTab.image);
    element.setAttribute("image", iconURL);
  }
  if (!aIsWindowsFragment) {
    element.setAttribute("value", aIndex);
  }

  if (aTagName == "menuitem") {
    element.setAttribute(
      "class",
      "menuitem-iconic bookmark-item menuitem-with-favicon"
    );
  }

  element.setAttribute(
    "oncommand",
    "undoClose" + (aIsWindowsFragment ? "Window" : "Tab") + "(" + aIndex + ");"
  );

  // Set the targetURI attribute so it will be shown in tooltip.
  // SessionStore uses one-based indexes, so we need to normalize them.
  let tabData;
  tabData = aIsWindowsFragment ? aClosedTab : aClosedTab.state;
  let activeIndex = (tabData.index || tabData.entries.length) - 1;
  if (activeIndex >= 0 && tabData.entries[activeIndex]) {
    element.setAttribute("targetURI", tabData.entries[activeIndex].url);
  }

  // Windows don't open in new tabs and menuitems dispatch command events on
  // middle click, so we only need to manually handle middle clicks for
  // toolbarbuttons.
  if (!aIsWindowsFragment && aTagName != "menuitem") {
    element.addEventListener(
      "click",
      RecentlyClosedTabsAndWindowsMenuUtils._undoCloseMiddleClick
    );
  }

  if (aIndex == 0) {
    element.setAttribute(
      "key",
      "key_undoClose" + (aIsWindowsFragment ? "Window" : "Tab")
    );
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
function createRestoreAllEntry(
  aDocument,
  aFragment,
  aPrefixRestoreAll,
  aIsWindowsFragment,
  aRestoreAllLabel,
  aEntryCount,
  aTagName
) {
  let restoreAllElements = aDocument.createXULElement(aTagName);
  restoreAllElements.classList.add("restoreallitem");

  // We cannot use aDocument.l10n.setAttributes because the menubar label is not
  // updated in time and displays a blank string (see Bug 1691553).
  restoreAllElements.setAttribute(
    "label",
    RecentlyClosedTabsAndWindowsMenuUtils.strings.formatValueSync(
      aRestoreAllLabel
    )
  );

  restoreAllElements.setAttribute(
    "oncommand",
    "for (var i = 0; i < " +
      aEntryCount +
      "; i++) undoClose" +
      (aIsWindowsFragment ? "Window" : "Tab") +
      "();"
  );
  if (aPrefixRestoreAll) {
    aFragment.insertBefore(restoreAllElements, aFragment.firstChild);
  } else {
    aFragment.appendChild(aDocument.createXULElement("menuseparator"));
    aFragment.appendChild(restoreAllElements);
  }
}
