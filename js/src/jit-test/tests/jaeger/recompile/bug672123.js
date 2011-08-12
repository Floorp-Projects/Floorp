var caught = false;
function h(code) {
    f = eval("(function(){" + code + "})")
    g()
}
function g() {
    try {
      f();
    } catch (r) { caught = true }
}
h("")
for (i = 0; i < 9; i++) {
    h("")
}
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("")
h("\"\"(gc())")
assertEq(caught, true);
