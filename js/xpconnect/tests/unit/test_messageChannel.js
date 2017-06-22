/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_task(async function() {
  let Cu = Components.utils;
  let sb = new Cu.Sandbox('http://www.example.com',
                          { wantGlobalProperties: ["MessageChannel"] });
  sb.do_check_true = do_check_true;
  Cu.evalInSandbox('do_check_true((new MessageChannel()) instanceof MessageChannel);',
                   sb);
  Cu.evalInSandbox('do_check_true((new MessageChannel()).port1 instanceof MessagePort);',
                   sb);

  Cu.importGlobalProperties(["MessageChannel"]);

  let mc = new MessageChannel();
  do_check_true(mc instanceof MessageChannel);
  do_check_true(mc.port1 instanceof MessagePort);
  do_check_true(mc.port2 instanceof MessagePort);

  mc.port1.postMessage(42);

  let result = await new Promise(resolve => {
    mc.port2.onmessage = e => {
      resolve(e.data);
    }
  });

  do_check_eq(result, 42);
});
