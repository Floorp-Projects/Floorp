// -*- mode: js2; indent-tabs-mode: nil; -*-

// Adapted from
//
// https://github.com/RiverTrail/RiverTrail/blob/master/examples/liquid-resize/resize-compute-dp.js
//
// which in turn is based on an algorithm from the paper below (which
// also appeared in ACM SIGGRAPH 2007):
// Shai Avidan and Ariel Shamir. 2007. Seam carving for content-aware image resizing.
// ACM Trans. Graph. 26, 3, Article 10 (July 2007).
// DOI=10.1145/1276377.1276390 http://doi.acm.org/10.1145/1276377.1276390

///////////////////////////////////////////////////////////////////////////
// Inputs

function buildArray(width, height, func) {
  var length = width * height;
  var array = new Array(length);
  var index = 0;
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      array[index++] = func(y, x);
    }
  }
  return array;
}

function parImage(seqImage, width, height) {
  return new ParallelArray([height, width], function (y, x) {
    return seqImage[y*width + x];
  });
}

var tinyImage =
  buildArray(20, 5, function(y, x) {
    var ret;
    if (6 <= x && x < 8 && 0 <= y && y < 4)
      ret = ".";
    else if ((x-15)*(x-15)+(y-1)*(y-1) < 2)
      ret = "^";
    else if ((x-20)*(x-20)+(y-3)*(y-3) < 2)
      ret = "%";
    else if ((x-1)*(x-1)+(y-3)*(y-3) < 2)
      ret = "@";
    else
      ret = " ";
    return ret.charCodeAt(0) - 32;
  });

var SmallImageWidth = 60;
var SmallImageHeight = 15;
var SmallImage =
  buildArray(SmallImageWidth, SmallImageHeight, function(y, x) {
    var ret;
    if (6 <= x && x < 8 && 0 <= y && y < 7)
      ret = ".";
    else if ((x-15)*(x-15)+(y-1)*(y-1) < 2)
      ret = "^";
    else if ((x-40)*(x-40)+(y-6)*(y-6) < 6)
      ret = "%";
    else if ((x-1)*(x-1)+(y-12)*(y-12) < 2)
      ret = "@";
    else
      ret = " ";
    return ret.charCodeAt(0) - 32;
  });

var SmallImagePar = parImage(SmallImage,
                             SmallImageWidth,
                             SmallImageHeight);

var bigImage =
  buildArray(200, 70, function(y, x) {
    var ret;
    if (4 <= x && x < 7 && 10 <= y && y < 40)
      ret = ".";
    else if ((x-150)*(x-150)+(y-13)*(y-13) < 70)
      ret = "^";
    else if ((x-201)*(x-201)+(y-33)*(y-33) < 200)
      ret = "%";
    else if ((x-15)*(x-15)+(y-3)*(y-3) < 7)
      ret = "@";
    else
      ret = " ";
    return ret.charCodeAt(0) - 32;
  });

// randomImage: Nat Nat Nat Nat -> RectArray
function randomImage(w, h, sparsity, variety) {
  return buildArray(w, h, function (x,y) {
    if (Math.random() > 1/sparsity)
      return 0;
    else
      return 1+Math.random()*variety|0;
  });
}

// stripedImage: Nat Nat -> RectArray
function stripedImage(w, h) {
  return buildArray(w, h, function (y, x) {
    return (Math.abs(x%100-y%100) < 10) ? 32 : 0;
  });
}

var massiveImage =
  buildArray(70, 10000, function(y, x) (Math.abs(x%100-y%100) < 10) ? 32 : 0);

function printImage(array, width, height) {
  print("Width", width, "Height", height);
  for (var y = 0; y < height; y++) {
    var line = "";
    for (var x = 0; x < width; x++) {
      var c = array[y*width + x];
      line += String.fromCharCode(c + 32);
    }
    print(line);
  }
}

///////////////////////////////////////////////////////////////////////////
// Common

var SobelX = [[-1.0,  0.0,  1.0],
              [-2.0,  0.0,  2.0],
              [-1.0,  0.0,  1.0]];
var SobelY = [[ 1.0,  2.0,  1.0],
              [ 0.0,  0.0,  0.0],
              [-1.0, -2.0, -1.0]];

