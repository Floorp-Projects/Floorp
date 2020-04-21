/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
/* eslint-disable no-shadow, max-nested-callbacks */

"use strict";

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);

registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

// Run test_unsafe_grips twice, one against a system principal debuggee
// and another time with a null principal debuggee

// The following tests work like this:
// - The specified code is evaluated in a system principal.
//   `Cu`, `systemPrincipal` and `Services` are provided as global variables.
// - The resulting object is debugged in a system or null principal debuggee,
//   depending on in which list the test is placed.
//   It is tested according to the specified test parameters.
// - An ordinary object that inherits from the resulting one is also debugged.
//   This is just to check that it can be normally debugged even with an unsafe
//   object in the prototype. The specified test parameters do not apply.

// The following tests are defined via properties with the following defaults.
const defaults = {
  // The class of the grip.
  class: "Restricted",

  // The stringification of the object
  string: "",

  // Whether the object (not its grip) has class "Function".
  isFunction: false,

  // Whether the grip has a preview property.
  hasPreview: true,

  // Code that assigns the object to be tested into the obj variable.
  code: "var obj = {}",

  // The type of the grip of the prototype.
  protoType: "null",

  // Whether the object has some own string properties.
  hasOwnPropertyNames: false,

  // Whether the object has some own symbol properties.
  hasOwnPropertySymbols: false,

  // The descriptor obtained when retrieving property "x" or Symbol("x").
  property: undefined,

  // Code evaluated after the test, whose result is expected to be true.
  afterTest: "true == true",
};

// The following tests use a system principal debuggee.
const systemPrincipalTests = [
  {
    // Dead objects throw a TypeError when accessing properties.
    class: "DeadObject",
    string: "<dead object>",
    code: `
    var obj = Cu.Sandbox(null);
    Cu.nukeSandbox(obj);
  `,
    property: descriptor({ value: "TypeError" }),
  },
  {
    // This proxy checks that no trap runs (using a second proxy as the handler
    // there is no need to maintain a list of all possible traps).
    class: "Proxy",
    string: "<proxy>",
    code: `
    var trapDidRun = false;
    var obj = new Proxy({}, new Proxy({}, {get: (_, trap) => {
      trapDidRun = true;
      throw new Error("proxy trap '" + trap + "' was called.");
    }}));
  `,
    afterTest: "trapDidRun === false",
  },
  {
    // Like the previous test, but now the proxy has a Function class.
    class: "Proxy",
    string: "<proxy>",
    isFunction: true,
    code: `
    var trapDidRun = false;
    var obj = new Proxy(function(){}, new Proxy({}, {get: (_, trap) => {
      trapDidRun = true;
      throw new Error("proxy trap '" + trap + "' was called.(function)");
    }}));
  `,
    afterTest: "trapDidRun === false",
  },
  {
    // Invisisible-to-debugger objects can't be unwrapped, so we don't know if
    // they are proxies. Thus they shouldn't be accessed.
    class: "InvisibleToDebugger: Array",
    string: "<invisibleToDebugger>",
    hasPreview: false,
    code: `
    var s = Cu.Sandbox(systemPrincipal, {invisibleToDebugger: true});
    var obj = s.eval("[1, 2, 3]");
  `,
  },
  {
    // Like the previous test, but now the object has a Function class.
    class: "InvisibleToDebugger: Function",
    string: "<invisibleToDebugger>",
    isFunction: true,
    hasPreview: false,
    code: `
    var s = Cu.Sandbox(systemPrincipal, {invisibleToDebugger: true});
    var obj = s.eval("(function func(arg){})");
  `,
  },
  {
    // Cu.Sandbox is a WrappedNative that throws when accessing properties.
    class: "nsXPCComponents_utils_Sandbox",
    string: "[object nsXPCComponents_utils_Sandbox]",
    code: `var obj = Cu.Sandbox;`,
    protoType: "object",
  },
];

