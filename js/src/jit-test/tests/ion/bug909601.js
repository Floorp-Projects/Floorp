// |jit-test| ion-eager
for (var i=0; i<3; i++)
    z = new Int32Array;

function f() {
    z.__proto__ = 2;
}

for (var i=0; i<3; i++)
    f();
