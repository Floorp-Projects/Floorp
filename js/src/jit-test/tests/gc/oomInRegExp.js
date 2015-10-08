load(libdir + 'oomTest.js');
oomTest(() => assertEq("foobar\xff5baz\u1200".search(/bar\u0178\d/i), 3));
oomTest(() => assertEq((/(?!(?!(?!6)[\Wc]))/i).test(), false));
