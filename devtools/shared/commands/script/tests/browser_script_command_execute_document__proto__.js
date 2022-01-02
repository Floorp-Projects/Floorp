/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing evaluating document.__proto__

add_task(async () => {
  const tab = await addTab(
    `data:text/html;charset=utf-8,Test evaluating document.__proto__`
  );
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const evaluationResponse = await commands.scriptCommand.execute(
    "document.__proto__"
  );
  checkObject(evaluationResponse, {
    input: "document.__proto__",
    result: {
      type: "object",
      actor: /[a-z]/,
    },
  });

  ok(!evaluationResponse.exception, "no eval exception");
  ok(!evaluationResponse.helperResult, "no helper result");

  const response = await evaluationResponse.result.getPrototypeAndProperties();
  ok(!response.error, "no response error");

  const props = response.ownProperties;
  ok(props, "response properties available");

  const expectedProps = Object.getOwnPropertyNames(
    Object.getPrototypeOf(document)
  );
  checkObject(Object.keys(props), expectedProps, "Same own properties.");

  await commands.destroy();
});
