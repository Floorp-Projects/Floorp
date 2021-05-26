/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/* global MozElements */

"use strict";

window.MozTranslationNotification = class extends MozElements.Notification {
  static get markup() {
    return `
      <hbox anonid="details" align="center" flex="1">
        <image class="messageImage"/>
        <panel anonid="welcomePanel" class="translation-welcome-panel" type="arrow" align="start">
          <image class="translation-welcome-logo"/>
          <vbox flex="1" class="translation-welcome-content">
            <description class="translation-welcome-headline" anonid="welcomeHeadline"/>
            <description class="translation-welcome-body" anonid="welcomeBody"/>
            <hbox align="center">
              <label anonid="learnMore" class="plain" onclick="openTrustedLinkIn('https://support.mozilla.org/kb/automatic-translation', 'tab'); this.parentNode.parentNode.parentNode.hidePopup();" is="text-link"/>
              <spacer flex="1"/>
              <button anonid="thanksButton" onclick="this.parentNode.parentNode.parentNode.hidePopup();"/>
            </hbox>
          </vbox>
        </panel>
        <deck anonid="translationStates" selectedIndex="0">
          <hbox class="translate-offer-box" align="center">
            <label value="&translation.thisPageIsIn.label;"/>
            <menulist class="notification-button" anonid="detectedLanguage">
              <menupopup/>
            </menulist>
            <label value="&translation.translateThisPage.label;"/>
            <button class="notification-button primary" label="&translation.translate.button;" anonid="translate" oncommand="this.closest('notification').translate();"/>
            <button class="notification-button" label="&translation.notNow.button;" anonid="notNow" oncommand="this.closest('notification').notNow();"/>
          </hbox>
          <vbox class="translating-box" pack="center">
            <hbox><label value="&translation.translatingContent.label;"/><label anonid="progress-label" value=""/></hbox>
          </vbox>
          <hbox class="translated-box" align="center">
            <label value="&translation.translatedFrom.label;"/>
            <menulist class="notification-button" anonid="fromLanguage" oncommand="this.closest('notification').fromLanguageChanged();">
              <menupopup/>
            </menulist>
            <label value="&translation.translatedTo.label;"/>
            <menulist class="notification-button" anonid="toLanguage" oncommand="this.closest('notification').toLanguageChanged();">
              <menupopup/>
            </menulist>
            <label value="&translation.translatedToSuffix.label;"/>
            <button anonid="showOriginal" class="notification-button" label="&translation.showOriginal.button;" oncommand="this.closest('notification').showOriginal();"/>
            <button anonid="showTranslation" class="notification-button" label="&translation.showTranslation.button;" oncommand="this.closest('notification').showTranslation();"/>
          </hbox>
          <hbox class="translation-error" align="center">
            <label value="&translation.errorTranslating.label;"/>
            <button class="notification-button" label="&translation.tryAgain.button;" anonid="tryAgain" oncommand="this.closest('notification').translate();"/>
          </hbox>
          <vbox class="translation-unavailable" pack="center">
            <label value="&translation.serviceUnavailable.label;"/>
          </vbox>
        </deck>
        <spacer flex="1"/>
        <button type="menu" class="notification-button" anonid="options" label="&translation.options.menu;">
          <menupopup class="translation-menupopup" onpopupshowing="this.closest('notification').optionsShowing();">
            <menuitem anonid="neverForLanguage" oncommand="this.closest('notification').neverForLanguage();"/>
            <menuitem anonid="neverForSite" oncommand="this.closest('notification').neverForSite();" label="&translation.options.neverForSite.label;" accesskey="&translation.options.neverForSite.accesskey;"/>
            <menuseparator/>
            <menuitem oncommand="openPreferences('paneGeneral');" label="&translation.options.preferences.label;" accesskey="&translation.options.preferences.accesskey;"/>
          </menupopup>
        </button>
      </hbox>
      <toolbarbutton anonid="closeButton" ondblclick="event.stopPropagation();"
                     class="messageCloseButton close-icon tabbable"
                     tooltiptext="&closeNotification.tooltip;"
                     oncommand="this.parentNode.closeCommand();"/>
    `;
  }

  static get entities() {
    return [
      "chrome://global/locale/notification.dtd",
      "chrome://browser/locale/translation.dtd",
    ];
  }

  connectedCallback() {
    this.appendChild(this.constructor.fragment);

    for (const [propertyName, selector] of [
      ["details", "[anonid=details]"],
      ["messageImage", ".messageImage"],
      ["spacer", "[anonid=spacer]"],
    ]) {
      this[propertyName] = this.querySelector(selector);
    }
  }

  async updateTranslationProgress(
    shouldShowTranslationProgress,
    localizedTranslationProgressText,
  ) {
    const progressLabelValue = shouldShowTranslationProgress
      ? localizedTranslationProgressText
      : "";
    this._getAnonElt("progress-label").setAttribute(
      "value",
      progressLabelValue,
    );
  }

  set state(val) {
    const deck = this._getAnonElt("translationStates");

    const activeElt = document.activeElement;
    if (activeElt && deck.contains(activeElt)) {
      activeElt.blur();
    }

    let stateName;
    for (const name of ["OFFER", "TRANSLATING", "TRANSLATED", "ERROR"]) {
      if (Translation["STATE_" + name] === val) {
        stateName = name.toLowerCase();
        break;
      }
    }
    this.setAttribute("state", stateName);

    if (val === this.translation.TranslationInfoBarStates.STATE_TRANSLATED) {
      this._handleButtonHiding();
    }

    deck.selectedIndex = val;
  }

  get state() {
    return this._getAnonElt("translationStates").selectedIndex;
  }

  init(translationBrowserChromeUiNotificationManager) {
    this.translation = translationBrowserChromeUiNotificationManager;

    const sortByLocalizedName = function(list) {
      const names = Services.intl.getLanguageDisplayNames(undefined, list);
      return list
        .map((code, i) => [code, names[i]])
        .sort((a, b) => a[1].localeCompare(b[1]));
    };

    // Fill the lists of supported source languages.
    const detectedLanguage = this._getAnonElt("detectedLanguage");
    const fromLanguage = this._getAnonElt("fromLanguage");
    const sourceLanguages = sortByLocalizedName(
      this.translation.uiState.supportedSourceLanguages,
    );
    for (const [code, name] of sourceLanguages) {
      detectedLanguage.appendItem(name, code);
      fromLanguage.appendItem(name, code);
    }
    detectedLanguage.value = this.translation.uiState.detectedLanguageResults.language;

    // translatedFrom is only set if we have already translated this page.
    if (translationBrowserChromeUiNotificationManager.uiState.translatedFrom) {
      fromLanguage.value =
        translationBrowserChromeUiNotificationManager.uiState.translatedFrom;
    }

    // Fill the list of supported target languages.
    const toLanguage = this._getAnonElt("toLanguage");
    const targetLanguages = sortByLocalizedName(
      this.translation.uiState.supportedTargetLanguages,
    );
    for (const [code, name] of targetLanguages) {
      toLanguage.appendItem(name, code);
    }

    if (translationBrowserChromeUiNotificationManager.uiState.translatedTo) {
      toLanguage.value =
        translationBrowserChromeUiNotificationManager.uiState.translatedTo;
    }

    if (translationBrowserChromeUiNotificationManager.uiState.infobarState) {
      this.state =
        translationBrowserChromeUiNotificationManager.uiState.infobarState;
    }

    /*
    // The welcome popup/notification is currently disabled
    const kWelcomePref = "browser.translation.ui.welcomeMessageShown";
    if (
      Services.prefs.prefHasUserValue(kWelcomePref) ||
      this.translation.browser !== gBrowser.selectedBrowser
    ) {
      return;
    }

    this.addEventListener(
      "transitionend",
      function() {
        // These strings are hardcoded because they need to reach beta
        // without riding the trains.
        const localizedStrings = {
          en: [
            "Hey look! It's something new!",
            "Now the Web is even more accessible with our new in-page translation feature. Click the translate button to try it!",
            "Learn more.",
            "Thanks",
          ],
          "es-AR": [
            "\xA1Mir\xE1! \xA1Hay algo nuevo!",
            "Ahora la web es a\xFAn m\xE1s accesible con nuestra nueva funcionalidad de traducci\xF3n integrada. \xA1Hac\xE9 clic en el bot\xF3n traducir para probarla!",
            "Conoc\xE9 m\xE1s.",
            "Gracias",
          ],
          "es-ES": [
            "\xA1Mira! \xA1Hay algo nuevo!",
            "Con la nueva funcionalidad de traducci\xF3n integrada, ahora la Web es a\xFAn m\xE1s accesible. \xA1Pulsa el bot\xF3n Traducir y pru\xE9bala!",
            "M\xE1s informaci\xF3n.",
            "Gracias",
          ],
          pl: [
            "Sp\xF3jrz tutaj! To co\u015B nowego!",
            "Sie\u0107 sta\u0142a si\u0119 w\u0142a\u015Bnie jeszcze bardziej dost\u0119pna dzi\u0119ki opcji bezpo\u015Bredniego t\u0142umaczenia stron. Kliknij przycisk t\u0142umaczenia, aby spr\xF3bowa\u0107!",
            "Dowiedz si\u0119 wi\u0119cej",
            "Dzi\u0119kuj\u0119",
          ],
          tr: [
            "Bak\u0131n, burada yeni bir \u015Fey var!",
            "Yeni sayfa i\xE7i \xE7eviri \xF6zelli\u011Fimiz sayesinde Web art\u0131k \xE7ok daha anla\u015F\u0131l\u0131r olacak. Denemek i\xE7in \xC7evir d\xFC\u011Fmesine t\u0131klay\u0131n!",
            "Daha fazla bilgi al\u0131n.",
            "Te\u015Fekk\xFCrler",
          ],
          vi: [
            "Nh\xECn n\xE0y! \u0110\u1ED3 m\u1EDBi!",
            "Gi\u1EDD \u0111\xE2y ch\xFAng ta c\xF3 th\u1EC3 ti\u1EBFp c\u1EADn web d\u1EC5 d\xE0ng h\u01A1n n\u1EEFa v\u1EDBi t\xEDnh n\u0103ng d\u1ECBch ngay trong trang.  Hay nh\u1EA5n n\xFAt d\u1ECBch \u0111\u1EC3 th\u1EED!",
            "T\xECm hi\u1EC3u th\xEAm.",
            "C\u1EA3m \u01A1n",
          ],
        };

        let locale = Services.locale.appLocaleAsBCP47;
        if (!(locale in localizedStrings)) {
          locale = "en";
        }
        const strings = localizedStrings[locale];

        this._getAnonElt("welcomeHeadline").setAttribute("value", strings[0]);
        this._getAnonElt("welcomeBody").textContent = strings[1];
        this._getAnonElt("learnMore").setAttribute("value", strings[2]);
        this._getAnonElt("thanksButton").setAttribute("label", strings[3]);

        // TODO: Figure out why this shows a strangely rendered popup at the corner of the window instead next to the URL bar
        const panel = this._getAnonElt("welcomePanel");
        panel.openPopup(
          this._getAnonElt("messageImage"),
          "bottomcenter topleft",
        );

        Services.prefs.setBoolPref(kWelcomePref, true);
      },
      { once: true },
    );
     */
  }

  _getAnonElt(anonId) {
    return this.querySelector("[anonid=" + anonId + "]");
  }

  fromLanguageChanged() {
    this.translation.fromLanguageChanged(
      this._getSourceLang(),
      this._getTargetLang(),
    );
    this.translate();
  }

  toLanguageChanged() {
    this.translation.toLanguageChanged(
      this._getSourceLang(),
      this._getTargetLang(),
    );
    this.translate();
  }

  translate() {
    const from = this._getSourceLang();
    const to = this._getTargetLang();

    // Initiate translation
    this.translation.translate(from, to);

    // Store the values used in the translation in the from and to inputs
    if (
      this.translation.uiState.infobarState ===
      this.translation.TranslationInfoBarStates.STATE_OFFER
    ) {
      this._getAnonElt("fromLanguage").value = from;
      this._getAnonElt("toLanguage").value = to;
    }
  }

  /**
   * To be called when the infobar should be closed per user's wish (e.g.
   * by clicking the notification's close button, the not now button or choosing never to translate)
   */
  closeCommand() {
    const from = this._getSourceLang();
    const to = this._getTargetLang();
    this.close();
    this.translation.infobarClosed(from, to);
  }

  /**
   * To be called when the infobar should be closed per user's wish
   * by clicking the Not now button
   */
  notNow() {
    this.translation.notNow(this._getSourceLang(), this._getTargetLang());
    this.closeCommand();
  }

  _handleButtonHiding() {
    const originalShown = this.translation.uiState.originalShown;
    this._getAnonElt("showOriginal").hidden = originalShown;
    this._getAnonElt("showTranslation").hidden = !originalShown;
  }

  showOriginal() {
    this.translation.showOriginalContent(
      this._getSourceLang(),
      this._getTargetLang(),
    );
    this._handleButtonHiding();
  }

  showTranslation() {
    this.translation.showTranslatedContent(
      this._getSourceLang(),
      this._getTargetLang(),
    );
    this._handleButtonHiding();
  }

  _getSourceLang() {
    let lang;
    if (
      this.translation.uiState.infobarState ===
      this.translation.TranslationInfoBarStates.STATE_OFFER
    ) {
      lang = this._getAnonElt("detectedLanguage").value;
    } else {
      lang = this._getAnonElt("fromLanguage").value;

      // If we have never attempted to translate the page before the
      // service became unavailable, "fromLanguage" isn't set.
      if (
        !lang &&
        this.translation.uiState.infobarState ===
          this.translation.TranslationInfoBarStates.STATE_UNAVAILABLE
      ) {
        lang = this.translation.uiState.defaultTargetLanguage;
      }
    }
    if (!lang) {
      throw new Error("Source language is not defined");
    }
    return lang;
  }

  _getTargetLang() {
    return (
      this._getAnonElt("toLanguage").value ||
      this.translation.uiState.defaultTargetLanguage
    );
  }

  optionsShowing() {
    const lang = this._getSourceLang();

    // Get the source language name.
    const langName = Services.intl.getLanguageDisplayNames(undefined, [
      lang,
    ])[0];

    // Set the label and accesskey on the menuitem.
    const bundle = Services.strings.createBundle(
      "chrome://browser/locale/translation.properties",
    );
    let item = this._getAnonElt("neverForLanguage");
    const kStrId = "translation.options.neverForLanguage";
    item.setAttribute(
      "label",
      bundle.formatStringFromName(kStrId + ".label", [langName]),
    );
    item.setAttribute(
      "accesskey",
      bundle.GetStringFromName(kStrId + ".accesskey"),
    );

    // We may need to disable the menuitems if they have already been used.
    // Check if translation is already disabled for this language:
    const neverForLangs = Services.prefs.getCharPref(
      "browser.translation.neverForLanguages",
    );
    item.disabled = neverForLangs.split(",").includes(lang);

    // Check if translation is disabled for the domain:
    const principal = this.translation.browser.contentPrincipal;
    const perms = Services.perms;
    item = this._getAnonElt("neverForSite");
    item.disabled =
      perms.testExactPermissionFromPrincipal(principal, "translate") ===
      perms.DENY_ACTION;
  }

  neverForLanguage() {
    const kPrefName = "browser.translation.neverForLanguages";
    const sourceLang = this._getSourceLang();

    let val = Services.prefs.getCharPref(kPrefName);
    if (val) {
      val += ",";
    }
    val += sourceLang;

    Services.prefs.setCharPref(kPrefName, val);

    this.translation.neverForLanguage(
      this._getSourceLang(),
      this._getTargetLang(),
    );
    this.closeCommand();
  }

  neverForSite() {
    const principal = this.translation.browser.contentPrincipal;
    const perms = Services.perms;
    perms.addFromPrincipal(principal, "translate", perms.DENY_ACTION);

    this.translation.neverForSite(this._getSourceLang(), this._getTargetLang());
    this.closeCommand();
  }
};

customElements.define(
  `translation-notification-${window.now}`,
  window.MozTranslationNotification,
  {
    extends: "notification",
  },
);