// computeEnergy: Array -> RectArray
//
// (The return type is forced upon us, for now at least, until we add
// appropriate API to ParallelArray; there's a dependency from each
// row upon its predecessor, but the contents of each row could be
// computed in parallel.)
function computeEnergy(source, width, height) {
  var energy = new Array(width * height);
  energy[0] = 0;
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      var e = source[y*width + x];
      if (y >= 1) {
        var p = energy[(y-1)*width + x];
        if (x > 0) {
          p = Math.min(p, energy[(y-1)*width + x-1]);
        }
        if (x < (width - 1)) {
          p = Math.min(p, energy[(y-1)*width + x+1]);
        }
        e += p;
      }
      energy[y*width + x] = e;
    }
  }
  return energy;
}

// findPath: RectArray -> Array
// (This is inherently sequential.)
function findPath(energy, width, height)
{
  var path = new Array(height);
  var y = height - 1;
  var minPos = 0;
  var minEnergy = energy[y*width + minPos];

  for (var x = 1; x < width; x++) {
    if (energy[y+width + x] < minEnergy) {
      minEnergy = energy[y*width + x];
      minPos = x;
    }
  }

  path[y] = minPos;
  for (y = height - 2; y >= 0; y--) {
    minEnergy = energy[y*width + minPos];
    // var line = energy[y]
    var p = minPos;
    if (p >= 1 && energy[y*width + p-1] < minEnergy) {
      minPos = p-1; minEnergy = energy[y*width + p-1];
    }
    if (p < width - 1 && energy[y*width + p+1] < minEnergy) {
      minPos = p+1; minEnergy = energy[y*width + p+1];
    }
    path[y] = minPos;
  }
  return path;
}

///////////////////////////////////////////////////////////////////////////
// Sequential

function transposeSeq(array, width, height) {
  return buildArray(height, width, function(y, x) {
    return array[x*width + y];
  });
}

