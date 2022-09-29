/*
 * Test for form auto fill content helper collectFormFields functions.
 */

"use strict";

var FormAutofillHandler;
add_setup(async () => {
  ({ FormAutofillHandler } = ChromeUtils.import(
    "resource://autofill/FormAutofillHandler.jsm"
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
    sections: [[]],
  },
  {
    description:
      "A valid credit card form with non-autocomplete-attr cc-number only (high confidence).",
    document: `<form>
               <input id="cc-number" name="cc-number">
               </form>`,
    sections: [[{ fieldName: "cc-number" }]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.numberOnly.confidenceThreshold",
        "0.95",
      ],
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.96"],
    ],
  },
  {
    description:
      "A invalid credit card form with non-autocomplete-attr cc-number + another input field",
    document: `<form>
               <input id="cc-number" name="cc-number">
               <input id="password" type="password">
               </form>`,
    sections: [[]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.numberOnly.confidenceThreshold",
        "0.95",
      ],
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.96"],
    ],
  },
  {
    description:
      "A valid credit card form with non-autocomplete-attr cc-number only (low confidence).",
    document: `<form>
               <input id="cc-number" name="cc-number">
               </form>`,
    sections: [[]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.numberOnly.confidenceThreshold",
        "0.95",
      ],
      ["extensions.formautofill.creditCards.heuristics.testConfidence", "0.9"],
    ],
  },
  {
    description: "An invalid credit card form due to omitted cc-number.",
    document: `<form>
               <input id="cc-name" name="cc-name">
               <input id="cc-exp-month" name="cc-exp-month">
               <input id="cc-exp-year" name="cc-exp-year">
               </form>`,
    sections: [[]],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.nameExpirySection.enabled",
        false,
      ],
    ],
  },
  {
    description:
      "A valid credit card form without cc-number but all other fields have autocomplete attr",
    document: `<form>
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [
      [
        { fieldName: "cc-name" },
        { fieldName: "cc-exp-month" },
        { fieldName: "cc-exp-year" },
      ],
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.nameExpirySection.enabled",
        false,
      ],
    ],
  },
  {
    description: "A valid credit card form with cc-name and cc-exp.",
    document: `<form>
               <input id="cc-name" name="cc-name">
               <input id="cc-exp-month" name="cc-exp-month">
               <input id="cc-exp-year" name="cc-exp-year">
               </form>`,
    sections: [
      [
        { fieldName: "cc-name" },
        { fieldName: "cc-exp-month" },
        { fieldName: "cc-exp-year" },
      ],
    ],
    prefs: [
      [
        "extensions.formautofill.creditCards.heuristics.nameExpirySection.enabled",
        true,
      ],
    ],
  },
  {
    description: "An invalid credit card form with only a cc-name field",
    document: `<form>
               <input id="cc-name" autocomplete="cc-name">
               </form>`,
    sections: [[{ fieldName: "cc-name" }]],
  },
  {
    description:
      "A valid credit card form with non-autocomplete-attr cc-number and cc-name.",
    document: `<form>
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-number" name="card-number">
               </form>`,
    sections: [[{ fieldName: "cc-name" }, { fieldName: "cc-number" }]],
  },
  {
    description:
      "A valid credit card form with autocomplete-attr cc-number only.",
    document: `<form><input id="cc-number" autocomplete="cc-number"></form>`,
    sections: [[{ fieldName: "cc-number" }]],
  },
  {
    description:
      "A valid credit card form with autocomplete-attr cc-name only.",
    document: `<form><input id="cc-name" autocomplete="cc-name"></form>`,
    sections: [[{ fieldName: "cc-name" }]],
  },
  {
    description:
      "A valid credit card form with autocomplete-attr cc-exp-month & cc-exp-year only.",
    document: `<form>
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [[{ fieldName: "cc-exp-month" }, { fieldName: "cc-exp-year" }]],
  },
  {
    description: "A valid credit card form with autocomplete-attr cc-exp only.",
    document: `<form><input id="cc-exp" autocomplete="cc-exp"></form>`,
    sections: [[{ fieldName: "cc-exp" }]],
  },
  {
    description:
      "A valid credit card form with non-autocomplete-attr cc-number and cc-exp.",
    document: `<form>
               <input id="cc-number" name="card-number">
               <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    sections: [[{ fieldName: "cc-number" }, { fieldName: "cc-exp" }]],
  },
  {
    description:
      "A valid credit card form with non-autocomplete-attr cc-number and cc-exp-month/cc-exp-year.",
    document: `<form>
               <input id="cc-number" name="card-number">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [
      [
        { fieldName: "cc-number" },
        { fieldName: "cc-exp-month" },
        { fieldName: "cc-exp-year" },
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
    allowDuplicates: true,
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
      "Dedup the same field names of the different telephone fields.",
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
      ],
    ],
    ids: ["i1", "i2", "i3", "i4", "homePhone"],
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
      ],
    ],
    ids: ["given-name", "family-name", "dummyAreaCode"],
  },
];

for (let tc of TESTCASES) {
  (function() {
    let testcase = tc;
    add_task(async function() {
      info("Starting testcase: " + testcase.description);

      if (testcase.prefs) {
        testcase.prefs.forEach(pref => SetPref(pref[0], pref[1]));
      }

      let doc = MockDocument.createTestDocument(
        "http://localhost:8080/test/",
        testcase.document
      );
      let form = doc.querySelector("form");
      let formLike = FormLikeFactory.createFromForm(form);

      function setElementWeakRef(details) {
        if (!details) {
          return;
        }

        details.forEach((detail, index) => {
          let id = testcase.ids?.[index] ?? detail.fieldName;
          let elementRef = doc.getElementById(id);

          detail.elementWeakRef = Cu.getWeakReference(elementRef);
        });
      }

      function verifyDetails(handlerDetails, testCaseDetails) {
        if (handlerDetails === null) {
          Assert.equal(handlerDetails, testCaseDetails);
          return;
        }
        Assert.equal(
          handlerDetails.length,
          testCaseDetails.length,
          "field count"
        );
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
            detail.elementWeakRef.get(),
            testCaseDetails[index].elementWeakRef.get(),
            "DOM reference"
          );
        });
      }
      setElementWeakRef(testcase.sections.flat());

      let handler = new FormAutofillHandler(formLike);
      let validFieldDetails = handler.collectFormFields(
        testcase.allowDuplicates
      );

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
