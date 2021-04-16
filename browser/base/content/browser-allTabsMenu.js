/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.defineModuleGetter(
  this,
  "TabsPanel",
  "resource:///modules/TabsList.jsm"
);

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
  _initializedElements: false,

  initElements() {
    if (this._initializedElements) {
      return;
    }
    let template = document.getElementById("allTabsMenu-container");
    template.replaceWith(template.content);

    for (let [name, id] of Object.entries(this.kElements)) {
      this[name] = document.getElementById(id);
    }
    this._initializedElements = true;
  },

  init() {
    if (this._initialized) {
      return;
    }

    this.initElements();

    this.hiddenAudioTabsPopup = new TabsPanel({
      view: this.allTabsView,
      insertBefore: document.getElementById("allTabsMenu-tabsSeparator"),
      filterFn: tab => tab.hidden && tab.soundPlaying,
    });
    this.allTabsPanel = new TabsPanel({
      view: this.allTabsView,
      containerNode: this.allTabsViewTabs,
      filterFn: tab => !tab.pinned && !tab.hidden,
    });

    this.allTabsView.addEventListener("ViewShowing", e => {
      PanelUI._ensureShortcutsShown(this.allTabsView);

      let containersEnabled =
        Services.prefs.getBoolPref("privacy.userContext.enabled") &&
        !PrivateBrowsingUtils.isWindowPrivate(window);
      document.getElementById(
        "allTabsMenu-containerTabsButton"
      ).hidden = !containersEnabled;

      let hasHiddenTabs = gBrowser.visibleTabs.length < gBrowser.tabs.length;
      document.getElementById(
        "allTabsMenu-hiddenTabsButton"
      ).hidden = !hasHiddenTabs;
      document.getElementById(
        "allTabsMenu-hiddenTabsSeparator"
      ).hidden = !hasHiddenTabs;
    });

    this.allTabsView.addEventListener("ViewShown", e => {
      let selectedRow = this.allTabsView.querySelector(
        ".all-tabs-item[selected]"
      );
      selectedRow.scrollIntoView({ block: "center" });
    });

    let containerTabsMenuSeparator = this.containerTabsView.querySelector(
      "toolbarseparator"
    );
    this.containerTabsView.addEventListener("ViewShowing", e => {
      let elements = [];
      let frag = document.createDocumentFragment();

      ContextualIdentityService.getPublicIdentities().forEach(identity => {
        let menuitem = document.createXULElement("toolbarbutton");
        menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
        menuitem.setAttribute(
          "label",
          ContextualIdentityService.getUserContextLabel(identity.userContextId)
        );
        // The styles depend on this.
        menuitem.setAttribute("usercontextid", identity.userContextId);
        // The command handler depends on this.
        menuitem.setAttribute("data-usercontextid", identity.userContextId);
        menuitem.classList.add("identity-icon-" + identity.icon);
        menuitem.classList.add("identity-color-" + identity.color);

        menuitem.setAttribute("command", "Browser:NewUserContextTab");

        frag.appendChild(menuitem);
        elements.push(menuitem);
      });

      e.target.addEventListener(
        "ViewHiding",
        () => {
          for (let element of elements) {
            element.remove();
          }
        },
        { once: true }
      );
      containerTabsMenuSeparator.parentNode.insertBefore(
        frag,
        containerTabsMenuSeparator
      );
    });

    this.hiddenTabsPopup = new TabsPanel({
      view: this.hiddenTabsView,
      filterFn: tab => tab.hidden,
    });

    this._initialized = true;
  },

  get canOpen() {
    this.initElements();
    return isElementVisible(this.allTabsButton);
  },

  showAllTabsPanel(event) {
    this.init();
    if (this.canOpen) {
      if (!gProton) {
        PanelMultiView.getViewNode(
          document,
          "allTabsMenu-searchTabs"
        ).classList.add("subviewbutton-iconic");
      }
      PanelUI.showSubView(
        this.kElements.allTabsView,
        this.allTabsButton,
        event
      );
    }
  },

  hideAllTabsPanel() {
    if (this.allTabsView) {
      PanelMultiView.hidePopup(this.allTabsView.closest("panel"));
    }
  },

  showHiddenTabsPanel(event) {
    this.init();
    if (!this.canOpen) {
      return;
    }
    this.allTabsView.addEventListener(
      "ViewShown",
      e => {
        PanelUI.showSubView(
          this.kElements.hiddenTabsView,
          this.hiddenTabsButton
        );
      },
      { once: true }
    );
    this.showAllTabsPanel(event);
  },

  searchTabs() {
    gURLBar.search(UrlbarTokenizer.RESTRICT.OPENPAGE, {
      searchModeEntry: "tabmenu",
    });
  },
};
