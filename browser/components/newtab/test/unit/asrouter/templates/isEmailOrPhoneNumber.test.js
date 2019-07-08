import { isEmailOrPhoneNumber } from "content-src/asrouter/templates/SendToDeviceSnippet/isEmailOrPhoneNumber";

const CONTENT = {};

describe("isEmailOrPhoneNumber", () => {
  it("should return 'email' for emails", () => {
    assert.equal(isEmailOrPhoneNumber("foobar@asd.com", CONTENT), "email");
    assert.equal(isEmailOrPhoneNumber("foobar@asd.co.uk", CONTENT), "email");
  });
  it("should return 'phone' for valid en-US/en-CA phone numbers", () => {
    assert.equal(
      isEmailOrPhoneNumber("14582731273", { locale: "en-US" }),
      "phone"
    );
    assert.equal(
      isEmailOrPhoneNumber("4582731273", { locale: "en-CA" }),
      "phone"
    );
  });
  it("should return an empty string for invalid phone number lengths in en-US/en-CA", () => {
    // Not enough digits
    assert.equal(isEmailOrPhoneNumber("4522", { locale: "en-US" }), "");
    assert.equal(isEmailOrPhoneNumber("4522", { locale: "en-CA" }), "");
  });
  it("should return 'phone' for valid German phone numbers", () => {
    assert.equal(
      isEmailOrPhoneNumber("145827312732", { locale: "de" }),
      "phone"
    );
  });
  it("should return 'phone' for any number of digits in other locales", () => {
    assert.equal(isEmailOrPhoneNumber("4", CONTENT), "phone");
  });
  it("should return an empty string for other invalid inputs", () => {
    assert.equal(
      isEmailOrPhoneNumber("abc", CONTENT),
      "",
      "abc should be invalid"
    );
    assert.equal(
      isEmailOrPhoneNumber("abc@", CONTENT),
      "",
      "abc@ should be invalid"
    );
    assert.equal(
      isEmailOrPhoneNumber("abc@foo", CONTENT),
      "",
      "abc@foo should be invalid"
    );
    assert.equal(
      isEmailOrPhoneNumber("123d1232", CONTENT),
      "",
      "123d1232 should be invalid"
    );
  });
});
