/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

add_task(async function run_test() {
  let sb = new Cu.Sandbox('http://www.example.com');

  let done = false;
  let iterator = {
    [Symbol.asyncIterator]() {
      return this;
    },

    next() {
      let promise = Cu.evalInSandbox(`Promise.resolve({done: ${done}, value: {hello: "world"}})`, sb);
      done = true;
      return promise;
    }
  }

  let stream = ReadableStream.from(iterator);
  let reader = stream.getReader();
  let result = await reader.read();
  Assert.equal(result.done, false);
  Assert.equal(result.value?.hello, "world");
  result = await reader.read();
  Assert.equal(result.done, true);
});
