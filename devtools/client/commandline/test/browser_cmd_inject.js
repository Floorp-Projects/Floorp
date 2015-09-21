/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inject commands works as they should

const TEST_URI = 'http://example.com/browser/devtools/client/commandline/'+
                 'test/browser_cmd_inject.html';

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup:    'inject',
        check: {
          input:  'inject',
          markup: 'VVVVVV',
          hints:        ' <library>',
          status: 'ERROR'
        },
      },
      {
        setup:    'inject j',
        check: {
          input:  'inject j',
          markup: 'VVVVVVVI',
          hints:          'Query',
          status: 'ERROR'
        },
      },
      {
        setup: 'inject notauri',
        check: {
          input:  'inject notauri',
          hints:                ' -> http://notauri/',
          markup: 'VVVVVVVIIIIIII',
          status: 'ERROR',
          args: {
            library: {
              value: undefined,
              status: 'INCOMPLETE'
            }
          }
        }
      },
      {
        setup:    'inject http://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js',
        check: {
          input:  'inject http://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          hints:                                                                                            '',
          status: 'VALID',
          args: {
            library: {
              value: function(library) {
                is(library.type, 'url', 'inject type name');
                is(library.url.origin, 'http://example.com', 'inject url hostname');
                ok(library.url.path.indexOf('_inject.js') != -1, 'inject url path');
              },
              status: 'VALID'
            }
          }
        },
        exec: {
          output: [ /http:\/\/example.com\/browser\/browser\/devtools\/commandline\/test\/browser_cmd_inject.js loaded/ ]
        }
      }
    ]);
  }).then(finish, helpers.handleError);
}
