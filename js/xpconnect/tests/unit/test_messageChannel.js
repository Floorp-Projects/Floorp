/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let Cu = Components.utils;
  let sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["MessageChannel"] });
  sb.ok = ok;
  Cu.evalInSandbox('ok((new MessageChannel()) instanceof MessageChannel);',
                   sb);
  Cu.evalInSandbox('ok((new MessageChannel()).port1 instanceof MessagePort);',
                   sb);

  Cu.importGlobalProperties(["MessageChannel"]);

  let mc = new MessageChannel();
  Assert.ok(mc instanceof MessageChannel);
  Assert.ok(mc.port1 instanceof MessagePort);
  Assert.ok(mc.port2 instanceof MessagePort);

  mc.port1.postMessage(42);

  let result = await new Promise(resolve => {
    mc.port2.onmessage = e => {
      resolve(e.data);
    }
  });

  Assert.equal(result, 42);
});
