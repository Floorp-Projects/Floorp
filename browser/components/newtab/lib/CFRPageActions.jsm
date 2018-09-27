/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {Localization} = ChromeUtils.import("resource://gre/modules/Localization.jsm", {});
ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");
ChromeUtils.import("resource://gre/modules/Services.jsm");
XPCOMUtils.defineLazyGlobalGetters(this, ["fetch"]);

ChromeUtils.defineModuleGetter(this, "PrivateBrowsingUtils",
  "resource://gre/modules/PrivateBrowsingUtils.jsm");

const POPUP_NOTIFICATION_ID = "contextual-feature-recommendation";
const SUMO_BASE_URL = Services.urlFormatter.formatURLPref("app.support.baseURL");
const ADDONS_API_URL = "https://services.addons.mozilla.org/api/v3/addons/addon";

const DELAY_BEFORE_EXPAND_MS = 1000;

/**
 * A WeakMap from browsers to {host, recommendation} pairs. Recommendations are
 * defined in the ExtensionDoorhanger.schema.json.
 *
 * A recommendation is specific to a browser and host and is active until the
 * given browser is closed or the user navigates (within that browser) away from
 * the host.
 */
let RecommendationMap = new WeakMap();

/**
 * A WeakMap from windows to their CFR PageAction.
 */
let PageActionMap = new WeakMap();

/**
 * We need one PageAction for each window
 */
class PageAction {
  constructor(win, dispatchToASRouter) {
    this.window = win;
    this.urlbar = win.document.getElementById("urlbar");
    this.container = win.document.getElementById("contextual-feature-recommendation");
    this.button = win.document.getElementById("cfr-button");
    this.label = win.document.getElementById("cfr-label");

    // This should NOT be use directly to dispatch message-defined actions attached to buttons.
    // Please use dispatchUserAction instead.
    this._dispatchToASRouter = dispatchToASRouter;

    this._popupStateChange = this._popupStateChange.bind(this);
    this._collapse = this._collapse.bind(this);
    this._handleClick = this._handleClick.bind(this);
    this.dispatchUserAction = this.dispatchUserAction.bind(this);

    this._l10n = new Localization([
      "browser/newtab/asrouter.ftl"
    ]);

    // Saved timeout IDs for scheduled state changes, so they can be cancelled
    this.stateTransitionTimeoutIDs = [];

    this.container.onclick = this._handleClick;
  }

  _dispatchImpression(message) {
    this._dispatchToASRouter({type: "IMPRESSION", data: message});
  }

  _sendTelemetry(ping) {
    // Note `useClientID` is set to true to tell TelemetryFeed to use client_id
    // instead of `impression_id`. TelemetryFeed is also responsible for deciding
    // whether to use `message_id` or `bucket_id` based on the release channel and
    // shield study setup.
    this._dispatchToASRouter({
      type: "DOORHANGER_TELEMETRY",
      data: {useClientID: true, action: "cfr_user_event", source: "CFR", ...ping}
    });
  }

  async show(recommendation, shouldExpand = false) {
    this.container.hidden = false;

    this.label.value = await this.getStrings(recommendation.content.notification_text);

    // Wait for layout to flush to avoid a synchronous reflow then calculate the
    // label width. We can safely get the width even though the recommendation is
    // collapsed; the label itself remains full width (with its overflow hidden)
    await this.window.promiseDocumentFlushed;
    const [{width}] = this.label.getClientRects();
    this.urlbar.style.setProperty("--cfr-label-width", `${width}px`);

    if (shouldExpand) {
      this._clearScheduledStateChanges();

      // After one second, expand
      this._expand(DELAY_BEFORE_EXPAND_MS);

      this._dispatchImpression(recommendation);
      // Only send an impression ping upon the first expansion.
      // Note that when the user clicks on the "show" button on the asrouter admin
      // page (both `bucket_id` and `id` will be set as null), we don't want to send
      // the impression ping in that case.
      if (!!recommendation.id && !!recommendation.content.bucket_id) {
        this._sendTelemetry({message_id: recommendation.id, bucket_id: recommendation.content.bucket_id, event: "IMPRESSION"});
      }
    }
  }

  hide() {
    this.container.hidden = true;
    this._clearScheduledStateChanges();
    this.urlbar.removeAttribute("cfr-recommendation-state");
    if (this.currentNotification) {
      this.window.PopupNotifications.remove(this.currentNotification);
      this.currentNotification = null;
    }
  }

  dispatchUserAction(action) {
    this._dispatchToASRouter(
      {type: "USER_ACTION", data: action},
      {browser: this.window.gBrowser.selectedBrowser}
    );
  }

