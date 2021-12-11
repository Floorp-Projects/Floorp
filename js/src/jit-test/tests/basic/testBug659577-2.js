gczeal(4);
evaluate("\
Date.formatFunctions = {count:0};\
Date.prototype.dateFormat = function(format) {\
    var funcName = 'format' + Date.formatFunctions.count++;\
    var code = 'Date.prototype.' + funcName + ' = function(){return ';\
    var ch = '';\
    for (var i = 0; i < format.length; ++i) {\
        ch = format.charAt(i);\
        eval(code.substring(0, code.length - 3) + ';}');\
    }\
};\
var date = new Date('1/1/2007 1:11:11');\
    var shortFormat = date.dateFormat('Y-m-d');\
");