// The following tests run code in a system principal, but the resulting object
// is debugged in a null principal.
const nullPrincipalTests = [
  {
    // The null principal gets undefined when attempting to access properties.
    string: "[object Object]",
    code: `var obj = {x: -1};`,
  },
  {
    // For arrays it's an error instead of undefined.
    string: "[object Object]",
    code: `var obj = [1, 2, 3];`,
    property: descriptor({ value: "Error" }),
  },
  {
    // For functions it's also an error.
    string: "function func(arg){}",
    isFunction: true,
    hasPreview: false,
    code: `var obj = function func(arg){};`,
    property: descriptor({ value: "Error" }),
  },
  {
    // Check that no proxy trap runs.
    string: "[object Object]",
    code: `
    var trapDidRun = false;
    var obj = new Proxy([], new Proxy({}, {get: (_, trap) => {
      trapDidRun = true;
      throw new Error("proxy trap '" + trap + "' was called.");
    }}));
  `,
    property: descriptor({ value: "Error" }),
    afterTest: `trapDidRun === false`,
  },
  {
    // Like the previous test, but now the object is a callable Proxy.
    string: "function () {\n    [native code]\n}",
    isFunction: true,
    hasPreview: false,
    code: `
    var trapDidRun = false;
    var obj = new Proxy(function(){}, new Proxy({}, {get: (_, trap) => {
      trapDidRun = true;
      throw new Error("proxy trap '" + trap + "' was called.");
    }}));
  `,
    property: descriptor({ value: "Error" }),
    afterTest: `trapDidRun === false`,
  },
  {
    // Cross-origin Window objects do expose some properties and have a preview.
    string: "[object Object]",
    code: `var obj = Services.appShell.createWindowlessBrowser().document.defaultView;`,
    hasOwnPropertyNames: true,
    hasOwnPropertySymbols: true,
    property: descriptor({ value: "SecurityError" }),
  },
  {
    // Cross-origin Location objects do expose some properties and have a preview.
    string: "[object Object]",
    code: `var obj = Services.appShell.createWindowlessBrowser().document.defaultView
                   .location;`,
    hasOwnPropertyNames: true,
    hasOwnPropertySymbols: true,
    property: descriptor({ value: "SecurityError" }),
  },
];

function descriptor(descr) {
  return Object.assign(
    {
      configurable: false,
      writable: false,
      enumerable: false,
      value: undefined,
    },
    descr
  );
}

async function test_unsafe_grips(
  { threadFront, debuggee, client },
  tests,
  principal
) {
  debuggee.eval(
    function stopMe(arg1, arg2) {
      debugger;
    }.toString()
  );
  for (let data of tests) {
    data = { ...defaults, ...data };

    // Run the code and test the results.
    const sandbox = Cu.Sandbox(systemPrincipal);
    Object.assign(sandbox, { Services, systemPrincipal, Cu });
    sandbox.eval(data.code);
    debuggee.obj = sandbox.obj;
    const inherits = `Object.create(obj, {
        x: {value: 1},
        [Symbol.for("x")]: {value: 2}
      })`;

    const packet = await executeOnNextTickAndWaitForPause(
      () => debuggee.eval(`stopMe(obj, ${inherits});`),
      threadFront
    );

    const [objGrip, inheritsGrip] = packet.frame.arguments;
    for (const grip of [objGrip, inheritsGrip]) {
      const isUnsafe = grip === objGrip;
      // If `isUnsafe` is true, the parameters in `data` will be used to assert
      // against `objGrip`, the grip of the object `obj` created by the test.
      // Otherwise, the grip will refer to `inherits`, an ordinary object which
      // inherits from `obj`. Then all checks are hardcoded because in every test
      // all methods are expected to work the same on `inheritsGrip`.
      check_grip(grip, data, isUnsafe);

      let objClient = threadFront.pauseGrip(grip);
      let response, slice;

      response = await objClient.getPrototypeAndProperties();
      check_properties(response.ownProperties, data, isUnsafe);
      check_symbols(response.ownSymbols, data, isUnsafe);
      check_prototype(response.prototype, data, isUnsafe);

      response = await objClient.enumProperties({
        ignoreIndexedProperties: true,
      });
      slice = await response.slice(0, response.count);
      check_properties(slice.ownProperties, data, isUnsafe);

      response = await objClient.enumProperties({});
      slice = await response.slice(0, response.count);
      check_properties(slice.ownProperties, data, isUnsafe);

      response = await objClient.getOwnPropertyNames();
      check_property_names(response.ownPropertyNames, data, isUnsafe);

      response = await objClient.getProperty("x");
      check_property(response.descriptor, data, isUnsafe);

      response = await objClient.enumSymbols();
      slice = await response.slice(0, response.count);
      check_symbol_names(slice.ownSymbols, data, isUnsafe);

      response = await objClient.getProperty(Symbol.for("x"));
      check_symbol(response.descriptor, data, isUnsafe);

      response = await objClient.getPrototype();
      check_prototype(response.prototype, data, isUnsafe);

      response = await objClient.getDisplayString();
      check_display_string(response.displayString, data, isUnsafe);

      if (data.isFunction && isUnsafe) {
        // For function-related methods, the object front checks that the class
        // of the grip is "Function", and if it's not, the method in object.js
        // is not called. But some tests have a grip with a class that is not
        // "Function" (e.g. it's "Proxy") but the DebuggerObject has a "Function"
        // class because the object is callable (despite not being a Function object).
        // So the grip class is changed in order to test the object.js method.
        grip.class = "Function";
        objClient = threadFront.pauseGrip(grip);
        try {
          response = await objClient.getParameterNames();
          ok(
            true,
            "getParameterNames passed. DebuggerObject.class is 'Function'" +
              "on the object actor"
          );
        } catch (e) {
          ok(
            false,
            "getParameterNames failed. DebuggerObject.class may not be" +
              " 'Function' on the object actor"
          );
        }
      }
      await objClient.release();
    }

    await threadFront.resume();

    ok(sandbox.eval(data.afterTest), "Check after test passes");
  }
}

