if (typeof 'oomAtAllocation' === 'undefined')
    quit();

function fn(i) {
    if (i == 3)
      return ["isFinite"].map(function (i) {});
    return [];
}

try {
    fn(0);
    fn(1);
    fn(2);
    oomAtAllocation(50);
    fn(3);
} catch(e) {
    // Ignore oom
}

