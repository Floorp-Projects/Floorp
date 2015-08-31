let SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

function makeMarkProvider(origin) {
  return { // used for testing install
    name: "mark provider " + origin,
    origin: "https://" + origin + ".example.com",
    markURL: "https://" + origin + ".example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
    markedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/unchecked.jpg",
    unmarkedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/checked.jpg",
    iconURL: "https://" + origin + ".example.com/browser/browser/base/content/test/general/moz.png",
    version: 1
  }
}

function test() {
  waitForExplicitFinish();

  runSocialTests(tests, undefined, undefined, function () {
    ok(CustomizableUI.inDefaultState, "Should be in the default state when we finish");
    CustomizableUI.reset();
    finish();
  });
}

var tests = {
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
      ensureEventFired(PopupNotifications.panel, "popupshown").then(() => {
        info("servicesInstall-notification panel opened");
        panel.button.click();
      });

      let activationURL = manifest.origin + "/browser/browser/base/content/test/social/social_activate.html"
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest.origin);
      let toolbar = document.getElementById("nav-bar");
      addTab(activationURL, function(tab) {
        let doc = tab.linkedBrowser.contentDocument;
        let data = {
          origin: doc.nodePrincipal.origin,
          url: doc.location.href,
          manifest: manifest,
          window: window
        }

        Social.installProvider(data, function(addonManifest) {
          // enable the provider so we know the button would have appeared
          SocialService.enableProvider(manifest.origin, function(provider) {
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