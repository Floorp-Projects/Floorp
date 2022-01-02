
function f(str) {
    for (var i=0; i<10; i++) {
	arr = /foo(ba(r))?/.exec(str);
	var x = arr[0] + " " + arr[1] + " " + arr[2];
    }
}
f("foobar");
f("foo");