function check_grip(grip, data, isUnsafe) {
  if (isUnsafe) {
    strictEqual(grip.class, data.class, "The grip has the proper class.");
    strictEqual("preview" in grip, data.hasPreview, "Check preview presence.");
  } else {
    strictEqual(grip.class, "Object", "The grip has 'Object' class.");
    ok("preview" in grip, "The grip has a preview.");
  }
}

function check_properties(props, data, isUnsafe) {
  const propNames = Reflect.ownKeys(props);
  check_property_names(propNames, data, isUnsafe);
  if (isUnsafe) {
    deepEqual(props.x, undefined, "The property does not exist.");
  } else {
    strictEqual(props.x.value, 1, "The property has the right value.");
  }
}

function check_property_names(props, data, isUnsafe) {
  if (isUnsafe) {
    strictEqual(
      props.length > 0,
      data.hasOwnPropertyNames,
      "Check presence of own string properties."
    );
  } else {
    strictEqual(props.length, 1, "1 own property was retrieved.");
    strictEqual(props[0], "x", "The property has the right name.");
  }
}

function check_property(descr, data, isUnsafe) {
  if (isUnsafe) {
    deepEqual(descr, data.property, "Got the right property descriptor.");
  } else {
    strictEqual(descr.value, 1, "The property has the right value.");
  }
}

function check_symbols(symbols, data, isUnsafe) {
  check_symbol_names(symbols, data, isUnsafe);
  if (!isUnsafe) {
    check_symbol(symbols[0].descriptor, data, isUnsafe);
  }
}

function check_symbol_names(props, data, isUnsafe) {
  if (isUnsafe) {
    strictEqual(
      props.length > 0,
      data.hasOwnPropertySymbols,
      "Check presence of own symbol properties."
    );
  } else {
    strictEqual(props.length, 1, "1 own symbol property was retrieved.");
    strictEqual(props[0].name, "Symbol(x)", "The symbol has the right name.");
  }
}

function check_symbol(descr, data, isUnsafe) {
  if (isUnsafe) {
    deepEqual(
      descr,
      data.property,
      "Got the right symbol property descriptor."
    );
  } else {
    strictEqual(descr.value, 2, "The symbol property has the right value.");
  }
}

function check_prototype(proto, data, isUnsafe) {
  const protoGrip = proto && proto.getGrip ? proto.getGrip() : proto;
  if (isUnsafe) {
    deepEqual(protoGrip.type, data.protoType, "Got the right prototype type.");
  } else {
    check_grip(protoGrip, data, true);
  }
}

function check_display_string(str, data, isUnsafe) {
  if (isUnsafe) {
    strictEqual(str, data.string, "The object stringifies correctly.");
  } else {
    strictEqual(str, "[object Object]", "The object stringifies correctly.");
  }
}

// threadFrontTest uses systemPrincipal by default, but let's be explicit here.
add_task(
  threadFrontTest(
    options => {
      return test_unsafe_grips(options, systemPrincipalTests, "system");
    },
    { principal: systemPrincipal }
  )
);

const nullPrincipal = Cc["@mozilla.org/nullprincipal;1"].createInstance(
  Ci.nsIPrincipal
);
add_task(
  threadFrontTest(
    options => {
      return test_unsafe_grips(options, nullPrincipalTests, "null");
    },
    { principal: nullPrincipal }
  )
);
