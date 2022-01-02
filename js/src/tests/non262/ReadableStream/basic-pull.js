// |reftest| skip-if(!this.ReadableStream||!this.drainJobQueue)

if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}

// Example of a stream that produces data on demand, the "pull" model.
let fibStream = new ReadableStream({
    start(controller) {
        this.a = 0;
        this.b = 1;
        controller.enqueue(0);
        controller.enqueue(1);
    },

    pull(controller) {
        [this.a, this.b] = [this.b, this.a + this.b];
        controller.enqueue(this.b);
    }
});

async function test() {
    assertEq(fibStream.locked, false);
    let reader = fibStream.getReader();
    assertEq(fibStream.locked, true);

    let results = [];
    while (results.length < 10) {
        results.push((await reader.read()).value);
    }

    assertEq(results.join(), "0,1,1,2,3,5,8,13,21,34");
    reader.releaseLock();
    assertEq(fibStream.locked, false);
}

runAsyncTest(test);
