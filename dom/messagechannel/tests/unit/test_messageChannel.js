/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

add_test(function test_messageChannel() {
  do_test_pending();

  let chromeWorker = new ChromeWorker(
    "resource://test/chromeWorker_messageChannel.js");
  let { port1, port2 } = new MessageChannel();
  port2.onmessage = pongEvt => {
    Assert.equal(pongEvt.data, "pong");
    let { port1: newPort1, port2: newPort2 } = new MessageChannel();
    newPort2.onmessage = goodbyeEvt => {
      Assert.equal(goodbyeEvt.data, "goodbye");

      do_test_finished();
      run_next_test();
    };
    pongEvt.ports[0].postMessage("hello", [newPort1]);
  };
  chromeWorker.postMessage("ping", [port1]);
});
