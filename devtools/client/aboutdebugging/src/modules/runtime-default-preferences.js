/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/**
 * This module provides a workaround for remote debugging when a preference is
 * defined in the firefox preference file (browser/app/profile/firefox.js) but
 * still read from the server, without any default value.
 *
 * This causes the server to crash and can't easily be recovered.
 *
 * While we work on better linting to prevent such issues (Bug 1660182), this
 * module will be able to set default values for all missing preferences.
 */

const PREFERENCE_TYPES = {
  BOOL: "BOOL",
  CHAR: "CHAR",
  INT: "INT",
};
exports.PREFERENCE_TYPES = PREFERENCE_TYPES;

/**
 * Expected properties for the preference descriptors:
 * - prefName {String}: the name of the preference.
 * - defaultValue {String|Bool|Number}: the value to set if the preference is
 *   missing.
 * - trait {String}: the name of the trait corresponding to this pref on the
 *   PreferenceFront.
 * - type {String}: the preference type (either BOOL, CHAR or INT).
 */
const DEFAULT_PREFERENCES = [];
exports.DEFAULT_PREFERENCES = DEFAULT_PREFERENCES;

const METHODS = {
  [PREFERENCE_TYPES.BOOL]: {
    setPref: "setBoolPref",
    getPref: "getBoolPref",
  },
  [PREFERENCE_TYPES.CHAR]: {
    setPref: "setCharPref",
    getPref: "getCharPref",
  },
  [PREFERENCE_TYPES.INT]: {
    setPref: "setIntPref",
    getPref: "getIntPref",
  },
};

/**
 * Set default values for all the provided preferences on the runtime
 * corresponding to the provided clientWrapper, if needed.
 *
 * Note: prefDescriptors will most likely be DEFAULT_PREFERENCES when
 * used in production code, but can be parameterized for tests.
 *
 * @param {ClientWrapper} clientWrapper
 * @param {Array} prefDescriptors
 *        Array of preference descriptors, see DEFAULT_PREFERENCES.
 */
async function setDefaultPreferencesIfNeeded(clientWrapper, prefDescriptors) {
  if (!prefDescriptors || prefDescriptors.length === 0) {
    return;
  }

  const preferenceFront = await clientWrapper.getFront("preference");
  const preferenceTraits = await preferenceFront.getTraits();

  // Note: using Promise.all here fails because the request/responses get mixed.
  for (const prefDescriptor of prefDescriptors) {
    // If the fix for this preference is already on this server, skip it.
    if (preferenceTraits[prefDescriptor.trait]) {
      continue;
    }

    await setDefaultPreference(preferenceFront, prefDescriptor);
  }
}
exports.setDefaultPreferencesIfNeeded = setDefaultPreferencesIfNeeded;

async function setDefaultPreference(preferenceFront, prefDescriptor) {
  const { prefName, type, defaultValue } = prefDescriptor;

  if (!Object.values(PREFERENCE_TYPES).includes(type)) {
    throw new Error(`Unsupported type for setDefaultPreference "${type}"`);
  }

  const prefMethods = METHODS[type];
  try {
    // Try to read the preference only to check if the call is successful.
    // If not, this means the preference is missing and should be initialized.
    await preferenceFront[prefMethods.getPref](prefName);
  } catch (e) {
    console.warn(
      `Preference "${prefName}"" is not set on the remote runtime. Setting default value.`
    );
    await preferenceFront[prefMethods.setPref](prefName, defaultValue);
  }
}
