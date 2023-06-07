// |jit-test| skip-if: !isAsmJSCompilationAvailable()

load(libdir + "asm.js");

var g = newGlobal({alwaysUseFdlibm: true})

var code = (fun) => {
  return `(function asm(glob) {
    "use asm";
    var sq=glob.Math.${fun}; function f(d) { d=+d; return +sq(d) } return f;
  })`;
}

// Linking RFP asm.js with non-RFP Math.sin.
let asmSin = g.eval(code("sin"));
assertAsmLinkFail(asmSin, {Math: {sin: Math.sin}});

// Linking RFP asm.js with RFP Math.sin
const sin = asmLink(asmSin, {Math: {sin: g.Math.sin}});
assertEq(sin(35*Math.LN2   ), -0.765996413898051);
assertEq(sin(110*Math.LOG2E),  0.9989410140273757);
assertEq(sin(7*Math.LOG10E ),  0.10135692924965616);

// Linking non-RFP asm.js with RFP Math.sin
asmSin = eval(code("sin"))
assertAsmLinkFail(asmSin, {Math: {sin: g.Math.sin}});

// Linking non-RFP asm.js with non-RFP Math.sin (the "normal case")
asmLink(asmSin, {Math: {sin: Math.sin}});

// Testing cos
let asmCos = g.eval(code("cos"));
assertAsmLinkFail(asmCos, {Math: {cos: Math.cos}});
const cos = asmLink(asmCos, {Math: {cos: g.Math.cos}});
assertEq(cos(1e130          ), -0.767224894221913);
assertEq(cos(1e272          ), -0.7415825695514536);
assertEq(cos(1e284          ),  0.7086865671674247);
assertEq(cos(1e75           ), -0.7482651726250321);
assertEq(cos(57*Math.E      ), -0.536911695749024);
assertEq(cos(21*Math.LN2    ), -0.4067775970251724);
assertEq(cos(51*Math.LN2    ), -0.7017203400855446);
assertEq(cos(21*Math.SQRT1_2), -0.6534063185820198);
assertEq(cos(17*Math.LOG10E ),  0.4537557425982784);
assertEq(cos(2*Math.LOG10E  ),  0.6459044007438142);

// Testing tan
let asmTan = g.eval(code("tan"));
assertAsmLinkFail(asmTan, {Math: {tan: Math.tan}});
const tan = asmLink(asmTan, {Math: {tan: g.Math.tan}});
assertEq(tan(1e140          ),  0.7879079967710036);
assertEq(tan(6*Math.E       ),  0.6866761546452431);
assertEq(tan(6*Math.LN2     ),  1.6182817135715877);
assertEq(tan(10*Math.LOG2E  ), -3.3537128705376014);
assertEq(tan(17*Math.SQRT2  ), -1.9222955461799982);
assertEq(tan(34*Math.SQRT1_2), -1.9222955461799982);
assertEq(tan(10*Math.LOG10E ),  2.5824856130712432);
