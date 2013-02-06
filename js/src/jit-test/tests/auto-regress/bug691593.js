// Binary: cache/js-dbg-64-acf3c1fb7c94-linux
// Flags:
//

genexp = "x * x for (x in [])";
genexpParened = "(" + genexp + ")";
needParens(2, "if (1, xx) { }");
function needParens(section, pat, exp) {
  ft = pat.replace(/xx/, genexpParened);
  try {
    f = new Function(ft);
  } catch(e) {  }
  overParenTest(section, f, exp);
}
function overParenTest(section, f, exp) {
  var uf = "" + f;
  if (uf.indexOf(genexpParened) != -1) {  }
}
