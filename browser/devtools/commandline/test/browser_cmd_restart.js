/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that restart command works properly (input wise)

const TEST_URI = "data:text/html;charset=utf-8,gcli-command-restart";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testRestart ]);
}

function testRestart() {
  helpers.setInput('restart');
  helpers.check({
    input:  'restart',
    markup: 'VVVVVVV',
    status: 'VALID',
    args: {
      nocache: { value: false },
    }
  });

  helpers.setInput('restart --nocache');
  helpers.check({
    input:  'restart --nocache',
    markup: 'VVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      nocache: { value: true },
    }
  });
}
