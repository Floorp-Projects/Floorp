var RGBAValues = [[42,142,23,148],
                  [234,165,177,91],
                  [74,228,75,195],
                  [140,108,73,65],
                  [25,177,3,201],
                  [127,104,12,199],
                  [196,93,240,131],
                  [250,121,231,189],
                  [175,131,215,190],
                  [145,122,166,70],
                  [18,196,210,162],
                  [225,1,90,188],
                  [223,216,182,233],
                  [115,48,168,56],
                  [50,206,198,199],
                  [152,28,70,130],
                  [176,134,133,51],
                  [148,46,43,144],
                  [78,171,141,95],
                  [24,177,102,110],
                  [0,27,127,91],
                  [31,221,41,170],
                  [85,7,218,146],
                  [65,30,198,238],
                  [121,56,123,88],
                  [246,39,140,146],
                  [174,195,254,149],
                  [29,153,92,116],
                  [17,240,5,111],
                  [38,162,84,143],
                  [237,159,201,244],
                  [93,68,14,246],
                  [143,142,82,221],
                  [187,215,243,154],
                  [24,121,220,53],
                  [80,153,151,219],
                  [202,241,250,191]];

function createOneTest(rgbaValue) {
  return new Promise(function(resolve, reject) {
    var tolerance = 5;
    var r = rgbaValue[0];
    var g = rgbaValue[1];
    var b = rgbaValue[2];
    var a = rgbaValue[3];
    var imageData = new ImageData(new Uint8ClampedArray([r, g, b, a]), 1, 1);

    var newImageData;
    createImageBitmap(imageData).then(
      function(imageBitmap) {
        var context = document.createElement("canvas").getContext("2d");
        context.drawImage(imageBitmap, 0, 0);
        newImageData = context.getImageData(0, 0, 1, 1);
        var newR = newImageData.data[0];
        var newG = newImageData.data[1];
        var newB = newImageData.data[2];
        var newA = newImageData.data[3];
        var isTheSame = Math.abs(r - newR) <= tolerance &&
                        Math.abs(g - newG) <= tolerance &&
                        Math.abs(b - newB) <= tolerance &&
                        Math.abs(a - newA) <= tolerance;
        ok(isTheSame, "newImageData(" + newR + "," + newG + "," + newB + "," + newA +
                      ") should equal to imageData(" + r + "," + g + "," + b + "," + a + ")." +
                      "Premultiplied Alpha is handled while creating ImageBitmap from ImageData.");
        if (isTheSame) {
          resolve();
        } else {
          reject();
        }
      },
      function() {
        reject();
      }
    );
  });
}

function testBug1239752() {
  var tests = [];
  for (var i = 0; i < RGBAValues.length; ++i) {
    tests.push(createOneTest(RGBAValues[i]));
  }

  return Promise.all(tests);
}