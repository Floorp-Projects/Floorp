/* Any copyright is dedicated to the Public Domain.
 http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

// Testing evaluating thowing expressions
const { DevToolsServer } = require("devtools/server/devtools-server");

add_task(async () => {
  const tab = await addTab(`data:text/html;charset=utf-8,Test throw`);
  const commands = await CommandsFactory.forTab(tab);
  await commands.targetCommand.startListening();

  const falsyValues = [
    "-0",
    "null",
    "undefined",
    "Infinity",
    "-Infinity",
    "NaN",
  ];
  for (const value of falsyValues) {
    const response = await commands.scriptCommand.execute(`throw ${value};`);
    is(
      response.exception.type,
      value,
      `Got the expected value for response.exception.type when throwing "${value}"`
    );
  }

  const identityTestValues = [false, 0];
  for (const value of identityTestValues) {
    const response = await commands.scriptCommand.execute(`throw ${value};`);
    is(
      response.exception,
      value,
      `Got the expected value for response.exception when throwing "${value}"`
    );
  }

  const symbolTestValues = [
    ["Symbol.iterator", "Symbol(Symbol.iterator)"],
    ["Symbol('foo')", "Symbol(foo)"],
    ["Symbol()", "Symbol()"],
  ];
  for (const [expr, message] of symbolTestValues) {
    const response = await commands.scriptCommand.execute(`throw ${expr};`);
    is(
      response.exceptionMessage,
      message,
      `Got the expected value for response.exceptionMessage when throwing "${expr}"`
    );
  }

  const longString = Array(DevToolsServer.LONG_STRING_LENGTH + 1).join("a"),
    shortedString = longString.substring(
      0,
      DevToolsServer.LONG_STRING_INITIAL_LENGTH
    );
  const response = await commands.scriptCommand.execute(
    "throw '" + longString + "';"
  );
  is(
    response.exception.initial,
    shortedString,
    "Got the expected value for exception.initial when throwing a longString"
  );
  is(
    response.exceptionMessage.initial,
    shortedString,
    "Got the expected value for exceptionMessage.initial when throwing a longString"
  );
});
