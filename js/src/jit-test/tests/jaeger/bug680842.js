
var gTestcases = new Array;
var gTc = gTestcases;
var msg = '';
function TestCase(n, d, e, a) {
  gTestcases[gTc++]=this;
}
TestCase.prototype.dump=function () {
  lines = msg
  for (var i=0; i<lines; ) {  }
}
function writeHeaderToLog( string ) {
  for (var i = 0; ; i++) {
    gTestcases[i].dump();
  }
}
try {
  TIME_2000 = 946684800000
} catch (exc1) {}
addNewTestCase( Date, 999 );
try {
  addNewTestCase( Date,TIME_2000 )( writeHeaderToLog( 2000,0,1,0,0,0,0), 0 );
} catch (exc2) {}
function addNewTestCase( DateCase, DateString, ResultArray ) {
  new TestCase
  Date.prototype=new TestCase
}
