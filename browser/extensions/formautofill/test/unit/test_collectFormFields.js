/*
 * Test for form auto fill content helper collectFormFields functions.
 */

"use strict";

Cu.import("resource://formautofill/FormAutofillHandler.jsm");

const TESTCASES = [
  {
    description: "Form without autocomplete property",
    document: `<form><input id="given-name"><input id="family-name">
               <input id="street-addr"><input id="city"><select id="country"></select>
               <input id='email'><input id="phone"></form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-line1"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["given-name", "family-name", "street-addr", "city", "country", "email", "phone"],
  },
  {
    description: "An address and credit card form with autocomplete properties and 1 token",
    document: `<form>
               <input id="given-name" autocomplete="given-name">
               <input id="family-name" autocomplete="family-name">
               <input id="street-addr" autocomplete="street-address">
               <input id="city" autocomplete="address-level2">
               <select id="country" autocomplete="country"></select>
               <input id="email" autocomplete="email">
               <input id="tel" autocomplete="tel">
               <input id="cc-number" autocomplete="cc-number">
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
    ],
  },
  {
    description: "An address form with autocomplete properties and 2 tokens",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="city" autocomplete="shipping address-level2">
               <input id="country" autocomplete="shipping country">
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
  },
  {
    description: "Form with autocomplete properties and profile is partly matched",
    document: `<form><input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input autocomplete="shipping address-level2">
               <select autocomplete="shipping country"></select>
               <input id='email' autocomplete="shipping email">
               <input id="tel" autocomplete="shipping tel"></form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "address-level2"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "country"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
  },
  {
    description: "It's a valid address and credit card form.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input id="family-name" autocomplete="shipping family-name">
               <input id="street-addr" autocomplete="shipping street-address">
               <input id="cc-number" autocomplete="shipping cc-number">
               </form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      ],
      creditCardFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "cc-number"},
      ],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "cc-number"},
    ],
  },
  {
    description: "An invalid address form due to less than 3 fields.",
    document: `<form>
               <input id="given-name" autocomplete="shipping given-name">
               <input autocomplete="shipping address-level2">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [],
  },
  {
    description: "An invalid credit card form due to omitted cc-number.",
    document: `<form>
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [],
  },
  {
    description: "An invalid credit card form due to non-autocomplete-attr cc-number and omitted cc-exp-*.",
    document: `<form>
               <input id="cc-name" autocomplete="cc-name">
               <input id="cc-number" name="card-number">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [],
  },
  {
    description: "A valid credit card form with autocomplete-attr cc-number only.",
    document: `<form>
               <input id="cc-number" autocomplete="cc-number">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      ],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
    ],
  },
  {
    description: "A valid credit card form with non-autocomplete-attr cc-number and cc-exp.",
    document: `<form>
               <input id="cc-number" name="card-number">
               <input id="cc-exp" autocomplete="cc-exp">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
      ],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp"},
    ],
    ids: [
      "cc-number",
      "cc-exp",
    ],
  },
  {
    description: "A valid credit card form with non-autocomplete-attr cc-number and cc-exp-month/cc-exp-year.",
    document: `<form>
               <input id="cc-number" name="card-number">
               <input id="cc-exp-month" autocomplete="cc-exp-month">
               <input id="cc-exp-year" autocomplete="cc-exp-year">
               </form>`,
    sections: [{
      addressFieldDetails: [],
      creditCardFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
      ],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-number"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-month"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "cc-exp-year"},
    ],
    ids: [
      "cc-number",
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
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      ],
      creditCardFieldDetails: [],
    }, {
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},

        // TODO Bug 1421181 - "tel-country-code" field should belong to the next
        // section. There should be a way to group the related fields during the
        // parsing stage.
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-country-code"},
      ],
      creditCardFieldDetails: [],
    }, {
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-extension"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-country-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    ids: [
      "shippingAC", "shippingPrefix", "shippingSuffix", "shippingTelExt",
      "billingAC", "billingPrefix", "billingSuffix",
      "otherCC", "otherAC", "otherPrefix", "otherSuffix",
    ],
  },
  {
    description: "Dedup the same field names of the different telephone fields.",
    document: `<form>
                 <input id="i1" autocomplete="given-name">
                 <input id="i2" autocomplete="family-name">
                 <input id="i3" autocomplete="street-address">
                 <input id="i4" autocomplete="email">

                 <input id="homePhone" maxlength="10">
                 <input id="mobilePhone" maxlength="10">
                 <input id="officePhone" maxlength="10">
               </form>`,
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
        {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["i1", "i2", "i3", "i4", "homePhone"],
  },
  {
    description: "The duplicated phones of a single one and a set with ac, prefix, suffix.",
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
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},

        // NOTES: Ideally, there is only one full telephone field(s) in a form for
        // this case. We can see if there is any better solution later.
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},

        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "street-address"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "email"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-area-code"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-prefix"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel-local-suffix"},
    ],
    ids: ["i1", "i2", "i3", "i4", "singlePhone",
      "shippingAreaCode", "shippingPrefix", "shippingSuffix"],
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
    sections: [{
      addressFieldDetails: [
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
        {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
      ],
      creditCardFieldDetails: [],
    }],
    validFieldDetails: [
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "given-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "family-name"},
      {"section": "", "addressType": "shipping", "contactType": "", "fieldName": "tel"},
    ],
    ids: ["given-name", "family-name", "dummyAreaCode"],
  },
];

