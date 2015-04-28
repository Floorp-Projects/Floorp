var o = {};
function foo() {
    var i = 0;
    startgc(0);
    Object.defineProperty(o, 'foo', {configurable: true, get: function g() { return i; },
                                                         set: function s() { return i; }});
    Object.defineProperty(o, 'foo', {configurable: true, get: function g() { return i; },
                                                         set: function s() { return i; }});
    Object.defineProperty(o, 'foo', {configurable: true, get: function g() { return i; },
                                                         set: function s() { return i; }});
    Object.defineProperty(o, 'foo', {configurable: true, get: function g() { return i; },
                                                         set: function s() { return i; }});
    abortgc();
}
foo();
