// |jit-test| skip-if: !('oomTest' in this)

var patchSet = new Set();

function checkSet(str) {
    patchSet.has(str);
}

for (var i = 0; i < 20; i++) {
    checkSet("s");
}

let re = /x(.*)x/;
let count = 0;
oomTest(() => {
    // Create a string that needs to be atomized.
    let result = re.exec("xa" + count++ + "ax")[1];
    checkSet(result);
})
