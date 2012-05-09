a = 'a';
b = [,];
exhaustiveSliceTest("exhaustive slice test 1", a);
print('---');
exhaustiveSliceTest("exhaustive slice test 2", b);
function exhaustiveSliceTest(testname, a){
  x = 0
  var y = 0;
  countHeap();
    for (y=a.length; y + a.length; y--) { print(y);
					  var b  = a.slice(x,y); }
}
