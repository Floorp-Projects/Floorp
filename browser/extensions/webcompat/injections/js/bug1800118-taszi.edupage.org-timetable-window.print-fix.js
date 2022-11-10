"use strict";

/**
 * Bug 1800118 - Timetable does not load on taszi.edupage.org/timetable
 *
 * We can define window.print as a no-op to work around the breakage,
 * which is caused by https://github.com/mozilla-mobile/fenix/issues/13214
 *
 * See https://bugzilla.mozilla.org/show_bug.cgi?id=1800118 for details.
 */

/* globals exportFunction */

Object.defineProperty(window.wrappedJSObject, "print", {
  value: exportFunction(function() {}, window),
});
