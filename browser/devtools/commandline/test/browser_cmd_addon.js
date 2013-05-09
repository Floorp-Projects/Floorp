/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

let CmdAddonFlags = (Cu.import("resource:///modules/devtools/BuiltinCommands.jsm", {})).CmdAddonFlags;

let tests = {};

function test() {
  helpers.addTabWithToolbar("about:blank", function(options) {
    return helpers.runTests(options, tests);
  }).then(finish);
}

tests.gatTest = function(options) {
  let deferred = Promise.defer();

  let onGatReady = function() {
    Services.obs.removeObserver(onGatReady, "gcli_addon_commands_ready");
    info("gcli_addon_commands_ready notification received, running tests");

    let auditDone = helpers.audit(options, [
      {
        setup: 'addon list dictionary',
        check: {
          input:  'addon list dictionary',
          hints:                       '',
          markup: 'VVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon list extension',
        check: {
          input:  'addon list extension',
          hints:                      '',
          markup: 'VVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon list locale',
        check: {
          input:  'addon list locale',
          hints:                   '',
          markup: 'VVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon list plugin',
        check: {
          input:  'addon list plugin',
          hints:                   '',
          markup: 'VVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon list theme',
        check: {
          input:  'addon list theme',
          hints:                  '',
          markup: 'VVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon list all',
        check: {
          input:  'addon list all',
          hints:                '',
          markup: 'VVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon disable Test_Plug-in_1.0.0.0',
        check: {
          input:  'addon disable Test_Plug-in_1.0.0.0',
          hints:                                    '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID'
        }
      },
      {
        setup: 'addon disable WRONG',
        check: {
          input:  'addon disable WRONG',
          hints:                     '',
          markup: 'VVVVVVVVVVVVVVEEEEE',
          status: 'ERROR'
        }
      },
      {
        setup: 'addon enable Test_Plug-in_1.0.0.0',
        check: {
          input:  'addon enable Test_Plug-in_1.0.0.0',
          hints:                                   '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            command: { name: 'addon enable' },
            name: { value: 'Test Plug-in', status: 'VALID' },
          }
        },
        exec: {
          completed: false
        }
      }
    ]);

    auditDone.then(function() {
      deferred.resolve();
    });
  };

  Services.obs.addObserver(onGatReady, "gcli_addon_commands_ready", false);

  if (CmdAddonFlags.addonsLoaded) {
    info("The call to AddonManager.getAllAddons in BuiltinCommands.jsm is done.");
    info("Send the gcli_addon_commands_ready notification ourselves.");

    Services.obs.notifyObservers(null, "gcli_addon_commands_ready", null);
  } else {
    info("Waiting for gcli_addon_commands_ready notification.");
  }

  return deferred.promise;
};
