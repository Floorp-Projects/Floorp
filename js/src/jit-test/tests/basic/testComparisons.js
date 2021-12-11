function testComparisons()
{
  // All the special values from each of the types in
  // ECMA-262, 3rd ed. section 8
  var undefinedType, nullType, booleanType, stringType, numberType, objectType;

  var types = [];
  types[undefinedType = 0] = "Undefined";
  types[nullType = 1] = "Null";
  types[booleanType = 2] = "Boolean";
  types[stringType = 3] = "String";
  types[numberType = 4] = "Number";
  types[objectType = 5] = "Object";

  var JSVAL_INT_MIN = -Math.pow(2, 30);
  var JSVAL_INT_MAX = Math.pow(2, 30) - 1;

  // Values from every ES3 type, hitting all the edge-case and special values
  // that can be dreamed up
  var values =
    {
     "undefined":
       {
         value: function() { return undefined; },
         type: undefinedType
       },
     "null":
       {
         value: function() { return null; },
         type: nullType
       },
     "true":
       {
         value: function() { return true; },
         type: booleanType
       },
     "false":
       {
         value: function() { return false; },
         type: booleanType
       },
     '""':
       {
         value: function() { return ""; },
         type: stringType
       },
     '"a"':
       {
         // a > [, for string-object comparisons
         value: function() { return "a"; },
         type: stringType
       },
     '"Z"':
       {
         // Z < [, for string-object comparisons
         value: function() { return "Z"; },
         type: stringType
       },
     "0":
       {
         value: function() { return 0; },
         type: numberType
       },
     "-0":
       {
         value: function() { return -0; },
         type: numberType
       },
     "1":
       {
         value: function() { return 1; },
         type: numberType
       },
     "Math.E":
       {
         value: function() { return Math.E; },
         type: numberType
       },
     "JSVAL_INT_MIN - 1":
       {
         value: function() { return JSVAL_INT_MIN - 1; },
         type: numberType
       },
     "JSVAL_INT_MIN":
       {
         value: function() { return JSVAL_INT_MIN; },
         type: numberType
       },
     "JSVAL_INT_MIN + 1":
       {
         value: function() { return JSVAL_INT_MIN + 1; },
         type: numberType
       },
     "JSVAL_INT_MAX - 1":
       {
         value: function() { return JSVAL_INT_MAX - 1; },
         type: numberType
       },
     "JSVAL_INT_MAX":
       {
         value: function() { return JSVAL_INT_MAX; },
         type: numberType
       },
     "JSVAL_INT_MAX + 1":
       {
         value: function() { return JSVAL_INT_MAX + 1; },
         type: numberType
       },
     "Infinity":
       {
         value: function() { return Infinity; },
         type: numberType
       },
     "-Infinity":
       {
         value: function() { return -Infinity; },
         type: numberType
       },
     "NaN":
       {
         value: function() { return NaN; },
         type: numberType
       },
     "{}":
       {
         value: function() { return {}; },
         type: objectType
       },
     "{ valueOf: undefined }":
       {
         value: function() { return { valueOf: undefined }; },
         type: objectType
       },
     "[]":
       {
         value: function() { return []; },
         type: objectType
       },
     '[""]':
       {
         value: function() { return [""]; },
         type: objectType
       },
     '["a"]':
       {
         value: function() { return ["a"]; },
         type: objectType
       },
     "[0]":
       {
         value: function() { return [0]; },
         type: objectType
       }
    };

  var orderOps =
    {
     "<": function(a, b) { return a < b; },
     ">": function(a, b) { return a > b; },
     "<=": function(a, b) { return a <= b; },
     ">=": function(a, b) { return a >= b; }
    };
  var eqOps =
    {
     "==": function(a, b) { return a == b; },
     "!=": function(a, b) { return a != b; },
     "===": function(a, b) { return a === b; },
     "!==": function(a, b) { return a !== b; }
    };


  var notEqualIncomparable =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };
  var notEqualLessThan =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": true, ">": false, "<=": true, ">=": false }
    };
  var notEqualGreaterThan =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": true, "<=": false, ">=": true }
    };
  var notEqualNorDifferent =
    {
      eq: { "==": false, "!=": true, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var strictlyEqual =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqual =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqualNotDifferent =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var looselyEqualIncomparable =
    {
      eq: { "==": true, "!=": false, "===": false, "!==": true },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };
  var strictlyEqualNotDifferent =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": true, ">=": true }
    };
  var strictlyEqualIncomparable =
    {
      eq: { "==": true, "!=": false, "===": true, "!==": false },
      order: { "<": false, ">": false, "<=": false, ">=": false }
    };

  var comparingZeroToSomething =
    {
      "undefined": notEqualIncomparable,
      "null": notEqualNorDifferent,
      "true": notEqualLessThan,
      "false": looselyEqual,
      '""': looselyEqualNotDifferent,
      '"a"': notEqualIncomparable,
      '"Z"': notEqualIncomparable,
      "0": strictlyEqual,
      "-0": strictlyEqual,
      "1": notEqualLessThan,
      "Math.E": notEqualLessThan,
      "JSVAL_INT_MIN - 1": notEqualGreaterThan,
      "JSVAL_INT_MIN": notEqualGreaterThan,
      "JSVAL_INT_MIN + 1": notEqualGreaterThan,
      "JSVAL_INT_MAX - 1": notEqualLessThan,
      "JSVAL_INT_MAX": notEqualLessThan,
      "JSVAL_INT_MAX + 1": notEqualLessThan,
      "Infinity": notEqualLessThan,
      "-Infinity": notEqualGreaterThan,
      "NaN": notEqualIncomparable,
      "{}": notEqualIncomparable,
      "{ valueOf: undefined }": notEqualIncomparable,
      "[]": looselyEqual,
      '[""]': looselyEqual,
      '["a"]': notEqualIncomparable,
      "[0]": looselyEqual
    };

  var comparingObjectOrObjectWithValueUndefined =
    {
      "undefined": notEqualIncomparable,
      "null": notEqualIncomparable,
      "true": notEqualIncomparable,
      "false": notEqualIncomparable,
      '""': notEqualGreaterThan,
      '"a"': notEqualLessThan,
      '"Z"': notEqualGreaterThan,
      "0": notEqualIncomparable,
      "-0": notEqualIncomparable,
      "1": notEqualIncomparable,
      "Math.E": notEqualIncomparable,
      "JSVAL_INT_MIN - 1": notEqualIncomparable,
      "JSVAL_INT_MIN": notEqualIncomparable,
      "JSVAL_INT_MIN + 1": notEqualIncomparable,
      "JSVAL_INT_MAX - 1": notEqualIncomparable,
      "JSVAL_INT_MAX": notEqualIncomparable,
      "JSVAL_INT_MAX + 1": notEqualIncomparable,
      "Infinity": notEqualIncomparable,
      "-Infinity": notEqualIncomparable,
      "NaN": notEqualIncomparable,
      "{}": notEqualNorDifferent,
      "{ valueOf: undefined }": notEqualNorDifferent,
      "[]": notEqualGreaterThan,
      '[""]': notEqualGreaterThan,
      '["a"]': notEqualLessThan,
      "[0]": notEqualGreaterThan
    };

  // Constructed expected-value matrix
  var expected =
    {
     "undefined":
       {
         "undefined": strictlyEqualIncomparable,
         "null": looselyEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualIncomparable,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualIncomparable,
         '[""]': notEqualIncomparable,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualIncomparable
       },
     "null":
       {
         "undefined": looselyEqualIncomparable,
         "null": strictlyEqualNotDifferent,
         "true": notEqualLessThan,
         "false": notEqualNorDifferent,
         '""': notEqualNorDifferent,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualNorDifferent,
         "-0": notEqualNorDifferent,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualNorDifferent
       },
     "true":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": strictlyEqual,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": looselyEqual,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "false":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": strictlyEqual,
         '""': looselyEqualNotDifferent,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": looselyEqual,
         '[""]': looselyEqual,
         '["a"]': notEqualIncomparable,
         "[0]": looselyEqual
       },
     '""':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': strictlyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": looselyEqual,
         '[""]': looselyEqual,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '"a"':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': strictlyEqual,
         '"Z"': notEqualGreaterThan,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualGreaterThan,
         "{ valueOf: undefined }": notEqualGreaterThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': looselyEqualNotDifferent,
         "[0]": notEqualGreaterThan
       },
     '"Z"':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': notEqualLessThan,
         '"Z"': strictlyEqual,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualLessThan,
         "[0]": notEqualGreaterThan
       },
     "0": comparingZeroToSomething,
     "-0": comparingZeroToSomething,
     "1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": looselyEqual,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": strictlyEqual,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "Math.E":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": strictlyEqual,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MIN - 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": strictlyEqual,
         "JSVAL_INT_MIN": notEqualLessThan,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MIN":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": strictlyEqual,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MIN + 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": strictlyEqual,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "JSVAL_INT_MAX - 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": strictlyEqual,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MAX":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": strictlyEqual,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "JSVAL_INT_MAX + 1":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": notEqualGreaterThan,
         "JSVAL_INT_MAX + 1": strictlyEqual,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "Infinity":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualGreaterThan,
         "true": notEqualGreaterThan,
         "false": notEqualGreaterThan,
         '""': notEqualGreaterThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualGreaterThan,
         "-0": notEqualGreaterThan,
         "1": notEqualGreaterThan,
         "Math.E": notEqualGreaterThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualGreaterThan,
         "JSVAL_INT_MAX": notEqualGreaterThan,
         "JSVAL_INT_MAX + 1": notEqualGreaterThan,
         "Infinity": strictlyEqual,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualGreaterThan
       },
     "-Infinity":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualLessThan,
         "true": notEqualLessThan,
         "false": notEqualLessThan,
         '""': notEqualLessThan,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualLessThan,
         "-0": notEqualLessThan,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualLessThan,
         "JSVAL_INT_MIN": notEqualLessThan,
         "JSVAL_INT_MIN + 1": notEqualLessThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": strictlyEqual,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualLessThan,
         '[""]': notEqualLessThan,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualLessThan
       },
     "NaN":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualIncomparable,
         '"a"': notEqualIncomparable,
         '"Z"': notEqualIncomparable,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualIncomparable,
         "{ valueOf: undefined }": notEqualIncomparable,
         "[]": notEqualIncomparable,
         '[""]': notEqualIncomparable,
         '["a"]': notEqualIncomparable,
         "[0]": notEqualIncomparable
       },
     "{}": comparingObjectOrObjectWithValueUndefined,
     "{ valueOf: undefined }": comparingObjectOrObjectWithValueUndefined,
     "[]":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': looselyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '[""]':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': looselyEqual,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualNorDifferent,
         '[""]': notEqualNorDifferent,
         '["a"]': notEqualLessThan,
         "[0]": notEqualLessThan
       },
     '["a"]':
       {
         "undefined": notEqualIncomparable,
         "null": notEqualIncomparable,
         "true": notEqualIncomparable,
         "false": notEqualIncomparable,
         '""': notEqualGreaterThan,
         '"a"': looselyEqual,
         '"Z"': notEqualGreaterThan,
         "0": notEqualIncomparable,
         "-0": notEqualIncomparable,
         "1": notEqualIncomparable,
         "Math.E": notEqualIncomparable,
         "JSVAL_INT_MIN - 1": notEqualIncomparable,
         "JSVAL_INT_MIN": notEqualIncomparable,
         "JSVAL_INT_MIN + 1": notEqualIncomparable,
         "JSVAL_INT_MAX - 1": notEqualIncomparable,
         "JSVAL_INT_MAX": notEqualIncomparable,
         "JSVAL_INT_MAX + 1": notEqualIncomparable,
         "Infinity": notEqualIncomparable,
         "-Infinity": notEqualIncomparable,
         "NaN": notEqualIncomparable,
         "{}": notEqualGreaterThan,
         "{ valueOf: undefined }": notEqualGreaterThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualNorDifferent,
         "[0]": notEqualGreaterThan
       },
     "[0]":
       {
         "undefined": notEqualIncomparable,
         "null": notEqualNorDifferent,
         "true": notEqualLessThan,
         "false": looselyEqual,
         '""': notEqualGreaterThan,
         '"a"': notEqualLessThan,
         '"Z"': notEqualLessThan,
         "0": looselyEqual,
         "-0": looselyEqual,
         "1": notEqualLessThan,
         "Math.E": notEqualLessThan,
         "JSVAL_INT_MIN - 1": notEqualGreaterThan,
         "JSVAL_INT_MIN": notEqualGreaterThan,
         "JSVAL_INT_MIN + 1": notEqualGreaterThan,
         "JSVAL_INT_MAX - 1": notEqualLessThan,
         "JSVAL_INT_MAX": notEqualLessThan,
         "JSVAL_INT_MAX + 1": notEqualLessThan,
         "Infinity": notEqualLessThan,
         "-Infinity": notEqualGreaterThan,
         "NaN": notEqualIncomparable,
         "{}": notEqualLessThan,
         "{ valueOf: undefined }": notEqualLessThan,
         "[]": notEqualGreaterThan,
         '[""]': notEqualGreaterThan,
         '["a"]': notEqualLessThan,
         "[0]": notEqualNorDifferent
       }
    };



  var failures = [];
  function fail(a, ta, b, tb, ex, ac, op)
  {
    failures.push("(" + a + " " + op + " " + b + ") wrong: " +
                  "expected " + ex + ", got " + ac +
                  " (types " + types[ta] + ", " + types[tb] + ")");
  }

  var result = false;
  for (var i in values)
  {
    for (var j in values)
    {
      // Constants, so hoist to help JIT know that
      var vala = values[i], valb = values[j];
      var a = vala.value(), b = valb.value();

      for (var opname in orderOps)
      {
        var op = orderOps[opname];
        var expect = expected[i][j].order[opname];
        var failed = false;

        for (var iter = 0; iter < 5; iter++)
        {
          result = op(a, b);
          failed = failed || result !== expect;
        }

        if (failed)
          fail(i, vala.type, j, valb.type, expect, result, opname);
      }

      for (var opname in eqOps)
      {
        var op = eqOps[opname];
        var expect = expected[i][j].eq[opname];
        var failed = false;

        for (var iter = 0; iter < 5; iter++)
        {
          result = op(a, b);
          failed = failed || result !== expect;
        }

        if (failed)
          fail(i, vala.type, j, valb.type, expect, result, opname);
      }
    }
  }

  if (failures.length == 0)
    return "no failures reported!";

  return "\n" + failures.join(",\n");
}
assertEq(testComparisons(), "no failures reported!");
