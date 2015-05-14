/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

XPCOMUtils.defineLazyModuleGetter(this, "Promise",
  "resource://gre/modules/Promise.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "Task",
  "resource://gre/modules/Task.jsm");
XPCOMUtils.defineLazyModuleGetter(this, "AboutHomeUtils",
  "resource:///modules/AboutHome.jsm");

let snippet =
'     <script>' +
'       var manifest = {' +
'         "name": "Demo Social Service",' +
'         "origin": "https://example.com",' +
'         "iconURL": "chrome://branding/content/icon16.png",' +
'         "icon32URL": "chrome://branding/content/icon32.png",' +
'         "icon64URL": "chrome://branding/content/icon64.png",' +
'         "sidebarURL": "https://example.com/browser/browser/base/content/test/social/social_sidebar_empty.html",' +
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
let snippet2 =
'     <script>' +
'       var manifest = {' +
'         "name": "Demo Social Service",' +
'         "origin": "https://example.com",' +
'         "iconURL": "chrome://branding/content/icon16.png",' +
'         "icon32URL": "chrome://branding/content/icon32.png",' +
'         "icon64URL": "chrome://branding/content/icon64.png",' +
'         "sidebarURL": "https://example.com/browser/browser/base/content/test/social/social_sidebar_empty.html",' +
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

let gTests = [

{
  desc: "Test activation with enable panel",
  snippet: snippet,
  run: function (aSnippetsMap)
  {
    let deferred = Promise.defer();
    let doc = gBrowser.selectedBrowser.contentDocument;

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    ok(!!doc.getElementById("activationSnippet"),
       "The snippet is present.");

    activateProvider(gBrowser.selectedTab, true, function() {
      ok(SocialSidebar.provider, "provider activated");
      checkSocialUI();
      is(gBrowser.contentDocument.location.href, SocialSidebar.provider.manifest.postActivationURL);
      gBrowser.removeTab(gBrowser.selectedTab);
      SocialService.uninstallProvider(SocialSidebar.provider.origin, function () {
        info("provider uninstalled");
        deferred.resolve(true);
      });
    });
    return deferred.promise;
  }
},

{
  desc: "Test activation bypassing enable panel",
  snippet: snippet2,
  run: function (aSnippetsMap)
  {
    let deferred = Promise.defer();
    let doc = gBrowser.selectedBrowser.contentDocument;

    let snippetsElt = doc.getElementById("snippets");
    ok(snippetsElt, "Found snippets element");
    ok(!!doc.getElementById("activationSnippet"),
       "The snippet is present.");

    activateProvider(gBrowser.selectedTab, false, function() {
      ok(SocialSidebar.provider, "provider activated");
      checkSocialUI();
      is(gBrowser.contentDocument.location.href, SocialSidebar.provider.manifest.postActivationURL);
      gBrowser.removeTab(gBrowser.selectedTab);
      SocialService.uninstallProvider(SocialSidebar.provider.origin, function () {
        info("provider uninstalled");
        deferred.resolve(true);
      });
    });
    return deferred.promise;
  }
}
];

function test()
{
  waitForExplicitFinish();
  requestLongerTimeout(2);
  ignoreAllUncaughtExceptions();

  Task.spawn(function () {
    for (let test of gTests) {
      info(test.desc);

      // Create a tab to run the test.
      let tab = gBrowser.selectedTab = gBrowser.addTab("about:blank");

      // Add an event handler to modify the snippets map once it's ready.
      let snippetsPromise = promiseSetupSnippetsMap(tab, test.snippet);

      // Start loading about:home and wait for it to complete.
      yield promiseTabLoadEvent(tab, "about:home", "AboutHomeLoadSnippetsCompleted");

      // This promise should already be resolved since the page is done,
      // but we still want to get the snippets map out of it.
      let snippetsMap = yield snippetsPromise;

      info("Running test");
      let testPromise = test.run(snippetsMap);
      yield testPromise;
      info("Cleanup");
      gBrowser.removeCurrentTab();
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
  let deferred = Promise.defer();
  info("Wait tab event: " + aEventType);
  aTab.linkedBrowser.addEventListener(aEventType, function load(event) {
    if (event.originalTarget != aTab.linkedBrowser.contentDocument ||
        event.target.location.href == "about:blank") {
      info("skipping spurious load event");
      return;
    }
    aTab.linkedBrowser.removeEventListener(aEventType, load, true);
    info("Tab event received: " + aEventType);
    deferred.resolve();
  }, true, true);
  aTab.linkedBrowser.loadURI(aURL);
  return deferred.promise;
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

    let obj = {};
    Cu.import("resource://gre/modules/Promise.jsm", obj);
    let deferred = obj.Promise.defer();

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
        deferred.resolve();
      });
    }, true, true);

    return deferred.promise;

  });
}


function sendActivationEvent(tab, callback) {
  // hack Social.lastEventReceived so we don't hit the "too many events" check.
  Social.lastEventReceived = 0;
  let doc = tab.linkedBrowser.contentDocument;
  // if our test has a frame, use it
  if (doc.defaultView.frames[0])
    doc = doc.defaultView.frames[0].document;
  let button = doc.getElementById("activationSnippet");
  BrowserTestUtils.synthesizeMouseAtCenter(button, {}, tab.linkedBrowser);
  executeSoon(callback);
}

function activateProvider(tab, expectPanel, aCallback) {
  if (expectPanel) {
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      panel.button.click();
    });
  }
  sendActivationEvent(tab, function() {
    waitForProviderLoad(function() {
      ok(SocialSidebar.provider, "new provider is active");
      ok(SocialSidebar.opened, "sidebar is open");
      checkSocialUI();
      executeSoon(aCallback);
    });
  });
}

function waitForProviderLoad(cb) {
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, "social:provider-enabled");
    info("social:provider-enabled observer was notified");
    waitForCondition(function() {
      let sbrowser = document.getElementById("social-sidebar-browser");
      let provider = SocialSidebar.provider;
      let postActivation = provider && gBrowser.contentDocument.location.href == provider.origin + "/browser/browser/base/content/test/social/social_postActivation.html";

      return provider &&
             postActivation &&
             sbrowser.docShellIsActive;
    }, function() {
      // executeSoon to let the browser UI observers run first
      executeSoon(cb);
    },
    "waitForProviderLoad: provider profile was not set");
  }, "social:provider-enabled", false);
}
