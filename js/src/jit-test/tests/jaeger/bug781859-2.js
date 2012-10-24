mjitChunkLimit(42);
Function("\
    switch (/x/) {\
        case 8:\
        break;\
        t(function(){})\
    }\
    while (false)(function(){})\
")()
