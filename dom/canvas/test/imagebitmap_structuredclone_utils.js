var gImage1;
var gImage2;
var gImageBitmap1;
var gImageBitmap2;

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

  return Promise.all([p1, p2]);
}