load(libdir + "util.js");

function stripedImage(w, h) {
  var resultArray = new Array(w * h);
  for (var x = 0; x < w; x++) {
    for (var y = 0; y < h; y++) {
      resultArray[x*h + y] = (Math.abs(x%100-y%100) < 10) ? 32 : 0;
    }
  }
  return resultArray;
}

function edges1dArraySequentially(data, width, height) {
  var sobelX =  [[-1.0,  0.0, 1.0],
                 [-2.0, 0.0, 2.0],
                 [-1.0, 0.0, 1.0]];
  var sobelY = [[1.0,  2.0, 1.0],
                [0.0, 0.0, 0.0],
                [-1.0, -2.0, -1.0]];

  var data1 = new Array(width * height);
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      // process pixel
      var totalX = 0;
      var totalY = 0;
      for (var offY = -1; offY <= 1; offY++) {
        var newY = y + offY;
        for (var offX = -1; offX <= 1; offX++) {
          var newX = x + offX;
          if ((newX >= 0) && (newX < width) && (newY >= 0) && (newY < height)) {
            var pointIndex = (x + offX) * height + (y + offY);
            var e = data[pointIndex];
            totalX += e * sobelX[offY + 1][offX + 1];
            totalY += e * sobelY[offY + 1][offX + 1];
          }
        }
      }
      var total = ((Math.abs(totalX) + Math.abs(totalY))/8.0)|0;
      var index = y*width+x;
      data1[index] = total | 0;
    }
  }
  return data1;
}

/* Compute edges using a flat JS array as input */
function edges1dArrayInParallel(data, width, height) {
  var sobelX = [[-1.0,  0.0, 1.0],
                [-2.0, 0.0, 2.0],
                [-1.0, 0.0, 1.0]];
  var sobelY = [[1.0,  2.0, 1.0],
                [0.0, 0.0, 0.0],
                [-1.0, -2.0, -1.0]];

  function computePixel(x, y) {
      var totalX = 0;
      var totalY = 0;
      for (var offY = -1; offY <= 1; offY++) {
        var newY = y + offY;
        for (var offX = -1; offX <= 1; offX++) {
          var newX = x + offX;
          if ((newX >= 0) && (newX < width) && (newY >= 0) && (newY < height)) {
            var pointIndex = (x + offX) * height + (y + offY);
            var e = data[pointIndex];
            totalX += e * sobelX[offY + 1][offX + 1];
            totalY += e * sobelY[offY + 1][offX + 1];
          }
        }
      }
      var total = (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
      return total;
  }

  return new ParallelArray([width, height], computePixel);
}

/* Compute edges using a flat JS array as input */
function edges1dParallelArrayInParallel(pa, width, height) {
  var sobelX = [[-1.0,  0.0, 1.0],
                [-2.0, 0.0, 2.0],
                [-1.0, 0.0, 1.0]];
  var sobelY = [[1.0,  2.0, 1.0],
                [0.0, 0.0, 0.0],
                [-1.0, -2.0, -1.0]];

  function computePixel(x, y) {
      var totalX = 0;
      var totalY = 0;
      for (var offY = -1; offY <= 1; offY++) {
        var newY = y + offY;
        for (var offX = -1; offX <= 1; offX++) {
          var newX = x + offX;
          if ((newX >= 0) && (newX < width) && (newY >= 0) && (newY < height)) {
            var pointIndex = (x + offX) * height + (y + offY);
            // var e = pa.buffer[pointIndex];
            var e = pa.get(pointIndex);
            totalX += e * sobelX[offY + 1][offX + 1];
            totalY += e * sobelY[offY + 1][offX + 1];
          }
        }
      }
      var total = (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
      return total;
  }

  return new ParallelArray([width, height], computePixel);
}

/* Compute edges using 2D parallel array as input */
function edges2dParallelArrayInParallel(pa) {
  var sobelX = [[-1.0,  0.0, 1.0],
                [-2.0, 0.0, 2.0],
                [-1.0, 0.0, 1.0]];
  var sobelY = [[1.0,  2.0, 1.0],
                [0.0, 0.0, 0.0],
                [-1.0, -2.0, -1.0]];

  var width = pa.shape[0];
  var height = pa.shape[1];

  function computePixel(x, y) {
      var totalX = 0;
      var totalY = 0;
      for (var offY = -1; offY <= 1; offY++) {
        var newY = y + offY;
        for (var offX = -1; offX <= 1; offX++) {
          var newX = x + offX;
          if ((newX >= 0) && (newX < width) && (newY >= 0) && (newY < height)) {
            var pointIndex = (x + offX) * height + (y + offY);
            // var e = pa.buffer[pointIndex];
            var e = pa.get(x + offX, y + offY);
            totalX += e * sobelX[offY + 1][offX + 1];
            totalY += e * sobelY[offY + 1][offX + 1];
          }
        }
      }
      var total = (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
      return total;
  }

  return new ParallelArray([width, height], computePixel);
}

var Width = 1024;
var Height = 768;
var ArrInput = stripedImage(1024, 768);
var ParArrInput1D = new ParallelArray(ArrInput);
var ParArrInput2D = new ParallelArray([1024, 768],
                                      function (x, y) ArrInput[x*Height + y]);


