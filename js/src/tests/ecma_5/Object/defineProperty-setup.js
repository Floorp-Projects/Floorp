// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

assertEq("defineProperty" in Object, true);
assertEq(Object.defineProperty.length, 3);

if (!Object.prototype.toSource)
{
  Object.defineProperty(Object.prototype, "toSource",
  {
    value: function toSource()
    {
      if (this instanceof RegExp)
      {
        var v = "new RegExp(" + uneval(this.source);
        var f = (this.multiline ? "m" : "") +
                (this.global ? "g" : "") +
                (this.ignoreCase ? "i" : "");
        return v + (f ? ", '" + f + "'" : "") + ")";
      }
      return JSON.stringify(this);
    },
    enumerable: false,
    configurable: true,
    writable: true
  });
}
if (!("uneval" in this))
{
  Object.defineProperty(this, "uneval",
  {
    value: function uneval(v)
    {
      if (v === null)
        return "null";
      if (typeof v === "object")
        return v.toSource();
      if (typeof v === "string")
      {
        v = JSON.stringify({v:v});
        return v.substring(5, v.length - 1);
      }
      return "" + v;
    },
    enumerable: false,
    configurable: true,
    writable: true
  });
}

// reimplemented for the benefit of engines which don't have this helper
function assertEq(v1, v2, m)
{
  if (!SameValue(v1, v2))
  {
    throw "assertion failed: " +
          "got " + uneval(v1) + ", expected " + uneval(v2) +
          (m ? ": " + m : "");
  }
}

function SameValue(v1, v2)
{
  if (v1 === 0 && v2 === 0)
    return 1 / v1 === 1 / v2;
  if (v1 !== v1 && v2 !== v2)
    return true;
  return v1 === v2;
}

function PropertyDescriptor(pd)
{
  if (pd)
    this.update(pd);
}
PropertyDescriptor.prototype.update = function update(pd)
{
  if ("get" in pd)
    this.get = pd.get;
  if ("set" in pd)
    this.set = pd.set;
  if ("configurable" in pd)
    this.configurable = pd.configurable;
  if ("writable" in pd)
    this.writable = pd.writable;
  if ("enumerable" in pd)
    this.enumerable = pd.enumerable;
  if ("value" in pd)
    this.value = pd.value;
};
PropertyDescriptor.prototype.convertToDataDescriptor = function convertToDataDescriptor()
{
  delete this.get;
  delete this.set;
  this.writable = false;
  this.value = undefined;
};
PropertyDescriptor.prototype.convertToAccessorDescriptor = function convertToAccessorDescriptor()
{
  delete this.writable;
  delete this.value;
  this.get = undefined;
  this.set = undefined;
};

function compareDescriptors(d1, d2)
{
  if (d1 === undefined)
  {
    assertEq(d2, undefined, "non-descriptors");
    return;
  }
  if (d2 === undefined)
  {
    assertEq(true, false, "descriptor-equality mismatch: " + uneval(d1) + ", " + uneval(d2));
    return;
  }

  var props = ["value", "get", "set", "enumerable", "configurable", "writable"];
  for (var i = 0, sz = props.length; i < sz; i++)
  {
    var p = props[i];
    assertEq(p in d1, p in d2, p + " different in d1/d2");
    if (p in d1)
      assertEq(d1[p], d2[p], p);
  }
}

function examine(desc, field, allowDefault)
{
  if (field in desc)
    return desc[field];
  assertEq(allowDefault, true, "reimplementation error");
  switch (field)
  {
    case "value":
    case "get":
    case "set":
      return undefined;
    case "writable":
    case "enumerable":
    case "configurable":
      return false;
    default:
      assertEq(true, false, "bad field name: " + field);
  }
}

function IsAccessorDescriptor(desc)
{
  if (!desc)
    return false;
  if (!("get" in desc) && !("set" in desc))
    return false;
  return true;
}

function IsDataDescriptor(desc)
{
  if (!desc)
    return false;
  if (!("value" in desc) && !("writable" in desc))
    return false;
  return true;
}

function IsGenericDescriptor(desc)
{
  if (!desc)
    return false;
  if (!IsAccessorDescriptor(desc) && !IsDataDescriptor(desc))
    return true;
  return false;
}



