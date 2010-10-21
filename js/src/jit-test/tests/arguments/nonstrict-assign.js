/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
load(libdir + 'array-compare.js');

function assign(a)
{
  a = 17;
  return arguments;
}

var a1;
for (var i = 0; i < 5; i++)
  a1 = assign(1);

assertEq(arraysEqual(a1, [17]), true);
