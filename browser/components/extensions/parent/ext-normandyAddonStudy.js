"use strict";

const { AddonStudies } = ChromeUtils.import(
  "resource://normandy/lib/AddonStudies.jsm"
);
const { ClientID } = ChromeUtils.import("resource://gre/modules/ClientID.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "AddonManager",
  "resource://gre/modules/AddonManager.jsm"
);

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
         * @returns {Object}
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
