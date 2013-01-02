// Don't assert.
eval("\
    x = RegExp(\"()\", \"y\");\
    x.test();\
    x = {};\
")
gc()
RegExp.$6
