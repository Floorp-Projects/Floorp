/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

function run_test() {
  let Cu = Components.utils;
  let sb = new Cu.Sandbox('https://www.example.com',
                          { wantGlobalProperties: ['rtcIdentityProvider'] });

  function exerciseInterface() {
    equal(typeof rtcIdentityProvider, 'object');
    equal(typeof rtcIdentityProvider.register, 'function');
    rtcIdentityProvider.register({
      generateAssertion: function(a, b, c) {
        return Promise.resolve({
          idp: { domain: 'example.com' },
          assertion: JSON.stringify([a, b, c])
        });
      },
      validateAssertion: function(d, e) {
        return Promise.resolve({
          identity: 'user@example.com',
          contents: JSON.stringify([d, e])
        });
      }
    });
  }

  sb.equal = equal;
  Cu.evalInSandbox('(' + exerciseInterface.toSource() + ')();', sb);
  ok(sb.rtcIdentityProvider.idp);

  Cu.importGlobalProperties(['rtcIdentityProvider']);
  exerciseInterface();
  ok(rtcIdentityProvider.idp);
}
