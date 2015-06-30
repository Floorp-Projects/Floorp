// |jit-test| error: InternalError

var Date_toString = newGlobal().Date.prototype.toString;
(function f(){ f(Date_toString.call({})); })();
