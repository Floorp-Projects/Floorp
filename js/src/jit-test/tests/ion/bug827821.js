var lfcode = new Array();
lfcode.push("3");
lfcode.push("\
	gczeal(2);\
	for (let q = 0; q < 50; ++q) {\
		var w = \"r\".match(/r/);\
	}\
	let (eval) (function  (a) {\
		Function = gczeal;\
	})();\
	// .js\
");
lfcode.push(" // .js");
lfcode.push(" // .js");
lfcode.push(" // .js");
while (true) {
	var file = lfcode.shift(); if (file == undefined) { break; }
        loadFile(file)
}
function loadFile(lfVarx) {
    try {
        if (lfVarx.substr(-3) == ".js") {
	    uneval("foo");
            switch (lfRunTypeId) {
                case 3: function newFunc(x) { new Function(x)(); }; newFunc(lfVarx); break;
                case 4: eval("(function() { " + lfVarx + " })();"); break;
            }
        } else if (!isNaN(lfVarx)) {
            lfRunTypeId = parseInt(lfVarx);
	}
    } catch (lfVare) {}
}
