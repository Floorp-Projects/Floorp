/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

var SocialService = Cu.import("resource:///modules/SocialService.jsm", {}).SocialService;

XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHome.jsm");

var snippet =
'     <script>' +
'       var manifest = {' +
'         "name": "Demo Social Service",' +
'         "origin": "https://example.com",' +
'         "iconURL": "chrome://branding/content/icon16.png",' +
'         "icon32URL": "chrome://branding/content/icon32.png",' +
'         "icon64URL": "chrome://branding/content/icon64.png",' +
'         "shareURL": "https://example.com/browser/browser/base/content/test/social/social_share.html",' +
'         "postActivationURL": "https://example.com/browser/browser/base/content/test/social/social_postActivation.html",' +
'       };' +
'       function activateProvider(node) {' +
'         node.setAttribute("data-service", JSON.stringify(manifest));' +
'         var event = new CustomEvent("ActivateSocialFeature");' +
'         node.dispatchEvent(event);' +
'       }' +
'     </script>' +
'     <div id="activationSnippet" onclick="activateProvider(this)">' +
'     <img src="chrome://branding/content/icon32.png"></img>' +
'     </div>';

// enable one-click activation
var snippet2 =
'     <script>' +
'       var manifest = {' +
'         "name": "Demo Social Service",' +
'         "origin": "https://example.com",' +
'         "iconURL": "chrome://branding/content/icon16.png",' +
'         "icon32URL": "chrome://branding/content/icon32.png",' +
'         "icon64URL": "chrome://branding/content/icon64.png",' +
'         "shareURL": "https://example.com/browser/browser/base/content/test/social/social_share.html",' +
'         "postActivationURL": "https://example.com/browser/browser/base/content/test/social/social_postActivation.html",' +
'         "oneclick": true' +
'       };' +
'       function activateProvider(node) {' +
'         node.setAttribute("data-service", JSON.stringify(manifest));' +
'         var event = new CustomEvent("ActivateSocialFeature");' +
'         node.dispatchEvent(event);' +
'       }' +
'     </script>' +
'     <div id="activationSnippet" onclick="activateProvider(this)">' +
'     <img src="chrome://branding/content/icon32.png"></img>' +
'     </div>';

var gTests = [

{
  desc: "Test activation with enable panel",
  snippet: snippet,
  panel: true
},

{
  desc: "Test activation bypassing enable panel",
  snippet: snippet2,
  panel: false
}
];

function test()
{
  waitForExplicitFinish();
  requestLongerTimeout(2);
  ignoreAllUncaughtExceptions();
  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
  });

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);

      // Create a tab to run the test.
      let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");

      // Add an event handler to modify the snippets map once it's ready.
      let snippetsPromise = promiseSetupSnippetsMap(tab, test.snippet);

      // Start loading about:home and wait for it to complete, snippets should be loaded
      yield promiseTabLoadEvent(tab, "about:home", "AboutHomeLoadSnippetsCompleted");

      yield snippetsPromise;

      // ensure our activation snippet is indeed available
      yield ContentTask.spawn(tab.linkedBrowser, {}, function*(arg) {
        ok(!!content.document.getElementById("snippets"), "Found snippets element");
        ok(!!content.document.getElementById("activationSnippet"), "The snippet is present.");
      });

      yield new Promise(resolve => {
        activateProvider(tab, test.panel).then(() => {
          checkSocialUI();
          SocialService.uninstallProvider("https://example.com", function () {
            info("provider uninstalled");
            resolve();
          });
        });
      });

      // activation opened a post-activation info tab, close it.
      yield BrowserTestUtils.removeTab(gBrowser.selectedTab);
      yield BrowserTestUtils.removeTab(tab);
    }
  }).then(finish, ex => {
    ok(false, "Unexpected Exception: " + ex);
    finish();
  });
}

/**
 * Starts a load in an existing tab and waits for it to finish (via some event).
 *
 * @param aTab
 *        The tab to load into.
 * @param aUrl
 *        The url to load.
 * @param aEvent
 *        The load event type to wait for.  Defaults to "load".
 * @return {Promise} resolved when the event is handled.
 */
function promiseTabLoadEvent(aTab, aURL, aEventType="load")
{
  return new Promise(resolve => {
    info("Wait tab event: " + aEventType);
    aTab.linkedBrowser.addEventListener(aEventType, function load(event) {
      if (event.originalTarget != aTab.linkedBrowser.contentDocument ||
          event.target.location.href == "about:blank") {
        info("skipping spurious load event");
        return;
      }
      aTab.linkedBrowser.removeEventListener(aEventType, load, true);
      info("Tab event received: " + aEventType);
      resolve();
    }, true, true);
    aTab.linkedBrowser.loadURI(aURL);
  });
}

/**
 * Cleans up snippets and ensures that by default we don't try to check for
 * remote snippets since that may cause network bustage or slowness.
 *
 * @param aTab
 *        The tab containing about:home.
 * @param aSetupFn
 *        The setup function to be run.
 * @return {Promise} resolved when the snippets are ready.  Gets the snippets map.
 */
function promiseSetupSnippetsMap(aTab, aSnippet)
{
  info("Waiting for snippets map");

  return ContentTask.spawn(aTab.linkedBrowser,
                    {snippetsVersion: AboutHomeUtils.snippetsVersion,
                     snippet: aSnippet},
                    function*(arg) {
    return new Promise(resolve => {
      addEventListener("AboutHomeLoadSnippets", function load(event) {
        removeEventListener("AboutHomeLoadSnippets", load, true);

        let cw = content.window.wrappedJSObject;

        // The snippets should already be ready by this point. Here we're
        // just obtaining a reference to the snippets map.
        cw.ensureSnippetsMapThen(function (aSnippetsMap) {
          aSnippetsMap = Cu.waiveXrays(aSnippetsMap);
          console.log("Got snippets map: " +
               "{ last-update: " + aSnippetsMap.get("snippets-last-update") +
               ", cached-version: " + aSnippetsMap.get("snippets-cached-version") +
               " }");
          // Don't try to update.
          aSnippetsMap.set("snippets-last-update", Date.now());
          aSnippetsMap.set("snippets-cached-version", arg.snippetsVersion);
          // Clear snippets.
          aSnippetsMap.delete("snippets");
          aSnippetsMap.set("snippets", arg.snippet);
          resolve();
        });
      }, true, true);
    });
  });
}


function sendActivationEvent(tab) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  let doc = tab.linkedBrowser.contentDocument;
  // if our test has a frame, use it
  if (doc.defaultView.frames[0])
    doc = doc.defaultView.frames[0].document;
  let button = doc.getElementById("activationSnippet");
  BrowserTestUtils.synthesizeMouseAtCenter(button, {}, tab.linkedBrowser);
}

function activateProvider(tab, expectPanel, aCallback) {
  return new Promise(resolve => {
    if (expectPanel) {
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
        let panel = document.getElementById("servicesInstall-notification");
        panel.button.click();
      });
    }
    waitForProviderLoad().then(() => {
      checkSocialUI();
      resolve();
    });
    sendActivationEvent(tab);
  });
}

function waitForProviderLoad(cb) {
  return Promise.all([
    promiseObserverNotified("social:provider-enabled"),
    ensureFrameLoaded(gBrowser, "https://example.com/browser/browser/base/content/test/social/social_postActivation.html"),
  ]);
}
