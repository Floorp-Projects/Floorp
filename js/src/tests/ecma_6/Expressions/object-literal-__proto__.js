// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

//-----------------------------------------------------------------------------
var BUGNUMBER = 1061853;
var summary =
  "__proto__ in object literals in non-__proto__:v contexts doesn't modify " +
  "[[Prototype]]";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

function hasOwn(obj, prop)
{
  return Object.getOwnPropertyDescriptor(obj, prop) !== undefined;
}

var objectStart = "{ ";
var objectEnd = " }";

var members =
  {
    nullProto: "__proto__: null",
    functionProtoProto: "__proto__: Function.prototype",
    computedNull:  "['__proto__']: null",
    method: "__proto__() {}",
    computedMethod: "['__proto__']() {}",
    generatorMethod: "*__proto__() {}",
    computedGenerator: "*['__proto__']() {}",
    shorthand: "__proto__",
    getter: "get __proto__() { return 42; }",
    getterComputed: "get ['__proto__']() { return 42; }",
    setter: "set __proto__(v) { }",
    setterComputed: "set ['__proto__'](v) { }",
  };

function isProtoMutation(key)
{
  return key === "nullProto" || key === "functionProtoProto";
}

function isGetter(key)
{
  return key === "getter" || key === "getterComputed";
}

function isSetter(key)
{
  return key === "setter" || key === "setterComputed";
}

function isData(key)
{
  return !isProtoMutation(key) && !isGetter(key) && !isSetter(key);
}

var __proto__ = "string value";

function typeOfProto(key)
{
  if (key === "computedNull")
    return "object";
  if (key === "method" || key === "computedMethod" ||
      key === "computedGenerator" || key === "generatorMethod")
  {
    return "function";
  }
  if (key === "getter" || key === "getterComputed")
    return "number";
  assertEq(key, "shorthand", "bug in test!");
  return "string";
}

