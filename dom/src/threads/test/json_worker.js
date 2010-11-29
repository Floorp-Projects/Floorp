var cyclicalObject = {};
cyclicalObject.foo = cyclicalObject;

var cyclicalArray = [];
cyclicalArray.push(cyclicalArray);

function makeCrazyNested(obj, count) {
  var innermostobj;
  for (var i = 0; i < count; i++) {
    obj.foo = { bar: 5 }
    innermostobj = obj.foo;
    obj = innermostobj;
  }
  return innermostobj;
}

var crazyNestedObject = {};
makeCrazyNested(crazyNestedObject, 100);

var crazyCyclicalObject = {};
var innermost = makeCrazyNested(crazyCyclicalObject, 1000);
innermost.baz = crazyCyclicalObject;

var objectWithSaneGetter = { };
objectWithSaneGetter.__defineGetter__("foo", function() { return 5; });

// We don't walk prototype chains for cloning so this won't actually do much...
function objectWithSaneGetter2() { }
objectWithSaneGetter2.prototype = {
  get foo() {
    return 5;
  }
};

var objectWithThrowingGetter = { };
objectWithThrowingGetter.__defineGetter__("foo", function() { throw "bad"; });

var typedArrayWithValues = new Int8Array(5);
for (var index in typedArrayWithValues) {
  typedArrayWithValues[index] = index;
}

var typedArrayWithFunBuffer = new Int8Array(4);
for (var index in typedArrayWithFunBuffer) {
  typedArrayWithFunBuffer[index] = 255;
}

var typedArrayWithFunBuffer2 = new Int32Array(typedArrayWithFunBuffer.buffer);

var xhr = new XMLHttpRequest();

