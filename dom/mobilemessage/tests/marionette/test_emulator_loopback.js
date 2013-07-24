/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

const SELF = "5554";

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  finish();
}

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager);

function randomString16() {
  return Math.random().toString(36).substr(2, 16);
}

function times(str, n) {
  return (new Array(n + 1)).join(str);
}

function repeat(func, array, oncomplete) {
  (function do_call(index) {
    let next = index < (array.length - 1) ? do_call.bind(null, index + 1) : oncomplete;
    func.apply(null, [array[index], next]);
  })(0);
}

function doTest(body, callback) {
  manager.addEventListener("received", function onReceived(event) {
    event.target.removeEventListener(event.type, arguments.callee);

    let message = event.message;
    is(message.body, body, "message.body");

    window.setTimeout(callback, 0);
  });

  let request = manager.send(SELF, body);
  request.onerror = function onerror() {
    ok(false, "failed to send message '" + body + "' to '" + SELF + "'");
  };
}

repeat(doTest, [
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
], cleanUp);
