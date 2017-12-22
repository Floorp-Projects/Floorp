// Any copyright is dedicated to the Public Domain.
// http://creativecommons.org/licenses/publicdomain/
// Contributor: Gary Kwong <gary@rumblingedge.com>, Jesse Ruderman <jruderman@gmail.com>

for (var u = 0; u < 3; ++u) {
    var y = [];
    Object.create(y);
    gc();
    y.t = 3;
    gc();
}

reportCompare(0, 0, 'ok');
