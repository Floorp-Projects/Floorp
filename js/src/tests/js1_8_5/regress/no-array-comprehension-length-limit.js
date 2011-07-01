/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */
function range(n)
{
  var i = 0;
  while (i < n)
    yield i++;
}

[0 for (_ in range(Math.pow(2, 20)))];

reportCompare(true, true);