// detectEdgesSeq: Array Nat Nat -> Array
function detectEdgesSeq(data, width, height) {
  var data1 = new Array(width * height);
  for (var y = 0; y < height; y++) {
    for (var x = 0; x < width; x++) {
      var total = compute(y, x);
      data1[y*width + x] = total | 0;
    }
  }
  return data1;

  function compute(y, x) {
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

function cutPathHorizontallyBWSeq(array, width, height, path) {
  return buildArray(width-1, height, function (y, x) {
    if (x < path[y]-1)
      return array[y*width + x];
    if (x == path[y]-1)
      return (array[y*width + x] + array[y*width + x+1]) / 2 | 0;
    else
      return array[y*width + x+1];
  });
}

function cutPathVerticallyBWSeq(array, width, height, path) {
  return buildArray(width, height-1, function (y, x) {
    if (y < path[x]-1)
      return array[y*width + x];
    if (y == path[x]-1)
      return (array[y*width + x] + array[(y+1)*width + x]) / 2 | 0;
    else
      return array[(y+1)*width + x];
  });
}

function cutHorizontalSeamBWSeq(array, width, height)
{
  var edges = detectEdgesSeq(array, width, height);
  var energy = computeEnergy(edges, width, height);
  var path = findPath(energy, width, height);
  edges = null; // no longer live
  return cutPathHorizontallyBWSeq(array, width, height, path);
}

function cutVerticalSeamBWSeq(array, width, height)
{
  var arrayT = transposeSeq(array, width, height);
  var edges = detectEdgesSeq(arrayT, height, width);
  var energy = computeEnergy(edges, height, width);
  var path = findPath(energy, height, width);
  edges = null; // no longer live
  return cutPathVerticallyBWSeq(array, width, height, path);
}

function reduceImageBWSeq(image,
                          width, height,
                          newWidth, newHeight,
                          intermediateFunc,
                          finalFunc) {
  while (width > newWidth || height > newHeight) {
    intermediateFunc(image, width, height);

    if (width > newWidth) {
      image = cutHorizontalSeamBWSeq(image, width, height);
      width -= 1;
    }

    if (height > newHeight) {
      image = cutVerticalSeamBWSeq(image, width, height);
      height -= 1;
    }
  }

  finalFunc(image, width, height);
  return image;
}

///////////////////////////////////////////////////////////////////////////
// Parallel

function transposePar(image) {
  var height = image.shape[0];
  var width = image.shape[1];
  return new ParallelArray([width, height], function (y, x) {
    return image.get(x, y);
  });
}

// detectEdgesSeq: Array Nat Nat -> Array
function detectEdgesPar(image) {
  var height = image.shape[0];
  var width = image.shape[1];
  return new ParallelArray([height, width], function (y, x) {
    var totalX = 0;
    var totalY = 0;

    var offYMin = (y == 0 ? 0 : -1);
    var offYMax = (y == height - 1 ? 0 : 1);
    var offXMin = (x == 0 ? 0 : -1);
    var offXMax = (x == width - 1 ? 0 : 1);

    for (var offY = offYMin; offY <= offYMax; offY++) {
      for (var offX = offXMin; offX <= offXMax; offX++) {
        var e = image.get(y + offY, x + offX);
        totalX += e * SobelX[offY + 1][offX + 1];
        totalY += e * SobelY[offY + 1][offX + 1];
      }
    }

    var result = (Math.abs(totalX) + Math.abs(totalY))/8.0 | 0;
    return result;
  });
}

function cutPathHorizontallyBWPar(image, path) {
  var height = image.shape[0];
  var width = image.shape[1];
  return new ParallelArray([height, width-1], function (y, x) {
    if (x < path[y]-1)
      return image.get(y, x);
    if (x == path[y]-1)
      return (image.get(y, x) + image.get(y, x+1)) / 2 | 0;
    else
      return image.get(y, x+1);
  });
}

function cutPathVerticallyBWPar(image, path) {
  var height = image.shape[0];
  var width = image.shape[1];
  return new ParallelArray([height-1, width], function (y, x) {
    if (y < path[x]-1)
      return image.get(y, x);
    if (y == path[x]-1)
      return (image.get(y, x) + image.get(y+1, x)) / 2 | 0;
    else
      return image.get(y+1, x);
  });
}

function cutHorizontalSeamBWPar(image)
{
  var height = image.shape[0];
  var width = image.shape[1];
  var edges = detectEdgesPar(image);
  var energy = computeEnergy(edges.buffer, width, height);
  var path = findPath(energy, width, height);
  edges = null; // no longer live
  return cutPathHorizontallyBWPar(image, path);
}

function cutVerticalSeamBWPar(image) {
  var height = image.shape[0];
  var width = image.shape[1];
  var imageT = transposePar(image);
  var edges = detectEdgesPar(imageT);
  var energy = computeEnergy(edges.buffer, height, width);
  var path = findPath(energy, height, width);
  edges = null; // no longer live
  return cutPathVerticallyBWPar(image, path);
}

function reduceImageBWPar(image,
                          newWidth, newHeight,
                          intermediateFunc,
                          finalFunc) {
  var height = image.shape[0];
  var width = image.shape[1];
  while (width > newWidth || height > newHeight) {
    intermediateFunc(image.buffer, width, height);

    if (width > newWidth) {
      image = cutHorizontalSeamBWPar(image);
      width -= 1;
    }

    if (height > newHeight) {
      image = cutVerticalSeamBWPar(image);
      height -= 1;
    }
  }

  finalFunc(image.buffer, width, height);
  return image.buffer;
}

///////////////////////////////////////////////////////////////////////////
// Benchmarking via run.sh

var BenchmarkImageWidth = 512;
var BenchmarkImageHeight = 256;
var BenchmarkImage = stripedImage(BenchmarkImageWidth,
                                  BenchmarkImageHeight);
var BenchmarkImagePar = parImage(BenchmarkImage,
                                 BenchmarkImageWidth,
                                 BenchmarkImageHeight);

var benchmarking = (typeof(MODE) != "undefined");
if (benchmarking) {
  load(libdir + "util.js");
  benchmark("LIQUID-RESIZE", 1, DEFAULT_MEASURE,
            function() {
              return reduceImageBWSeq(
                BenchmarkImage,
                BenchmarkImageWidth, BenchmarkImageHeight,
                BenchmarkImageWidth/2, BenchmarkImageHeight/2,
                function() {},
                function() {});
            },

            function() {
              return reduceImageBWPar(
                BenchmarkImagePar,
                BenchmarkImageWidth/2, BenchmarkImageHeight/2,
                function() {},
                function() {});
            });
}

///////////////////////////////////////////////////////////////////////////
// Running (sanity check)

if (!benchmarking) {
  var seqData =
    reduceImageBWSeq(SmallImage, SmallImageWidth, SmallImageHeight,
                     SmallImageWidth - 15, SmallImageHeight - 10,
                     function() {},
                     printImage);

  var parData =
    reduceImageBWPar(SmallImagePar,
                     SmallImageWidth - 15, SmallImageHeight - 10,
                     function() {},
                     printImage);

  assertEq(seqData, parData);
}
