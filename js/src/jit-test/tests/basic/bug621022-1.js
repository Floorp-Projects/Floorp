function f(x) {
    delete arguments[0];
    undefined != arguments[0];
    undefined == arguments[0];
    undefined != arguments[0];
    undefined === arguments[0];
}

for(var i=0; i<20; i++) {
    f(1);
}

// Don't assert.

