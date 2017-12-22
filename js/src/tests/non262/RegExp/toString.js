var BUGNUMBER = 1079919;
var summary = "Make RegExp.prototype.toString to be a generic function.";

print(BUGNUMBER + ": " + summary);

assertEq(RegExp.prototype.toString(), "/(?:)/");
assertEq(/foo/.toString(), "/foo/");
assertEq(/foo/i.toString(), "/foo/i");
assertEq(/foo/gimy.toString(), "/foo/gimy");
assertEq(/foo/igym.toString(), "/foo/gimy");
assertEq(/\n\r/.toString(), "/\\n\\r/");
assertEq(/\u2028\u2029/.toString(), "/\\u2028\\u2029/");
assertEq(/\//.toString(), "/\\//");
assertEq(RegExp().toString(), "/(?:)/");
assertEq(RegExp("", "gimy").toString(), "/(?:)/gimy");
assertEq(RegExp("\n\r").toString(), "/\\n\\r/");
assertEq(RegExp("\u2028\u2029").toString(), "/\\u2028\\u2029/");
assertEq(RegExp("/").toString(), "/\\//");

assertThrowsInstanceOf(() => RegExp.prototype.toString.call(), TypeError);
assertThrowsInstanceOf(() => RegExp.prototype.toString.call(1), TypeError);
assertThrowsInstanceOf(() => RegExp.prototype.toString.call(""), TypeError);
assertEq(RegExp.prototype.toString.call({}), "/undefined/undefined");
assertEq(RegExp.prototype.toString.call({ source:"foo", flags:"bar" }), "/foo/bar");

var a = [];
var p = new Proxy({}, {
  get(that, name) {
    a.push(name);
    return {
      toString() {
        a.push(name + "-tostring");
        return name;
      }
    };
  }
});
assertEq(RegExp.prototype.toString.call(p), "/source/flags");
assertEq(a.join(","), "source,source-tostring,flags,flags-tostring");

if (typeof reportCompare === "function")
    reportCompare(true, true);
