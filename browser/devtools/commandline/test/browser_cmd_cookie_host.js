/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the cookie command works for host with a port specified

const TEST_URI = "http://mochi.test:8888/browser/browser/devtools/commandline/"+
                 "test/browser_cmd_cookie.html";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
        {
          setup: 'cookie list',
          exec: {
            output: [ /zap=zep/, /zip=zop/ ],
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
        }
    ]);
  }).then(finish, helpers.handleError);
}

