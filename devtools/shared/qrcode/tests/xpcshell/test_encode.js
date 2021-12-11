/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Test encoding a simple message.
 */

const { require } = ChromeUtils.import("resource://devtools/shared/Loader.jsm");

const QR = require("devtools/shared/qrcode/index");

function run_test() {
  const imgData = QR.encodeToDataURI("HELLO", "L");
  Assert.equal(
    imgData.src,
    "data:image/gif;base64,R0lGODdhOgA6AIAAAAAAAP///ywAAAAAOgA6AAAC" +
      "/4yPqcvtD6OctNqLs968+w+G4gKU5nkaKKquLuW+QVy2tAkDTj3rfQts8CRDko" +
      "+HPPoYRUgy9YsyldDm44mLWhHYZM6W7WaDqyCRGkZDySxpRGw2sqvLt1q5w/fo" +
      "XyE6vnUQOJUHBlinMGh046V1F5PDqNcoqcgBOWKBKbK2N+aY+Ih49VkmqMcl2l" +
      "dkhZUK1umE6jZXJ2ZJaujZaRqH4bpb2uZrJxvIt4Ebe9qoYYrJOsw8apz2bCut" +
      "m9kqDcw52uuImyr5Oh1KXH1jrn2anuunywtODU/o2c6teceW39ZcLFg/fNMo1b" +
      "t3jVw2dwTPwJq1KYG3gAklCgu37yGxeScYKyiCc+7DR34hPVQiuQ7UhJMagyEb" +
      "lymmzJk0a9q8iTOnzp0NCgAAOw=="
  );
  Assert.equal(imgData.width, 58);
  Assert.equal(imgData.height, 58);
}
