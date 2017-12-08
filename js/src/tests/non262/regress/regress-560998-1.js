// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Jesse Ruderman

for (let j = 0; j < 4; ++j) {
  function g() { j; }
  g();
}

reportCompare(0, 0, "ok");
