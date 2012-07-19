/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

var gTestfile = 'proxy-__proto__.js';
var BUGNUMBER = 770344;
var summary = "Behavior of __proto__ on proxies";

print(BUGNUMBER + ": " + summary);

/**************
 * BEGIN TEST *
 **************/

var protoDesc = Object.getOwnPropertyDescriptor(Object.prototype, "__proto__");
var protoGetter = protoDesc.get;
var protoSetter = protoDesc.set;

function pp(arr)
{
  return arr.map(function(v) { return "" + v; }).join(", ");
}

function testProxy(creator, args, proto)
{
  print("Now testing behavior for " +
        "Proxy." + creator + "(" + pp(args) + ")");

  var pobj = Proxy[creator].apply(Proxy, args);

  // Check [[Prototype]] before attempted mutation
  assertEq(Object.getPrototypeOf(pobj), proto);
  assertEq(protoGetter.call(pobj), proto);

  // Attempt [[Prototype]] mutation
  try
  {
    protoSetter.call(pobj);
    throw new Error("should throw trying to mutate a proxy's [[Prototype]]");
  }
  catch (e)
  {
    assertEq(e instanceof TypeError, true,
             "expected TypeError, instead got: " + e);
  }

  // Check [[Prototype]] after attempted mutation
  assertEq(Object.getPrototypeOf(pobj), proto);
  assertEq(protoGetter.call(pobj), proto);
}

// Proxy object with non-null [[Prototype]]
var nonNullProto = { toString: function() { return "non-null prototype"; } };
var nonNullHandler = { toString: function() { return "non-null handler"; } };
testProxy("create", [nonNullHandler, nonNullProto], nonNullProto);

// Proxy object with null [[Prototype]]
var nullProto = null;
var nullHandler = { toString: function() { return "null handler"; } };
testProxy("create", [nullHandler, nullProto], nullProto);

// Proxy function with [[Call]]
var callForCallOnly = function () { };
callForCallOnly.toString = function() { return "callForCallOnly"; };
var callOnlyHandler = { toString: function() { return "call-only handler"; } };
testProxy("createFunction",
          [callOnlyHandler, callForCallOnly], Function.prototype);

// Proxy function with [[Call]] and [[Construct]]
var callForCallConstruct = function() { };
callForCallConstruct.toString = function() { return "call/construct call"; };
var constructForCallConstruct = function() { };
constructForCallConstruct.toString =
  function() { return "call/construct construct"; };
var handlerForCallConstruct =
  { toString: function() { return "call/construct handler"; } };
testProxy("createFunction",
          [handlerForCallConstruct,
           callForCallConstruct,
           constructForCallConstruct],
          Function.prototype);


/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
