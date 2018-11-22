// |reftest| skip-if(!this.ReadableStream||!this.drainJobQueue)
// stream.tee() shouldn't try to call a .start() method.

Object.prototype.start = function () { throw "FAIL"; };
let source = Object.create(null);
new ReadableStream(source).tee();

drainJobQueue();

if (typeof reportCompare == 'function')
    reportCompare(0, 0);
