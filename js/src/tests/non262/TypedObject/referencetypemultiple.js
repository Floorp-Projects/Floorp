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


function runTests()
{
  var S = new StructType({f: Any, g: Any});
  var s = new S({f: "Hello", g: "Hello"});
  assertEq(s.f, s.g);
  reportCompare(true, true, "TypedObjects trace tests");
}

runTests();
