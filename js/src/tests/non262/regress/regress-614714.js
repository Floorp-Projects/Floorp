// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/

function f(reportCompare) {
    if (typeof clear === 'function')
        clear(this);
    return f;
}

// This must be called before clear().
reportCompare(0, 0, 'ok');
f();  // don't assert
