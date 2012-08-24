/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function test() {
  DeveloperToolbarTest.test("about:blank", [ GAT_test ]);
}

function isOpen() {
  return !!gBrowser.selectedTab.__responsiveUI;
}

function isClosed() {
  return !isOpen();
}

function GAT_test() {
  helpers.setInput('resize toggle');
  helpers.check({
    input:  'resize toggle',
    hints:               '',
    markup: 'VVVVVVVVVVVVV',
    status: 'VALID'
  });

  DeveloperToolbarTest.exec();
  ok(isOpen(), "responsive mode is open");

  helpers.setInput('resize toggle');
  helpers.check({
    input:  'resize toggle',
    hints:               '',
    markup: 'VVVVVVVVVVVVV',
    status: 'VALID'
  });

  DeveloperToolbarTest.exec();
  ok(isClosed(), "responsive mode is closed");

  helpers.setInput('resize on');
  helpers.check({
    input:  'resize on',
    hints:           '',
    markup: 'VVVVVVVVV',
    status: 'VALID'
  });

  DeveloperToolbarTest.exec();
  ok(isOpen(), "responsive mode is open");

  helpers.setInput('resize off');
  helpers.check({
    input:  'resize off',
    hints:            '',
    markup: 'VVVVVVVVVV',
    status: 'VALID'
  });

  DeveloperToolbarTest.exec();
  ok(isClosed(), "responsive mode is closed");

  helpers.setInput('resize to 400 400');
  helpers.check({
    input:  'resize to 400 400',
    hints:                   '',
    markup: 'VVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      width: { value: 400 },
      height: { value: 400 },
    }
  });

  DeveloperToolbarTest.exec();
  ok(isOpen(), "responsive mode is open");

  helpers.setInput('resize off');
  helpers.check({
    input:  'resize off',
    hints:            '',
    markup: 'VVVVVVVVVV',
    status: 'VALID'
  });

  DeveloperToolbarTest.exec();
  ok(isClosed(), "responsive mode is closed");
}