  _expand(delay = 0) {
    if (!delay) {
      // Non-delayed state change overrides any scheduled state changes
      this._clearScheduledStateChanges();
      this.urlbar.setAttribute("cfr-recommendation-state", "expanded");
    } else {
      this.stateTransitionTimeoutIDs.push(this.window.setTimeout(() => {
        this.urlbar.setAttribute("cfr-recommendation-state", "expanded");
      }, delay));
    }
  }

  _collapse(delay = 0) {
    if (!delay) {
      // Non-delayed state change overrides any scheduled state changes
      this._clearScheduledStateChanges();
      if (this.urlbar.getAttribute("cfr-recommendation-state") === "expanded") {
        this.urlbar.setAttribute("cfr-recommendation-state", "collapsed");
      }
    } else {
      this.stateTransitionTimeoutIDs.push(this.window.setTimeout(() => {
        if (this.urlbar.getAttribute("cfr-recommendation-state") === "expanded") {
          this.urlbar.setAttribute("cfr-recommendation-state", "collapsed");
        }
      }, delay));
    }
  }

  _clearScheduledStateChanges() {
    while (this.stateTransitionTimeoutIDs.length > 0) {
      // clearTimeout is safe even with invalid/expired IDs
      this.window.clearTimeout(this.stateTransitionTimeoutIDs.pop());
    }
  }

  // This is called when the popup closes as a result of interaction _outside_
  // the popup, e.g. by hitting <esc>
  _popupStateChange(state) {
    if (["dismissed", "removed"].includes(state)) {
      this._collapse();
      if (this.currentNotification) {
        this.window.PopupNotifications.remove(this.currentNotification);
        this.currentNotification = null;
      }
    }
  }

  _blockMessage(messageID) {
    this._dispatchToASRouter(
      {type: "BLOCK_MESSAGE_BY_ID", data: {id: messageID}}
    );
  }

  /**
   * getStrings - Handles getting the localized strings vs message overrides.
   *              If string_id is not defined it assumes you passed in an override
   *              message and it just returns it.
   *              If subAttribute is provided, the string for it is returned.
   * @return A string. One of 1) passed in string 2) a String object with
   *         attributes property if there are attributes 3) the sub attribute.
   */
  async getStrings(string, subAttribute = "") {
    if (!string.string_id) {
      return string;
    }

    const [localeStrings] = await this._l10n.formatMessages([{
      id: string.string_id,
      args: string.args
    }]);

    const mainString = new String(localeStrings.value); // eslint-disable-line no-new-wrappers
    if (localeStrings.attributes) {
      const attributes = localeStrings.attributes.reduce((acc, attribute) => {
        acc[attribute.name] = attribute.value;
        return acc;
      }, {});
      mainString.attributes = attributes;
    }

    return subAttribute ? mainString.attributes[subAttribute] : mainString;
  }

