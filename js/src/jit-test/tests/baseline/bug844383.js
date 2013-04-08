s = newGlobal()
try {
    evalcx("\
        function g() {\
            h()\
        }\
        for (p in this) {\
            function h(h) {\
                ''instanceof 5\
            }\
        }\
        h.valueOf=g;\
        h==9\
    ", s)
} catch (e) {}
try {
    evalcx("throw h", s)
} catch (e) {
    try {
	"" + e
    } catch(e) {}
}
