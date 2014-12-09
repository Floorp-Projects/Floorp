// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 898359;
var summary = 'TypedObjects reference type default values';
var actual = '';
var expect = '';

var ArrayType = TypedObject.ArrayType;
var StructType = TypedObject.StructType;
var Any = TypedObject.Any;
var Object = TypedObject.Object;
var string = TypedObject.string;

function runTests()
{
  printBugNumber(BUGNUMBER);
  printStatus(summary);

  var S = new StructType({any: Any,
                          object: Object,
                          string: string});
  var s = new S();

  assertEq(s.any, undefined);
  assertEq(s.object, null);
  assertEq(s.string, "");

  reportCompare(true, true, "TypedObjects ref type uninit");
}

runTests();
