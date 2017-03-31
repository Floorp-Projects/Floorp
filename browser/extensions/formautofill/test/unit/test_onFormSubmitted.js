"use strict";

Cu.import("resource://formautofill/FormAutofillContent.jsm");

const MOCK_DOC = MockDocument.createTestDocument("http://localhost:8080/test/",
                   `<form id="form1">
                      <input id="street-addr" autocomplete="street-address">
                      <input id="city" autocomplete="address-level2">
                      <input id="country" autocomplete="country">
                      <input id="email" autocomplete="email">
                      <input id="tel" autocomplete="tel">
                      <input id="submit" type="submit">
                    </form>`);

add_task(function* () {
  do_print("Starting testcase: Make sure content handle earlyformsubmit correctly");

  let form = MOCK_DOC.getElementById("form1");
  FormAutofillContent.identifyAutofillFields(MOCK_DOC);
  sinon.spy(FormAutofillContent, "_onFormSubmit");

  do_check_eq(FormAutofillContent.notify(form), true);
  do_check_eq(FormAutofillContent._onFormSubmit.called, true);

  let fakeForm = MOCK_DOC.createElement("form");
  FormAutofillContent._onFormSubmit.reset();

  do_check_eq(FormAutofillContent.notify(fakeForm), true);
  do_check_eq(FormAutofillContent._onFormSubmit.called, false);
});
