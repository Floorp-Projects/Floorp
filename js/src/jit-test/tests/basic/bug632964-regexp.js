var sText = "s";

for (var i = 0; i < 250000; ++i)
    sText += 'a\n';

sText += 'e';

var start = new Date();
var match = sText.match(/s(\s|.)*?e/gi);
//var match = sText.match(/s([\s\S]*?)e/gi);
//var match = sText.match(/s(?:[\s\S]*?)e/gi);
var end = new Date();

assertEq(match.length, 1);
