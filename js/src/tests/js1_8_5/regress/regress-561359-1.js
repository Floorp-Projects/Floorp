// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Gary Kwong <gary@rumblingedge.com>

for (let z = 0; z < 2; z++) {
    with({
        x: function() {}
    }) {
        for (l in [x]) {}
    }
}

reportCompare(0, 0, 'ok');
