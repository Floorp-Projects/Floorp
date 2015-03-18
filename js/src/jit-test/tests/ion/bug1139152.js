function toLiteralSource(value) {
    if (value === null) {
        return 'null';
    }
    if (typeof value === 'string') {
        return escapeString(value);
    }
    if (typeof value === 'number') {
        return generateNumber(value);
    }
    if (typeof value === 'boolean') {
        return value ? 'true' : 'false';
    }
    value.test();
}

function test(x) {
   var b = x ? true : {};
   return toLiteralSource(b);
}

var output = true
for (var i=0; i<1000; i++) {
    output = test(output) == 'true';
}
