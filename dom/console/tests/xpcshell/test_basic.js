/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  Assert.ok("console" in this);

  let p = new Promise(resolve => {
    function consoleListener() {
      Services.obs.addObserver(this, "console-api-log-event");
    }

    consoleListener.prototype  = {
      observe: function(aSubject, aTopic, aData) {
        let obj = aSubject.wrappedJSObject;
        Assert.ok(obj.arguments[0] === 42, "Message received!");
        Assert.ok(obj.ID === "jsm", "The ID is JSM");
        Assert.ok(obj.innerID.endsWith("test_basic.js"), "The innerID matches");

        Services.obs.removeObserver(this, "console-api-log-event");
        resolve();
      }
    };

    new consoleListener();
  });

  console.log(42);
  await p;
});
