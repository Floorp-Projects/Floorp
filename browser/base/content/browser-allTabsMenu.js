/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.defineModuleGetter(this, "TabsPanel",
                               "resource:///modules/TabsList.jsm");

var gTabsPanel = {
  kElements: {
    allTabsButton: "alltabs-button",
    allTabsView: "allTabsMenu-allTabsView",
    allTabsViewTabs: "allTabsMenu-allTabsViewTabs",
    containerTabsView: "allTabsMenu-containerTabsView",
    hiddenTabsButton: "allTabsMenu-hiddenTabsButton",
    hiddenTabsView: "allTabsMenu-hiddenTabsView",
  },
  _initialized: false,

  initElements() {
    for (let [name, id] of Object.entries(this.kElements)) {
      this[name] = document.getElementById(id);
    }
  },

  init() {
    if (this._initialized) return;

    this.initElements();

    let hiddenTabsMenuButton = this.allTabsView.querySelector(".hidden-tabs-button");
    let hiddenTabsSeparator = this.allTabsView.querySelector(".hidden-tabs-separator");
    this.hiddenAudioTabsPopup = new TabsPanel({
      view: this.allTabsView,
      insertBefore: hiddenTabsSeparator,
      filterFn: (tab) => tab.hidden && tab.soundPlaying,
    });
    this.allTabsPanel = new TabsPanel({
      view: this.allTabsView,
      containerNode: this.allTabsViewTabs,
      filterFn: (tab) => !tab.pinned && !tab.hidden,
      onPopulate() {
        // Set the visible tab status.
        let tabContainer = gBrowser.tabContainer;
        // We don't want menu item decoration unless there is overflow.
        if (tabContainer.getAttribute("overflow") != "true") {
          return;
        }

        let windowUtils = window.QueryInterface(Ci.nsIInterfaceRequestor)
                                .getInterface(Ci.nsIDOMWindowUtils);
        let arrowScrollboxRect = windowUtils.getBoundsWithoutFlushing(tabContainer.arrowScrollbox);
        for (let row of this.rows) {
          let curTabRect = windowUtils.getBoundsWithoutFlushing(row.tab);
          if (curTabRect.left >= arrowScrollboxRect.left &&
              curTabRect.right <= arrowScrollboxRect.right) {
            row.setAttribute("tabIsVisible", "true");
          } else {
            row.removeAttribute("tabIsVisible");
          }
        }
      },
    });

    let containerTabsButton = this.allTabsView.querySelector(".container-tabs-button");
    let containerTabsSeparator = this.allTabsView.querySelector(".container-tabs-separator");
    this.allTabsView.addEventListener("ViewShowing", (e) => {
      PanelUI._ensureShortcutsShown(this.allTabsView);
      e.target.querySelector(".undo-close-tab").disabled =
          SessionStore.getClosedTabCount(window) == 0;

      let containersEnabled = Services.prefs.getBoolPref("privacy.userContext.enabled")
                                && !PrivateBrowsingUtils.isWindowPrivate(window);
      containerTabsButton.hidden = !containersEnabled;
      containerTabsSeparator.hidden = !containersEnabled;

      let hasHiddenTabs = gBrowser.visibleTabs.length < gBrowser.tabs.length;
      hiddenTabsMenuButton.hidden = !hasHiddenTabs;
      hiddenTabsSeparator.hidden = !hasHiddenTabs;
    });

    this.allTabsView.addEventListener("ViewShown", (e) => {
      let selectedRow = this.allTabsView.querySelector(".all-tabs-item[selected]");
      selectedRow.scrollIntoView({block: "center"});
    });

    let containerTabsMenuSeparator = this.containerTabsView.querySelector("toolbarseparator");
    this.containerTabsView.addEventListener("ViewShowing", (e) => {
      let elements = [];
      let frag = document.createDocumentFragment();

      ContextualIdentityService.getPublicIdentities().forEach(identity => {
        let menuitem = document.createElement("toolbarbutton");
        menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
        menuitem.setAttribute("label", ContextualIdentityService.getUserContextLabel(identity.userContextId));
        // The styles depend on this.
        menuitem.setAttribute("usercontextid", identity.userContextId);
        // The command handler depends on this.
        menuitem.setAttribute("data-usercontextid", identity.userContextId);
        menuitem.setAttribute("data-identity-color", identity.color);
        menuitem.setAttribute("data-identity-icon", identity.icon);

        menuitem.setAttribute("command", "Browser:NewUserContextTab");

        frag.appendChild(menuitem);
        elements.push(menuitem);
      });

      e.target.addEventListener("ViewHiding", () => {
        for (let element of elements) {
          element.remove();
        }
      }, {once: true});
      containerTabsMenuSeparator.parentNode.insertBefore(frag, containerTabsMenuSeparator);
    });

    this.hiddenTabsPopup = new TabsPanel({
      view: this.hiddenTabsView,
      filterFn: (tab) => tab.hidden,
    });

    this._initialized = true;
  },

  showAllTabsPanel() {
    this.init();
    PanelUI.showSubView(this.kElements.allTabsView, this.allTabsButton);
  },

  hideAllTabsPanel() {
    this.init();
    PanelMultiView.hidePopup(this.allTabsView.closest("panel"));
  },

  showHiddenTabsPanel() {
    this.init();
    this.allTabsView.addEventListener("ViewShown", (e) => {
      PanelUI.showSubView(this.kElements.hiddenTabsView, this.hiddenTabsButton);
    }, {once: true});
    this.showAllTabsPanel();
  },
};
