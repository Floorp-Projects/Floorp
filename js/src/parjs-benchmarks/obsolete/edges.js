load(libdir + "util.js");

var SobelX = [[-1.0,  0.0,  1.0],
              [-2.0,  0.0,  2.0],
              [-1.0,  0.0,  1.0]];
var SobelY = [[ 1.0,  2.0,  1.0],
              [ 0.0,  0.0,  0.0],
              [-1.0, -2.0, -1.0]];

function stripedImage(w, h) {
  var resultArray = new Array(w * h);
  for (var y = 0; y < h; y++) {
    for (var x = 0; x < w; x++) {
      resultArray[y*w + x] = (Math.abs(x%100-y%100) < 10) ? 32 : 0;
    }
  }
  return resultArray;
}

function edgesSequentially(data, width, height) {
  var data1 = new Array(width * height);
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      var total = compute(x, y);
      data1[y*width + x] = total | 0;
    }
  }
  return data1;

  function compute(x, y) {
      var totalX = 0;
      var totalY = 0;

      var offYMin = (y == 0 ? 0 : -1);
      var offYMax = (y == height - 1 ? 0 : 1);
      var offXMin = (x == 0 ? 0 : -1);
      var offXMax = (x == width - 1 ? 0 : 1);

      for (var offY = offYMin; offY <= offYMax; offY++) {
        for (var offX = offXMin; offX <= offXMax; offX++) {
          var e = data[(y + offY) * width + x + offX];
          totalX += e * SobelX[offY + 1][offX + 1];
          totalY += e * SobelY[offY + 1][offX + 1];
        }
      }

    return (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
  }
}

function edgesParallel(data) {
  var pa = new ParallelArray([Height, Width], function (y, x) {
    var totalX = 0;
    var totalY = 0;

    var offYMin = (y == 0 ? 0 : -1);
    var offYMax = (y == Height - 1 ? 0 : 1);
    var offXMin = (x == 0 ? 0 : -1);
    var offXMax = (x == Width - 1 ? 0 : 1);

    for (var offY = offYMin; offY <= offYMax; offY++) {
      for (var offX = offXMin; offX <= offXMax; offX++) {
        var e = data.get(y + offY, x + offX);
        totalX += e * SobelX[offY + 1][offX + 1];
        totalY += e * SobelY[offY + 1][offX + 1];
      }
    }

    return (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
  });
  return pa.flatten();
}

var Width = 1024;
var Height = 768;
var ArrInput = stripedImage(Width, Height);
var ParArrInput2D = new ParallelArray([Height, Width],
                                      function (y, x) ArrInput[y*Width + x]);

benchmark("EDGES", 2, DEFAULT_MEASURE * 20,
          function() edgesSequentially(ArrInput, Width, Height),
          function() edgesParallel(ParArrInput2D));
