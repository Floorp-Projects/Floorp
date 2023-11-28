/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { XPCOMUtils } = ChromeUtils.importESModule(
  "resource://gre/modules/XPCOMUtils.sys.mjs"
);

const lazy = {};

XPCOMUtils.defineLazyModuleGetters(lazy, {
  ASRouter: "resource://activity-stream/lib/ASRouter.jsm",
  ASRouterTargeting: "resource://activity-stream/lib/ASRouterTargeting.jsm",
});

const AWScreenUtils = {
  /**
   * Filter the given screens in place with a predicate.
   *
   * @param {object[]} screens - The screens to filter.
   * @param {Function} callback - The predicate for filtering the screens.
   */
  async removeScreens(screens, callback) {
    for (let i = 0; i < screens?.length; i++) {
      if (await callback(screens[i], i)) {
        screens.splice(i--, 1);
      }
    }
  },
  /**
   * Given a JEXL expression, returns the evaluation of the expression or returns
   * true if the expression did not evaluate successfully
   *
   * @param {string} targeting - The JEXL expression that will be evaluated
   * @returns {boolean}
   */
  async evaluateScreenTargeting(targeting) {
    const result = await lazy.ASRouter.evaluateExpression({
      expression: targeting,
      context: lazy.ASRouterTargeting.Environment,
    });
    if (result?.evaluationStatus?.success) {
      return result.evaluationStatus.result;
    }

    return true;
  },
  /**
   * Filter out screens whose targeting do not match.
   *
   * Given an array of screens, each screen will have it's `targeting` property
   * evaluated, and removed if it's targeting evaluates to false
   *
   * @param {object[]} screens - An array of screens that will be looped
   * through to be evaluated for removal
   * @returns {object[]} - A new array containing the screens that were not removed
   */
  async evaluateTargetingAndRemoveScreens(screens) {
    const filteredScreens = [...screens];
    await this.removeScreens(filteredScreens, async screen => {
      if (screen.targeting === undefined) {
        // Don't remove the screen if we don't have a targeting property
        return false;
      }

      const result = await this.evaluateScreenTargeting(screen.targeting);
      // Flipping the value because a true evaluation means we
      // don't want to remove the screen, while false means we do
      return !result;
    });

    return filteredScreens;
  },

  async addScreenImpression(screen) {
    await lazy.ASRouter.addScreenImpression(screen);
  },
};

const EXPORTED_SYMBOLS = ["AWScreenUtils"];
