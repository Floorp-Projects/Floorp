/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global TranslationBrowserChromeUiNotificationManager */
/* eslint no-unused-vars: ["error", { "varsIgnorePattern": "(TranslationBrowserChromeUi)" }]*/

const { setTimeout, clearTimeout } = ChromeUtils.import(
  "resource://gre/modules/Timer.jsm",
  {},
);

const TranslationInfoBarStates = {
  STATE_OFFER: 0,
  STATE_TRANSLATING: 1,
  STATE_TRANSLATED: 2,
  STATE_ERROR: 3,
  STATE_UNAVAILABLE: 4,
};

class TranslationBrowserChromeUi {
  constructor(Services, browser, context, apiEventEmitter, tabId) {
    this.Services = Services;
    this.uiState = null;
    this.browser = browser;
    this.context = context;
    this.translationInfoBarShown = false;
    this.shouldShowTranslationProgressTimer = undefined;
    this.importTranslationNotification();

    // The manager instance is injected into the translation notification bar and handles events from therein
    this.translationBrowserChromeUiNotificationManager = new TranslationBrowserChromeUiNotificationManager(
      browser,
      apiEventEmitter,
      tabId,
      TranslationInfoBarStates,
    );
  }

  get notificationBox() {
    return this.browser.ownerGlobal.gBrowser.getNotificationBox(this.browser);
  }

  importTranslationNotification() {
    const chromeWin = this.browser.ownerGlobal;

    // As a workaround to be able to load updates for the translation notification on extension reload
    // we use the current unix timestamp as part of the element id.
    // TODO: Restrict use of Date.now() as cachebuster to development mode only
    chromeWin.now = Date.now();

    try {
      chromeWin.customElements.setElementCreationCallback(
        `translation-notification-${chromeWin.now}`,
        () => {
          this.Services.scriptloader.loadSubScript(
            this.context.extension.getURL(
              "experiment-apis/translateUi/content/translation-notification.js",
            ) +
              "?cachebuster=" +
              chromeWin.now,
            chromeWin,
          );
        },
      );
    } catch (e) {
      console.log(
        "Error occurred when attempting to load the translation notification script, but we continue nevertheless",
        e,
      );
    }
  }

  onUiStateUpdate(uiState) {
    // Set all values before showing a new translation infobar.
    this.translationBrowserChromeUiNotificationManager.uiState = uiState;
    this.setInfobarState(uiState.infobarState);
    this.updateTranslationProgress(uiState);
    if (this.shouldShowInfoBar(this.browser.contentPrincipal)) {
      this.showTranslationInfoBarIfNotAlreadyShown();
    } else {
      this.hideTranslationInfoBarIfShown();
    }
  }

  /**
   * Syncs infobarState with the inner infobar state variable of the infobar
   * @param val
   */
  setInfobarState(val) {
    const notif = this.notificationBox.getNotificationWithValue("translation");
    if (notif) {
      notif.state = val;
    }
  }

  /**
   * Informs the infobar element of the current translation progress
   */
  updateTranslationProgress(uiState) {
    // Don't bother updating translation progress if not currently translating
    if (
      this.translationBrowserChromeUiNotificationManager.uiState
        .infobarState !== TranslationInfoBarStates.STATE_TRANSLATING
    ) {
      return;
    }
    const notif = this.notificationBox.getNotificationWithValue("translation");
    if (notif) {
      const {
        modelDownloading,
        translationDurationMs,
        localizedTranslationProgressText,
      } = uiState;

      // Always cancel ongoing timers so that we start from a clean state
      if (this.shouldShowTranslationProgressTimer) {
        clearTimeout(this.shouldShowTranslationProgressTimer);
      }

      // Only show progress if translation has been going on for at least 3 seconds
      // or we are currently downloading a model
      let shouldShowTranslationProgress;
      const thresholdMsAfterWhichToShouldTranslationProgress = 3000;
      if (
        translationDurationMs >=
          thresholdMsAfterWhichToShouldTranslationProgress ||
        modelDownloading
      ) {
        shouldShowTranslationProgress = true;
      } else {
        // Use a timer to show the translation progress after the threshold
        this.shouldShowTranslationProgressTimer = setTimeout(() => {
          notif.updateTranslationProgress(
            true,
            localizedTranslationProgressText,
          );
          clearTimeout(this.shouldShowTranslationProgressTimer);
        }, thresholdMsAfterWhichToShouldTranslationProgress - translationDurationMs);
        // Don't show until then
        shouldShowTranslationProgress = false;
      }
      notif.updateTranslationProgress(
        shouldShowTranslationProgress,
        localizedTranslationProgressText,
      );
    }
  }

