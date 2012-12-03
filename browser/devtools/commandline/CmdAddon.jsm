/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

this.EXPORTED_SYMBOLS = [ "Flags" ];

Cu.import("resource:///modules/devtools/gcli.jsm");
Cu.import("resource://gre/modules/XPCOMUtils.jsm");
Cu.import("resource://gre/modules/Services.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "AddonManager",
                                  "resource://gre/modules/AddonManager.jsm");

// We need to use an object in which to store any flags because a primitive
// would remain undefined.
this.Flags = {
  addonsLoaded: false
};

/**
 * 'addon' command.
 */
gcli.addCommand({
  name: "addon",
  description: gcli.lookup("addonDesc")
});

/**
 * 'addon list' command.
 */
gcli.addCommand({
  name: "addon list",
  description: gcli.lookup("addonListDesc"),
  params: [{
    name: 'type',
    type: {
      name: 'selection',
      data: ["dictionary", "extension", "locale", "plugin", "theme", "all"]
    },
    defaultValue: 'all',
    description: gcli.lookup("addonListTypeDesc"),
  }],
  exec: function(aArgs, context) {
    function representEnabledAddon(aAddon) {
      return "<li><![CDATA[" + aAddon.name + "\u2002" + aAddon.version +
      getAddonStatus(aAddon) + "]]></li>";
    }

    function representDisabledAddon(aAddon) {
      return "<li class=\"gcli-addon-disabled\">" +
        "<![CDATA[" + aAddon.name + "\u2002" + aAddon.version + aAddon.version +
        "]]></li>";
    }

    function getAddonStatus(aAddon) {
      let operations = [];

      if (aAddon.pendingOperations & AddonManager.PENDING_ENABLE) {
        operations.push("PENDING_ENABLE");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_DISABLE) {
        operations.push("PENDING_DISABLE");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_UNINSTALL) {
        operations.push("PENDING_UNINSTALL");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_INSTALL) {
        operations.push("PENDING_INSTALL");
      }

      if (aAddon.pendingOperations & AddonManager.PENDING_UPGRADE) {
        operations.push("PENDING_UPGRADE");
      }

      if (operations.length) {
        return " (" + operations.join(", ") + ")";
      }
      return "";
    }

    /**
     * Compares two addons by their name. Used in sorting.
     */
    function compareAddonNames(aNameA, aNameB) {
      return String.localeCompare(aNameA.name, aNameB.name);
    }

    /**
     * Resolves the promise which is the scope (this) of this function, filling
     * it with an HTML representation of the passed add-ons.
     */
    function list(aType, aAddons) {
      if (!aAddons.length) {
        this.resolve(gcli.lookup("addonNoneOfType"));
      }

      // Separate the enabled add-ons from the disabled ones.
      let enabledAddons = [];
      let disabledAddons = [];

      aAddons.forEach(function(aAddon) {
        if (aAddon.isActive) {
          enabledAddons.push(aAddon);
        } else {
          disabledAddons.push(aAddon);
        }
      });

      let header;
      switch(aType) {
        case "dictionary":
          header = gcli.lookup("addonListDictionaryHeading");
          break;
        case "extension":
          header = gcli.lookup("addonListExtensionHeading");
          break;
        case "locale":
          header = gcli.lookup("addonListLocaleHeading");
          break;
        case "plugin":
          header = gcli.lookup("addonListPluginHeading");
          break;
        case "theme":
          header = gcli.lookup("addonListThemeHeading");
        case "all":
          header = gcli.lookup("addonListAllHeading");
          break;
        default:
          header = gcli.lookup("addonListUnknownHeading");
      }

      // Map and sort the add-ons, and create an HTML list.
      let message = header +
                    "<ol>" +
                    enabledAddons.sort(compareAddonNames).map(representEnabledAddon).join("") +
                    disabledAddons.sort(compareAddonNames).map(representDisabledAddon).join("") +
                    "</ol>";

      this.resolve(context.createView({ html: message }));
    }

    // Create the promise that will be resolved when the add-on listing has
    // been finished.
    let promise = context.createPromise();
    let types = aArgs.type == "all" ? null : [aArgs.type];
    AddonManager.getAddonsByTypes(types, list.bind(promise, aArgs.type));
    return promise;
  }
});

