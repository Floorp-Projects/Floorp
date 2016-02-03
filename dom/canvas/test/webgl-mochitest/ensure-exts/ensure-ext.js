'use strict';

function EnsureExt(name, shouldBe = true) {
    var c = document.createElement('canvas');
    var gl = c.getContext('experimental-webgl');

    if (shouldBe) {
        ok(gl.getExtension(name), 'Should have extension ' + name + '.');
    } else {
        ok(!gl.getExtension(name), 'Should not have extension ' + name + '.');
    }
}

function EnsureDraftExt(name, shouldBe = true) {
    SimpleTest.waitForExplicitFinish();

    var fnEnsure = function() {
        EnsureExt(name, shouldBe);
        SimpleTest.finish();
    };

    if ('SpecialPowers' in window) {
        var prefStateList = [
            ['webgl.enable-draft-extensions', true],
        ];
        var prefEnv = {'set': prefStateList};
        SpecialPowers.pushPrefEnv(prefEnv, fnEnsure);
    } else {
        console.log('Couldn\'t use SpecialPowers to enable draft extensions.');
        fnEnsure();
    }
}
