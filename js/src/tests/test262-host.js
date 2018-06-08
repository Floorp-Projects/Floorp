// This Source Code Form is subject to the terms of the Mozilla Public
// License, v. 2.0. If a copy of the MPL was not distributed with this
// file, You can obtain one at http://mozilla.org/MPL/2.0/.

// https://github.com/tc39/test262/blob/master/INTERPRETING.md#host-defined-functions
;(function createHostObject(global) {
    var FunctionToString = global.Function.prototype.toString;
    var ReflectApply = global.Reflect.apply;
    var NewGlobal = global.newGlobal;
    var Atomics = global.Atomics;
    var SharedArrayBuffer = global.SharedArrayBuffer;
    var Int32Array = global.Int32Array;
    var setSharedArrayBuffer = global.setSharedArrayBuffer;
    var getSharedArrayBuffer = global.getSharedArrayBuffer;
    var evalInWorker = global.evalInWorker;
    var monotonicNow = global.monotonicNow;

    var hasCreateIsHTMLDDA = "createIsHTMLDDA" in global;
    var hasThreads = ("helperThreadCount" in global ? global.helperThreadCount() > 0 : true);
    var hasMailbox = typeof setSharedArrayBuffer == "function" && typeof getSharedArrayBuffer == "function";
    var hasEvalInWorker = typeof evalInWorker == "function";

    if (!hasCreateIsHTMLDDA && !("document" in global && "all" in global.document))
        throw new Error("no [[IsHTMLDDA]] object available for testing");

    var IsHTMLDDA = hasCreateIsHTMLDDA
                    ? global.createIsHTMLDDA()
                    : global.document.all;



    // The $262.agent framework is not appropriate for browsers yet, and some
    // test cases can't work in browsers (they block the main thread).

    var shellCode = hasMailbox && hasEvalInWorker;
    var sabTestable = Atomics && SharedArrayBuffer && hasThreads && shellCode;

    global.$262 = {
        __proto__: null,
        createRealm() {
            var newGlobalObject = NewGlobal();
            var createHostObjectFn = ReflectApply(FunctionToString, createHostObject, []);
            newGlobalObject.Function(`${createHostObjectFn} createHostObject(this);`)();
            return newGlobalObject.$262;
        },
        detachArrayBuffer: global.detachArrayBuffer,
        evalScript: global.evaluateScript || global.evaluate,
        global,
        IsHTMLDDA,
        agent: (function () {

            // SpiderMonkey complication: With run-time argument --no-threads
            // our test runner will not properly filter test cases that can't be
            // run because agents can't be started, and so we do a little
            // filtering here: We will quietly succeed and exit if an agent test
            // should not have been run because threads cannot be started.
            //
            // Firefox complication: The test cases that use $262.agent can't
            // currently work in the browser, so for now we rely on them not
            // being run at all.

            if (!sabTestable) {
                return {
                    _notAvailable() {
                        // See comment above.
                        if (!hasThreads && shellCode) {
                            global.reportCompare(0,0);
                            global.quit(0);
                        }
                        throw new Error("Agents not available");
                    },
                    start(script) { this._notAvailable() },
                    broadcast(sab, id) { this._notAvailable() },
                    getReport() { this._notAvailable() },
                    sleep(s) { this._notAvailable() }
                }
            }

            // The SpiderMonkey implementation uses a designated shared buffer _ia
            // for coordination, and spinlocks for everything except sleeping.

            var _MSG_LOC = 0;           // Low bit set: broadcast available; High bits: seq #
            var _ID_LOC = 1;            // ID sent with broadcast
            var _ACK_LOC = 2;           // Worker increments this to ack that broadcast was received
            var _RDY_LOC = 3;           // Worker increments this to ack that worker is up and running
            var _LOCKTXT_LOC = 4;       // Writer lock for the text buffer: 0=open, 1=closed
            var _NUMTXT_LOC = 5;        // Count of messages in text buffer
            var _NEXT_LOC = 6;          // First free location in the buffer
            var _SLEEP_LOC = 7;         // Used for sleeping

            var _FIRST = 10;            // First location of first message

            var _ia = new Int32Array(new SharedArrayBuffer(65536));
            _ia[_NEXT_LOC] = _FIRST;

            var _worker_prefix =
// BEGIN WORKER PREFIX
`if (typeof $262 == 'undefined')
    $262 = {};
$262.agent = (function () {
    var _ia = new Int32Array(getSharedArrayBuffer());
    var agent = {
        receiveBroadcast(receiver) {
            var k;
            while (((k = Atomics.load(_ia, ${_MSG_LOC})) & 1) == 0)
                ;
            var received_sab = getSharedArrayBuffer();
            var received_id = Atomics.load(_ia, ${_ID_LOC});
            Atomics.add(_ia, ${_ACK_LOC}, 1);
            while (Atomics.load(_ia, ${_MSG_LOC}) == k)
                ;
            receiver(received_sab, received_id);
        },

        report(msg) {
            while (Atomics.compareExchange(_ia, ${_LOCKTXT_LOC}, 0, 1) == 1)
                ;
            msg = "" + msg;
            var i = _ia[${_NEXT_LOC}];
            _ia[i++] = msg.length;
            for ( let j=0 ; j < msg.length ; j++ )
                _ia[i++] = msg.charCodeAt(j);
            _ia[${_NEXT_LOC}] = i;
            Atomics.add(_ia, ${_NUMTXT_LOC}, 1);
            Atomics.store(_ia, ${_LOCKTXT_LOC}, 0);
        },

        sleep(s) {
            Atomics.wait(_ia, ${_SLEEP_LOC}, 0, s);
        },

        leaving() {},

        monotonicNow,
    };
    Atomics.add(_ia, ${_RDY_LOC}, 1);
    return agent;
})();`;
// END WORKER PREFIX

            return {
                _numWorkers: 0,
                _numReports: 0,
                _reportPtr: _FIRST,

                _bailIfNotAvailable() {
                    if (!sabTestable) {
                        // See comment above.
                        if (!hasThreads && shellCode) {
                            global.reportCompare(0,0);
                            global.quit(0);
                        }
                        throw new Error("Agents not available");
                    }
                },

                start(script) {
                    this._bailIfNotAvailable();
                    setSharedArrayBuffer(_ia.buffer);
                    var oldrdy = Atomics.load(_ia, _RDY_LOC);
                    evalInWorker(_worker_prefix + script);
                    while (Atomics.load(_ia, _RDY_LOC) == oldrdy)
                        ;
                    this._numWorkers++;
                },

                broadcast(sab, id) {
                    this._bailIfNotAvailable();
                    setSharedArrayBuffer(sab);
                    Atomics.store(_ia, _ID_LOC, id);
                    Atomics.store(_ia, _ACK_LOC, 0);
                    Atomics.add(_ia, _MSG_LOC, 1);
                    while (Atomics.load(_ia, _ACK_LOC) < this._numWorkers)
                        ;
                    Atomics.add(_ia, _MSG_LOC, 1);
                },

                getReport() {
                    this._bailIfNotAvailable();
                    if (this._numReports == Atomics.load(_ia, _NUMTXT_LOC))
                        return null;
                    var s = "";
                    var i = this._reportPtr;
                    var len = _ia[i++];
                    for ( let j=0 ; j < len ; j++ )
                        s += String.fromCharCode(_ia[i++]);
                    this._reportPtr = i;
                    this._numReports++;
                    return s;
                },

                sleep(s) {
                    this._bailIfNotAvailable();
                    Atomics.wait(_ia, _SLEEP_LOC, 0, s);
                },

                monotonicNow,
            };
        })()
    };
})(this);

var $mozAsyncTestDone = false;
function $DONE(failure) {
    // This function is generally called from within a Promise handler, so any
    // exception thrown by this method will be swallowed and most likely
    // ignored by the Promise machinery.
    if ($mozAsyncTestDone) {
        reportFailure("$DONE() already called");
        return;
    }
    $mozAsyncTestDone = true;

    if (failure)
        reportFailure(failure);
    else
        reportCompare(0, 0);
}
