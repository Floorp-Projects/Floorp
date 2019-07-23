/* Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */
"use strict";

const { AttributionCode } = ChromeUtils.import(
  "resource:///modules/AttributionCode.jsm"
);

let validAttrCodes = [
  {
    code:
      "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
      content: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D%26content%3D",
    parsed: { source: "google.com", medium: "organic" },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic%26campaign%3D(not%20set)",
    parsed: {
      source: "google.com",
      medium: "organic",
      campaign: "(not%20set)",
    },
  },
  {
    code: "source%3Dgoogle.com%26medium%3Dorganic",
    parsed: { source: "google.com", medium: "organic" },
  },
  { code: "source%3Dgoogle.com", parsed: { source: "google.com" } },
  { code: "medium%3Dgoogle.com", parsed: { medium: "google.com" } },
  { code: "campaign%3Dgoogle.com", parsed: { campaign: "google.com" } },
  { code: "content%3Dgoogle.com", parsed: { content: "google.com" } },
  {
    code: "experiment%3Dexperimental",
    parsed: { experiment: "experimental" },
  },
  { code: "variation%3Dvaried", parsed: { variation: "varied" } },
];

let invalidAttrCodes = [
  // Empty string
  "",
  // Not escaped
  "source=google.com&medium=organic&campaign=(not set)&content=(not set)",
  // Too long
  "source%3Dreallyreallyreallyreallyreallyreallyreallyreallyreallylongdomain.com%26medium%3Dorganic%26campaign%3D(not%20set)%26content%3Dalmostexactlyenoughcontenttomakethisstringlongerthanthe200characterlimit",
  // Unknown key name
  "source%3Dgoogle.com%26medium%3Dorganic%26large%3Dgeneticallymodified",
  // Empty key name
  "source%3Dgoogle.com%26medium%3Dorganic%26%3Dgeneticallymodified",
];
