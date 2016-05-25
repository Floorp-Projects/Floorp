'use strict';

function EnsureExt(extName, shouldHave=true) {
    EnsureExtFor('webgl', extName, shouldHave);
    EnsureExtFor('webgl2', extName, shouldHave);
}

function EnsureExtFor(contextType, extName, shouldHave=true) {
    var c = document.createElement('canvas');
    var gl = c.getContext(contextType);

    if (!gl) {
        todo(false, 'Failed to create context: ' + contextType);
        return;
    }

    var ext = gl.getExtension(extName);
    var haveText = ' have ' + contextType + ' extension ' + extName + '.';
    if (shouldHave) {
        ok(ext, 'Should' + haveText);
    } else {
        ok(!ext, 'Should not' + haveText);
    }
}

function Lastly_WithDraftExtsEnabled(func) {
    SimpleTest.waitForExplicitFinish();

    var fnEnsure = function() {
        func();
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
