// This test case was created before %TypedArrayPrototype%.toString was
// implemented. Now that we've got %TypedArrayPrototype%.toString the test will
// attempt to create a 300300001 character long string and either timeout or
// throw an oom error. Restore the original behavior by replacing toString with
// Object.prototype.toString.
Uint8ClampedArray.prototype.toString = Object.prototype.toString;

function A(a) { this.a = a; }
A.prototype.foo = function (x) {};
function B(b) { this.b = b; }
B.prototype.foo = function (x) {};
function C(c) {}
function makeArray(n) {
    var classes = [A, B, C];
    var arr = [];
    for (var i = 0; i < n; i++) {
        arr.push(new classes[i % 3](i % 3));
    }
    return arr;
}
function runner(arr, resultArray, len) {
    for (var i = 0; i < len; i++) {
        var obj = arr[i];
        resultArray[0] += new obj.foo(i);
    }
}
var resultArray = [0];
var arr = makeArray(30000);
C.prototype.foo = Uint8ClampedArray;
runner(arr, resultArray, 30000);
