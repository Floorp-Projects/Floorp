// Binary: cache/js-dbg-32-b39f4007be5a-linux
// Flags: -m -n -a
//

gczeal(4);
var a = ['a','test string',456,9.34,new String("string object"),[],['h','i','j','k']];
var b = [1,2,3,4,5,6,7,8,9,0];
exhaustiveSliceTest("exhaustive slice test 1", a);
function mySlice(a, from, to) {
  var returnArray = [];
  try {  }  catch ( [ x   ]   ) {  }   finally {  }
  return returnArray;
}
function exhaustiveSliceTest(testname, a) {
  var x = 0;
    for (y = (2 + a.length); y >= -(2 + a.length); y--) {
      var c = mySlice(a,x,y);
      if (String(b) != String(c))
          " expected result: " + String(c) + "\n";
    }
}
