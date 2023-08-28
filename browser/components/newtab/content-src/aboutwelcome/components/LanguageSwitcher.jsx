/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

import React, { useState, useEffect } from "react";
import { Localized } from "./MSLocalized";
import { AboutWelcomeUtils } from "../../lib/aboutwelcome-utils";

/**
 * The language switcher implements a hook that should be placed at a higher level
 * than the actual language switcher component, as it needs to preemptively fetch
 * and install langpacks for the user if there is a language mismatch screen.
 */
export function useLanguageSwitcher(
  appAndSystemLocaleInfo,
  screens,
  screenIndex,
  setScreenIndex
) {
  const languageMismatchScreenIndex = screens.findIndex(
    ({ id }) => id === "AW_LANGUAGE_MISMATCH"
  );
  const screen = screens[languageMismatchScreenIndex];

  // Ensure fluent messages have the negotiatedLanguage args set, as they are rendered
  // before the negotiatedLanguage is known. If the arg isn't present then Firefox will
  // crash in development mode.
  useEffect(() => {
    if (screen?.content?.languageSwitcher) {
      for (const text of Object.values(screen.content.languageSwitcher)) {
        if (text?.args && text.args.negotiatedLanguage === undefined) {
          text.args.negotiatedLanguage = "";
        }
      }
    }
  }, [screen]);

  // If there is a mismatch, then Firefox can negotiate a better langpack to offer
  // the user.
  const [negotiatedLanguage, setNegotiatedLanguage] = useState(null);
  useEffect(
    function getNegotiatedLanguage() {
      if (!appAndSystemLocaleInfo) {
        return;
      }
      if (appAndSystemLocaleInfo.matchType !== "language-mismatch") {
        // There is no language mismatch, so there is no need to negotiate a langpack.
        return;
      }

      (async () => {
        const { langPack, langPackDisplayName } =
          await window.AWNegotiateLangPackForLanguageMismatch(
            appAndSystemLocaleInfo
          );
        if (langPack) {
          setNegotiatedLanguage({
            langPackDisplayName,
            appDisplayName: appAndSystemLocaleInfo.displayNames.appLanguage,
            langPack,
            requestSystemLocales: [
              langPack.target_locale,
              appAndSystemLocaleInfo.appLocaleRaw,
            ],
            originalAppLocales: [appAndSystemLocaleInfo.appLocaleRaw],
          });
        } else {
          setNegotiatedLanguage({
            langPackDisplayName: null,
            appDisplayName: null,
            langPack: null,
            requestSystemLocales: null,
          });
        }
      })();
    },
    [appAndSystemLocaleInfo]
  );

  /**
   * @type {
   *  "before-installation"
   *  | "installing"
   *  | "installed"
   *  | "installation-error"
   *  | "none-available"
   * }
   */
  const [langPackInstallPhase, setLangPackInstallPhase] = useState(
    "before-installation"
  );
  useEffect(
    function ensureLangPackInstalled() {
      if (!negotiatedLanguage) {
        // There are no negotiated languages to download yet.
        return;
      }
      setLangPackInstallPhase("installing");
      window
        .AWEnsureLangPackInstalled(negotiatedLanguage, screen?.content)
        .then(
          content => {
            // Update screen content with strings that might have changed.
            screen.content = content;
            setLangPackInstallPhase("installed");
          },
          error => {
            console.error(error);
            setLangPackInstallPhase("installation-error");
          }
        );
    },
    [negotiatedLanguage, screen]
  );

  const [languageFilteredScreens, setLanguageFilteredScreens] =
    useState(screens);
  useEffect(
    function filterScreen() {
      // Remove the language screen if it exists (already removed for no live
      // reload) and we either don't-need-to or can't switch.
      if (
        screen &&
        (appAndSystemLocaleInfo?.matchType !== "language-mismatch" ||
          negotiatedLanguage?.langPack === null)
      ) {
        if (screenIndex > languageMismatchScreenIndex) {
          setScreenIndex(screenIndex - 1);
        }
        setLanguageFilteredScreens(
          screens.filter(s => s.id !== "AW_LANGUAGE_MISMATCH")
        );
      } else {
        setLanguageFilteredScreens(screens);
      }
    },
    [
      appAndSystemLocaleInfo?.matchType,
      languageMismatchScreenIndex,
      negotiatedLanguage,
      screen,
      screenIndex,
      screens,
      setScreenIndex,
    ]
  );

  return {
    negotiatedLanguage,
    langPackInstallPhase,
    languageFilteredScreens,
  };
}

