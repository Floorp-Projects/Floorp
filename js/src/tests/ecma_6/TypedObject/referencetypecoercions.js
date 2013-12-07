// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 898359;
var summary = 'TypedObjects reference type coercions';
var actual = '';
var expect = '';

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var Any = TypedObject.Any;
var Object = TypedObject.Object;
var string = TypedObject.string;

function TestValues(type, values) {
  for (var i = 0; i < values.length; i++) {
    compare(type(values[i].input), values[i]);
  }

  var Struct = new StructType({f: type});
  for (var i = 0; i < values.length; i++) {
    var struct = new Struct({f: values[i].input});
    compare(struct.f, values[i]);
  }

  for (var i = 0; i < values.length; i++) {
    var struct = new Struct();
    struct.f = values[i].input;
    compare(struct.f, values[i]);
  }

  var Array = new ArrayType(type).dimension(1);
  for (var i = 0; i < values.length; i++) {
    var array = new Array();
    array[0] = values[i].input;
    compare(array[0], values[i]);
  }

  function compare(v, spec) {
    if (spec.source)
      v = v.toSource();
    assertEq(v, spec.output);
  }
}

function runTests()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  var x = {};

  TestValues(Any, [{input: undefined, output: undefined},
                   {input: x, output: x},
                   {input: 22.22, output: 22.22},
                   {input: true, output: true}]);

  TestValues(string, [{input: undefined, output: "undefined"},
                      {input: x, output: x.toString()},
                      {input: 22.22, output: "22.22"},
                      {input: true, output: "true"}]);

  assertThrows(() => Object(undefined));

  TestValues(Object, [{input: x, output: x},
                      {input: 22.22, source: true, output: "(new Number(22.22))"},
                      {input: true, source: true, output: "(new Boolean(true))"}]);

  reportCompare(true, true, "TypedObjects reference type tests");
}

runTests();
