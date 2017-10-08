/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ts=2 et sw=2 tw=80: */
/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

add_task(function* () {
  info("Test JSON encoding started");

  const text = Symbol("text");

  const tests = [
    {
      "UTF-8 with BOM": "",
      "UTF-16BE with BOM": "",
      "UTF-16LE with BOM": "",
      [text]: ""
    }, {
      "UTF-8": "%30",
      "UTF-16BE": "%00%30",
      "UTF-16LE": "%30%00",
      [text]: "0"
    }, {
      "UTF-8": "%30%FF",
      "UTF-16BE": "%00%30%00",
      "UTF-16LE": "%30%00%00",
      [text]: "0\uFFFD" // 0�
    }, {
      "UTF-8": "%C3%A0",
      "UTF-16BE": "%00%E0",
      "UTF-16LE": "%E0%00",
      [text]: "\u00E0" // à
    }, {
      "UTF-8 with BOM": "%E2%9D%A4",
      "UTF-16BE with BOM": "%27%64",
      "UTF-16LE with BOM": "%64%27",
      [text]: "\u2764" // ❤
    }, {
      "UTF-8": "%30%F0%9F%9A%80",
      "UTF-16BE": "%00%30%D8%3D%DE%80",
      "UTF-16LE": "%30%00%3D%D8%80%DE",
      [text]: "0\uD83D\uDE80" // 0🚀
    }
  ];

  const bom = {
    "UTF-8": "%EF%BB%BF",
    "UTF-16BE": "%FE%FF",
    "UTF-16LE": "%FF%FE"
  };

  for (let test of tests) {
    let result = test[text];
    for (let [encoding, data] of Object.entries(test)) {
      info("Testing " + JSON.stringify(result) + " encoded in " + encoding + ".");

      if (encoding.endsWith("BOM")) {
        data = bom[encoding.split(" ")[0]] + data;
      }

      yield addJsonViewTab("data:application/json," + data);
      yield selectJsonViewContentTab("rawdata");

      // Check displayed data.
      let output = yield getElementText(".textPanelBox .data");
      is(output, result, "The right data has been received.");
    }
  }
});