  /**
   * Respond to a user click on the recommendation by showing a doorhanger/
   * popup notification
   */
  async _handleClick(event) { // eslint-disable-line max-statements
    const browser = this.window.gBrowser.selectedBrowser;
    if (!RecommendationMap.has(browser)) {
      // There's no recommendation for this browser, so the user shouldn't have
      // been able to click
      this.hide();
      return;
    }
    const {id, content} = RecommendationMap.get(browser);

    // The recommendation should remain either collapsed or expanded while the
    // doorhanger is showing
    this._clearScheduledStateChanges();

    // A hacky way of setting the popup anchor outside the usual url bar icon box
    // See https://searchfox.org/mozilla-central/rev/847b64cc28b74b44c379f9bff4f415b97da1c6d7/toolkit/modules/PopupNotifications.jsm#42
    browser.cfrpopupnotificationanchor = this.container;

    const notification = this.window.document.getElementById("contextual-feature-recommendation-notification");
    const headerLabel = this.window.document.getElementById("cfr-notification-header-label");
    const headerLink = this.window.document.getElementById("cfr-notification-header-link");
    const headerImage = this.window.document.getElementById("cfr-notification-header-image");
    const author = this.window.document.getElementById("cfr-notification-author");
    const footerText = this.window.document.getElementById("cfr-notification-footer-text");
    const footerFilledStars = this.window.document.getElementById("cfr-notification-footer-filled-stars");
    const footerEmptyStars = this.window.document.getElementById("cfr-notification-footer-empty-stars");
    const footerUsers = this.window.document.getElementById("cfr-notification-footer-users");
    const footerSpacer = this.window.document.getElementById("cfr-notification-footer-spacer");
    const footerLink = this.window.document.getElementById("cfr-notification-footer-learn-more-link");

    headerLabel.value = await this.getStrings(content.heading_text);
    headerLink.setAttribute("href", SUMO_BASE_URL + content.info_icon.sumo_path);
    const isRTL = this.window.getComputedStyle(notification).direction === "rtl";
    const attribute = isRTL ? "left" : "right";
    headerLink.setAttribute(attribute, 0);
    headerImage.setAttribute("tooltiptext", await this.getStrings(content.info_icon.label, "tooltiptext"));
    headerLink.onclick = () => this._sendTelemetry({message_id: id, bucket_id: content.bucket_id, event: "RATIONALE"});

    author.textContent = await this.getStrings({
      string_id: "cfr-doorhanger-extension-author",
      args: {name: content.addon.author}
    });

    footerText.textContent = await this.getStrings(content.text);

    const {rating} = content.addon;
    if (rating) {
      const MAX_RATING = 5;
      const STARS_WIDTH = 17 * MAX_RATING;
      const calcWidth = stars => `${stars / MAX_RATING * STARS_WIDTH}px`;
      footerFilledStars.style.width = calcWidth(rating);
      footerEmptyStars.style.width = calcWidth(MAX_RATING - rating);

      const ratingString = await this.getStrings({
        string_id: "cfr-doorhanger-extension-rating",
        args: {total: rating}
      }, "tooltiptext");
      footerFilledStars.setAttribute("tooltiptext", ratingString);
      footerEmptyStars.setAttribute("tooltiptext", ratingString);
    } else {
      footerFilledStars.style.width = "";
      footerEmptyStars.style.width = "";
      footerFilledStars.removeAttribute("tooltiptext");
      footerEmptyStars.removeAttribute("tooltiptext");
    }

    const {users} = content.addon;
    if (users) {
      footerUsers.setAttribute("value", await this.getStrings({
        string_id: "cfr-doorhanger-extension-total-users",
        args: {total: users}
      }));
      footerUsers.removeAttribute("hidden");
    } else {
      // Prevent whitespace around empty label from affecting other spacing
      footerUsers.setAttribute("hidden", true);
      footerUsers.removeAttribute("value");
    }

    // Spacer pushes the link to the opposite end when there's other content
    if (rating || users) {
      footerSpacer.removeAttribute("hidden");
    } else {
      footerSpacer.setAttribute("hidden", true);
    }

    footerLink.value = await this.getStrings({string_id: "cfr-doorhanger-extension-learn-more-link"});
    footerLink.setAttribute("href", content.addon.amo_url);
    footerLink.onclick = () => this._sendTelemetry({message_id: id, bucket_id: content.bucket_id, event: "LEARN_MORE"});

    const {primary, secondary} = content.buttons;
    const primaryBtnStrings = await this.getStrings(primary.label);
    const secondaryBtnStrings = await this.getStrings(secondary.label);

    const mainAction = {
      label: primaryBtnStrings,
      accessKey: primaryBtnStrings.attributes.accesskey,
      callback: () => {
        this._blockMessage(id);
        this.dispatchUserAction(primary.action);
        this.hide();
        this._sendTelemetry({message_id: id, bucket_id: content.bucket_id, event: "INSTALL"});
        RecommendationMap.delete(browser);
      }
    };

    const secondaryActions = [{
      label: secondaryBtnStrings,
      accessKey: secondaryBtnStrings.attributes.accesskey,
      callback: () => {
        this.hide();
        this._sendTelemetry({message_id: id, bucket_id: content.bucket_id, event: "DISMISS"});
        RecommendationMap.delete(browser);
      }
    }];

    const options = {
      popupIconURL: content.addon.icon,
      hideClose: true,
      eventCallback: this._popupStateChange
    };

    this._sendTelemetry({message_id: id, bucket_id: content.bucket_id, event: "CLICK_DOORHANGER"});
    this.currentNotification = this.window.PopupNotifications.show(
      browser,
      POPUP_NOTIFICATION_ID,
      await this.getStrings(content.addon.title),
      "cfr",
      mainAction,
      secondaryActions,
      options
    );
  }
}

function isHostMatch(browser, host) {
  return (browser.documentURI.scheme.startsWith("http") &&
    browser.documentURI.host === host);
}

