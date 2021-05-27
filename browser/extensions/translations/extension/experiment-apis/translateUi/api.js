/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* eslint-env commonjs */
/* eslint no-unused-vars: off */
/* eslint no-inner-declarations: off */
/* eslint no-console: ["warn", { allow: ["info", "warn", "error"] }] */
/* global ExtensionAPI */

"use strict";

this.translateUi = class extends ExtensionAPI {
  getAPI(context) {
    const { Services } = ChromeUtils.import(
      "resource://gre/modules/Services.jsm",
      {},
    );

    const now = Date.now();

    /* global TranslationBrowserChromeUiManager */
    Services.scriptloader.loadSubScript(
      context.extension.getURL(
        "experiment-apis/translateUi/TranslationBrowserChromeUiManager.js",
      ) +
        "?cachebuster=" +
        now,
    );
    /* global TranslationBrowserChromeUi */
    Services.scriptloader.loadSubScript(
      context.extension.getURL(
        "experiment-apis/translateUi/TranslationBrowserChromeUi.js",
      ) +
        "?cachebuster=" +
        now,
    );

    const { ExtensionCommon } = ChromeUtils.import(
      "resource://gre/modules/ExtensionCommon.jsm",
      {},
    );

    const { EventManager, EventEmitter } = ExtensionCommon;

    const apiEventEmitter = new EventEmitter();

    const { ExtensionUtils } = ChromeUtils.import(
      "resource://gre/modules/ExtensionUtils.jsm",
      {},
    );
    const { ExtensionError } = ExtensionUtils;

    /**
     * Boilerplate-reducing factory method translating between
     * apiEventEmitter.emit("translateUi.onFoo", ...args)
     * and the actual web extension event being emitted
     *
     * @param {string} eventRef the event reference, eg "onFoo"
     * @returns {void}
     */
    const eventManagerFactory = eventRef => {
      const eventId = `translateUi.${eventRef}`;
      return new EventManager({
        context,
        name: eventId,
        register: fire => {
          const listener = (event, ...args) => fire.async(...args);
          apiEventEmitter.on(eventRef, listener);
          return () => {
            apiEventEmitter.off(eventRef, listener);
          };
        },
      });
    };

    const { tabManager } = context.extension;
    const translationBrowserChromeUiInstancesByTabId = new Map();
    const getTranslationBrowserChromeUiInstanceByTabId = tabId => {
      if (translationBrowserChromeUiInstancesByTabId.has(tabId)) {
        return translationBrowserChromeUiInstancesByTabId.get(tabId);
      }
      const tab = tabManager.get(tabId);
      const { browser } = tab;
      const translationBrowserChromeUi = new TranslationBrowserChromeUi(
        Services,
        browser,
        context,
        apiEventEmitter,
        tabId,
      );
      translationBrowserChromeUiInstancesByTabId.set(
        tabId,
        translationBrowserChromeUi,
      );
      return translationBrowserChromeUi;
    };

    return {
      experiments: {
        translateUi: {
          /* Start reacting to translation state updates */
          start: async function start() {
            try {
              console.log("Called start()");

              console.log(
                "Inactivating legacy built-in translation feature (by setting browser.translation.ui.show and browser.translation.detectLanguage to false)",
              );
              Services.prefs.setBoolPref(`browser.translation.ui.show`, false);
              Services.prefs.setBoolPref(
                `browser.translation.detectLanguage`,
                false,
              );

              return undefined;
            } catch (error) {
              // Surface otherwise silent or obscurely reported errors
              console.error(error.message, error.stack);
              throw new ExtensionError(error.message);
            }
          },

          /* Set current ui state */
          setUiState: async function setUiState(tabId, uiState) {
            try {
              // console.log("Called setUiState(tabId, uiState)", {tabId,uiState});
              const translationBrowserChromeUi = getTranslationBrowserChromeUiInstanceByTabId(
                tabId,
              );
              translationBrowserChromeUi.onUiStateUpdate(uiState);
              return undefined;
            } catch (error) {
              // Surface otherwise silent or obscurely reported errors
              console.error(error.message, error.stack);
              throw new ExtensionError(error.message);
            }
          },

          /* Stop reacting to translation state updates */
          stop: async function stop() {
            try {
              console.log("Called stop()");
              return undefined;
            } catch (error) {
              // Surface otherwise silent or obscurely reported errors
              console.error(error.message, error.stack);
              throw new ExtensionError(error.message);
            }
          },

          /* Event boilerplate with listeners that forwards all but the first argument to the web extension event */
          onInfoBarDisplayed: eventManagerFactory("onInfoBarDisplayed").api(),
          onSelectTranslateTo: eventManagerFactory("onSelectTranslateTo").api(),
          onSelectTranslateFrom: eventManagerFactory(
            "onSelectTranslateFrom",
          ).api(),
          onInfoBarClosed: eventManagerFactory("onInfoBarClosed").api(),
          onNeverTranslateSelectedLanguage: eventManagerFactory(
            "onNeverTranslateSelectedLanguage",
          ).api(),
          onNeverTranslateThisSite: eventManagerFactory(
            "onNeverTranslateThisSite",
          ).api(),
          onShowOriginalButtonPressed: eventManagerFactory(
            "onShowOriginalButtonPressed",
          ).api(),
          onShowTranslatedButtonPressed: eventManagerFactory(
            "onShowTranslatedButtonPressed",
          ).api(),
          onTranslateButtonPressed: eventManagerFactory(
            "onTranslateButtonPressed",
          ).api(),
          onNotNowButtonPressed: eventManagerFactory(
            "onNotNowButtonPressed",
          ).api(),
        },
      },
    };
  }
};
