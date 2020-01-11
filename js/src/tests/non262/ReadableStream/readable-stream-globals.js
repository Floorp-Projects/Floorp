// |reftest| skip-if(!this.hasOwnProperty("ReadableStream"))

if ("ignoreUnhandledRejections" in this) {
  ignoreUnhandledRejections();
}

async function test() {
    if (typeof newGlobal !== 'undefined') {
        otherGlobal = newGlobal();
    }

    OtherReadableStream = otherGlobal.ReadableStream;

    ReadableStreamReader = new ReadableStream().getReader().constructor;
    OtherReadableStreamReader = new otherGlobal.ReadableStream().getReader().constructor;

    let byteStreamsSupported = false;
    try {
        let controller;
        let reader = new ReadableStream({
            start(c) {
                ByteStreamController = c.constructor;
                controller = c;
            },
            type: "bytes"
        }).getReader({ mode: "byob" })
        ReadableStreamBYOBReader = reader.constructor;
        reader.read(new Uint8Array(10));
        BYOBRequest = controller.byobRequest.constructor;
        reader = new otherGlobal.ReadableStream({
            start(c) {
                OtherByteStreamController = c.constructor;
                controller = c;
            },
            type: "bytes"
        }).getReader({ mode: "byob" });
        OtherReadableStreamBYOBReader = reader.constructor;
        reader.read(new Uint8Array(10));
        OtherBYOBRequest = controller.byobRequest.constructor;

        BYOBRequestGetter = Object.getOwnPropertyDescriptor(ByteStreamController.prototype,
                                                            "byobRequest").get;
        OtherBYOBRequestGetter = Object.getOwnPropertyDescriptor(OtherByteStreamController.prototype,
                                                                 "byobRequest").get;

        byteStreamsSupported = true;
    } catch (e) {
    }

    let chunk = { name: "chunk" };
    let enqueuedError = { name: "enqueuedError" };

    let controller;
    let stream;
    let otherStream;
    let otherController;
    let reader;
    let otherReader;

    function getFreshInstances(type, otherType = type) {
        stream = new ReadableStream({ start(c) { controller = c; }, type });

        otherStream = new OtherReadableStream({ start(c) { otherController = c; }, type: otherType });
    }

    getFreshInstances();

    Controller = controller.constructor;
    OtherController = otherController.constructor;


    otherReader = OtherReadableStream.prototype.getReader.call(stream);
    assertEq(otherReader instanceof ReadableStreamReader, false);
    assertEq(otherReader instanceof OtherReadableStreamReader, true);
    assertEq(otherController instanceof Controller, false);

    assertEq(stream.locked, true);
    Object.defineProperty(stream, "locked",
        Object.getOwnPropertyDescriptor(OtherReadableStream.prototype, "locked"));
    assertEq(stream.locked, true);


    request = otherReader.read();
    assertEq(request instanceof otherGlobal.Promise, true);
    controller.close();
    assertEq(await request instanceof Object, true);

    getFreshInstances();
    otherReader = new OtherReadableStreamReader(stream);

    getFreshInstances();
    otherReader = new OtherReadableStreamReader(stream);
    let cancelSucceeded = false;
    let cancelPromise = ReadableStreamReader.prototype.cancel.call(otherReader);
    assertEq(cancelPromise instanceof Promise, true);
    assertEq(await cancelPromise, undefined);

    getFreshInstances();
    otherReader = new OtherReadableStreamReader(stream);
    let closeSucceeded = false;
    Object.defineProperty(otherReader, "closed",
        Object.getOwnPropertyDescriptor(ReadableStreamReader.prototype, "closed"));
    let closedPromise = otherReader.closed;
    assertEq(closedPromise instanceof otherGlobal.Promise, true);
    controller.close();
    assertEq(await closedPromise, undefined);

    getFreshInstances();

    otherReader = OtherReadableStream.prototype.getReader.call(stream);
    request = otherReader.read();
    assertEq(request instanceof otherGlobal.Promise, true);
    otherController.close.call(controller);
    assertEq(await request instanceof otherGlobal.Object, true);

    getFreshInstances();

    assertEq(controller.desiredSize, 1);
    Object.defineProperty(controller, "desiredSize",
        Object.getOwnPropertyDescriptor(OtherController.prototype, "desiredSize"));
    assertEq(controller.desiredSize, 1);


    request = otherReader.read();

    controller.error(enqueuedError);

    expectException(() => controller.close(), TypeError);
    expectException(() => otherController.close.call(controller), otherGlobal.TypeError);

    otherReader.releaseLock();

    reader = stream.getReader();
    assertEq(await expectAsyncException(async () => reader.read(), enqueuedError.constructor),
             enqueuedError);

    otherReader.releaseLock.call(reader);
    assertEq(reader.closed instanceof otherGlobal.Promise, true);

    // getFreshInstances();

    // reader = stream.getReader();
    // request = otherReader.read.call(reader);
    // assertEq(request instanceof otherGlobal.Promise, true);
    // controller.enqueue(chunk);
    // assertEq((await request).value, chunk);

    // reader.releaseLock();

    // getFreshInstances();

    // reader = stream.getReader();
    // request = otherReader.read.call(reader);
    // otherController.enqueue.call(controller, chunk);
    // otherController.enqueue.call(controller, new otherGlobal.Uint8Array(10));
    // controller.enqueue(new otherGlobal.Uint8Array(10));
    // request = otherReader.read.call(reader);

    getFreshInstances();

    stream = new ReadableStream({ start(c) { controller = c; } }, { size() {return 1} });
    otherController.enqueue.call(controller, chunk);
    otherController.enqueue.call(controller, new otherGlobal.Uint8Array(10));
    controller.enqueue(new otherGlobal.Uint8Array(10));

    getFreshInstances();

    controller.close();
    expectException(() => controller.enqueue(new otherGlobal.Uint8Array(10)), TypeError);
    expectException(() => otherController.enqueue.call(controller, chunk), otherGlobal.TypeError);
    expectException(() => otherController.enqueue.call(controller, new otherGlobal.Uint8Array(10)),
                    otherGlobal.TypeError);

    getFreshInstances();

    let [branch1, branch2] = otherGlobal.ReadableStream.prototype.tee.call(stream);
    assertEq(branch1 instanceof otherGlobal.ReadableStream, true);
    assertEq(branch2 instanceof otherGlobal.ReadableStream, true);

    controller.enqueue(chunk);
    reader = branch1.getReader();
    result = await reader.read();
    reader.releaseLock();
    let subPromiseCreated = false;
    let speciesInvoked = false;
    class SubPromise extends Promise {
        constructor(executor) {
            super(executor);
            subPromiseCreated = true;
        }
    }
    Object.defineProperty(Promise, Symbol.species, {get: function() {
        speciesInvoked = true;
        return SubPromise;
    }
    });

    otherGlobal.eval(`
    subPromiseCreated = false;
    speciesInvoked = false;
    class OtherSubPromise extends Promise {
        constructor(executor) {
            super(executor);
            subPromiseCreated = true;
        }
    }
    Object.defineProperty(Promise, Symbol.species, {get: function() {
        speciesInvoked = true;
        return OtherSubPromise;
    }
    });`);

    controller.error(enqueuedError);
    subPromiseCreated = false;
    speciesInvoked = false;
    otherGlobal.subPromiseCreated = false;
    otherGlobal.speciesInvoked = false;
    let cancelPromise1 = branch1.cancel({ name: "cancel 1" });
    assertEq(cancelPromise1 instanceof otherGlobal.Promise, true);
    assertEq(subPromiseCreated, false);
    assertEq(speciesInvoked, false);
    assertEq(otherGlobal.subPromiseCreated, false);
    assertEq(otherGlobal.speciesInvoked, false);
    subPromiseCreated = false;
    speciesInvoked = false;
    otherGlobal.subPromiseCreated = false;
    otherGlobal.speciesInvoked = false;
    let cancelPromise2 = branch2.cancel({ name: "cancel 2" });
    assertEq(cancelPromise2 instanceof otherGlobal.Promise, true);
    assertEq(subPromiseCreated, false);
    assertEq(speciesInvoked, false);
    assertEq(otherGlobal.subPromiseCreated, false);
    assertEq(otherGlobal.speciesInvoked, false);
    await 1;


    getFreshInstances();

    [branch1, branch2] = otherGlobal.ReadableStream.prototype.tee.call(stream);
    assertEq(branch1 instanceof otherGlobal.ReadableStream, true);
    assertEq(branch2 instanceof otherGlobal.ReadableStream, true);

    controller.enqueue(chunk);
    reader = branch1.getReader();
    result = await reader.read();
    reader.releaseLock();


    assertEq(result.value, chunk);

    controller.error(enqueuedError);
    subPromiseCreated = false;
    speciesInvoked = false;
    otherGlobal.subPromiseCreated = false;
    otherGlobal.speciesInvoked = false;
    cancelPromise1 = ReadableStream.prototype.cancel.call(branch1, { name: "cancel 1" });
    assertEq(cancelPromise1 instanceof Promise, true);
    assertEq(subPromiseCreated, false);
    assertEq(speciesInvoked, false);
    assertEq(otherGlobal.subPromiseCreated, false);
    assertEq(otherGlobal.speciesInvoked, false);
    subPromiseCreated = false;
    speciesInvoked = false;
    otherGlobal.subPromiseCreated = false;
    otherGlobal.speciesInvoked = false;
    cancelPromise2 = ReadableStream.prototype.cancel.call(branch2, { name: "cancel 2" });
    assertEq(cancelPromise2 instanceof Promise, true);
    assertEq(subPromiseCreated, false);
    assertEq(speciesInvoked, false);
    assertEq(otherGlobal.subPromiseCreated, false);
    assertEq(otherGlobal.speciesInvoked, false);

    if (!byteStreamsSupported) {
        return;
    }

    if (typeof nukeCCW === 'function') {
        getFreshInstances("bytes");
        assertEq(otherController instanceof OtherByteStreamController, true);
        reader = ReadableStream.prototype.getReader.call(otherStream);
        otherGlobal.reader = reader;
        otherGlobal.nukeCCW(otherGlobal.reader);
        let chunk = new Uint8Array(10);
        expectException(() => otherController.enqueue(chunk), otherGlobal.TypeError);
        // otherController.error();
        expectException(() => reader.read(), TypeError);
    }

    function testBYOBRequest(controller, view) {
        const request = new BYOBRequest(controller, view);
        let storedView = request.view;
        assertEq(storedView, view);
        storedView = Object.getOwnPropertyDescriptor(OtherBYOBRequest.prototype, "view").get.call(request);
        assertEq(storedView, view);
        request.respond(10);
        OtherBYOBRequest.prototype.respond.call(request, 10);
        request.respondWithNewView(new view.constructor(10));
        OtherBYOBRequest.prototype.respondWithNewView.call(request, new view.constructor(10));
    }

    expectException(() => new BYOBRequest(), TypeError);
    getFreshInstances("bytes");
    expectException(() => new BYOBRequest(controller, new Uint8Array(10)), TypeError);
    expectException(() => new BYOBRequest(otherController, new Uint8Array(10)), TypeError);
    expectException(() => new BYOBRequest(otherController, new Uint8Array(10)), TypeError);
    expectException(() => new BYOBRequest(otherController, new otherGlobal.Uint8Array(10)), TypeError);

    getFreshInstances("bytes");

    reader = stream.getReader({ mode: "byob" });
    request = OtherReadableStreamBYOBReader.prototype.read.call(reader, new Uint8Array(10));
    assertEq(request instanceof otherGlobal.Promise, true);
    controller.enqueue(new Uint8Array([1, 2, 3, 4]));
    result = await request;

    getFreshInstances("bytes");

    reader = stream.getReader({ mode: "byob" });
    request = OtherReadableStreamBYOBReader.prototype.read.call(reader, new Uint8Array(10));
    assertEq(request instanceof otherGlobal.Promise, true);
    try {
        let byobRequest = OtherBYOBRequestGetter.call(controller);
    } catch (e) {
        print(e, '\n', e.stack);
    }
    controller.enqueue(new Uint8Array([1, 2, 3, 4]));
    result = await request;

    await 1;
}

function expectException(closure, errorType) {
    let error;
    try {
        closure();
    } catch (e) {
        error = e;
    }
    assertEq(error !== undefined, true);
    assertEq(error.constructor, errorType);
    return error;
}

async function expectAsyncException(closure, errorType) {
    let error;
    try {
        await closure();
    } catch (e) {
        error = e;
    }
    assertEq(error !== undefined, true);
    assertEq(error.constructor, errorType);
    return error;
}

async function runTest() {
    try {
        await test();
    } catch (e) {
        assertEq(false, true, `Unexpected exception ${e}\n${e.stack}`);
    }
    console.log("done");
    if (typeof reportCompare === "function")
        reportCompare(true, true);
}

runTest();
