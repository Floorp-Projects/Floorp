/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

var asmjs = (function() {
  "use asm";
  function f() {
    return 1 | 0;
  }
  return { f: f };
})();
