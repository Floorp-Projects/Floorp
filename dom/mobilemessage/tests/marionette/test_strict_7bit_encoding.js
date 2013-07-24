/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/ */

MARIONETTE_TIMEOUT = 60000;

// Copied from ril_consts.js. Some entries are commented out in ril_const.js,
// but we still want to test them here.
const GSM_SMS_STRICT_7BIT_CHARMAP = {
  "\u0024": "\u0024", // "$" => "$", already in default alphabet
  "\u00a5": "\u00a5", // "¥" => "¥", already in default alphabet
  "\u00c0": "\u0041", // "À" => "A"
  "\u00c1": "\u0041", // "Á" => "A"
  "\u00c2": "\u0041", // "Â" => "A"
  "\u00c3": "\u0041", // "Ã" => "A"
  "\u00c4": "\u00c4", // "Ä" => "Ä", already in default alphabet
  "\u00c5": "\u00c5", // "Å" => "Å", already in default alphabet
  "\u00c6": "\u00c6", // "Æ" => "Æ", already in default alphabet
  "\u00c7": "\u00c7", // "Ç" => "Ç", already in default alphabet
  "\u00c8": "\u0045", // "È" => "E"
  "\u00c9": "\u00c9", // "É" => "É", already in default alphabet
  "\u00ca": "\u0045", // "Ê" => "E"
  "\u00cb": "\u0045", // "Ë" => "E"
  "\u00cc": "\u0049", // "Ì" => "I"
  "\u00cd": "\u0049", // "Í" => "I"
  "\u00ce": "\u0049", // "Î" => "I"
  "\u00cf": "\u0049", // "Ï" => "I"
  "\u00d1": "\u00d1", // "Ñ" => "Ñ", already in default alphabet
  "\u00d2": "\u004f", // "Ò" => "O"
  "\u00d3": "\u004f", // "Ó" => "O"
  "\u00d4": "\u004f", // "Ô" => "O"
  "\u00d5": "\u004f", // "Õ" => "O"
  "\u00d6": "\u00d6", // "Ö" => "Ö", already in default alphabet
  "\u00d9": "\u0055", // "Ù" => "U"
  "\u00da": "\u0055", // "Ú" => "U"
  "\u00db": "\u0055", // "Û" => "U"
  "\u00dc": "\u00dc", // "Ü" => "Ü", already in default alphabet
  "\u00df": "\u00df", // "ß" => "ß", already in default alphabet
  "\u00e0": "\u00e0", // "à" => "à", already in default alphabet
  "\u00e1": "\u0061", // "á" => "a"
  "\u00e2": "\u0061", // "â" => "a"
  "\u00e3": "\u0061", // "ã" => "a"
  "\u00e4": "\u00e4", // "ä" => "ä", already in default alphabet
  "\u00e5": "\u00e5", // "å" => "å", already in default alphabet
  "\u00e6": "\u00e6", // "æ" => "æ", already in default alphabet
  "\u00e7": "\u00c7", // "ç" => "Ç"
  "\u00e8": "\u00e8", // "è" => "è", already in default alphabet
  "\u00e9": "\u00e9", // "é" => "é", already in default alphabet
  "\u00ea": "\u0065", // "ê" => "e"
  "\u00eb": "\u0065", // "ë" => "e"
  "\u00ec": "\u00ec", // "ì" => "ì", already in default alphabet
  "\u00ed": "\u0069", // "í" => "i"
  "\u00ee": "\u0069", // "î" => "i"
  "\u00ef": "\u0069", // "ï" => "i"
  "\u00f1": "\u00f1", // "ñ" => "ñ", already in default alphabet
  "\u00f2": "\u00f2", // "ò" => "ò", already in default alphabet
  "\u00f3": "\u006f", // "ó" => "o"
  "\u00f4": "\u006f", // "ô" => "o"
  "\u00f5": "\u006f", // "õ" => "o"
  "\u00f6": "\u00f6", // "ö" => "ö", already in default alphabet
  "\u00f8": "\u00f8", // "ø" => "ø", already in default alphabet
  "\u00f9": "\u00f9", // "ù" => "ù", already in default alphabet
  "\u00fa": "\u0075", // "ú" => "u"
  "\u00fb": "\u0075", // "û" => "u"
  "\u00fc": "\u00fc", // "ü" => "ü", already in default alphabet
  "\u00fe": "\u0074", // "þ" => "t"
  "\u0100": "\u0041", // "Ā" => "A"
  "\u0101": "\u0061", // "ā" => "a"
  "\u0106": "\u0043", // "Ć" => "C"
  "\u0107": "\u0063", // "ć" => "c"
  "\u010c": "\u0043", // "Č" => "C"
  "\u010d": "\u0063", // "č" => "c"
  "\u010f": "\u0064", // "ď" => "d"
  "\u0110": "\u0044", // "Đ" => "D"
  "\u0111": "\u0064", // "đ" => "d"
  "\u0112": "\u0045", // "Ē" => "E"
  "\u0113": "\u0065", // "ē" => "e"
  "\u0118": "\u0045", // "Ę" => "E"
  "\u0119": "\u0065", // "ę" => "e"
  "\u0128": "\u0049", // "Ĩ" => "I"
  "\u0129": "\u0069", // "ĩ" => "i"
  "\u012a": "\u0049", // "Ī" => "I"
  "\u012b": "\u0069", // "ī" => "i"
  "\u012e": "\u0049", // "Į" => "I"
  "\u012f": "\u0069", // "į" => "i"
  "\u0141": "\u004c", // "Ł" => "L"
  "\u0142": "\u006c", // "ł" => "l"
  "\u0143": "\u004e", // "Ń" => "N"
  "\u0144": "\u006e", // "ń" => "n"
  "\u0147": "\u004e", // "Ň" => "N"
  "\u0148": "\u006e", // "ň" => "n"
  "\u014c": "\u004f", // "Ō" => "O"
  "\u014d": "\u006f", // "ō" => "o"
  "\u0152": "\u004f", // "Œ" => "O"
  "\u0153": "\u006f", // "œ" => "o"
  "\u0158": "\u0052", // "Ř" => "R"
  "\u0159": "\u0072", // "ř" => "r"
  "\u0160": "\u0053", // "Š" => "S"
  "\u0161": "\u0073", // "š" => "s"
  "\u0165": "\u0074", // "ť" => "t"
  "\u0168": "\u0055", // "Ū" => "U"
  "\u0169": "\u0075", // "ū" => "u"
  "\u016a": "\u0055", // "Ū" => "U"
  "\u016b": "\u0075", // "ū" => "u"
  "\u0178": "\u0059", // "Ÿ" => "Y"
  "\u0179": "\u005a", // "Ź" => "Z"
  "\u017a": "\u007a", // "ź" => "z"
  "\u017b": "\u005a", // "Ż" => "Z"
  "\u017c": "\u007a", // "ż" => "z"
  "\u017d": "\u005a", // "Ž" => "Z"
  "\u017e": "\u007a", // "ž" => "z"
  "\u025b": "\u0045", // "ɛ" => "E"
  "\u0398": "\u0398", // "Θ" => "Θ", already in default alphabet
  "\u1e7c": "\u0056", // "Ṽ" => "V"
  "\u1e7d": "\u0076", // "ṽ" => "v"
  "\u1ebc": "\u0045", // "Ẽ" => "E"
  "\u1ebd": "\u0065", // "ẽ" => "e"
  "\u1ef8": "\u0059", // "Ỹ" => "Y"
  "\u1ef9": "\u0079", // "ỹ" => "y"
  "\u20a4": "\u00a3", // "₤" => "£"
  "\u20ac": "\u20ac", // "€" => "€", already in default alphabet
};

