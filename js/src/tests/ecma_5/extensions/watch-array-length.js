/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var hitCount;
function watcher(p,o,n) { hitCount++; return n; }

var a = [1];
a.watch('length', watcher);
hitCount = 0;
a.length = 0;
reportCompare(1, hitCount, "lenient; configurable: watchpoint hit");

var b = Object.defineProperty([1],'0',{configurable:false});
b.watch('length', watcher);
hitCount = 0;
var result;
try {
    b.length = 0;    
    result = "no error";
} catch (x) {
    result = x.toString();
}
reportCompare(1, hitCount, "lenient; non-configurable: watchpoint hit");
reportCompare(1, b.length, "lenient; non-configurable: length unchanged");
reportCompare("no error", result, "lenient; non-configurable: no error thrown");

var c = Object.defineProperty([1],'0',{configurable:false});
c.watch('length', watcher);
hitCount = 0;
var threwTypeError;
try {
    (function(){'use strict'; c.length = 0;})();
    threwTypeError = false;
} catch (x) {
    threwTypeError = x instanceof TypeError;
}
reportCompare(1, hitCount, "strict; non-configurable: watchpoint hit");
reportCompare(1, c.length, "strict; non-configurable: length unchanged");
reportCompare(true, threwTypeError, "strict; non-configurable: TypeError thrown");
