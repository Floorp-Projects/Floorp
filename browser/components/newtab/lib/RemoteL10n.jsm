/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

/**
 * The downloaded Fluent file is located in this sub-directory of the local
 * profile directory.
 */
const RS_DOWNLOADED_FILE_SUBDIR = "settings/main/ms-language-packs";
const USE_REMOTE_L10N_PREF =
  "browser.newtabpage.activity-stream.asrouter.useRemoteL10n";

const { XPCOMUtils } = ChromeUtils.import(
  "resource://gre/modules/XPCOMUtils.jsm"
);

XPCOMUtils.defineLazyModuleGetters(this, {
  L10nRegistry: "resource://gre/modules/L10nRegistry.jsm",
  FileSource: "resource://gre/modules/L10nRegistry.jsm",
  OS: "resource://gre/modules/osfile.jsm",
  Services: "resource://gre/modules/Services.jsm",
});

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
    async function* generateBundles(resourceIds) {
      const appLocale = Services.locale.appLocaleAsBCP47;
      const appLocales = Services.locale.appLocalesAsBCP47;
      const l10nFluentDir = OS.Path.join(
        OS.Constants.Path.localProfileDir,
        RS_DOWNLOADED_FILE_SUBDIR
      );
      const fs = new FileSource("cfr", [appLocale], `file://${l10nFluentDir}/`);
      // In the case that the Fluent file has not been downloaded from Remote Settings,
      // `fetchFile` will return `false` and fall back to the packaged Fluent file.
      const resource = await fs.fetchFile(appLocale, "asrouter.ftl");
      for await (let bundle of L10nRegistry.generateBundles(
        appLocales.slice(0, 1),
        resourceIds
      )) {
        // Override built-in messages with the resource loaded from remote settings for
        // the app locale, i.e. the first item of `appLocales`.
        if (resource) {
          bundle.addResource(resource, { allowOverrides: true });
        }
        yield bundle;
      }
      // Now generating bundles for the rest of locales of `appLocales`.
      yield* L10nRegistry.generateBundles(appLocales.slice(1), resourceIds);
    }

    return new DOMLocalization(
      [
        "browser/newtab/asrouter.ftl",
        "browser/branding/brandings.ftl",
        "browser/branding/sync-brand.ftl",
        "branding/brand.ftl",
        "browser/defaultBrowserNotification.ftl",
      ],
      false,
      Services.prefs.getBoolPref(USE_REMOTE_L10N_PREF, true)
        ? { generateBundles }
        : {}
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
}

this.RemoteL10n = new _RemoteL10n();

const EXPORTED_SYMBOLS = ["RemoteL10n", "_RemoteL10n"];
