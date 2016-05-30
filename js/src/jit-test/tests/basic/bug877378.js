evalcx("\
    let z = 'x';\
    for (var v of z) {\
        y = evaluate(\"Object.defineProperty(this,\\\"y\\\",\
                         {get:  function() {}}\
                    );\",\
               {catchTermination: true}\
        );\
    }",
    newGlobal('')
)

evalcx("\
    for (x = 0; x < 1; ++x) { \
        v = evaluate(\"gc\",{ \
        })\
    }\
", newGlobal(''));
