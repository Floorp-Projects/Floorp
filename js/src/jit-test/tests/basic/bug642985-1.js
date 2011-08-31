gczeal(2);
function complex(aReal, aImag) {}
function mandelbrotValueOO (aC, aIterMax) {
  for (var iter = 0; iter < aIterMax; iter++) {  }
}
function f(trace) {
  const width = 5;
  const height = 5;
  const max_iters = 5;
  var output = [];
  for (let img_x = 0; img_x < width; img_x++) {
    for (let img_y = 0; img_y < height; img_y++) {
      let C = new complex(-2 + (img_x / width) * 3,
                          -1.5 + (img_y / height) * 3);
      var res = mandelbrotValueOO(C, max_iters);
      if (output.length > 0 && complex(5)) {
      } else {
        output.push([res, 1]);
      }
    }
  }
}
var timenonjit = f(false);
