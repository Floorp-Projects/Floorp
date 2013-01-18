// Binary: cache/js-dbg-32-67673422f7d2-linux
// Flags: -m -n -a
//

gczeal(2);
var lfcode = new Array();
lfcode.push("");
lfcode.push("");
while (lfcode.length > 0) {
        var file = lfcode.shift();
        loadFile(file);
}
function loadFile(lfVarx) {
        try {
                eval("\
                        Array.prototype[30] = 'B';\
                        delete Array.prototype[30];\
                        assertEquals('edcba', a.join(''));\
                ");
        } catch (lfVare) {}
}
