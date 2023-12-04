/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// This file is loaded into the browser window scope.
/* eslint-env mozilla/browser-window */

ChromeUtils.defineESModuleGetters(this, {
  TabsPanel: "resource:///modules/TabsList.sys.mjs",
});

var gTabsPanel = {
  kElements: {
    allTabsButton: "alltabs-button",
    allTabsView: "allTabsMenu-allTabsView",
    allTabsViewTabs: "allTabsMenu-allTabsView-tabs",
    dropIndicator: "allTabsMenu-dropIndicator",
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
    let showPinnedTabs = Services.prefs.getBoolPref(
      "browser.tabs.tabmanager.enabled"
    );
    this.allTabsPanel = new TabsPanel({
      view: this.allTabsView,
      containerNode: this.allTabsViewTabs,
      filterFn: tab =>
        !tab.hidden && (!tab.pinned || (showPinnedTabs && tab.pinned)),
      dropIndicator: this.dropIndicator,
    });

    this.allTabsView.addEventListener("ViewShowing", e => {
      PanelUI._ensureShortcutsShown(this.allTabsView);

      let containersEnabled =
        Services.prefs.getBoolPref("privacy.userContext.enabled") &&
        !PrivateBrowsingUtils.isWindowPrivate(window);
      document.getElementById("allTabsMenu-containerTabsButton").hidden =
        !containersEnabled;

      let hasHiddenTabs = gBrowser.visibleTabs.length < gBrowser.tabs.length;
      document.getElementById("allTabsMenu-hiddenTabsButton").hidden =
        !hasHiddenTabs;
      document.getElementById("allTabsMenu-hiddenTabsSeparator").hidden =
        !hasHiddenTabs;
    });

    this.allTabsView.addEventListener("ViewShown", e =>
      this.allTabsView
        .querySelector(".all-tabs-item[selected]")
        ?.scrollIntoView({ block: "center" })
    );

    let containerTabsMenuSeparator =
      this.containerTabsView.querySelector("toolbarseparator");
    this.containerTabsView.addEventListener("ViewShowing", e => {
      let elements = [];
      let frag = document.createDocumentFragment();

      ContextualIdentityService.getPublicIdentities().forEach(identity => {
        let menuitem = document.createXULElement("toolbarbutton");
        menuitem.setAttribute("class", "subviewbutton subviewbutton-iconic");
        if (identity.name) {
          menuitem.setAttribute("label", identity.name);
        } else {
          document.l10n.setAttributes(menuitem, identity.l10nId);
        }
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

  showAllTabsPanel(event, entrypoint = "unknown") {
    // Note that event may be null.

    // Only space and enter should open the popup, ignore other keypresses:
    if (event?.type == "keypress" && event.key != "Enter" && event.key != " ") {
      return;
    }
    this.init();
    if (this.canOpen) {
      Services.telemetry.keyedScalarAdd(
        "browser.ui.interaction.all_tabs_panel_entrypoint",
        entrypoint,
        1
      );
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

  showHiddenTabsPanel(event, entrypoint = "unknown") {
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
    this.showAllTabsPanel(event, entrypoint);
  },

  searchTabs() {
    gURLBar.search(UrlbarTokenizer.RESTRICT.OPENPAGE, {
      searchModeEntry: "tabmenu",
    });
  },
};