function CustomObject()
{
  this.properties = {};
  this.extensible = true;
}
CustomObject.prototype =
{
  _reject: function _reject(throwing, msg)
  {
    if (throwing)
      throw new TypeError(msg + "; rejected!");
    return false;
  },
  defineOwnProperty: function defineOwnProperty(propname, desc, throwing)
  {
    assertEq(typeof propname, "string", "non-string propname");

    // Step 1.
    var current = this.properties[propname];

    // Step 2.
    var extensible = this.extensible;

    // Step 3.
    if (current === undefined && !extensible)
      return this._reject(throwing, "object not extensible");

    // Step 4.
    if (current === undefined && extensible)
    {
      var p;
      // Step 4(a).
      if (IsGenericDescriptor(desc) || IsDataDescriptor(desc))
      {
        p = new PropertyDescriptor();
        p.value = examine(desc, "value", true);
        p.writable = examine(desc, "writable", true);
        p.enumerable = examine(desc, "enumerable", true);
        p.configurable = examine(desc, "configurable", true);
      }
      // Step 4(b).
      else
      {
        p = new PropertyDescriptor();
        p.get = examine(desc, "get", true);
        p.set = examine(desc, "set", true);
        p.enumerable = examine(desc, "enumerable", true);
        p.configurable = examine(desc, "configurable", true);
      }

      this.properties[propname] = p;

      // Step 4(c).
      return true;
    }

    // Step 5.
    if (!("value" in desc) && !("get" in desc) && !("set" in desc) &&
        !("writable" in desc) && !("enumerable" in desc) &&
        !("configurable" in desc))
    {
      return;
    }

    // Step 6.
    do
    {
      if ("value" in desc)
      {
        if (!("value" in current) || !SameValue(desc.value, current.value))
          break;
      }
      if ("get" in desc)
      {
        if (!("get" in current) || !SameValue(desc.get, current.get))
          break;
      }
      if ("set" in desc)
      {
        if (!("set" in current) || !SameValue(desc.set, current.set))
          break;
      }
      if ("writable" in desc)
      {
        if (!("writable" in current) ||
            !SameValue(desc.writable, current.writable))
        {
          break;
        }
      }
      if ("enumerable" in desc)
      {
        if (!("enumerable" in current) ||
            !SameValue(desc.enumerable, current.enumerable))
        {
          break;
        }
      }
      if ("configurable" in desc)
      {
        if (!("configurable" in current) ||
            !SameValue(desc.configurable, current.configurable))
        {
          break;
        }
      }

      // all fields in desc also in current, with the same values
      return true;
    }
    while (false);

    // Step 7.
    if (!examine(current, "configurable"))
    {
      if ("configurable" in desc && examine(desc, "configurable"))
        return this._reject(throwing, "can't make configurable again");
      if ("enumerable" in desc &&
          examine(current, "enumerable") !== examine(desc, "enumerable"))
      {
        return this._reject(throwing, "can't change enumerability");
      }
    }

    // Step 8.
    if (IsGenericDescriptor(desc))
    {
      // do nothing
    }
    // Step 9.
    else if (IsDataDescriptor(current) !== IsDataDescriptor(desc))
    {
      // Step 9(a).
      if (!examine(current, "configurable"))
        return this._reject(throwing, "can't change unconfigurable descriptor's type");
      // Step 9(b).
      if (IsDataDescriptor(current))
        current.convertToAccessorDescriptor();
      // Step 9(c).
      else
        current.convertToDataDescriptor();
    }
    // Step 10.
    else if (IsDataDescriptor(current) && IsDataDescriptor(desc))
    {
      // Step 10(a)
      if (!examine(current, "configurable"))
      {
        // Step 10(a).i.
        if (!examine(current, "writable") &&
            "writable" in desc && examine(desc, "writable"))
        {
          return this._reject(throwing, "can't make data property writable again");
        }
        // Step 10(a).ii.
        if (!examine(current, "writable"))
        {
          if ("value" in desc &&
              !SameValue(examine(desc, "value"), examine(current, "value")))
          {
            return this._reject(throwing, "can't change value if not writable");
          }
        }
      }
      // Step 10(b).
      else
      {
        assertEq(examine(current, "configurable"), true,
                 "spec bug step 10(b)");
      }
    }
    // Step 11.
    else
    {
      assertEq(IsAccessorDescriptor(current) && IsAccessorDescriptor(desc),
               true,
               "spec bug");

      // Step 11(a).
      if (!examine(current, "configurable"))
      {
        // Step 11(a).i.
        if ("set" in desc &&
            !SameValue(examine(desc, "set"), examine(current, "set")))
        {
          return this._reject(throwing, "can't change setter if not configurable");
        }
        // Step 11(a).ii.
        if ("get" in desc &&
            !SameValue(examine(desc, "get"), examine(current, "get")))
        {
          return this._reject(throwing, "can't change getter if not configurable");
        }
      }
    }

    // Step 12.
    current.update(desc);

    // Step 13.
    return true;
  }
};

