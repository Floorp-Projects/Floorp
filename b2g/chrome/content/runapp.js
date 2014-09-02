"use strict";

// runapp.js:
// Provide a --runapp APPNAME command-line option.

let runAppObj;
window.addEventListener('load', function() {
  // Get the command line arguments that were passed to the b2g client
  let args;
  try {
    let service = Cc["@mozilla.org/commandlinehandler/general-startup;1?type=b2gcmds"].getService(Ci.nsISupports);
    args = service.wrappedJSObject.cmdLine;
  } catch(e) {}

  if (!args) {
    return;
  }

  let appname;

  // - Check if the argument is present before doing any work.
  try {
    // Returns null if the argument was not specified.  Throws
    // NS_ERROR_INVALID_ARG if there is no parameter specified (because
    // it was the last argument or the next argument starts with '-').
    // However, someone could still explicitly pass an empty argument!
    appname = args.handleFlagWithParam('runapp', false);
  } catch(e) {
    // treat a missing parameter like an empty parameter (=> show usage)
    appname = '';
  }

  // not specified, bail.
  if (appname === null) {
    return;
  }

  runAppObj = new AppRunner(appname);
  Services.obs.addObserver(runAppObj, 'remote-browser-shown', false);
  Services.obs.addObserver(runAppObj, 'inprocess-browser-shown', false);
});

window.addEventListener('unload', function() {
  if (runAppObj) {
    Services.obs.removeObserver(runAppObj, 'remote-browser-shown');
    Services.obs.removeObserver(runAppObj, 'inprocess-browser-shown');
  }
});

function AppRunner(aName) {
  this._appName = aName;
  this._apps = [];
}
AppRunner.prototype = {
  observe: function(aSubject, aTopic, aData) {
    let frameLoader = aSubject;
    // get a ref to the app <iframe>
    frameLoader.QueryInterface(Ci.nsIFrameLoader);
    // Ignore notifications that aren't from a BrowserOrApp
    if (!frameLoader.ownerIsBrowserOrAppFrame) {
      return;
    }

    let frame = frameLoader.ownerElement;
    if (!frame.appManifestURL) { // Ignore all frames but app frames
      return;
    }

    if (aTopic == 'remote-browser-shown' ||
        aTopic == 'inprocess-browser-shown') {
      this.doRunApp(frame);
    }
  },

  doRunApp: function(currentFrame) {
    // - Get the list of apps since the parameter was specified
    if (this._apps.length) {
      this.getAllSuccess(this._apps, currentFrame)
    } else {
      var req = navigator.mozApps.mgmt.getAll();
      req.onsuccess = function() {
        this._apps = req.result;
        this.getAllSuccess(this._apps, currentFrame)
      }.bind(this);
      req.onerror = this.getAllError.bind(this);
    }
  },

  getAllSuccess: function(apps, currentFrame) {
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

    if (this._appName === '') {
      usageAndDie();
      return;
    }

    let appsService = Cc["@mozilla.org/AppsService;1"].getService(Ci.nsIAppsService);
    let currentApp = appsService.getAppByManifestURL(currentFrame.appManifestURL);

    if (!currentApp || currentApp.role !== 'homescreen') {
      return;
    }

    let app = findAppWithName(this._appName);
    if (!app) {
      dump('Could not find app: "' + this._appName + '". Maybe you meant one of:\n');
      usageAndDie(true);
      return;
    }

    currentFrame.addEventListener('mozbrowserloadend', launchApp);

    function launchApp() {
      currentFrame.removeEventListener('mozbrowserloadend', launchApp);

      let setReq =
        navigator.mozSettings.createLock().set({'lockscreen.enabled': false});
      setReq.onsuccess = function() {
        // give the event loop 100ms to disable the lock screen
        window.setTimeout(function() {
          dump('--runapp launching app: ' + app.manifest.name + '\n');
          app.launch();
        }, 100);
      };
      setReq.onerror = function() {
        dump('--runapp failed to disable lock-screen.  Giving up.\n');
      };

      dump('--runapp found app: ' + app.manifest.name +
           ', disabling lock screen...\n');

      // Disable observers once we have made the request to launch the app.
      Services.obs.removeObserver(runAppObj, 'remote-browser-shown');
      Services.obs.removeObserver(runAppObj, 'inprocess-browser-shown');
    }
  },

  getAllError: function() {
    dump('Problem getting the list of all apps!');
  }
};
