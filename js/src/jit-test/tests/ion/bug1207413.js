if (typeof oomAfterAllocations !== 'function')
    quit();

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
