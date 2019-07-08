/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env mozilla/browser-window */

/**
 * Utility object to handle manipulations of the protections indicators in the UI
 */
var gProtectionsHandler = {
  // smart getters
  get _protectionsPopup() {
    delete this._protectionsPopup;
    return (this._protectionsPopup = document.getElementById(
      "protections-popup"
    ));
  },
  get _protectionsIconBox() {
    delete this._protectionsIconBox;
    return (this._protectionsIconBox = document.getElementById(
      "tracking-protection-icon-animatable-box"
    ));
  },
  get _protectionsPopupMultiView() {
    delete this._protectionsPopupMultiView;
    return (this._protectionsPopupMultiView = document.getElementById(
      "protections-popup-multiView"
    ));
  },
  get _protectionsPopupMainView() {
    delete this._protectionsPopupMainView;
    return (this._protectionsPopupMainView = document.getElementById(
      "protections-popup-mainView"
    ));
  },
  get _protectionsPopupMainViewHeaderLabel() {
    delete this._protectionsPopupMainViewHeaderLabel;
    return (this._protectionsPopupMainViewHeaderLabel = document.getElementById(
      "protections-popup-mainView-panel-header-span"
    ));
  },
  get _protectionsPopupTPSwitchBreakageLink() {
    delete this._protectionsPopupTPSwitchBreakageLink;
    return (this._protectionsPopupTPSwitchBreakageLink = document.getElementById(
      "protections-popup-tp-switch-breakage-link"
    ));
  },
  get _protectionsPopupTPSwitch() {
    delete this._protectionsPopupTPSwitch;
    return (this._protectionsPopupTPSwitch = document.getElementById(
      "protections-popup-tp-switch"
    ));
  },
  get _protectionPopupSettingsButton() {
    delete this._protectionPopupSettingsButton;
    return (this._protectionPopupSettingsButton = document.getElementById(
      "protections-popup-settings-button"
    ));
  },
  get _protectionPopupFooter() {
    delete this._protectionPopupFooter;
    return (this._protectionPopupFooter = document.getElementById(
      "protections-popup-footer"
    ));
  },
  get _protectionPopupTrackersCounterDescription() {
    delete this._protectionPopupTrackersCounterDescription;
    return (this._protectionPopupTrackersCounterDescription = document.getElementById(
      "protections-popup-trackers-blocked-counter-description"
    ));
  },
  get _protectionsPopupSiteNotWorkingTPSwitch() {
    delete this._protectionsPopupSiteNotWorkingTPSwitch;
    return (this._protectionsPopupSiteNotWorkingTPSwitch = document.getElementById(
      "protections-popup-siteNotWorking-tp-switch"
    ));
  },
  get _protectionsPopupSendReportLearnMore() {
    delete this._protectionsPopupSendReportLearnMore;
    return (this._protectionsPopupSendReportLearnMore = document.getElementById(
      "protections-popup-sendReportView-learn-more"
    ));
  },
  get _protectionsPopupSendReportURL() {
    delete this._protectionsPopupSendReportURL;
    return (this._protectionsPopupSendReportURL = document.getElementById(
      "protections-popup-sendReportView-collection-url"
    ));
  },
  get _protectionsPopupToastTimeout() {
    delete this._protectionsPopupToastTimeout;
    XPCOMUtils.defineLazyPreferenceGetter(
      this,
      "_protectionsPopupToastTimeout",
      "browser.protections_panel.toast.timeout",
      5000
    );
    return this._protectionsPopupToastTimeout;
  },

  handleProtectionsButtonEvent(event) {
    event.stopPropagation();
    if (
      (event.type == "click" && event.button != 0) ||
      (event.type == "keypress" &&
        event.charCode != KeyEvent.DOM_VK_SPACE &&
        event.keyCode != KeyEvent.DOM_VK_RETURN)
    ) {
      return; // Left click, space or enter only
    }

    this.showProtectionsPopup({ event });
  },

  onPopupShown(event) {
    if (event.target == this._protectionsPopup) {
      window.addEventListener("focus", this, true);
    }
  },

  onPopupHidden(event) {
    if (event.target == this._protectionsPopup) {
      window.removeEventListener("focus", this, true);
      this._protectionsPopup.removeAttribute("open");
    }
  },

  onHeaderClicked(event) {
    // Display the whole protections panel if the toast has been clicked.
    if (this._protectionsPopup.hasAttribute("toast")) {
      // Hide the toast first.
      PanelMultiView.hidePopup(this._protectionsPopup);

      // Open the full protections panel.
      this.showProtectionsPopup({ event });
    }
  },

  onLocationChange() {
    if (!this._showToastAfterRefresh) {
      return;
    }

    this._showToastAfterRefresh = false;

    // We only display the toast if we're still on the same page.
    if (
      this._previousURI != gBrowser.currentURI.spec ||
      this._previousOuterWindowID != gBrowser.selectedBrowser.outerWindowID
    ) {
      return;
    }

    this.showProtectionsPopup({
      toast: true,
    });
  },

  handleEvent(event) {
    let elem = document.activeElement;
    let position = elem.compareDocumentPosition(this._protectionsPopup);

    if (
      !(
        position &
        (Node.DOCUMENT_POSITION_CONTAINS | Node.DOCUMENT_POSITION_CONTAINED_BY)
      ) &&
      !this._protectionsPopup.hasAttribute("noautohide")
    ) {
      // Hide the panel when focusing an element that is
      // neither an ancestor nor descendant unless the panel has
      // @noautohide (e.g. for a tour).
      PanelMultiView.hidePopup(this._protectionsPopup);
    }
  },

  refreshProtectionsPopup() {
    let host = gIdentityHandler.getHostForDisplay();

    // Push the appropriate strings out to the UI.
    this._protectionsPopupMainViewHeaderLabel.textContent =
      // gNavigatorBundle.getFormattedString("protections.header", [host]);
      `Tracking Protections for ${host}`;

    let currentlyEnabled = !this._protectionsPopup.hasAttribute("hasException");

    for (let tpSwitch of [
      this._protectionsPopupTPSwitch,
      this._protectionsPopupSiteNotWorkingTPSwitch,
    ]) {
      tpSwitch.toggleAttribute("enabled", currentlyEnabled);
    }

    // Display the breakage link according to the current enable state.
    // The display state of the breakage link will be fixed once the protections
    // panel opened no matter how the TP switch state is.
    this._protectionsPopupTPSwitchBreakageLink.hidden = !currentlyEnabled;

    // Set the counter of the 'Trackers blocked This Week'.
    // We need to get the statistics of trackers. So far, we haven't implemented
    // this yet. So we use a fake number here. Should be resolved in
    // Bug 1555231.
    this.setTrackersBlockedCounter(244051);
  },

  async onTPSwitchCommand(event) {
    // When the switch is clicked, we wait 500ms and then disable/enable
    // protections, causing the page to refresh, and close the popup.
    // We need to ensure we don't handle more clicks during the 500ms delay,
    // so we keep track of state and return early if needed.
    if (this._TPSwitchCommanding) {
      return;
    }

    this._TPSwitchCommanding = true;

    // Toggling the 'hasException' on the protections panel in order to do some
    // styling after toggling the TP switch.
    let newExceptionState = this._protectionsPopup.toggleAttribute(
      "hasException"
    );
    for (let tpSwitch of [
      this._protectionsPopupTPSwitch,
      this._protectionsPopupSiteNotWorkingTPSwitch,
    ]) {
      tpSwitch.toggleAttribute("enabled", !newExceptionState);
    }

    // Indicating that we need to show a toast after refreshing the page.
    // And caching the current URI and window ID in order to only show the mini
    // panel if it's still on the same page.
    this._showToastAfterRefresh = true;
    this._previousURI = gBrowser.currentURI.spec;
    this._previousOuterWindowID = gBrowser.selectedBrowser.outerWindowID;

    await new Promise(resolve => setTimeout(resolve, 500));

    if (newExceptionState) {
      ContentBlocking.disableForCurrentPage();
      gIdentityHandler.recordClick("unblock");
    } else {
      ContentBlocking.enableForCurrentPage();
      gIdentityHandler.recordClick("block");
    }

    PanelMultiView.hidePopup(this._protectionsPopup);
    delete this._TPSwitchCommanding;
  },

  setTrackersBlockedCounter(trackerCount) {
    this._protectionPopupTrackersCounterDescription.textContent =
      // gNavigatorBundle.getFormattedString(
      //   "protections.trackers_counter", [cnt]);
      `Trackers blocked this week: ${trackerCount.toLocaleString()}`;
  },

  /**
   * Showing the protections popup.
   *
   * @param {Object} options
   *                 The object could have two properties.
   *                 event:
   *                   The event triggers the protections popup to be opened.
   *                 toast:
   *                   A boolean to indicate if we need to open the protections
   *                   popup as a toast. A toast only has a header section and
   *                   will be hidden after a certain amount of time.
   */
  showProtectionsPopup(options = {}) {
    const { event, toast } = options;

    // We need to clear the toast timer if it exists before showing the
    // protections popup.
    if (this._toastPanelTimer) {
      clearTimeout(this._toastPanelTimer);
      delete this._toastPanelTimer;
    }

    // Make sure that the display:none style we set in xul is removed now that
    // the popup is actually needed
    this._protectionsPopup.hidden = false;

    this._protectionsPopup.toggleAttribute("toast", !!toast);
    if (!toast) {
      // Refresh strings if we want to open it as a standard protections popup.
      this.refreshProtectionsPopup();
    }

    if (toast) {
      this._protectionsPopup.addEventListener(
        "popupshown",
        () => {
          this._toastPanelTimer = setTimeout(() => {
            PanelMultiView.hidePopup(this._protectionsPopup);
            delete this._toastPanelTimer;
          }, this._protectionsPopupToastTimeout);
        },
        { once: true }
      );
    }

    // Now open the popup, anchored off the primary chrome element
    PanelMultiView.openPopup(
      this._protectionsPopup,
      gIdentityHandler._identityIcon,
      {
        position: "bottomcenter topleft",
        triggerEvent: event,
      }
    ).catch(Cu.reportError);
  },

  showSiteNotWorkingView() {
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-siteNotWorkingView"
    );
  },

  showSendReportView() {
    // Save this URI to make sure that the user really only submits the location
    // they see in the report breakage dialog.
    this.reportURI = gBrowser.currentURI;
    let urlWithoutQuery = this.reportURI.asciiSpec.replace(
      "?" + this.reportURI.query,
      ""
    );
    this._protectionsPopupSendReportURL.value = urlWithoutQuery;
    this._protectionsPopupMultiView.showSubView(
      "protections-popup-sendReportView"
    );
  },

  onSendReportClicked() {
    this._protectionsPopup.hidePopup();
    let comments = document.getElementById(
      "protections-popup-sendReportView-collection-comments"
    );
    ContentBlocking.submitBreakageReport(this.reportURI, comments);
  },
};

let baseURL = Services.urlFormatter.formatURLPref("app.support.baseURL");
gProtectionsHandler._protectionsPopupSendReportLearnMore.href =
  baseURL + "blocking-breakage";
