var BUGNUMBER = 887016;
var summary = "Trace RegExp.prototype[@@search] behavior.";

print(BUGNUMBER + ": " + summary);

var n;
var log;
var target;

var execResult;
var lastIndexResult;
var lastIndexExpected;

function P(index) {
  return new Proxy({ index }, {
    get(that, name) {
      log += "get:result[" + name + "],";
      return that[name];
    }
  });
}

var myRegExp = {
  get lastIndex() {
    log += "get:lastIndex,";
    return lastIndexResult[n];
  },
  set lastIndex(v) {
    log += "set:lastIndex,";
    assertEq(v, lastIndexExpected[n]);
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
}

// Trace hit.
reset();
execResult        = [     P(16) ];
lastIndexResult   = [ 10, ,     ];
lastIndexExpected = [ 0,  10    ];
var ret = RegExp.prototype[Symbol.search].call(myRegExp, target);
assertEq(ret, 16);
assertEq(log,
         "get:lastIndex," +
         "set:lastIndex," +
         "get:exec,call:exec," +
         "set:lastIndex," +
         "get:result[index],");

// Trace not hit.
reset();
execResult        = [     null ];
lastIndexResult   = [ 10, ,    ];
lastIndexExpected = [ 0,  10   ];
ret = RegExp.prototype[Symbol.search].call(myRegExp, target);
assertEq(ret, -1);
assertEq(log,
         "get:lastIndex," +
         "set:lastIndex," +
         "get:exec,call:exec," +
         "set:lastIndex,");

if (typeof reportCompare === "function")
    reportCompare(true, true);
