/* Test case for bug 335531
 *
 * Uses nsIConverterInputStream to decode UTF-16 text with all combinations
 * of UTF-16BE and UTF-16LE with and without BOM.
 *
 * Sample text is: "Все счастливые семьи похожи друг на друга, каждая несчастливая семья несчастлива по-своему."
 *
 * The enclosing quotation marks are included in the sample text to test that
 * UTF-16LE is recognized even when there is no BOM and the UTF-16LE decoder is
 * not explicitly called. This only works when the first character of the text
 * is an eight-bit character.
 */

const beBOM="%00%00%FE%FF";
const leBOM="%FF%FE%00%00";
const outBOM="\uFEFF";
const sampleUTF32BE="%00%00%00%22%00%00%04%12%00%00%04%41%00%00%04%35%00%00%00%20%00%00%04%41%00%00%04%47%00%00%04%30%00%00%04%41%00%00%04%42%00%00%04%3B%00%00%04%38%00%00%04%32%00%00%04%4B%00%00%04%35%00%00%00%20%00%00%04%41%00%00%04%35%00%00%04%3C%00%00%04%4C%00%00%04%38%00%00%00%20%00%00%04%3F%00%00%04%3E%00%00%04%45%00%00%04%3E%00%00%04%36%00%00%04%38%00%00%00%20%00%00%04%34%00%00%04%40%00%00%04%43%00%00%04%33%00%00%00%20%00%00%04%3D%00%00%04%30%00%00%00%20%00%00%04%34%00%00%04%40%00%00%04%43%00%00%04%33%00%00%04%30%00%00%00%2C%00%00%00%20%00%00%04%3A%00%00%04%30%00%00%04%36%00%00%04%34%00%00%04%30%00%00%04%4F%00%00%00%20%00%00%04%3D%00%00%04%35%00%00%04%41%00%00%04%47%00%00%04%30%00%00%04%41%00%00%04%42%00%00%04%3B%00%00%04%38%00%00%04%32%00%00%04%30%00%00%04%4F%00%00%00%20%00%00%04%41%00%00%04%35%00%00%04%3C%00%00%04%4C%00%00%04%4F%00%00%00%20%00%00%04%3D%00%00%04%35%00%00%04%41%00%00%04%47%00%00%04%30%00%00%04%41%00%00%04%42%00%00%04%3B%00%00%04%38%00%00%04%32%00%00%04%30%00%00%00%20%00%00%04%3F%00%00%04%3E%00%00%00%2D%00%00%04%41%00%00%04%32%00%00%04%3E%00%00%04%35%00%00%04%3C%00%00%04%43%00%00%00%2E%00%00%00%22";
const sampleUTF32LE="%22%00%00%00%12%04%00%00%41%04%00%00%35%04%00%00%20%00%00%00%41%04%00%00%47%04%00%00%30%04%00%00%41%04%00%00%42%04%00%00%3B%04%00%00%38%04%00%00%32%04%00%00%4B%04%00%00%35%04%00%00%20%00%00%00%41%04%00%00%35%04%00%00%3C%04%00%00%4C%04%00%00%38%04%00%00%20%00%00%00%3F%04%00%00%3E%04%00%00%45%04%00%00%3E%04%00%00%36%04%00%00%38%04%00%00%20%00%00%00%34%04%00%00%40%04%00%00%43%04%00%00%33%04%00%00%20%00%00%00%3D%04%00%00%30%04%00%00%20%00%00%00%34%04%00%00%40%04%00%00%43%04%00%00%33%04%00%00%30%04%00%00%2C%00%00%00%20%00%00%00%3A%04%00%00%30%04%00%00%36%04%00%00%34%04%00%00%30%04%00%00%4F%04%00%00%20%00%00%00%3D%04%00%00%35%04%00%00%41%04%00%00%47%04%00%00%30%04%00%00%41%04%00%00%42%04%00%00%3B%04%00%00%38%04%00%00%32%04%00%00%30%04%00%00%4F%04%00%00%20%00%00%00%41%04%00%00%35%04%00%00%3C%04%00%00%4C%04%00%00%4F%04%00%00%20%00%00%00%3D%04%00%00%35%04%00%00%41%04%00%00%47%04%00%00%30%04%00%00%41%04%00%00%42%04%00%00%3B%04%00%00%38%04%00%00%32%04%00%00%30%04%00%00%20%00%00%00%3F%04%00%00%3E%04%00%00%2D%00%00%00%41%04%00%00%32%04%00%00%3E%04%00%00%35%04%00%00%3C%04%00%00%43%04%00%00%2E%00%00%00%22%00%00%00";
const expectedNoBOM = "\"\u0412\u0441\u0435 \u0441\u0447\u0430\u0441\u0442\u043B\u0438\u0432\u044B\u0435 \u0441\u0435\u043C\u044C\u0438 \u043F\u043E\u0445\u043E\u0436\u0438 \u0434\u0440\u0443\u0433 \u043D\u0430 \u0434\u0440\u0443\u0433\u0430, \u043A\u0430\u0436\u0434\u0430\u044F \u043D\u0435\u0441\u0447\u0430\u0441\u0442\u043B\u0438\u0432\u0430\u044F \u0441\u0435\u043C\u044C\u044F \u043D\u0435\u0441\u0447\u0430\u0441\u0442\u043B\u0438\u0432\u0430 \u043F\u043E-\u0441\u0432\u043E\u0435\u043C\u0443.\""; 

