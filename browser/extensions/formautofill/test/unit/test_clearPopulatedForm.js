/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

"use strict";

const TESTCASES = [
  {
    description: "Clear populated address form with text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-addr">
                <input id="city">
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-addr": "1000 Main Street",
      city: "Nowhere",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-addr": "",
      city: "",
    },
  },
  {
    description: "Clear populated address form with select and text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-addr">
                <select id="state">
                  <option value="AL">Alabama</option>
                  <option value="AK">Alaska</option>
                  <option value="OH">Ohio</option>
                </select>
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-addr": "1000 Main Street",
      state: "OH",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-addr": "",
      state: "AL",
    },
  },
  {
    description:
      "Clear populated address form with select element with selected attribute and text inputs",
    document: `<form>
                <input id="given-name">
                <input id="family-name">
                <input id="street-addr">
                <select id="state">
                  <option value="AL">Alabama</option>
                  <option selected value="AK">Alaska</option>
                  <option value="OH">Ohio</option>
                </select>
               </form>`,
    focusedInputId: "given-name",
    profileData: {
      "given-name": "John",
      "family-name": "Doe",
      "street-addr": "1000 Main Street",
      state: "OH",
    },
    expectedResult: {
      "given-name": "",
      "family-name": "",
      "street-addr": "",
      state: "AK",
    },
  },
];

add_task(async function do_test() {
  let { FormAutofillHandler } = ChromeUtils.importESModule(
    "resource://gre/modules/shared/FormAutofillHandler.sys.mjs"
  );
  for (let test of TESTCASES) {
    info("Test case: " + test.description);
    let testDoc = MockDocument.createTestDocument(
      "http://localhost:8080/test",
      test.document
    );
    let form = testDoc.querySelector("form");
    let formLike = FormLikeFactory.createFromForm(form);
    let handler = new FormAutofillHandler(formLike);
    handler.collectFormFields();
    let focusedInput = testDoc.getElementById(test.focusedInputId);
    handler.focusedInput = focusedInput;
    let [adaptedProfile] = handler.activeSection.getAdaptedProfiles([
      test.profileData,
    ]);
    await handler.autofillFormFields(adaptedProfile, focusedInput);

    handler.activeSection.clearPopulatedForm();
    handler.activeSection.fieldDetails.forEach(detail => {
      let element = detail.element;
      let id = element.id;
      Assert.equal(
        element.value,
        test.expectedResult[id],
        `Check the ${id} field was restored to the correct value`
      );
    });
  }
});
