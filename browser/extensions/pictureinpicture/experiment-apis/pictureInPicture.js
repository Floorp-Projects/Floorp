/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

/* global AppConstants, ChromeUtils, ExtensionAPI, Services */

ChromeUtils.defineESModuleGetters(this, {
  KEYBOARD_CONTROLS: "resource://gre/modules/PictureInPictureControls.sys.mjs",
  TOGGLE_POLICIES: "resource://gre/modules/PictureInPictureControls.sys.mjs",
});

const TOGGLE_ENABLED_PREF =
  "media.videocontrols.picture-in-picture.video-toggle.enabled";

/**
 * This API is expected to be running in the parent process.
 */
this.pictureInPictureParent = class extends ExtensionAPI {
  /**
   * Override ExtensionAPI with PiP override's specific API
   * Relays the site overrides to this extension's child process
   *
   * @param {ExtensionContext} context the context of our extension
   * @returns {object} returns the necessary API structure required to manage sharedData in PictureInPictureParent
   */
  getAPI(context) {
    return {
      pictureInPictureParent: {
        setOverrides(overrides) {
          // The Picture-in-Picture toggle is only implemented for Desktop, so make
          // this a no-op for non-Desktop builds.
          if (AppConstants.platform == "android") {
            return;
          }

          Services.ppmm.sharedData.set(
            "PictureInPicture:SiteOverrides",
            overrides
          );
        },
      },
    };
  }
};

/**
 * This API is expected to be running in a content process - specifically,
 * the WebExtension content process that the background scripts run in. We
 * split these out so that they can return values synchronously to the
 * background scripts.
 */
this.pictureInPictureChild = class extends ExtensionAPI {
  /**
   * Override ExtensionAPI with PiP override's specific API
   * Clone constants into the Picture-in-Picture child process
   *
   * @param {ExtensionContext} context the context of our extension
   * @returns {object} returns the necessary API structure required to get data from PictureInPictureChild
   */
  getAPI(context) {
    return {
      pictureInPictureChild: {
        getKeyboardControls() {
          // The Picture-in-Picture toggle is only implemented for Desktop, so make
          // this return nothing for non-Desktop builds.
          if (AppConstants.platform == "android") {
            return Cu.cloneInto({}, context.cloneScope);
          }

          return Cu.cloneInto(KEYBOARD_CONTROLS, context.cloneScope);
        },
        getPolicies() {
          // The Picture-in-Picture toggle is only implemented for Desktop, so make
          // this return nothing for non-Desktop builds.
          if (AppConstants.platform == "android") {
            return Cu.cloneInto({}, context.cloneScope);
          }

          return Cu.cloneInto(TOGGLE_POLICIES, context.cloneScope);
        },
      },
    };
  }
};
