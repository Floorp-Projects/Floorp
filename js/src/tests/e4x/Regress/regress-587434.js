/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

(function() {
  function::a(eval("false"), true);
  function a({}) {}
})()

/* Don't crash because of bad |this| value. */
reportCompare(0, 0, "ok");
