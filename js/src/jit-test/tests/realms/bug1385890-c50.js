// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
// See <https://bugzilla.mozilla.org/show_bug.cgi?id=1385890#c50>.

let otherGlobal = newGlobal();
function getFreshInstances(type, otherType = type) {
    stream = new ReadableStream({
        start(c) {
            controller = c;
        },
        type
    });
}
getFreshInstances();
let [branch1, branch2] = otherGlobal.ReadableStream.prototype.tee.call(stream);
cancelPromise1 = ReadableStream.prototype.cancel.call(branch1, {
    name: "cancel 1"
});
cancelPromise2 = ReadableStream.prototype.cancel.call(branch2, {
    name: "cancel 2"
});
