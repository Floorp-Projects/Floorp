/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cookie commands works as they should

const TEST_URI = "data:text/html;charset=utf-8,gcli-cookie";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: 'cookie',
        check: {
          input:  'cookie',
          hints:        '',
          markup: 'IIIIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie lis',
        check: {
          input:  'cookie lis',
          hints:            't',
          markup: 'IIIIIIVIII',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie list',
        check: {
          input:  'cookie list',
          hints:             '',
          markup: 'VVVVVVVVVVV',
          status: 'VALID'
        },
      },
      {
        setup: 'cookie remove',
        check: {
          input:  'cookie remove',
          hints:               ' <key>',
          markup: 'VVVVVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie set',
        check: {
          input:  'cookie set',
          hints:            ' <key> <value> [options]',
          markup: 'VVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie set fruit',
        check: {
          input:  'cookie set fruit',
          hints:                  ' <value> [options]',
          markup: 'VVVVVVVVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie set fruit ban',
        check: {
          input:  'cookie set fruit ban',
          hints:                      ' [options]',
          markup: 'VVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            key: { value: 'fruit' },
            value: { value: 'ban' },
            secure: { value: false },
          }
        },
      },
      {
        setup: "cookie set fruit banana",
        check: {
          args: {
            key: { value: 'fruit' },
            value: { value: 'banana' },
          }
        },
        exec: {
          output: ""
        }
      },
      {
        setup: "cookie list",
        exec: {
          output: /Key/
        }
      },
      {
        setup: "cookie remove fruit",
        check: {
          args: {
            key: { value: 'fruit' },
          }
        },
        exec: {
          output: ""
        }
      },
    ]);
  }).then(finish);
}