// Emulator will loop back the outgoing SMS if the phone number equals to its
// control port, which is 5554 for the first emulator instance.
const SELF = "5554";

SpecialPowers.setBoolPref("dom.sms.enabled", true);
SpecialPowers.addPermission("sms", true, document);

let manager = window.navigator.mozMobileMessage;
ok(manager instanceof MozMobileMessageManager);

let tasks = {
  // List of test fuctions. Each of them should call |tasks.next()| when
  // completed or |tasks.finish()| to jump to the last one.
  _tasks: [],
  _nextTaskIndex: 0,

  push: function push(func) {
    this._tasks.push(func);
  },

  next: function next() {
    let index = this._nextTaskIndex++;
    let task = this._tasks[index];
    try {
      task();
    } catch (ex) {
      ok(false, "test task[" + index + "] throws: " + ex);
      // Run last task as clean up if possible.
      if (index != this._tasks.length - 1) {
        this.finish();
      }
    }
  },

  finish: function finish() {
    this._tasks[this._tasks.length - 1]();
  },

  run: function run() {
    this.next();
  }
};

function testStrict7BitEncodingHelper(sent, received) {
  // The log message contains unicode and Marionette seems unable to process
  // it and throws: |UnicodeEncodeError: 'ascii' codec can't encode character
  // u'\xa5' in position 14: ordinal not in range(128)|.
  //
  //log("Testing '" + sent + "' => '" + received + "'");

  let count = 0;
  function done(step) {
    count += step;
    if (count >= 2) {
      window.setTimeout(tasks.next.bind(tasks), 0);
    }
  }

  manager.addEventListener("received", function onReceived(event) {
    event.target.removeEventListener("received", onReceived);

    let message = event.message;
    is(message.body, received, "received message.body");

    done(1);
  });

  let request = manager.send(SELF, sent);
  request.addEventListener("success", function onRequestSuccess(event) {
    let message = event.target.result;
    is(message.body, sent, "sent message.body");

    done(1);
  });
  request.addEventListener("error", function onRequestError(event) {
    ok(false, "Can't send message out!!!");
    done(2);
  });
}

