var BUGNUMBER = 1147817;
var summary = "RegExp constructor should check pattern.constructor.";

print(BUGNUMBER + ": " + summary);

var g = newGlobal();

var re = /foo/;
assertEq(RegExp(re), re);
re.constructor = 10;
assertEq(RegExp(re) === re, false);
assertEq(RegExp(re).toString(), re.toString());

// If pattern comes from different global, RegExp shouldn't return it.
re = g.eval(`var re = /foo/; re;`);
assertEq(RegExp(re) === re, false);
assertEq(RegExp(re).toString(), re.toString());
g.eval(`re.constructor = 10;`);
assertEq(RegExp(re) === re, false);
assertEq(RegExp(re).toString(), re.toString());


re = new Proxy(/a/, {
  get(that, name) {
    return that[name];
  }
});
assertEq(RegExp(re), re);
re = new Proxy(/a/, {
  get(that, name) {
    if (name == "constructor") {
      return function() {};
    }
    return that[name];
  }
});
assertEq(RegExp(re) === re, false);
re = new Proxy(/a/, {
  get(that, name) {
    if (name == Symbol.match) {
      return undefined;
    }
    return that[name];
  }
});
assertEq(RegExp(re) === re, false);

re = new Proxy(g.eval(`/a/`), {
  get(that, name) {
    return that[name];
  }
});
assertEq(RegExp(re) === re, false);

re = g.eval(`new Proxy(/a/, {
  get(that, name) {
    return that[name];
  }
});`);
assertEq(RegExp(re) === re, false);


var obj = { [Symbol.match]: true, source: "foo", flags: "gi" };
assertEq(RegExp(obj) === obj, false);
assertEq(RegExp(obj).toString(), "/foo/gi");
obj.constructor = RegExp;
assertEq(RegExp(obj), obj);

obj = g.eval(`var obj = { [Symbol.match]: true, source: "foo", flags: "gi" }; obj;`);
assertEq(RegExp(obj) === obj, false);
assertEq(RegExp(obj).toString(), "/foo/gi");
g.eval(`obj.constructor = RegExp`);
assertEq(RegExp(obj) === obj, false);
obj.constructor = RegExp;
assertEq(RegExp(obj), obj);

if (typeof reportCompare === "function")
    reportCompare(true, true);
