function run_test()
{
  test_generate_xpath();
}

// TEST CODE

function test_generate_xpath()
{
  let docString = `
    <html>
    <body>
      <label><input type="checkbox" id="input1" />Input 1</label>
      <label><input type="checkbox" id="input2'" />Input 2</label>
      <label><input type="checkbox" id='"input3"' />Input 3</label>
      <label><input type="checkbox"/>Input 4</label>
      <label><input type="checkbox" />Input 5</label>
    </body>
    </html>
  `;
  let doc = DOMParser().parseFromString(docString, "text/html");

  // Test generate xpath for body.
  do_print("Test generate xpath for body node");
  let body = doc.getElementsByTagName("body")[0];
  let bodyXPath = body.generateXPath();
  let bodyExpXPath = "/xhtml:html/xhtml:body";
  equal(bodyExpXPath, bodyXPath, " xpath generated for body");

  // Test generate xpath for input with id.
  do_print("Test generate xpath for input with id");
  let inputWithId = doc.getElementById("input1");
  let inputWithIdXPath = inputWithId.generateXPath();
  let inputWithIdExpXPath = "//xhtml:input[@id='input1']";
  equal(inputWithIdExpXPath, inputWithIdXPath, " xpath generated for input with id");

  // Test generate xpath for input with id has single quote.
  do_print("Test generate xpath for input with id has single quote");
  let inputWithIdSingleQuote = doc.getElementsByTagName("input")[1];
  let inputWithIdXPathSingleQuote = inputWithIdSingleQuote.generateXPath();
  let inputWithIdExpXPathSingleQuote = '//xhtml:input[@id="input2\'"]';
  equal(inputWithIdExpXPathSingleQuote, inputWithIdXPathSingleQuote, " xpath generated for input with id");

  // Test generate xpath for input with id has double quote.
  do_print("Test generate xpath for input with id has double quote");
  let inputWithIdDoubleQuote = doc.getElementsByTagName("input")[2];
  let inputWithIdXPathDoubleQuote = inputWithIdDoubleQuote.generateXPath();
  let inputWithIdExpXPathDoubleQuote = "//xhtml:input[@id='\"input3\"']";
  equal(inputWithIdExpXPathDoubleQuote, inputWithIdXPathDoubleQuote, " xpath generated for input with id");

  // Test generate xpath for input with id has both single and double quote.
  do_print("Test generate xpath for input with id has single and double quote");
  let inputWithIdSingleDoubleQuote = doc.getElementsByTagName("input")[3];
  inputWithIdSingleDoubleQuote.setAttribute("id", "\"input'4");
  let inputWithIdXPathSingleDoubleQuote = inputWithIdSingleDoubleQuote.generateXPath();
  let inputWithIdExpXPathSingleDoubleQuote = "//xhtml:input[@id=concat('\"input',\"'\",'4')]";
  equal(inputWithIdExpXPathSingleDoubleQuote, inputWithIdXPathSingleDoubleQuote, " xpath generated for input with id");

  // Test generate xpath for input without id.
  do_print("Test generate xpath for input without id");
  let inputNoId = doc.getElementsByTagName("input")[4];
  let inputNoIdXPath = inputNoId.generateXPath();
  let inputNoIdExpXPath = "/xhtml:html/xhtml:body/xhtml:label[5]/xhtml:input";
  equal(inputNoIdExpXPath, inputNoIdXPath, " xpath generated for input without id");
}
