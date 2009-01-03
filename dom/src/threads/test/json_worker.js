var messages = [
  {
    type: "object",
    array: false,
    exception: false,
    shouldCompare: false,
    shouldEqual: false,
    value: { foo: "bar" }
  },
  {
    type: "object",
    array: true,
    exception: false,
    shouldCompare: false,
    shouldEqual: false,
    value: [9, 8, 7]
  },
  {
    type: "object",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: null
  },
  {
    type: "undefined",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: undefined,
    compareValue: undefined
  },
  {
    type: "string",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: "Hello"
  },
  {
    type: "string",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: JSON.stringify({ foo: "bar" })
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: 1
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: 0
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: -1
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: 238573459843702923492399923049
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: -238573459843702923492399923049
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: 0.25
  },
  {
    type: "number",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: -0.25
  },
  {
    type: "boolean",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: true
  },
  {
    type: "boolean",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: false
  },

  /*
  // Uncomment these once bug 465371 is fixed!
  {
    type: "function",
    array: false,
    exception: true,
    shouldCompare: false,
    shouldEqual: false,
    value: function (foo) { return "Bad!"; }
  },
  {
    type: "xml",
    array: false,
    exception: true,
    shouldCompare: true,
    shouldEqual: true,
    value: <funtimes></funtimes>
  },
  */
  {
    type: "object",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: NaN,
    compareValue: null
  },
  {
    type: "object",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: Infinity,
    compareValue: null
  },
  {
    type: "object",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: -Infinity,
    compareValue: null
  },
  {
    type: "string",
    array: false,
    exception: false,
    shouldCompare: true,
    shouldEqual: true,
    value: "testFinished"
  }
];

for (var index = 0; index < messages.length; index++) {
  var message = messages[index];
  if (message.hasOwnProperty("compareValue")) {
    continue;
  }
  message.compareValue = message.value;
}

var onmessage = function(event) {
  for (var index = 0; index < messages.length; index++) {
    var exception = false;

    try {
      postMessage(messages[index].value);
    }
    catch (e) {
      exception = true;
    }

    if (messages[index].exception != exception) {
      throw "Exception inconsistency!";
    }
  }
}
