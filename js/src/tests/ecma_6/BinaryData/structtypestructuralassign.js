// |reftest| skip-if(!this.hasOwnProperty("Type"))
var BUGNUMBER = 578700;
var summary = 'BinaryData StructType structural assignment';

function assertEqColor(c1, c2) {
  assertEq(c1.r, c2.r);
  assertEq(c1.g, c2.g);
  assertEq(c1.b, c2.b);
}

function runTests() {
  var RgbColor = new StructType({r: uint8, g: uint8, b: uint8});
  var Fade = new StructType({from: RgbColor, to: RgbColor});

  var white = new RgbColor({r: 255, g: 255, b: 255});
  var gray = new RgbColor({r: 129, g: 128, b: 127});
  var black = new RgbColor({r: 0, g: 0, b: 0});

  var fade = new Fade({from: white, to: white});
  assertEqColor(white, fade.from);
  assertEqColor(white, fade.to);

  fade.to = gray;
  assertEqColor(white, fade.from);
  assertEqColor(gray, fade.to);

  fade.to = black;
  assertEqColor(white, fade.from);
  assertEqColor(black, fade.to);

  fade.to = {r: 129, g: 128, b: 127};
  assertEqColor(white, fade.from);
  assertEqColor(gray, fade.to);

  fade.from = {r: 0, g: 0, b: 0};
  assertEqColor(black, fade.from);
  assertEqColor(gray, fade.to);

  // Create a variation of color which is still binary data but the
  // properties are in the wrong order. This will prevent a simple
  // memcopy, but it should still work, just on the "slow path".
  var BrgColor = new StructType({b: uint8, r: uint8, g: uint8});
  var brgGray = new BrgColor(gray);
  assertEqColor(gray, brgGray);

  fade.from = brgGray;
  assertEqColor(gray, fade.from);

  // One last test where we have to recursively adapt:
  var BrgFade = new StructType({from: BrgColor, to: BrgColor});
  var brgFade = new BrgFade(fade);
  assertEqColor(brgFade.from, fade.from);
  assertEqColor(brgFade.to, fade.to);

  // Test that extra and missing properties are ok:
  fade.from = {r: 129, g: 128, b: 127, a: 126};
  assertEqColor(fade.from, gray);

  // Missing properties are just treated as undefined:
  fade.from = {r: 129, g: 128};
  assertEq(fade.from.r, 129);
  assertEq(fade.from.g, 128);
  assertEq(fade.from.b, 0);

  // Which means weird stuff like this is legal:
  fade.from = [];
  assertEqColor(fade.from, black);
  fade.from = {};
  assertEqColor(fade.from, black);

  // But assignment from a scalar is NOT:
  var failed = false;
  try {
    civic.color = 5;
  } catch(e) { failed = true; }
  if (!failed) throw new Exception("Should have thrown");

  reportCompare(true, true);
  print("Tests complete");
}

runTests();
