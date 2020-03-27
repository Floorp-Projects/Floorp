/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

// @flow

import React from "react";
import { shallow } from "enzyme";
import { showMenu } from "devtools-contextmenu";

import SourcesTreeItem from "../SourcesTreeItem";
import { makeMockSource } from "../../../utils/test-mockup";
import { copyToTheClipboard } from "../../../utils/clipboard";

jest.mock("devtools-contextmenu", () => ({ showMenu: jest.fn() }));
jest.mock("../../../utils/clipboard", () => ({
  copyToTheClipboard: jest.fn(),
}));

const blackBoxAllContexMenuItem = {
  id: "node-blackbox-all",
  label: "Blackbox",
  submenu: [
    {
      id: "node-blackbox-all-inside",
      label: "Blackbox files in this directory",
      disabled: false,
      click: expect.any(Function),
    },
    {
      id: "node-blackbox-all-outside",
      label: "Blackbox files outside this directory",
      disabled: false,
      click: expect.any(Function),
    },
  ],
};

describe("SourceTreeItem", () => {
  afterEach(() => {
    (copyToTheClipboard: any).mockClear();
    showMenu.mockClear();
  });

  it("should show menu on contextmenu of an item", async () => {
    const { instance, component } = render();
    const { item } = instance.props;
    instance.onContextMenu = jest.fn(() => {});

    const event = { event: "contextmenu" };
    component.simulate("contextmenu", event);
    expect(instance.onContextMenu).toHaveBeenCalledWith(event, item);
  });

  describe("onContextMenu of the tree", () => {
    it("shows context menu on directory to set as root", async () => {
      const menuOptions = [
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-collapse-all",
          label: "Collapse all",
        },
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-expand-all",
          label: "Expand all",
        },
        {
          accesskey: "r",
          click: expect.any(Function),
          disabled: false,
          id: "node-set-directory-root",
          label: "Set directory root",
        },
        blackBoxAllContexMenuItem,
      ];
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };
      const { props, instance } = render({
        projectRoot: "root/",
      });
      await instance.onContextMenu(mockEvent, createMockDirectory());
      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][2].click();
      expect(props.setProjectDirectoryRoot).toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(copyToTheClipboard).not.toHaveBeenCalled();
    });

    it("shows context menu on file to copy source uri", async () => {
      const menuOptions = [
        {
          accesskey: "u",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-copy-source",
          label: "Copy source URI",
        },
        {
          accesskey: "B",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-blackbox",
          label: "Blackbox source",
        },
        {
          accesskey: "d",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-download-file",
          label: "Download file",
        },
      ];
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };
      const { props, instance } = render({
        projectRoot: "root/",
      });
      const { item, source } = instance.props;

      await instance.onContextMenu(mockEvent, item, source);

      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);
      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][0].click();
      expect(props.setProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(copyToTheClipboard).toHaveBeenCalled();
    });

    it("shows context menu on file to blackbox source", async () => {
      const menuOptions = [
        {
          accesskey: "u",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-copy-source",
          label: "Copy source URI",
        },
        {
          accesskey: "B",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-blackbox",
          label: "Blackbox source",
        },
        {
          accesskey: "d",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-download-file",
          label: "Download file",
        },
      ];
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };
      const { props, instance } = render({
        projectRoot: "root/",
      });
      const { item, source } = instance.props;

      await instance.onContextMenu(mockEvent, item, source);

      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][1].click();
      expect(props.setProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.toggleBlackBox).toHaveBeenCalled();
    });

    it("shows context menu on directory to blackbox all with submenu options", async () => {
      const menuOptions = [
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-collapse-all",
          label: "Collapse all",
        },
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-expand-all",
          label: "Expand all",
        },
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-remove-directory-root",
          label: "Remove directory root",
        },
        blackBoxAllContexMenuItem,
      ];

      const { props, instance } = render({
        projectRoot: "root/",
      });

      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };

      await instance.onContextMenu(
        mockEvent,
        createMockDirectory("root/", "root")
      );

      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][3].submenu[0].click();
      showMenu.mock.calls[0][1][3].submenu[1].click();

      expect(props.setProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).not.toHaveBeenCalled();

      expect(props.blackBoxSources.mock.calls).toHaveLength(2);
    });

    it("shows context menu on file to download source file", async () => {
      const menuOptions = [
        {
          accesskey: "u",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-copy-source",
          label: "Copy source URI",
        },
        {
          accesskey: "B",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-blackbox",
          label: "Blackbox source",
        },
        {
          accesskey: "d",
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-download-file",
          label: "Download file",
        },
      ];
      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };

      const { props, instance } = render({
        projectRoot: "root/",
      });
      const { item, source } = instance.props;

      instance.handleDownloadFile = jest.fn(() => {});

      await instance.onContextMenu(mockEvent, item, source);

      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][2].click();
      expect(props.setProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.toggleBlackBox).not.toHaveBeenCalled();
      expect(copyToTheClipboard).not.toHaveBeenCalled();
      expect(instance.handleDownloadFile).toHaveBeenCalled();
    });

    it("shows context menu on root to remove directory root", async () => {
      const menuOptions = [
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-collapse-all",
          label: "Collapse all",
        },
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-menu-expand-all",
          label: "Expand all",
        },
        {
          click: expect.any(Function),
          disabled: false,
          id: "node-remove-directory-root",
          label: "Remove directory root",
        },
        blackBoxAllContexMenuItem,
      ];
      const { props, instance } = render({
        projectRoot: "root/",
      });

      const mockEvent = {
        preventDefault: jest.fn(),
        stopPropagation: jest.fn(),
      };

      await instance.onContextMenu(
        mockEvent,
        createMockDirectory("root/", "root")
      );

      expect(showMenu).toHaveBeenCalledWith(mockEvent, menuOptions);

      expect(mockEvent.preventDefault).toHaveBeenCalled();
      expect(mockEvent.stopPropagation).toHaveBeenCalled();

      showMenu.mock.calls[0][1][2].click();
      expect(props.setProjectDirectoryRoot).not.toHaveBeenCalled();
      expect(props.clearProjectDirectoryRoot).toHaveBeenCalled();
      expect(copyToTheClipboard).not.toHaveBeenCalled();
    });
  });

  describe("renderItem", () => {
    it("should show icon for webpack item", async () => {
      const item = createMockDirectory("webpack://", "webpack://");
      const node = render({ item });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for angular item", async () => {
      const item = createMockDirectory("ng://", "ng://");
      const node = render({ item });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for moz-extension item", async () => {
      const item = createMockDirectory(
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856",
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856"
      );
      const node = render({ item, depth: 1 });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for moz-extension item when a thread is set to root", async () => {
      const item = createMockDirectory(
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856",
        "moz-extension://e37c3c08-beac-a04b-8032-c4f699a1a856"
      );
      const node = render({
        item,
        depth: 0,
        projectRoot: "server1.conn13.child1/thread19",
        threads: [
          { name: "Main Thread", actor: "server1.conn13.child1/thread19" },
        ],
      });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for domain item when a thread is set to root", async () => {
      const item = createMockDirectory();
      const node = render({
        item,
        depth: 0,
        projectRoot: "server1.conn13.child1/thread19",
        threads: [
          { name: "Main Thread", actor: "server1.conn13.child1/thread19" },
        ],
      });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for folder with arrow", async () => {
      const item = createMockDirectory();
      const node = render({ item, source: null });
      expect(node).toMatchSnapshot();
    });

    it("should show icon for folder with expanded arrow", async () => {
      const node = render({
        item: createMockDirectory(),
        source: null,
        depth: 1,
        focused: false,
        expanded: true,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show focused item for folder with expanded arrow", async () => {
      const node = render({
        item: createMockDirectory(),
        source: null,
        depth: 1,
        focused: true,
        expanded: true,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show source item with source icon", async () => {
      const node = render({ item: createMockItem() });
      expect(node).toMatchSnapshot();
    });

    it("should show source item with blackbox icon", async () => {
      const isBlackBoxed = true;
      const mockSource = {
        ...makeMockSource("http://mdn.com/one.js", "server1.conn13.child1/39"),
        isBlackBoxed,
      };
      const node = render({
        item: createMockItem({ contents: mockSource }),
        source: mockSource,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show source item with prettyPrint icon", async () => {
      const node = render({
        item: createMockItem(),
        hasPrettySource: true,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show (mapped) for duplicate source items", async () => {
      const node = render({
        item: createMockItem(),
        hasMatchingGeneratedSource: true,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show source item with source icon with focus", async () => {
      const node = render({
        depth: 1,
        focused: true,
        expanded: false,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show domain item", async () => {
      const node = render({
        item: createMockItem({ name: "root", path: "root" }),
        depth: 0,
      });
      expect(node).toMatchSnapshot();
    });

    it("should show domain item as debuggee", async () => {
      const node = render({
        item: createMockDirectory("root", "http://mdn.com"),
        depth: 0,
      });

      expect(node).toMatchSnapshot();
    });

    it("should show domain item as debuggee with focus and arrow", async () => {
      const node = render({
        item: createMockDirectory("root", "http://mdn.com"),
        depth: 0,
        focused: true,
      });

      expect(node).toMatchSnapshot();
    });

    it("should not show domain item when the projectRoot exists", async () => {
      const node = render({
        projectRoot: "root/",
      });
      expect(node).toMatchSnapshot();
    });

    it("should focus on and select item on click", async () => {
      const event = { event: "click" };
      const selectItem = jest.fn();
      const { component, instance, props } = render({
        depth: 1,
        focused: true,
        expanded: false,
        selectItem,
      });

      const { item } = instance.props;
      component.simulate("click", event);
      await component.simulate("keydown", { keyCode: 13 });
      expect(props.selectItem).toHaveBeenCalledWith(item);
    });

    it("should focus on directory on click", async () => {
      const selectItem = jest.fn();

      const { component, props } = render({
        item: createMockDirectory(),
        source: null,
        depth: 1,
        focused: true,
        expanded: false,
        selectItem,
      });

      component.simulate("click", { event: "click" });
      expect(props.selectItem).not.toHaveBeenCalled();
    });

    it("should unescape escaped source URLs", async () => {
      const item = createMockItem({
        path: "mdn.com/external%20file",
        name: "external%20file",
      });

      const node = render({ item });

      expect(node).toMatchSnapshot();
    });
  });
});

function generateDefaults(overrides) {
  const source = makeMockSource(
    "http://mdn.com/one.js",
    "server1.conn13.child1/39"
  );

  const item = {
    name: "one.js",
    path: "mdn.com/one.js",
    contents: source,
  };

  return {
    expanded: false,
    item,
    source,
    debuggeeUrl: "http://mdn.com",
    projectRoot: "",
    clearProjectDirectoryRoot: jest.fn(),
    setProjectDirectoryRoot: jest.fn(),
    toggleBlackBox: jest.fn(),
    selectItem: jest.fn(),
    focusItem: jest.fn(),
    setExpanded: jest.fn(),
    blackBoxSources: jest.fn(),
    getSourcesGroups: () => {
      return {
        sourcesInside: [makeMockSource("https://example.com/a.js", "actor1")],
        sourcesOuside: [makeMockSource("https://example.com/b.js", "actor2")],
      };
    },
    threads: [{ name: "Main Thread" }],
    ...overrides,
  };
}

function render(overrides = {}) {
  const props = generateDefaults(overrides);
  // $FlowIgnore
  const component = shallow(<SourcesTreeItem.WrappedComponent {...props} />);
  const defaultState = component.state();
  const instance = component.instance();

  return { component, props, defaultState, instance };
}

function createMockDirectory(
  path = "domain/subfolder",
  name = "folder",
  contents = []
) {
  return {
    type: "directory",
    name,
    path,
    contents,
  };
}

function createMockItem(overrides = {}) {
  overrides = {
    ...overrides,
    contents: {
      ...makeMockSource(undefined, "server1.conn13.child1/39"),
      ...(overrides.contents || {}),
    },
  };

  return {
    type: "source",
    name: "one.js",
    path: "http://mdn.com/one.js",
    ...overrides,
  };
}
