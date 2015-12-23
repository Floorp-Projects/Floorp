// |jit-test| test-also-noasmjs
/* -*- Mode: javascript; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 ; js-indent-level : 2 ; js-curly-indent-offset: 0 -*- */
/* vim: set ts=4 et sw=4 tw=80: */

// Author: Peter Jensen

load(libdir + "asm.js");
if (!isSimdAvailable() || typeof SIMD === 'undefined') {
    print("won't run tests as simd extensions aren't activated yet");
    quit(0);
}

const NUM_BIRDS = 30;
const NUM_UPDATES = 20;
const ACCEL_DATA_STEPS = 30;

var buffer = new ArrayBuffer(0x200000);
var bufferF32 = new Float32Array(buffer);

var actualBirds = 0;

function init() {
    actualBirds = 0;
    // Make it a power of two, for quick modulo wrapping.
    var accelDataValues = [10.0, 9.5, 9.0, 8.0, 7.0, 6.0, 5.5, 5.0, 5.0, 5.0, 5.5, 6.0, 7.0, 8.0, 9.0, 10.0];
    accelDataValues = accelDataValues.map(function(v) { return 50*v; });
    var accelDataValuesLength = accelDataValues.length;
    assertEq(accelDataValuesLength, 16); // Hard coded in the asm.js module
    for (i = 0; i < accelDataValuesLength; i++)
        bufferF32[i + NUM_BIRDS * 2] = accelDataValues[i];
}

function addBird(pos, vel) {
    bufferF32[actualBirds] = pos;
    bufferF32[actualBirds + NUM_BIRDS] = vel;
    actualBirds++;
    return actualBirds - 1;
}

function getActualBirds() {
    return actualBirds;
}

var code = `
    "use asm";
    var toF = global.Math.fround;
    var u8 = new global.Uint8Array(buffer);
    var f32 = new global.Float32Array(buffer);
    const maxBirds = 100000;
    const maxBirdsx4 = 400000;
    const maxBirdsx8 = 800000;
    const accelMask = 0x3c;
    const mk4 = 0x000ffff0;

    const getMaxPos = 1000.0;
    const getAccelDataSteps = imp.accelDataSteps | 0;
    var getActualBirds = imp.getActualBirds;

    var i4 = global.SIMD.Int32x4;
    var f4 = global.SIMD.Float32x4;
    var b4 = global.SIMD.Bool32x4;
    var i4add = i4.add;
    var i4and = i4.and;
    var f4select = f4.select;
    var f4add = f4.add;
    var f4sub = f4.sub;
    var f4mul = f4.mul;
    var f4greaterThan = f4.greaterThan;
    var f4splat = f4.splat;
    var f4load = f4.load;
    var f4store = f4.store;
    var b4any = b4.anyTrue;

    const zerox4 = f4(0.0,0.0,0.0,0.0);

    function declareHeapSize() {
        f32[0x0007ffff] = toF(0.0);
    }

    function update(timeDelta) {
        timeDelta = toF(timeDelta);
        //      var steps               = Math.ceil(timeDelta/accelData.interval);
        var steps = 0;
        var subTimeDelta = toF(0.0);
        var actualBirds = 0;
        var maxPos = toF(0.0);
        var maxPosx4 = f4(0.0,0.0,0.0,0.0);
        var subTimeDeltax4  = f4(0.0,0.0,0.0,0.0);
        var subTimeDeltaSquaredx4 = f4(0.0,0.0,0.0,0.0);
        var point5x4 = f4(0.5, 0.5, 0.5, 0.5);
        var i = 0;
        var len = 0;
        var accelIndex = 0;
        var newPosx4 = f4(0.0,0.0,0.0,0.0);
        var newVelx4 = f4(0.0,0.0,0.0,0.0);
        var accel = toF(0.0);
        var accelx4 = f4(0.0,0.0,0.0,0.0);
        var a = 0;
        var posDeltax4 = f4(0.0,0.0,0.0,0.0);
        var cmpx4 = b4(0,0,0,0);
        var newVelTruex4 = f4(0.0,0.0,0.0,0.0);

        steps = getAccelDataSteps | 0;
        subTimeDelta = toF(toF(timeDelta / toF(steps | 0)) / toF(1000.0));
        actualBirds = getActualBirds() | 0;
        maxPos = toF(+getMaxPos);
        maxPosx4 = f4splat(maxPos);
        subTimeDeltax4 = f4splat(subTimeDelta);
        subTimeDeltaSquaredx4 = f4mul(subTimeDeltax4, subTimeDeltax4);

        len = ((actualBirds + 3) >> 2) << 4;

        for (i = 0; (i | 0) < (len | 0); i = (i + 16) | 0) {
            accelIndex = 0;
            newPosx4 = f4load(u8, i & mk4);
            newVelx4 = f4load(u8, (i & mk4) + maxBirdsx4);
            for (a = 0; (a | 0) < (steps | 0); a = (a + 1) | 0) {
                accel = toF(f32[(accelIndex & accelMask) + maxBirdsx8 >> 2]);
                accelx4 = f4splat(accel);
                accelIndex = (accelIndex + 4) | 0;
                posDeltax4 = f4mul(point5x4, f4mul(accelx4, subTimeDeltaSquaredx4));
                posDeltax4 = f4add(posDeltax4, f4mul(newVelx4, subTimeDeltax4));
                newPosx4 = f4add(newPosx4, posDeltax4);
                newVelx4 = f4add(newVelx4, f4mul(accelx4, subTimeDeltax4));
                cmpx4 = f4greaterThan(newPosx4, maxPosx4);

                if (b4any(cmpx4)) {
                    // Work around unimplemented 'neg' operation, using 0 - x.
                    newVelTruex4 = f4sub(zerox4, newVelx4);
                    newVelx4 = f4select(cmpx4, newVelTruex4, newVelx4);
                }
            }
            f4store(u8, i & mk4, newPosx4);
            f4store(u8, (i & mk4) + maxBirdsx4, newVelx4);
        }
    }

    return update;
`

