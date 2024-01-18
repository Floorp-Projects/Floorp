/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  CustomizableUI: "resource:///modules/CustomizableUI.sys.mjs",
  PrivateBrowsingUtils: "resource://gre/modules/PrivateBrowsingUtils.sys.mjs",
  RemoteL10n: "resource://activity-stream/lib/RemoteL10n.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "TrackingDBService",
  "@mozilla.org/tracking-db-service;1",
  "nsITrackingDBService"
);
XPCOMUtils.defineLazyPreferenceGetter(
  lazy,
  "milestones",
  "browser.contentblocking.cfr-milestone.milestones",
  "[]",
  null,
  JSON.parse
);

const POPUP_NOTIFICATION_ID = "contextual-feature-recommendation";
const SUMO_BASE_URL = Services.urlFormatter.formatURLPref(
  "app.support.baseURL"
);
const ADDONS_API_URL =
  "https://services.addons.mozilla.org/api/v4/addons/addon";

const DELAY_BEFORE_EXPAND_MS = 1000;
const CATEGORY_ICONS = {
  cfrAddons: "webextensions-icon",
  cfrFeatures: "recommendations-icon",
  cfrHeartbeat: "highlights-icon",
};

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
  constructor(win, dispatchCFRAction) {
    this.window = win;

    this.urlbar = win.gURLBar; // The global URLBar object
    this.urlbarinput = win.gURLBar.textbox; // The URLBar DOM node

    this.container = win.document.getElementById(
      "contextual-feature-recommendation"
    );
    this.button = win.document.getElementById("cfr-button");
    this.label = win.document.getElementById("cfr-label");

    // This should NOT be use directly to dispatch message-defined actions attached to buttons.
    // Please use dispatchUserAction instead.
    this._dispatchCFRAction = dispatchCFRAction;

    this._popupStateChange = this._popupStateChange.bind(this);
    this._collapse = this._collapse.bind(this);
    this._cfrUrlbarButtonClick = this._cfrUrlbarButtonClick.bind(this);
    this._executeNotifierAction = this._executeNotifierAction.bind(this);
    this.dispatchUserAction = this.dispatchUserAction.bind(this);

    // Saved timeout IDs for scheduled state changes, so they can be cancelled
    this.stateTransitionTimeoutIDs = [];

    ChromeUtils.defineLazyGetter(this, "isDarkTheme", () => {
      try {
        return this.window.document.documentElement.hasAttribute(
          "lwt-toolbar-field-brighttext"
        );
      } catch (e) {
        return false;
      }
    });
  }

  addImpression(recommendation) {
    this._dispatchImpression(recommendation);
    // Only send an impression ping upon the first expansion.
    // Note that when the user clicks on the "show" button on the asrouter admin
    // page (both `bucket_id` and `id` will be set as null), we don't want to send
    // the impression ping in that case.
    if (!!recommendation.id && !!recommendation.content.bucket_id) {
      this._sendTelemetry({
        message_id: recommendation.id,
        bucket_id: recommendation.content.bucket_id,
        event: "IMPRESSION",
      });
    }
  }

  reloadL10n() {
    lazy.RemoteL10n.reloadL10n();
  }

  async showAddressBarNotifier(recommendation, shouldExpand = false) {
    this.container.hidden = false;

    let notificationText = await this.getStrings(
      recommendation.content.notification_text
    );
    this.label.value = notificationText;
    if (notificationText.attributes) {
      this.button.setAttribute(
        "tooltiptext",
        notificationText.attributes.tooltiptext
      );
      // For a11y, we want the more descriptive text.
      this.container.setAttribute(
        "aria-label",
        notificationText.attributes.tooltiptext
      );
    }
    this.container.setAttribute(
      "data-cfr-icon",
      CATEGORY_ICONS[recommendation.content.category]
    );
    if (recommendation.content.active_color) {
      this.container.style.setProperty(
        "--cfr-active-color",
        recommendation.content.active_color
      );
    }

    if (recommendation.content.active_text_color) {
      this.container.style.setProperty(
        "--cfr-active-text-color",
        recommendation.content.active_text_color
      );
    }

    // Wait for layout to flush to avoid a synchronous reflow then calculate the
    // label width. We can safely get the width even though the recommendation is
    // collapsed; the label itself remains full width (with its overflow hidden)
    let [{ width }] = await this.window.promiseDocumentFlushed(() =>
      this.label.getClientRects()
    );
    this.urlbarinput.style.setProperty("--cfr-label-width", `${width}px`);

    this.container.addEventListener("click", this._cfrUrlbarButtonClick);
    // Collapse the recommendation on url bar focus in order to free up more
    // space to display and edit the url
    this.urlbar.addEventListener("focus", this._collapse);

    if (shouldExpand) {
      this._clearScheduledStateChanges();

      // After one second, expand
      this._expand(DELAY_BEFORE_EXPAND_MS);

      this.addImpression(recommendation);
    }

    if (notificationText.attributes) {
      this.window.A11yUtils.announce({
        raw: notificationText.attributes["a11y-announcement"],
        source: this.container,
      });
    }
  }

  hideAddressBarNotifier() {
    this.container.hidden = true;
    this._clearScheduledStateChanges();
    this.urlbarinput.removeAttribute("cfr-recommendation-state");
    this.container.removeEventListener("click", this._cfrUrlbarButtonClick);
    this.urlbar.removeEventListener("focus", this._collapse);
    if (this.currentNotification) {
      this.window.PopupNotifications.remove(this.currentNotification);
      this.currentNotification = null;
    }
  }

  _expand(delay) {
    if (delay > 0) {
      this.stateTransitionTimeoutIDs.push(
        this.window.setTimeout(() => {
          this.urlbarinput.setAttribute("cfr-recommendation-state", "expanded");
        }, delay)
      );
    } else {
      // Non-delayed state change overrides any scheduled state changes
      this._clearScheduledStateChanges();
      this.urlbarinput.setAttribute("cfr-recommendation-state", "expanded");
    }
  }

  _collapse(delay) {
    if (delay > 0) {
      this.stateTransitionTimeoutIDs.push(
        this.window.setTimeout(() => {
          if (
            this.urlbarinput.getAttribute("cfr-recommendation-state") ===
            "expanded"
          ) {
            this.urlbarinput.setAttribute(
              "cfr-recommendation-state",
              "collapsed"
            );
          }
        }, delay)
      );
    } else {
      // Non-delayed state change overrides any scheduled state changes
      this._clearScheduledStateChanges();
      if (
        this.urlbarinput.getAttribute("cfr-recommendation-state") === "expanded"
      ) {
        this.urlbarinput.setAttribute("cfr-recommendation-state", "collapsed");
      }
    }
  }

  _clearScheduledStateChanges() {
    while (this.stateTransitionTimeoutIDs.length) {
      // clearTimeout is safe even with invalid/expired IDs
      this.window.clearTimeout(this.stateTransitionTimeoutIDs.pop());
    }
  }

  // This is called when the popup closes as a result of interaction _outside_
  // the popup, e.g. by hitting <esc>
  _popupStateChange(state) {
    if (state === "shown") {
      if (this._autoFocus) {
        this.window.document.commandDispatcher.advanceFocusIntoSubtree(
          this.currentNotification.owner.panel
        );
        this._autoFocus = false;
      }
    } else if (state === "removed") {
      if (this.currentNotification) {
        this.window.PopupNotifications.remove(this.currentNotification);
        this.currentNotification = null;
      }
    } else if (state === "dismissed") {
      const message = RecommendationMap.get(this.currentNotification?.browser);
      this._sendTelemetry({
        message_id: message?.id,
        bucket_id: message?.content.bucket_id,
        event: "DISMISS",
      });
      this._collapse();
    }
  }

  shouldShowDoorhanger(recommendation) {
    if (recommendation.content.layout === "chiclet_open_url") {
      return false;
    }

    return true;
  }

  dispatchUserAction(action) {
    this._dispatchCFRAction(
      { type: "USER_ACTION", data: action },
      this.window.gBrowser.selectedBrowser
    );
  }

  _dispatchImpression(message) {
    this._dispatchCFRAction({ type: "IMPRESSION", data: message });
  }

  _sendTelemetry(ping) {
    const data = { action: "cfr_user_event", source: "CFR", ...ping };
    if (lazy.PrivateBrowsingUtils.isWindowPrivate(this.window)) {
      data.is_private = true;
    }
    this._dispatchCFRAction({
      type: "DOORHANGER_TELEMETRY",
      data,
    });
  }

  _blockMessage(messageID) {
    this._dispatchCFRAction({
      type: "BLOCK_MESSAGE_BY_ID",
      data: { id: messageID },
    });
  }

  maybeLoadCustomElement(win) {
    if (!win.customElements.get("remote-text")) {
      Services.scriptloader.loadSubScript(
        "resource://activity-stream/data/custom-elements/paragraph.js",
        win
      );
    }
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
      if (subAttribute) {
        if (string.attributes) {
          return string.attributes[subAttribute];
        }

        console.error(`String ${string.value} does not contain any attributes`);
        return subAttribute;
      }

      if (typeof string.value === "string") {
        const stringWithAttributes = new String(string.value); // eslint-disable-line no-new-wrappers
        stringWithAttributes.attributes = string.attributes;
        return stringWithAttributes;
      }

      return string;
    }

    const [localeStrings] = await lazy.RemoteL10n.l10n.formatMessages([
      {
        id: string.string_id,
        args: string.args,
      },
    ]);

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

  async _setAddonRating(document, content) {
    const footerFilledStars = this.window.document.getElementById(
      "cfr-notification-footer-filled-stars"
    );
    const footerEmptyStars = this.window.document.getElementById(
      "cfr-notification-footer-empty-stars"
    );
    const footerUsers = this.window.document.getElementById(
      "cfr-notification-footer-users"
    );

    const rating = content.addon?.rating;
    if (rating) {
      const MAX_RATING = 5;
      const STARS_WIDTH = 16 * MAX_RATING;
      const calcWidth = stars => `${(stars / MAX_RATING) * STARS_WIDTH}px`;
      const filledWidth =
        rating <= MAX_RATING ? calcWidth(rating) : calcWidth(MAX_RATING);
      const emptyWidth =
        rating <= MAX_RATING ? calcWidth(MAX_RATING - rating) : calcWidth(0);

      footerFilledStars.style.width = filledWidth;
      footerEmptyStars.style.width = emptyWidth;

      const ratingString = await this.getStrings(
        {
          string_id: "cfr-doorhanger-extension-rating",
          args: { total: rating },
        },
        "tooltiptext"
      );
      footerFilledStars.setAttribute("tooltiptext", ratingString);
      footerEmptyStars.setAttribute("tooltiptext", ratingString);
    } else {
      footerFilledStars.style.width = "";
      footerEmptyStars.style.width = "";
      footerFilledStars.removeAttribute("tooltiptext");
      footerEmptyStars.removeAttribute("tooltiptext");
    }

    const users = content.addon?.users;
    if (users) {
      footerUsers.setAttribute("value", users);
      footerUsers.hidden = false;
    } else {
      // Prevent whitespace around empty label from affecting other spacing
      footerUsers.hidden = true;
      footerUsers.removeAttribute("value");
    }
  }

  _createElementAndAppend({ type, id }, parent) {
    let element = this.window.document.createXULElement(type);
    if (id) {
      element.setAttribute("id", id);
    }
    parent.appendChild(element);
    return element;
  }

  async _renderMilestonePopup(message, browser) {
    this.maybeLoadCustomElement(this.window);

    let { content, id } = message;
    let { primary, secondary } = content.buttons;
    let earliestDate = await lazy.TrackingDBService.getEarliestRecordedDate();
    let timestamp = earliestDate ?? new Date().getTime();
    let panelTitle = "";
    let headerLabel = this.window.document.getElementById(
      "cfr-notification-header-label"
    );
    let reachedMilestone = 0;
    let totalSaved = await lazy.TrackingDBService.sumAllEvents();
    for (let milestone of lazy.milestones) {
      if (totalSaved >= milestone) {
        reachedMilestone = milestone;
      }
    }
    if (headerLabel.firstChild) {
      headerLabel.firstChild.remove();
    }
    headerLabel.appendChild(
      lazy.RemoteL10n.createElement(this.window.document, "span", {
        content: message.content.heading_text,
        attributes: {
          blockedCount: reachedMilestone,
          date: timestamp,
        },
      })
    );

    // Use the message layout as a CSS selector to hide different parts of the
    // notification template markup
    this.window.document
      .getElementById("contextual-feature-recommendation-notification")
      .setAttribute("data-notification-category", content.layout);
    this.window.document
      .getElementById("contextual-feature-recommendation-notification")
      .setAttribute("data-notification-bucket", content.bucket_id);

    let primaryBtnString = await this.getStrings(primary.label);
    let primaryActionCallback = () => {
      this.dispatchUserAction(primary.action);
      this._sendTelemetry({
        message_id: id,
        bucket_id: content.bucket_id,
        event: "CLICK_BUTTON",
      });

      RecommendationMap.delete(browser);
      // Invalidate the pref after the user interacts with the button.
      // We don't need to show the illustration in the privacy panel.
      Services.prefs.clearUserPref(
        "browser.contentblocking.cfr-milestone.milestone-shown-time"
      );
    };

    let secondaryBtnString = await this.getStrings(secondary[0].label);
    let secondaryActionsCallback = () => {
      this.dispatchUserAction(secondary[0].action);
      this._sendTelemetry({
        message_id: id,
        bucket_id: content.bucket_id,
        event: "DISMISS",
      });
      RecommendationMap.delete(browser);
    };

    let mainAction = {
      label: primaryBtnString,
      accessKey: primaryBtnString.attributes.accesskey,
      callback: primaryActionCallback,
    };

    let secondaryActions = [
      {
        label: secondaryBtnString,
        accessKey: secondaryBtnString.attributes.accesskey,
        callback: secondaryActionsCallback,
      },
    ];

    // Actually show the notification
    this.currentNotification = this.window.PopupNotifications.show(
      browser,
      POPUP_NOTIFICATION_ID,
      panelTitle,
      "cfr",
      mainAction,
      secondaryActions,
      {
        hideClose: true,
        persistWhileVisible: true,
        recordTelemetryInPrivateBrowsing: content.show_in_private_browsing,
      }
    );
    Services.prefs.setIntPref(
      "browser.contentblocking.cfr-milestone.milestone-achieved",
      reachedMilestone
    );
    Services.prefs.setStringPref(
      "browser.contentblocking.cfr-milestone.milestone-shown-time",
      Date.now().toString()
    );
  }

  // eslint-disable-next-line max-statements
  async _renderPopup(message, browser) {
    this.maybeLoadCustomElement(this.window);

    const { id, content } = message;

    const headerLabel = this.window.document.getElementById(
      "cfr-notification-header-label"
    );
    const headerLink = this.window.document.getElementById(
      "cfr-notification-header-link"
    );
    const headerImage = this.window.document.getElementById(
      "cfr-notification-header-image"
    );
    const footerText = this.window.document.getElementById(
      "cfr-notification-footer-text"
    );
    const footerLink = this.window.document.getElementById(
      "cfr-notification-footer-learn-more-link"
    );
    const { primary, secondary } = content.buttons;
    let primaryActionCallback;
    let persistent = !!content.persistent_doorhanger;
    let options = {
      persistent,
      persistWhileVisible: persistent,
      recordTelemetryInPrivateBrowsing: content.show_in_private_browsing,
    };
    let panelTitle;

    headerLabel.value = await this.getStrings(content.heading_text);
    if (content.info_icon) {
      headerLink.setAttribute(
        "href",
        SUMO_BASE_URL + content.info_icon.sumo_path
      );
      headerImage.setAttribute(
        "tooltiptext",
        await this.getStrings(content.info_icon.label, "tooltiptext")
      );
    }
    headerLink.onclick = () =>
      this._sendTelemetry({
        message_id: id,
        bucket_id: content.bucket_id,
        event: "RATIONALE",
      });
    // Use the message layout as a CSS selector to hide different parts of the
    // notification template markup
    this.window.document
      .getElementById("contextual-feature-recommendation-notification")
      .setAttribute("data-notification-category", content.layout);
    this.window.document
      .getElementById("contextual-feature-recommendation-notification")
      .setAttribute("data-notification-bucket", content.bucket_id);

    const author = this.window.document.getElementById(
      "cfr-notification-author"
    );
    if (author.firstChild) {
      author.firstChild.remove();
    }

    switch (content.layout) {
      case "icon_and_message":
        //Clearing content and styles that may have been set by a prior addon_recommendation CFR
        this._setAddonRating(this.window.document, content);
        author.appendChild(
          lazy.RemoteL10n.createElement(this.window.document, "span", {
            content: content.text,
          })
        );
        primaryActionCallback = () => {
          this._blockMessage(id);
          this.dispatchUserAction(primary.action);
          this.hideAddressBarNotifier();
          this._sendTelemetry({
            message_id: id,
            bucket_id: content.bucket_id,
            event: "ENABLE",
          });
          RecommendationMap.delete(browser);
        };

        let getIcon = () => {
          if (content.icon_dark_theme && this.isDarkTheme) {
            return content.icon_dark_theme;
          }
          return content.icon;
        };

        let learnMoreURL = content.learn_more
          ? SUMO_BASE_URL + content.learn_more
          : null;

        panelTitle = await this.getStrings(content.heading_text);
        options = {
          popupIconURL: getIcon(),
          popupIconClass: content.icon_class,
          learnMoreURL,
          ...options,
        };
        break;
      default:
        const authorText = await this.getStrings({
          string_id: "cfr-doorhanger-extension-author",
          args: { name: content.addon.author },
        });
        panelTitle = await this.getStrings(content.addon.title);
        await this._setAddonRating(this.window.document, content);
        if (footerText.firstChild) {
          footerText.firstChild.remove();
        }
        if (footerText.lastChild) {
          footerText.lastChild.remove();
        }

        // Main body content of the dropdown
        footerText.appendChild(
          lazy.RemoteL10n.createElement(this.window.document, "span", {
            content: content.text,
          })
        );

        footerLink.value = await this.getStrings({
          string_id: "cfr-doorhanger-extension-learn-more-link",
        });
        footerLink.setAttribute("href", content.addon.amo_url);
        footerLink.onclick = () =>
          this._sendTelemetry({
            message_id: id,
            bucket_id: content.bucket_id,
            event: "LEARN_MORE",
          });

        footerText.appendChild(footerLink);
        options = {
          popupIconURL: content.addon.icon,
          popupIconClass: content.icon_class,
          name: authorText,
          ...options,
        };

        primaryActionCallback = async () => {
          primary.action.data.url =
            // eslint-disable-next-line no-use-before-define
            await CFRPageActions._fetchLatestAddonVersion(content.addon.id);
          this._blockMessage(id);
          this.dispatchUserAction(primary.action);
          this.hideAddressBarNotifier();
          this._sendTelemetry({
            message_id: id,
            bucket_id: content.bucket_id,
            event: "INSTALL",
          });
          RecommendationMap.delete(browser);
        };
    }

    const primaryBtnStrings = await this.getStrings(primary.label);
    const mainAction = {
      label: primaryBtnStrings,
      accessKey: primaryBtnStrings.attributes.accesskey,
      callback: primaryActionCallback,
    };

    let _renderSecondaryButtonAction = async (event, button) => {
      let label = await this.getStrings(button.label);
      let { attributes } = label;

      return {
        label,
        accessKey: attributes.accesskey,
        callback: () => {
          if (button.action) {
            this.dispatchUserAction(button.action);
          } else {
            this._blockMessage(id);
            this.hideAddressBarNotifier();
            RecommendationMap.delete(browser);
          }

          this._sendTelemetry({
            message_id: id,
            bucket_id: content.bucket_id,
            event,
          });
          // We want to collapse if needed when we dismiss
          this._collapse();
        },
      };
    };

    // For each secondary action, define default telemetry event
    const defaultSecondaryEvent = ["DISMISS", "BLOCK", "MANAGE"];
    const secondaryActions = await Promise.all(
      secondary.map((button, i) => {
        return _renderSecondaryButtonAction(
          button.event || defaultSecondaryEvent[i],
          button
        );
      })
    );

    // If the recommendation button is focused, it was probably activated via
    // the keyboard. Therefore, focus the first element in the notification when
    // it appears.
    // We don't use the autofocus option provided by PopupNotifications.show
    // because it doesn't focus the first element; i.e. the user still has to
    // press tab once. That's not good enough, especially for screen reader
    // users. Instead, we handle this ourselves in _popupStateChange.
    this._autoFocus = this.window.document.activeElement === this.container;

    // Actually show the notification
    this.currentNotification = this.window.PopupNotifications.show(
      browser,
      POPUP_NOTIFICATION_ID,
      panelTitle,
      "cfr",
      mainAction,
      secondaryActions,
      {
        ...options,
        hideClose: true,
        eventCallback: this._popupStateChange,
      }
    );
  }

  _executeNotifierAction(browser, message) {
    switch (message.content.layout) {
      case "chiclet_open_url":
        this._dispatchCFRAction(
          {
            type: "USER_ACTION",
            data: {
              type: "OPEN_URL",
              data: {
                args: message.content.action.url,
                where: message.content.action.where,
              },
            },
          },
          this.window
        );
        break;
    }

    this._blockMessage(message.id);
    this.hideAddressBarNotifier();
    RecommendationMap.delete(browser);
  }

  /**
   * Respond to a user click on the recommendation by showing a doorhanger/
   * popup notification or running the action defined in the message
   */
  async _cfrUrlbarButtonClick(event) {
    const browser = this.window.gBrowser.selectedBrowser;
    if (!RecommendationMap.has(browser)) {
      // There's no recommendation for this browser, so the user shouldn't have
      // been able to click
      this.hideAddressBarNotifier();
      return;
    }
    const message = RecommendationMap.get(browser);
    const { id, content } = message;

    this._sendTelemetry({
      message_id: id,
      bucket_id: content.bucket_id,
      event: "CLICK_DOORHANGER",
    });

    if (this.shouldShowDoorhanger(message)) {
      // The recommendation should remain either collapsed or expanded while the
      // doorhanger is showing
      this._clearScheduledStateChanges(browser, message);
      await this.showPopup();
    } else {
      await this._executeNotifierAction(browser, message);
    }
  }

  _getVisibleElement(idOrEl) {
    const element =
      typeof idOrEl === "string"
        ? idOrEl && this.window.document.getElementById(idOrEl)
        : idOrEl;
    if (!element) {
      return null; // element doesn't exist at all
    }
    const { visibility, display } = this.window.getComputedStyle(element);
    if (
      !this.window.isElementVisible(element) ||
      visibility !== "visible" ||
      display === "none"
    ) {
      // CSS rules like visibility: hidden or display: none. these result in
      // element being invisible and unclickable.
      return null;
    }
    let widget = lazy.CustomizableUI.getWidget(idOrEl);
    if (
      widget &&
      (this.window.CustomizationHandler.isCustomizing() ||
        widget.areaType?.includes("panel"))
    ) {
      // The element is a customizable widget (a toolbar item, e.g. the
      // reload button or the downloads button). Widgets can be in various
      // areas, like the overflow panel or the customization palette.
      // Widgets in the palette are present in the chrome's DOM during
      // customization, but can't be used.
      return null;
    }
    return element;
  }

  async showPopup() {
    const browser = this.window.gBrowser.selectedBrowser;
    const message = RecommendationMap.get(browser);
    const { content } = message;

    // A hacky way of setting the popup anchor outside the usual url bar icon box
    // See https://searchfox.org/mozilla-central/rev/eb07633057d66ab25f9db4c5900eeb6913da7579/toolkit/modules/PopupNotifications.sys.mjs#44
    browser.cfrpopupnotificationanchor =
      this._getVisibleElement(content.anchor_id) ||
      this._getVisibleElement(content.alt_anchor_id) ||
      this._getVisibleElement(this.button) ||
      this._getVisibleElement(this.container);

    await this._renderPopup(message, browser);
  }

  async showMilestonePopup() {
    const browser = this.window.gBrowser.selectedBrowser;
    const message = RecommendationMap.get(browser);
    const { content } = message;

    // A hacky way of setting the popup anchor outside the usual url bar icon box
    // See https://searchfox.org/mozilla-central/rev/eb07633057d66ab25f9db4c5900eeb6913da7579/toolkit/modules/PopupNotifications.sys.mjs#44
    browser.cfrpopupnotificationanchor =
      this.window.document.getElementById(content.anchor_id) || this.container;

    await this._renderMilestonePopup(message, browser);
    return true;
  }
}

