"use strict";

Cu.import("resource://formautofill/FormAutofillHeuristics.jsm");

const TESTCASES = [
  {
    description: "Input contains in a label element.",
    document: `<form>
                 <label id="labelA"> label type A
                   <input id="typeA" type="text">
                 </label>
               </form>`,
    inputId: "typeA",
    expectedLabelIds: ["labelA"],
  },
  {
    description: "Input contains in a label element.",
    document: `<label id="labelB"> label type B
                 <div> inner div
                   <input id="typeB" type="text">
                 </div>
               </label>`,
    inputId: "typeB",
    expectedLabelIds: ["labelB"],
  },
  {
    description: "\"for\" attribute used to indicate input by one label.",
    document: `<label id="labelC" for="typeC">label type C</label>
               <input id="typeC" type="text">`,
    inputId: "typeC",
    expectedLabelIds: ["labelC"],
  },
  {
    description: "\"for\" attribute used to indicate input by multiple labels.",
    document: `<form>
                 <label id="labelD1" for="typeD">label type D1</label>
                 <label id="labelD2" for="typeD">label type D2</label>
                 <label id="labelD3" for="typeD">label type D3</label>
                 <input id="typeD" type="text">
               </form>`,
    inputId: "typeD",
    expectedLabelIds: ["labelD1", "labelD2", "labelD3"],
  },
  {
    description: "\"for\" attribute used to indicate input by multiple labels with space prefix/postfix.",
    document: `<label id="labelE1" for="typeE">label type E1</label>
               <label id="labelE2" for="typeE  ">label type E2</label>
               <label id="labelE3" for="  TYPEe">label type E3</label>
               <label id="labelE4" for="  typeE  ">label type E4</label>
               <input id="   typeE  " type="text">`,
    inputId: "   typeE  ",
    expectedLabelIds: [],
  },
  {
    description: "Input contains in a label element.",
    document: `<label id="labelF"> label type F
                 <label for="dummy"> inner label
                   <input id="typeF" type="text">
                   <input id="dummy" type="text">
                 </div>
               </label>`,
    inputId: "typeF",
    expectedLabelIds: ["labelF"],
  },
  {
    description: "\"for\" attribute used to indicate input by labels out of the form.",
    document: `<label id="labelG1" for="typeG">label type G1</label>
               <form>
                 <label id="labelG2" for="typeG">label type G2</label>
                 <input id="typeG" type="text">
               </form>
               <label id="labelG3" for="typeG">label type G3</label>`,
    inputId: "typeG",
    expectedLabelIds: ["labelG1", "labelG2", "labelG3"],
  },
];

TESTCASES.forEach(testcase => {
  add_task(async function() {
    info("Starting testcase: " + testcase.description);

    let doc = MockDocument.createTestDocument(
      "http://localhost:8080/test/", testcase.document);

    let input = doc.getElementById(testcase.inputId);
    let labels = LabelUtils.findLabelElements(input);

    Assert.deepEqual(labels.map(l => l.id), testcase.expectedLabelIds);
    LabelUtils.clearLabelMap();
  });
});
