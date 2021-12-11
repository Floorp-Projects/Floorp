/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function add(a, b, k) {
  var result = a + b;
  return k(result);
}
