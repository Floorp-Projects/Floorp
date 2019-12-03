// |reftest| skip-if(!this.ReadableStream||!this.drainJobQueue)

// 3.5.6. ReadableStreamError ( stream, e ) nothrow
//
// 9. Reject reader.[[closedPromise]] with e.
// 10. Set reader.[[closedPromise]].[[PromiseIsHandled]] to true.
//
// Rejection for [[closedPromise]] shouldn't be reported as unhandled.

const rs = new ReadableStream({
  start() {
    return Promise.reject(new Error("test"));
  }
});

let rejected = false;
rs.getReader().read().then(() => {}, () => { rejected = true; });

drainJobQueue();

assertEq(rejected, true);

if (typeof reportCompare === 'function') {
  reportCompare(0, 0);
}

// Shell itself reports unhandled rejection if there's any.
