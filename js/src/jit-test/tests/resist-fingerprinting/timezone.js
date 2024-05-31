// |jit-test| tz-pacific; skip-if: typeof Intl === 'undefined'

let tzRE = /\(([^\)]+)\)/;

const SPOOFED_TZ_NAME = "Atlantic/Reykjavik";
const SPOOFED_TZ_GENERIC = "Greenwich Mean Time";

// Make sure we aren't already running with spoofed TZ
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
assertEq(tzRE.exec(date.toString())[1], SPOOFED_TZ_GENERIC);
assertEq(tzRE.exec(date.toTimeString())[1], SPOOFED_TZ_GENERIC);
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
assertEq(dt.format(date).endsWith(SPOOFED_TZ_GENERIC), true);
assertEq(dt.resolvedOptions().timeZone, SPOOFED_TZ_NAME);
