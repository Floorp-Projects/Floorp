
var emptyArray = [];
var denseArray = [1, 2, 3, 4];
var sparseArray = [1,,2,,3,,4];
var bigArray = new Array();
for (var i = 0; i < 128; i++) {
    bigArray.push(i);
}
var nonArray = {0:1, 1:2, 2:3, 3:4, length:2};
var indexedGetterArray = new Array();
Object.defineProperty(indexedGetterArray, '2', {get:function () { return 51; }});

var ARRAYS = [emptyArray, denseArray, sparseArray, bigArray, nonArray, indexedGetterArray];

var targetFun = function (a, b, c, d) {
    if (a === undefined)
        a = 0;
    if (b === undefined)
        b = 0;
    if (c === undefined)
        c = 0;
    if (d === undefined)
        d = 0;
    this.count += arguments.length + a + b + c + d;
}

var PERMUTATIONS = ARRAYS.length * ARRAYS.length;
function arrayPermutation(num) {
    var idx1 = num % ARRAYS.length;
    var idx2 = ((num / ARRAYS.length)|0) % ARRAYS.length;
    var resultArray = [];
    resultArray.push(ARRAYS[idx1]);
    resultArray.push(ARRAYS[idx2]);
    return resultArray;
}
var EXPECTED_RESULTS = {
    0:0, 1:280, 2:200, 3:2680, 4:100, 5:1080, 6:280, 7:560, 8:480, 9:2960,
    10:380, 11:1360, 12:200, 13:480, 14:400, 15:2880, 16:300, 17:1280, 18:2680,
    19:2960, 20:2880, 21:5360, 22:2780, 23:3760, 24:100, 25:380, 26:300, 27:2780,
    28:200, 29:1180, 30:1080, 31:1360, 32:1280, 33:3760, 34:1180, 35:2160
};

var callerNo = 0;
function generateCaller() {
    var fn;

    // Salt eval-string with callerNo to make sure eval caching doesn't take effect.
    var s = "function caller" + callerNo + "(fn, thisObj, arrays) {" +
            "  for (var i = 0; i < arrays.length; i++) {" +
            "    fn.apply(thisObj, arrays[i]);" +
            "  }" +
            "}" +
            "fn = caller" + callerNo + ";";
    eval(s);
    return fn;
};

function main() {
    for (var i = 0; i < PERMUTATIONS; i++) {
        var obj = {count:0};
        var arrs = arrayPermutation(i);
        var fn = generateCaller(arrs.length);
        // Loop 20 times so baseline compiler has chance to kick in and compile the scripts.
        for (var j = 0; j < 20; j++)
            fn(targetFun, obj, arrs);
        assertEq(obj.count, EXPECTED_RESULTS[i]);
    }
}

main();
