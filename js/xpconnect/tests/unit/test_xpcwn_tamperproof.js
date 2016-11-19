// Test that it's not possible to create expando properties on XPCWNs.
// See <https://bugzilla.mozilla.org/show_bug.cgi?id=1143810#c5>.

const Cc = Components.classes;
const Ci = Components.interfaces;

function check_throws(f) {
  try {
    f();
  } catch (exc) {
    return;
  }
  throw new TypeError("Expected exception, no exception thrown");
}

/*
 * Test that XPCWrappedNative objects do not permit expando properties to be created.
 *
 * This function is called twice. The first time, realObj is an nsITimer XPCWN
 * and accessObj === realObj.
 *
 * The second time, accessObj is a scripted proxy with realObj as its target.
 * So the second time we are testing that scripted proxies don't magically
 * bypass whatever mechanism enforces the expando policy on XPCWNs.
 */
function test_tamperproof(realObj, accessObj, {method, constant, attribute}) {
  // Assignment can't create an expando property.
  check_throws(function () { accessObj.expando = 14; });
  do_check_false("expando" in realObj);

  // Strict assignment throws.
  check_throws(function () { "use strict"; accessObj.expando = 14; });
  do_check_false("expando" in realObj);

  // Assignment to an inherited method name doesn't work either.
  check_throws(function () { accessObj.hasOwnProperty = () => "lies"; });
  check_throws(function () { "use strict"; accessObj.hasOwnProperty = () => "lies"; });
  do_check_false(realObj.hasOwnProperty("hasOwnProperty"));

  // Assignment to a method name doesn't work either.
  let originalMethod;
  if (method) {
    originalMethod = accessObj[method];
    accessObj[method] = "nope";  // non-writable data property, no exception in non-strict code
    check_throws(function () { "use strict"; accessObj[method] = "nope"; });
    do_check_true(realObj[method] === originalMethod);
  }

  // A constant is the same thing.
  let originalConstantValue;
  if (constant) {
    originalConstantValue = accessObj[constant];
    accessObj[constant] = "nope";
    do_check_eq(realObj[constant], originalConstantValue);
    check_throws(function () { "use strict"; accessObj[constant] = "nope"; });
    do_check_eq(realObj[constant], originalConstantValue);
  }

  // Assignment to a readonly accessor property with no setter doesn't work either.
  let originalAttributeDesc;
  if (attribute) {
    originalAttributeDesc = Object.getOwnPropertyDescriptor(realObj, attribute);
    do_check_true("set" in originalAttributeDesc);
    do_check_true(originalAttributeDesc.set === undefined);

    accessObj[attribute] = "nope";  // accessor property with no setter: no exception in non-strict code
    check_throws(function () { "use strict"; accessObj[attribute] = "nope"; });

    let desc = Object.getOwnPropertyDescriptor(realObj, attribute);
    do_check_true("set" in desc);
    do_check_eq(originalAttributeDesc.get, desc.get);
    do_check_eq(undefined, desc.set);
  }

  // Reflect.set doesn't work either.
  if (method) {
    do_check_false(Reflect.set({}, method, "bad", accessObj));
    do_check_eq(realObj[method], originalMethod);
  }
  if (attribute) {
    do_check_false(Reflect.set({}, attribute, "bad", accessObj));
    do_check_eq(originalAttributeDesc.get, Object.getOwnPropertyDescriptor(realObj, attribute).get);
  }

  // Object.defineProperty can't do anything either.
  let names = ["expando"];
  if (method) names.push(method);
  if (constant) names.push(constant);
  if (attribute) names.push(attribute);
  for (let name of names) {
    let originalDesc = Object.getOwnPropertyDescriptor(realObj, name);
    check_throws(function () {
      Object.defineProperty(accessObj, name, {configurable: true});
    });
    check_throws(function () {
      Object.defineProperty(accessObj, name, {writable: true});
    });
    check_throws(function () {
      Object.defineProperty(accessObj, name, {get: function () { return "lies"; }});
    });
    check_throws(function () {
      Object.defineProperty(accessObj, name, {value: "bad"});
    });
    let desc = Object.getOwnPropertyDescriptor(realObj, name);
    if (originalDesc === undefined) {
      do_check_eq(undefined, desc);
    } else {
      do_check_eq(originalDesc.configurable, desc.configurable);
      do_check_eq(originalDesc.enumerable, desc.enumerable);
      do_check_eq(originalDesc.writable, desc.writable);
      do_check_eq(originalDesc.value, desc.value);
      do_check_eq(originalDesc.get, desc.get);
      do_check_eq(originalDesc.set, desc.set);
    }
  }

  // Existing properties can't be deleted.
  if (method) {
    do_check_false(delete accessObj[method]);
    check_throws(function () { "use strict"; delete accessObj[method]; });
    do_check_eq(realObj[method], originalMethod);
  }
  if (constant) {
    do_check_false(delete accessObj[constant]);
    check_throws(function () { "use strict"; delete accessObj[constant]; });
    do_check_eq(realObj[constant], originalConstantValue);
  }
  if (attribute) {
    do_check_false(delete accessObj[attribute]);
    check_throws(function () { "use strict"; delete accessObj[attribute]; });
    desc = Object.getOwnPropertyDescriptor(realObj, attribute);
    do_check_eq(originalAttributeDesc.get, desc.get);
  }
}

function test_twice(obj, options) {
  test_tamperproof(obj, obj, options);

  let handler = {
    getPrototypeOf(t) {
      return new Proxy(Object.getPrototypeOf(t), handler);
    }
  };
  test_tamperproof(obj, new Proxy(obj, handler), options);
}

function run_test() {
  let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
  test_twice(timer, {
    method: "init",
    constant: "TYPE_ONE_SHOT",
    attribute: "callback"
  });

  let cmdline = Cc["@mozilla.org/toolkit/command-line;1"].createInstance(Ci.nsICommandLine);
  test_twice(cmdline, {});

  test_twice(Object.getPrototypeOf(cmdline), {
    method: "getArgument",
    constant: "STATE_INITIAL_LAUNCH",
    attribute: "length"
  });

  // Test a tearoff object.
  Components.manager.autoRegister(do_get_file('../components/js/xpctest.manifest'));
  let b = Cc["@mozilla.org/js/xpc/test/js/TestInterfaceAll;1"].createInstance(Ci.nsIXPCTestInterfaceB);
  let tearoff = b.nsIXPCTestInterfaceA;
  test_twice(tearoff, {
    method: "QueryInterface"
  });
}
