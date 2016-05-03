/**
 * Support logic to run a test file as an installed app.
 * You really only want to do this if your test cares about the app's origin
 * or you REALLY want to double-check AvailableIn and other WebIDL-provided
 * security mechanisms.
 *
 * If you trust WebIDL, your life may be made significantly easier by just
 * setting the pref "dom.ignore_webidl_scope_checks" to true, which makes
 * BindingUtils.cpp's IsInPrivilegedApp and IsInCertifiedApp return true no
 * matter what *on the main thread*.  You are potentially out of luck on
 * workers since at the time of writing this since the values stored on
 * WorkerPrivateParent are based on the app status and ignore the pref.
 *
 * TO USE THIS:
 *
 * Make sure you have the usual header boilerplate:
 *   <script type="application/javascript" src="/tests/SimpleTest/SimpleTest.js"></script>
 *   <link rel="stylesheet" type="text/css" href="/tests/SimpleTest/test.css"/>
 *
 * You also want to add this file!
 *   <script type="application/javascript" src="shim_app_as_test.js"></script>
 *
 * In your script body, issue a call like so:
 * runAppTest({
 *   appFile: 'testapp_downloads_adopt_download.html',
 *   appManifest: 'testapp_downloads_adopt_download.manifest',
 *   appType: 'certified',
 *   extraPrefs: {
 *     set: [["dom.mozDownloads.enabled", true]]
 *   }
 * });
 *
 * You shouldn't be adding other stuff to that file.  Instead, you want
 * everything in your testapp_*.html file.  And you probably just want to copy
 * and paste from an existing one of those...
 */

  var gManifestURL;
  var gApp;
  var gOptions;

  // Load the chrome script.
  var gChromeHelper = SpecialPowers.loadChromeScript(
                        SimpleTest.getTestFileURL('shim_app_as_test_chrome.js'));

  function installApp() {
    info("installing app");
    var useOrigin = document.location.origin;
    gChromeHelper.sendAsyncMessage(
      'install',
      {
        origin: useOrigin,
        manifestURL: SimpleTest.getTestFileURL(gOptions.appManifest),
      });
  }

  function installedApp(appInfo) {
    gApp = appInfo;
    ok(!!appInfo, 'installed app');
    runTests();
  }
  gChromeHelper.addMessageListener('installed', installedApp);

  function uninstallApp() {
    info('uninstalling app');
    gChromeHelper.sendAsyncMessage('uninstall', gApp);
  }

  function uninstalledApp(success) {
    ok(success, 'uninstalled app');
    runTests();
  }
  gChromeHelper.addMessageListener('uninstalled', uninstalledApp);

  function testApp() {
    var cleanupFrame;
    var handleTestMessage = function(message) {
      if (/^OK/.exec(message)) {
        ok(true, "Message from app: " + message);
      } else if (/^KO/.exec(message)) {
        ok(false, "Message from app: " + message);
      } else if (/^INFO/.exec(message)) {
        info("Message from app: " + message.substring(5));
      } else if (/^DONE$/.exec(message)) {
        ok(true, "Messaging from app complete");
        cleanupFrame();
        runTests();
      }
    };

    // Bug 1097479 means that embed-webapps does not work if you are already
    // OOP, as we are for b2g.  So we need to have the chrome script run our
    // app in a sibling iframe to the one we're living in.  When that bug is
    // fixed or we are run in a non-b2g context, we can set this value to false
    // or otherwise conditionalize based on behaviour.
    var needSiblingIframeHack = true;

    if (needSiblingIframeHack) {
      gChromeHelper.sendAsyncMessage('run', gApp);

      gChromeHelper.addMessageListener('appMessage', handleTestMessage);
      gChromeHelper.addMessageListener('appError', function(data) {
        ok(false, "Error in app frame: " + data.message);
      });

      cleanupFrame = function() {
        gChromeHelper.sendAsyncMessage('close', {});
      };
    } else {
      var ifr = document.createElement('iframe');
      ifr.setAttribute('mozbrowser', 'true');
      ifr.setAttribute('mozapp', gApp.manifestURL);

      cleanupFrame = function() {
        ifr.removeEventListener('mozbrowsershowmodalprompt', listener);
        domParent.removeChild(ifr);
      };

      // Set us up to listen for messages from the app.
      var listener = function(e) {
        var message = e.detail.message; // e.detail.message;
        handleTestMessage(message);
      };

      // This event is triggered when the app calls "alert".
      ifr.addEventListener('mozbrowsershowmodalprompt', listener, false);
      ifr.addEventListener('mozbrowsererror', function(evt) {
        ok(false, "Error in app frame: " + evt.detail);
      });

      ifr.setAttribute('src', gApp.manifest.launch_path);
      var domParent = document.getElementById('content');
      if (!domParent) {
        document.createElement('div');
        document.body.insertBefore(domParent, document.body.firstChild);
      }
      domParent.appendChild(ifr);
    }
  }

  var tests = [
    // Permissions
    function() {
      info("pushing permissions");
      SpecialPowers.pushPermissions(
        [{ "type": "browser", "allow": 1, "context": document },
         { "type": "embed-apps", "allow": 1, "context": document },
         { "type": "webapps-manage", "allow": 1, "context": document }
        ],
        runTests);
    },

    // Preferences
    function() {
      info("pushing preferences: " + gOptions.extraPrefs.set);
      SpecialPowers.pushPrefEnv({
        "set": gOptions.extraPrefs.set
      }, runTests);
    },

    function() {
      info("enabling use of mozbrowser");
      SpecialPowers.setBoolPref("dom.mozBrowserFramesEnabled", true);
      runTests();
    },

    // No confirmation needed when an app is installed
    function() {
      SpecialPowers.autoConfirmAppInstall(function() {
        SpecialPowers.autoConfirmAppUninstall(runTests);
      });
    },

    // Installing the app
    installApp,

    // Run tests in app
    testApp,

    // Uninstall the app
    uninstallApp,
  ];

  function runTests() {
    if (!tests.length) {
      ok(true, 'DONE!');
      SimpleTest.finish();
      return;
    }

    var test = tests.shift();
    test();
  }

  SimpleTest.waitForExplicitFinish();

  function runAppTest(options) {
    gOptions = options;
    var href = document.location.href;
    gManifestURL = href.substring(0, href.lastIndexOf('/') + 1) +
      options.appManifest;
    runTests();
  }
