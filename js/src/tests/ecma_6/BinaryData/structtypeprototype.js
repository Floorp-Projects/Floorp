// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData StructType prototype chains';


function runTests() {
  var RgbColor1 = new StructType({r: uint8, g: uint8, b: uint8});
  var RgbColor2 = new StructType({r: uint8, g: uint8, b: uint8});
  var Fade1 = new StructType({from: RgbColor1, to: RgbColor1});
  var Fade2 = new StructType({from: RgbColor2, to: RgbColor2});

  // Available on all objects
  Data.prototype.sub = function(c) {
    this.r -= c;
    this.g -= c;
    this.b -= c;
  };

  // Available on `RgbColor2` instances only
  RgbColor2.prototype.add = function(c) {
    this.r += c;
    this.g += c;
    this.b += c;
  };

  var black = new RgbColor1({r: 0, g: 0, b: 0});
  var gray = new RgbColor2({r: 129, g: 128, b: 127});

  // `add` works on `RgbColor2`
  assertThrows(function() { black.add(1); });
  gray.add(1);
  assertEq(130, gray.r);
  assertEq(129, gray.g);
  assertEq(128, gray.b);

  // `add` fails (for both!) when accessed via `fade1`
  var fade1 = new Fade1({from: black, to: gray});
  assertThrows(function() { fade1.from.add(1); });
  assertThrows(function() { fade1.to.add(1); });

  // `sub` works on both
  black.sub(1);
  assertEq(black.r, 255);
  assertEq(black.g, 255);
  assertEq(black.b, 255);
  gray.sub(1);
  assertEq(gray.r, 129);
  assertEq(gray.g, 128);
  assertEq(gray.b, 127);

  // `add` works (for both!) when accessed via `fade2`
  var fade2 = new Fade2(fade1);
  fade2.from.add(1);
  assertEq(fade2.from.r, 1);
  assertEq(fade2.from.g, 1);
  assertEq(fade2.from.b, 1);
  fade2.to.add(1);
  assertEq(fade2.to.r, 131);
  assertEq(fade2.to.g, 130);
  assertEq(fade2.to.b, 129);

  reportCompare(true, true);
  print("Tests complete");
}

runTests();


