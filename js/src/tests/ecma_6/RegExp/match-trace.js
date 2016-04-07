var BUGNUMBER = 887016;
var summary = "Trace RegExp.prototype[@@match] behavior.";

print(BUGNUMBER + ": " + summary);

var n;
var log;
var target;
var global;
var unicode;
var logProxy;

var execResult;
var lastIndexResult;
var lastIndexExpected;

function P(A) {
  return new Proxy(A, {
    get(that, name) {
      if (logProxy)
        log += "get:result[" + name + "],";
      return that[name];
    }
  });
}

var myRegExp = {
  get global() {
    log += "get:global,";
    return global;
  },
  get lastIndex() {
    log += "get:lastIndex,";
    return lastIndexResult[n];
  },
  set lastIndex(v) {
    log += "set:lastIndex,";
    assertEq(v, lastIndexExpected[n]);
  },
  get unicode() {
    log += "get:unicode,";
    return unicode;
  },
  get exec() {
    log += "get:exec,";
    return function(S) {
      log += "call:exec,";
      assertEq(S, target);
      return execResult[n++];
    };
  },
};

function reset() {
  n = 0;
  log = "";
  target = "abcAbcABC";
  global = true;
  unicode = false;
  logProxy = true;
}

// Trace global with non-empty match.
reset();
execResult        = [    P(["abc"]), P(["ABC"]), null ];
lastIndexResult   = [ ,  ,           ,                ];
lastIndexExpected = [ 0, ,           ,                ];
var ret = RegExp.prototype[Symbol.match].call(myRegExp, target);
assertEq(JSON.stringify(ret), `["abc","ABC"]`);
assertEq(log,
         "get:global," +
         "get:unicode," +
         "set:lastIndex," +
         "get:exec,call:exec,get:result[0]," +
         "get:exec,call:exec,get:result[0]," +
         "get:exec,call:exec,");

// Trace global with empty match.
reset();
execResult        = [    P([""]), P([""]), null ];
lastIndexResult   = [ ,  4,       20,           ];
lastIndexExpected = [ 0, 5,       21,           ];
ret = RegExp.prototype[Symbol.match].call(myRegExp, target);
assertEq(JSON.stringify(ret), `["",""]`);
assertEq(log,
         "get:global," +
         "get:unicode," +
         "set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,");

// Trace global and unicode with empty match.
// 1. not surrogate pair
// 2. lead surrogate pair
// 3. trail surrogate pair
// 4. lead surrogate pair without trail surrogate pair
// 5. index overflow
reset();
unicode = true;
//        0123     4     5678
target = "___\uD83D\uDC38___\uD83D";
execResult        = [    P([""]), P([""]), P([""]), P([""]), P([""]), null ];
lastIndexResult   = [ ,  2,       3,       4,       8,       9,            ];
lastIndexExpected = [ 0, 3,       5,       5,       9,       10,           ];
ret = RegExp.prototype[Symbol.match].call(myRegExp, target);
assertEq(JSON.stringify(ret), `["","","","",""]`);
assertEq(log,
         "get:global," +
         "get:unicode," +
         "set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,get:result[0],get:lastIndex,set:lastIndex," +
         "get:exec,call:exec,");

// Trace global with no match.
reset();
execResult        = [    null ];
lastIndexResult   = [ ,       ];
lastIndexExpected = [ 0,      ];
ret = RegExp.prototype[Symbol.match].call(myRegExp, target);
assertEq(ret, null);
assertEq(log,
         "get:global," +
         "get:unicode," +
         "set:lastIndex," +
         "get:exec,call:exec,");

// Trace non-global.
reset();
global = false;
execResult        = [ P(["abc"]) ];
lastIndexResult   = [];
lastIndexExpected = [];
ret = RegExp.prototype[Symbol.match].call(myRegExp, target);
// ret is the Proxy on non-global case, disable logging.
logProxy = false;
assertEq(JSON.stringify(ret), `["abc"]`);
assertEq(log,
         "get:global," +
         "get:exec,call:exec,");

if (typeof reportCompare === "function")
    reportCompare(true, true);
