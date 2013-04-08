function h(code) {
    f = eval("(function(){" + code + "})")
}
h("")
h("debugger;")
