var noParamFuns = ["".bold, "".italics, "".fixed, "".strike, "".small, "".big,
	           "".blink, "".sup, "".sub];
var noParamTags = ["b", "i", "tt", "strike", "small", "big",
		   "blink", "sup", "sub"];

function testNoParam(s) {
    for (var i=0; i<noParamFuns.length; i++) {
	var fun = noParamFuns[i];
	assertEq(fun.length, 0);
	var res = fun.call(s);
	var tag = noParamTags[i];
	assertEq(res, "<" + tag + ">" + String(s) + "</" + tag + ">");
    }
}
testNoParam("Foo");
testNoParam('aaa"bbb\'c<>123');
testNoParam(123);

// toString should be called, not valueOf
testNoParam({toString: () => 1, valueOf: () => { throw "fail"; } });

assertEq("".anchor.length, 1);
assertEq("".link.length, 1);
assertEq("".fontsize.length, 1);
assertEq("".fontcolor.length, 1);

// " in the attribute value is escaped (&quot;)
assertEq("bla\"<>'".anchor("foo'<>\"\"123\"/\\"),
	 "<a name=\"foo'<>&quot;&quot;123&quot;/\\\">bla\"<>'</a>");
assertEq("bla\"<>'".link("foo'<>\"\"123\"/\\"),
	 "<a href=\"foo'<>&quot;&quot;123&quot;/\\\">bla\"<>'</a>");
assertEq("bla\"<>'".fontsize("foo'<>\"\"123\"/\\"),
	 "<font size=\"foo'<>&quot;&quot;123&quot;/\\\">bla\"<>'</font>");
assertEq("bla\"<>'".fontcolor("foo'<>\"\"123\"/\\"),
	 "<font color=\"foo'<>&quot;&quot;123&quot;/\\\">bla\"<>'</font>");

assertEq("".anchor('"'), '<a name="&quot;"></a>');
assertEq("".link('"'), '<a href="&quot;"></a>');
assertEq("".fontcolor('"'), '<font color="&quot;"></font>');
assertEq("".fontsize('"'), '<font size="&quot;"></font>');

assertEq("".anchor('"1'), '<a name="&quot;1"></a>');
assertEq("".link('"1'), '<a href="&quot;1"></a>');
assertEq("".fontcolor('"1'), '<font color="&quot;1"></font>');
assertEq("".fontsize('"1'), '<font size="&quot;1"></font>');

assertEq("".anchor('"""a"'), '<a name="&quot;&quot;&quot;a&quot;"></a>');
assertEq("".link('"""a"'), '<a href="&quot;&quot;&quot;a&quot;"></a>');
assertEq("".fontcolor('"""a"'), '<font color="&quot;&quot;&quot;a&quot;"></font>');
assertEq("".fontsize('"""a"'), '<font size="&quot;&quot;&quot;a&quot;"></font>');

assertEq("".anchor(""), '<a name=""></a>');
assertEq("".link(""), '<a href=""></a>');
assertEq("".fontcolor(""), '<font color=""></font>');
assertEq("".fontsize(""), '<font size=""></font>');

assertEq("foo".anchor(), "<a name=\"undefined\">foo</a>");
assertEq("foo".link(), "<a href=\"undefined\">foo</a>");
assertEq("foo".fontcolor(), "<font color=\"undefined\">foo</font>");
assertEq("foo".fontsize(), "<font size=\"undefined\">foo</font>");

assertEq("foo".anchor(3.14), '<a name="3.14">foo</a>');
assertEq("foo".link(3.14), '<a href="3.14">foo</a>');
assertEq("foo".fontcolor(3.14), '<font color="3.14">foo</font>');
assertEq("foo".fontsize(3.14), '<font size="3.14">foo</font>');

assertEq("foo".anchor({}), '<a name="[object Object]">foo</a>');
assertEq("foo".link(Math), '<a href="[object Math]">foo</a>');
assertEq("foo".fontcolor([1,2]), '<font color="1,2">foo</font>');
assertEq("foo".fontsize({}), '<font size="[object Object]">foo</font>');

// toString should be called, not valueOf, and toString must be called on |this|
// before it's called on the argument. Also makes sure toString is called only
// once.
var count = 0;
var o1 = {toString: () => { return count += 1; }, valueOf: () => { throw "fail"; } };
var o2 = {toString: () => { return count += 5; }, valueOf: () => { throw "fail"; } };
assertEq("".anchor.call(o1, o2), '<a name="6">1</a>');
assertEq("".link.call(o1, o2), '<a href="12">7</a>');
assertEq("".fontcolor.call(o1, o2), '<font color="18">13</font>');
assertEq("".fontsize.call(o1, o2), '<font size="24">19</font>');
assertEq(count, 24);
