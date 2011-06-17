// first build a big honkin' string
str = "a";
for (var i = 0; i < 20; ++i)
    str = str + str;
str.indexOf('a');

// copying this sucker should gc
makeFinalizeObserver();
assertEq(finalizeCount(), 0);
// if the assert fails, add more iterations
for (var i = 0; i < 50; ++i)
    str.replace(/(a)/, '$1');
assertEq(finalizeCount(), 1);
