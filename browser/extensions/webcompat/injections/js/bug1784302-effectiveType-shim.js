"use strict";

/**
 * Bug 1784302 - Issues due to missing navigator.connection after
 * https://bugzilla.mozilla.org/show_bug.cgi?id=1637922 landed.
 * Webcompat issue #104838 - https://github.com/webcompat/web-bugs/issues/104838
 */

/* globals cloneInto, exportFunction */

console.info(
  "navigator.connection has been shimmed for compatibility reasons. See https://bugzilla.mozilla.org/show_bug.cgi?id=1756692 for details."
);

var connection = {
  addEventListener: () => {},
  removeEventListener: () => {},
  effectiveType: "4g",
};

window.navigator.wrappedJSObject.connection = cloneInto(connection, window, {
  cloneFunctions: true,
});
