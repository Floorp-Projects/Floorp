// Binary: cache/js-dbg-64-c34398f961e7-linux
// Flags: --ion-eager
//

var lfcode = new Array();
lfcode.push("gczeal(4);");
lfcode.push("");
while (true) {
	var file = lfcode.shift();
	if (file == undefined) { break; }
        loadFile(file);
}
function loadFile(lfVarx) {
	eval(lfVarx);
}
