/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * Provide unit converter.
 */

import { XPCOMUtils } from "resource://gre/modules/XPCOMUtils.sys.mjs";

import { UnitConverterSimple } from "resource:///modules/UnitConverterSimple.sys.mjs";
import { UnitConverterTemperature } from "resource:///modules/UnitConverterTemperature.sys.mjs";
import { UnitConverterTimezone } from "resource:///modules/UnitConverterTimezone.sys.mjs";
import {
  UrlbarProvider,
  UrlbarUtils,
} from "resource:///modules/UrlbarUtils.sys.mjs";

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarResult: "resource:///modules/UrlbarResult.sys.mjs",
  UrlbarView: "resource:///modules/UrlbarView.sys.mjs",
});

XPCOMUtils.defineLazyServiceGetter(
  lazy,
  "ClipboardHelper",
  "@mozilla.org/widget/clipboardhelper;1",
  "nsIClipboardHelper"
);

const CONVERTERS = [
  new UnitConverterSimple(),
  new UnitConverterTemperature(),
  new UnitConverterTimezone(),
];

const DYNAMIC_RESULT_TYPE = "unitConversion";
const VIEW_TEMPLATE = {
  attributes: {
    selectable: true,
  },
  children: [
    {
      name: "content",
      tag: "span",
      classList: ["urlbarView-no-wrap"],
      children: [
        {
          name: "icon",
          tag: "img",
          classList: ["urlbarView-favicon"],
          attributes: {
            src: "chrome://global/skin/icons/edit-copy.svg",
          },
        },
        {
          name: "output",
          tag: "strong",
        },
        {
          name: "action",
          tag: "span",
        },
      ],
    },
  ],
};

/**
 * Provide a feature that converts given units.
 */
class ProviderUnitConversion extends UrlbarProvider {
  constructor() {
    super();
    lazy.UrlbarResult.addDynamicResultType(DYNAMIC_RESULT_TYPE);
    lazy.UrlbarView.addDynamicViewTemplate(DYNAMIC_RESULT_TYPE, VIEW_TEMPLATE);
  }

  /**
   * Returns the name of this provider.
   * @returns {string} the name of this provider.
   */
  get name() {
    return "UnitConversion";
  }

  /**
   * Returns the type of this provider.
   * @returns {integer} one of the types from UrlbarUtils.PROVIDER_TYPE.*
   */
  get type() {
    return UrlbarUtils.PROVIDER_TYPE.PROFILE;
  }

  /**
   * Whether the provider should be invoked for the given context.  If this
   * method returns false, the providers manager won't start a query with this
   * provider, to save on resources.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @returns {boolean}
   *   Whether this provider should be invoked for the search.
   */
  isActive({ searchString }) {
    if (!lazy.UrlbarPrefs.get("unitConversion.enabled")) {
      return false;
    }

    for (const converter of CONVERTERS) {
      const result = converter.convert(searchString);
      if (result) {
        this._activeResult = result;
        return true;
      }
    }

    this._activeResult = null;
    return false;
  }

  /**
   * This is called only for dynamic result types, when the urlbar view updates
   * the view of one of the results of the provider.  It should return an object
   * describing the view update.
   *
   * @param {UrlbarResult} result The result whose view will be updated.
   * @param {Map} idsByName
   *   A Map from an element's name, as defined by the provider; to its ID in
   *   the DOM, as defined by the browser.
   * @returns {object} An object describing the view update.
   */
  getViewUpdate(result) {
    return {
      output: {
        textContent: result.payload.output,
      },
      action: {
        l10n: { id: "urlbar-result-action-copy-to-clipboard" },
      },
    };
  }

  /**
   * This method is called by the providers manager when a query starts to fetch
   * each extension provider's results.  It fires the resultsRequested event.
   *
   * @param {UrlbarQueryContext} queryContext
   *   The query context object.
   * @param {function} addCallback
   *   The callback invoked by this method to add each result.
   */
  startQuery(queryContext, addCallback) {
    const result = new lazy.UrlbarResult(
      UrlbarUtils.RESULT_TYPE.DYNAMIC,
      UrlbarUtils.RESULT_SOURCE.OTHER_LOCAL,
      {
        dynamicType: DYNAMIC_RESULT_TYPE,
        output: this._activeResult,
        input: queryContext.searchString,
      }
    );
    result.suggestedIndex = lazy.UrlbarPrefs.get(
      "unitConversion.suggestedIndex"
    );

    addCallback(this, result);
  }

  pickResult(result, element) {
    const { textContent } = element.querySelector(
      ".urlbarView-dynamic-unitConversion-output"
    );
    lazy.ClipboardHelper.copyString(textContent);
  }
}

export const UrlbarProviderUnitConversion = new ProviderUnitConversion();
