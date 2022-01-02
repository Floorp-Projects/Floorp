load(libdir + 'andTestHelper.js');
load(libdir + 'orTestHelper.js');

function source(x) {
  switch (typeof x) {
    case "number":
    case "boolean":
      return String(x);
    case "undefined":
      return "(void 0)";
    case "string":
      return JSON.stringify(x);
    case "object":
      if (x === null) {
        return "null";
      }
      return `(${JSON.stringify(x)})`;
    default:
      throw new Error("not implemented: " + typeof x);
  }
}

(function () {
   var opsies   = ["||", "&&"];
   var falsies  = [null, undefined, false, NaN, 0, ""];
   var truthies = [{}, true, 1, 42, 1/0, -1/0, "blah"];
   var boolies  = [falsies, truthies];

   for (var op of opsies) {
     for (var i in boolies) {
       for (var j in boolies[i]) {
	 var x = source(boolies[i][j]);
	 for (var k in boolies) {
	   for (var l in boolies[k]) {
	     var y = source(boolies[k][l]);
	     var prefix = (op == "||") ? "or" : "and";
	     var f = new Function("return " + prefix + "TestHelper(" + x + "," + y + ",10)");
	     var expected = eval(x + op + y) ? 45 : 0;
	     assertEq(f(), expected);
	   }
	 }
       }
     }
   }
 })();
