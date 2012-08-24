/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cookie commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,gcli-cookie";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testCookieCheck, testCookieExec ]);
}

function testCookieCheck() {
  helpers.setInput('cookie');
  helpers.check({
    input:  'cookie',
    hints:        '',
    markup: 'IIIIII',
    status: 'ERROR'
  });

  helpers.setInput('cookie lis');
  helpers.check({
    input:  'cookie lis',
    hints:            't',
    markup: 'IIIIIIVIII',
    status: 'ERROR'
  });

  helpers.setInput('cookie list');
  helpers.check({
    input:  'cookie list',
    hints:             '',
    markup: 'VVVVVVVVVVV',
    status: 'VALID'
  });

  helpers.setInput('cookie remove');
  helpers.check({
    input:  'cookie remove',
    hints:               ' <key>',
    markup: 'VVVVVVVVVVVVV',
    status: 'ERROR'
  });

  helpers.setInput('cookie set');
  helpers.check({
    input:  'cookie set',
    hints:            ' <key> <value> [options]',
    markup: 'VVVVVVVVVV',
    status: 'ERROR'
  });

  helpers.setInput('cookie set fruit');
  helpers.check({
    input:  'cookie set fruit',
    hints:                  ' <value> [options]',
    markup: 'VVVVVVVVVVVVVVVV',
    status: 'ERROR'
  });

  helpers.setInput('cookie set fruit ban');
  helpers.check({
    input:  'cookie set fruit ban',
    hints:                      ' [options]',
    markup: 'VVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      key: { value: 'fruit' },
      value: { value: 'ban' },
      secure: { value: false },
    }
  });
}

function testCookieExec() {
  DeveloperToolbarTest.exec({
    typed: "cookie set fruit banana",
    args: {
      key: "fruit",
      value: "banana",
      path: "/",
      domain: null,
      secure: false
    },
    blankOutput: true,
  });

  DeveloperToolbarTest.exec({
    typed: "cookie list",
    outputMatch: /Key/
  });

  DeveloperToolbarTest.exec({
    typed: "cookie remove fruit",
    args: { key: "fruit" },
    blankOutput: true,
  });
}
