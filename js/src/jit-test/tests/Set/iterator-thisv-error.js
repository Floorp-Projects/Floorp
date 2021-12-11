/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/ */

function test(fn, thisv) {
  var message;
  try {
    fn.call(thisv);
  } catch (e) {
    message = e.message;
  }

  assertEq(/^values method called on incompatible.+/.test(message), true);
}

for (var thisv of [null, undefined, false, true, 0, ""]) {
  test(Set.prototype.values, thisv);
  test(Set.prototype.keys, thisv);
  test(Set.prototype[Symbol.iterator], thisv);
}
