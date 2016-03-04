var gImage1;
var gImage2;
var gImageBitmap1;
var gImageBitmap2;

// Bug 1239752.
var gImageData;
var gImageBitmap3;

function comparePixelColor(testImgageData, groundTruthImageData, x, y, tolerance, info) {
  ok(testImgageData.width == groundTruthImageData.width && testImgageData.height == groundTruthImageData.height,
     "testImgageData and groundTruthImageData should have the same dimension.");

  var index = (groundTruthImageData.width * y + x) * 4;
  var r = groundTruthImageData.data[index + 0];
  var g = groundTruthImageData.data[index + 1];
  var b = groundTruthImageData.data[index + 2];
  var a = groundTruthImageData.data[index + 3];
  var newR = testImgageData.data[index + 0];
  var newG = testImgageData.data[index + 1];
  var newB = testImgageData.data[index + 2];
  var newA = testImgageData.data[index + 3];
  var isTheSame = Math.abs(r - newR) <= tolerance &&
                  Math.abs(g - newG) <= tolerance &&
                  Math.abs(b - newB) <= tolerance &&
                  Math.abs(a - newA) <= tolerance;
  ok(isTheSame, "[" + info + "] " +
                "newImageData(" + newR + "," + newG + "," + newB + "," + newA +
                ") should equal to imageData(" + r + "," + g + "," + b + "," + a + ").");
}

function compareImageBitmapWithImageElement(imageBitmap, imageElement) {
  var canvas1 = document.createElement('canvas');
  var canvas2 = document.createElement('canvas');

  canvas1.width  = imageElement.naturalWidth;
  canvas1.height = imageElement.naturalHeight;
  canvas2.width  = imageElement.naturalWidth;
  canvas2.height = imageElement.naturalHeight;

  var ctx1 = canvas1.getContext('2d');
  var ctx2 = canvas2.getContext('2d');

  ctx1.drawImage(imageElement, 0, 0);
  ctx2.drawImage(imageBitmap, 0, 0);

  document.body.appendChild(canvas1);
  document.body.appendChild(canvas2);

  var imageData1 = ctx1.getImageData(0, 0, canvas1.width, canvas1.height);
  var imageData2 = ctx2.getImageData(0, 0, canvas2.width, canvas2.height);

  // Create an array of pixels that is going to be tested.
  var pixels = [];
  var xGap = imageElement.naturalWidth / 4;
  var yGap = imageElement.naturalHeight / 4;
  for (var y = 0; y < imageElement.naturalHeight; y += yGap) {
    for (var x = 0; x < imageElement.naturalWidth; x += xGap) {
      pixels.push({"x":x, "y":y});
    }
  }

  // Also, put the button-right pixel into pixels.
  pixels.push({"x":imageElement.naturalWidth-1, "y":imageElement.naturalHeight-1});

  // Do the test.
  for (var pixel of pixels) {
    comparePixelColor(imageData2, imageData1, pixel.x, pixel.y, 0);
  }
}

function compareImageBitmapWithImageData(imageBitmap, imageData, info) {
  var canvas1 = document.createElement('canvas');

  canvas1.width  = imageBitmap.width;
  canvas1.height = imageBitmap.height;

  var ctx1 = canvas1.getContext('2d');

  ctx1.drawImage(imageBitmap, 0, 0);

  document.body.appendChild(canvas1);

  var imageData1 = ctx1.getImageData(0, 0, canvas1.width, canvas1.height);

  // Create an array of pixels that is going to be tested.
  var pixels = [];
  var xGap = imageBitmap.width / 4;
  var yGap = imageBitmap.height / 4;
  for (var y = 0; y < imageBitmap.height; y += yGap) {
    for (var x = 0; x < imageBitmap.width; x += xGap) {
      pixels.push({"x":x, "y":y});
    }
  }

  // Also, put the button-right pixel into pixels.
  pixels.push({"x":imageBitmap.width-1, "y":imageBitmap.height-1});

  // Do the test.
  for (var pixel of pixels) {
    comparePixelColor(imageData1, imageData, pixel.x, pixel.y, 5, info);
  }
}

function prepareImageBitmaps() {
  gImage1 = document.createElement('img');
  gImage2 = document.createElement('img');
  gImage1.src = "image_rgrg-256x256.png";
  gImage2.src = "image_yellow.png";

  var p1 = new Promise(function(resolve, reject) {
    gImage1.onload = function() {
      var promise = createImageBitmap(gImage1);
      promise.then(function(bitmap) {
        gImageBitmap1 = bitmap;
        resolve(true);
      });
    }
  });

  var p2 = new Promise(function(resolve, reject) {
    gImage2.onload = function() {
      var promise = createImageBitmap(gImage2);
      promise.then(function(bitmap) {
        gImageBitmap2 = bitmap;
        resolve(true);
      });
    }
  });

  var p3 = new Promise(function(resolve, reject) {
    // Create an ImageData with random colors.
    var width = 5;
    var height = 10;
    var data = [43,143,24,148,    235,165,179,91,   74,228,75,195,    141,109,74,65,    25,177,3,201,
                128,105,12,199,   196,93,241,131,   250,121,232,189,  175,131,216,190,  145,123,167,70,
                18,196,210,162,   225,1,90,188,     223,216,182,233,  118,50,168,56,    51,206,198,199,
                153,29,70,130,    180,135,135,51,   148,46,44,144,    80,171,142,95,    25,178,102,110,
                0,28,128,91,      31,222,42,170,    85,8,218,146,     65,30,198,238,    121,57,124,88,
                246,40,141,146,   174,195,255,149,  30,153,92,116,    18,241,6,111,     39,162,85,143,
                237,159,201,244,  93,68,14,246,     143,143,83,221,   187,215,243,154,  24,125,221,53,
                80,153,151,219,   202,241,250,191,  153,129,181,57,   94,18,136,231,    41,252,168,207,
                213,103,118,172,  53,213,184,204,   25,29,249,199,    101,55,49,167,    25,23,173,78,
                19,234,205,155,   250,175,44,201,   215,92,25,59,     25,29,249,199,    153,129,181,57];

    gImageData = new ImageData(new Uint8ClampedArray(data), width, height);

    // Create an ImageBitmap from the above ImageData.
    createImageBitmap(gImageData).then(
      (bitmap) => { gImageBitmap3 = bitmap; resolve(true); },
      () => { reject(); }
    );

  });

  return Promise.all([p1, p2, p3]);
}