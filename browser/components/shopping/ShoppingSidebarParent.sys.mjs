/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};
ChromeUtils.defineESModuleGetters(lazy, {
  BrowserWindowTracker: "resource:///modules/BrowserWindowTracker.sys.mjs",
  EveryWindow: "resource:///modules/EveryWindow.sys.mjs",
  isProductURL: "chrome://global/content/shopping/ShoppingProduct.mjs",
  NimbusFeatures: "resource://nimbus/ExperimentAPI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  ShoppingUtils: "resource:///modules/ShoppingUtils.sys.mjs",
});

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

export class ShoppingSidebarParent extends JSWindowActorParent {
  static SHOPPING_ACTIVE_PREF = "browser.shopping.experience2023.active";
  static SHOPPING_OPTED_IN_PREF = "browser.shopping.experience2023.optedIn";

  updateProductURL(uri, flags) {
    this.sendAsyncMessage("ShoppingSidebar:UpdateProductURL", {
      url: uri?.spec ?? null,
      isReload: !!(flags & Ci.nsIWebProgressListener.LOCATION_CHANGE_RELOAD),
    });
  }

  async receiveMessage(message) {
    switch (message.name) {
      case "GetProductURL":
        let sidebarBrowser = this.browsingContext.top.embedderElement;
        let panel = sidebarBrowser.closest(".browserSidebarContainer");
        let associatedTabbedBrowser = panel.querySelector(
          "browser[messagemanagergroup=browsers]"
        );
        return associatedTabbedBrowser.currentURI?.spec ?? null;
    }
    return null;
  }

  /**
   * Called when the user clicks the URL bar button.
   */
  static urlbarButtonClick(event) {
    if (event.button > 0) {
      return;
    }
    this.toggleAllSidebars("urlBar");
  }

  /**
   * Toggles opening or closing all Shopping sidebars.
   * Sets the active pref value for all windows to respond to.
   * params:
   *
   *  @param {string?} source
   *  Optional value, describes where the call came from.
   */
  static toggleAllSidebars(source) {
    let activeState = Services.prefs.getBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF
    );
    Services.prefs.setBoolPref(
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF,
      !activeState
    );

    let optedIn = Services.prefs.getIntPref(
      ShoppingSidebarParent.SHOPPING_OPTED_IN_PREF
    );
    // If the user was opted out, then clicked the button, reset the optedIn
    // pref so they see onboarding.
    if (optedIn == 2) {
      Services.prefs.setIntPref(
        ShoppingSidebarParent.SHOPPING_OPTED_IN_PREF,
        0
      );
    }
    if (source == "urlBar") {
      if (activeState) {
        Glean.shopping.surfaceClosed.record({ source: "addressBarIcon" });
        Glean.shopping.addressBarIconClicked.record({ action: "closed" });
      } else {
        Glean.shopping.addressBarIconClicked.record({ action: "opened" });
      }
    }
  }
}

class ShoppingSidebarManagerClass {
  #initialized = false;
  #everyWindowCallbackId = `shopping-${Services.uuid.generateUUID()}`;

