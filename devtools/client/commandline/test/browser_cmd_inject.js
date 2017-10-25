/* Any copyright is dedicated to the Public Domain.
* http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that the inject commands works as they should

// Note that we use var instead of const here on purpose. Indeed, this test injects itself
// in the test page, twice. Using a const would result in a const redeclaration JS error.
var TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_inject.html";

function test() {
  helpers.addTabWithToolbar(TEST_URI, function (options) {
    return helpers.audit(options, [
      {
        setup:    "inject",
        check: {
          input:  "inject",
          markup: "VVVVVV",
          hints:        " <library>",
          status: "ERROR"
        },
      },
      {
        setup:    "inject j",
        check: {
          input:  "inject j",
          markup: "VVVVVVVI",
          hints:          "Query",
          status: "ERROR"
        },
      },
      {
        setup: "inject notauri",
        check: {
          input:  "inject notauri",
          hints:                " -> http://notauri/",
          markup: "VVVVVVVIIIIIII",
          status: "ERROR",
          args: {
            library: {
              value: undefined,
              status: "INCOMPLETE"
            }
          }
        }
      },
      {
        setup:    "inject http://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js",
        check: {
          input:  "inject http://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js",
          markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
          hints:                                                                                           "",
          status: "VALID",
          args: {
            library: {
              value: function (library) {
                is(library.type, "url", "inject type name");
                is(library.url.origin, "http://example.com", "inject url hostname");
                ok(library.url.pathname.indexOf("_inject.js") != -1, "inject url path");
              },
              status: "VALID"
            }
          }
        },
        exec: {
          output: [ /http:\/\/example.com\/browser\/devtools\/client\/commandline\/test\/browser_cmd_inject.js loaded/ ]
        }
      },
      {
        setup:    "inject https://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js",
        check: {
          input:  "inject https://example.com/browser/devtools/client/commandline/test/browser_cmd_inject.js",
          markup: "VVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVVV",
          hints:                                                                                            "",
          status: "VALID",
          args: {
            library: {
              value: function (library) {
                is(library.type, "url", "inject type name");
                is(library.url.origin, "https://example.com", "inject url hostname");
                ok(library.url.pathname.indexOf("_inject.js") != -1, "inject url path");
              },
              status: "VALID"
            }
          }
        },
        exec: {
          output: [ /https:\/\/example.com\/browser\/devtools\/client\/commandline\/test\/browser_cmd_inject.js loaded/ ]
        }
      }
    ]);
  }).then(finish, helpers.handleError);
}