for (var first in members)
{
  var fcode = "return " + objectStart + members[first] + objectEnd;
  var f = Function(fcode);
  var oneProp = f();

  if (first === "nullProto")
  {
    assertEq(Object.getPrototypeOf(oneProp), null);
    assertEq(hasOwn(oneProp, "__proto__"), false);
  }
  else if (first === "functionProtoProto")
  {
    assertEq(Object.getPrototypeOf(oneProp), Function.prototype);
    assertEq(hasOwn(oneProp, "__proto__"), false);
  }
  else if (isSetter(first))
  {
    assertEq(Object.getPrototypeOf(oneProp), Object.prototype);
    assertEq(hasOwn(oneProp, "__proto__"), true);
    assertEq(typeof Object.getOwnPropertyDescriptor(oneProp, "__proto__").set,
             "function");
  }
  else
  {
    assertEq(Object.getPrototypeOf(oneProp), Object.prototype);
    assertEq(hasOwn(oneProp, "__proto__"), true);
    assertEq(typeof oneProp.__proto__, typeOfProto(first));
  }

  for (var second in members)
  {
    try
    {
      var gcode = "return " + objectStart + members[first] + ", " +
                                            members[second] + objectEnd;
      var g = Function(gcode);
    }
    catch (e)
    {
      assertEq(e instanceof SyntaxError, true,
               "__proto__ member conflicts should be syntax errors, got " + e);
      assertEq(+(first === "nullProto" || first === "functionProtoProto") +
               +(second === "nullProto" || second === "functionProtoProto") > 1,
               true,
               "unexpected conflict between members: " + first + ", " + second);
      continue;
    }

    var twoProps = g();

    if (first === "nullProto" || second === "nullProto")
      assertEq(Object.getPrototypeOf(twoProps), null);
    else if (first === "functionProtoProto" || second === "functionProtoProto")
      assertEq(Object.getPrototypeOf(twoProps), Function.prototype);
    else
      assertEq(Object.getPrototypeOf(twoProps), Object.prototype);

    if (isSetter(second))
    {
      assertEq(hasOwn(twoProps, "__proto__"), true);
      assertEq(typeof Object.getOwnPropertyDescriptor(twoProps, "__proto__").get,
               isGetter(first) ? "function" : "undefined");
    }
    else if (!isProtoMutation(second))
    {
      assertEq(hasOwn(twoProps, "__proto__"), true);
      assertEq(typeof twoProps.__proto__, typeOfProto(second));
      if (isGetter(second))
      {
        assertEq(typeof Object.getOwnPropertyDescriptor(twoProps, "__proto__").get,
                 "function");
        assertEq(typeof Object.getOwnPropertyDescriptor(twoProps, "__proto__").set,
                 isSetter(first) ? "function" : "undefined");
      }
    }
    else if (isSetter(first))
    {
      assertEq(hasOwn(twoProps, "__proto__"), true);
      assertEq(typeof Object.getOwnPropertyDescriptor(twoProps, "__proto__").set,
               "function");
      assertEq(typeof Object.getOwnPropertyDescriptor(twoProps, "__proto__").get,
               "undefined");
    }
    else if (!isProtoMutation(first))
    {
      assertEq(hasOwn(twoProps, "__proto__"), true);
      assertEq(typeof twoProps.__proto__, typeOfProto(first));
    }
    else
    {
      assertEq(true, false, "should be unreachable: " + first + ", " + second);
    }

    for (var third in members)
    {
      try
      {
        var hcode = "return " + objectStart + members[first] + ", " +
                                              members[second] + ", " +
                                              members[third] + objectEnd;
        var h = Function(hcode);
      }
      catch (e)
      {
        assertEq(e instanceof SyntaxError, true,
                 "__proto__ member conflicts should be syntax errors, got " + e);
        assertEq(+(first === "nullProto" || first === "functionProtoProto") +
                 +(second === "nullProto" || second === "functionProtoProto") +
                 +(third === "nullProto" || third === "functionProtoProto") > 1,
                 true,
                 "unexpected conflict among members: " +
                 first + ", " + second + ", " + third);
        continue;
      }

      var threeProps = h();

      if (first === "nullProto" || second === "nullProto" ||
          third === "nullProto")
      {
        assertEq(Object.getPrototypeOf(threeProps), null);
      }
      else if (first === "functionProtoProto" ||
               second === "functionProtoProto" ||
               third === "functionProtoProto")
      {
        assertEq(Object.getPrototypeOf(threeProps), Function.prototype);
      }
      else
      {
        assertEq(Object.getPrototypeOf(threeProps), Object.prototype);
      }

      if (isSetter(third))
      {
        assertEq(hasOwn(threeProps, "__proto__"), true);
        assertEq(typeof Object.getOwnPropertyDescriptor(threeProps, "__proto__").get,
                 isGetter(second) || (!isData(second) && isGetter(first))
                 ? "function"
                 : "undefined",
                 "\n" + hcode);
      }
      else if (!isProtoMutation(third))
      {
        assertEq(hasOwn(threeProps, "__proto__"), true);
        assertEq(typeof threeProps.__proto__, typeOfProto(third), first + ", " + second + ", " +  third);
        if (isGetter(third))
        {
          var desc = Object.getOwnPropertyDescriptor(threeProps, "__proto__");
          assertEq(typeof desc.get, "function");
          assertEq(typeof desc.set,
                   isSetter(second) || (!isData(second) && isSetter(first))
                   ? "function"
                   : "undefined");
        }
      }
      else if (isSetter(second))
      {
        assertEq(hasOwn(threeProps, "__proto__"), true);
        assertEq(typeof Object.getOwnPropertyDescriptor(threeProps, "__proto__").get,
                 isGetter(first) ? "function" : "undefined");
      }
      else if (!isProtoMutation(second))
      {
        assertEq(hasOwn(threeProps, "__proto__"), true);
        assertEq(typeof threeProps.__proto__, typeOfProto(second));
        if (isGetter(second))
        {
          var desc = Object.getOwnPropertyDescriptor(threeProps, "__proto__");
          assertEq(typeof desc.get, "function");
          assertEq(typeof desc.set,
                   isSetter(first) ? "function" : "undefined");
        }
      }
      else
      {
        assertEq(true, false,
                 "should be unreachable: " +
                 first + ", " + second + ", " + third);
      }
    }
  }
}

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