function makeText(withBOM, charset)
{
  var theText = eval("sample" + charset);
  if (withBOM) {
    if (charset == "UTF32BE") {
      theText = beBOM + theText;
    } else {
      theText = leBOM + theText;
    }
  }
  return theText;
}

function testCase(withBOM, charset, charsetDec, decoder, bufferLength)
{
  var dataURI = "data:text/plain;charset=" + charsetDec + "," +
                 makeText(withBOM, charset);

  var IOService = Components.Constructor("@mozilla.org/network/io-service;1",
					 "nsIIOService");
  var ConverterInputStream =
      Components.Constructor("@mozilla.org/intl/converter-input-stream;1",
			     "nsIConverterInputStream",
			     "init");

  var ios = new IOService();
  var channel = ios.newChannel(dataURI, "", null);
  var testInputStream = channel.open();
  var testConverter = new ConverterInputStream(testInputStream,
					       decoder,
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

  var expected = expectedNoBOM;
  if (withBOM) {
    // BE / LE decoder wouldn't strip the BOM
    if (decoder == "UTF-32BE" || decoder == "UTF-32LE") {
      expected = outBOM + expectedNoBOM;
    }
  }

  do_check_eq(outStr, expected);
}

// Tests conversion of one to three byte(s) from UTF-32 to Unicode

const expectedString = "\ufffd";

const charset = "UTF-32";

function testCase2(inString) {
    var ScriptableUnicodeConverter =
	Components.Constructor("@mozilla.org/intl/scriptableunicodeconverter",
			       "nsIScriptableUnicodeConverter");

    var converter = new ScriptableUnicodeConverter();
    converter.charset = charset;
    var outString;
    try {
      outString = converter.ConvertToUnicode(inString) + converter.Finish();
    } catch(e) {
      outString = "\ufffd";
    }
    do_check_eq(escape(outString), escape(expectedString));
}

/*
 * Uses nsIConverterInputStream to decode UTF-32 text with surrogate characters
 *
 * Sample text is: "g" in Mathematical Bold Symbolls (U+1D420)
 *
 * The test uses buffers of 4 different lengths to test end of buffer in mid-
 * UTF32 character
 */

// Single supplementaly character
// expected: surrogate pair
const test0="%00%00%00%2D%00%00%00%2D%00%01%D4%20%00%00%00%2D%00%00%00%2D";
const expected0 = "--\uD835\uDC20--";
// High surrogate followed by low surrogate (invalid in UTF-32)
// expected: two replacement chars
const test1="%00%00%00%2D%00%00%00%2D%00%00%D8%35%00%00%DC%20%00%00%00%2D%00%00%00%2D";
const expected1 = "--\uFFFD\uFFFD--";
// Lone high surrogate
// expected: one replacement char
const test2="%00%00%00%2D%00%00%00%2D%00%00%D8%35%00%00%00%2D%00%00%00%2D";
const expected2 = "--\uFFFD--";
// Lone low surrogate
// expected: one replacement char
const test3="%00%00%00%2D%00%00%00%2D%00%00%DC%20%00%00%00%2D%00%00%00%2D";
const expected3 = "--\uFFFD--";
// Two high surrogates
// expected: two replacement chars
const test4="%00%00%00%2D%00%00%00%2D%00%00%D8%35%00%00%D8%35%00%00%00%2D%00%00%00%2D";
const expected4 = "--\uFFFD\uFFFD--";
// Two low surrogates
// expected: two replacement chars
const test5="%00%00%00%2D%00%00%00%2D%00%00%DC%20%00%00%DC%20%00%00%00%2D%00%00%00%2D";
const expected5 = "--\uFFFD\uFFFD--";
// Low surrogate followed by high surrogate
// expected: two replacement chars
const test6="%00%00%00%2D%00%00%00%2D%00%00%DC%20%00%00%D8%35%00%00%00%2D%00%00%00%2D";
const expected6 = "--\uFFFD\uFFFD--";
// Lone high surrogate followed by supplementaly character
// expected: replacement char followed by surrogate pair
const test7="%00%00%00%2D%00%00%00%2D%00%00%D8%35%00%01%D4%20%00%00%00%2D%00%00%00%2D";
const expected7 = "--\uFFFD\uD835\uDC20--";
// Lone low surrogate followed by supplementaly character
// expected: replacement char followed by surrogate pair
const test8="%00%00%00%2D%00%00%00%2D%00%00%DC%20%00%01%D4%20%00%00%00%2D%00%00%00%2D";
const expected8 = "--\uFFFD\uD835\uDC20--";
// Supplementaly character followed by lone high surrogate
// expected: surrogate pair followed by replacement char
const test9="%00%00%00%2D%00%00%00%2D%00%01%D4%20%00%00%D8%35%00%00%00%2D%00%00%00%2D";
const expected9 = "--\uD835\uDC20\uFFFD--";
// Supplementaly character followed by lone low surrogate
// expected: surrogate pair followed by replacement char
const test10="%00%00%00%2D%00%00%00%2D%00%01%D4%20%00%00%DC%20%00%00%00%2D%00%00%00%2D";
const expected10 = "--\uD835\uDC20\uFFFD--";
// Lone high surrogate at the end of the input
// expected: one replacement char (invalid in UTF-32)
const test11="%00%00%00%2D%00%00%00%2D%00%00%00%2D%00%00%00%2D%00%00%D8%35";
const expected11 = "----\uFFFD";
// Half code unit at the end of the input
// expected: nothing
const test12="%00%00%00%2D%00%00%00%2D%00%00%00%2D%00%00%00%2D%D8";
const expected12 = "----";

function testCase3(testNumber, bufferLength)
{
  var dataURI = "data:text/plain;charset=UTF32BE," + eval("test" + testNumber);

  var IOService = Components.Constructor("@mozilla.org/network/io-service;1",
					 "nsIIOService");
  var ConverterInputStream =
      Components.Constructor("@mozilla.org/intl/converter-input-stream;1",
			     "nsIConverterInputStream",
			     "init");

  var ios = new IOService();
  var channel = ios.newChannel(dataURI, "", null);
  var testInputStream = channel.open();
  var testConverter = new ConverterInputStream(testInputStream,
					       "UTF-32BE",
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
  do_check_eq(escape(outStr), escape(eval("expected" + testNumber)));
}

function run_test()
{
    /*       BOM    charset    charset   decoder     buffer
                               declaration           length */
    testCase(true,  "UTF32LE", "UTF-32", "UTF-32",   64);
    testCase(true,  "UTF32BE", "UTF-32", "UTF-32",   64);
    testCase(true,  "UTF32LE", "UTF-32", "UTF-32LE", 64);
    testCase(true,  "UTF32BE", "UTF-32", "UTF-32BE", 64);
    testCase(false, "UTF32LE", "UTF-32", "UTF-32",   64);
    testCase(false, "UTF32BE", "UTF-32", "UTF-32",   64);
    testCase(false, "UTF32LE", "UTF-32", "UTF-32LE", 64);
    testCase(false, "UTF32BE", "UTF-32", "UTF-32BE", 64);
    testCase(true,  "UTF32LE", "UTF-32", "UTF-32",   65);
    testCase(true,  "UTF32BE", "UTF-32", "UTF-32",   65);
    testCase(true,  "UTF32LE", "UTF-32", "UTF-32LE", 65);
    testCase(true,  "UTF32BE", "UTF-32", "UTF-32BE", 65);
    testCase(false, "UTF32LE", "UTF-32", "UTF-32",   65);
    testCase(false, "UTF32BE", "UTF-32", "UTF-32",   65);
    testCase(false, "UTF32LE", "UTF-32", "UTF-32LE", 65);
    testCase(false, "UTF32BE", "UTF-32", "UTF-32BE", 65);

    testCase2("A");
    testCase2("AB");
    testCase2("ABC");

    for (var test = 0; test <= 12; ++ test) {
        for (var bufferLength = 4; bufferLength < 8; ++ bufferLength) {
            testCase3(test, bufferLength);
        }
    }
}
