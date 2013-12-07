// |reftest| skip-if(!this.hasOwnProperty("TypedObject"))
var BUGNUMBER = 926401;
var summary = 'TypedObjects ArrayType implementation';

// Test creation of zero-length array

function runTest() {
  var T = TypedObject;
  var Color = new T.StructType({'r': T.uint8, 'g': T.uint8, 'b': T.uint8});
  var Rainbow = Color.array(0);
  var theOneISawWasJustBlack = new Rainbow([]);
  if (typeof reportCompare === "function")
    reportCompare(true, true);
  print("Tests complete");
}

runTest();
