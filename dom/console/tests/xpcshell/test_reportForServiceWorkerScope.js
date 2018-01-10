/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

Components.utils.import("resource://gre/modules/Services.jsm");

add_task(async function() {
  let p = new Promise(resolve => {
    function consoleListener() {
      Services.obs.addObserver(this, "console-api-log-event");
    }

    consoleListener.prototype  = {
      observe: function(aSubject, aTopic, aData) {
        let obj = aSubject.wrappedJSObject;
        Assert.ok(obj.arguments[0] === "Hello world!", "Message received!");
        Assert.ok(obj.ID === "scope", "The ID is the scope");
        Assert.ok(obj.innerID === "ServiceWorker", "The innerID is ServiceWorker");
        Assert.ok(obj.filename === "filename", "The filename matches");
        Assert.ok(obj.lineNumber === 42, "The lineNumber matches");
        Assert.ok(obj.columnNumber === 24, "The columnNumber matches");
        Assert.ok(obj.level === "error", "The level is correct");

        Services.obs.removeObserver(this, "console-api-log-event");
        resolve();
      }
    };

    new consoleListener();
  });

  let ci = console.createInstance();
  ci.reportForServiceWorkerScope("scope", "Hello world!", "filename", 42, 24, "error");
  await p;
});
