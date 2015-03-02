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
    locals.map(i => "var l% = i + %;\n".replace(/%/g, i)).join("")
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

// Check ion frames during function apply calls with the argument vector.
function ionFrame_funApply_0() {
  assertJitStackInvariants.apply(this, arguments);
}
function ionFrame_funApply_1() {
  ionFrame_funApply_0.apply(this, arguments);
}

// Check ion frames during function apply calls with an array of arguments.
function ionFrame_funApply_2() {
  var arr = Array.apply(Array, arguments);
  assertJitStackInvariants.apply(this, arr);
}
function ionFrame_funApply_3() {
  var arr = Array.apply(Array, arguments);
  ionFrame_funApply_2.apply(this, arr);
}

// Check ion frames during function .call calls.
function ionFrame_funCall_0() {
  assertJitStackInvariants.call(this);
}
function ionFrame_funCall_1(a) {
  assertJitStackInvariants.call(this, a);
}
function ionFrame_funCall_2(a, b) {
  assertJitStackInvariants.call(this, a, b);
}
function ionFrame_funCall_3(a, b, c) {
  assertJitStackInvariants.call(this, a, b, c);
}

function ionFrame_funCall_x0() {
  ionFrame_funCall_0.call(this);
}
function ionFrame_funCall_x1(a) {
  ionFrame_funCall_1.call(this, a);
}
function ionFrame_funCall_x2(a, b) {
  ionFrame_funCall_2.call(this, a, b);
}
function ionFrame_funCall_x3(a, b, c) {
  ionFrame_funCall_3.call(this, a, b, c);
}

// Check ion frames during spread calls.
function ionFrame_spreadCall_0() {
  var arr = Array.apply(Array, arguments);
  assertJitStackInvariants(...arr);
}
function ionFrame_spreadCall_1() {
  var arr = Array.apply(Array, arguments);
  ionFrame_spreadCall_0(...arr);
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

  ionFrame_funApply_0();
  ionFrame_funApply_0(1);
  ionFrame_funApply_0(1, 2);
  ionFrame_funApply_0(1, 2, 3);
  ionFrame_funApply_1();
  ionFrame_funApply_1(1);
  ionFrame_funApply_1(1, 2);
  ionFrame_funApply_1(1, 2, 3);

  ionFrame_funApply_2();
  ionFrame_funApply_2(1);
  ionFrame_funApply_2(1, 2);
  ionFrame_funApply_2(1, 2, 3);
  ionFrame_funApply_3();
  ionFrame_funApply_3(1);
  ionFrame_funApply_3(1, 2);
  ionFrame_funApply_3(1, 2, 3);

  ionFrame_funCall_0();
  ionFrame_funCall_1(1);
  ionFrame_funCall_2(1, 2);
  ionFrame_funCall_3(1, 2, 3);
  ionFrame_funCall_x0();
  ionFrame_funCall_x1(1);
  ionFrame_funCall_x2(1, 2);
  ionFrame_funCall_x3(1, 2, 3);

  ionFrame_spreadCall_0();
  ionFrame_spreadCall_0(1);
  ionFrame_spreadCall_0(1, 2);
  ionFrame_spreadCall_0(1, 2, 3);
  ionFrame_spreadCall_1();
  ionFrame_spreadCall_1(1);
  ionFrame_spreadCall_1(1, 2);
  ionFrame_spreadCall_1(1, 2, 3);
}
