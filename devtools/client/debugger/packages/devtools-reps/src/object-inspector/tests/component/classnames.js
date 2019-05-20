/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

const ObjectClient = require("../__mocks__/object-client");
const { mountObjectInspector } = require("../test-utils");

function generateDefaults(overrides) {
  return {
    autoExpandDepth: 0,
    roots: [
      {
        path: "root",
        name: "root",
        contents: { value: 42 },
      },
    ],
    ...overrides,
  };
}

function mount(props) {
  const client = { createObjectClient: grip => ObjectClient(grip) };

  return mountObjectInspector({
    client,
    props: generateDefaults(props),
  });
}

describe("ObjectInspector - classnames", () => {
  it("has the expected class", () => {
    const { tree } = mount();
    expect(tree.hasClass("tree")).toBeTruthy();
    expect(tree.hasClass("inline")).toBeFalsy();
    expect(tree.hasClass("nowrap")).toBeFalsy();
    expect(tree).toMatchSnapshot();
  });

  it("has the nowrap class when disableWrap prop is true", () => {
    const { tree } = mount({ disableWrap: true });
    expect(tree.hasClass("nowrap")).toBeTruthy();
    expect(tree).toMatchSnapshot();
  });

  it("has the inline class when inline prop is true", () => {
    const { tree } = mount({ inline: true });
    expect(tree.hasClass("inline")).toBeTruthy();
    expect(tree).toMatchSnapshot();
  });
});
