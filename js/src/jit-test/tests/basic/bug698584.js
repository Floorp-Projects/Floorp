// |jit-test| error: InternalError
/* 
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

const MAX = 10000;
var str = "";
for (var i = 0; i < MAX; ++i) {
    /x/.test(str);
    str += str + 'xxxxxxxxxxxxxx';
}

/* Don't crash */