var messages = [
  {
    type: "object",
    value: { },
    jsonValue: '{}'
  },
  {
    type: "object",
    value: {foo: "bar"},
    jsonValue: '{"foo":"bar"}'
  },
  {
    type: "object",
    value: {foo: "bar", foo2: {bee: "bop"}},
    jsonValue: '{"foo":"bar","foo2":{"bee":"bop"}}'
  },
  {
    type: "object",
    value: {foo: "bar", foo2: {bee: "bop"}, foo3: "baz"},
    jsonValue: '{"foo":"bar","foo2":{"bee":"bop"},"foo3":"baz"}'
  },
  {
    type: "object",
    value: {foo: "bar", foo2: [1,2,3]},
    jsonValue: '{"foo":"bar","foo2":[1,2,3]}'
  },
  {
    type: "object",
    value: cyclicalObject,
    exception: true
  },
  {
    type: "object",
    value: [null, 2, false, cyclicalObject],
    exception: true
  },
  {
    type: "object",
    value: cyclicalArray,
    exception: true
  },
  {
    type: "object",
    value: {foo: 1, bar: cyclicalArray},
    exception: true
  },
  {
    type: "object",
    value: crazyNestedObject,
    jsonValue: JSON.stringify(crazyNestedObject)
  },
  {
    type: "object",
    value: crazyCyclicalObject,
    exception: true
  },
  {
    type: "object",
    value: objectWithSaneGetter,
    jsonValue: '{"foo":5}'
  },
  {
    type: "object",
    value: new objectWithSaneGetter2(),
    jsonValue: '{}'
  },
  {
    type: "object",
    value: objectWithThrowingGetter,
    exception: true
  },
  {
    type: "object",
    array: true,
    value: [9, 8, 7],
    jsonValue: '[9,8,7]'
  },
  {
    type: "object",
    array: true,
    value: [9, false, 10.5, {foo: "bar"}],
    jsonValue: '[9,false,10.5,{"foo":"bar"}]'
  },
  {
    type: "object",
    shouldEqual: true,
    value: null
  },
  {
    type: "undefined",
    shouldEqual: true,
    value: undefined
  },
  {
    type: "string",
    shouldEqual: true,
    value: "Hello"
  },
  {
    type: "string",
    shouldEqual: true,
    value: JSON.stringify({ foo: "bar" }),
    compareValue: '{"foo":"bar"}'
  },
  {
    type: "number",
    shouldEqual: true,
    value: 1
  },
  {
    type: "number",
    shouldEqual: true,
    value: 0
  },
  {
    type: "number",
    shouldEqual: true,
    value: -1
  },
  {
    type: "number",
    shouldEqual: true,
    value: 238573459843702923492399923049
  },
  {
    type: "number",
    shouldEqual: true,
    value: -238573459843702923492399923049
  },
  {
    type: "number",
    shouldEqual: true,
    value: 0.25
  },
  {
    type: "number",
    shouldEqual: true,
    value: -0.25
  },
  {
    type: "boolean",
    shouldEqual: true,
    value: true
  },
  {
    type: "boolean",
    shouldEqual: true,
    value: false
  },
  {
    type: "object",
    value: function (foo) { return "Bad!"; },
    exception: true
  },
  {
    type: "number",
    isNaN: true,
    value: NaN
  },
  {
    type: "number",
    isInfinity: true,
    value: Infinity
  },
  {
    type: "number",
    isNegativeInfinity: true,
    value: -Infinity
  },
  {
    type: "object",
    value: new Int32Array(10),
    jsonValue: '{"0":0,"1":0,"2":0,"3":0,"4":0,"5":0,"6":0,"7":0,"8":0,"9":0}'
  },
  {
    type: "object",
    value: new Float32Array(5),
    jsonValue: '{"0":0,"1":0,"2":0,"3":0,"4":0}'
  },
  {
    type: "object",
    value: typedArrayWithValues,
    jsonValue: '{"0":0,"1":1,"2":2,"3":3,"4":4}'
  },
  {
    type: "number",
    value: typedArrayWithValues[2],
    compareValue: 2,
    shouldEqual: true
  },
  {
    type: "object",
    value: typedArrayWithValues.buffer,
    jsonValue: '{}'
  },
  {
    type: "object",
    value: typedArrayWithFunBuffer2,
    jsonValue: '{"0":-1}'
  },
  {
    type: "object",
    value: { foo: typedArrayWithFunBuffer2 },
    jsonValue: '{"foo":{"0":-1}}'
  },
  {
    type: "object",
    value: [ typedArrayWithFunBuffer2 ],
    jsonValue: '[{"0":-1}]'
  },
  {
    type: "object",
    value: { foo: function(a) { alert(b); } },
    exception: true
  },
  {
    type: "object",
    value: xhr,
    exception: true
  },
  {
    type: "number",
    value: xhr.readyState,
    shouldEqual: true
  },
  {
    type: "object",
    value: { xhr: xhr },
    exception: true
  },
  {
    type: "object",
    value: self,
    exception: true
  },
  {
    type: "object",
    value: { p: ArrayBuffer.prototype },
    exception: true
  },
  {
    type: "string",
    shouldEqual: true,
    value: "testFinished"
  }
];

for (var index = 0; index < messages.length; index++) {
  var message = messages[index];
  if (message.hasOwnProperty("compareValue")) {
    continue;
  }
  if (message.hasOwnProperty("shouldEqual") ||
      message.hasOwnProperty("shouldCompare")) {
    message.compareValue = message.value;
  }
}

function onmessage(event) {
  for (var index = 0; index < messages.length; index++) {
    var exception = undefined;

    try {
      postMessage(messages[index].value);
    }
    catch (e) {
      if (e.result != 2152923161) {
        throw "Exception of the wrong type: " + e.result;
      }
      exception = e;
    }

    if ((exception !== undefined && !messages[index].exception) ||
        (exception === undefined && messages[index].exception)) {
      throw "Exception inconsistency [index = " + index + ", " +
            messages[index].toSource() + "]: " + exception;
    }
  }
}
