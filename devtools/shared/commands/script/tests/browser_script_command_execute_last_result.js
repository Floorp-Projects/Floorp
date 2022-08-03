/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing that last evaluation result can be accessed with `$_`

add_task(async () => {
  const tab = await addTab(`data:text/html;charset=utf-8,`);

  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  info("$_ returns undefined if nothing has evaluated yet");
  let response = await commands.scriptCommand.execute("$_");
  basicResultCheck(response, "$_", { type: "undefined" });

  info("$_ returns last value and performs basic arithmetic");
  response = await commands.scriptCommand.execute("2+2");
  basicResultCheck(response, "2+2", 4);

  response = await commands.scriptCommand.execute("$_");
  basicResultCheck(response, "$_", 4);

  response = await commands.scriptCommand.execute("$_ + 2");
  basicResultCheck(response, "$_ + 2", 6);

  response = await commands.scriptCommand.execute("$_ + 4");
  basicResultCheck(response, "$_ + 4", 10);

  info("$_ has correct references to objects");
  response = await commands.scriptCommand.execute("var foo = {bar:1}; foo;");
  basicResultCheck(response, "var foo = {bar:1}; foo;", {
    type: "object",
    class: "Object",
    actor: /[a-z]/,
  });
  checkObject(response.result.getGrip().preview.ownProperties, {
    bar: {
      value: 1,
    },
  });

  response = await commands.scriptCommand.execute("$_");
  basicResultCheck(response, "$_", {
    type: "object",
    class: "Object",
    actor: /[a-z]/,
  });
  checkObject(response.result.getGrip().preview.ownProperties, {
    bar: {
      value: 1,
    },
  });

  info(
    "Update a property value and check that evaluating $_ returns the expected object instance"
  );
  await ContentTask.spawn(gBrowser.selectedBrowser, null, () => {
    content.wrappedJSObject.foo.bar = "updated_value";
  });

  response = await commands.scriptCommand.execute("$_");
  basicResultCheck(response, "$_", {
    type: "object",
    class: "Object",
    actor: /[a-z]/,
  });
  checkObject(response.result.getGrip().preview.ownProperties, {
    bar: {
      value: "updated_value",
    },
  });

  await commands.destroy();
});

function basicResultCheck(response, input, output) {
  checkObject(response, {
    input,
    result: output,
  });
  ok(!response.exception, "no eval exception");
  ok(!response.helperResult, "no helper result");
}
