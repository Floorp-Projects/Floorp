// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 922216;
var summary = 'TypedObjects Equivalent Numeric Types';

var T = TypedObject;

function runTests() {
  print(BUGNUMBER + ": " + summary);

  var simpleTypes = [
    T.int8, T.int16, T.int32,
    T.uint8, T.uint16, T.uint32,
    T.float32, T.float64,
    T.Object, T.Any, T.string
  ];

  for (var i = 0; i < simpleTypes.length; i++)
    for (var j = 0; j < simpleTypes.length; j++)
      assertEq(i == j, simpleTypes[i].equivalent(simpleTypes[j]));

  reportCompare(true, true);
  print("Tests complete");
}

runTests();
