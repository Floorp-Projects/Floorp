// This will pop the getiter fuse.
Array.prototype[Symbol.iterator] = Array.prototype[Symbol.iterator];

function f() {
    [] = []
}

// We should nevertheless be able to pass this loop without crashing.
for (let i = 0; i < 100; i++) {
    f();
}
