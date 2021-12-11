gc()
evalcx("\
    if (!(\"gcslice\" in this))\
      gcslice = function() { };\
    array = new Uint8Array;\
    t0 = array.subarray();\
    gcslice(12); \
    array.subarray();\
    gc();\
    gc();\
    array.subarray().a = 1;\
    gc();",
newGlobal(''))
