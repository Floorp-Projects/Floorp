/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*-
 * vim: sw=2 ts=2 sts=2 et filetype=javascript
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/**
 * Deprecated utilities for JavaScript components loaded by the JS component
 * loader.
 */

var EXPORTED_SYMBOLS = ["ComponentUtils"];

const nsIFactoryQI = ChromeUtils.generateQI(["nsIFactory"]);

var ComponentUtils = {
  /**
   * Generates a singleton nsIFactory implementation that can be used as
   * an argument to nsIComponentRegistrar.registerFactory.
   * @param aServiceConstructor
   *        Constructor function of the component.
   */
  generateSingletonFactory(aServiceConstructor) {
    return {
      _instance: null,
      createInstance(aIID) {
        if (this._instance === null) {
          this._instance = new aServiceConstructor();
        }
        return this._instance.QueryInterface(aIID);
      },
      QueryInterface: nsIFactoryQI,
    };
  },
};
