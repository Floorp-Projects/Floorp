/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the appcache commands works as they should

const TEST_URI = "http://sub1.test2.example.com/browser/devtools/client/" +
                 "commandline/test/browser_cmd_appcache_valid_index.html";

function test() {
  return Task.spawn(spawnTest).then(finish, helpers.handleError);
}

function* spawnTest() {
  let options = yield helpers.openTab(TEST_URI);

  info("adding cache listener.");
  // Wait for site to be cached.
  yield helpers.listenOnce(gBrowser.contentWindow.applicationCache, "cached");

  yield helpers.openToolbar(options);

  // Pages containing an appcache the notification bar gives options to allow
  // or deny permission for the app to save data offline. Let's click Allow.
  let notificationID = "offline-app-requested-sub1.test2.example.com";
  let notification = PopupNotifications.getNotification(notificationID, gBrowser.selectedBrowser);

  if (notification) {
    info("Authorizing offline storage.");
    notification.mainAction.callback();
  } else {
    info("No notification box is available.");
  }

  info("Site now cached, running tests.");
  yield helpers.audit(options, [
    {
      setup: "appcache",
      check: {
        input:  "appcache",
        markup: "IIIIIIII",
        status: "ERROR",
        args: {}
      },
    },

    {
      setup: function () {
        Services.prefs.setBoolPref("browser.cache.disk.enable", false);
        return helpers.setInput(options, "appcache list", 13);
      },
      check: {
        input:  "appcache list",
        markup: "VVVVVVVVVVVVV",
        status: "VALID",
        args: {},
      },
      exec: {
        output: [ /cache is disabled/ ]
      },
      post: function (output) {
        Services.prefs.setBoolPref("browser.cache.disk.enable", true);
      }
    },

    {
      setup: "appcache list",
      check: {
        input:  "appcache list",
        markup: "VVVVVVVVVVVVV",
        status: "VALID",
        args: {},
      },
      exec: {
        output: [ /index/, /page1/, /page2/, /page3/ ]
      },
    },

    {
      setup: "appcache list page",
      check: {
        input:  "appcache list page",
        markup: "VVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          search: { value: "page" },
        }
      },
      exec: {
        output: [ /page1/, /page2/, /page3/ ]
      },
      post: function (output, text) {
        ok(!text.includes("index"), "index is not contained in output");
      }
    },

    {
      setup: "appcache validate",
      check: {
        input:  "appcache validate",
        markup: "VVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {}
      },
      exec: {
        output: [ /successfully/ ]
      },
    },

    {
      setup: "appcache validate " + TEST_URI,
      check: {
        input:  "appcache validate " + TEST_URI,
              // appcache validate http://sub1.test2.example.com/browser/devtools/client/commandline/test/browser_cmd_appcache_valid_index.html
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {
          uri: {
            value: TEST_URI
          },
        }
      },
      exec: {
        output: [ /successfully/ ]
      },
    },

    {
      setup: "appcache clear",
      check: {
        input:  "appcache clear",
        markup: "VVVVVVVVVVVVVV",
        status: "VALID",
        args: {},
      },
      exec: {
        output: [ /successfully/ ]
      },
    },

    {
      setup: "appcache list",
      check: {
        input:  "appcache list",
        markup: "VVVVVVVVVVVVV",
        status: "VALID",
        args: {},
      },
      exec: {
        output: [ /no results/ ]
      },
      post: function (output, text) {
        ok(!text.includes("index"), "index is not contained in output");
        ok(!text.includes("page1"), "page1 is not contained in output");
        ok(!text.includes("page2"), "page1 is not contained in output");
        ok(!text.includes("page3"), "page1 is not contained in output");
      }
    },

    {
      setup: "appcache viewentry --key " + TEST_URI,
      check: {
        input:  "appcache viewentry --key " + TEST_URI,
              // appcache viewentry --key http://sub1.test2.example.com/browser/devtools/client/commandline/test/browser_cmd_appcache_valid_index.html
        markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
        status: "VALID",
        args: {}
      },
    },
  ]);

  yield helpers.closeToolbar(options);
  yield helpers.closeTab(options);
}
