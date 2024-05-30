// |jit-test| tz-pacific; skip-if: typeof Intl === 'undefined'

let tzRE = /\(([^\)]+)\)/;

// Make sure we aren't already running with UTC
let original = new Date(0);
assertEq(tzRE.exec(original.toString())[1], "Pacific Standard Time");

let originalDT = Intl.DateTimeFormat("en-US", {
  dateStyle: "full",
  timeStyle: "full",
});
assertEq(originalDT.format(original).endsWith("Pacific Standard Time"), true);
assertEq(originalDT.resolvedOptions().timeZone, "PST8PDT");

let global = newGlobal({forceUTC: true});

let date = new global.Date();
assertEq(tzRE.exec(date.toString())[1], "Coordinated Universal Time");
assertEq(tzRE.exec(date.toTimeString())[1], "Coordinated Universal Time");
assertEq(date.getFullYear(), date.getUTCFullYear());
assertEq(date.getMonth(), date.getUTCMonth());
assertEq(date.getDate(), date.getUTCDate());
assertEq(date.getDay(), date.getUTCDay());
assertEq(date.getHours(),date.getUTCHours());
assertEq(date.getTimezoneOffset(), 0);

let dt = global.Intl.DateTimeFormat("en-US", {
  dateStyle: "full",
  timeStyle: "full",
});
assertEq(dt.format(date).endsWith("Coordinated Universal Time"), true);
assertEq(dt.resolvedOptions().timeZone, "UTC");
