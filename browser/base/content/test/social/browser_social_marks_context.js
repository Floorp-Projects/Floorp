var SocialService = Cu.import("resource://gre/modules/SocialService.jsm", {}).SocialService;

function makeMarkProvider(origin) {
  return { // used for testing install
    name: "mark provider " + origin,
    origin: "https://" + origin + ".example.com",
    markURL: "https://" + origin + ".example.com/browser/browser/base/content/test/social/social_mark.html?url=%{url}",
    markedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/unchecked.jpg",
    unmarkedIcon: "https://" + origin + ".example.com/browser/browser/base/content/test/social/checked.jpg",
    iconURL: "https://" + origin + ".example.com/browser/browser/base/content/test/general/moz.png",
    version: "1.0"
  }
}

function test() {
  waitForExplicitFinish();
  PopupNotifications.panel.setAttribute("animate", "false");
  registerCleanupFunction(function () {
    PopupNotifications.panel.removeAttribute("animate");
  });

  runSocialTests(tests, undefined, undefined, finish);
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
      BrowserTestUtils.waitForEvent(PopupNotifications.panel, "popupshown").then(() => {
        info("servicesInstall-notification panel opened");
        panel.button.click();
      });

      let activationURL = manifest.origin + "/browser/browser/base/content/test/social/social_activate.html"
      let id = SocialMarks._toolbarHelper.idFromOrigin(manifest.origin);
      let toolbar = document.getElementById("nav-bar");
      BrowserTestUtils.openNewForegroundTab(gBrowser, activationURL).then(tab => {
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
            BrowserTestUtils.waitForCondition(() => { return CustomizableUI.getWidget(id) },
                             "button exists after enabling social").then(() => {
              BrowserTestUtils.removeTab(tab).then(() => {
                installed.push(manifest.origin);
                // checkSocialUI will properly check where the menus are located
                checkSocialUI(window);
                executeSoon(function() {
                  addProviders(callback);
                });
              });
            });
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