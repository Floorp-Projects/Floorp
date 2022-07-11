/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  Assert.ok("console" in this);

  let p = new Promise(resolve => {
    function consoleListener() {
      addConsoleStorageListener(this);
    }

    consoleListener.prototype = {
      observe(aSubject) {
        let obj = aSubject.wrappedJSObject;
        Assert.ok(obj.arguments[0] === 42, "Message received!");
        Assert.ok(obj.ID === "jsm", "The ID is JSM");
        Assert.ok(obj.innerID.endsWith("test_basic.js"), "The innerID matches");

        removeConsoleStorageListener(this);
        resolve();
      },
    };

    new consoleListener();
  });

  console.log(42);
  await p;
});
