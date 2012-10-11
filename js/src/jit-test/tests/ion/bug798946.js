// Don't assert with --ion-eager.
function RandBool() { var x = Math.random() >= 0.5; return x; }
var CHARS = "aaa";
function RandStr() {
  var c = Math.floor(Math.random() * CHARS.length);
}
function RandVal() { 
  return RandBool() ? RandStr() : RandStr(); 
}
function GenerateSpecPermutes(matchVals, resultArray) {
    var maxPermuteBody = (1 << matchVals.length) - 1;
    for(var bod_pm = 0; bod_pm <= maxPermuteBody; bod_pm++)
      for(var k = 0; k < matchVals.length; k++)
        var body = ((bod_pm & (1 << k)) > 0) ? null : RandVal();
}
GenerateSpecPermutes(["foo", "bar", "zing"]);
