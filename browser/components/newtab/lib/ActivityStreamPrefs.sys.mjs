/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// We use importESModule here instead of static import so that
// the Karma test environment won't choke on this module. This
// is because the Karma test environment already stubs out
// AppConstants, and overrides importESModule to be a no-op (which
// can't be done for a static import statement).

// eslint-disable-next-line mozilla/use-static-import
const { AppConstants } = ChromeUtils.importESModule(
  "resource://gre/modules/AppConstants.sys.mjs"
);
// eslint-disable-next-line mozilla/use-static-import
const { Preferences } = ChromeUtils.importESModule(
  "resource://gre/modules/Preferences.sys.mjs"
);

const ACTIVITY_STREAM_PREF_BRANCH = "browser.newtabpage.activity-stream.";

export class Prefs extends Preferences {
  /**
   * Prefs - A wrapper around Preferences that always sets the branch to
   *         ACTIVITY_STREAM_PREF_BRANCH
   */
  constructor(branch = ACTIVITY_STREAM_PREF_BRANCH) {
    super({ branch });
    this._branchObservers = new Map();
  }

  ignoreBranch(listener) {
    const observer = this._branchObservers.get(listener);
    this._prefBranch.removeObserver("", observer);
    this._branchObservers.delete(listener);
  }

  observeBranch(listener) {
    const observer = (subject, topic, pref) => {
      listener.onPrefChanged(pref, this.get(pref));
    };
    this._prefBranch.addObserver("", observer);
    this._branchObservers.set(listener, observer);
  }
}

export class DefaultPrefs extends Preferences {
  /**
   * DefaultPrefs - A helper for setting and resetting default prefs for the add-on
   *
   * @param  {Map} config A Map with {string} key of the pref name and {object}
   *                      value with the following pref properties:
   *         {string} .title (optional) A description of the pref
   *         {bool|string|number} .value The default value for the pref
   * @param  {string} branch (optional) The pref branch (defaults to ACTIVITY_STREAM_PREF_BRANCH)
   */
  constructor(config, branch = ACTIVITY_STREAM_PREF_BRANCH) {
    super({
      branch,
      defaultBranch: true,
    });
    this._config = config;
  }

  /**
   * init - Set default prefs for all prefs in the config
   */
  init() {
    // Local developer builds (with the default mozconfig) aren't OFFICIAL
    const IS_UNOFFICIAL_BUILD = !AppConstants.MOZILLA_OFFICIAL;

    for (const pref of this._config.keys()) {
      try {
        // Avoid replacing existing valid default pref values, e.g., those set
        // via Autoconfig or policy
        if (this.get(pref) !== undefined) {
          continue;
        }
      } catch (ex) {
        // We get NS_ERROR_UNEXPECTED for prefs that have a user value (causing
        // default branch to believe there's a type) but no actual default value
      }

      const prefConfig = this._config.get(pref);
      let value;
      if (IS_UNOFFICIAL_BUILD && "value_local_dev" in prefConfig) {
        value = prefConfig.value_local_dev;
      } else {
        value = prefConfig.value;
      }

      try {
        this.set(pref, value);
      } catch (ex) {
        // Potentially the user somehow set an unexpected value type, so we fail
        // to set a default of our expected type
      }
    }
  }
}
