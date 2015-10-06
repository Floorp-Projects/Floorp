/**
 * This is the chrome helper for shim_app_as_test.js.  Its load is triggered by
 * shim_app_as_test.js by a call to SpecialPowers.loadChromeScript and runs
 * in the parent process in a sandbox created with the system principal.  (Which
 * seems like it can never get collected because it's reachable via the
 * apparently singleton SpecialPowersObserverAPI instance and there's no logic
 * to support reaping.  Wuh-oh.)
 *
 * It exists to help install fake privileged/certified applications.  It needs
 * to exist because:
 * - We need to poke at DOMApplicationRegistry directly.
 * - By using SpecialPowers.loadChromeScript we are able to ensure this file
 *   is run in the parent process.  This is important because
 *   DOMApplicationRegistry only lives in the parent process!
 * - By running entirely in a chrome privileged compartment, we avoid crazy
 *   wrapper problems that we would otherwise face with our shenanigans of
 *   directly meddling with DOMApplicationRegistry.  (And hopefully save
 *   anyone changing DOMApplicationRegistry from frustration/hating us if
 *   things were just barely working.)
 * - Bug 1097479 means that embed-webapps doesn't work when the content process
 *   that is telling us to do things is itself OOP.  So it falls upon us to
 *   handle the running of the app by creating a sibling mozbrowser/mozapp
 *   iframe to the one running the mochitests.
 *
 * Note that in this file we try to do *only* those things that can't otherwise
 * be cleanly done using SpecialPowers.
 *
 * Want to better understand our execution context?  Check out
 * SpecialPowersObserverAPI.js and search on SPLoadChromeScript.
 */

var Cc = Components.classes;
var Ci = Components.interfaces;
var Cu = Components.utils;
var CC = Components.Constructor;

Cu.import('resource://gre/modules/Webapps.jsm'); // for DOMApplicationRegistry
Cu.import('resource://gre/modules/AppsUtils.jsm'); // for AppUtils
Cu.import('resource://gre/modules/Services.jsm'); // for AppUtils

// Yes, you would think there was something like this already exposed easily
// in a JSM somewhere.  No.
function fetchManifest(manifestURL) {
  return new Promise(function(resolve, reject) {
    let xhr = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                .createInstance(Ci.nsIXMLHttpRequest);
    xhr.open("GET", manifestURL, true);
    xhr.responseType = "json";

    xhr.addEventListener("load", function() {
      if (xhr.status == 200) {
        resolve(xhr.response);
      } else {
        reject();
      }
    });

    xhr.addEventListener("error", function() {
      reject();
    });

    xhr.send(null);
  });
}

/**
 * Install an app using confirmInstall using pre-chewed data.  This avoids the
 * check in the normal installApp flow that gets all judgemental about the
 * installation of privileged and certified apps.
 */
function installApp(req) {
  fetchManifest(req.manifestURL).then(function(manifestObj) {
    var data = {
      // cloneAppObj normalizes the representation for us
      app: AppsUtils.cloneAppObject({
        installOrigin: req.origin,
        origin: req.origin,
        manifestURL: req.manifestURL,
        appStatus: AppsUtils.getAppManifestStatus(manifestObj),
        receipts: [],
        categories: []
      }),

      from: req.origin, // unused?
      oid: 0, // unused?
      requestID: 0, // unused-ish
      appId: 0, // unused
      isBrowser: false,
      isPackage: false, // used
      // magic to auto-ack... don't think we care about this...
      forceSuccessAck: false
      // stuff that probably doesn't matter: 'mm', 'apkInstall',
    };
    // cloneAppObject does not propagate the manifest
    data.app.manifest = manifestObj;

    return DOMApplicationRegistry.confirmInstall(data).then(
      function() {
        var appId =
          DOMApplicationRegistry.getAppLocalIdByManifestURL(req.manifestURL);
        // act like this is a privileged app having all of its permissions
        // authorized at first run.
        DOMApplicationRegistry.updatePermissionsForApp(
          appId,
          /* preinstalled */ true,
          /* system update? */ true);

        sendAsyncMessage(
          'installed',
          {
            appId: appId,
            manifestURL: req.manifestURL,
            manifest: manifestObj
          });
      },
      function(err) {
        sendAsyncMessage('installed', false);
      });
  });
}

function uninstallApp(appInfo) {
  DOMApplicationRegistry.uninstall(appInfo.manifestURL).then(
    function() {
      sendAsyncMessage('uninstalled', true);
    },
    function() {
      sendAsyncMessage('uninstalled', false);
    });
}

var activeIframe = null;

/**
 * Run our app in a sibling mozbrowser/mozapp iframe to the mochitest iframe.
 * This is needed because we can't nest mozbrowser/mozapp iframes inside our
 * already-OOP iframe until bug 1097479 is resolved.
 */
function runApp(appInfo) {
  let shellDomWindow = Services.wm.getMostRecentWindow('navigator:browser');
  let sysAppFrame = shellDomWindow.document.body.querySelector('#systemapp');
  let sysAppDoc = sysAppFrame.contentDocument;

  let siblingFrame = sysAppDoc.body.querySelector('#test-container');

  let ifr = activeIframe = sysAppDoc.createElement('iframe');
  ifr.setAttribute('mozbrowser', 'true');
  ifr.setAttribute('remote', 'true');
  ifr.setAttribute('mozapp', appInfo.manifestURL);

  ifr.addEventListener('mozbrowsershowmodalprompt', function(evt) {
    var message = evt.detail.message;
    // only send the message as long as we haven't been told to clean up.
    if (activeIframe) {
      sendAsyncMessage('appMessage', message);
    }
  }, false);
  ifr.addEventListener('mozbrowsererror', function(evt) {
    if (activeIframe) {
      sendAsyncMessage('appError', { message: '' + evt.detail });
    }
  });

  ifr.setAttribute('src', appInfo.manifest.launch_path);
  siblingFrame.parentElement.appendChild(ifr);
}

function closeApp() {
  if (activeIframe) {
    activeIframe.parentElement.removeChild(activeIframe);
    activeIframe = null;
  }
}

addMessageListener('install', installApp);
addMessageListener('uninstall', uninstallApp);
addMessageListener('run', runApp);
addMessageListener('close', closeApp);
