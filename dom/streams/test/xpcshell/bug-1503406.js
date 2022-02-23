let read;
let reader;

add_task(async function test() {
  let g = newGlobal({ wantGlobalProperties: ["ReadableStream"] });
  reader = g.eval(`
    let stream = new ReadableStream({
        start(controller) {
            controller.enqueue([]);
        },
    });
    let [b1, b2] = stream.tee();
    b1.getReader();
`);
  read = new ReadableStream({}).getReader().read;
});

add_task(async function test2() {
  read.call(reader);
});
