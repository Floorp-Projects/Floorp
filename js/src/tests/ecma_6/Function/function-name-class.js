var BUGNUMBER = 883377;
var summary = "Anonymous class with name method shouldn't be affected by assignment";

print(BUGNUMBER + ": " + summary);

var classWithStaticNameMethod = class { static name() {} };
assertEq(typeof classWithStaticNameMethod.name, "function");

var classWithStaticNameGetter = class { static get name() { return "static name"; } };
assertEq(typeof Object.getOwnPropertyDescriptor(classWithStaticNameGetter, "name").get, "function");
assertEq(classWithStaticNameGetter.name, "static name");

var classWithStaticNameSetter = class { static set name(v) {} };
assertEq(typeof Object.getOwnPropertyDescriptor(classWithStaticNameSetter, "name").set, "function");

var n = "NAME".toLowerCase();
var classWithStaticNameMethodComputed = class { static [n]() {} };
assertEq(typeof classWithStaticNameMethodComputed.name, "function");

// It doesn't apply for non-static method.

var classWithNameMethod = class { name() {} };
assertEq(classWithNameMethod.name, "classWithNameMethod");

var classWithNameGetter = class { get name() { return "name"; } };
assertEq(classWithNameGetter.name, "classWithNameGetter");

var classWithNameSetter = class { set name(v) {} };
assertEq(classWithNameSetter.name, "classWithNameSetter");

if (typeof reportCompare === "function")
    reportCompare(0, 0);