for (let tc of TESTCASES) {
  (function() {
    let testcase = tc;
    add_task(async function() {
      do_print("Starting testcase: " + testcase.description);

      let doc = MockDocument.createTestDocument("http://localhost:8080/test/",
                                                testcase.document);
      let form = doc.querySelector("form");
      let formLike = FormLikeFactory.createFromForm(form);

      function setElementWeakRef(details) {
        if (!details) {
          return;
        }

        details.forEach((detail, index) => {
          let elementRef;
          if (testcase.ids && testcase.ids[index]) {
            elementRef = doc.getElementById(testcase.ids[index]);
          } else {
            elementRef = doc.querySelector("*[autocomplete*='" + detail.fieldName + "']");
          }
          detail.elementWeakRef = Cu.getWeakReference(elementRef);
        });
      }

      function verifyDetails(handlerDetails, testCaseDetails) {
        if (handlerDetails === null) {
          Assert.equal(handlerDetails, testCaseDetails);
          return;
        }
        Assert.equal(handlerDetails.length, testCaseDetails.length);
        handlerDetails.forEach((detail, index) => {
          Assert.equal(detail.fieldName, testCaseDetails[index].fieldName, "fieldName");
          Assert.equal(detail.section, testCaseDetails[index].section, "section");
          Assert.equal(detail.addressType, testCaseDetails[index].addressType, "addressType");
          Assert.equal(detail.contactType, testCaseDetails[index].contactType, "contactType");
          Assert.equal(detail.elementWeakRef.get(), testCaseDetails[index].elementWeakRef.get(), "DOM reference");
        });
      }
      setElementWeakRef(testcase.sections.reduce((fieldDetails, section) => {
        fieldDetails.push(...section.addressFieldDetails, ...section.creditCardFieldDetails);
        return fieldDetails;
      }, []));
      setElementWeakRef(testcase.validFieldDetails);

      let handler = new FormAutofillHandler(formLike);
      let validFieldDetails = handler.collectFormFields(testcase.allowDuplicates);

      Assert.equal(handler.sections.length, testcase.sections.length);
      for (let i = 0; i < handler.sections.length; i++) {
        let section = handler.sections[i];
        verifyDetails(section.address.fieldDetails, testcase.sections[i].addressFieldDetails);
        verifyDetails(section.creditCard.fieldDetails, testcase.sections[i].creditCardFieldDetails);
      }
      verifyDetails(validFieldDetails, testcase.validFieldDetails);
    });
  })();
}
