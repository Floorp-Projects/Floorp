if (typeof ParallelArray === "undefined")
    quit();

function toString(r) {
    var result = "";
    for (var i = 0; i < 5; i++)
	result += r.get(i);
    return result;
}
Array.prototype[2] = 'y';
assertEq(toString(new ParallelArray(['x', 'x'])), "xxyundefinedundefined");
