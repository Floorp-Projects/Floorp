/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const { WorkerDispatcher, workerHandler } = require("../worker-utils");

describe("worker utils", () => {
  it("starts a worker", () => {
    const dispatcher = new WorkerDispatcher();
    global.Worker = jest.fn();
    dispatcher.start("foo");
    expect(dispatcher.worker).toEqual(global.Worker.mock.instances[0]);
  });

  it("stops a worker", () => {
    const dispatcher = new WorkerDispatcher();
    const terminateMock = jest.fn();

    global.Worker = jest.fn(() => ({
      terminate: terminateMock,
    }));

    dispatcher.start();
    dispatcher.stop();

    expect(dispatcher.worker).toEqual(null);
    expect(terminateMock.mock.calls).toHaveLength(1);
  });

  it("dispatches a task", () => {
    const dispatcher = new WorkerDispatcher();
    const postMessageMock = jest.fn();
    const addEventListenerMock = jest.fn();

    global.Worker = jest.fn(() => {
      return {
        postMessage: postMessageMock,
        addEventListener: addEventListenerMock,
      };
    });

    dispatcher.start();
    const task = dispatcher.task("foo");
    task("bar");

    const postMessageMockCall = postMessageMock.mock.calls[0][0];

    expect(postMessageMockCall).toEqual({
      calls: [["bar"]],
      id: 1,
      method: "foo",
    });

    expect(addEventListenerMock.mock.calls).toHaveLength(1);
  });

  it("dispatches a queued task", async () => {
    const dispatcher = new WorkerDispatcher();
    let postMessageMock;
    const postMessagePromise = new Promise(resolve => {
      postMessageMock = jest.fn(resolve);
    });

    const addEventListenerMock = jest.fn();

    global.Worker = jest.fn(() => {
      return {
        postMessage: postMessageMock,
        addEventListener: addEventListenerMock,
      };
    });

    dispatcher.start();
    const task = dispatcher.task("foo", { queue: true });
    task("bar");
    task("baz");

    expect(postMessageMock).not.toHaveBeenCalled();

    await postMessagePromise;

    const postMessageMockCall = postMessageMock.mock.calls[0][0];

    expect(postMessageMockCall).toEqual({
      calls: [["bar"], ["baz"]],
      id: 1,
      method: "foo",
    });

    expect(addEventListenerMock.mock.calls).toHaveLength(1);
  });

  it("test workerHandler error case", async () => {
    let postMessageMock;
    const postMessagePromise = new Promise(resolve => {
      postMessageMock = jest.fn(resolve);
    });

    self.postMessage = postMessageMock;

    const callee = {
      doSomething: () => {
        throw new Error("failed");
      },
    };

    const handler = workerHandler(callee);

    handler({ data: { id: 53, method: "doSomething", calls: [[]] } });

    await postMessagePromise;

    expect(postMessageMock.mock.calls[0][0]).toEqual({
      id: 53,
      results: [
        {
          error: true,
          message: "failed",
          metadata: undefined,
        },
      ],
    });
  });
});
