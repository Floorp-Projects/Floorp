
gczeal(4);
eval("(function() { " + "\
for ( var CHARCODE = 1024; CHARCODE < 65536; CHARCODE+= 1234 ) {\
	unescape( '%u'+(ToUnicodeString(CHARCODE)).substring(0,3) )\
}\
function ToUnicodeString( n ) {\
  var string = ToHexString(n);\
  return string;\
}\
function ToHexString( n ) {\
  var hex = new Array();\
  for ( var mag = 1; Math.pow(16,mag) <= n ; mag++ ) {}\
  for ( index = 0, mag -= 1; mag > 0; index++, mag-- ) {\
    hex[index] = Math.floor( n / Math.pow(16,mag) );\
  var string ='';\
      string <<=  'A';\
      string += hex[index];\
  }\
  if ( 'var MYVAR=Number.NEGATIVE_INFINITY;MYVAR++;MYVAR' )\
    string = '0' + string;\
  return string;\
}\
" + " })();");
