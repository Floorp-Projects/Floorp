/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cookie commands works as they should

const TEST_URI = "http://example.com/browser/browser/devtools/commandline/"+
                 "test/browser_cmd_cookie.html";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: 'cookie',
        check: {
          input:  'cookie',
          hints:        ' list',
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
          hints:               ' <name>',
          markup: 'VVVVVVVVVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'cookie set',
        check: {
          input:  'cookie set',
          hints:            ' <name> <value> [options]',
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
            name: { value: 'fruit' },
            value: { value: 'ban' },
            secure: { value: false },
          }
        },
      },
      {
        setup:    'cookie set fruit ban --path ""',
        check: {
          input:  'cookie set fruit ban --path ""',
          hints:                                ' [options]',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            name: { value: 'fruit' },
            value: { value: 'ban' },
            path: { value: '' },
            secure: { value: false },
          }
        },
      },
      {
        setup: "cookie list",
        exec: {
          output: [ /zap=zep/, /zip=zop/, /Edit/ ]
        }
      },
      {
        setup: "cookie set zup banana",
        check: {
          args: {
            name: { value: 'zup' },
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
          output: [ /zap=zep/, /zip=zop/, /zup=banana/, /Edit/ ]
        }
      },
      {
        setup: "cookie remove zip",
        exec: { },
      },
      {
        setup: "cookie list",
        exec: {
          output: [ /zap=zep/, /zup=banana/, /Edit/ ]
        },
        post: function(output, text) {
          ok(!text.contains("zip"), "");
          ok(!text.contains("zop"), "");
        }
      },
      {
        setup: "cookie remove zap",
        exec: { },
      },
      {
        setup: "cookie list",
        exec: {
          output: [ /zup=banana/, /Edit/ ]
        },
        post: function(output, text) {
          ok(!text.contains("zap"), "");
          ok(!text.contains("zep"), "");
          ok(!text.contains("zip"), "");
          ok(!text.contains("zop"), "");
        }
      },
      {
        setup: "cookie remove zup",
        exec: { }
      },
      {
        setup: "cookie list",
        exec: {
          output: 'No cookies found for host example.com'
        },
        post: function(output, text) {
          ok(!text.contains("zap"), "");
          ok(!text.contains("zep"), "");
          ok(!text.contains("zip"), "");
          ok(!text.contains("zop"), "");
          ok(!text.contains("zup"), "");
          ok(!text.contains("banana"), "");
          ok(!text.contains("Edit"), "");
        }
      },
    ]);
  }).then(finish);
}
