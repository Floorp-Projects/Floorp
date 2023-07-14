/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

function testMathAttr(iface, attr, subrole, textLeafValue) {
  ok(iface.attributeNames.includes(attr), `Object has ${attr} attribute`);
  let value = iface.getAttributeValue(attr);
  is(
    value.getAttributeValue("AXSubrole"),
    subrole,
    `${attr} value has correct subrole`
  );

  if (textLeafValue) {
    let children = value.getAttributeValue("AXChildren");
    is(children.length, 1, `${attr} value has one child`);

    is(
      children[0].getAttributeValue("AXRole"),
      "AXStaticText",
      `${attr} value's child is static text`
    );
    is(
      children[0].getAttributeValue("AXValue"),
      textLeafValue,
      `${attr} value has correct text`
    );
  }
}

addAccessibleTask(
  `<math id="math">
     <msqrt id="sqrt">
      <mn>2</mn>
    </msqrt>
    </math>`,
  async (browser, accDoc) => {
    let math = getNativeInterface(accDoc, "math");
    is(
      math.getAttributeValue("AXSubrole"),
      "AXDocumentMath",
      "Math element has correct subrole"
    );

    let sqrt = getNativeInterface(accDoc, "sqrt");
    is(
      sqrt.getAttributeValue("AXSubrole"),
      "AXMathSquareRoot",
      "msqrt has correct subrole"
    );

    testMathAttr(sqrt, "AXMathRootRadicand", "AXMathNumber", "2");
  }
);

addAccessibleTask(
  `<math>
    <mroot id="root">
      <mi>sin</mi>
      <mn>3</mn>
    </mroot>
  </math>`,
  async (browser, accDoc) => {
    let root = getNativeInterface(accDoc, "root");
    is(
      root.getAttributeValue("AXSubrole"),
      "AXMathRoot",
      "mroot has correct subrole"
    );

    testMathAttr(root, "AXMathRootRadicand", "AXMathIdentifier", "sin");
    testMathAttr(root, "AXMathRootIndex", "AXMathNumber", "3");
  }
);

addAccessibleTask(
  `<math>
    <mfrac id="fraction">
      <mn>2</mn>
      <mn>3</mn>
     </mfrac>
  </math>`,
  async (browser, accDoc) => {
    let fraction = getNativeInterface(accDoc, "fraction");
    is(
      fraction.getAttributeValue("AXSubrole"),
      "AXMathFraction",
      "mfrac has correct subrole"
    );
    ok(fraction.attributeNames.includes("AXMathFractionNumerator"));
    ok(fraction.attributeNames.includes("AXMathFractionDenominator"));
    ok(fraction.attributeNames.includes("AXMathLineThickness"));

    // Bug 1639745
    todo_is(fraction.getAttributeValue("AXMathLineThickness"), 1);

    testMathAttr(fraction, "AXMathFractionNumerator", "AXMathNumber", "2");
    testMathAttr(fraction, "AXMathFractionDenominator", "AXMathNumber", "3");
  }
);

addAccessibleTask(
  `<math>
  <msubsup id="subsup">
    <mo>∫</mo>
    <mn>0</mn>
    <mn>1</mn>
  </msubsup>
  </math>`,
  async (browser, accDoc) => {
    let subsup = getNativeInterface(accDoc, "subsup");
    is(
      subsup.getAttributeValue("AXSubrole"),
      "AXMathSubscriptSuperscript",
      "msubsup has correct subrole"
    );

    testMathAttr(subsup, "AXMathSubscript", "AXMathNumber", "0");
    testMathAttr(subsup, "AXMathSuperscript", "AXMathNumber", "1");
    testMathAttr(subsup, "AXMathBase", "AXMathOperator", "∫");
  }
);

addAccessibleTask(
  `<math>
  <munderover id="underover">
    <mo>∫</mo>
    <mn>0</mn>
    <mi>∞</mi>
  </munderover>
  </math>`,
  async (browser, accDoc) => {
    let underover = getNativeInterface(accDoc, "underover");
    is(
      underover.getAttributeValue("AXSubrole"),
      "AXMathUnderOver",
      "munderover has correct subrole"
    );

    testMathAttr(underover, "AXMathUnder", "AXMathNumber", "0");
    testMathAttr(underover, "AXMathOver", "AXMathIdentifier", "∞");
    testMathAttr(underover, "AXMathBase", "AXMathOperator", "∫");
  }
);
