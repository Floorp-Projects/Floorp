"use strict";

var FormAutofillUtils;
add_setup(async () => {
  ({ FormAutofillUtils } = ChromeUtils.import(
    "resource://autofill/FormAutofillUtils.jsm"
  ));
});

add_task(async function test_parseAddressFormat() {
  const TEST_CASES = [
    {
      fmt: "%N%n%O%n%A%n%C, %S %Z", // US
      parsed: [
        { fieldId: "name", newLine: true },
        { fieldId: "organization", newLine: true },
        { fieldId: "street-address", newLine: true },
        { fieldId: "address-level2" },
        { fieldId: "address-level1" },
        { fieldId: "postal-code" },
      ],
    },
    {
      fmt: "%N%n%O%n%A%n%C %S %Z", // CA
      parsed: [
        { fieldId: "name", newLine: true },
        { fieldId: "organization", newLine: true },
        { fieldId: "street-address", newLine: true },
        { fieldId: "address-level2" },
        { fieldId: "address-level1" },
        { fieldId: "postal-code" },
      ],
    },
    {
      fmt: "%N%n%O%n%A%n%Z %C", // DE
      parsed: [
        { fieldId: "name", newLine: true },
        { fieldId: "organization", newLine: true },
        { fieldId: "street-address", newLine: true },
        { fieldId: "postal-code" },
        { fieldId: "address-level2" },
      ],
    },
    {
      fmt: "%N%n%O%n%A%n%D%n%C%n%S %Z", // IE
      parsed: [
        { fieldId: "name", newLine: true },
        { fieldId: "organization", newLine: true },
        { fieldId: "street-address", newLine: true },
        { fieldId: "address-level3", newLine: true },
        { fieldId: "address-level2", newLine: true },
        { fieldId: "address-level1" },
        { fieldId: "postal-code" },
      ],
    },
  ];

  Assert.throws(
    () => FormAutofillUtils.parseAddressFormat(),
    /fmt string is missing./,
    "Should throw if fmt is empty"
  );
  for (let tc of TEST_CASES) {
    Assert.deepEqual(FormAutofillUtils.parseAddressFormat(tc.fmt), tc.parsed);
  }
});
