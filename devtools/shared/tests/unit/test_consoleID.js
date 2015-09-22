/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

const { console, ConsoleAPI } = Cu.import("resource://gre/modules/devtools/shared/Console.jsm");

const { ConsoleAPIListener } = require("devtools/shared/webconsole/utils");

var seenMessages = 0;
var seenTypes = 0;

var callback = {
  onConsoleAPICall: function(aMessage) {
    switch (aMessage.consoleID) {
      case "foo":
        do_check_eq(aMessage.level, "warn");
        do_check_eq(aMessage.arguments[0], "Warning from foo");
        seenTypes |= 1;
        break;
      case "bar":
        do_check_eq(aMessage.level, "error");
        do_check_eq(aMessage.arguments[0], "Error from bar");
        seenTypes |= 2;
        break;
      default:
        do_check_eq(aMessage.level, "log");
        do_check_eq(aMessage.arguments[0], "Hello from default console");
        seenTypes |= 4;
        break;
    }
    seenMessages++;
  }
};

/**
 * Tests that the consoleID property of the ConsoleAPI options gets passed
 * through to console messages.
 */
function run_test() {
  let console1 = new ConsoleAPI({
    consoleID: "foo"
  });
  let console2 = new ConsoleAPI({
    consoleID: "bar"
  });

  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");

  let listener = new ConsoleAPIListener(null, callback);
  listener.init();
  let messages = listener.getCachedMessages();

  seenTypes = 0;
  seenMessages = 0;
  messages.forEach(callback.onConsoleAPICall);
  do_check_eq(seenMessages, 3);
  do_check_eq(seenTypes, 7);

  seenTypes = 0;
  seenMessages = 0;
  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");
  do_check_eq(seenMessages, 3);
  do_check_eq(seenTypes, 7);

  listener.destroy();

  listener = new ConsoleAPIListener(null, callback, "foo");
  listener.init();
  messages = listener.getCachedMessages();

  seenTypes = 0;
  seenMessages = 0;
  messages.forEach(callback.onConsoleAPICall);
  do_check_eq(seenMessages, 2);
  do_check_eq(seenTypes, 1);

  seenTypes = 0;
  seenMessages = 0;
  console.log("Hello from default console");
  console1.warn("Warning from foo");
  console2.error("Error from bar");
  do_check_eq(seenMessages, 1);
  do_check_eq(seenTypes, 1);
}
