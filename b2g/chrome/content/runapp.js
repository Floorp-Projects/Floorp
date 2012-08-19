// runapp.js:
// Provide a --runapp APPNAME command-line option.

window.addEventListener('load', function() {
  // Get the command line arguments that were passed to the b2g client
  let args = window.arguments[0].QueryInterface(Ci.nsICommandLine);
  let appname;

  // - Check if the argument is present before doing any work.
  try {
    // Returns null if the argument was not specified.  Throws
    // NS_ERROR_INVALID_ARG if there is no parameter specified (because
    // it was the last argument or the next argument starts with '-').
    // However, someone could still explicitly pass an empty argument!
    appname = args.handleFlagWithParam('runapp', false);
  }
  catch(e) {
    // treat a missing parameter like an empty parameter (=> show usage)
    appname = '';
  }

  // not specified, bail.
  if (appname === null)
    return;

  // - Get the list of apps since the parameter was specified
  let appsReq = navigator.mozApps.mgmt.getAll();
  appsReq.onsuccess = function() {
    let apps = appsReq.result;
    function findAppWithName(name) {
      let normalizedSearchName = name.replace(/[- ]+/g, '').toLowerCase();

      for (let i = 0; i < apps.length; i++) {
        let app = apps[i];
        let normalizedAppName =
              app.manifest.name.replace(/[- ]+/g, '').toLowerCase();
        if (normalizedSearchName === normalizedAppName) {
          return app;
        }
      }
      return null;
    }

    function usageAndDie(justApps) {
      if (!justApps)
        dump(
          'The --runapp argument specifies an app to automatically run at\n'+
          'startup.  We match against app names per their manifest and \n' +
          'ignoring capitalization, dashes, and whitespace.\n' +
          '\nThe system will load as usual except the lock screen will be ' +
          'automatically be disabled.\n\n' +
          'Known apps:\n');

      for (let i = 0; i < apps.length; i++) {
        dump('  ' + apps[i].manifest.name + '\n');
      }

      // Exit the b2g client
      Services.startup.quit(Ci.nsIAppStartup.eAttemptQuit);
    }

    if (appname === '') {
      usageAndDie();
      return;
    }

    let app = findAppWithName(appname);
    if (!app) {
      dump('Could not find app: "' + appname + '". Maybe you meant one of:\n');
      usageAndDie(true);
      return;
    }

    let setReq =
      navigator.mozSettings.getLock().set({'lockscreen.enabled': false});
    setReq.onsuccess = function() {
      // give the event loop another turn to disable the lock screen
      window.setTimeout(function() {
        dump('--runapp launching app: ' + app.manifest.name + '\n');
        app.launch();
      }, 0);
    };
    setReq.onerror = function() {
      dump('--runapp failed to disable lock-screen.  Giving up.\n');
    };

    dump('--runapp found app: ' + app.manifest.name +
         ', disabling lock screen...\n');
 };
 appsReq.onerror = function() {
   dump('Problem getting the list of all apps!');
 };
});
