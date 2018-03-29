"use strict";

XPCOMUtils.defineLazyModuleGetters(this, {
  ctypes: "resource://gre/modules/ctypes.jsm",
  NativeManifests: "resource://gre/modules/NativeManifests.jsm",
  OS: "resource://gre/modules/osfile.jsm",
});

XPCOMUtils.defineLazyServiceGetter(this, "pkcs11db",
                                   "@mozilla.org/security/pkcs11moduledb;1",
                                   "nsIPKCS11ModuleDB");

var {DefaultMap} = ExtensionUtils;

const findModuleByPath = function(path) {
  let modules = pkcs11db.listModules();
  for (let module of XPCOMUtils.IterSimpleEnumerator(modules, Ci.nsIPKCS11Module)) {
    if (module && module.libName === path) {
      return module;
    }
  }
  return null;
};

this.pkcs11 = class extends ExtensionAPI {
  getAPI(context) {
    let manifestCache = new DefaultMap(async name => {
      let hostInfo = await NativeManifests.lookupManifest("pkcs11", name, context);
      if (hostInfo) {
        if (AppConstants.platform === "win") {
          hostInfo.manifest.path = OS.Path.join(OS.Path.dirname(hostInfo.path), hostInfo.manifest.path);
        }
        let manifestLib = OS.Path.basename(hostInfo.manifest.path);
        if (AppConstants.platform !== "linux") {
          manifestLib = manifestLib.toLowerCase(manifestLib);
        }
        if (manifestLib !== ctypes.libraryName("nssckbi")) {
          return hostInfo.manifest;
        }
      }
      return Promise.reject({message: `No such PKCS#11 module ${name}`});
    });
    return {
      pkcs11: {
        /**
          * Verify whether a given PKCS#11 module is installed.
          *
          * @param {string} name The name of the module, as specified in
          *                      the manifest file.
          * @returns {Promise} A Promise that resolves to true if the package
          *                    is installed, or false if it is not. May be
          *                    rejected if the module could not be found.
          */
        async isModuleInstalled(name) {
          let manifest = await manifestCache.get(name);
          return findModuleByPath(manifest.path) !== null;
        },
        /**
          * Install a PKCS#11 module
          *
          * @param {string} name The name of the module, as specified in
          *                      the manifest file.
          * @param {integer} [flags = 0] Any flags to be passed on to the
          *                              nsIPKCS11ModuleDB.addModule method
          * @returns {Promise} When the Promise resolves, the module will have
          *                    been installed. When it is rejected, the module
          *                    either is already installed or could not be
          *                    installed for some reason.
          */
        async installModule(name, flags = 0) {
          let manifest = await manifestCache.get(name);
          if (!manifest.description) {
            return Promise.reject({message: `The description field in the manifest for PKCS#11 module ${name} must have a value`});
          }
          pkcs11db.addModule(manifest.description, manifest.path, flags, 0);
        },
        /**
          * Uninstall a PKCS#11 module
          *
          * @param {string} name The name of the module, as specified in
          *                      the manifest file.
          * @returns {Promise}. When the Promise resolves, the module will have
          *                     been uninstalled. When it is rejected, the
          *                     module either was not installed or could not be
          *                     uninstalled for some reason.
          */
        async uninstallModule(name) {
          let manifest = await manifestCache.get(name);
          let module = findModuleByPath(manifest.path);
          if (!module) {
            return Promise.reject({message: `The PKCS#11 module ${name} is not loaded`});
          }
          pkcs11db.deleteModule(module.name);
        },
        /**
         * Get a list of slots for a given PKCS#11 module, with
         * information on the token (if any) in the slot.
         *
         * The PKCS#11 standard defines slots as an abstract concept
         * that may or may not have at most one token. In practice, when
         * using PKCS#11 for smartcards (the most likely use case of
         * PKCS#11 for Firefox), a slot corresponds to a cardreader, and
         * a token corresponds to a card.
         *
         * @param {string} name The name of the PKCS#11 module, as
         *                 specified in the manifest file.
         * @returns {Promise} A promise that resolves to an array of objects
         *                    with two properties.  The `name` object contains
         *                    the name of the slot; the `token` object is null
         *                    if there is no token in the slot, or is an object
         *                    describing various properties of the token if
         *                    there is.
         */
        async getModuleSlots(name) {
          let manifest = await manifestCache.get(name);
          let module = findModuleByPath(manifest.path);
          if (!module) {
            return Promise.reject({message: `The module ${name} is not installed`});
          }
          let rv = [];
          for (let slot of XPCOMUtils.IterSimpleEnumerator(module.listSlots(), Ci.nsIPKCS11Slot)) {
            let token = slot.getToken();
            let slotobj = {
              name: slot.name,
              token: null,
            };
            if (slot.status != 1 /* SLOT_NOT_PRESENT */) {
              slotobj.token = {
                name: token.tokenName,
                manufacturer: token.tokenManID,
                HWVersion: token.tokenHWVersion,
                FWVersion: token.tokenFWVersion,
                serial: token.tokenSerialNumber,
                isLoggedIn: token.isLoggedIn(),
              };
            }
            rv.push(slotobj);
          }
          return rv;
        },
      },
    };
  }
};
