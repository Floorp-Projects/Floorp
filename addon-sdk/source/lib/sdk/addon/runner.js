/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, Cu } = require('chrome');
const { rootURI, metadata, isNative } = require('@loader/options');
const { id, loadReason } = require('../self');
const { descriptor, Sandbox, evaluate, main, resolveURI } = require('toolkit/loader');
const { once } = require('../system/events');
const { exit, env, staticArgs } = require('../system');
const { when: unload } = require('../system/unload');
const globals = require('../system/globals');
const xulApp = require('../system/xul-app');
const { get } = require('../preferences/service');
const appShellService = Cc['@mozilla.org/appshell/appShellService;1'].
                        getService(Ci.nsIAppShellService);
const { preferences } = metadata;

const Startup = Cu.import("resource://gre/modules/sdk/system/Startup.js", {}).exports;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");
XPCOMUtils.defineLazyGetter(this, "BrowserToolboxProcess", function () {
  return Cu.import("resource://devtools/client/framework/ToolboxProcess.jsm", {}).
         BrowserToolboxProcess;
});

// Initializes default preferences
function setDefaultPrefs(prefsURI) {
  const prefs = Cc['@mozilla.org/preferences-service;1'].
                getService(Ci.nsIPrefService).
                QueryInterface(Ci.nsIPrefBranch2);
  const branch = prefs.getDefaultBranch('');
  const sandbox = Sandbox({
    name: prefsURI,
    prototype: {
      pref: function(key, val) {
        switch (typeof val) {
          case 'boolean':
            branch.setBoolPref(key, val);
            break;
          case 'number':
            if (val % 1 == 0) // number must be a integer, otherwise ignore it
              branch.setIntPref(key, val);
            break;
          case 'string':
            branch.setCharPref(key, val);
            break;
        }
      }
    }
  });
  // load preferences.
  evaluate(sandbox, prefsURI);
}

function definePseudo(loader, id, exports) {
  let uri = resolveURI(id, loader.mapping);
  loader.modules[uri] = { exports: exports };
}

function startup(reason, options) {
  return Startup.onceInitialized.then(() => {
    // Inject globals ASAP in order to have console API working ASAP
    Object.defineProperties(options.loader.globals, descriptor(globals));

    // NOTE: Module is intentionally required only now because it relies
    // on existence of hidden window, which does not exists until startup.
    let { ready } = require('../addon/window');
    // Load localization manifest and .properties files.
    // Run the addon even in case of error (best effort approach)
    require('../l10n/loader').
      load(rootURI).
      then(null, function failure(error) {
        if (!isNative)
          console.info("Error while loading localization: " + error.message);
      }).
      then(function onLocalizationReady(data) {
        // Exports data to a pseudo module so that api-utils/l10n/core
        // can get access to it
        definePseudo(options.loader, '@l10n/data', data ? data : null);
        return ready;
      }).then(function() {
        run(options);
      }).then(null, console.exception);
    return void 0; // otherwise we raise a warning, see bug 910304
  });
}

function run(options) {
  try {
    // Try initializing HTML localization before running main module. Just print
    // an exception in case of error, instead of preventing addon to be run.
    try {
      // Do not enable HTML localization while running test as it is hard to
      // disable. Because unit tests are evaluated in a another Loader who
      // doesn't have access to this current loader.
      if (options.main !== 'sdk/test/runner') {
        require('../l10n/html').enable();
      }
    }
    catch(error) {
      console.exception(error);
    }

    // native-options does stuff directly with preferences key from package.json
    if (preferences && preferences.length > 0) {
      try {
        require('../preferences/native-options').
          enable({ preferences: preferences, id: id }).
          catch(console.exception);
      }
      catch (error) {
        console.exception(error);
      }
    }
    else {
      // keeping support for addons packaged with older SDK versions,
      // when cfx didn't include the 'preferences' key in @loader/options

      // Initialize inline options localization, without preventing addon to be
      // run in case of error
      try {
        require('../l10n/prefs').enable();
      }
      catch(error) {
        console.exception(error);
      }

      // TODO: When bug 564675 is implemented this will no longer be needed
      // Always set the default prefs, because they disappear on restart
      if (options.prefsURI) {
        // Only set if `prefsURI` specified
        try {
          setDefaultPrefs(options.prefsURI);
        }
        catch (err) {
          // cfx bootstrap always passes prefsURI, even in addons without prefs
        }
      }
    }

    // this is where the addon's main.js finally run.
    let program = main(options.loader, options.main);

    if (typeof(program.onUnload) === 'function')
      unload(program.onUnload);

    if (typeof(program.main) === 'function') {
      program.main({
        loadReason: loadReason,
        staticArgs: staticArgs
      }, {
        print: function print(_) { dump(_ + '\n') },
        quit: exit
      });
    }

    if (get("extensions." + id + ".sdk.debug.show", false)) {
      BrowserToolboxProcess.init({ addonID: id });
    }
  } catch (error) {
    console.exception(error);
    throw error;
  }
}
exports.startup = startup;

// If add-on is lunched via `cfx run` we need to use `system.exit` to let
// cfx know we're done (`cfx test` will take care of exit so we don't do
// anything here).
if (env.CFX_COMMAND === 'run') {
  unload(function(reason) {
    if (reason === 'shutdown')
      exit(0);
  });
}