function IsCallable(v)
{
  return typeof v === "undefined" || typeof v === "function";
}

var NativeTest =
  {
    newObject: function newObject()
    {
      return {};
    },
    defineProperty: function defineProperty(obj, propname, propdesc)
    {
      Object.defineProperty(obj, propname, propdesc);
    },
    getDescriptor: function getDescriptor(obj, propname)
    {
      return Object.getOwnPropertyDescriptor(obj, propname);
    }
  };

var ReimplTest =
  {
    newObject: function newObject()
    {
      return new CustomObject();
    },
    defineProperty: function defineProperty(obj, propname, propdesc)
    {
      assertEq(obj instanceof CustomObject, true, "obj not instanceof CustomObject");
      if ("get" in propdesc || "set" in propdesc)
      {
        if ("value" in propdesc || "writable" in propdesc)
          throw new TypeError("get/set and value/writable");
        if (!IsCallable(propdesc.get))
          throw new TypeError("get defined, uncallable");
        if (!IsCallable(propdesc.set))
          throw new TypeError("set defined, uncallable");
      }
      return obj.defineOwnProperty(propname, propdesc, true);
    },
    getDescriptor: function getDescriptor(obj, propname)
    {
      if (!(propname in obj.properties))
        return undefined;

      return new PropertyDescriptor(obj.properties[propname]);
    }
  };

var JSVAL_INT_MAX = Math.pow(2, 30) - 1;
var JSVAL_INT_MIN = -Math.pow(2, 30);


function isValidDescriptor(propdesc)
{
  if ("get" in propdesc || "set" in propdesc)
  {
    if ("value" in propdesc || "writable" in propdesc)
      return false;

    // We permit null here simply because this test's author believes the
    // implementation may sometime be susceptible to making mistakes in this
    // regard and would prefer to be cautious.
    if (propdesc.get !== null && propdesc.get !== undefined && !IsCallable(propdesc.get))
      return false;
    if (propdesc.set !== null && propdesc.set !== undefined && !IsCallable(propdesc.set))
      return false;
  }

  return true;
}


var OMIT = {};
var VALUES =
  [-Infinity, JSVAL_INT_MIN, -0, +0, 1.5, JSVAL_INT_MAX, Infinity,
   NaN, "foo", "bar", null, undefined, true, false, {}, /a/, OMIT];
var GETS =
  [undefined, function get1() { return 1; }, function get2() { return 2; },
   null, 5, OMIT];
var SETS =
  [undefined, function set1() { return 1; }, function set2() { return 2; },
   null, 5, OMIT];
var ENUMERABLES = [true, false, OMIT];
var CONFIGURABLES = [true, false, OMIT];
var WRITABLES = [true, false, OMIT];

function mapTestDescriptors(filter)
{
  var descs = [];
  var desc = {};

  function put(field, value)
  {
    if (value !== OMIT)
      desc[field] = value;
  }

  VALUES.forEach(function(value)
  {
    GETS.forEach(function(get)
    {
      SETS.forEach(function(set)
      {
        ENUMERABLES.forEach(function(enumerable)
        {
          CONFIGURABLES.forEach(function(configurable)
          {
            WRITABLES.forEach(function(writable)
            {
              desc = {};
              put("value", value);
              put("get", get);
              put("set", set);
              put("enumerable", enumerable);
              put("configurable", configurable);
              put("writable", writable);
              if (filter(desc))
                descs.push(desc);
            });
          });
        });
      });
    });
  });

  return descs;
}

