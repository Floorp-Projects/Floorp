function f(s) {
    return arguments[s];
}

for (var i = 0; i < 10; ++i)
    assertEq(f(String(i+1), 0,1,2,3,4,5,6,7,8,9), i);
