/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci } = require('chrome');
const { descriptor, Sandbox, evaluate, main, resolveURI } = require('toolkit/loader');
const { once } = require('../system/events');
const { exit, env, staticArgs } = require('../system');
const { when: unload } = require('../system/unload');
const { loadReason } = require('../self');
const { rootURI } = require("@loader/options");
const globals = require('../system/globals');
const xulApp = require('../system/xul-app');
const appShellService = Cc['@mozilla.org/appshell/appShellService;1'].
                        getService(Ci.nsIAppShellService);

const NAME2TOPIC = {
  'Firefox': 'sessionstore-windows-restored',
  'Fennec': 'sessionstore-windows-restored',
  'SeaMonkey': 'sessionstore-windows-restored',
  'Thunderbird': 'mail-startup-done'
};

// Set 'final-ui-startup' as default topic for unknown applications
let appStartup = 'final-ui-startup';

// Gets the topic that fit best as application startup event, in according with
// the current application (e.g. Firefox, Fennec, Thunderbird...)
for (let name of Object.keys(NAME2TOPIC)) {
  if (xulApp.is(name)) {
    appStartup = NAME2TOPIC[name];
    break;
  }
}

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

function wait(reason, options) {
  once(appStartup, function() {
    startup(null, options);
  });
}

function startup(reason, options) {
  // Try accessing hidden window to guess if we are running during firefox
  // startup, so that we should wait for session restore event before
  // running the addon
  let initialized = false;
  try {
    appShellService.hiddenDOMWindow;
    initialized = true;
  }
  catch(e) {}
  if (reason === 'startup' || !initialized) {
    return wait(reason, options);
  }

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
}

function run(options) {
  try {
    // Try initializing HTML localization before running main module. Just print
    // an exception in case of error, instead of preventing addon to be run.
    try {
      // Do not enable HTML localization while running test as it is hard to
      // disable. Because unit tests are evaluated in a another Loader who
      // doesn't have access to this current loader.
      if (options.main !== 'test-harness/run-tests')
        require('../l10n/html').enable();
    }
    catch(error) {
      console.exception(error);
    }
    // Initialize inline options localization, without preventing addon to be
    // run in case of error
    try {
      require('../l10n/prefs');
    }
    catch(error) {
      console.exception(error);
    }

    // TODO: When bug 564675 is implemented this will no longer be needed
    // Always set the default prefs, because they disappear on restart
    setDefaultPrefs(options.prefsURI);

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