const CFRPageActions = {
  // For testing purposes
  RecommendationMap,
  PageActionMap,

  /**
   * To be called from browser.js on a location change, passing in the browser
   * that's been updated
   */
  updatePageActions(browser) {
    const win = browser.ownerGlobal;
    const pageAction = PageActionMap.get(win);
    if (!pageAction || browser !== win.gBrowser.selectedBrowser) {
      return;
    }
    if (RecommendationMap.has(browser)) {
      const recommendation = RecommendationMap.get(browser);
      if (isHostMatch(browser, recommendation.host)) {
        // The browser has a recommendation specified with this host, so show
        // the page action
        pageAction.show(recommendation);
      } else {
        // The user has navigated away from the specified host in the given
        // browser, so the recommendation is no longer valid and should be removed
        RecommendationMap.delete(browser);
        pageAction.hide();
      }
    } else {
      // There's no recommendation specified for this browser, so hide the page action
      pageAction.hide();
    }
  },

  /**
   * Fetch the URL to the latest add-on xpi so the recommendation can download it.
   * @param addon          The add-on provided by the CFRMessageProvider
   * @return               A string for the URL that was fetched
   */
  async _fetchLatestAddonVersion({id}) {
    let url = null;
    try {
      const response = await fetch(`${ADDONS_API_URL}/${id}`);
      if (response.status !== 204 && response.ok) {
        const json = await response.json();
        url = json.current_version.files[0].url;
      }
    } catch (e) {
      Cu.reportError("Failed to get the latest add-on version for this recommendation");
    }
    return url;
  },

  async _maybeAddAddonInstallURL(recommendation) {
    const {content, template} = recommendation;
    // If this is CFR is not for an add-on, return the original recommendation
    if (template !== "cfr_doorhanger") {
      return recommendation;
    }

    const url = await this._fetchLatestAddonVersion(content.addon);
    // If we failed to get a url to the latest xpi, return false so we know not to show
    // a recommendation
    if (!url) {
      return false;
    }

    // Update the action's data with the url to the latest xpi, leave the rest
    // of the recommendation properties intact
    return {
      ...recommendation,
      content: {
        ...content,
        buttons: {
          ...content.buttons,
          primary: {
            ...content.buttons.primary,
            action: {...content.buttons.primary.action, data: {url}},
          },
        },
      },
    };
  },

  /**
   * Force a recommendation to be shown. Should only happen via the Admin page.
   * @param browser                 The browser for the recommendation
   * @param originalRecommendation  The recommendation to show
   * @param dispatchToASRouter      A function to dispatch resulting actions to
   * @return                        Did adding the recommendation succeed?
   */
  async forceRecommendation(browser, originalRecommendation, dispatchToASRouter) {
    // If we are forcing via the Admin page, the browser comes in a different format
    const win = browser.browser.ownerGlobal;
    const recommendation = await this._maybeAddAddonInstallURL(originalRecommendation);
    if (!recommendation) {
      return false;
    }
    const {id, content} = recommendation;
    RecommendationMap.set(browser.browser, {id, content});
    if (!PageActionMap.has(win)) {
      PageActionMap.set(win, new PageAction(win, dispatchToASRouter));
    }
    await PageActionMap.get(win).show(recommendation, true);
    return true;
  },

  /**
   * Add a recommendation specific to the given browser and host.
   * @param browser                 The browser for the recommendation
   * @param host                    The host for the recommendation
   * @param originalRecommendation  The recommendation to show
   * @param dispatchToASRouter      A function to dispatch resulting actions to
   * @return                        Did adding the recommendation succeed?
   */
  async addRecommendation(browser, host, originalRecommendation, dispatchToASRouter) {
    const win = browser.ownerGlobal;
    if (PrivateBrowsingUtils.isWindowPrivate(win)) {
      return false;
    }
    if (browser !== win.gBrowser.selectedBrowser || !isHostMatch(browser, host)) {
      return false;
    }
    const recommendation = await this._maybeAddAddonInstallURL(originalRecommendation);
    if (!recommendation) {
      return false;
    }
    const {id, content} = recommendation;
    RecommendationMap.set(browser, {id, host, content});
    if (!PageActionMap.has(win)) {
      PageActionMap.set(win, new PageAction(win, dispatchToASRouter));
    }
    await PageActionMap.get(win).show(recommendation, true);
    return true;
  },

  /**
   * Clear all recommendations and hide all PageActions
   */
  clearRecommendations() {
    // WeakMaps aren't iterable so we have to test all existing windows
    for (const win of Services.wm.getEnumerator("navigator:browser")) {
      if (win.closed || !PageActionMap.has(win)) {
        continue;
      }
      PageActionMap.get(win).hide();
    }
    // WeakMaps don't have a `clear` method
    PageActionMap = new WeakMap();
    RecommendationMap = new WeakMap();
  }
};
this.CFRPageActions = CFRPageActions;

const EXPORTED_SYMBOLS = ["CFRPageActions"];
