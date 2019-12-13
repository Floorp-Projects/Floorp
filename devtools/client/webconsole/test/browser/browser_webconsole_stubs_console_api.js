/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const {
  STUBS_UPDATE_ENV,
  getStubFilePath,
  getCleanedPacket,
  getSerializedPacket,
  writeStubsToFile,
} = require("devtools/client/webconsole/test/browser/stub-generator-helpers");

const TEST_URI =
  "http://example.com/browser/devtools/client/webconsole/test/browser/test-console-api.html";
const STUB_FILE = "consoleApi.js";

add_task(async function() {
  const isStubsUpdate = env.get(STUBS_UPDATE_ENV) == "true";
  info(`${isStubsUpdate ? "Update" : "Check"} ${STUB_FILE}`);

  const generatedStubs = await generateConsoleApiStubs();

  if (isStubsUpdate) {
    await writeStubsToFile(
      getStubFilePath(STUB_FILE, env, true),
      generatedStubs
    );
    ok(true, `${STUB_FILE} was updated`);
    return;
  }
  const existingStubs = require(getStubFilePath(STUB_FILE));
  const FAILURE_MSG =
    "The consoleApi stubs file needs to be updated by running " +
    "`mach test devtools/client/webconsole/test/browser/" +
    "browser_webconsole_stubs_console_api.js --headless " +
    "--setenv WEBCONSOLE_STUBS_UPDATE=true`";

  if (generatedStubs.size !== existingStubs.rawPackets.size) {
    ok(false, FAILURE_MSG);
    return;
  }

  let failed = false;
  for (const [key, packet] of generatedStubs) {
    const packetStr = getSerializedPacket(packet);
    const existingPacketStr = getSerializedPacket(
      existingStubs.rawPackets.get(key)
    );

    is(packetStr, existingPacketStr, `"${key}" packet has expected value`);
    failed = failed || packetStr !== existingPacketStr;
  }

  if (failed) {
    ok(false, FAILURE_MSG);
  } else {
    ok(true, "Stubs are up to date");
  }

  await closeTabAndToolbox().catch(() => {});
});

async function generateConsoleApiStubs() {
  const stubs = new Map();

  const hud = await openNewTabAndConsole(TEST_URI);
  const target = hud.currentTarget;
  const webConsoleFront = await target.getFront("console");

  for (const { keys, code } of getCommands()) {
    const received = new Promise(resolve => {
      let i = 0;
      const listener = async res => {
        const callKey = keys[i];

        stubs.set(callKey, getCleanedPacket(callKey, res));

        if (++i === keys.length) {
          webConsoleFront.off("consoleAPICall", listener);
          resolve();
        }
      };
      webConsoleFront.on("consoleAPICall", listener);
    });

    await SpecialPowers.spawn(gBrowser.selectedBrowser, [code], function(
      subCode
    ) {
      const script = content.document.createElement("script");
      script.append(
        content.document.createTextNode(`function triggerPacket() {${subCode}}`)
      );
      content.document.body.append(script);
      content.wrappedJSObject.triggerPacket();
      script.remove();
    });

    await received;
  }

  // We have everything we want, we can freeze the console to avoid communication
  // with the server.
  const {
    START_IGNORE_ACTION,
  } = require("devtools/client/shared/redux/middleware/ignore");
  await hud.ui.wrapper.getStore().dispatch(START_IGNORE_ACTION);

  return stubs;
}

