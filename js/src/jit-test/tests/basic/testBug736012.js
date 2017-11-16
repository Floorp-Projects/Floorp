evaluate("\
function fatty() {\
    try { fatty(); } catch (e) {\
        for (foo of [1]) {}\
    }\
}\
fatty();\
");