  ensureInitialized() {
    if (this.#initialized) {
      return;
    }

    this.updateSidebarVisibility = this.updateSidebarVisibility.bind(this);
    lazy.NimbusFeatures.shopping2023.onUpdate(this.updateSidebarVisibility);
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "optedInPref",
      "browser.shopping.experience2023.optedIn",
      null,
      this.updateSidebarVisibility
    );
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "isActive",
      ShoppingSidebarParent.SHOPPING_ACTIVE_PREF,
      true,
      this.updateSidebarVisibility
    );
    this.updateSidebarVisibility();

    lazy.EveryWindow.registerCallback(
      this.#everyWindowCallbackId,
      window => {
        let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(window);
        if (isPBM) {
          return;
        }

        window.gBrowser.tabContainer.addEventListener("TabSelect", this);
        window.addEventListener("visibilitychange", this);
      },
      window => {
        let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(window);
        if (isPBM) {
          return;
        }

        window.gBrowser.tabContainer.removeEventListener("TabSelect", this);
        window.removeEventListener("visibilitychange", this);
      }
    );

    this.#initialized = true;
  }

  updateSidebarVisibility() {
    this.enabled = lazy.NimbusFeatures.shopping2023.getVariable("enabled");

    for (let window of lazy.BrowserWindowTracker.orderedWindows) {
      this.updateSidebarVisibilityForWindow(window);
    }
  }

  updateSidebarVisibilityForWindow(window) {
    if (window.closed) {
      return;
    }

    let document = window.document;

    if (!this.isActive) {
      document.querySelectorAll("shopping-sidebar").forEach(sidebar => {
        sidebar.hidden = true;
      });
    }

    this._maybeToggleButton(window.gBrowser);

    if (!this.enabled) {
      document.querySelectorAll("shopping-sidebar").forEach(sidebar => {
        sidebar.remove();
      });
      return;
    }

    let { selectedBrowser, currentURI } = window.gBrowser;
    this._maybeToggleSidebar(selectedBrowser, currentURI, 0);
  }

  _maybeToggleSidebar(aBrowser, aLocationURI, aFlags) {
    let gBrowser = aBrowser.getTabBrowser();
    let document = aBrowser.ownerDocument;
    if (!this.enabled) {
      return;
    }

    let browserPanel = gBrowser.getPanel(aBrowser);
    let sidebar = browserPanel.querySelector("shopping-sidebar");
    let actor;
    if (sidebar) {
      let { browsingContext } = sidebar.querySelector("browser");
      let global = browsingContext.currentWindowGlobal;
      actor = global.getExistingActor("ShoppingSidebar");
    }
    let isProduct = lazy.isProductURL(aLocationURI);
    if (isProduct && this.isActive) {
      if (!sidebar) {
        sidebar = document.createXULElement("shopping-sidebar");
        sidebar.hidden = false;
        let splitter = document.createXULElement("splitter");
        splitter.classList.add("sidebar-splitter");
        browserPanel.appendChild(splitter);
        browserPanel.appendChild(sidebar);
      } else {
        actor?.updateProductURL(aLocationURI, aFlags);
        sidebar.hidden = false;
      }
    } else if (sidebar && !sidebar.hidden) {
      actor?.updateProductURL(null);
      sidebar.hidden = true;
    }

    this._updateBCActiveness(aBrowser);
    this._setShoppingButtonState(aBrowser);

    if (
      sidebar &&
      !sidebar.hidden &&
      lazy.ShoppingUtils.isProductPageNavigation(aLocationURI, aFlags)
    ) {
      Glean.shopping.surfaceDisplayed.record();
    }

    if (isProduct) {
      // This is the auto-enable behavior that toggles the `active` pref. It
      // must be at the end of this function, or 2 sidebars could be created.
      lazy.ShoppingUtils.handleAutoActivateOnProduct();

      if (!this.isActive) {
        lazy.ShoppingUtils.sendTrigger({
          browser: aBrowser,
          id: "shoppingProductPageWithSidebarClosed",
          context: { isSidebarClosing: !!sidebar },
        });
      }
    }
  }

  _maybeToggleButton(gBrowser) {
    let optedOut = this.optedInPref === 2;
    if (this.enabled && optedOut) {
      this._setShoppingButtonState(gBrowser.selectedBrowser);
    }
  }

  _updateBCActiveness(aBrowser) {
    let gBrowser = aBrowser.getTabBrowser();
    let document = aBrowser.ownerDocument;
    let browserPanel = gBrowser.getPanel(aBrowser);
    let sidebar = browserPanel.querySelector("shopping-sidebar");
    if (!sidebar) {
      return;
    }
    let { browsingContext } = sidebar.querySelector("browser");
    try {
      // Tell Gecko when the sidebar visibility changes to avoid background
      // sidebars taking more CPU / energy than needed.
      browsingContext.isActive =
        !document.hidden &&
        aBrowser == gBrowser.selectedBrowser &&
        !sidebar.hidden;
    } catch (ex) {
      // The setter can throw and we do need to run the rest of this
      // code in that case.
      console.error(ex);
    }
  }

  _setShoppingButtonState(aBrowser) {
    let gBrowser = aBrowser.getTabBrowser();
    let document = aBrowser.ownerDocument;
    if (aBrowser !== gBrowser.selectedBrowser) {
      return;
    }

    let button = document.getElementById("shopping-sidebar-button");

    let isCurrentBrowserProduct = lazy.isProductURL(
      gBrowser.selectedBrowser.currentURI
    );

    // Only record if the state of the icon will change from hidden to visible.
    if (button.hidden && isCurrentBrowserProduct) {
      Glean.shopping.addressBarIconDisplayed.record();
    }

    button.hidden = !isCurrentBrowserProduct;
    button.setAttribute("shoppingsidebaropen", !!this.isActive);
    let l10nId = this.isActive
      ? "shopping-sidebar-close-button2"
      : "shopping-sidebar-open-button2";
    document.l10n.setAttributes(button, l10nId);
  }

  /**
   * Called by TabsProgressListener whenever any browser navigates from one
   * URL to another.
   * Note that this includes hash changes / pushState navigations, because
   * those can be significant for us.
   */
  onLocationChange(aBrowser, aLocationURI, aFlags) {
    let isPBM = lazy.PrivateBrowsingUtils.isWindowPrivate(aBrowser.ownerGlobal);
    if (isPBM) {
      return;
    }

    lazy.ShoppingUtils.maybeRecordExposure(aLocationURI, aFlags);

    this._maybeToggleButton(aBrowser.getTabBrowser());
    this._maybeToggleSidebar(aBrowser, aLocationURI, aFlags);
  }

  handleEvent(event) {
    switch (event.type) {
      case "TabSelect": {
        if (!this.enabled) {
          return;
        }
        this.updateSidebarVisibility();
        if (event.detail?.previousTab.linkedBrowser) {
          this._updateBCActiveness(event.detail.previousTab.linkedBrowser);
        }
        break;
      }
      case "visibilitychange": {
        if (!this.enabled) {
          return;
        }
        this.updateSidebarVisibilityForWindow(event.target.ownerGlobal.top);
        this._updateBCActiveness(
          event.target.ownerGlobal.top.gBrowser.selectedBrowser
        );
      }
    }
  }
}

const ShoppingSidebarManager = new ShoppingSidebarManagerClass();
export { ShoppingSidebarManager };
