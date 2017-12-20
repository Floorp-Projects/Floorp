/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  do_check_true("console" in this);

  let p = new Promise(resolve => {
    function consoleListener() {
      Services.obs.addObserver(this, "console-api-log-event");
    }

    consoleListener.prototype  = {
      observe: function(aSubject, aTopic, aData) {
        let obj = aSubject.wrappedJSObject;
        do_check_true(obj.arguments[0] === 42, "Message received!");
        do_check_true(obj.ID === "jsm", "The ID is JSM");
        do_check_true(obj.innerID.endsWith("test_basic.js"), "The innerID matches");

        Services.obs.removeObserver(this, "console-api-log-event");
        resolve();
      }
    };

    new consoleListener();
  });

  console.log(42);
  await p;
});
