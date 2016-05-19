/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that screenshot command works properly
const TEST_URI = "http://example.com/browser/devtools/client/commandline/" +
                 "test/browser_cmd_media.html";
var tests = {
  testInput: function (options) {
    return helpers.audit(options, [
      {
        setup: "media emulate braille",
        check: {
          input:  "media emulate braille",
          markup: "VVVVVVVVVVVVVVVVVVVVV",
          status: "VALID",
          args: {
            type: { value: "braille"},
          }
        },
      },
      {
        setup: "media reset",
        check: {
          input:  "media reset",
          markup: "VVVVVVVVVVV",
          status: "VALID",
          args: {
          }
        },
      },
    ]);
  },

  testEmulateMedia: function (options) {
    return helpers.audit(options, [
      {
        setup: "media emulate braille",
        check: {
          args: {
            type: { value: "braille"}
          }
        },
        exec: {
          output: ""
        },
        post: function () {
          let body = options.window.document.body;
          let style = options.window.getComputedStyle(body);
          is(style.backgroundColor, "rgb(255, 255, 0)", "media correctly emulated");
        }
      }
    ]);
  },

  testEndMediaEmulation: function (options) {
    return helpers.audit(options, [
      {
        setup: function () {
          let mDV = options.browser.markupDocumentViewer;
          mDV.emulateMedium("embossed");
          return helpers.setInput(options, "media reset");
        },
        exec: {
          output: ""
        },
        post: function () {
          let body = options.window.document.body;
          let style = options.window.getComputedStyle(body);
          is(style.backgroundColor, "rgb(255, 255, 255)", "media reset");
        }
      }
    ]);
  }
};

function test() {
  return Task.spawn(function* () {
    let options = yield helpers.openTab(TEST_URI);
    yield helpers.openToolbar(options);

    yield helpers.runTests(options, tests);

    yield helpers.closeToolbar(options);
    yield helpers.closeTab(options);
  }).then(finish, helpers.handleError);
}
