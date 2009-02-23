/* Test case for bug 317216
 *
 * Uses nsIConverterInputStream to decode UTF-16 text with valid surrogate
 * pairs and lone surrogate characters
 *
 * Sample text is: "A" in Mathematical Bold Capitals (U+1D400)
 *
 * The test uses buffers of 4 different lengths to test end of buffer in mid-
 * UTF16 character and mid-surrogate pair
 */

const test = [
// 0: Valid surrogate pair
              ["%00%2D%00%2D%D8%35%DC%20%00%2D%00%2D",
//    expected: surrogate pair
               "--\uD835\uDC20--"],
// 1: Lone high surrogate
              ["%00%2D%00%2D%D8%35%00%2D%00%2D",
//    expected: one replacement char
               "--\uFFFD--"],
// 2: Lone low surrogate
              ["%00%2D%00%2D%DC%20%00%2D%00%2D",
//    expected: one replacement char
               "--\uFFFD--"],
// 3: Two high surrogates
              ["%00%2D%00%2D%D8%35%D8%35%00%2D%00%2D",
//    expected: two replacement chars
               "--\uFFFD\uFFFD--"],
// 4: Two low surrogates
              ["%00%2D%00%2D%DC%20%DC%20%00%2D%00%2D",
//    expected: two replacement chars
              "--\uFFFD\uFFFD--"],
// 5: Low surrogate followed by high surrogate
              ["%00%2D%00%2D%DC%20%D8%35%00%2D%00%2D",
//    expected: two replacement chars
               "--\uFFFD\uFFFD--"],
// 6: Lone high surrogate followed by valid surrogate pair
              ["%00%2D%00%2D%D8%35%D8%35%DC%20%00%2D%00%2D",
//    expected: replacement char followed by surrogate pair
               "--\uFFFD\uD835\uDC20--"],
// 7: Lone low surrogate followed by valid surrogate pair
              ["%00%2D%00%2D%DC%20%D8%35%DC%20%00%2D%00%2D",
//    expected: replacement char followed by surrogate pair
               "--\uFFFD\uD835\uDC20--"],
// 8: Valid surrogate pair followed by lone high surrogate
              ["%00%2D%00%2D%D8%35%DC%20%D8%35%00%2D%00%2D",
//    expected: surrogate pair followed by replacement char
               "--\uD835\uDC20\uFFFD--"],
// 9: Valid surrogate pair followed by lone low surrogate
              ["%00%2D%00%2D%D8%35%DC%20%DC%20%00%2D%00%2D",
//    expected: surrogate pair followed by replacement char
               "--\uD835\uDC20\uFFFD--"],
// 10: Lone high surrogate at the end of the input
              ["%00%2D%00%2D%00%2D%00%2D%D8%35%",
//    expected: nothing
               "----"],
// 11: Half code unit at the end of the input
              ["%00%2D%00%2D%00%2D%00%2D%D8",
//    expected: nothing
              "----"]];

const IOService = Components.Constructor("@mozilla.org/network/io-service;1",
                                         "nsIIOService");
const ConverterInputStream =
      Components.Constructor("@mozilla.org/intl/converter-input-stream;1",
                             "nsIConverterInputStream",
                             "init");
const ios = new IOService();

function testCase(testText, expectedText, bufferLength, charset)
{
  var dataURI = "data:text/plain;charset=" + charset + "," + testText;

  var channel = ios.newChannel(dataURI, "", null);
  var testInputStream = channel.open();
  var testConverter = new ConverterInputStream(testInputStream,
                                               charset,
                                               bufferLength,
                                               0xFFFD);

  if (!(testConverter instanceof
        Components.interfaces.nsIUnicharLineInputStream))
    throw "not line input stream";

  var outStr = "";
  var more;
  do {
    // read the line and check for eof
    var line = {};
    more = testConverter.readLine(line);
    outStr += line.value;
  } while (more);

  // escape the strings before comparing for better readability
  do_check_eq(escape(outStr), escape(expectedText));
}

// Byte-swap %-encoded utf-16
function flip(str) { return str.replace(/(%..)(%..)/g, "$2$1"); }

function run_test()
{
  for (var i = 0; i < 12; ++i) {
    for (var bufferLength = 4; bufferLength < 8; ++ bufferLength) {
      testCase(test[i][0], test[i][1], bufferLength, "UTF-16BE");
      testCase(flip(test[i][0]), test[i][1], bufferLength, "UTF-16LE");
    }
  }
}
