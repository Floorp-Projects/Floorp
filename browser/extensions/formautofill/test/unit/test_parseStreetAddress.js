"use strict";

const { AddressParser } = ChromeUtils.importESModule(
  "resource://gre/modules/shared/AddressParser.sys.mjs"
);

// To add a new test entry to a "TESTCASES" variable,
// you would need to create a new array containing two elements.
// - The first element is a string representing a street address to be parsed.
// - The second element is an array containing the expected output after parsing the address,
//   which should follow the format of
//   [street number, street name, apartment number (if applicable), floor number (if applicable)].
//
// Note. If we expect the passed street address cannot be parsed, set the second element to null.
const TESTCASES = [
  ["123 Main St. Apt 4, Floor 2", [123, "Main St.", "4", 2]],
  ["32 Vassar Street MIT Room 4", [32, "Vassar Street MIT", "4", null]],
  ["32 Vassar Street MIT", [32, "Vassar Street MIT", null, null]],
  [
    "32 Vassar Street MIT Room 32-G524",
    [32, "Vassar Street MIT", "32-G524", null],
  ],
  ["163 W Hastings\nSuite 209", [163, "W Hastings", "209", null]],
  ["1234 Elm St. Apt 4, Floor 2", [1234, "Elm St.", "4", 2]],
  ["456 Oak Drive, Unit 2A", [456, "Oak Drive", "2A", null]],
  ["789 Maple Ave, Suite 300", [789, "Maple Ave", "300", null]],
  ["321 Willow Lane, #5", [321, "Willow Lane", "5", null]],
  ["654 Pine Circle, Apt B", [654, "Pine Circle", "B", null]],
  ["987 Birch Court, 3rd Floor", [987, "Birch Court", null, 3]],
  ["234 Cedar Way, Unit 6-2", [234, "Cedar Way", "6-2", null]],
  ["345 Cherry St, Ste 12", [345, "Cherry St", "12", null]],
  ["234 Palm St, Bldg 1, Apt 12", null],
];

add_task(async function test_parseStreetAddress() {
  for (const TEST of TESTCASES) {
    let [address, expected] = TEST;
    const result = AddressParser.parseStreetAddress(address);
    if (!expected) {
      Assert.equal(result, null, "Expect failure to parse this street address");
      continue;
    }

    const options = {
      trim: true,
      ignore_case: true,
    };

    const expectedSN = AddressParser.normalizeString(expected[0], options);
    Assert.equal(
      result.street_number,
      expectedSN,
      `expect street number to be ${expectedSN}, but got ${result.street_number}`
    );
    const expectedSNA = AddressParser.normalizeString(expected[1], options);
    Assert.equal(
      result.street_name,
      expectedSNA,
      `expect street name to be ${expectedSNA}, but got ${result.street_name}`
    );
    const expectedAN = AddressParser.normalizeString(expected[2], options);
    Assert.equal(
      result.apartment_number,
      expectedAN,
      `expect apartment number to be ${expectedAN}, but got ${result.apartment_number}`
    );
    const expectedFN = AddressParser.normalizeString(expected[3], options);
    Assert.equal(
      result.floor_number,
      expectedFN,
      `expect floor number to be ${expectedFN}, but got ${result.floor_number}`
    );
  }
});
