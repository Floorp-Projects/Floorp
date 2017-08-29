gczeal(2);
o1 = Iterator;
var o2 = (function() { return arguments; })();
function f(o) {
    for(var j=0; j<20; j++) {
        Object.seal(o2);
        (function() { return eval(o); })() == o1;
        (function() { return {x: arguments}.x; })();
        if (false) {};
    }
}
f({});
