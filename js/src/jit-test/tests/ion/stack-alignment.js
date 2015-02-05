setJitCompilerOption("baseline.warmup.trigger", 10);
setJitCompilerOption("ion.warmup.trigger", 30);
var i;

// Check that an entry frame is always aligned properly.
function entryFrame_1() {
  assertJitStackInvariants();
}

// Check rectifier frames are keeping the same alignment.
function rectifierFrame_verify(a, b, c, d) {
  assertJitStackInvariants();
}

function rectifierFrame_1(i) {
  rectifierFrame_verify();
}
function rectifierFrame_2(i) {
  rectifierFrame_verify(i);
}
function rectifierFrame_3(i) {
  rectifierFrame_verify(i, i);
}
function rectifierFrame_4(i) {
  rectifierFrame_verify(i, i, i);
}

// Check that an ion frame size is always aligned properly.
function gen_ionFrameSize(x, y, name) {
  var locals = (new Array(x)).fill(0).map((v, i) => i);
  var args = (new Array(y)).fill(0).map((v, i) => i);

  return new Function("i",
    locals.map(i => "var l% = i + %;\n".replace("%", i, "g")).join("")
  + name + "(" + args.map(i => "l%".replace("%", i)).join(", ") + ");\n"
  + "return " + locals.map(i => "l%".replace("%", i)).join(" + ") + ";\n"
  );
}

var ionFrameSize_0 = gen_ionFrameSize(30, 0, "assertJitStackInvariants");
var ionFrameSize_1 = gen_ionFrameSize(31, 0, "assertJitStackInvariants");
var ionFrameSize_2 = gen_ionFrameSize(32, 0, "assertJitStackInvariants");
var ionFrameSize_3 = gen_ionFrameSize(33, 0, "assertJitStackInvariants");

function ionFrameSize_callee_verify(a, b, c, d) {
  assertJitStackInvariants();
}

var ionFrameSize_args = [];
for (var l = 0; l < 4; l++) {
  ionFrameSize_args[l] = [];
  for (var a = 0; a < 4; a++)
    ionFrameSize_args[l][a] = gen_ionFrameSize(30 + l, a, "ionFrameSize_callee_verify");;
}

for (i = 0; i < 40; i++) {
  entryFrame_1();
  entryFrame_1(0);
  entryFrame_1(0, 1);
  rectifierFrame_1(i);
  rectifierFrame_2(i);
  rectifierFrame_3(i);
  rectifierFrame_4(i);
  ionFrameSize_0(i);
  ionFrameSize_1(i);
  ionFrameSize_2(i);
  ionFrameSize_3(i);
  for (var l = 0; l < 4; l++)
    for (var a = 0; a < 4; a++)
      ionFrameSize_args[l][a](i);
}
