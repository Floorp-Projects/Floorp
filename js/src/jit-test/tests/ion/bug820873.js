
var lfcode = new Array();
lfcode.push("3");
lfcode.push("with(evalcx('')) this.__defineGetter__('x', Function);");
lfcode.push("gczeal(2)");
lfcode.push("4");
lfcode.push("\
	var log = '';\
	for (var { m  } = i = 0 ;  ; i++) {\
		log += x; \
		if (x === 6)\
			a.slow = true; if (i > 1000) break;\
	}\
");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
        loadFile(file)
}
function loadFile(lfVarx) {
	if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
        } else {
            switch (lfRunTypeId) {
                case 3: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
                case 4: eval("(function() { " + lfVarx + " })();"); break;
	}
    }
}