function getCommands() {
  const consoleApiCommands = [
    "console.log('foobar', 'test')",
    "console.log(undefined)",
    "console.warn('danger, will robinson!')",
    "console.log(NaN)",
    "console.log(null)",
    "console.log('\u9f2c')",
    "console.clear()",
    "console.count('bar')",
    "console.assert(false, {message: 'foobar'})",
    "console.log('\xFA\u1E47\u0129\xE7\xF6d\xEA \u021B\u0115\u0219\u0165')",
    "console.dirxml(window)",
    "console.log('myarray', ['red', 'green', 'blue'])",
    "console.log('myregex', /a.b.c/)",
    "console.table(['red', 'green', 'blue']);",
    "console.log('myobject', {red: 'redValue', green: 'greenValue', blue: 'blueValue'});",
    "console.debug('debug message');",
    "console.info('info message');",
    "console.error('error message');",
  ];

  const consoleApi = consoleApiCommands.map(cmd => ({
    keys: [cmd],
    code: cmd,
  }));

  consoleApi.push(
    {
      keys: ["console.log('mymap')"],
      code: `
  var map = new Map();
  map.set("key1", "value1");
  map.set("key2", "value2");
  console.log('mymap', map);
  `,
    },
    {
      keys: ["console.log('myset')"],
      code: `
  console.log('myset', new Set(["a", "b"]));
  `,
    },
    {
      keys: ["console.trace()"],
      code: `
  function testStacktraceFiltering() {
    console.trace()
  }
  function foo() {
    testStacktraceFiltering()
  }

  foo()
  `,
    },
    {
      keys: ["console.trace('bar', {'foo': 'bar'}, [1,2,3])"],
      code: `
  function testStacktraceWithLog() {
    console.trace('bar', {'foo': 'bar'}, [1,2,3])
  }
  function foo() {
    testStacktraceWithLog()
  }

  foo()
  `,
    },
    {
      keys: [
        "console.time('bar')",
        "timerAlreadyExists",
        "console.timeLog('bar') - 1",
        "console.timeLog('bar') - 2",
        "console.timeEnd('bar')",
        "timeEnd.timerDoesntExist",
        "timeLog.timerDoesntExist",
      ],
      code: `
  console.time("bar");
  console.time("bar");
  console.timeLog("bar");
  console.timeLog("bar", "second call", {state: 1});
  console.timeEnd("bar");
  console.timeEnd("bar");
  console.timeLog("bar");
  `,
    },
    {
      keys: ["console.table('bar')"],
      code: `
  console.table('bar');
  `,
    },
    {
      keys: ["console.table(['a', 'b', 'c'])"],
      code: `
  console.table(['a', 'b', 'c']);
  `,
    },
    {
      keys: ["console.group('bar')", "console.groupEnd('bar')"],
      code: `
  console.group("bar");
  console.groupEnd();
  `,
    },
    {
      keys: ["console.groupCollapsed('foo')", "console.groupEnd('foo')"],
      code: `
  console.groupCollapsed("foo");
  console.groupEnd();
  `,
    },
    {
      keys: ["console.group()", "console.groupEnd()"],
      code: `
  console.group();
  console.groupEnd();
  `,
    },
    {
      keys: ["console.log(%cfoobar)"],
      code: `
  console.log(
    "%cfoo%cbar",
    "color:blue; font-size:1.3em; background:url('http://example.com/test'); position:absolute; top:10px; ",
    "color:red; line-height: 1.5; background:\\165rl('http://example.com/test')"
  );
  `,
    },
    {
      keys: ['console.log("%cHello%c|%cWorld")'],
      code: `
    console.log(
      "%cHello%c|%cWorld",
      "color:red",
      "",
      "color: blue"
    );
  `,
    },
    {
      keys: ["console.group(%cfoo%cbar)", "console.groupEnd(%cfoo%cbar)"],
      code: `
  console.group(
    "%cfoo%cbar",
    "color:blue;font-size:1.3em;background:url('http://example.com/test');position:absolute;top:10px",
    "color:red;background:\\165rl('http://example.com/test')");
  console.groupEnd();
  `,
    },
    {
      keys: [
        "console.groupCollapsed(%cfoo%cbaz)",
        "console.groupEnd(%cfoo%cbaz)",
      ],
      code: `
  console.groupCollapsed(
    "%cfoo%cbaz",
    "color:blue;font-size:1.3em;background:url('http://example.com/test');position:absolute;top:10px",
    "color:red;background:\\165rl('http://example.com/test')");
  console.groupEnd();
  `,
    },
    {
      keys: ["console.dir({C, M, Y, K})"],
      code: "console.dir({cyan: 'C', magenta: 'M', yellow: 'Y', black: 'K'});",
    },
    {
      keys: [
        "console.count | default: 1",
        "console.count | default: 2",
        "console.count | test counter: 1",
        "console.count | test counter: 2",
        "console.count | default: 3",
        "console.count | clear",
        "console.count | default: 4",
        "console.count | test counter: 3",
        "console.countReset | test counter: 0",
        "console.countReset | counterDoesntExist",
      ],
      code: `
      console.count();
      console.count();
      console.count("test counter");
      console.count("test counter");
      console.count();
      console.clear();
      console.count();
      console.count("test counter");
      console.countReset("test counter");
      console.countReset("test counter");
  `,
    },
    {
      keys: ["console.log escaped characters"],
      code: "console.log('hello \\nfrom \\rthe \\\"string world!')",
    }
  );
  return consoleApi;
}
