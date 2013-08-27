if (typeof schedulegc != 'undefined') {
    Function("\
        x = (function() { yield })();\
        Set(x);\
        schedulegc(1);\
        print( /x/ );\
        for (p in x) {}\
    ")();
}
