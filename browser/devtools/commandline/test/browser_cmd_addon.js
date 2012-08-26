/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the addon commands works as they should

function test() {
  DeveloperToolbarTest.test("about:blank", [ GAT_test ]);
}

function GAT_test() {
  var GAT_ready = DeveloperToolbarTest.checkCalled(function() {
    Services.obs.removeObserver(GAT_ready, "gcli_addon_commands_ready", false);

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
}
