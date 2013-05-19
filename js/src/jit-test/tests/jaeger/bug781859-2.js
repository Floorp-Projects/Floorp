Function("\
    switch (/x/) {\
        case 8:\
        break;\
        t(function(){})\
    }\
    while (false)(function(){})\
")()
