/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
});

const RESULT_MENU_COMMANDS = {
  DISMISS: "dismiss",
};

/**
 * A provider that returns a suggested url to the user based
 * on a valid URL stored in the clipboard.
 */
class ProviderClipboard extends UrlbarProvider {
  #lastDismissedClipboardText = "";
  constructor() {
    super();
  }

  get name() {
    return "UrlbarProviderClipboard";
  }

  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  isActive(queryContext, controller) {
    // Return clipboard results only for empty searches.
    if (
      !lazy.UrlbarPrefs.get("suggest.clipboard") ||
      queryContext.searchString
    ) {
      return false;
    }
    let textFromClipboard = controller.browserWindow.readFromClipboard();
    if (!textFromClipboard) {
      return false;
    }
    textFromClipboard =
      controller.input.sanitizeTextFromClipboard(textFromClipboard);
    const validUrl = this.#validUrl(textFromClipboard);
    if (!validUrl) {
      return false;
    }
    this.urlFromClipboard = validUrl;
    if (this.#lastDismissedClipboardText == this.urlFromClipboard) {
      return false;
    }
    this.#lastDismissedClipboardText = ""; // clear the cache since clipboard value has changed
    return true;
  }

  #validUrl(clipboardVal) {
    try {
      let givenUrl;
      givenUrl = new URL(clipboardVal);
      if (givenUrl.protocol == "http:" || givenUrl.protocol == "https:") {
        return givenUrl.href;
      }
    } catch (ex) {
      // Not a valid URI.
    }
    return null;
  }

  getPriority(queryContext) {
    // Zero-prefix suggestions have the same priority as top sites.
    return 1;
  }

  async startQuery(queryContext, addCallback) {
    // If the query was started, isActive should have cached a url already.
    let result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.URL,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        url: this.urlFromClipboard,
        title: this.urlFromClipboard,
        icon: "chrome://global/skin/icons/edit-copy.svg",
      }
    );

    if (lazy.UrlbarPrefs.get("resultMenu")) {
      result.isBlockable = true;
      result.blockL10n = {
        id: "urlbar-result-menu-dismiss-firefox-suggest",
      };
    }

    addCallback(this, result);
  }

  onEngagement(state, queryContext, details, controller) {
    if (details.result?.providerName != this.name) {
      return;
    }

    // Handle commands.
    this.#handlePossibleCommand(
      controller.view,
      details.result,
      details.selType
    );
  }

  #handlePossibleCommand(view, result, selType) {
    switch (selType) {
      case RESULT_MENU_COMMANDS.DISMISS:
        view.onQueryResultRemoved(result.rowIndex);
        this.#lastDismissedClipboardText = this.urlFromClipboard;
        break;
    }
  }

  getResultCommands(result) {
    let commands = [
      {
        name: RESULT_MENU_COMMANDS.DISMISS,
        l10n: {
          id: "urlbar-result-menu-dismiss-firefox-suggest",
        },
      },
    ];
    return commands;
  }
}

export var UrlbarProviderClipboard = new ProviderClipboard();
