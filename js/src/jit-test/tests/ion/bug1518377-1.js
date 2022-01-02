Object.prototype[Symbol.toPrimitive] = inIon;
which = function() {};
for (var i = 0; i < 10; ++i) {
    s = which[which[which]];
    a = which;
    a += s + "";
}
