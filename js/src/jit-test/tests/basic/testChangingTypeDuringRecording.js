/* Touch/init early so global shape doesn't change in loop */
var SetOnIter = HOTLOOP - 1;
var x = 3;
var i = 0;
assertEq(true, true);

for (i = 0; i < SetOnIter + 10; ++i) {
    x = 3;
    setGlobalPropIf(i == SetOnIter, 'x', 'pretty');
    assertEq(x == 'pretty', i == SetOnIter);
    x = 3;
}

for (i = 0; i < SetOnIter + 10; ++i) {
    x = 3;
    defGlobalPropIf(i == SetOnIter, 'x', { value:'pretty' });
    assertEq(x == 'pretty', i == SetOnIter);
    x = 3;
}
