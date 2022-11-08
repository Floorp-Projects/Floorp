/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const lazy = {};

ChromeUtils.defineESModuleGetters(lazy, {
  UrlbarPrefs: "resource:///modules/UrlbarPrefs.sys.mjs",
  UrlbarUtils: "resource:///modules/UrlbarUtils.sys.mjs",
});

/**
 * Base class for quick suggest features. It can be extended to implement a
 * feature that is part of the larger quick suggest feature and that should be
 * enabled only when quick suggest is enabled.
 *
 * You can extend this class as an alternative to implementing your feature
 * directly in `QuickSuggest`. Doing so has the following advantages:
 *
 * - If your feature is gated on a Nimbus variable or preference, `QuickSuggest`
 *   will manage its lifetime automatically. This is really only useful if the
 *   feature has state that must be initialized when the feature is enabled and
 *   uninitialized when it's disabled.
 *
 * - Encapsulation. You can keep all the code related to your feature in one
 *   place, without mixing it with unrelated code and cluttering up
 *   `QuickSuggest`. You can also test it in isolation from `QuickSuggest`.
 *
 * - Your feature will automatically get its own logger.
 *
 * If your feature can't benefit from these advantages, especially the first,
 * feel free to implement it directly in `QuickSuggest`.
 *
 * To register your subclass with `QuickSuggest`, add it to the `FEATURES` const
 * in QuickSuggest.sys.mjs.
 */
export class BaseFeature {
  /**
   * {boolean}
   *   Whether the feature should be enabled. Typically the subclass will check
   *   the values of one or more Nimbus variables or preferences. `QuickSuggest`
   *   will access this getter only when the quick suggest feature as a whole is
   *   enabled. Otherwise the subclass feature will be disabled automatically.
   */
  get shouldEnable() {
    throw new Error("`shouldEnable` must be overridden");
  }

  /**
   * @returns {Array}
   *   If the subclass's `shouldEnable` implementation depends on preferences
   *   instead of Nimbus variables, the subclass should override this getter and
   *   return their names in this array so that `enable()` can be called when
   *   they change. Names should be in the same format that `UrlbarPrefs.get()`
   *   expects.
   */
  get enablingPreferences() {
    return null;
  }

  /**
   * This method should initialize or uninitialize any state related to the
   * feature.
   *
   * @param {boolean} enabled
   *   Whether the feature should be enabled or not.
   */
  enable(enabled) {
    throw new Error("`enable()` must be overridden");
  }

  /**
   * @returns {Logger}
   *   The feature's logger.
   */
  get logger() {
    if (!this._logger) {
      this._logger = lazy.UrlbarUtils.getLogger({
        prefix: `QuickSuggest.${this.constructor.name}`,
      });
    }
    return this._logger;
  }

  /**
   * @returns {boolean}
   *   Whether the feature is enabled. The enabled status is automatically
   *   managed by `QuickSuggest` and subclasses should not override this.
   */
  get isEnabled() {
    return this.#isEnabled;
  }

  /**
   * Enables or disables the feature according to `shouldEnable` and whether
   * quick suggest is enabled. If the feature is already enabled appropriately,
   * does nothing.
   */
  update() {
    let enable =
      lazy.UrlbarPrefs.get("quickSuggestEnabled") && this.shouldEnable;
    if (enable != this.isEnabled) {
      this.logger.info(`Setting enabled = ${enable}`);
      this.enable(enable);
      this.#isEnabled = enable;
    }
  }

  #isEnabled = false;
}
