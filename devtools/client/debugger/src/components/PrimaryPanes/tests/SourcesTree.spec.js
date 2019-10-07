/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import { showMenu } from "devtools-contextmenu";

import SourcesTree from "../SourcesTree";
import { makeMockSource, mockcx } from "../../../utils/test-mockup";
import { copyToTheClipboard } from "../../../utils/clipboard";

jest.mock("devtools-contextmenu", () => ({ showMenu: jest.fn() }));
jest.mock("../../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn(),
}));

describe("SourcesTree", () => {
  afterEach(() => {
    (copyToTheClipboard: any).mockClear();
    showMenu.mockClear();
  });

  it("Should show the tree with nothing expanded", async () => {
    const { component } = render();
    expect(component).toMatchSnapshot();
  });

  it("Should show a 'No Sources' message if there are no sources", async () => {
    const { component, defaultState } = render();
    const sourceTree = defaultState.sourceTree;
    sourceTree.contents = [];
    component.setState({ sourceTree: sourceTree });
    expect(component).toMatchSnapshot();
  });

  describe("When loading initial source", () => {
    it("Shows the tree with one.js, two.js and three.js expanded", async () => {
      const { component, props } = render();
      await component.setProps({
        ...props,
        expanded: ["one.js", "two.js", "three.js"],
      });

      expect(component).toMatchSnapshot();
    });
  });

  describe("After changing expanded nodes", () => {
    it("Shows the tree with four.js, five.js and six.js expanded", async () => {
      const { component, props } = render();
      await component.setProps({
        ...props,
        expanded: ["four.js", "five.js", "six.js"],
      });

      expect(component).toMatchSnapshot();
    });
  });

  describe("on receiving new props", () => {
    describe("recreates tree", () => {
      it("does not recreate tree if no new source is added", async () => {
        const { component, props, defaultState } = render();
        const sources = {
          "server1.conn13.child1/41": createMockSource(
            "server1.conn13.child1/41",
            "http://mdn.com/three.js"
          ),
        };

        await component.setProps({ ...props, sources });

        expect(component.state("uncollapsedTree")).toEqual(
          defaultState.uncollapsedTree
        );
      });

      it("updates tree with a new item", async () => {
        const { component, props } = render();
        const newSource = createMockSource(
          "server1.conn13.child1/43",
          "http://mdn.com/four.js",
          true
        );

        const newThreadSources = {
          ...props.sources.FakeThread,
          "server1.conn13.child1/43": newSource,
        };

        await component.setProps({
          ...props,
          sources: newThreadSources,
        });

        expect(
          component.state("uncollapsedTree").contents[0].contents[0].contents
        ).toHaveLength(5);
      });

      it("updates sources if sources are emptied", async () => {
        const { component, props, defaultState } = render();

        expect(defaultState.uncollapsedTree.contents).toHaveLength(1);

        await component.setProps({
          ...props,
          sources: {},
          sourceCount: 0,
        });

        expect(component.state("uncollapsedTree").contents).toHaveLength(0);
      });

      it("recreates tree if projectRoot is changed", async () => {
        const { component, props, defaultState } = render();
        const sources = {
          FakeThread: {
            "server1.conn13.child1/41": createMockSource(
              "server1.conn13.child1/41",
              "http://mozilla.com/three.js"
            ),
          },
        };

        expect(
          defaultState.uncollapsedTree.contents[0].contents[0].contents
        ).toHaveLength(5);

        await component.setProps({
          ...props,
          sources,
          projectRoot: "mozilla",
        });

        expect(
          component.state("uncollapsedTree").contents[0].contents
        ).toHaveLength(1);
      });

      it("recreates tree if debuggeeUrl is changed", async () => {
        const { component, props, defaultState } = render();
        const sources = {
          FakeThread: {
            "server1.conn13.child1/41": createMockSource(
              "server1.conn13.child1/41",
              "http://mdn.com/three.js",
              true
            ),
          },
        };

        expect(
          defaultState.uncollapsedTree.contents[0].contents[0].contents
        ).toHaveLength(5);

        await component.setProps({
          ...props,
          debuggeeUrl: "mozilla",
          sources,
        });

        expect(
          component.state("uncollapsedTree").contents[0].contents
        ).toHaveLength(1);
      });
    });

    describe("updates threads", () => {
      it("adds sources to the correct thread", async () => {
        const { component, props } = render();
        const newSource = createMockSource(
          "server1.conn13.child1/43",
          "http://mdn.com/four.js",
          true
        );

        const newThreadSources = {
          FakeThread1: {
            "server1.conn13.child1/43": newSource,
          },
        };

        expect(component.state("uncollapsedTree").contents[0].name).toEqual(
          "FakeThread"
        );
        expect(
          component.state("uncollapsedTree").contents[0].contents[0].contents
        ).toHaveLength(5);
        expect(component.state("uncollapsedTree").contents[1]).toEqual(
          undefined
        );

        await component.setProps({
          ...props,
          sources: {
            ...props.sources,
            ...newThreadSources,
          },
        });

        expect(component.state("uncollapsedTree").contents[0].name).toEqual(
          "FakeThread"
        );
        expect(
          component.state("uncollapsedTree").contents[0].contents[0].contents
        ).toHaveLength(5);

        expect(component.state("uncollapsedTree").contents[1].name).toEqual(
          "FakeThread1"
        );
        expect(
          component.state("uncollapsedTree").contents[1].contents[0].contents
        ).toHaveLength(1);
      });
    });

    describe("updates highlighted items", () => {
      it("updates highlightItems if selectedSource changes", async () => {
        const { component, props } = render();
        const mockSource = createMockSource(
          "server1.conn13.child1/41",
          "http://mdn.com/three.js",
          false
        );
        await component.setProps({
          ...props,
          selectedSource: mockSource,
        });
        expect(component).toMatchSnapshot();
      });
    });
  });

  describe("activateItem", () => {
    it("select activated item", async () => {
      const { instance, props } = render();
      const item = createMockItem();
      const spy = jest.spyOn(instance, "selectItem");

      instance.onActivate(item);
      expect(spy).toHaveBeenCalledWith(item);
      expect(props.selectSource).toHaveBeenCalledWith(
        mockcx,
        "server1.conn13.child1/39"
      );
    });
  });

  describe("selectItem", () => {
    it("should select item with no children", async () => {
      const { instance, props } = render();
      instance.selectItem(createMockItem());
      expect(props.selectSource).toHaveBeenCalledWith(
        mockcx,
        "server1.conn13.child1/39"
      );
    });

    it("should not select item with children", async () => {
      const { props, instance } = render();
      instance.selectItem(createMockDirectory());
      expect(props.selectSource).not.toHaveBeenCalled();
    });
  });

  describe("handles items", () => {
    it("getChildren from directory", async () => {
      const { component } = render();
      const item = createMockDirectory("http://mdn.com/views", "views", [
        "a",
        "b",
      ]);
      const children = component
        .find("ManagedTree")
        .props()
        .getChildren(item);
      expect(children).toEqual(["a", "b"]);
    });

    it("getChildren from non directory", async () => {
      const { component } = render();
      const children = component
        .find("ManagedTree")
        .props()
        .getChildren(createMockItem());
      expect(children).toEqual([]);
    });

    it("onExpand", async () => {
      const { component, props } = render();
      const expandedState = ["x", "y"];
      await component
        .find("ManagedTree")
        .props()
        .onExpand({}, expandedState);
      expect(props.setExpandedState).toHaveBeenCalledWith(expandedState);
    });

    it("onCollapse", async () => {
      const { component, props } = render();
      const expandedState = ["y", "z"];
      await component
        .find("ManagedTree")
        .props()
        .onCollapse({}, expandedState);
      expect(props.setExpandedState).toHaveBeenCalledWith(expandedState);
    });

    it("getParent", async () => {
      const { component } = render();
      const item = component.state("sourceTree").contents[0].contents[0];
      const parent = component
        .find("ManagedTree")
        .props()
        .getParent(item);

      expect(parent.path).toEqual("FakeThread");
      expect(parent.contents[0].contents).toHaveLength(5);
    });
  });

  describe("getPath", () => {
    it("should return path for item", async () => {
      const { instance } = render();
      const path = instance.getPath(createMockItem());
      expect(path).toEqual("http://mdn.com/one.js/server1.conn13.child1/39/");
    });

    it("should return path for generated item", async () => {
      const { instance } = render();
      const pathOriginal = instance.getPath(
        createMockItem("http://mdn.com/four.js", "four.js", {
          id: "server1.conn13.child1/42/originalSource-sha",
        })
      );
      expect(pathOriginal).toEqual(
        "http://mdn.com/four.js/server1.conn13.child1/42/originalSource-sha/"
      );

      const pathGenerated = instance.getPath(
        createMockItem("http://mdn.com/four.js", "four.js", {
          id: "server1.conn13.child1/42",
        })
      );
      expect(pathGenerated).toEqual(
        "http://mdn.com/four.js/server1.conn13.child1/42/"
      );
    });
  });
});