function isHostMatch(browser, host) {
  return (
    browser.documentURI.scheme.startsWith("http") &&
    browser.documentURI.host === host
  );
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
      if (
        !recommendation.content.skip_address_bar_notifier &&
        (isHostMatch(browser, recommendation.host) ||
          // If there is no host associated we assume we're back on a tab
          // that had a CFR message so we should show it again
          !recommendation.host)
      ) {
        // The browser has a recommendation specified with this host, so show
        // the page action
        pageAction.showAddressBarNotifier(recommendation);
      } else if (!recommendation.content.persistent_doorhanger) {
        if (recommendation.retain) {
          // Keep the recommendation first time the user navigates away just in
          // case they will go back to the previous page
          pageAction.hideAddressBarNotifier();
          recommendation.retain = false;
        } else {
          // The user has navigated away from the specified host in the given
          // browser, so the recommendation is no longer valid and should be removed
          RecommendationMap.delete(browser);
          pageAction.hideAddressBarNotifier();
        }
      }
    } else {
      // There's no recommendation specified for this browser, so hide the page action
      pageAction.hideAddressBarNotifier();
    }
  },

  /**
   * Fetch the URL to the latest add-on xpi so the recommendation can download it.
   * @param id          The add-on ID
   * @return            A string for the URL that was fetched
   */
  async _fetchLatestAddonVersion(id) {
    let url = null;
    try {
      const response = await fetch(`${ADDONS_API_URL}/${id}/`, {
        credentials: "omit",
      });
      if (response.status !== 204 && response.ok) {
        const json = await response.json();
        url = json.current_version.files[0].url;
      }
    } catch (e) {
      console.error(
        "Failed to get the latest add-on version for this recommendation"
      );
    }
    return url;
  },

  /**
   * Force a recommendation to be shown. Should only happen via the Admin page.
   * @param browser                 The browser for the recommendation
   * @param recommendation  The recommendation to show
   * @param dispatchCFRAction      A function to dispatch resulting actions to
   * @return                        Did adding the recommendation succeed?
   */
  async forceRecommendation(browser, recommendation, dispatchCFRAction) {
    if (!browser) {
      return false;
    }
    // If we are forcing via the Admin page, the browser comes in a different format
    const win = browser.ownerGlobal;
    const { id, content } = recommendation;
    RecommendationMap.set(browser, {
      id,
      content,
      retain: true,
    });
    if (!PageActionMap.has(win)) {
      PageActionMap.set(win, new PageAction(win, dispatchCFRAction));
    }

    if (content.skip_address_bar_notifier) {
      if (recommendation.template === "milestone_message") {
        await PageActionMap.get(win).showMilestonePopup();
        PageActionMap.get(win).addImpression(recommendation);
      } else {
        await PageActionMap.get(win).showPopup();
        PageActionMap.get(win).addImpression(recommendation);
      }
    } else {
      await PageActionMap.get(win).showAddressBarNotifier(recommendation, true);
    }
    return true;
  },

  /**
   * Add a recommendation specific to the given browser and host.
   * @param browser                 The browser for the recommendation
   * @param host                    The host for the recommendation
   * @param recommendation          The recommendation to show
   * @param dispatchCFRAction       A function to dispatch resulting actions to
   * @return                        Did adding the recommendation succeed?
   */
  async addRecommendation(browser, host, recommendation, dispatchCFRAction) {
    if (!browser) {
      return false;
    }
    const win = browser.ownerGlobal;
    if (
      browser !== win.gBrowser.selectedBrowser ||
      // We can have recommendations without URL restrictions
      (host && !isHostMatch(browser, host))
    ) {
      return false;
    }
    if (RecommendationMap.has(browser)) {
      // Don't replace an existing message
      return false;
    }
    const { id, content } = recommendation;
    if (
      !content.show_in_private_browsing &&
      lazy.PrivateBrowsingUtils.isWindowPrivate(win)
    ) {
      return false;
    }
    RecommendationMap.set(browser, {
      id,
      host,
      content,
      retain: true,
    });
    if (!PageActionMap.has(win)) {
      PageActionMap.set(win, new PageAction(win, dispatchCFRAction));
    }

    if (content.skip_address_bar_notifier) {
      if (recommendation.template === "milestone_message") {
        await PageActionMap.get(win).showMilestonePopup();
        PageActionMap.get(win).addImpression(recommendation);
      } else {
        // Tracking protection messages
        await PageActionMap.get(win).showPopup();
        PageActionMap.get(win).addImpression(recommendation);
      }
    } else {
      // Doorhanger messages
      await PageActionMap.get(win).showAddressBarNotifier(recommendation, true);
    }
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
      PageActionMap.get(win).hideAddressBarNotifier();
    }
    // WeakMaps don't have a `clear` method
    PageActionMap = new WeakMap();
    RecommendationMap = new WeakMap();
    this.PageActionMap = PageActionMap;
    this.RecommendationMap = RecommendationMap;
  },

  /**
   * Reload the l10n Fluent files for all PageActions
   */
  reloadL10n() {
    for (const win of Services.wm.getEnumerator("navigator:browser")) {
      if (win.closed || !PageActionMap.has(win)) {
        continue;
      }
      PageActionMap.get(win).reloadL10n();
    }
  },
};

const EXPORTED_SYMBOLS = ["CFRPageActions", "PageAction"];
