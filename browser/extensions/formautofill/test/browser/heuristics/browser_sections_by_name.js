/* Any copyright is dedicated to the Public Domain.
http://creativecommons.org/publicdomain/zero/1.0/ */

/* global add_heuristic_tests */

"use strict";

// The following are included in this test
// - One named billing section
// - One named billing section and one named shipping section
// - One named billing section and one section without name
// - Fields without section name are merged to a section with section name
// - Two sections without name

add_heuristic_tests([
  {
    description: `One named billing section`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `One billing section and one shipping section`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
            <input id="street-address" autocomplete="shipping street-address">
            <input id="postal-code" autocomplete="shipping postal-code">
            <input id="country" autocomplete="shipping country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
          addressType: "shipping",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `One billing section, one shipping section, and then billing section`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="street-address" autocomplete="shipping street-address">
            <input id="postal-code" autocomplete="shipping postal-code">
            <input id="country" autocomplete="shipping country">
            <input id="country" autocomplete="billing country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
          addressType: "shipping",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `one section without a name and one billing section`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="street-address">
            <input id="postal-code" autocomplete="postal-code">
            <input id="country" autocomplete="country">
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `One billing section and one section without a name`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
            <input id="street-address" autocomplete="street-address">
            <input id="postal-code" autocomplete="postal-code">
            <input id="country" autocomplete="country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
  {
    description: `Fields without section name are merged (test both before and after the section with a name)`,
    fixtureData: `
        <html><body>
            <input id="name" autocomplete="name">
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
            <input id="street-address" autocomplete="shipping street-address">
            <input id="postal-code" autocomplete="shipping postal-code">
            <input id="country" autocomplete="shipping country">
            <input id="name" autocomplete="name">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "name", addressType: "" },
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
          addressType: "shipping",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
          { fieldName: "name", addressType: "" },
        ],
      },
    ],
  },
  {
    description: `Fields without section name are merged, but do not merge if the field already exists`,
    fixtureData: `
        <html><body>
            <input id="name" autocomplete="name">
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
            <input id="name" autocomplete="name">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "name", addressType: "" },
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        invalid: true,
        fields: [{ fieldName: "name", reason: "autocomplete" }],
      },
    ],
  },
  {
    description: `Fields without section name are merged (multi-fields)`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="billing street-address">
            <input id="postal-code" autocomplete="billing postal-code">
            <input id="country" autocomplete="billing country">
            <input id="email" autocomplete="email">
            <input id="email" autocomplete="email">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
          addressType: "billing",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
          { fieldName: "email", addressType: "" },
          { fieldName: "email", addressType: "" },
        ],
      },
    ],
  },
  {
    description: `Two sections without name`,
    fixtureData: `
        <html><body>
            <input id="street-address" autocomplete="street-address">
            <input id="postal-code" autocomplete="postal-code">
            <input id="country" autocomplete="country">
            <input id="street-address" autocomplete="street-address">
            <input id="postal-code" autocomplete="postal-code">
            <input id="country" autocomplete="country">
        </body></html>
      `,
    expectedResult: [
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
      {
        default: {
          reason: "autocomplete",
        },
        fields: [
          { fieldName: "street-address" },
          { fieldName: "postal-code" },
          { fieldName: "country" },
        ],
      },
    ],
  },
]);
