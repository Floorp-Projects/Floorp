/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint no-unused-vars: ["error", { "varsIgnorePattern": "(TranslationBrowserChromeUiNotificationManager)" }]*/

class TranslationBrowserChromeUiNotificationManager {
  constructor(browser, apiEventEmitter, tabId, TranslationInfoBarStates) {
    this.uiState = null;
    this.TranslationInfoBarStates = TranslationInfoBarStates;
    this.apiEventEmitter = apiEventEmitter;
    this.tabId = tabId;
    this.browser = browser;
  }

  infobarDisplayed(from, to) {
    console.log("infobarDisplayed", { from, to });
    this.apiEventEmitter.emit("onInfoBarDisplayed", this.tabId, from, to);
  }

  toLanguageChanged(from, newTo) {
    console.log("toLanguageChanged", { from, newTo });
    this.apiEventEmitter.emit("onSelectTranslateTo", this.tabId, from, newTo);
  }

  fromLanguageChanged(newFrom, to) {
    console.log("fromLanguageChanged", { newFrom, to });
    this.apiEventEmitter.emit("onSelectTranslateFrom", this.tabId, newFrom, to);
  }

  infobarClosed(from, to) {
    console.log("infobarClosed", { from, to });
    this.apiEventEmitter.emit("onInfoBarClosed", this.tabId, from, to);
  }

  neverForLanguage(from, to) {
    console.log("neverForLanguage", { from, to });
    this.apiEventEmitter.emit(
      "onNeverTranslateSelectedLanguage",
      this.tabId,
      from,
      to,
    );
  }

  neverForSite(from, to) {
    console.log("neverForSite", { from, to });
    this.apiEventEmitter.emit("onNeverTranslateThisSite", this.tabId, from, to);
  }

  showOriginalContent(from, to) {
    console.log("showOriginalContent", { from, to });
    this.apiEventEmitter.emit(
      "onShowOriginalButtonPressed",
      this.tabId,
      from,
      to,
    );
  }

  showTranslatedContent(from, to) {
    console.log("showTranslatedContent", { from, to });
    this.apiEventEmitter.emit(
      "onShowTranslatedButtonPressed",
      this.tabId,
      from,
      to,
    );
  }

  translate(from, to) {
    console.log("translate", { from, to });
    this.apiEventEmitter.emit("onTranslateButtonPressed", this.tabId, from, to);
  }

  notNow(from, to) {
    console.log("notNow", { from, to });
    this.apiEventEmitter.emit("onNotNowButtonPressed", this.tabId, from, to);
  }
}
