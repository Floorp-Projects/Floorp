/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

class MozTranslationNotification extends MozElements.Notification {
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
            <label data-l10n-id="translation-notification-this-page-is-in"/>
            <menulist class="notification-button" anonid="detectedLanguage">
              <menupopup/>
            </menulist>
            <label data-l10n-id="translation-notification-translate-this-page"/>
            <button class="notification-button primary" data-l10n-id="translation-notification-translate-button" anonid="translate" oncommand="this.closest('notification').translate();"/>
            <button class="notification-button" data-l10n-id="translation-notification-not-now-button" anonid="notNow" oncommand="this.closest('notification').closeCommand();"/>
          </hbox>
          <vbox class="translating-box" pack="center">
            <label data-l10n-id="translation-notification-translating-content"/>
          </vbox>
          <hbox class="translated-box" align="center">
            <label data-l10n-id="translation-notification-translated-from"/>
            <menulist class="notification-button" anonid="fromLanguage" oncommand="this.closest('notification').translate();">
              <menupopup/>
            </menulist>
            <label data-l10n-id="translation-notification-translated-to"/>
            <menulist class="notification-button" anonid="toLanguage" oncommand="this.closest('notification').translate();">
              <menupopup/>
            </menulist>
            <label data-l10n-id="translation-notification-translated-to-suffix"/>
            <button anonid="showOriginal" class="notification-button" data-l10n-id="translation-notification-show-original-button" oncommand="this.closest('notification').showOriginal();"/>
            <button anonid="showTranslation" class="notification-button" data-l10n-id="translation-notification-show-translation-button" oncommand="this.closest('notification').showTranslation();"/>
          </hbox>
          <hbox class="translation-error" align="center">
            <label data-l10n-id="translation-notification-error-translating"/>
            <button class="notification-button" data-l10n-id="translation-notification-try-again-button" anonid="tryAgain" oncommand="this.closest('notification').translate();"/>
          </hbox>
          <vbox class="translation-unavailable" pack="center">
            <label data-l10n-id="translation-notification-service-unavailable"/>
          </vbox>
        </deck>
        <spacer flex="1"/>
        <button type="menu" class="notification-button" anonid="options" data-l10n-id="translation-notification-options-menu">
          <menupopup class="translation-menupopup" onpopupshowing="this.closest('notification').optionsShowing();">
            <menuitem anonid="neverForLanguage" oncommand="this.closest('notification').neverForLanguage();"/>
            <menuitem anonid="neverForSite" oncommand="this.closest('notification').neverForSite();" data-l10n-id="translation-notification-options-never-for-site"/>
            <menuseparator/>
            <menuitem oncommand="openPreferences('paneGeneral');" data-l10n-id="translation-notification-options-preferences"/>
            <menuitem oncommand="this.closest('notification').openProviderAttribution();">
              <deck anonid="translationEngine" selectedIndex="0">
                <hbox class="translation-attribution">
                  <label/>
                  <image src="chrome://browser/content/microsoft-translator-attribution.png" aria-label="Microsoft Translator"/>
                  <label/>
                </hbox>
                <label class="translation-attribution"/>
              </deck>
            </menuitem>
          </menupopup>
        </button>
      </hbox>
      <toolbarbutton anonid="closeButton" ondblclick="event.stopPropagation();"
                     class="messageCloseButton close-icon tabbable"
                     data-l10n-id="close-notification-message"
                     oncommand="this.parentNode.closeCommand();"/>
    `;
  }

  connectedCallback() {
    MozXULElement.insertFTLIfNeeded("browser/translationNotification.ftl");
    MozXULElement.insertFTLIfNeeded("toolkit/global/notification.ftl");
    this.appendChild(this.constructor.fragment);

    for (let [propertyName, selector] of [
      ["details", "[anonid=details]"],
      ["messageImage", ".messageImage"],
      ["spacer", "[anonid=spacer]"],
    ]) {
      this[propertyName] = this.querySelector(selector);
    }
  }

  set state(val) {
    let deck = this._getAnonElt("translationStates");

    let activeElt = document.activeElement;
    if (activeElt && deck.contains(activeElt)) {
      activeElt.blur();
    }

    let stateName;
    for (let name of ["OFFER", "TRANSLATING", "TRANSLATED", "ERROR"]) {
      if (Translation["STATE_" + name] == val) {
        stateName = name.toLowerCase();
        break;
      }
    }
    this.setAttribute("state", stateName);

    if (val == Translation.STATE_TRANSLATED) {
      this._handleButtonHiding();
    }

    deck.selectedIndex = val;
  }

  get state() {
    return this._getAnonElt("translationStates").selectedIndex;
  }

  // aTranslation is the TranslationParent actor.
  init(aTranslation) {
    this.translation = aTranslation;

    let sortByLocalizedName = function(aList) {
      let names = Services.intl.getLanguageDisplayNames(undefined, aList);
      return aList
        .map((code, i) => [code, names[i]])
        .sort((a, b) => a[1].localeCompare(b[1]));
    };

    // Fill the lists of supported source languages.
    let detectedLanguage = this._getAnonElt("detectedLanguage");
    let fromLanguage = this._getAnonElt("fromLanguage");
    let sourceLanguages = sortByLocalizedName(
      Translation.supportedSourceLanguages
    );
    for (let [code, name] of sourceLanguages) {
      detectedLanguage.appendItem(name, code);
      fromLanguage.appendItem(name, code);
    }
    detectedLanguage.value = this.translation.detectedLanguage;

    // translatedFrom is only set if we have already translated this page.
    if (aTranslation.translatedFrom) {
      fromLanguage.value = aTranslation.translatedFrom;
    }

    // Fill the list of supported target languages.
    let toLanguage = this._getAnonElt("toLanguage");
    let targetLanguages = sortByLocalizedName(
      Translation.supportedTargetLanguages
    );
    for (let [code, name] of targetLanguages) {
      toLanguage.appendItem(name, code);
    }

    if (aTranslation.translatedTo) {
      toLanguage.value = aTranslation.translatedTo;
    }

    if (aTranslation.state) {
      this.state = aTranslation.state;
    }

    // Show attribution for the preferred translator.
    let engineIndex = Object.keys(Translation.supportedEngines).indexOf(
      Translation.translationEngine
    );
    // We currently only have attribution for the Bing and Yandex engines.
    if (engineIndex >= 0) {
      --engineIndex;
    }
    let attributionNode = this._getAnonElt("translationEngine");
    if (engineIndex != -1) {
      attributionNode.selectedIndex = engineIndex;
    } else {
      // Hide the attribution menuitem
      attributionNode.parentNode.hidden = true;
    }

    const kWelcomePref = "browser.translation.ui.welcomeMessageShown";
    if (
      Services.prefs.prefHasUserValue(kWelcomePref) ||
      this.translation.browser != gBrowser.selectedBrowser
    ) {
      return;
    }

    this.addEventListener(
      "transitionend",
      function() {
        // These strings are hardcoded because they need to reach beta
        // without riding the trains.
        let localizedStrings = {
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
        let strings = localizedStrings[locale];

        this._getAnonElt("welcomeHeadline").setAttribute("value", strings[0]);
        this._getAnonElt("welcomeBody").textContent = strings[1];
        this._getAnonElt("learnMore").setAttribute("value", strings[2]);
        this._getAnonElt("thanksButton").setAttribute("label", strings[3]);

        let panel = this._getAnonElt("welcomePanel");
        panel.openPopup(this._getAnonElt("messageImage"), "bottomleft topleft");

        Services.prefs.setBoolPref(kWelcomePref, true);
      },
      { once: true }
    );
  }

  _getAnonElt(aAnonId) {
    return this.querySelector("[anonid=" + aAnonId + "]");
  }

  translate() {
    if (this.state == Translation.STATE_OFFER) {
      this._getAnonElt("fromLanguage").value = this._getAnonElt(
        "detectedLanguage"
      ).value;
      this._getAnonElt("toLanguage").value = Translation.defaultTargetLanguage;
    }

    this.translation.translate(
      this._getAnonElt("fromLanguage").value,
      this._getAnonElt("toLanguage").value
    );
  }

  /**
   * To be called when the infobar should be closed per user's wish (e.g.
   * by clicking the notification's close button
   */
  closeCommand() {
    this.close();
    this.translation.infobarClosed();
  }

  _handleButtonHiding() {
    let originalShown = this.translation.originalShown;
    this._getAnonElt("showOriginal").hidden = originalShown;
    this._getAnonElt("showTranslation").hidden = !originalShown;
  }

  showOriginal() {
    this.translation.showOriginalContent();
    this._handleButtonHiding();
  }

  showTranslation() {
    this.translation.showTranslatedContent();
    this._handleButtonHiding();
  }

  optionsShowing() {
    // Get the source language name.
    let lang;
    if (this.state == Translation.STATE_OFFER) {
      lang = this._getAnonElt("detectedLanguage").value;
    } else {
      lang = this._getAnonElt("fromLanguage").value;

      // If we have never attempted to translate the page before the
      // service became unavailable, "fromLanguage" isn't set.
      if (!lang && this.state == Translation.STATE_UNAVAILABLE) {
        lang = this.translation.detectedLanguage;
      }
    }

    let langName = Services.intl.getLanguageDisplayNames(undefined, [lang])[0];

    // Set the label and accesskey on the menuitem.
    let item = this._getAnonElt("neverForLanguage");
    document.l10n.setAttributes(
      item,
      "translation-notification-options-never-for-language",
      { langName }
    );
    item.langCode = lang;

    // We may need to disable the menuitems if they have already been used.
    // Check if translation is already disabled for this language:
    let neverForLangs = Services.prefs.getCharPref(
      "browser.translation.neverForLanguages"
    );
    item.disabled = neverForLangs.split(",").includes(lang);

    // Check if translation is disabled for the domain:
    let principal = this.translation.browser.contentPrincipal;
    let perms = Services.perms;
    item = this._getAnonElt("neverForSite");
    item.disabled =
      perms.testExactPermissionFromPrincipal(principal, "translate") ==
      perms.DENY_ACTION;
  }

  neverForLanguage() {
    const kPrefName = "browser.translation.neverForLanguages";

    let val = Services.prefs.getCharPref(kPrefName);
    if (val) {
      val += ",";
    }
    val += this._getAnonElt("neverForLanguage").langCode;

    Services.prefs.setCharPref(kPrefName, val);

    this.closeCommand();
  }

  neverForSite() {
    let principal = this.translation.browser.contentPrincipal;
    let perms = Services.perms;
    perms.addFromPrincipal(principal, "translate", perms.DENY_ACTION);

    this.closeCommand();
  }

  openProviderAttribution() {
    Translation.openProviderAttribution();
  }
}

customElements.define("translation-notification", MozTranslationNotification, {
  extends: "notification",
});