/**
 * The language switcher is a separate component as it needs to perform some asynchronous
 * network actions such as retrieving the list of langpacks available, and downloading
 * a new langpack. On a fast connection, this won't be noticeable, but on slow or unreliable
 * internet this may fail for a user.
 */
export function LanguageSwitcher(props) {
  const {
    content,
    handleAction,
    negotiatedLanguage,
    langPackInstallPhase,
    messageId,
  } = props;

  const [isAwaitingLangpack, setIsAwaitingLangpack] = useState(false);

  // Determine the status of the langpack installation.
  useEffect(() => {
    if (isAwaitingLangpack && langPackInstallPhase !== "installing") {
      window.AWSetRequestedLocales(negotiatedLanguage.requestSystemLocales);
      requestAnimationFrame(() => {
        handleAction(
          // Simulate the click event.
          { currentTarget: { value: "download_complete" } }
        );
      });
    }
  }, [
    handleAction,
    isAwaitingLangpack,
    langPackInstallPhase,
    negotiatedLanguage?.requestSystemLocales,
  ]);

  let showWaitingScreen = false;
  let showPreloadingScreen = false;
  let showReadyScreen = false;

  if (isAwaitingLangpack && langPackInstallPhase !== "installed") {
    showWaitingScreen = true;
  } else if (langPackInstallPhase === "before-installation") {
    showPreloadingScreen = true;
  } else {
    showReadyScreen = true;
  }

  // Use {display: "none"} rather than if statements to prevent layout thrashing with
  // the localized text elements rendering as blank, then filling in the text.
  return (
    <div className="action-buttons language-switcher-container">
      {/* Pre-loading screen */}
      <div style={{ display: showPreloadingScreen ? "block" : "none" }}>
        <button
          className="primary"
          value="primary_button"
          disabled={true}
          type="button"
        >
          <img
            className="language-loader"
            src="chrome://browser/skin/tabbrowser/tab-connecting.png"
            alt=""
          />
          <Localized text={content.languageSwitcher.waiting} />
        </button>
        <div className="secondary-cta">
          <Localized text={content.languageSwitcher.skip}>
            <button
              value="decline_waiting"
              type="button"
              className="secondary text-link arrow-icon"
              onClick={handleAction}
            />
          </Localized>
        </div>
      </div>
      {/* Waiting to download the language screen. */}
      <div style={{ display: showWaitingScreen ? "block" : "none" }}>
        <button
          className="primary"
          value="primary_button"
          disabled={true}
          type="button"
        >
          <img
            className="language-loader"
            src="chrome://browser/skin/tabbrowser/tab-connecting.png"
            alt=""
          />
          <Localized text={content.languageSwitcher.downloading} />
        </button>
        <div className="secondary-cta">
          <Localized text={content.languageSwitcher.cancel}>
            <button
              type="button"
              className="secondary text-link"
              onClick={() => {
                setIsAwaitingLangpack(false);
                handleAction({
                  currentTarget: { value: "cancel_waiting" },
                });
              }}
            />
          </Localized>
        </div>
      </div>
      {/* The typical ready screen. */}
      <div style={{ display: showReadyScreen ? "block" : "none" }}>
        <div>
          <button
            className="primary"
            value="primary_button"
            onClick={() => {
              AboutWelcomeUtils.sendActionTelemetry(
                messageId,
                "download_langpack"
              );
              setIsAwaitingLangpack(true);
            }}
          >
            {content.languageSwitcher.switch ? (
              <Localized text={content.languageSwitcher.switch} />
            ) : (
              // This is the localized name from the Intl.DisplayNames API.
              negotiatedLanguage?.langPackDisplayName
            )}
          </button>
        </div>
        <div>
          <button
            type="button"
            className="primary"
            value="decline"
            onClick={event => {
              window.AWSetRequestedLocales(
                negotiatedLanguage.originalAppLocales
              );
              handleAction(event);
            }}
          >
            {content.languageSwitcher.continue ? (
              <Localized text={content.languageSwitcher.continue} />
            ) : (
              // This is the localized name from the Intl.DisplayNames API.
              negotiatedLanguage?.appDisplayName
            )}
          </button>
        </div>
      </div>
    </div>
  );
}
