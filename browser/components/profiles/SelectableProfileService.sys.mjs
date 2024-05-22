/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// TDOD: Remove this line once methods are updated. See bug 1896727
/* eslint-disable no-unused-vars */

/**
 * The service that manages selectable profiles
 */
export class SelectableProfileService {
  /**
   * At startup, store the nsToolkitProfile for the group.
   * Get the groupDBPath from the nsToolkitProfile, and connect to it.
   *
   * @param {nsToolkitProfile} groupProfile The current toolkit profile
   */
  init(groupProfile) {}

  /**
   * Create the SQLite DB for the profile group.
   * Init shared prefs for the group and add to DB.
   * Create the Group DB path to aNamedProfile entry in profiles.ini.
   * Import aNamedProfile into DB.
   *
   * @param {nsToolkitProfile} groupProfile The current toolkit profile
   */
  createProfileGroup(groupProfile) {}

  /**
   * When the last selectable profile in a group is deleted,
   * also remove the profile group's named profile entry from profiles.ini
   * and delete the group DB.
   */
  deleteProfileGroup() {}

  // App session lifecycle methods and multi-process support

  /**
   * When the group DB has been updated, either changes to prefs or profiles,
   * ask the remoting service to notify other running instances that they should
   * check for updates and refresh their UI accordingly.
   */
  notify() {}

  /**
   * Invoked when the remoting service has notified this instance that another
   * instance has updated the database. Triggers refreshProfiles() and refreshPrefs().
   */
  observe() {}

  /**
   * Init or update the current SelectableProfiles from the DB.
   */
  refreshProfiles() {}

  /**
   * Fetch all prefs from the DB and write to the current instance.
   */
  refreshPrefs() {}

  /**
   * Update the current default profile by setting its path as the Path
   * of the nsToolkitProfile for the group.
   *
   * @param {SelectableProfile} aSelectableProfile The new default SelectableProfile
   */
  setDefault(aSelectableProfile) {}

  /**
   * Update whether to show the selectable profile selector window at startup.
   * Set on the nsToolkitProfile instance for the group.
   *
   * @param {boolean} shouldShow Whether or not we should show the profile selector
   */
  setShowProfileChooser(shouldShow) {}

  /**
   * Update the path to the group DB. Set on the nsToolkitProfile instance
   * for the group.
   *
   * @param {string} aPath The path to the group DB
   */
  setGroupDBPath(aPath) {}

  // SelectableProfile lifecycle

  /**
   * Create an empty SelectableProfile and add it to the group DB.
   * This is an unmanaged profile from the nsToolkitProfile perspective.
   */
  createProfile() {}

  /**
   * Delete a SelectableProfile from the group DB.
   * If it was the last profile in the group, also call deleteProfileGroup().
   */
  deleteProfile() {}

  /**
   * Schedule deletion of the current SelectableProfile as a background task, then exit.
   */
  deleteCurrentProfile() {}

  /**
   * Write an updated profile to the DB.
   *
   * @param {SelectableProfile} aSelectableProfile The SelectableProfile to be update
   */
  updateProfile(aSelectableProfile) {}

  /**
   * Get the complete list of profiles in the group.
   */
  getProfiles() {}

  /**
   * Get a specific profile by its internal ID.
   *
   * @param {number} aProfileID The internal id of the profile
   */
  getProfile(aProfileID) {}

  // Shared Prefs management

  /**
   * Get all shared prefs as a list.
   */
  getAllPrefs() {}

  /**
   * Get the value of a specific shared pref.
   *
   * @param {string} aPrefName The name of the pref to get
   */
  getPref(aPrefName) {}

  /**
   * Insert or update a pref value, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref
   * @param {string} aPrefType The type of the pref
   * @param {any} aPrefValue The value of the pref
   */
  createOrUpdatePref(aPrefName, aPrefType, aPrefValue) {}

  /**
   * Remove a shared pref, then notify() other running instances.
   *
   * @param {string} aPrefName The name of the pref to delete
   */
  deletePref(aPrefName) {}

  // DB lifecycle

  /**
   * Create the SQLite DB for the profile group at groupDBPath.
   * Init shared prefs for the group and add to DB.
   */
  createGroupDB() {}

  /**
   * Delete the SQLite DB when the last selectable profile is deleted.
   */
  deleteGroupDB() {}
}
