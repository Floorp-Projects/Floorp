// |trace-test| slow;

// XXXbz I would dearly like to wrap it up into a function to avoid polluting
// the global scope, but the function ends up heavyweight, and then we lose on
// the jit.
load(libdir + "mandelbrot-results.js");
//function testMandelbrotAll() {
  // Configuration options that affect which codepaths we follow.
  var doImageData = true;
  var avoidSparseArray = true;

  // Control of iteration numbers and sizing.  We'll do
  // scaler * colorNames.length iterations or so before deciding that we
  // don't escape.
  const scaler = 5;
  const numRows = 600;
  const numCols = 600;

  const colorNames = [
    "black",
    "green",
    "blue",
    "red",
    "purple",
    "orange",
    "cyan",
    "yellow",
    "magenta",
    "brown",
    "pink",
    "chartreuse",
    "darkorange",
    "crimson",
    "gray",
    "deeppink",
    "firebrick",
    "lavender",
    "lawngreen",
    "lightsalmon",
    "lime",
    "goldenrod"
  ];
  const threshold = (colorNames.length - 1) * scaler;

  // Now set up our colors
  var colors = [];
  // 3-part for loop (iterators buggy, we will add a separate test for them)
  for (var colorNameIdx = 0; colorNameIdx < colorNames.length; ++colorNameIdx) {
  //for (var colorNameIdx in colorNames) {
    colorNameIdx = parseInt(colorNameIdx);
    colors.push([colorNameIdx, colorNameIdx, colorNameIdx, 0]);
  }

  // Storage for our point data
  var points;

  var scratch = {};
  var scratchZ = {};
  function complexMult(a, b) {
    var newr = a.r * b.r - a.i * b.i;
    var newi = a.r * b.i + a.i * b.r;
    scratch.r = newr;
    scratch.i = newi;
    return scratch;
  }
  function complexAdd(a, b) {
    scratch.r = a.r + b.r;
    scratch.i = a.i + b.i;
    return scratch;
  }
  function abs(a) {
    return Math.sqrt(a.r * a.r + a.i * a.i);
  }

  function escapeAbsDiff(normZ, absC) {
    var absZ = Math.sqrt(normZ);
    return normZ > absZ + absC;
  }

  function escapeNorm2(normZ) {
    return normZ > 4;
  }

  function fuzzyColors(i) {
    return Math.floor(i / scaler) + 1;
  }

  function moddedColors(i) {
    return (i % (colorNames.length - 1)) + 1;
  }

  function computeEscapeSpeedObjects(real, imag) {
    var c = { r: real, i: imag }
    scratchZ.r = scratchZ.i = 0;
    var absC = abs(c);
    for (var i = 0; i < threshold; ++i) {
      scratchZ = complexAdd(c, complexMult(scratchZ, scratchZ));
      if (escape(scratchZ.r * scratchZ.r + scratchZ.i * scratchZ.i,
                 absC)) {
        return colorMap(i);
      }
    }
    return 0;
  }

  function computeEscapeSpeedOneObject(real, imag) {
    // fold in the fact that we start with 0
    var r = real;
    var i = imag;
    var absC = abs({r: real, i: imag});
    for (var j = 0; j < threshold; ++j) {
      var r2 = r * r;
      var i2 = i * i;
      if (escape(r2 + i2, absC)) {
        return colorMap(j);
      }
      i = 2 * r * i + imag;
      r = r2 - i2 + real;
    }
    return 0;
  }

  function computeEscapeSpeedDoubles(real, imag) {
    // fold in the fact that we start with 0
    var r = real;
    var i = imag;
    var absC = Math.sqrt(real * real + imag * imag);
    for (var j = 0; j < threshold; ++j) {
      var r2 = r * r;
      var i2 = i * i;
      if (escape(r2 + i2, absC)) {
        return colorMap(j);
      }
      i = 2 * r * i + imag;
      r = r2 - i2 + real;
    }
    return 0;
  }

  var computeEscapeSpeed = computeEscapeSpeedDoubles;
  var escape = escapeNorm2;
  var colorMap = fuzzyColors;

  function addPointOrig(pointArray, n, i, j) {
    if (!points[n]) {
      points[n] = [];
      points[n].push([i, j, 1, 1]);
    } else {
      var point = points[n][points[n].length-1];
      if (point[0] == i && point[1] == j - point[3]) {
        ++point[3];
      } else {
        points[n].push([i, j, 1, 1]);
      }
    }
  }

  function addPointImagedata(pointArray, n, col, row) {
    var slotIdx = ((row * numCols) + col) * 4;
    pointArray[slotIdx] = colors[n][0];
    pointArray[slotIdx+1] = colors[n][1];
    pointArray[slotIdx+2] = colors[n][2];
    pointArray[slotIdx+3] = colors[n][3];
  }

  function createMandelSet() {
    var realRange = { min: -2.1, max: 1 };
    var imagRange = { min: -1.5, max: 1.5 };

    var addPoint;
    if (doImageData) {
      addPoint = addPointImagedata;
      points = new Array(4*numCols*numRows);
      if (avoidSparseArray) {
        for (var idx = 0; idx < 4*numCols*numRows; ++idx) {
          points[idx] = 0;
        }
      }
    } else {
      addPoint = addPointOrig;
      points = [];
    }
    var realStep = (realRange.max - realRange.min)/numCols;
    var imagStep = (imagRange.min - imagRange.max)/numRows;
    for (var i = 0, curReal = realRange.min;
         i < numCols;
         ++i, curReal += realStep) {
      for (var j = 0, curImag = imagRange.max;
           j < numRows;
           ++j, curImag += imagStep) {
        var n = computeEscapeSpeed(curReal, curImag);
        addPoint(points, n, i, j)
      }
    }
    var result;
    if (doImageData) {
      if (colorMap == fuzzyColors) {
        result = mandelbrotImageDataFuzzyResult;
      } else {
        result = mandelbrotImageDataModdedResult;
      }
    } else {
      result = mandelbrotNoImageDataResult;
    }
    return points.toSource() == result;
  }

  const escapeTests = [ escapeAbsDiff ];
  const colorMaps = [ fuzzyColors, moddedColors ];
  const escapeComputations = [ computeEscapeSpeedObjects,
                               computeEscapeSpeedOneObject,
                               computeEscapeSpeedDoubles ];
  // Test all possible escape-speed generation codepaths, using the
  // imageData + sparse array avoidance storage.
  doImageData = true;
  avoidSparseArray = true;
  for (var escapeIdx in escapeTests) {
    escape = escapeTests[escapeIdx];
    for (var colorMapIdx in colorMaps) {
      colorMap = colorMaps[colorMapIdx];
      for (var escapeComputationIdx in escapeComputations) {
        computeEscapeSpeed = escapeComputations[escapeComputationIdx];
	assertEq(createMandelSet(), true);
      }
    }
  }

  // Test all possible storage strategies. Note that we already tested
  // doImageData == true with avoidSparseArray == true.
  escape = escapeAbsDiff;
  colorMap = fuzzyColors; // This part doesn't really matter too much here
  computeEscapeSpeed = computeEscapeSpeedDoubles;

  doImageData = true;
  avoidSparseArray = false;
  assertEq(createMandelSet(), true);

  escape = escapeNorm2;
  doImageData = false;  // avoidSparseArray doesn't matter here
  assertEq(createMandelSet(), true);
//}
//testMandelbrotAll();
