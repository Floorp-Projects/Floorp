function foo(arr) {
  var result = 0;
  for (var i = 0; i < arr.length; i++) {
    for (var j = 0; j < arr.length; j++) {
      if (Object.is(arr[i], arr[j])) {
        result++;
      }
    }
  }
  return result;
}

var input = [1,"one",{x:1},
	     1,"one",{x:1}];

var sum = 0;
for (var i = 0; i < 100; i++) {
  sum += foo(input);
}
assertEq(sum, 1000);
