// Binary: cache/js-dbg-32-b4fee3813956-linux
// Flags: -j
//
var aej=new Array( 'A3|297|420', 'dummy|1|1' );
var aes=aej.length-1,
aet=new Array();
for(var i=0; i <= aes; ++i)
  aet[i] = aej[i].split('|');
afo=4;
R=new Array(
  '17523|2500|275||',
  '17524|5000|300.3||',
  '17535|500|207.4|=|120x120|=|=|=|=|=|=|=',
  '17556|500|349.3|=|A5|=|=|=|=|=|=|='
);
var ags = R.length-1;
px= new Array();
for(var i=1; i<=ags; i++) {
  px[i] = R[i].split('|');
  for(var j=0; j<=11;j++)
    agt = ank(px[i][afo]);
}
function ank(akr) {
  var XX=YY=0;
  for(var i=0;i<aet.length;i++)
    if((XX==0)  &&  (akr.indexOf('x')>0)) {
      var tt=akr.split('x');
      XX=tt[0];YY=tt[1]
    }
}
