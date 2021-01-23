/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

// InactivePropertyHelper `fill-*` and `stroke-*` test cases.
const PROPERTIES = [
  "fill",
  "fill-opacity",
  "stroke",
  "stroke-dasharray",
  "stroke-dashoffset",
  "stroke-linecap",
  "stroke-linejoin",
  "stroke-miterlimit",
  "stroke-opacity",
  "stroke-width",
];

const TEST_DATA = [
  {
    isActive: true,
    element: "circle",
    elementCode: '<circle cx="50" cy="50" r="50" fill="green" />',
  },
  {
    isActive: true,
    element: "ellipse",
    elementCode: '<ellipse cx="50" cy="50" rx="50" ry="20" fill="green" />',
  },
  {
    isActive: true,
    element: "line",
    elementCode: '<line x1="0" y1="80" x2="100" y2="20" stroke="black" />',
  },
  {
    isActive: true,
    element: "path",
    elementCode: `
      <path d="M 10,30
        A 20,20 0,0,1 50,30
        A 20,20 0,0,1 90,30
        Q 90,60 50,90
        Q 10,60 10,30 z"/>
    `,
  },
  {
    isActive: true,
    element: "polygon",
    elementCode: '<polygon points="0,50 25,12.5 25,37.5 50,0" />',
  },
  {
    isActive: true,
    element: "polyline",
    elementCode: `
      <polyline points="50,50 75,12.5 75,37.5 100,0" fill="none" stroke="black" />
    `,
  },
  {
    isActive: true,
    element: "rect",
    elementCode: '<rect width="100" fill="green" />',
  },
  {
    isActive: true,
    element: "text",
    elementCode: '<text x="20" y="20">Test</text>',
  },
  {
    isActive: true,
    element: "textPath",
    elementCode: `
      <path id="path" fill="none"
        d="M10,90 Q90,90 90,45 Q90,10 50,10 Q10,10 10,40 Q10,70 45,70 Q70,70 75,50" />
      <textPath href="#path">Quick brown fox jumps over the lazy dog.</textPath>
    `,
  },
  {
    isActive: true,
    element: "tspan",
    elementCode: '<text x="20" y="20">Te<tspan>st</tspan></text>',
  },
  {
    isActive: false,
    element: "a",
    elementCode:
      '<a href="https://mozilla.org"><text x="20" y="20">Test</text></a>',
  },
  {
    isActive: false,
    element: "div",
    elementCode: "<div></div>",
  },
];

function makeTestCase(property, isActive, element, elementCode) {
  return {
    info: `${property} is ${
      isActive ? "active" : "inactive"
    } on a <${element}> element`,
    property,
    createTestElement: main => {
      // eslint-disable-next-line no-unsanitized/property
      main.innerHTML =
        element === "div"
          ? elementCode
          : `
            <svg width="100" height="100">
              ${elementCode}
            </svg>
          `;
      return main.querySelector(element);
    },
    rules: [],
    isActive,
  };
}

const mainTests = [];

for (const property of PROPERTIES) {
  for (const { isActive, element, elementCode } of TEST_DATA) {
    mainTests.push(makeTestCase(property, isActive, element, elementCode));
  }
}

export default mainTests;
