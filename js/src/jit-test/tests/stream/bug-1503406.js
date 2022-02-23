// |jit-test| skip-if: !this.hasOwnProperty("ReadableStream")
let g = newGlobal();
let reader = g.eval(`
    let stream = new ReadableStream({
        start(controller) {
            controller.enqueue([]);
        },
    });
    let [b1, b2] = stream.tee();
    b1.getReader();
`);
let read = new ReadableStream({}).getReader().read;
drainJobQueue();  // let the stream be started before reading
read.call(reader);