  shouldShowInfoBar(principal) {
    if (
      ![
        TranslationInfoBarStates.STATE_OFFER,
        TranslationInfoBarStates.STATE_TRANSLATING,
        TranslationInfoBarStates.STATE_TRANSLATED,
        TranslationInfoBarStates.STATE_ERROR,
      ].includes(
        this.translationBrowserChromeUiNotificationManager.uiState.infobarState,
      )
    ) {
      return false;
    }

    // Don't show the infobar if we have no language detection results yet
    if (
      !this.translationBrowserChromeUiNotificationManager.uiState
        .detectedLanguageResults
    ) {
      return false;
    }

    // Don't show the infobar if we couldn't confidently detect the language
    if (
      !this.translationBrowserChromeUiNotificationManager.uiState
        .detectedLanguageResults.confident
    ) {
      return false;
    }

    // Check if we should never show the infobar for this language.
    const neverForLangs = this.Services.prefs.getCharPref(
      "browser.translation.neverForLanguages",
    );
    if (
      neverForLangs
        .split(",")
        .includes(
          this.translationBrowserChromeUiNotificationManager.uiState
            .detectedLanguageResults.language,
        )
    ) {
      // TranslationTelemetry.recordAutoRejectedTranslationOffer();
      return false;
    }

    // or if we should never show the infobar for this domain.
    const perms = this.Services.perms;
    if (
      perms.testExactPermissionFromPrincipal(principal, "translate") ===
      perms.DENY_ACTION
    ) {
      // TranslationTelemetry.recordAutoRejectedTranslationOffer();
      return false;
    }

    return true;
  }

  hideURLBarIcon() {
    const chromeWin = this.browser.ownerGlobal;
    const PopupNotifications = chromeWin.PopupNotifications;
    const removeId = this.translationBrowserChromeUiNotificationManager.uiState
      .originalShown
      ? "translated"
      : "translate";
    const notification = PopupNotifications.getNotification(
      removeId,
      this.browser,
    );
    if (notification) {
      PopupNotifications.remove(notification);
    }
  }

  showURLBarIcon() {
    const chromeWin = this.browser.ownerGlobal;
    const PopupNotifications = chromeWin.PopupNotifications;
    const removeId = this.translationBrowserChromeUiNotificationManager.uiState
      .originalShown
      ? "translated"
      : "translate";
    const notification = PopupNotifications.getNotification(
      removeId,
      this.browser,
    );
    if (notification) {
      PopupNotifications.remove(notification);
    }

    const callback = (topic /* , aNewBrowser */) => {
      if (topic === "swapping") {
        const infoBarVisible = this.notificationBox.getNotificationWithValue(
          "translation",
        );
        if (infoBarVisible) {
          this.showTranslationInfoBar();
        }
        return true;
      }

      if (topic !== "showing") {
        return false;
      }
      const translationNotification = this.notificationBox.getNotificationWithValue(
        "translation",
      );
      if (translationNotification) {
        translationNotification.close();
      } else {
        this.showTranslationInfoBar();
      }
      return true;
    };

    const addId = this.translationBrowserChromeUiNotificationManager.uiState
      .originalShown
      ? "translate"
      : "translated";
    PopupNotifications.show(
      this.browser,
      addId,
      null,
      addId + "-notification-icon",
      null,
      null,
      { dismissed: true, eventCallback: callback },
    );
  }

  showTranslationInfoBarIfNotAlreadyShown() {
    const translationNotification = this.notificationBox.getNotificationWithValue(
      "translation",
    );
    if (!translationNotification && !this.translationInfoBarShown) {
      this.showTranslationInfoBar();
    }
    this.showURLBarIcon();
  }

  hideTranslationInfoBarIfShown() {
    const translationNotification = this.notificationBox.getNotificationWithValue(
      "translation",
    );
    if (translationNotification) {
      translationNotification.close();
    }
    this.hideURLBarIcon();
    this.translationInfoBarShown = false;
  }

  showTranslationInfoBar() {
    console.debug("showTranslationInfoBar");
    this.translationInfoBarShown = true;
    const notificationBox = this.notificationBox;
    const chromeWin = this.browser.ownerGlobal;
    const notif = notificationBox.appendNotification(
      "",
      "translation",
      null,
      notificationBox.PRIORITY_INFO_HIGH,
      null,
      null,
      `translation-notification-${chromeWin.now}`,
    );
    notif.init(this.translationBrowserChromeUiNotificationManager);
    this.translationBrowserChromeUiNotificationManager.infobarDisplayed(
      notif._getSourceLang(),
      notif._getTargetLang(),
    );
    return notif;
  }
}
