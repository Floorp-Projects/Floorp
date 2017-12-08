var str =
  '[\n' +
  '    "JSON Test Pattern pass1",\n' +
  '    {"object with 1 member":["array with 1 element"]},\n' +
  '    {},\n' +
  '    [],\n' +
  '    -42,\n' +
  '    true,\n' +
  '    false,\n' +
  '    null,\n' +
  '    {\n' +
  '        "integer": 1234567890,\n' +
  '        "real": -9876.543210,\n' +
  '        "e": 0.123456789e-12,\n' +
  '        "E": 1.234567890E+34,\n' +
  '        "":  23456789012E66,\n' +
  '        "zero": 0,\n' +
  '        "one": 1,\n' +
  '        "space": " ",\n' +
  '        "quote": "\\"",\n' +
  '        "backslash": "\\\\",\n' +
  '        "controls": "\\b\\f\\n\\r\\t",\n' +
  '        "slash": "/ & \\/",\n' +
  '        "alpha": "abcdefghijklmnopqrstuvwyz",\n' +
  '        "ALPHA": "ABCDEFGHIJKLMNOPQRSTUVWYZ",\n' +
  '        "digit": "0123456789",\n' +
  '        "0123456789": "digit",\n' +
  '        "special": "`1~!@#$%^&*()_+-={\':[,]}|;.</>?",\n' +
  '        "hex": "\\u0123\\u4567\\u89AB\\uCDEF\\uabcd\\uef4A",\n' +
  '        "true": true,\n' +
  '        "false": false,\n' +
  '        "null": null,\n' +
  '        "array":[  ],\n' +
  '        "object":{  },\n' +
  '        "address": "50 St. James Street",\n' +
  '        "url": "http://www.JSON.org/",\n' +
  '        "comment": "// /* <!-- --",\n' +
  '        "# -- --> */": " ",\n' +
  '        " s p a c e d " :[1,2 , 3\n' +
  '\n' +
  ',\n' +
  '\n' +
  '4 , 5        ,          6           ,7        ],"compact":[1,2,3,4,5,6,7],\n' +
  '        "jsontext": "{\\"object with 1 member\\":[\\"array with 1 element\\"]}",\n' +
  '        "quotes": "&#34; \\u0022 %22 0x22 034 &#x22;",\n' +
  '        "\\/\\\\\\"\\uCAFE\\uBABE\\uAB98\\uFCDE\\ubcda\\uef4A\\b\\f\\n\\r\\t`1~!@#$%^&*()_+-=[]{}|;:\',./<>?"\n' +
  ': "A key can be any string"\n' +
  '    },\n' +
  '    0.5 ,98.6\n' +
  ',\n' +
  '99.44\n' +
  ',\n' +
  '\n' +
  '1066,\n' +
  '1e1,\n' +
  '0.1e1,\n' +
  '1e-1,\n' +
  '1e00,2e+00,2e-00\n' +
  ',"rosebud"]\n';

var x = JSON.parse(str);

assertEq(x[0], "JSON Test Pattern pass1");
assertEq(x[1]["object with 1 member"][0], "array with 1 element");
assertEq(x[2].constructor, Object);
assertEq(x[3].constructor, Array);
assertEq(x[4], -42);
assertEq(x[5], true);
assertEq(x[6], false);
assertEq(x[7], null);
assertEq(x[8].constructor, Object);
assertEq(x[8]["integer"], 1234567890);
assertEq(x[8]["real"], -9876.543210);
assertEq(x[8]["e"], 0.123456789e-12);
assertEq(x[8]["E"], 1.234567890E+34);
assertEq(x[8][""], 23456789012E66);
assertEq(x[8]["zero"], 0);
assertEq(x[8]["one"], 1);
assertEq(x[8]["space"], " ");
assertEq(x[8]["quote"], "\"");
assertEq(x[8]["backslash"], "\\");
assertEq(x[8]["controls"], "\b\f\n\r\t");
assertEq(x[8]["slash"], "/ & /");
assertEq(x[8]["alpha"], "abcdefghijklmnopqrstuvwyz");
assertEq(x[8]["ALPHA"], "ABCDEFGHIJKLMNOPQRSTUVWYZ");
assertEq(x[8]["digit"], "0123456789");
assertEq(x[8]["0123456789"], "digit");
assertEq(x[8]["special"], "`1~!@#$%^&*()_+-={':[,]}|;.</>?");
assertEq(x[8]["hex"], "\u0123\u4567\u89AB\uCDEF\uabcd\uef4A");
assertEq(x[8]["true"], true);
assertEq(x[8]["false"], false);
assertEq(x[8]["null"], null);
assertEq(x[8]["array"].length, 0);
assertEq(x[8]["object"].constructor, Object);
assertEq(x[8]["address"], "50 St. James Street");
assertEq(x[8]["url"], "http://www.JSON.org/");
assertEq(x[8]["comment"], "// /* <!-- --");
assertEq(x[8]["# -- --> */"], " ");
assertEq(x[8][" s p a c e d "].length, 7);
assertEq(x[8]["compact"].length, 7);
assertEq(x[8]["jsontext"], "{\"object with 1 member\":[\"array with 1 element\"]}");
assertEq(x[8]["quotes"], "&#34; \u0022 %22 0x22 034 &#x22;");
assertEq(x[8]["\/\\\"\uCAFE\uBABE\uAB98\uFCDE\ubcda\uef4A\b\f\n\r\t`1~!@#$%^&*()_+-=[]{}|;:',./<>?"], "A key can be any string");
assertEq(x[9], 0.5);
assertEq(x[10], 98.6);
assertEq(x[11], 99.44);
assertEq(x[12], 1066);
assertEq(x[13], 1e1);
assertEq(x[14], 0.1e1);
assertEq(x[15], 1e-1);
assertEq(x[16], 1e00);
assertEq(x[17], 2e+00);
assertEq(x[18], 2e-00);
assertEq(x[19], "rosebud");

/******************************************************************************/

if (typeof reportCompare === "function")
  reportCompare(true, true);

print("Tests complete");
