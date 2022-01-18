// |reftest| skip-if(!this.hasOwnProperty('ReadableStream'))

// The second argument to `new ReadableStream` defaults to `{}`, so it observes
// properties hacked onto Object.prototype.

let log = [];

Object.defineProperty(Object.prototype, "size", {
    configurable: true,
    get() {
        log.push("size");
        log.push(this);
        return undefined;
    }
});
Object.prototype.highWaterMark = 1337;

let s = new ReadableStream({
    start(controller) {
        log.push("start");
        log.push(controller.desiredSize);
    }
});
assertDeepEq(log, ["size", {}, "start", 1337]);

if (typeof reportCompare == "function")
    reportCompare(0, 0);