var ALL_DESCRIPTORS = mapTestDescriptors(function(d) { return true; });
var VALID_DESCRIPTORS = mapTestDescriptors(isValidDescriptor);

var SKIP_FULL_FUNCTION_LENGTH_TESTS = true;

function TestRunner()
{
  this._logLines = [];
}
TestRunner.prototype =
  {
    // MAIN METHODS

    runFunctionLengthTests: function runFunctionLengthTests()
    {
      var self = this;
      function functionLengthTests()
      {
        if (SKIP_FULL_FUNCTION_LENGTH_TESTS)
        {
          print("Skipping full tests for redefining Function.length for now " +
                "because we don't support redefinition of properties with " +
                "native getter or setter...");
          self._simpleFunctionLengthTests();
        }
        else
        {
          self._simpleFunctionLengthTests();
          self._fullFunctionLengthTests(function() { }, 0);
          self._fullFunctionLengthTests(function(one) { }, 1);
          self._fullFunctionLengthTests(function(one, two) { }, 2);
        }
      }

      this._runTestSet(functionLengthTests, "Function length tests completed!");
    },

    runNotPresentTests: function runNotPresentTests()
    {
      var self = this;
      function notPresentTests()
      {
        print("Running not-present tests now...");

        for (var i = 0, sz = ALL_DESCRIPTORS.length; i < sz; i++)
          self._runSingleNotPresentTest(ALL_DESCRIPTORS[i]);
      };

      this._runTestSet(notPresentTests, "Not-present length tests completed!");
    },

    runPropertyPresentTestsFraction:
    function runPropertyPresentTestsFraction(part, parts)
    {
      var self = this;
      function propertyPresentTests()
      {
        print("Running already-present tests now...");

        var total = VALID_DESCRIPTORS.length;
        var start = Math.floor((part - 1) / parts * total);
        var end = Math.floor(part / parts * total);

        for (var i = start; i < end; i++)
        {
          var old = VALID_DESCRIPTORS[i];
          print("Starting test with old descriptor " + old.toSource() + "...");

          for (var j = 0, sz2 = VALID_DESCRIPTORS.length; j < sz2; j++)
            self._runSinglePropertyPresentTest(old, VALID_DESCRIPTORS[j], []);
        }
      }

      this._runTestSet(propertyPresentTests,
                       "Property-present fraction " + part + " of " + parts +
                       " completed!");
    },

    runNonTerminalPropertyPresentTestsFraction:
    function runNonTerminalPropertyPresentTestsFraction(part, parts)
    {
      var self = this;

      /*
       * A plain old property to define on the object before redefining the
       * originally-added property, to test redefinition of a property that's
       * not also lastProperty.  NB: we could loop over every possible
       * descriptor here if we wanted, even try adding more than one, but we'd
       * hit cubic complexity and worse, and SpiderMonkey only distinguishes by
       * the mere presence of the middle property, not its precise details.
       */
      var middleDefines =
        [{
           property: "middle",
           descriptor:
             { value: 17, writable: true, configurable: true, enumerable: true }
         }];

      function nonTerminalPropertyPresentTests()
      {
        print("Running non-terminal already-present tests now...");

        var total = VALID_DESCRIPTORS.length;
        var start = Math.floor((part - 1) / parts * total);
        var end = Math.floor(part / parts * total);

        for (var i = start; i < end; i++)
        {
          var old = VALID_DESCRIPTORS[i];
          print("Starting test with old descriptor " + old.toSource() + "...");

          for (var j = 0, sz2 = VALID_DESCRIPTORS.length; j < sz2; j++)
          {
            self._runSinglePropertyPresentTest(old, VALID_DESCRIPTORS[j],
                                               middleDefines);
          }
        }
      }

      this._runTestSet(nonTerminalPropertyPresentTests,
                       "Non-terminal property-present fraction " +
                       part + " of " + parts + " completed!");
    },

    runDictionaryPropertyPresentTestsFraction:
    function runDictionaryPropertyPresentTestsFraction(part, parts)
    {
      var self = this;

      /*
       * Add and readd properties such that the scope for the object is in
       * dictionary mode.
       */
      var middleDefines =
        [
         {
           property: "mid1",
           descriptor:
             { value: 17, writable: true, configurable: true, enumerable: true }
         },
         {
           property: "mid2",
           descriptor:
             { value: 17, writable: true, configurable: true, enumerable: true }
         },
         {
           property: "mid1",
           descriptor:
             { get: function g() { }, set: function s(v){}, configurable: false,
               enumerable: true }
         },
         ];

      function dictionaryPropertyPresentTests()
      {
        print("Running dictionary already-present tests now...");

        var total = VALID_DESCRIPTORS.length;
        var start = Math.floor((part - 1) / parts * total);
        var end = Math.floor(part / parts * total);

        for (var i = start; i < end; i++)
        {
          var old = VALID_DESCRIPTORS[i];
          print("Starting test with old descriptor " + old.toSource() + "...");

          for (var j = 0, sz2 = VALID_DESCRIPTORS.length; j < sz2; j++)
          {
            self._runSinglePropertyPresentTest(old, VALID_DESCRIPTORS[j],
                                               middleDefines);
          }
        }
      }

      this._runTestSet(dictionaryPropertyPresentTests,
                       "Dictionary property-present fraction " +
                       part + " of " + parts + " completed!");
    },


    // HELPERS

    runPropertyPresentTests: function runPropertyPresentTests()
    {
      print("Running already-present tests now...");

      for (var i = 0, sz = VALID_DESCRIPTORS.length; i < sz; i++)
      {
        var old = VALID_DESCRIPTORS[i];
        print("Starting test with old descriptor " + old.toSource() + "...");

        for (var j = 0, sz2 = VALID_DESCRIPTORS.length; j < sz2; j++)
          this._runSinglePropertyPresentTest(old, VALID_DESCRIPTORS[j], []);
      }
    },
    _runTestSet: function _runTestSet(fun, completeMessage)
    {
      try
      {
        fun();

        print(completeMessage);
      }
      catch (e)
      {
        print("ERROR, EXITING (line " + (e.lineNumber || -1) + "): " + e);
        throw e;
      }
      finally
      {
        this._reportAllErrors();
      }
    },
    _reportAllErrors: function _reportAllErrors()
    {
      var errorCount = this._logLines.length;
      print("Full accumulated number of errors: " + errorCount);
      if (errorCount > 0)
        throw errorCount + " errors detected, FAIL";
    },
    _simpleFunctionLengthTests: function _simpleFunctionLengthTests(fun)
    {
      print("Running simple Function.length tests now..");

      function expectThrowTypeError(o, p, desc)
      {
        var err = "<none>", passed = false;
        try
        {
          Object.defineProperty(o, p, desc);
        }
        catch (e)
        {
          err = e;
          passed = e instanceof TypeError;
        }
        assertEq(passed, true, fun + " didn't throw TypeError when called: " + err);
      }

      expectThrowTypeError(function a() { }, "length", { value: 1 });
      expectThrowTypeError(function a() { }, "length", { enumerable: true });
      expectThrowTypeError(function a() { }, "length", { configurable: true });
      expectThrowTypeError(function a() { }, "length", { writable: true });
    },
    _fullFunctionLengthTests: function _fullFunctionLengthTests(fun)
    {
      var len = fun.length;
      print("Running Function.length (" + len + ") tests now...");

      var desc;
      var gen = new DescriptorState();
      while ((desc = gen.nextDescriptor()))
        this._runSingleFunctionLengthTest(fun, len, desc);
    },
    _log: function _log(v)
    {
      var m = "" + v;
      print(m);
      this._logLines.push(m);
    },
    _runSingleNotPresentTest: function _runSingleNotPresentTest(desc)
    {
      var nativeObj = NativeTest.newObject();
      var reimplObj = ReimplTest.newObject();

      try
      {
        NativeTest.defineProperty(nativeObj, "foo", desc);
      }
      catch (e)
      {
        try
        {
          ReimplTest.defineProperty(reimplObj, "foo", desc);
        }
        catch (e2)
        {
          if (e.constructor !== e2.constructor)
          {
            this._log("Difference when comparing native/reimplementation " +
                      "behavior for new descriptor " + desc.toSource() +
                      ", native threw " + e + ", reimpl threw " + e2);
          }
          return;
        }
        this._log("Difference when comparing native/reimplementation " +
                  "behavior for new descriptor " + desc.toSource() +
                  ", error " + e);
        return;
      }

      try
      {
        ReimplTest.defineProperty(reimplObj, "foo", desc);
      }
      catch (e)
      {
        this._log("Reimpl threw defining new descriptor " + desc.toSource() +
                  ", error: " + e);
        return;
      }

      var nativeDesc = NativeTest.getDescriptor(nativeObj, "foo");
      var reimplDesc = ReimplTest.getDescriptor(reimplObj, "foo");
      try
      {
        compareDescriptors(nativeDesc, reimplDesc);
      }
      catch (e)
      {
        this._log("Difference comparing returned descriptors for new " +
                  "property defined with descriptor " + desc.toSource() +
                  "; error: " + e);
        return;
      }
    },
    _runSinglePropertyPresentTest:
    function _runSinglePropertyPresentTest(old, add, middleDefines)
    {
      var nativeObj = NativeTest.newObject();
      var reimplObj = ReimplTest.newObject();

      try
      {
        NativeTest.defineProperty(nativeObj, "foo", old);
      }
      catch (e)
      {
        if (!SameValue(NativeTest.getDescriptor(nativeObj, "foo"), undefined))
        {
          this._log("defining bad property descriptor: " + old.toSource());
          return;
        }

        try
        {
          ReimplTest.defineProperty(reimplObj, "foo", old);
        }
        catch (e2)
        {
          if (!SameValue(ReimplTest.getDescriptor(reimplObj, "foo"),
                         undefined))
          {
            this._log("defining bad property descriptor: " + old.toSource() +
                      "; reimplObj: " + uneval(reimplObj));
          }

          if (e.constructor !== e2.constructor)
          {
            this._log("Different errors defining bad property descriptor: " +
                      old.toSource() + "; native threw " + e + ", reimpl " +
                      "threw " + e2);
          }

          return;
        }

        this._log("Difference defining a property with descriptor " +
                  old.toSource() + ", error " + e);
        return;
      }

      try
      {
        ReimplTest.defineProperty(reimplObj, "foo", old);
      }
      catch (e)
      {
        this._log("Difference when comparing native/reimplementation " +
                  "behavior when adding descriptor " + add.toSource() +
                  ", error: " + e);
        return;
      }

      // Now add (or even readd) however many properties were specified between
      // the original property to add and the new one, to test redefining
      // non-last-properties and properties in scopes in dictionary mode.
      for (var i = 0, sz = middleDefines.length; i < sz; i++)
      {
        var middle = middleDefines[i];
        var prop = middle.property;
        var desc = middle.descriptor;

        try
        {
          NativeTest.defineProperty(nativeObj, prop, desc);
          ReimplTest.defineProperty(reimplObj, prop, desc);
        }
        catch (e)
        {
          this._log("failure defining middle descriptor: " + desc.toSource() +
                    ", error " + e);
          return;
        }

        // Sanity check
        var nativeDesc = NativeTest.getDescriptor(nativeObj, prop);
        var reimplDesc = ReimplTest.getDescriptor(reimplObj, prop);

        compareDescriptors(nativeDesc, reimplDesc);
        compareDescriptors(nativeDesc, desc);
      }

      try
      {
        NativeTest.defineProperty(nativeObj, "foo", add);
      }
      catch (e)
      {
        try
        {
          ReimplTest.defineProperty(reimplObj, "foo", add);
        }
        catch (e2)
        {
          if (e.constructor !== e2.constructor)
          {
            this._log("Difference when comparing native/reimplementation " +
                      "behavior for descriptor " + add.toSource() +
                      " overwriting descriptor " + old.toSource() + "; " +
                      "native threw " + e + ", reimpl threw " + e2);
          }
          return;
        }
        this._log("Difference when comparing native/reimplementation " +
                  "behavior for added descriptor " + add.toSource() + ", " +
                  "initial was " + old.toSource() + "; error: " + e);
        return;
      }

      try
      {
        ReimplTest.defineProperty(reimplObj, "foo", add);
      }
      catch (e)
      {
        this._log("Difference when comparing native/reimplementation " +
                  "behavior for readded descriptor " + add.toSource() + ", " +
                  "initial was " + old.toSource() + "; native readd didn't " +
                  "throw, reimpl add did, error: " + e);
        return;
      }

      var nativeDesc = NativeTest.getDescriptor(nativeObj, "foo");
      var reimplDesc = ReimplTest.getDescriptor(reimplObj, "foo");
      try
      {
        compareDescriptors(nativeDesc, reimplDesc);
      }
      catch (e)
      {
        this._log("Difference comparing returned descriptors for readded " +
                  "property defined with descriptor " + add.toSource() + "; " +
                  "initial was " + old.toSource() + "; error: " + e);
        return;
      }
    },
    _runSingleFunctionLengthTest: function _runSingleFunctionLengthTest(fun, len, desc)
    {
      var nativeObj = fun;
      var reimplObj = ReimplTest.newObject();
      ReimplTest.defineProperty(reimplObj, "length",
      {
        value: len,
        enumerable: false,
        configurable: false,
        writable: false
      });

      try
      {
        NativeTest.defineProperty(nativeObj, "length", desc);
      }
      catch (e)
      {
        try
        {
          ReimplTest.defineProperty(reimplObj, "length", desc);
        }
        catch (e2)
        {
          if (e.constructor !== e2.constructor)
          {
            this._log("Difference when comparing native/reimplementation " +
                      "behavior defining fun.length with " + desc.toSource() +
                      "; native threw " + e + ", reimpl threw " + e2);
          }
          return;
        }
        this._log("Difference when comparing Function.length native/reimpl " +
                  "behavior for descriptor " + desc.toSource() +
                  ", native impl threw error " + e);
        return;
      }

      try
      {
        ReimplTest.defineProperty(reimplObj, "length", desc);
      }
      catch (e)
      {
        this._log("Difference defining new Function.length descriptor: impl " +
                  "succeeded, reimpl threw for descriptor " +
                  desc.toSource() + ", error: " + e);
        return;
      }

      var nativeDesc = NativeTest.getDescriptor(nativeObj, "length");
      var reimplDesc = ReimplTest.getDescriptor(reimplObj, "length");
      try
      {
        compareDescriptors(nativeDesc, reimplDesc);
      }
      catch (e)
      {
        this._log("Difference comparing returned descriptors for " +
                  "Function.length with descriptor " + desc.toSource() +
                  "; error: " + e);
        return;
      }
    }
  };

