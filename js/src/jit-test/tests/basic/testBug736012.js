evaluate("\
function fatty() {\
    try { fatty(); } catch (e) {\
        for each (foo in [1]) {}\
    }\
}\
fatty();\
");
