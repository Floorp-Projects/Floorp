/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { mount, shallow } from "enzyme";

import ManagedTree from "../ManagedTree";

function getTestContent() {
  const testTree = {
    a: {
      value: "FOO",
      children: [
        { value: 1 },
        { value: 2 },
        { value: 3 },
        { value: 4 },
        { value: 5 },
      ],
    },
    b: {
      value: "BAR",
      children: [
        { value: "A" },
        { value: "B" },
        { value: "C" },
        { value: "D" },
        { value: "E" },
      ],
    },
    c: { value: "BAZ" },
  };
  const renderItem = item => <div>{item.value ? item.value : item}</div>;
  const onFocus = jest.fn();
  const onExpand = jest.fn();
  const onCollapse = jest.fn();
  const getPath = (item, i) => {
    if (item.value) {
      return item.value;
    }
    if (i) {
      return `${i}`;
    }
    return `${item}-$`;
  };

  return {
    testTree,
    props: {
      getRoots: () => Object.keys(testTree),
      getParent: item => null,
      getChildren: branch => branch.children || [],
      itemHeight: 24,
      autoExpandAll: true,
      autoExpandDepth: 1,
      getPath,
      renderItem,
      onFocus,
      onExpand,
      onCollapse,
    },
  };
}

describe("ManagedTree", () => {
  it("render", () =>
    expect(
      shallow(<ManagedTree {...getTestContent().props} />)
    ).toMatchSnapshot());
  it("expands list items", () => {
    const { props, testTree } = getTestContent();
    const wrapper = shallow(<ManagedTree {...props} />);
    wrapper.setProps({
      listItems: testTree.b.children,
    });
    expect(wrapper).toMatchSnapshot();
  });
  it("highlights list items", () => {
    const { props, testTree } = getTestContent();
    const wrapper = shallow(<ManagedTree {...props} />);
    wrapper.setProps({
      highlightItems: testTree.a.children,
    });
    expect(wrapper).toMatchSnapshot();
  });

  it("sets expanded items", () => {
    const { props, testTree } = getTestContent();
    const wrapper = mount(<ManagedTree {...props} />);
    expect(wrapper).toMatchSnapshot();
    // We auto-expanded the first layer, so unexpand first node.
    wrapper
      .find("TreeNode")
      .first()
      .simulate("click");
    expect(wrapper).toMatchSnapshot();
    expect(props.onExpand).toHaveBeenCalledWith(
      "c",
      new Set(
        Object.keys(testTree)
          .filter(i => i !== "a")
          .map(k => `${k}-$`)
      )
    );
    wrapper
      .find("TreeNode")
      .first()
      .simulate("click");
    expect(props.onExpand).toHaveBeenCalledWith(
      "c",
      new Set(Object.keys(testTree).map(k => `${k}-$`))
    );
  });
});
