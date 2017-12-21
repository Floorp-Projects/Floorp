/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let Cu = Components.utils;
  let sb = new Cu.Sandbox('https://www.example.com',
                          { wantGlobalProperties:
                            ["crypto", "TextEncoder", "TextDecoder"]
                          });
  sb.ok = ok;
  Cu.evalInSandbox('ok(this.crypto);', sb);
  Cu.evalInSandbox('ok(this.crypto.subtle);', sb);
  sb.equal = equal;
  let innerPromise = new Promise(r => (sb.test_done = r));
  Cu.evalInSandbox('crypto.subtle.digest("SHA-256", ' +
                   '                     new TextEncoder("utf-8").encode("abc"))' +
                   '  .then(h => equal(new Uint16Array(h)[0], 30906))' +
                   '  .then(test_done);', sb);

  Cu.importGlobalProperties(["crypto"]);
  ok(crypto);
  ok(crypto.subtle);
  let outerPromise = crypto.subtle.digest("SHA-256", new TextEncoder("utf-8").encode("abc"))
      .then(h => Assert.equal(new Uint16Array(h)[0], 30906));

  do_test_pending();
  Promise.all([innerPromise, outerPromise]).then(() => do_test_finished());
}
