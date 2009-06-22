/* Test case for ISO-2022-KR
 *
 * Uses nsIConverterInputStream to decode ISO-2022-KR text.
 *
 * Sample text is: 
 * 1: 소 잃고 외양간 고친다
 * 2: 빈 수레가 요란하다
 * 3: 하늘의 별 따기
 * 4: 아는 길도 물어가라
 *
 * (From http://en.wikiquote.org/wiki/Korean_proverbs)
 */

const sample = "%1B%24%29C1%3A%20%0E%3CR%0F%20%0E%40R0m%0F%20%0E%3F%5C%3Eg0%23%0F%20%0E0mD%234Y%0A2%3A%20%0E%3As%0F%20%0E%3Cv790%21%0F%20%0E%3Fd6uGO4Y%0A3%3A%20%0EGO4C%40G%0F%20%0E%3A0%0F%20%0E5%7B1b%0A4%3A%20%0E%3EF4B%0F%20%0E1f55%0F%20%0E90%3En0%216s";

const expected = "1: \uC18C \uC783\uACE0 \uC678\uC591\uAC04 \uACE0\uCE5C\uB2E4\n2: \uBE48 \uC218\uB808\uAC00 \uC694\uB780\uD558\uB2E4\n3: \uD558\uB298\uC758 \uBCC4 \uB530\uAE30\n4: \uC544\uB294 \uAE38\uB3C4 \uBB3C\uC5B4\uAC00\uB77C\n"; 

const charset="ISO-2022-KR";

function testCase(bufferLength)
{
  var dataURI = "data:text/plain;charset=" + charset + "," + sample;

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
      outStr += line.value + "\n";
  } while (more);

  if (outStr != expected) {
    dump("Failed with bufferLength = " + bufferLength + "\n");
    if (outStr.length == expected.length) {
      for (i = 0; i < outStr.length; ++i) {
	if (outStr.charCodeAt(i) != expected.charCodeAt(i)) {
	  dump(i + ": " + outStr.charCodeAt(i).toString(16) + " != " + expected.charCodeAt(i).toString(16) + "\n");
	}
      }
    }
  }

  // escape the strings before comparing for better readability
  do_check_eq(escape(outStr), escape(expected));
}

function run_test()
{
  testCase(34);
  testCase(35);
}
