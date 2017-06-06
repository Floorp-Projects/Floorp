gczeal(0);
function g() {
    for (var j = 0; j < 999; ++j) {
        try {
            k
        } catch (e) {
            try {
                r
            } catch (e) {}
        }
    }
}
function h(code) {
    try {
        f = Function(code)
    } catch (r) {};
    try {
        f()
    } catch (r) {}
    eval("")
}
h("m=function(){};g(m,[,])")
h("=")
h("=")
h("=")
h("startgc(1,'shrinking')")
h("gcparam(\"maxBytes\",gcparam(\"gcBytes\")+4);for(r;;i++){}")