var ffi = {
    getActualBirds,
    accelDataSteps: ACCEL_DATA_STEPS
};

var fbirds = asmLink(asmCompile('global', 'imp', 'buffer', code), this, ffi, buffer);

init();
for (var i = 0; i < NUM_BIRDS; i++) {
    addBird(i / 10, Math.exp(2, NUM_BIRDS - i));
}

var b = dateNow();
for (var j = 0; j < NUM_UPDATES; j++) {
    fbirds(16);
}
print(dateNow() - b);

assertEq(bufferF32[0], 0);
assertEq(bufferF32[1], 0.10000000149011612);
assertEq(bufferF32[2], 0.20000000298023224);
assertEq(bufferF32[3], 0.30000001192092896);
assertEq(bufferF32[4], 0.4000000059604645);
assertEq(bufferF32[5], 0.5);
assertEq(bufferF32[6], 0.6000000238418579);
assertEq(bufferF32[7], 0.699999988079071);
assertEq(bufferF32[8], 0.800000011920929);
assertEq(bufferF32[9], 0.8999999761581421);
assertEq(bufferF32[10], 1);
assertEq(bufferF32[11], 1.100000023841858);
assertEq(bufferF32[12], 1.2000000476837158);
assertEq(bufferF32[13], 1.2999999523162842);
assertEq(bufferF32[14], 1.399999976158142);
assertEq(bufferF32[15], 1.5);
assertEq(bufferF32[16], 1.600000023841858);
assertEq(bufferF32[17], 1.7000000476837158);
assertEq(bufferF32[18], 1.7999999523162842);
assertEq(bufferF32[19], 1.899999976158142);
assertEq(bufferF32[20], 2);
assertEq(bufferF32[21], 2.0999999046325684);
assertEq(bufferF32[22], 2.200000047683716);
assertEq(bufferF32[23], 2.299999952316284);
assertEq(bufferF32[24], 2.4000000953674316);
assertEq(bufferF32[25], 2.5);
assertEq(bufferF32[26], 2.5999999046325684);
assertEq(bufferF32[27], 2.700000047683716);
assertEq(bufferF32[28], 2.799999952316284);
assertEq(bufferF32[29], 2.9000000953674316);


// Code used to generate the assertEq list above.
function generateAssertList() {
    var buf = '';
    for (var k = 0; k < NUM_BIRDS; k++) {
        buf += 'assertEq(bufferF32['+ k + '], ' + bufferF32[k] + ');\n';
    }
    print(buf);
}
//generateAssertList();
