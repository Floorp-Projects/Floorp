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
        const langPack = await window.AWNegotiateLangPackForLanguageMismatch(
          appAndSystemLocaleInfo
        );
        if (langPack) {
          // Convert the BCP 47 identifiers into the proper display names.
          // e.g. "fr-CA" -> "Canadian French".
          const displayNames = new Intl.DisplayNames(
            appAndSystemLocaleInfo.appLocaleRaw,
            { type: "language" }
          );

          setNegotiatedLanguage({
            displayName: displayNames.of(langPack.target_locale),
            langPack,
            requestSystemLocales: [
              langPack.target_locale,
              appAndSystemLocaleInfo.appLocaleRaw,
            ],
          });
        } else {
          setNegotiatedLanguage({
            displayName: null,
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
      window.AWEnsureLangPackInstalled(negotiatedLanguage.langPack).then(
        () => {
          setLangPackInstallPhase("installed");
        },
        error => {
          console.error(error);
          setLangPackInstallPhase("installation-error");
        }
      );
    },
    [negotiatedLanguage]
  );

  const shouldHideLanguageSwitcher =
    screen && appAndSystemLocaleInfo?.matchType !== "language-mismatch";

  const [languageFilteredScreens, setLanguageFilteredScreens] = useState(
    screens
  );
  useEffect(
    function filterScreen() {
      if (shouldHideLanguageSwitcher || negotiatedLanguage?.langPack === null) {
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
    [screens, negotiatedLanguage]
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
  }, [isAwaitingLangpack, langPackInstallPhase]);

  // The message args are the localized language names.
  const withMessageArgs = obj => {
    const displayName = negotiatedLanguage?.displayName;
    if (displayName) {
      return {
        ...obj,
        args: {
          ...obj.args,
          negotiatedLanguage: displayName,
        },
      };
    }
    return obj;
  };

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
    <>
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
              className="secondary text-link"
              onClick={handleAction}
            />
          </Localized>
        </div>
      </div>
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
          <Localized
            text={withMessageArgs(content.languageSwitcher.downloading)}
          />
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
      <div style={{ display: showReadyScreen ? "block" : "none" }}>
        <div>
          <Localized text={withMessageArgs(content.languageSwitcher.switch)}>
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
            />
          </Localized>
        </div>
        <div className="secondary-cta">
          <Localized text={content.languageSwitcher.not_now}>
            <button
              type="button"
              className="secondary text-link"
              value="decline"
              onClick={handleAction}
            />
          </Localized>
        </div>
      </div>
    </>
  );
}
