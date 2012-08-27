/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inspect command works as it should

const TEST_URI = "http://example.com/browser/browser/devtools/highlighter/" +
                 "test/browser_inspector_cmd_inspect.html";

function test() {
  DeveloperToolbarTest.test(TEST_URI, [ testInspect ]);
}

function testInspect() {
  helpers.setInput('inspect');
  helpers.check({
    input:  'inspect',
    hints:         ' <selector>',
    markup: 'VVVVVVV',
    status: 'ERROR',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect h1');
  helpers.check({
    input:  'inspect h1',
    hints:            '',
    markup: 'VVVVVVVVII',
    status: 'ERROR',
    args: {
      selector: { message: 'No matches' },
    }
  });

  helpers.setInput('inspect span');
  helpers.check({
    input:  'inspect span',
    hints:              '',
    markup: 'VVVVVVVVEEEE',
    status: 'ERROR',
    args: {
      selector: { message: 'Too many matches (2)' },
    }
  });

  helpers.setInput('inspect div');
  helpers.check({
    input:  'inspect div',
    hints:             '',
    markup: 'VVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect .someclas');
  helpers.check({
    input:  'inspect .someclas',
    hints:                   '',
    markup: 'VVVVVVVVIIIIIIIII',
    status: 'ERROR',
    args: {
      selector: { message: 'No matches' },
    }
  });

  helpers.setInput('inspect .someclass');
  helpers.check({
    input:  'inspect .someclass',
    hints:                    '',
    markup: 'VVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect #someid');
  helpers.check({
    input:  'inspect #someid',
    hints:                 '',
    markup: 'VVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect button[disabled]');
  helpers.check({
    input:  'inspect button[disabled]',
    hints:                          '',
    markup: 'VVVVVVVVVVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect p>strong');
  helpers.check({
    input:  'inspect p>strong',
    hints:                  '',
    markup: 'VVVVVVVVVVVVVVVV',
    status: 'VALID',
    args: {
      selector: { message: '' },
    }
  });

  helpers.setInput('inspect :root');
  helpers.check({
    input:  'inspect :root',
    hints:               '',
    markup: 'VVVVVVVVVVVVV',
    status: 'VALID'
  });
}
