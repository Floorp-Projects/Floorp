// |jit-test| skip-if: !('oomAfterAllocations' in this)

function first(a) {
    return a[0];
}

try {
    first([function() {}]);
    first([function() {}]);
    oomAfterAllocations(50);
    first([function() {}]);
} catch(e) {
    // ignore oom
}
