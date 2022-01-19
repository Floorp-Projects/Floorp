// |reftest| skip-if(!xulRuntime.shell||!this.hasOwnProperty('ReadableStream')) -- needs drainJobQueue

if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}

// Spot-check subclassing of stream constructors.

// ReadableStream can be subclassed.
class PartyStreamer extends ReadableStream {}

// The base class constructor is called.
let started = false;
let stream = new PartyStreamer({
    // (The ReadableStream constructor calls this start method.)
    start(c) { started = true; }
});
drainJobQueue();
assertEq(started, true);

// The instance's prototype chain is correct.
assertEq(stream.__proto__, PartyStreamer.prototype);
assertEq(stream.__proto__.__proto__, ReadableStream.prototype);
assertEq(stream.__proto__.__proto__.__proto__, Object.prototype);
assertEq(stream.__proto__.__proto__.__proto__.__proto__, null);
assertEq(stream instanceof ReadableStream, true);

// Non-generic methods can be called on the resulting stream.
let reader = stream.getReader();
assertEq(stream.locked, true);


// CountQueuingStrategy can be subclassed.         
class PixelStrategy extends CountQueuingStrategy {}
assertEq(new PixelStrategy({highWaterMark: 4}).__proto__, PixelStrategy.prototype);

// The base class constructor is called.
assertThrowsInstanceOf(() => new PixelStrategy, TypeError);
assertEq(new PixelStrategy({highWaterMark: -1}).highWaterMark, -1);


// VerySmartStrategy can be subclassed.
class VerySmartStrategy extends ByteLengthQueuingStrategy {
    size(chunk) {
        return super.size(chunk) * 8;
    }
}
let vss = new VerySmartStrategy({highWaterMark: 12});
assertEq(vss.size(new ArrayBuffer(8)), 64);
assertEq(vss.__proto__, VerySmartStrategy.prototype);


// Even ReadableStreamDefaultReader can be subclassed.
async function readerTest() {
    const ReadableStreamDefaultReader = new ReadableStream().getReader().constructor;
    class MindReader extends ReadableStreamDefaultReader {
        async read() {
            let foretold = {value: "death", done: false};
            let actual = await super.read();
            actual = foretold; // ZOMG I WAS RIGHT, EXACTLY AS FORETOLD they should call me a righter
            return actual;
        }
    }

    let stream = new ReadableStream({
        start(c) { c.enqueue("one"); c.enqueue("two"); },
        pull(c) { c.close(); }
    });
    let reader = new MindReader(stream);
    let result = await reader.read();
    assertEq(result.value, "death");
    reader.releaseLock();

    reader = stream.getReader();
    result = await reader.read();
    assertEq(result.done, false);
    assertEq(result.value, "two");
    result = await reader.read();
    assertEq(result.done, true);
    assertEq(result.value, undefined);
}
runAsyncTest(readerTest);


// Even ReadableStreamDefaultController, which can't be constructed,
// can be subclassed.
let ReadableStreamDefaultController;
new ReadableStream({
    start(c) {
        ReadableStreamDefaultController = c.constructor;
    }
});
class MasterController extends ReadableStreamDefaultController {
    constructor() {
        // don't call super, it'll just throw
        return Object.create(MasterController.prototype);
    }
}
let c = new MasterController();

// The prototype chain is per spec.
assertEq(c instanceof ReadableStreamDefaultController, true);

// But the instance does not have the internal slots of a
// ReadableStreamDefaultController, so the non-generic methods can't be used.
assertThrowsInstanceOf(() => c.enqueue("horse"), TypeError);


if (typeof reportCompare === 'function') {
    reportCompare(0, 0);
}
