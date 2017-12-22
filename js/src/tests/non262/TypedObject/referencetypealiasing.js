// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 898359;
var summary = 'TypedObjects reference type aliasing';
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

  var MyType = new StructType({f: Object});

  // Test aliasing
  var myInstance = new MyType({f: {a: 22}});
  var anotherInstance = new MyType({f: myInstance.f});
  assertEq(myInstance.f.a, 22);
  assertEq(myInstance.f.a, anotherInstance.f.a);

  myInstance.f.a += 1;
  assertEq(myInstance.f.a, 23);
  assertEq(myInstance.f.a, anotherInstance.f.a);

  reportCompare(true, true, "TypedObjects reference type aliasing tests");
}

runTests();
