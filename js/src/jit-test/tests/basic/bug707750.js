// |jit-test| error: ReferenceError
var lfcode = new Array();
lfcode.push("gczeal(4);");
lfcode.push('print(BUGNUMBER + ": " + (W       --    ));');
while (true) {
        var file = lfcode.shift(); if (file == undefined) { break; }
        eval(file);
}

