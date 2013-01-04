/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

let imported = {};
Components.utils.import("resource:///modules/devtools/BuiltinCommands.jsm", imported);

function test() {
  DeveloperToolbarTest.test("about:blank", [ GAT_test ]);
}

function GAT_test() {
  var GAT_ready = DeveloperToolbarTest.checkCalled(function() {
    Services.obs.removeObserver(GAT_ready, "gcli_addon_commands_ready", false);
    info("gcli_addon_commands_ready notification received, running tests");

    helpers.setInput('addon list dictionary');
    helpers.check({
      input:  'addon list dictionary',
      hints:                       '',
      markup: 'VVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon list extension');
    helpers.check({
      input:  'addon list extension',
      hints:                      '',
      markup: 'VVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon list locale');
    helpers.check({
      input:  'addon list locale',
      hints:                   '',
      markup: 'VVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon list plugin');
    helpers.check({
      input:  'addon list plugin',
      hints:                   '',
      markup: 'VVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon list theme');
    helpers.check({
      input:  'addon list theme',
      hints:                  '',
      markup: 'VVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon list all');
    helpers.check({
      input:  'addon list all',
      hints:                '',
      markup: 'VVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon disable Test_Plug-in_1.0.0.0');
    helpers.check({
      input:  'addon disable Test_Plug-in_1.0.0.0',
      hints:                                    '',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID'
    });

    helpers.setInput('addon disable WRONG');
    helpers.check({
      input:  'addon disable WRONG',
      hints:                     '',
      markup: 'VVVVVVVVVVVVVVEEEEE',
      status: 'ERROR'
    });

    helpers.setInput('addon enable Test_Plug-in_1.0.0.0');
    helpers.check({
      input:  'addon enable Test_Plug-in_1.0.0.0',
      hints:                                   '',
      markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
      status: 'VALID',
      args: {
        command: { name: 'addon enable' },
        name: { value: 'Test Plug-in', status: 'VALID' },
      }
    });

    DeveloperToolbarTest.exec({ completed: false });
  });

  Services.obs.addObserver(GAT_ready, "gcli_addon_commands_ready", false);

  if (imported.CmdAddonFlags.addonsLoaded) {
    info("The getAllAddons command has already completed and we have missed ");
    info("the notification. Let's send the gcli_addon_commands_ready ");
    info("notification ourselves.");

    Services.obs.notifyObservers(null, "gcli_addon_commands_ready", null);
  } else {
    info("gcli_addon_commands_ready notification has not yet been received.");
  }
}
