r = evalcx("/x/", undefined);
s = "";
gc()
Function("\
    s.match(r);\
    schedulezone(__proto__);\
    ({c:schedulegc(2)});\
    s.match(r);\
")()
