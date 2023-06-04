/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { AddonStudies } = ChromeUtils.importESModule(
  "resource://normandy/lib/AddonStudies.sys.mjs"
);
const { ClientID } = ChromeUtils.importESModule(
  "resource://gre/modules/ClientID.sys.mjs"
);

ChromeUtils.defineESModuleGetters(this, {
  AddonManager: "resource://gre/modules/AddonManager.sys.mjs",
});

this.normandyAddonStudy = class extends ExtensionAPI {
  getAPI(context) {
    let { extension } = context;

    return {
      normandyAddonStudy: {
        /**
         * Returns a study object for the current study.
         *
         * @returns {Study}
         */
        async getStudy() {
          const studies = await AddonStudies.getAll();
          return studies.find(study => study.addonId === extension.id);
        },

        /**
         * Marks the study as ended and then uninstalls the addon.
         *
         * @param {string} reason Why the study is ending
         */
        async endStudy(reason) {
          const study = await this.getStudy();

          // Mark the study as ended
          await AddonStudies.markAsEnded(study, reason);

          // Uninstall the addon
          const addon = await AddonManager.getAddonByID(study.addonId);
          if (addon) {
            await addon.uninstall();
          }
        },

        /**
         * Returns an object with metadata about the client which may
         * be required for constructing survey URLs.
         *
         * @returns {object}
         */
        async getClientMetadata() {
          return {
            updateChannel: Services.appinfo.defaultUpdateChannel,
            fxVersion: Services.appinfo.version,
            clientID: await ClientID.getClientID(),
          };
        },

        onUnenroll: new EventManager({
          context,
          name: "normandyAddonStudy.onUnenroll",
          register: fire => {
            const listener = async reason => {
              await fire.async(reason);
            };

            AddonStudies.addUnenrollListener(extension.id, listener);

            return () => {
              AddonStudies.removeUnenrollListener(extension.id, listener);
            };
          },
        }).api(),
      },
    };
  }
};
