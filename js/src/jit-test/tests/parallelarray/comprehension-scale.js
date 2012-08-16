
function buildComprehension() {
  var H = 96;
  var W = 96;
  var d = 4;
  // 3D 96x96x4 texture-like PA
  var p = new ParallelArray([H,W,d], function (i,j,k) { return i + j + k; });
  var a = "<";
  for (var i = 0; i < H; i++) {
    a += "<";
    for (var j = 0; j < W; j++) {
      a += "<";
      for (var k = 0; k < d; k++) {
        a += i+j+k;
        if (k !== d - 1)
          a += ",";
      }
      a += ">";
      if (j !== W - 1)
        a += ","
    }
    a += ">";
    if (i !== H - 1)
      a += ","
  }
  a += ">"

  assertEq(p.toString(), a);
}

buildComprehension();
