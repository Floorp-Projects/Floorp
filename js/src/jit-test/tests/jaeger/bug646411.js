__proto__ = Function();
eval("\
var MS = 16;\
addNewTestCase(new Date(1899,11,31,16,0,0), \"new Date(1899,11,31,16,0,0)\",\
typeof UTC_DAY == 'undefined');\
function addNewTestCase( DateCase, DateString, ResultArray ) {\
        ResultArray[MS];\
}\
");
