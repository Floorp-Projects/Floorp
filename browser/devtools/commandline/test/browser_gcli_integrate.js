/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Tests that source URLs in the Web Console can be clicked to display the
// standard View Source window.

function test() {
  testCreateCommands();
  testRemoveCommands();
}

let [ define, require ] = (function() {
  let tempScope = {};
  Components.utils.import("resource://gre/modules/devtools/Require.jsm", tempScope);
  return [ tempScope.define, tempScope.require ];
})();

registerCleanupFunction(function tearDown() {
  define = undefined;
  require = undefined;
});

let tselarr = {
  name: 'tselarr',
  params: [
    { name: 'num', type: { name: 'selection', data: [ '1', '2', '3' ] } },
    { name: 'arr', type: { name: 'array', subtype: 'string' } },
  ],
  exec: function(args, env) {
    return "flu " + args.num + "-" + args.arr.join("_");
  }
};

function testCreateCommands() {
  let gcliIndex = require("gcli/index");
  gcliIndex.addCommand(tselarr);

  let canon = require("gcli/canon");
  let tselcmd = canon.getCommand("tselarr");
  ok(tselcmd != null, "tselarr exists in the canon");
  ok(tselcmd instanceof canon.Command, "canon storing commands");
}

function testRemoveCommands() {
  let gcliIndex = require("gcli/index");
  gcliIndex.removeCommand(tselarr);

  let canon = require("gcli/canon");
  let tselcmd = canon.getCommand("tselarr");
  ok(tselcmd == null, "tselcmd removed from the canon");
}
