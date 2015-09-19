// |jit-test| error: too much recursion
var tokenCodes = {
    get finally() {
        if (tokenCodes[arr[i]] !== i) {}
    }
};
var arr = ['finally'];
for (var i = 0; i < arr.length; i++) {
    if (tokenCodes[arr[i]] !== i) {}
}
