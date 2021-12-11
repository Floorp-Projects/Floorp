var BUGNUMBER = '';
function printBugNumber (num)
{
	return "foo";
}
function optionsClear() {
  var x = printBugNumber().split(',');
}
function optionsReset() {
    optionsClear();
}
var code = new Array();
code.push("evaluate");
var x0 = "\
    printBugNumber(BUGNUMBER);\n\
    function gen()\n\
    {\n\
      try {\n\
        yield 0;\n\
      } finally {\n\
      }\n\
    }\n\
    var iter1 = gen( iter1=\"NaN\", new gen(gen)) ;\n\
    gc();\n\
";
code.push(x0);
code.push("evaluate");
var files = new Array();
while (true) {
	var file = code.shift();
	if (file == "evaluate") {
		loadFiles(files);
	} else if (file == undefined) {
		break;
	} else {
		files.push(file);
	}
}
function loadFiles(x) {
	for (i in x) {
		try {
			eval(x[i]); 
		} catch (e) {
		}
	}
	optionsReset();
}

