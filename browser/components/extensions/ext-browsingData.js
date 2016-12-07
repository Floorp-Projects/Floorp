"use strict";

XPCOMUtils.defineLazyModuleGetter(this, "Preferences",
                                  "resource://gre/modules/Preferences.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Sanitizer",
                                  "resource:///modules/Sanitizer.jsm");

extensions.registerSchemaAPI("browsingData", "addon_parent", context => {
  return {
    browsingData: {
      settings() {
        const PREF_DOMAIN = "privacy.cpd.";
        // The following prefs are the only ones in Firefox that match corresponding
        // values used by Chrome when rerturning settings.
        const PREF_LIST = ["cache", "cookies", "history", "formdata", "downloads"];

        // since will be the start of what is returned by Sanitizer.getClearRange
        // divided by 1000 to convert to ms.
        let since = Sanitizer.getClearRange()[0] / 1000;
        let options = {since};

        let dataToRemove = {};
        let dataRemovalPermitted = {};

        for (let item of PREF_LIST) {
          dataToRemove[item] = Preferences.get(`${PREF_DOMAIN}${item}`);
          // Firefox doesn't have the same concept of dataRemovalPermitted
          // as Chrome, so it will always be true.
          dataRemovalPermitted[item] = true;
        }
        // formData has a different case than the pref formdata.
        dataToRemove.formData = Preferences.get(`${PREF_DOMAIN}formdata`);
        dataRemovalPermitted.formData = true;

        return Promise.resolve({options, dataToRemove, dataRemovalPermitted});
      },
    },
  };
});