function runDictionaryPropertyPresentTestsFraction(PART, PARTS)
{
  var testfile =
    '15.2.3.6-dictionary-redefinition-' + PART + '-of-' + PARTS + '.js';
  var BUGNUMBER = 560566;
  var summary =
    'ES5 Object.defineProperty(O, P, Attributes): dictionary redefinition ' +
    PART + ' of ' + PARTS;

  print(BUGNUMBER + ": " + summary);

  try
  {
    new TestRunner().runDictionaryPropertyPresentTestsFraction(PART, PARTS);
  }
  catch (e)
  {
    throw "Error thrown during testing: " + e +
            " at line " + e.lineNumber + "\n" +
          (e.stack
            ? "Stack: " + e.stack.split("\n").slice(2).join("\n") + "\n"
            : "");
  }

  if (typeof reportCompare === "function")
    reportCompare(true, true);

  print("Tests complete!");
}

function runNonTerminalPropertyPresentTestsFraction(PART, PARTS)
{
  var BUGNUMBER = 560566;
  var summary =
    'ES5 Object.defineProperty(O, P, Attributes): middle redefinition ' +
    PART + ' of ' + PARTS;

  print(BUGNUMBER + ": " + summary);


  /**************
   * BEGIN TEST *
   **************/

  try
  {
    new TestRunner().runNonTerminalPropertyPresentTestsFraction(PART, PARTS);
  }
  catch (e)
  {
    throw "Error thrown during testing: " + e +
            " at line " + e.lineNumber + "\n" +
          (e.stack
            ? "Stack: " + e.stack.split("\n").slice(2).join("\n") + "\n"
            : "");
  }

  /******************************************************************************/

  if (typeof reportCompare === "function")
    reportCompare(true, true);

  print("Tests complete!");
}
