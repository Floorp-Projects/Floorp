
function f() {
    var propNames = ["a","b","c","d","e","f","g","h","i","j","x","y"];
    var arr = [];
    for (var i=0; i<64; i++)
	arr.push({x:1, y:2});
    for (var i=0; i<64; i++) {
        // Make sure there are expandos with dynamic slots for each object.
        for (var j = 0; j < propNames.length; j++)
            arr[i][propNames[j]] = j;
    }
    var res = 0;
    for (var i=0; i<100000; i++) {
	var o = arr[i % 64];
	var p = propNames[i % propNames.length];
	res += o[p];
    }
    assertEq(res, 549984);
}
f();
