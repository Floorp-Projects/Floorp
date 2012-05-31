// |reftest| skip-if(!xulRuntime.shell)
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

assertEq(testLenientAndStrict("var o = {m:function(){}}; o.function::m()",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict("default xml namespace = 'foo'",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict("var x = <x/>",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict("var x = <x><p>42</p></x>",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict("var x = <x><p>42</p></x>; x.(p == 42)",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

assertEq(testLenientAndStrict("var x = <x><p>42</p></x>; x..p",
                              parsesSuccessfully,
                              parseRaisesException(SyntaxError)),
         true);

if (typeof this.options == "function") {
    this.options('moar_xml');

    assertEq(testLenientAndStrict("var cdata = <![CDATA[bar]]>",
                                  parsesSuccessfully,
                                  parseRaisesException(SyntaxError)),
             true);

    assertEq(testLenientAndStrict("var comment = <!-- hi -->",
                                  parsesSuccessfully,
                                  parseRaisesException(SyntaxError)),
             true);

    this.options('moar_xml');
}

reportCompare(true, true);
