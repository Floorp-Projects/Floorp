/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

if (typeof evalcx == 'function') {
    var src = 'try {\n' +
    '    for (var [e] = /x/ in d) {\n' +
    '        (function () {});\n' +
    '    }\n' +
    '} catch (e) {}\n' +
    'try {\n' +
    '    let(x = Object.freeze(this, /x/))\n' +
    '    e = {}.toString\n' +
    '    function y() {}\n' +
    '} catch (e) {}';

    evalcx(src);
}

reportCompare(0, 0, "don't crash");
