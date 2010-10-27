var arr = [StopIteration, StopIteration, StopIteration, StopIteration, {}];
var obj = {};
var x;
var result = 'no error';
try {
    for (var i = 0; i < arr.length; i++)
        x = (obj instanceof arr[i]);  // last iteration throws, triggering deep bail
} catch (exc) {
    result = exc.constructor.name;
}
assertEq(result, 'TypeError');

