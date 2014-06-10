/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inject commands works as they should

const TEST_URI = 'http://example.com/browser/browser/devtools/commandline/'+
                 'test/browser_cmd_inject.html';

function test() {
  helpers.addTabWithToolbar(TEST_URI, function(options) {
    return helpers.audit(options, [
      {
        setup: 'inject',
        check: {
          input:  'inject',
          hints:  ' <library>',
          markup: 'VVVVVV',
          status: 'ERROR'
        },
      },
      {
        setup: 'inject j',
        check: {
          input:  'inject j',
          hints:  'Query',
          markup: 'VVVVVVVI',
          status: 'ERROR'
        },
      },
      {
        setup: 'inject http://example.com/browser/browser/devtools/commandline/test/browser_cmd_inject.js',
        check: {
          input:  'inject http://example.com/browser/browser/devtools/commandline/test/browser_cmd_inject.js',
          hints:  '',
          markup: 'VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            library: {
              value: function(library) {
                is(library.type, 'string', 'inject type name');
                is(library.string, 'http://example.com/browser/browser/devtools/commandline/test/browser_cmd_inject.js',
                  'inject uri data');
              },
              status: 'VALID'
            }
          }
        },
        exec: {
          output: [ /http:\/\/example.com\/browser\/browser\/devtools\/commandline\/test\/browser_cmd_inject.js loaded/ ]
        }
      },
      {
        setup: 'inject notauri',
        check: {
          input:  'inject notauri',
          hints:  '',
          markup: 'VVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            library: {
              value: function(library) {
                is(library.type, 'string', 'inject type name');
                is(library.string, 'notauri', 'inject notauri data');
              },
              status: 'VALID'
            }
          }
        },
        exec: {
          output: [ /Failed to load notauri - Invalid URI/ ]
        }
      },
      {
        setup: 'inject jQuery',
        check: {
          input:  'inject jQuery',
          hints:  '',
          markup: 'VVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            library: {
              value: function(library) {
                is(library.type, 'selection', 'inject type name');
                is(library.selection.name, 'jQuery', 'inject jquery name');
                is(library.selection.src, 'http://ajax.googleapis.com/ajax/libs/jquery/2.1.1/jquery.min.js',
                  'inject jquery src');
              },
              status: 'VALID'
            }
          }
        },
        exec: {
          output: [ /jQuery loaded/ ]
        }
      },
    ]);
  }).then(finish, helpers.handleError);
}
