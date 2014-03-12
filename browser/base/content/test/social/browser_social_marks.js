/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

let manifest = { // builtin provider
  name: "provider example.com",
  origin: "https://example.com",
  sidebarURL: "https://example.com/browser/browser/base/content/test/social/social_sidebar.html",
  workerURL: "https://example.com/browser/browser/base/content/test/social/social_worker.js",
  iconURL: "https://example.com/browser/browser/base/content/test/general/moz.png"
};
let manifest2 = { // used for testing install
  name: "provider test1",
  origin: "https://test1.example.com",
  workerURL: "https://test1.example.com/browser/browser/base/content/test/social/social_worker.js",
  markURL: "https://test1.example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
  markedIcon: "https://test1.example.com/browser/browser/base/content/test/social/unchecked.jpg",
  unmarkedIcon: "https://test1.example.com/browser/browser/base/content/test/social/checked.jpg",

  iconURL: "https://test1.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};
let manifest3 = { // used for testing install
  name: "provider test2",
  origin: "https://test2.example.com",
  sidebarURL: "https://test2.example.com/browser/browser/base/content/test/social/social_sidebar.html",
  iconURL: "https://test2.example.com/browser/browser/base/content/test/general/moz.png",
  version: 1
};
function makeMarkProvider(origin) {
  return { // used for testing install
    name: "mark provider " + origin,
    origin: "https://" + origin + ".example.com",
    workerURL: "https://" + origin + ".example.com/browser/browser/base/content/test/social/social_worker.js",
    markURL: "https://" + origin + ".example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
    markedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/unchecked.jpg",
    unmarkedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/checked.jpg",

    iconURL: "https://" + origin + ".example.com/browser/browser/base/content/test/general/moz.png",
    version: 1
  }
}

function openWindowAndWaitForInit(callback) {
  let topic = "browser-delayed-startup-finished";
  let w = OpenBrowserWindow();
  Services.obs.addObserver(function providerSet(subject, topic, data) {
    Services.obs.removeObserver(providerSet, topic);
    executeSoon(() => callback(w));
  }, topic, false);
}

function test() {
  waitForExplicitFinish();

  let toolbar = document.getElementById("nav-bar");
  let currentsetAtStart = toolbar.currentSet;
  runSocialTestWithProvider(manifest, function (finishcb) {
    runSocialTests(tests, undefined, undefined, function () {
      Services.prefs.clearUserPref("social.remote-install.enabled");
      // just in case the tests failed, clear these here as well
      Services.prefs.clearUserPref("social.whitelist");
      ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
      CustomizableUI.reset();
      finishcb();
    });
  });
}

var tests = {
  testNoButtonOnEnable: function(next) {
    // we expect the addon install dialog to appear, we need to accept the
    // install from the dialog.
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    let activationURL = manifest3.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      Social.installProvider(doc, manifest3, function(addonManifest) {
        // enable the provider so we know the button would have appeared
        SocialService.addBuiltinProvider(manifest3.origin, function(provider) {
          is(provider.origin, manifest3.origin, "provider is installed");
          let id = SocialMarks._toolbarHelper.idFromOrigin(provider.origin);
          let widget = CustomizableUI.getWidget(id);
          ok(!widget || !widget.forWindow(window).node, "no button added to widget set");
          Social.uninstallProvider(manifest3.origin, function() {
            gBrowser.removeTab(tab);
            next();
          });
        });
      });
    });
  },

  testButtonOnEnable: function(next) {
    let panel = document.getElementById("servicesInstall-notification");
    PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
      PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
      info("servicesInstall-notification panel opened");
      panel.button.click();
    });

    // enable the provider now
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      let doc = tab.linkedBrowser.contentDocument;
      Social.installProvider(doc, manifest2, function(addonManifest) {
        SocialService.addBuiltinProvider(manifest2.origin, function(provider) {
          is(provider.origin, manifest2.origin, "provider is installed");
          let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
          let widget = CustomizableUI.getWidget(id).forWindow(window)
          ok(widget.node, "button added to widget set");
          checkSocialUI(window);
          gBrowser.removeTab(tab);
          next();
        });
      });
    });
  },

  testMarkPanel: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a mark button");
    let port = provider.getWorkerPort();
    ok(port, "got a port");

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      ok(!btn.disabled, "button is enabled");
      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-init-done":
            ok(true, "test-init-done received");
            ok(provider.profile.userName, "profile was set by test worker");
            // first click marks the page, second click opens the page. We have to
            // synthesize so the command event happens
            EventUtils.synthesizeMouseAtCenter(btn, {});
            // wait for the button to be marked, click to open panel
            waitForCondition(function() btn.isMarked, function() {
              is(btn.panel.state, "closed", "panel should not be visible yet");
              EventUtils.synthesizeMouseAtCenter(btn, {});
            }, "button is marked");
            break;
          case "got-social-panel-visibility":
            ok(true, "got the panel message " + e.data.result);
            if (e.data.result == "shown") {
              // unmark the page via the button in the page
              let doc = btn.contentDocument;
              let unmarkBtn = doc.getElementById("unmark");
              ok(unmarkBtn, "got the panel unmark button");
              EventUtils.sendMouseEvent({type: "click"}, unmarkBtn, btn.contentWindow);
            } else {
              // page should no longer be marked
              port.close();
              waitForCondition(function() !btn.isMarked, function() {
                // cleanup after the page has been unmarked
                gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
                  gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
                    executeSoon(function () {
                      ok(btn.disabled, "button is disabled");
                      next();
                    });
                });
                gBrowser.removeTab(tab);
              }, "button unmarked");
            }
            break;
        }
      };
      port.postMessage({topic: "test-init"});
    });
  },

  testMarkPanelLoggedOut: function(next) {
    // click on panel to open and wait for visibility
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider.enabled, "provider is enabled");
    let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
    let widget = CustomizableUI.getWidget(id);
    let btn = widget.forWindow(window).node;
    ok(btn, "got a mark button");
    let port = provider.getWorkerPort();
    ok(port, "got a port");

    // verify markbutton is disabled when there is no browser url
    ok(btn.disabled, "button is disabled");
    let activationURL = manifest2.origin + "/browser/browser/base/content/test/social/social_activate.html"
    addTab(activationURL, function(tab) {
      ok(!btn.disabled, "button is enabled");
      port.onmessage = function (e) {
        let topic = e.data.topic;
        switch (topic) {
          case "test-init-done":
            ok(true, "test-init-done received");
            ok(provider.profile.userName, "profile was set by test worker");
            port.postMessage({topic: "test-logout"});
            waitForCondition(function() !provider.profile.userName,
                function() {
                  // when the provider has not indicated to us that a user is
                  // logged in, the first click opens the page.
                  EventUtils.synthesizeMouseAtCenter(btn, {});
                },
                "profile was unset by test worker");
            break;
          case "got-social-panel-visibility":
            ok(true, "got the panel message " + e.data.result);
            if (e.data.result == "shown") {
              // our test marks the page during the load event (see
              // social_mark.html) regardless of login state, unmark the page
              // via the button in the page
              let doc = btn.contentDocument;
              let unmarkBtn = doc.getElementById("unmark");
              ok(unmarkBtn, "got the panel unmark button");
              EventUtils.sendMouseEvent({type: "click"}, unmarkBtn, btn.contentWindow);
            } else {
              // page should no longer be marked
              port.close();
              waitForCondition(function() !btn.isMarked, function() {
                // cleanup after the page has been unmarked
                gBrowser.tabContainer.addEventListener("TabClose", function onTabClose() {
                  gBrowser.tabContainer.removeEventListener("TabClose", onTabClose);
                    executeSoon(function () {
                      ok(btn.disabled, "button is disabled");
                      next();
                    });
                });
                gBrowser.removeTab(tab);
              }, "button unmarked");
            }
            break;
        }
      };
      port.postMessage({topic: "test-init"});
    });
  },

  testButtonOnDisable: function(next) {
    // enable the provider now
    let provider = Social._getProviderFromOrigin(manifest2.origin);
    ok(provider, "provider is installed");
    SocialService.removeProvider(manifest2.origin, function() {
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest2.origin);
      waitForCondition(function() {
                        // getWidget now returns null since we've destroyed the widget
                        return !CustomizableUI.getWidget(id)
                       },
                       function() {
                         checkSocialUI(window);
                         Social.uninstallProvider(manifest2.origin, next);
                       }, "button does not exist after disabling the provider");
    });
  },

  testContextSubmenu: function(next) {
    // install 4 providers to test that the menu's are added as submenus
    let manifests = [
      makeMarkProvider("sub1.test1"),
      makeMarkProvider("sub2.test1"),
      makeMarkProvider("sub1.test2"),
      makeMarkProvider("sub2.test2")
    ];
    let installed = [];
    let markLinkMenu = document.getElementById("context-marklinkMenu").firstChild;
    let markPageMenu = document.getElementById("context-markpageMenu").firstChild;

    function addProviders(callback) {
      let manifest = manifests.pop();
      if (!manifest) {
        info("INSTALLATION FINISHED");
        executeSoon(callback);
        return;
      }
      info("INSTALLING " + manifest.origin);
      let panel = document.getElementById("servicesInstall-notification");
      PopupNotifications.panel.addEventListener("popupshown", function onpopupshown() {
        PopupNotifications.panel.removeEventListener("popupshown", onpopupshown);
        info("servicesInstall-notification panel opened");
        panel.button.click();
      })

      let activationURL = manifest.origin + "/browser/browser/base/content/test/social/social_activate.html"
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest.origin);
      let toolbar = document.getElementById("nav-bar");
      addTab(activationURL, function(tab) {
        let doc = tab.linkedBrowser.contentDocument;
        Social.installProvider(doc, manifest, function(addonManifest) {
          // enable the provider so we know the button would have appeared
          SocialService.addBuiltinProvider(manifest.origin, function(provider) {
            waitForCondition(function() { return CustomizableUI.getWidget(id) },
                             function() {
              gBrowser.removeTab(tab);
              installed.push(manifest.origin);
              // checkSocialUI will properly check where the menus are located
              checkSocialUI(window);
              executeSoon(function() {
                addProviders(callback);
              });
            }, "button exists after enabling social");
          });
        });
      });
    }

    function removeProviders(callback) {
      let origin = installed.pop();
      if (!origin) {
        executeSoon(callback);
        return;
      }
      Social.uninstallProvider(origin, function(provider) {
        executeSoon(function() {
          removeProviders(callback);
        });
      });
    }

    addProviders(function() {
      removeProviders(function() {
        is(SocialMarks.getProviders().length, 0, "mark providers removed");
        is(markLinkMenu.childNodes.length, 0, "marklink menu ok");
        is(markPageMenu.childNodes.length, 0, "markpage menu ok");
        checkSocialUI(window);
        next();
      });
    });
  }
}
