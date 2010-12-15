options('atline')

var F, L;

eval('//@line 42 "foo"\n' +
     'try { nosuchname; } catch (e) { F = e.fileName; L = e.lineNumber; }');
assertEq(F, "foo");
assertEq(L, 42);

reportCompare(0, 0, "ok");
