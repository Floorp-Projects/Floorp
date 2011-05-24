/* Test case for bug 90411
 *
 * Uses nsIConverterInputStream to decode GB_HK test.
 *
 * Sample text is: 
 * 问他谁是傻瓜了5分钟。但是，他谁不要求仍然是一个傻瓜永远
 * 我听见 我忘记; 我看见 我记住; 我做 我了解。
 */

const sample = "~{NJK{K-JGI59OAK~}5~{7VVS!%235+JG%23,K{K-2;R*GsHTH;JGR;8vI59OS@T6!%23~} ~{NRL}<{~} ~{NRM|<G~}; ~{NR?4<{~} ~{NR<GW!~}; ~{NRWv~} ~{NRAK=b!%23~}";

const expected = "\u95EE\u4ED6\u8C01\u662F\u50BB\u74DC\u4E865\u5206\u949F\u3002\u4F46\u662F\uFF0C\u4ED6\u8C01\u4E0D\u8981\u6C42\u4ECD\u7136\u662F\u4E00\u4E2A\u50BB\u74DC\u6C38\u8FDC\u3002 \u6211\u542C\u89C1 \u6211\u5FD8\u8BB0; \u6211\u770B\u89C1 \u6211\u8BB0\u4F4F; \u6211\u505A \u6211\u4E86\u89E3\u3002"; 

const charset="HZ-GB-2312";

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
      outStr += line.value;
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
  testCase(32);
  testCase(33);
}
