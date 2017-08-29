// |jit-test| error: TypeError

var Date_toString = newGlobal().Date.prototype.toString;
(function f(){ f(Date_toString.call({})); })();
