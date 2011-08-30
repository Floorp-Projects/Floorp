var gTestcases = new Array;
var gTc = gTestcases;
function TestCase(n, d, e, a) {
  this.description=d
  this.reason=''
  gTestcases[gTc++]=this
}
TestCase.prototype.dump=function () + toPrinted(this.description) + toPrinted(this.reason) + '\n';
function toPrinted(value) value=value.replace(/\\n/g, 'NL').replace(/[^\x20-\x7E]+/g, escapeString);
function escapeString (str) {
  try {
     err
  } catch(ex) { }
}
function jsTestDriverEnd() {
  for (var i = 0; i < gTestcases.length; i++)
  gTestcases[i].dump()
}
var SECTION = "dowhile-007";
DoWhile();
function DoWhile( object ) result1=false;
new TestCase(
    SECTION,
    "break one: ",
    result1 
);
jsTestDriverEnd();
new TestCase( SECTION, "'�O� �:i��'.match(new RegExp('.+'))", [], '�O� �:i��');
jsTestDriverEnd();
