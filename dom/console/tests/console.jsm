/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
this.EXPORTED_SYMBOLS = [ "ConsoleTest" ];

this.ConsoleTest = {
  go: function(dumpFunction) {
    console.log("Hello world!");
    console.createInstance().log("Hello world!");

    let c = console.createInstance({
      consoleID: "wow",
      innerID: "CUSTOM INNER",
      dump: dumpFunction,
      prefix: "_PREFIX_",
    });

    c.log("Hello world!");
    c.trace("Hello world!");

    console.createInstance({ innerID: "LEVEL", maxLogLevel: "off" }).log("Invisible!");
    console.createInstance({ innerID: "LEVEL",  maxLogLevel: "all" }).log("Hello world!");
    console.createInstance({ innerID: "LEVEL", maxLogLevelPref: "foo.pref" }).log("Invisible!");
    console.createInstance({ innerID: "LEVEL", maxLogLevelPref: "pref.test.console" }).log("Hello world!");
  }
};
