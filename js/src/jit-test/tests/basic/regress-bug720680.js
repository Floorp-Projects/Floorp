// |jit-test| error: InternalError
version(0);
eval("\
function TimeFromYear( y ) {}\
addTestCase( -2208988800000 );\
function addTestCase( t ) {\
  var start = TimeFromYear((addTestCase(addTestCase << t, 0)));\
    new TestCase( \
                  SECTION,\
                  '(new Date('+d+')).getUTCDay()',\
                  WeekDay((d)),\
                  (new Date(foo ({ stop } = 'properties.length' )('/ab[c\\\n]/'))).getUTCDay() \
                );\
}\
");
