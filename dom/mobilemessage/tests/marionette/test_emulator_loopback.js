/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;
MARIONETTE_HEAD_JS = 'head.js';

const SELF = "5554";

function randomString16() {
  return Math.random().toString(36).substr(2, 16);
}

function times(str, n) {
  return (new Array(n + 1)).join(str);
}

function test(aBody) {
  let promises = [];

  promises.push(waitForManagerEvent('received')
    .then(function(aEvent) {
      let message = aEvent.message;
      is(message.body, aBody, "message.body");
    }));

  promises.push(sendSmsWithSuccess(SELF, aBody));

  return Promise.all(promises);
}

const TEST_DATA = [
  // Random alphanumeric string of 16 characters.
  randomString16(),
  // Long long text message for multipart messages.
  times(randomString16(), 100),

  // UCS2 string for the first sentence of "The Cooing", Classic of Poetry.
  "\u95dc\u95dc\u96ce\u9ce9\uff0c\u5728\u6cb3\u4e4b\u6d32\u3002",
  // Long long UCS2 text message for multipart messages.
  times("\u95dc\u95dc\u96ce\u9ce9\uff0c\u5728\u6cb3\u4e4b\u6d32\u3002", 100),

  // Test case from Bug 809553
  "zertuuuppzuzyeueieieyeieoeiejeheejrueufjfjfjjfjfkxifjfjfufjjfjfufujdjduxxjdu"
  + "djdjdjdudhdjdhdjdbddhbfjfjxbuwjdjdudjddjdhdhdvdyudusjdudhdjjfjdvdudbddjdbd"
  + "usjfbjdfudjdhdjbzuuzyzehdjjdjwybwudjvwywuxjdbfudsbwuwbwjdjdbwywhdbddudbdjd"
  + "uejdhdudbdduwjdbjddudjdjdjdudjdbdjdhdhdjjdjbxudjdbxufjudbdjhdjdisjsjzusbzh"
  + "xbdudksksuqjgdjdb jeudi jeudis duhebevzcevevsvs DVD suscite eh du d des jv"
  + " y b Dj. Du  wh. Hu Deb wh. Du web h w show d DVD h w v.  Th\u00e9 \u00e9c"
  + "hec d whdvdj. Wh d'h\u00f4tel DVD. IMAX eusjw ii ce",

  // Android Emulator specific problems:
  // 1) wrong default 7Bit alphabet character for "Ň"(U+0147).
  "\u0147",
  // 2) problem in decoding strings encoded with GSM 7Bit Alphabets and
  // containing characters on extension tables.
  "\u20ac****",
];

startTestBase(function testCaseMain() {
  return ensureMobileMessage()
    .then(function() {
      let promise = Promise.resolve();

      for (let i = 0; i < TEST_DATA.length; i++) {
        let text = TEST_DATA[i];
        promise = promise.then(() => test(text));
      }

      return promise;
    });
});
