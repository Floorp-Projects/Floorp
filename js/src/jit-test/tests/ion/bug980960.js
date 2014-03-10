if (typeof TypedObject === 'undefined')
    quit();

var StructType = TypedObject.StructType;
var uint8 = TypedObject.uint8;

function check(c) {
  assertEq(c.r, 129);
}

function run() {
  var RgbColor = new StructType({r: uint8, g: uint8, b: uint8});
  var Fade = new StructType({from: RgbColor, to: RgbColor});

  var BrgColor = new StructType({b: uint8, r: uint8, g: uint8});
  var BrgFade = new StructType({from: BrgColor, to: BrgColor});

  var gray = new RgbColor({r: 129, g: 128, b: 127});

  var fade = new Fade({from: gray, to: gray});
  fade.to = {r: 129, g: 128, b: 127};

  var brgGray = new BrgColor(gray);
  fade.from = brgGray;

  var brgFade = new BrgFade(fade);

  check(fade.to);
  check(brgFade.to);
  check(fade.to);
  check(brgFade.to);
}

run();