function generateDefaults(overrides: Object) {
  const defaultSources = {
    FakeThread: {
      "server1.conn13.child1/39": createMockSource(
        "server1.conn13.child1/39",
        "http://mdn.com/one.js",
        false
      ),
      "server1.conn13.child1/40": createMockSource(
        "server1.conn13.child1/40",
        "http://mdn.com/two.js",
        false
      ),
      "server1.conn13.child1/41": createMockSource(
        "server1.conn13.child1/41",
        "http://mdn.com/three.js",
        false
      ),
      "server1.conn13.child1/42/originalSource-sha": createMockSource(
        "server1.conn13.child1/42/originalSource-sha",
        "http://mdn.com/four.js",
        false
      ),
      "server1.conn13.child1/42": createMockSource(
        "server1.conn13.child1/42",
        "http://mdn.com/four.js"
      ),
    },
  };

  return {
    cx: mockcx,
    autoExpandAll: true,
    selectSource: jest.fn(),
    setExpandedState: jest.fn(),
    sources: defaultSources,
    debuggeeUrl: "http://mdn.com",
    clearProjectDirectoryRoot: jest.fn(),
    setProjectDirectoryRoot: jest.fn(),
    focusItem: jest.fn(),
    projectRoot: "",
    threads: [
      {
        name: "FakeThread",
        actor: "FakeThread",
      },
      {
        name: "FakeThread1",
        actor: "FakeThread1",
      },
    ],
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  // $FlowIgnore
  const component = shallow(<SourcesTree.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  instance.shouldComponentUpdate = () => true;

  return { component, props, defaultState, instance };
}

function createMockSource(id, url, isBlackBoxed = false) {
  return {
    ...makeMockSource(url, id),
    isBlackBoxed,
  };
}

function createMockDirectory(path = "folder/", name = "folder", contents = []) {
  return {
    type: "directory",
    name,
    path,
    contents,
  };
}

function createMockItem(
  path = "http://mdn.com/one.js",
  name = "one.js",
  contents = { id: "server1.conn13.child1/39" }
) {
  return {
    type: "source",
    name,
    path,
    contents,
  };
}