// Bug 877141 - If you send several spaces together in a sms, the other
//              dipositive receives a "*" for each space.
//
// This function is called twice, with strict 7bit encoding enabled or
// disabled.  Expect the same result in both sent and received text and with
// either strict 7bit encoding enabled or disabled.
function testBug877141() {
  log("Testing bug 877141");
  let sent = "1 2     3";
  testStrict7BitEncodingHelper(sent, sent);
}

tasks.push(function () {
  log("Testing with dom.sms.strict7BitEncoding enabled");
  SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", true);
  tasks.next();
});


// Test for combined string.
tasks.push(function () {
  let sent = "", received = "";
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    sent += c;
    received += GSM_SMS_STRICT_7BIT_CHARMAP[c];
  }
  testStrict7BitEncodingHelper(sent, received);
});

// When strict7BitEncoding is enabled, we should replace characters that
// can't be encoded with GSM 7-Bit alphabets with '*'.
tasks.push(function () {
  // "Happy New Year" in Chinese.
  let sent = "\u65b0\u5e74\u5feb\u6a02", received = "****";
  testStrict7BitEncodingHelper(sent, received);
});

tasks.push(testBug877141);

tasks.push(function () {
  log("Testing with dom.sms.strict7BitEncoding disabled");
  SpecialPowers.setBoolPref("dom.sms.strict7BitEncoding", false);
  tasks.next();
});

// Test for combined string.
tasks.push(function () {
  let sent = "";
  for (let c in GSM_SMS_STRICT_7BIT_CHARMAP) {
    sent += c;
  }
  testStrict7BitEncodingHelper(sent, sent);
});

tasks.push(function () {
  // "Happy New Year" in Chinese.
  let sent = "\u65b0\u5e74\u5feb\u6a02";
  testStrict7BitEncodingHelper(sent, sent);
});

tasks.push(testBug877141);

// WARNING: All tasks should be pushed before this!!!
tasks.push(function cleanUp() {
  SpecialPowers.removePermission("sms", document);
  SpecialPowers.clearUserPref("dom.sms.enabled");
  SpecialPowers.clearUserPref("dom.sms.strict7BitEncoding");

  finish();
});

tasks.run();
