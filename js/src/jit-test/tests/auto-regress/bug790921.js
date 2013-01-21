// |jit-test| slow;
// Binary: cache/js-dbg-64-fdfaef738a00-linux
// Flags: --ion-eager
//

evaluate("\
gcparam(\"maxBytes\", gcparam(\"gcBytes\") + 4 );\n\
test();\n\
function test() {\n\
  function flatten(arr)\n\
  actual = flatten([1, [2], 3]);\
}\n\
");
try {} catch (lfVare) {}
