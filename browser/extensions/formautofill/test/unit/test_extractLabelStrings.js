"use strict";

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

const TESTCASES = [
  {
    description: "A label element contains one input element.",
    document: `<label id="typeA"> label type A
                 <!-- This comment should not be extracted. -->
                 <input type="text">
                 <script>FOO</script>
                 <noscript>FOO</noscript>
                 <option>FOO</option>
                 <style>FOO</style>
               </label>`,
    inputId: "typeA",
    expectedStrings: ["label type A"],
  },
  {
    description: "A label element with inner div contains one input element.",
    document: `<label id="typeB"> label type B
                 <!-- This comment should not be extracted. -->
                 <script>FOO</script>
                 <noscript>FOO</noscript>
                 <option>FOO</option>
                 <style>FOO</style>
                 <div> inner div
                   <input type="text">
                 </div>
               </label>`,
    inputId: "typeB",
    expectedStrings: ["label type B", "inner div"],
  },
  {
    description: "A label element with inner prefix/postfix strings contains span elements.",
    document: `<label id="typeC"> label type C
                 <!-- This comment should not be extracted. -->
                 <script>FOO</script>
                 <noscript>FOO</noscript>
                 <option>FOO</option>
                 <style>FOO</style>
                 <div> inner div prefix
                   <span>test C-1 </span>
                   <span> test C-2</span>
                  inner div postfix
                 </div>
               </label>`,
    inputId: "typeC",
    expectedStrings: ["label type C", "inner div prefix", "test C-1",
      "test C-2", "inner div postfix"],
  },
];

TESTCASES.forEach(testcase => {
  add_task(async function() {
    do_print("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/", testcase.document);

    let element = doc.getElementById(testcase.inputId);
    let strings = FormAutofillUtils.extractLabelStrings(element);

    Assert.deepEqual(strings, testcase.expectedStrings);
  });
});
