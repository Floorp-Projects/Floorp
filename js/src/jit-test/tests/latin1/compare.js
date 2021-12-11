function test() {
    var arr = [
	"abc",
	"abcd",
	"123\u00ff"
    ];
    for (var i = 0; i < arr.length; i++) {
	for (var j = 0; j < arr.length; j++) {
	    var s1 = arr[i];
	    var s2 = arr[j];
	    var s1tb = "\u1200" + s1;
	    var s2tb = "\u1200" + s2;
	    assertEq(s1 < s2, s1tb < s2tb);
	    assertEq(s1 > s2, s1tb > s2tb);
	    assertEq(s1 <= s2, s1tb <= s2tb);
	    assertEq(s1 >= s2, s1tb >= s2tb);
	}
    }
}
test();
