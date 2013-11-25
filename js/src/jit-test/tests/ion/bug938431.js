if (typeof ParallelArray === "undefined")
  quit();

function x() {}
ParallelArray(3385, function(y) {
    Object.defineProperty([], 8, {
        e: (y ? x : Math.fround(1))
    })
})
