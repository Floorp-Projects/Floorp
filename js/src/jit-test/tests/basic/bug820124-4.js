// Don't assert.
eval("\
    r = RegExp(\"(?!()(((!))))\", \"g\");\
    \"^\".replace(r, '');\
    r = (\"1+\")\
")
gc()
RegExp.$8
