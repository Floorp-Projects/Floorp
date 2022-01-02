loadFile("\
saveStack();\
gcPreserveCode = function() {};\
gc();\
saveStack() == 3\
");
function loadFile(lfVarx) {
   evaluate(lfVarx);
}
