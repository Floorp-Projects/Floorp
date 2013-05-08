/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the appcache commands works as they should

const TEST_URI = "http://sub1.test2.example.com/browser/browser/devtools/" +
                 "commandline/test/browser_cmd_appcache_valid_index.html";
let tests = {};

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    let deferred = Promise.defer();

    info("adding cache listener.");

    // Wait for site to be cached.
    gBrowser.contentWindow.applicationCache.addEventListener('cached', function BCAV_cached() {
      gBrowser.contentWindow.applicationCache.removeEventListener('cached', BCAV_cached);

      info("Site now cached, running tests.");

      deferred.resolve(helpers.audit(options, [
        {
          setup: 'appcache',
          check: {
            input:  'appcache',
            markup: 'IIIIIIII',
            status: 'ERROR',
            args: {}
          },
        },

        {
          setup: 'appcache list',
          check: {
            input:  'appcache list',
            markup: 'VVVVVVVVVVVVV',
            status: 'VALID',
            args: {},
          },
          exec: {
            output: [ /index/, /page1/, /page2/, /page3/ ]
          },
        },

        {
          setup: 'appcache list page',
          check: {
            input:  'appcache list page',
            markup: 'VVVVVVVVVVVVVVVVVV',
            status: 'VALID',
            args: {
              search: { value: 'page' },
            }
          },
          exec: {
            output: [ /page1/, /page2/, /page3/ ]
          },
          post: function(output) {
            ok(!output.contains("index"), "index is not contained in output");
          }
        },

        {
          setup: 'appcache validate',
          check: {
            input:  'appcache validate',
            markup: 'VVVVVVVVVVVVVVVVV',
            status: 'VALID',
            args: {}
          },
          exec: {
            completed: false,
            output: [ /successfully/ ]
          },
        },

        {
          setup: 'appcache validate ' + TEST_URI,
          check: {
            input:  'appcache validate ' + TEST_URI,
                  // appcache validate http://sub1.test2.example.com/browser/browser/devtools/commandline/test/browser_cmd_appcache_valid_index.html
            markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
            status: 'VALID',
            args: {
              uri: {
                value: TEST_URI
              },
            }
          },
          exec: {
            completed: false,
            output: [ /successfully/ ]
          },
        },

        {
          setup: 'appcache clear',
          check: {
            input:  'appcache clear',
            markup: 'VVVVVVVVVVVVVV',
            status: 'VALID',
            args: {},
          },
          exec: {
            output: [ /successfully/ ]
          },
        },

        {
          setup: 'appcache list',
          check: {
            input:  'appcache list',
            markup: 'VVVVVVVVVVVVV',
            status: 'VALID',
            args: {},
          },
          exec: {
            output: [ /no results/ ]
          },
          post: function(output) {
            ok(!output.contains("index"), "index is not contained in output");
            ok(!output.contains("page1"), "page1 is not contained in output");
            ok(!output.contains("page2"), "page1 is not contained in output");
            ok(!output.contains("page3"), "page1 is not contained in output");
          }
        },

        {
          setup: 'appcache viewentry --key ' + TEST_URI,
          check: {
            input:  'appcache viewentry --key ' + TEST_URI,
                  // appcache viewentry --key http://sub1.test2.example.com/browser/browser/devtools/commandline/test/browser_cmd_appcache_valid_index.html
            markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
            status: 'VALID',
            args: {}
          },
        },
      ]));
    });

    acceptOfflineCachePrompt();

    return deferred.promise;
  }).then(finish);

  function acceptOfflineCachePrompt() {
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
  }
}
