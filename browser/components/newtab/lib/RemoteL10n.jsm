/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * The downloaded Fluent file is located in this sub-directory of the local
 * profile directory.
 */
const USE_REMOTE_L10N_PREF =
  "browser.newtabpage.activity-stream.asrouter.useRemoteL10n";

/**
 * All supported locales for remote l10n
 *
 * This is used by ASRouter.jsm to check if the locale is supported before
 * issuing the request for remote fluent files to RemoteSettings.
 *
 * Note:
 *   * this is generated based on "browser/locales/all-locales" as l10n doesn't
 *     provide an API to fetch that list
 *
 *   * this list doesn't include "en-US", though "en-US" is well supported and
 *     `_RemoteL10n.isLocaleSupported()` will handle it properly
 */
const ALL_LOCALES = new Set([
  "ach",
  "af",
  "an",
  "ar",
  "ast",
  "az",
  "be",
  "bg",
  "bn",
  "bo",
  "br",
  "brx",
  "bs",
  "ca",
  "ca-valencia",
  "cak",
  "ckb",
  "cs",
  "cy",
  "da",
  "de",
  "dsb",
  "el",
  "en-CA",
  "en-GB",
  "eo",
  "es-AR",
  "es-CL",
  "es-ES",
  "es-MX",
  "et",
  "eu",
  "fa",
  "ff",
  "fi",
  "fr",
  "fy-NL",
  "ga-IE",
  "gd",
  "gl",
  "gn",
  "gu-IN",
  "he",
  "hi-IN",
  "hr",
  "hsb",
  "hu",
  "hy-AM",
  "hye",
  "ia",
  "id",
  "is",
  "it",
  "ja",
  "ja-JP-mac",
  "ka",
  "kab",
  "kk",
  "km",
  "kn",
  "ko",
  "lij",
  "lo",
  "lt",
  "ltg",
  "lv",
  "meh",
  "mk",
  "mr",
  "ms",
  "my",
  "nb-NO",
  "ne-NP",
  "nl",
  "nn-NO",
  "oc",
  "pa-IN",
  "pl",
  "pt-BR",
  "pt-PT",
  "rm",
  "ro",
  "ru",
  "scn",
  "si",
  "sk",
  "sl",
  "son",
  "sq",
  "sr",
  "sv-SE",
  "szl",
  "ta",
  "te",
  "th",
  "tl",
  "tr",
  "trs",
  "uk",
  "ur",
  "uz",
  "vi",
  "wo",
  "xh",
  "zh-CN",
  "zh-TW",
]);

class _RemoteL10n {
  constructor() {
    this._l10n = null;
  }

  createElement(doc, elem, options = {}) {
    let node;
    if (options.content && options.content.string_id) {
      node = doc.createElement("remote-text");
    } else {
      node = doc.createElementNS("http://www.w3.org/1999/xhtml", elem);
    }
    if (options.classList) {
      node.classList.add(options.classList);
    }
    this.setString(node, options);

    return node;
  }

  // If `string_id` is present it means we are relying on fluent for translations.
  // Otherwise, we have a vanilla string.
  setString(el, { content, attributes = {} }) {
    if (content && content.string_id) {
      for (let [fluentId, value] of Object.entries(attributes)) {
        el.setAttribute(`fluent-variable-${fluentId}`, value);
      }
      el.setAttribute("fluent-remote-id", content.string_id);
    } else {
      el.textContent = content;
    }
  }

  /**
   * Creates a new DOMLocalization instance with the Fluent file from Remote Settings.
   *
   * Note: it will use the local Fluent file in any of following cases:
   *   * the remote Fluent file is not available
   *   * it was told to use the local Fluent file
   */
  _createDOML10n() {
    /* istanbul ignore next */
    let useRemoteL10n = Services.prefs.getBoolPref(USE_REMOTE_L10N_PREF, true);
    if (useRemoteL10n && !L10nRegistry.getInstance().hasSource("cfr")) {
      const appLocale = Services.locale.appLocaleAsBCP47;
      const l10nFluentDir = PathUtils.join(
        Services.dirsvc.get("ProfLD", Ci.nsIFile).path,
        "settings",
        "main",
        "ms-language-packs"
      );
      let cfrIndexedFileSource = new L10nFileSource(
        "cfr",
        "app",
        [appLocale],
        `file://${l10nFluentDir}/`,
        {
          addResourceOptions: {
            allowOverrides: true,
          },
        },
        [`file://${l10nFluentDir}/browser/newtab/asrouter.ftl`]
      );
      L10nRegistry.getInstance().registerSources([cfrIndexedFileSource]);
    } else if (!useRemoteL10n && L10nRegistry.getInstance().hasSource("cfr")) {
      L10nRegistry.getInstance().removeSources(["cfr"]);
    }

    return new DOMLocalization(
      [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ],
      false
    );
  }

  get l10n() {
    if (!this._l10n) {
      this._l10n = this._createDOML10n();
    }
    return this._l10n;
  }

  reloadL10n() {
    this._l10n = null;
  }

  isLocaleSupported(locale) {
    return locale === "en-US" || ALL_LOCALES.has(locale);
  }

  /**
   * Format given `localizableText`.
   *
   * Format `localizableText` if it is an object using any `string_id` field,
   * otherwise return `localizableText` unmodified.
   *
   * @param {object|string} `localizableText` to format.
   * @return {string} formatted text.
   */
  async formatLocalizableText(localizableText) {
    if (typeof localizableText !== "string") {
      // It's more useful to get an error than passing through an object without
      // a `string_id` field.
      let value = await this.l10n.formatValue(localizableText.string_id);
      return value;
    }
    return localizableText;
  }
}

const RemoteL10n = new _RemoteL10n();

const EXPORTED_SYMBOLS = ["RemoteL10n", "_RemoteL10n"];
