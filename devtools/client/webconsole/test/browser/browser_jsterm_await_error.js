/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

// Test that failing top-level await expression (rejected or throwing) work as expected.

"use strict";

const TEST_URI =
  "data:text/html;charset=utf-8,<!DOCTYPE html>Web Console test failing top-level await";

add_task(async function () {
  // Needed for the execute() function below
  await pushPref("security.allow_parent_unrestricted_js_loads", true);

  // Enable await mapping.
  await pushPref("devtools.debugger.features.map-await-expression", true);
  const hud = await openNewTabAndConsole(TEST_URI);

  info("Check that awaiting for a rejecting promise displays an error");
  let res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise((resolve,reject) => setTimeout(() => reject("await-rej"), 250))`,
    "Uncaught (in promise) await-rej"
  );
  ok(res.node, "awaiting for a rejecting promise displays an error message");

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject("await-rej-2")`,
    `Uncaught (in promise) await-rej-2`
  );
  ok(res.node, "awaiting for Promise.reject displays an error");

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject("")`,
    `Uncaught (in promise) <empty string>`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with empty string displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject(null)`,
    `Uncaught (in promise) null`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with null displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject(undefined)`,
    `Uncaught (in promise) undefined`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with undefined displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject(false)`,
    `Uncaught (in promise) false`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with false displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject(0)`,
    `Uncaught (in promise) 0`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with 0 displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject({foo: "bar"})`,
    `Uncaught (in promise) Object { foo: "bar" }`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with an object displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await Promise.reject(new Error("foo"))`,
    `Uncaught (in promise) Error: foo`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with an error object displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `var err = new Error("foo");
     err.name = "CustomError";
     await Promise.reject(err);
     `,
    `Uncaught (in promise) CustomError: foo`
  );
  ok(
    res.node,
    "awaiting for Promise rejecting with an error object with a name property displays the expected error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(() => a.b.c)`,
    `ReferenceError: a is not defined`
  );
  ok(
    res.node,
    "awaiting for a promise with a throwing function displays an error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(res => setTimeout(() => res(d.e.f), 250))`,
    `ReferenceError: d is not defined`
  );
  ok(
    res.node,
    "awaiting for a promise with a throwing function displays an error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(res => { throw "instant throw"; })`,
    `Uncaught (in promise) instant throw`
  );
  ok(
    res.node,
    "awaiting for a promise with a throwing function displays an error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(res => { throw new Error("instant error throw"); })`,
    `Error: instant error throw`
  );
  ok(
    res.node,
    "awaiting for a promise with a thrown Error displays an error message"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(res => { setTimeout(() => { throw "throw in timeout"; }, 250) })`,
    `Uncaught throw in timeout`
  );
  ok(
    res.node,
    "awaiting for a promise with a throwing function displays an error"
  );

  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise(res => {
      setTimeout(() => { throw new Error("throw error in timeout"); }, 250)
    })`,
    `throw error in timeout`
  );
  ok(
    res.node,
    "awaiting for a promise with a throwing function displays an error"
  );

  info("Check that we have the expected number of commands");
  const expectedInputsNumber = 16;
  is(
    (await findMessagesVirtualizedByType({ hud, typeSelector: ".command" }))
      .length,
    expectedInputsNumber,
    "There is the expected number of commands messages"
  );

  info("Check that we have as many errors as commands");
  const expectedErrorsNumber = expectedInputsNumber;
  is(
    (await findMessagesVirtualizedByType({ hud, typeSelector: ".error" }))
      .length,
    expectedErrorsNumber,
    "There is the expected number of error messages"
  );

  info("Check that there's no result message");
  is(
    (await findMessagesVirtualizedByType({ hud, typeSelector: ".result" }))
      .length,
    0,
    "There is no result messages"
  );

  info("Check that malformed await expressions displays a meaningful error");
  res = await executeAndWaitForErrorMessage(
    hud,
    `await new Promise())`,
    `SyntaxError: unexpected token: ')'`
  );
  ok(
    res.node,
    "awaiting for a malformed expression displays a meaningful error"
  );
});
