function makeAllAppsLaunchable() {
  var Webapps = {};
  SpecialPowers.Cu.import("resource://gre/modules/Webapps.jsm", Webapps);
  var appRegistry = SpecialPowers.wrap(Webapps.DOMApplicationRegistry);

  var originalValue = appRegistry.allAppsLaunchable;
  appRegistry.allAppsLaunchable = true;

  // Clean up after ourselves once tests are done so the test page is unloaded.
  window.addEventListener("unload", function restoreAllAppsLaunchable(event) {
    if (event.target == window.document) {
      window.removeEventListener("unload", restoreAllAppsLaunchable, false);
      appRegistry.allAppsLaunchable = originalValue;
    }
  }, false);
}

SimpleTest.waitForExplicitFinish();
makeAllAppsLaunchable();

var fileTestOnCurrentOrigin = 'http://example.org/tests/dom/tests/mochitest/webapps/file_bug_779982.html';

var previousPrefs = {
  mozBrowserFramesEnabled: undefined,
  oop_by_default: undefined,
};

try {
  previousPrefs.mozBrowserFramesEnabled = SpecialPowers.getBoolPref('dom.mozBrowserFramesEnabled');
} catch(e)
{
}

SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', true);

SpecialPowers.addPermission("browser", true, window.document);
SpecialPowers.addPermission("embed-apps", true, window.document);

var gData = [
  // APP 1
  {
    app: 'http://example.org/manifest.webapp',
    action: 'getSelf',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'getSelf() for app should return something'
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'checkInstalled',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for app should return true'
  },
  {
    app: 'http://example.org/manifest.webapp',
    action: 'checkInstalledWrong',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
  // Browser
  {
    browser: true,
    action: 'getSelf',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'getSelf() for browser should return null'
  },
  {
    browser: true,
    action: 'checkInstalled',
    isnull: false,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
  {
    browser: true,
    action: 'checkInstalledWrong',
    isnull: true,
    src: fileTestOnCurrentOrigin,
    message: 'checkInstalled() for browser should return true'
  },
];

function runTest() {
  for (var i in gData) {
    var iframe = document.createElement('iframe');
    var data = gData[i];

    if (data.app) {
      iframe.setAttribute('mozbrowser', '');
      iframe.setAttribute('mozapp', data.app);
    } else if (data.browser) {
      iframe.setAttribute('mozbrowser', '');
    }

    if (data.app || data.browser) {
      iframe.addEventListener('mozbrowsershowmodalprompt', function(e) {
        is(e.detail.message, 'success', data.message);

        i++;
        if (i >= gData.length) {
          if (previousPrefs.mozBrowserFramesEnabled !== undefined) {
            SpecialPowers.setBoolPref('dom.mozBrowserFramesEnabled', previousPrefs.mozBrowserFramesEnabled);
          }

          SpecialPowers.removePermission("browser", window.document);
          SpecialPowers.removePermission("embed-apps", window.document);

          SimpleTest.finish();
        } else {
          gTestRunner.next();
        }
      });
    }

    iframe.src = data.src + '?' + data.action + '&' + data.isnull;
    document.getElementById('content').appendChild(iframe);

    yield;
  }
}

var gTestRunner = runTest();

gTestRunner.next();
