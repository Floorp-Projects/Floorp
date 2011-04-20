Object.defineProperty(Function.prototype, 'prototype',
                      {get: function () { if (i == HOTLOOP + 1) throw "X"; }});
var x;
try {
    for (var i = 0; i < HOTLOOP + 2; i++)
        x = new Function.prototype;
} catch (exc) {
    assertEq(i, HOTLOOP + 1);
    assertEq(exc, "X");
}
