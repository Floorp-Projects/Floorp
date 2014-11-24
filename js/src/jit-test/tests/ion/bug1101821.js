
// Makes it easier for fuzzers to get max or min statements with multiple arguments.
function test(a, b, c, d, e, f) {
    var r = 0
    r += Math.max(a);
    r += Math.max(a,b);
    r += Math.max(a,b,c);
    r += Math.max(a,b,c,d);
    r += Math.max(a,b,c,d,e);
    r += Math.max(a,b,c,d,e,f);
    r += Math.min(a);
    r += Math.min(a,b);
    r += Math.min(a,b,c);
    r += Math.min(a,b,c,d);
    r += Math.min(a,b,c,d,e);
    r += Math.min(a,b,c,d,e,f);
    return r;
}
for (var i=0; i<10; i++) {
    assertEq(test(12,5,32,6,18,2), 186);
    assertEq(test(1,5,3,6,18,-10), 48);
    assertEq(test(-19,5,20,6,18,1), -48);
}

// Test max/min result up to 5 arguments.
for (var i=1; i<5; i++) {
    var args = [];
    for (var j=0; j<i; j++)
        args[args.length] = "arg" + j;
    var max = new Function(args, "return Math.max("+args.join(",")+");");
    var min = new Function(args, "return Math.min("+args.join(",")+");");

    var input = [];
    for (var j=0; j<i; j++) {
        input[input.length] = j;
    }

    permutate(input, function (a) {
        var min_value = min.apply(undefined, a);
        var max_value = max.apply(undefined, a);
        assertEq(min_value, minimum(a));
        assertEq(max_value, maximum(a));
    });
}

function minimum(arr) {
    var min = arr[0]
    for (var i=1; i<arr.length; i++) {
        if (min > arr[i])
            min = arr[i]
    }
    return min
}
function maximum(arr) {
    var max = arr[0]
    for (var i=1; i<arr.length; i++) {
        if (max < arr[i])
            max = arr[i]
    }
    return max
}

function permutate(array, callback) {
    function p(array, index, callback) {
        function swap(a, i1, i2) {
            var t = a[i1];
            a[i1] = a[i2];
            a[i2] = t;
        }

        if (index == array.length - 1) {
            callback(array);
            return 1;
        } else {
            var count = p(array, index + 1, callback);
            for (var i = index + 1; i < array.length; i++) {
                swap(array, i, index);
                count += p(array, index + 1, callback);
                swap(array, i, index);
            }
            return count;
        }
    }

    if (!array || array.length == 0) {
        return 0;
    }
    return p(array, 0, callback);
}
