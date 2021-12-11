/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(async function() {
  info("Test JSON encoding started");

  const bom = "%EF%BB%BF"; // UTF-8 BOM
  const tests = [
    {
      input: bom,
      output: "",
    },
    {
      input: "%FE%FF", // UTF-16BE BOM
      output: "\uFFFD\uFFFD",
    },
    {
      input: "%FF%FE", // UTF-16LE BOM
      output: "\uFFFD\uFFFD",
    },
    {
      input: bom + "%30",
      output: "0",
    },
    {
      input: bom + bom,
      output: "\uFEFF",
    },
    {
      input: "%00%61",
      output: "\u0000a",
    },
    {
      input: "%61%00",
      output: "a\u0000",
    },
    {
      input: "%30%FF",
      output: "0�",
    },
    {
      input: "%C3%A0",
      output: "à",
    },
    {
      input: "%E2%9D%A4",
      output: "❤",
    },
    {
      input: "%F0%9F%9A%80",
      output: "🚀",
    },
  ];

  for (const { input, output } of tests) {
    info("Test decoding of " + JSON.stringify(input) + ".");

    await addJsonViewTab("data:application/json," + input);
    await selectJsonViewContentTab("rawdata");

    // Check displayed data.
    const data = await getElementText(".textPanelBox .data");
    is(data, output, "The right data has been received.");
  }
});
