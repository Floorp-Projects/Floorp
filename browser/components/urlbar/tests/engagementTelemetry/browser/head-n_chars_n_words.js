/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

/* import-globals-from head.js */

async function doNCharsTest({ trigger, assert }) {
  for (const input of ["x", "xx", "xx x", "xx x "]) {
    await doTest(async browser => {
      await openPopup(input);

      await trigger();
      await assert(input.length);
    });
  }
}

async function doNCharsWithOverMaxTextLengthCharsTest({ trigger, assert }) {
  await doTest(async browser => {
    let input = "";
    for (let i = 0; i < UrlbarUtils.MAX_TEXT_LENGTH * 2; i++) {
      input += "x";
    }
    await openPopup(input);

    await trigger();
    await assert(UrlbarUtils.MAX_TEXT_LENGTH * 2);
  });
}

async function doNWordsTest({ trigger, assert }) {
  for (const input of ["x", "xx", "xx x", "xx x "]) {
    await doTest(async browser => {
      await openPopup(input);

      await trigger();
      const splits = input.trim().split(" ");
      await assert(splits.length);
    });
  }
}

async function doNWordsWithOverMaxTextLengthCharsTest({ trigger, assert }) {
  await doTest(async browser => {
    const word = "1234 ";
    let input = "";
    while (input.length < UrlbarUtils.MAX_TEXT_LENGTH * 2) {
      input += word;
    }
    await openPopup(input);

    await trigger();
    await assert(UrlbarUtils.MAX_TEXT_LENGTH / word.length);
  });
}
