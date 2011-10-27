// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertEq(new WeakMap().set({}, 0), undefined);

reportCompare(0, 0, 'ok');
