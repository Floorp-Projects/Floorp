// |reftest| skip-if(!this.hasOwnProperty('ReadableStream'))
// A stream can become errored with an exception from another realm.

let g = newGlobal();
let g_enqueue;
new g.ReadableStream({
    start(controller) {
        g_enqueue = controller.enqueue;
    },
});

let controller;
let stream = new ReadableStream({
    start(c) {
        controller = c;
    }
}, {
    size(chunk) {}
});

assertThrowsInstanceOf(() => g_enqueue.call(controller, {}), g.RangeError);

if (typeof reportCompare == 'function')
    reportCompare(0, 0);

