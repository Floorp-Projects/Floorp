/* Tests conversion from ISO-2022-KR to Unicode (bug 449578)
 */

// designator sequence at beginning of line - legal
const in1 = "%1B$)C%0E0!3*4Y6s%0F 1234";     
// empty non-ASCII sequence -- illegal
const in2 = "%1B$)Cab%0E%0Fcd";
// designator sequence not at beginning of line - illegal
const in3 = "abc %1B$)C%0E0!3*4Y6s%0F 1234";

const expected1 = "\uAC00\uB098\uB2E4\uB77C 1234";
const expected2 = "ab\uFFFD\cd";
const expected3 = "abc \u001B$)C\uAC00\uB098\uB2E4\uB77C 1234";

function testCase(inStr, expected)
{
    var dataURI = "data:text/plain;charset=ISO-2022-KR," + inStr;

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
						 "ISO-2022-KR",
						 8192,
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

    do_check_eq(outStr, expected);
}
    
function run_test()
{
    testCase(in1, expected1);
    testCase(in2, expected2);
    testCase(in3, expected3);
}
