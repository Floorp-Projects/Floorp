/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

///////////////////
//
// Whitelisting this test.
// As part of bug 1077403, the leaking uncaught rejection should be fixed.
//
thisTestLeaksUncaughtRejectionsAndShouldBeFixed("destroy");

function test() {
  function isOpen() {
    return !!gBrowser.selectedTab.__responsiveUI;
  }

  helpers.addTabWithToolbar("about:blank", function(options) {
    return helpers.audit(options, [
      {
        setup: "resize toggle",
        check: {
          input:  'resize toggle',
          hints:               '',
          markup: 'VVVVVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(isOpen(), "responsive mode is open");
        },
      },
      {
        setup: "resize toggle",
        check: {
          input:  'resize toggle',
          hints:               '',
          markup: 'VVVVVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(!isOpen(), "responsive mode is closed");
        },
      },
      {
        setup: "resize on",
        check: {
          input:  'resize on',
          hints:           '',
          markup: 'VVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(isOpen(), "responsive mode is open");
        },
      },
      {
        setup: "resize off",
        check: {
          input:  'resize off',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(!isOpen(), "responsive mode is closed");
        },
      },
      {
        setup: "resize to 400 400",
        check: {
          input:  'resize to 400 400',
          hints:                   '',
          markup: 'VVVVVVVVVVVVVVVVV',
          status: 'VALID',
          args: {
            width: { value: 400 },
            height: { value: 400 },
          }
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(isOpen(), "responsive mode is open");
        },
      },
      {
        setup: "resize off",
        check: {
          input:  'resize off',
          hints:            '',
          markup: 'VVVVVVVVVV',
          status: 'VALID'
        },
        exec: {
          output: ""
        },
        post: function() {
          ok(!isOpen(), "responsive mode is closed");
        },
      },
    ]);
  }).then(finish);
}
