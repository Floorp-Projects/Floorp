
const numRows = 600;
const numCols = 600;
function computeEscapeSpeed(c) {
  const scaler = 5;
  const threshold = (colors.length - 1) * scaler + 1;
  for (var i = 1; i < threshold; ++i) {}
}
const colorStrings = [ "deeppink", ];
var colors = [];
function createMandelSet(realRange, imagRange) {
    for each (var color in colorStrings) {
        var [r, g, b] = [0, 0, 0];
        colors.push([r, g, b, 0xff]);
      }
    var realStep = (realRange.max - realRange.min)/numCols;
    var imagStep = (imagRange.min - imagRange.max)/(function    (colors) {} )     ;
    for (var i = 0, curReal = realRange.min; i < numCols; ++i, curReal += realStep) {
      for (var j = 0, curImag = imagRange.max; j < numRows; ++j, curImag += imagStep) {
        var c = { r: curReal, i: curImag }
        var n = computeEscapeSpeed(c);
      }
    }
  }
var realRange = { min: -2.1, max: 2 };
var imagRange = { min: -2, max: 2 };
createMandelSet(realRange, imagRange);