// We need a list of addon names for the enable and disable commands. Because
// getting the name list is async we do not add the commands until we have the
// list.
AddonManager.getAllAddons(function addonAsync(aAddons) {
  // We listen for installs to keep our addon list up to date. There is no need
  // to listen for uninstalls because uninstalled addons are simply disabled
  // until restart (to enable undo functionality).
  AddonManager.addAddonListener({
    onInstalled: function(aAddon) {
      addonNameCache.push({
        name: representAddon(aAddon).replace(/\s/g, "_"),
        value: aAddon.name
      });
    },
    onUninstalled: function(aAddon) {
      let name = representAddon(aAddon).replace(/\s/g, "_");

      for (let i = 0; i < addonNameCache.length; i++) {
        if(addonNameCache[i].name == name) {
          addonNameCache.splice(i, 1);
          break;
        }
      }
    },
  });

  /**
   * Returns a string that represents the passed add-on.
   */
  function representAddon(aAddon) {
    let name = aAddon.name + " " + aAddon.version;
    return name.trim();
  }

  let addonNameCache = [];

  // The name parameter, used in "addon enable" and "addon disable."
  let nameParameter = {
    name: "name",
    type: {
      name: "selection",
      lookup: addonNameCache
    },
    description: gcli.lookup("addonNameDesc")
  };

  for (let addon of aAddons) {
    addonNameCache.push({
      name: representAddon(addon).replace(/\s/g, "_"),
      value: addon.name
    });
  }

  /**
   * 'addon enable' command.
   */
  gcli.addCommand({
    name: "addon enable",
    description: gcli.lookup("addonEnableDesc"),
    params: [nameParameter],
    exec: function(aArgs, context) {
      /**
       * Enables the addon in the passed list which has a name that matches
       * according to the passed name comparer, and resolves the promise which
       * is the scope (this) of this function to display the result of this
       * enable attempt.
       */
      function enable(aName, addons) {
        // Find the add-on.
        let addon = null;
        addons.some(function(candidate) {
          if (candidate.name == aName) {
            addon = candidate;
            return true;
          } else {
            return false;
          }
        });

        let name = representAddon(addon);
        let message = "";

        if (!addon.userDisabled) {
          message = gcli.lookupFormat("addonAlreadyEnabled", [name]);
        } else {
          addon.userDisabled = false;
          message = gcli.lookupFormat("addonEnabled", [name]);
        }
        this.resolve(message);
      }

      let promise = context.createPromise();
      // List the installed add-ons, enable one when done listing.
      AddonManager.getAllAddons(enable.bind(promise, aArgs.name));
      return promise;
    }
  });

  /**
   * 'addon disable' command.
   */
  gcli.addCommand({
    name: "addon disable",
    description: gcli.lookup("addonDisableDesc"),
    params: [nameParameter],
    exec: function(aArgs, context) {
      /**
       * Like enable, but ... you know ... the exact opposite.
       */
      function disable(aName, addons) {
        // Find the add-on.
        let addon = null;
        addons.some(function(candidate) {
          if (candidate.name == aName) {
            addon = candidate;
            return true;
          } else {
            return false;
          }
        });

        let name = representAddon(addon);
        let message = "";

        if (addon.userDisabled) {
          message = gcli.lookupFormat("addonAlreadyDisabled", [name]);
        } else {
          addon.userDisabled = true;
          message = gcli.lookupFormat("addonDisabled", [name]);
        }
        this.resolve(message);
      }

      let promise = context.createPromise();
      // List the installed add-ons, disable one when done listing.
      AddonManager.getAllAddons(disable.bind(promise, aArgs.name));
      return promise;
    }
  });
  Flags.addonsLoaded = true;
  Services.obs.notifyObservers(null, "gcli_addon_commands_ready", null);
});
