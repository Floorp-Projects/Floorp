/*
 * Test for form auto fill content helper collectFormFields functions.
 */

"use strict";

var FormAutofillHandler;
add_setup(async () => {
  ({ FormAutofillHandler } = ChromeUtils.importESModule(
    "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
  ));
});

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form>
               <input id="given-name">
               <input id="family-name">
               <input id="street-addr">
               <input id="city">
               <select id="country"></select>
               <input id='email'>
               <input id="phone">
               </form>`,
    sections: [
      [
        { fieldName: "given-name" },
        { fieldName: "family-name" },
        { fieldName: "address-line1" },
        { fieldName: "address-level2" },
        { fieldName: "country" },
        { fieldName: "email" },
        { fieldName: "tel" },
      ],
    ],
    ids: [
      "given-name",
      "family-name",
      "street-addr",
      "city",
      "country",
      "email",
      "phone",
    ],
  },
  {
    description:
      "An address and credit card form with autocomplete properties and 1 token",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-address" autocomplete="street-address">
               <input id="address-level2" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="cc-number" autocomplete="cc-number">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [
      [
        { fieldName: "given-name" },
        { fieldName: "family-name" },
        { fieldName: "street-address" },
        { fieldName: "address-level2" },
        { fieldName: "country" },
        { fieldName: "email" },
        { fieldName: "tel" },
      ],
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-name" },
        { fieldName: "cc-exp-month" },
        { fieldName: "cc-exp-year" },
      ],
    ],
  },
  {
    description: "An address form with autocomplete properties and 2 tokens",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="address-level2" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    sections: [
      [
        { addressType: "shipping", fieldName: "given-name" },
        { addressType: "shipping", fieldName: "family-name" },
        { addressType: "shipping", fieldName: "street-address" },
        { addressType: "shipping", fieldName: "address-level2" },
        { addressType: "shipping", fieldName: "country" },
        { addressType: "shipping", fieldName: "email" },
        { addressType: "shipping", fieldName: "tel" },
      ],
    ],
  },
  {
    description:
      "Form with autocomplete properties and profile is partly matched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="address-level2" autocomplete="shipping address-level2">
               <select id="country" autocomplete="shipping country"></select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    sections: [
      [
        { addressType: "shipping", fieldName: "given-name" },
        { addressType: "shipping", fieldName: "family-name" },
        { addressType: "shipping", fieldName: "street-address" },
        { addressType: "shipping", fieldName: "address-level2" },
        { addressType: "shipping", fieldName: "country" },
        { addressType: "shipping", fieldName: "email" },
        { addressType: "shipping", fieldName: "tel" },
      ],
    ],
  },
  {
    description: "It's a valid address and credit card form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-address" autocomplete="shipping street-address">
               <input id="cc-number" autocomplete="shipping cc-number">
               </form>`,
    sections: [
      [
        { addressType: "shipping", fieldName: "given-name" },
        { addressType: "shipping", fieldName: "family-name" },
        { addressType: "shipping", fieldName: "street-address" },
      ],
      [{ addressType: "shipping", fieldName: "cc-number" }],
    ],
  },
  {
    description: "An invalid address form due to less than 3 fields.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input autocomplete="shipping address-level2">
               </form>`,
    sections: [],
  },
  /*
   * Valid Credit Card Form with autocomplete attribute
   */
  {
    description: "@autocomplete - A valid credit card form",
    document: `<form>
                 <input id="cc-number" autocomplete="cc-number">
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    sections: [
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-name" },
        { fieldName: "cc-exp" },
      ],
    ],
  },
  {
    description: "@autocomplete - A valid credit card form without cc-numner",
    document: `<form>
                 <input id="cc-name" autocomplete="cc-name">
                 <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    sections: [[{ fieldName: "cc-name" }, { fieldName: "cc-exp" }]],
  },
  {
    description: "@autocomplete - A valid cc-number only form",
    document: `<form><input id="cc-number" autocomplete="cc-number"></form>`,
    sections: [[{ fieldName: "cc-number" }]],
  },
  {
    description: "@autocomplete - A valid cc-name only form",
    document: `<form><input id="cc-name" autocomplete="cc-name"></form>`,
    sections: [[{ fieldName: "cc-name" }]],
  },
  {
    description: "@autocomplete - A valid cc-exp only form",
    document: `<form><input id="cc-exp" autocomplete="cc-exp"></form>`,
    sections: [[{ fieldName: "cc-exp" }]],
  },
  {
    description: "@autocomplete - A valid cc-exp-month + cc-exp-year form",
    document: `<form>
                 <input id="cc-exp-month" autocomplete="cc-exp-month">
                 <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [[{ fieldName: "cc-exp-month" }, { fieldName: "cc-exp-year" }]],
  },
  {
    description: "@autocomplete - A valid cc-exp-month only form",
    document: `<form><input id="cc-exp-month" autocomplete="cc-exp-month"></form>`,
    sections: [[{ fieldName: "cc-exp-month" }]],
  },
  {
    description: "@autocomplete - A valid cc-exp-year only form",
    document: `<form><input id="cc-exp-year" autocomplete="cc-exp-year"></form>`,
    sections: [[{ fieldName: "cc-exp-year" }]],
  },
  /*
   * Valid Credit Card Form when cc-number or cc-name is detected by fathom
   */
  {
    description:
      "A valid credit card form without autocomplete attribute (cc-number is detected by fathom)",
    document: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    sections: [
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-name" },
        { fieldName: "cc-exp" },
      ],
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  {
    description:
      "A valid credit card form without autocomplete attribute (only cc-number is detected by fathom)",
    document: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    sections: [
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-name" },
        { fieldName: "cc-exp" },
      ],
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.types",
        "cc-number",
      ],
    ],
  },
  {
    description:
      "A valid credit card form without autocomplete attribute (only cc-name is detected by fathom)",
    document: `<form>
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    sections: [[{ fieldName: "cc-name" }, { fieldName: "cc-exp" }]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.types",
        "cc-name",
      ],
    ],
  },
  /*
   * Invalid Credit Card Form when a cc-number or cc-name is detected by fathom
   */
  {
    description:
      "A credit card form is invalid when a fathom detected cc-number field is the only field in the form",
    document: `<form><input id="cc-number" name="cc-number"></form>`,
    sections: [],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  {
    description:
      "A credit card form is invalid when a fathom detected cc-name field is the only field in the form",
    document: `<form><input id="cc-name" name="cc-name"></form>`,
    sections: [],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.9",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.8",
      ],
    ],
  },
  /*
   * Valid Credit Card Form when a cc-number or cc-name only form is detected by fathom (field is high confidence)
   */
  {
    description:
      "A cc-number only form is considered a valid credit card form when fathom is confident and there is no other <input> in the form",
    document: `<form><input id="cc-number" name="cc-number"></form>`,
    sections: [[{ fieldName: "cc-number" }]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.99",
      ],
    ],
  },
  {
    description:
      "A cc-name only form is considered a valid credit card form when fathom is confident and there is no other <input> in the form",
    document: `<form><input id="cc-name" name="cc-name"></form>`,
    sections: [[{ fieldName: "cc-name" }]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.99",
      ],
    ],
  },
  /*
   * Invalid Credit Card Form when none of the fields is identified by fathom
   */
  {
    description:
      "A credit card form is invalid when none of the fields are identified by fathom or autocomplete",
    document: `<form>
                 <input id="cc-number" name="cc-number">
                 <input id="cc-name" name="cc-name">
                 <input id="cc-exp" name="cc-exp">
               </form>`,
    sections: [],
    prefs: [
      ["extensions.formautofill.creditCards.heuristics.fathom.types", ""],
    ],
  },
  // Special Cases
  {
    description:
      "A credit card form with a high-confidence cc-name field is still considered invalid when there is another <input> field",
    document: `<form>
               <input id="cc-name" name="cc-name">
               <input id="password" type="password">
               </form>`,
    sections: [],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.fathom.highConfidenceThreshold",
        "0.95",
      ],
      [
        "extensions.formautofill.creditCards.heuristics.fathom.testConfidence",
        "0.96",
      ],
    ],
  },
  {
    description: "A valid credit card form with multiple cc-number fields",
    document: `<form>
                <input id="cc-number1" maxlength="4">
                <input id="cc-number2" maxlength="4">
                <input id="cc-number3" maxlength="4">
                <input id="cc-number4" maxlength="4">
                <input id="cc-exp-month" autocomplete="cc-exp-month">
                <input id="cc-exp-year" autocomplete="cc-exp-year">
                </form>`,
    sections: [
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-number" },
        { fieldName: "cc-number" },
        { fieldName: "cc-number" },
        { fieldName: "cc-exp-month" },
        { fieldName: "cc-exp-year" },
      ],
    ],
    ids: [
      "cc-number1",
      "cc-number2",
      "cc-number3",
      "cc-number4",
      "cc-exp-month",
      "cc-exp-year",
    ],
  },
  {
    description: "Three sets of adjacent phone number fields",
    document: `<form>
                 <input id="shippingAC" name="phone" maxlength="3">
                 <input id="shippingPrefix" name="phone" maxlength="3">
                 <input id="shippingSuffix" name="phone" maxlength="4">
                 <input id="shippingTelExt" name="extension">

                 <input id="billingAC" name="phone" maxlength="3">
                 <input id="billingPrefix" name="phone" maxlength="3">
                 <input id="billingSuffix" name="phone" maxlength="4">

                 <input id="otherCC" name="phone" maxlength="3">
                 <input id="otherAC" name="phone" maxlength="3">
                 <input id="otherPrefix" name="phone" maxlength="3">
                 <input id="otherSuffix" name="phone" maxlength="4">
               </form>`,
    sections: [
      [
        { fieldName: "tel-area-code" },
        { fieldName: "tel-local-prefix" },
        { fieldName: "tel-local-suffix" },
        { fieldName: "tel-extension" },
      ],
      [
        { fieldName: "tel-area-code" },
        { fieldName: "tel-local-prefix" },
        { fieldName: "tel-local-suffix" },

        // TODO Bug 1421181 - "tel-country-code" field should belong to the next
        // section. There should be a way to group the related fields during the
        // parsing stage.
        { fieldName: "tel-country-code" },
      ],
      [
        { fieldName: "tel-area-code" },
        { fieldName: "tel-local-prefix" },
        { fieldName: "tel-local-suffix" },
      ],
    ],
    ids: [
      "shippingAC",
      "shippingPrefix",
      "shippingSuffix",
      "shippingTelExt",
      "billingAC",
      "billingPrefix",
      "billingSuffix",
      "otherCC",
      "otherAC",
      "otherPrefix",
      "otherSuffix",
    ],
  },
  {
    description:
      "Do not dedup the same field names of the different telephone fields.",
    document: `<form>
                 <input id="i1" autocomplete="given-name">
                 <input id="i2" autocomplete="family-name">
                 <input id="i3" autocomplete="street-address">
                 <input id="i4" autocomplete="email">

                 <input id="homePhone" maxlength="10">
                 <input id="mobilePhone" maxlength="10">
                 <input id="officePhone" maxlength="10">
               </form>`,
    sections: [
      [
        { fieldName: "given-name" },
        { fieldName: "family-name" },
        { fieldName: "street-address" },
        { fieldName: "email" },
        { fieldName: "tel" },
        { fieldName: "tel" },
        { fieldName: "tel" },
      ],
    ],
    ids: ["i1", "i2", "i3", "i4", "homePhone", "mobilePhone", "officePhone"],
  },
  {
    description:
      "The duplicated phones of a single one and a set with ac, prefix, suffix.",
    document: `<form>
                 <input id="i1" autocomplete="shipping given-name">
                 <input id="i2" autocomplete="shipping family-name">
                 <input id="i3" autocomplete="shipping street-address">
                 <input id="i4" autocomplete="shipping email">
                 <input id="singlePhone" autocomplete="shipping tel">
                 <input id="shippingAreaCode" autocomplete="shipping tel-area-code">
                 <input id="shippingPrefix" autocomplete="shipping tel-local-prefix">
                 <input id="shippingSuffix" autocomplete="shipping tel-local-suffix">
               </form>`,
    sections: [
      [
        { addressType: "shipping", fieldName: "given-name" },
        { addressType: "shipping", fieldName: "family-name" },
        { addressType: "shipping", fieldName: "street-address" },
        { addressType: "shipping", fieldName: "email" },

        // NOTES: Ideally, there is only one full telephone field(s) in a form for
        // this case. We can see if there is any better solution later.
        { addressType: "shipping", fieldName: "tel" },
        { addressType: "shipping", fieldName: "tel-area-code" },
        { addressType: "shipping", fieldName: "tel-local-prefix" },
        { addressType: "shipping", fieldName: "tel-local-suffix" },
      ],
    ],
    ids: [
      "i1",
      "i2",
      "i3",
      "i4",
      "singlePhone",
      "shippingAreaCode",
      "shippingPrefix",
      "shippingSuffix",
    ],
  },
  {
    description: "Always adopt the info from autocomplete attribute.",
    document: `<form>
                 <input id="given-name" autocomplete="shipping given-name">
                 <input id="family-name" autocomplete="shipping family-name">
                 <input id="dummyAreaCode" autocomplete="shipping tel" maxlength="3">
                 <input id="dummyPrefix" autocomplete="shipping tel" maxlength="3">
                 <input id="dummySuffix" autocomplete="shipping tel" maxlength="4">
               </form>`,
    sections: [
      [
        { addressType: "shipping", fieldName: "given-name" },
        { addressType: "shipping", fieldName: "family-name" },
        { addressType: "shipping", fieldName: "tel" },
        { addressType: "shipping", fieldName: "tel" },
        { addressType: "shipping", fieldName: "tel" },
      ],
    ],
    ids: [
      "given-name",
      "family-name",
      "dummyAreaCode",
      "dummyPrefix",
      "dummySuffix",
    ],
  },
];

function verifyDetails(handlerDetails, testCaseDetails) {
  if (handlerDetails === null) {
    Assert.equal(handlerDetails, testCaseDetails);
    return;
  }
  Assert.equal(handlerDetails.length, testCaseDetails.length, "field count");
  handlerDetails.forEach((detail, index) => {
    Assert.equal(
      detail.fieldName,
      testCaseDetails[index].fieldName,
      "fieldName"
    );
    Assert.equal(
      detail.section,
      testCaseDetails[index].section ?? "",
      "section"
    );
    Assert.equal(
      detail.addressType,
      testCaseDetails[index].addressType ?? "",
      "addressType"
    );
    Assert.equal(
      detail.contactType,
      testCaseDetails[index].contactType ?? "",
      "contactType"
    );
    Assert.equal(
      detail.element,
      testCaseDetails[index].elementWeakRef.deref(),
      "DOM reference"
    );
  });
}

for (let tc of TESTCASES) {
  (function () {
    let testcase = tc;
    add_task(async function () {
      info("Starting testcase: " + testcase.description);

      if (testcase.prefs) {
        testcase.prefs.forEach(pref => SetPref(pref[0], pref[1]));
      }

      let doc = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );
      testcase.sections.flat().forEach((field, idx) => {
        let elementRef = doc.getElementById(
          testcase.ids?.[idx] ?? field.fieldName
        );
        field.elementWeakRef = new WeakRef(elementRef);
      });

      let form = doc.querySelector("form");
      let formLike = FormLikeFactory.createFromForm(form);

      let handler = new FormAutofillHandler(formLike);
      let validFieldDetails = handler.collectFormFields();

      Assert.equal(
        handler.sections.length,
        testcase.sections.length,
        "section count"
      );
      for (let i = 0; i < handler.sections.length; i++) {
        let section = handler.sections[i];
        verifyDetails(section.fieldDetails, testcase.sections[i]);
      }
      verifyDetails(validFieldDetails, testcase.sections.flat());

      if (testcase.prefs) {
        testcase.prefs.forEach(pref => Services.prefs.clearUserPref(pref[0]));
      }
    });
  })();
}
