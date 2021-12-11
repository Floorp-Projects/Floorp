/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing basic expression evaluation
const {
  MAX_AUTOCOMPLETE_ATTEMPTS,
  MAX_AUTOCOMPLETIONS,
} = require("devtools/shared/webconsole/js-property-provider");
const { DevToolsServer } = require("devtools/server/devtools-server");

add_task(async () => {
  const tab = await addTab(`data:text/html;charset=utf-8,
  <script>
    window.foobarObject = Object.create(
      null,
      Object.getOwnPropertyDescriptors({
        foo: 1,
        foobar: 2,
        foobaz: 3,
        omg: 4,
        omgfoo: 5,
        strfoo: "foobarz",
        omgstr: "foobarz" + "abb".repeat(${DevToolsServer.LONG_STRING_LENGTH} * 2),
      })
    );

    window.largeObject1 = Object.create(null);
    for (let i = 0; i < ${MAX_AUTOCOMPLETE_ATTEMPTS} + 1; i++) {
      window.largeObject1["a" + i] = i;
    }

    window.largeObject2 = Object.create(null);
    for (let i = 0; i < ${MAX_AUTOCOMPLETIONS} * 2; i++) {
      window.largeObject2["a" + i] = i;
    }
  </script>`);

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  await doSimpleEval(commands);
  await doWindowEval(commands);
  await doEvalWithException(commands);
  await doEvalWithHelper(commands);
  await doEvalString(commands);
  await doEvalLongString(commands);
  await doEvalWithBinding(commands);
  await forceLexicalInit(commands);

  await commands.destroy();
});

async function doSimpleEval(commands) {
  info("test eval '2+2'");
  const response = await commands.scriptCommand.execute("2+2");
  checkObject(response, {
    input: "2+2",
    result: 4,
  });

  ok(!response.exception, "no eval exception");
  ok(!response.helperResult, "no helper result");
}

async function doWindowEval(commands) {
  info("test eval 'document'");
  const response = await commands.scriptCommand.execute("document");
  checkObject(response, {
    input: "document",
    result: {
      type: "object",
      class: "HTMLDocument",
      actor: /[a-z]/,
    },
  });

  ok(!response.exception, "no eval exception");
  ok(!response.helperResult, "no helper result");
}

async function doEvalWithException(commands) {
  info("test eval with exception");
  const response = await commands.scriptCommand.execute(
    "window.doTheImpossible()"
  );
  checkObject(response, {
    input: "window.doTheImpossible()",
    result: {
      type: "undefined",
    },
    exceptionMessage: /doTheImpossible/,
  });

  ok(response.exception, "js eval exception");
  ok(!response.helperResult, "no helper result");
}

async function doEvalWithHelper(commands) {
  info("test eval with helper");
  const response = await commands.scriptCommand.execute("clear()");
  checkObject(response, {
    input: "clear()",
    result: {
      type: "undefined",
    },
    helperResult: { type: "clearOutput" },
  });

  ok(!response.exception, "no eval exception");
}

async function doEvalString(commands) {
  const response = await commands.scriptCommand.execute(
    "window.foobarObject.strfoo"
  );

  checkObject(response, {
    input: "window.foobarObject.strfoo",
    result: "foobarz",
  });
}

async function doEvalLongString(commands) {
  const response = await commands.scriptCommand.execute(
    "window.foobarObject.omgstr"
  );

  const str = await SpecialPowers.spawn(
    gBrowser.selectedBrowser,
    [],
    function() {
      return content.wrappedJSObject.foobarObject.omgstr;
    }
  );

  const initial = str.substring(0, DevToolsServer.LONG_STRING_INITIAL_LENGTH);

  checkObject(response, {
    input: "window.foobarObject.omgstr",
    result: {
      type: "longString",
      initial: initial,
      length: str.length,
    },
  });
}

async function doEvalWithBinding(commands) {
  const response = await commands.scriptCommand.execute("document;");
  const documentActor = response.result.actorID;

  info("running a command with _self as document using selectedObjectActor");
  const selectedObjectSame = await commands.scriptCommand.execute(
    "_self === document",
    {
      selectedObjectActor: documentActor,
    }
  );
  checkObject(selectedObjectSame, {
    result: true,
  });
}

async function forceLexicalInit(commands) {
  info("test that failed let/const bindings are initialized to undefined");

  const testData = [
    {
      stmt: "let foopie = wubbalubadubdub",
      vars: ["foopie"],
    },
    {
      stmt: "let {z, w={n}=null} = {}",
      vars: ["z", "w"],
    },
    {
      stmt: "let [a, b, c] = null",
      vars: ["a", "b", "c"],
    },
    {
      stmt: "const nein1 = rofl, nein2 = copter",
      vars: ["nein1", "nein2"],
    },
    {
      stmt: "const {ha} = null",
      vars: ["ha"],
    },
    {
      stmt: "const [haw=[lame]=null] = []",
      vars: ["haw"],
    },
    {
      stmt: "const [rawr, wat=[lame]=null] = []",
      vars: ["rawr", "haw"],
    },
    {
      stmt: "let {zzz: xyz=99, zwz: wb} = nexistepas()",
      vars: ["xyz", "wb"],
    },
    {
      stmt: "let {c3pdoh=101} = null",
      vars: ["c3pdoh"],
    },
  ];

  for (const data of testData) {
    const response = await commands.scriptCommand.execute(data.stmt);
    checkObject(response, {
      input: data.stmt,
      result: { type: "undefined" },
    });
    ok(response.exception, "expected exception");
    for (const varName of data.vars) {
      const response2 = await commands.scriptCommand.execute(varName);
      checkObject(response2, {
        input: varName,
        result: { type: "undefined" },
      });
      ok(!response2.exception, "unexpected exception");
    }
  }
}
