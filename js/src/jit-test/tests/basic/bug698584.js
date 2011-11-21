// |jit-test| allow-oom;
/* 
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

try
{ 
  const MAX = 10000;
  var str = "";
  for (var i = 0; i < MAX; ++i) {
      /x/.test(str);
      str += str + 'xxxxxxxxxxxxxx';
  }
}
catch (e)
{
  assertEq(""+e, "InternalError: allocation size overflow");
}

/* Don't crash */
