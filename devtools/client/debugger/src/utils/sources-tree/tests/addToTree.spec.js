/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

/* eslint max-nested-callbacks: ["error", 4]*/

import { makeMockDisplaySource, formatTree } from "../../../utils/test-mockup";

import {
  addToTree,
  createDirectoryNode,
  createSourceNode,
  createTree,
  nodeHasChildren,
} from "../index";

function createSourcesMap(sources) {
  const sourcesMap = sources.reduce((map, source) => {
    map[source.id] = makeMockDisplaySource(
      source.url,
      source.id,
      source.thread
    );
    return map;
  }, {});

  return sourcesMap;
}

function createSourcesList(sources) {
  return sources.map((s, i) => makeMockDisplaySource(s.url, s.id));
}

function getChildNode(tree, ...path) {
  return path.reduce((child, index) => child.contents[index], tree);
}

describe("sources-tree", () => {
  describe("addToTree", () => {
    it("should provide node API", () => {
      const source = makeMockDisplaySource(
        "http://example.com/a/b/c.js",
        "actor1"
      );

      const root = createDirectoryNode("root", "", [
        createSourceNode("foo", "/foo", source),
      ]);

      expect(root.name).toBe("root");
      expect(nodeHasChildren(root)).toBe(true);
      expect(root.contents).toHaveLength(1);

      const child = root.contents[0];
      expect(child.name).toBe("foo");
      expect(child.path).toBe("/foo");
      expect(child.contents).toBe(source);
      expect(nodeHasChildren(child)).toBe(false);
    });

    it("builds a path-based tree", () => {
      const source1 = makeMockDisplaySource(
        "http://example.com/foo/source1.js",
        "actor1"
      );
      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      expect(tree.contents).toHaveLength(1);

      const base = tree.contents[0].contents[0];
      expect(base.name).toBe("example.com");
      expect(base.contents).toHaveLength(1);

      const fooNode = base.contents[0];
      expect(fooNode.name).toBe("foo");
      expect(fooNode.contents).toHaveLength(1);

      const source1Node = fooNode.contents[0];
      expect(source1Node.name).toBe("source1.js");
    });

    it("builds a path-based tree for webpack URLs", () => {
      const source1 = makeMockDisplaySource(
        "webpack:///foo/source1.js",
        "actor1",
        ""
      );
      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      expect(tree.contents).toHaveLength(1);

      const base = tree.contents[0];
      expect(base.name).toBe("webpack://");
      expect(base.contents).toHaveLength(1);

      const fooNode = base.contents[0];
      expect(fooNode.name).toBe("foo");
      expect(fooNode.contents).toHaveLength(1);

      const source1Node = fooNode.contents[0];
      expect(source1Node.name).toBe("source1.js");
    });

    it("builds a path-based tree for webpack URLs with absolute path", () => {
      const source1 = makeMockDisplaySource(
        "webpack:////Users/foo/source1.js",
        "actor1",
        ""
      );
      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      expect(tree.contents).toHaveLength(1);

      const base = tree.contents[0];
      expect(base.name).toBe("webpack://");
      expect(base.contents).toHaveLength(1);

      const userNode = base.contents[0];
      expect(userNode.name).toBe("Users");
      expect(userNode.contents).toHaveLength(1);

      const fooNode = userNode.contents[0];
      expect(fooNode.name).toBe("foo");
      expect(fooNode.contents).toHaveLength(1);

      const source1Node = fooNode.contents[0];
      expect(source1Node.name).toBe("source1.js");
    });

    it("handles url with no filename", function() {
      const source1 = makeMockDisplaySource(
        "http://example.com/",
        "actor1",
        ""
      );
      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      expect(tree.contents).toHaveLength(1);

      const base = tree.contents[0];
      expect(base.name).toBe("example.com");
      expect(base.contents).toHaveLength(1);

      const indexNode = base.contents[0];
      expect(indexNode.name).toBe("(index)");
    });

    it("does not mangle encoded URLs", () => {
      const sourceName = // eslint-disable-next-line max-len
        "B9724220.131821496;dc_ver=42.111;sz=468x60;u_sd=2;dc_adk=2020465299;ord=a53rpc;dc_rfl=1,https%3A%2F%2Fdavidwalsh.name%2F$0;xdt=1";

      const source1 = makeMockDisplaySource(
        `https://example.com/foo/${sourceName}`,
        "actor1"
      );

      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      const childNode = getChildNode(tree, 0, 0, 0, 0);
      expect(childNode.name).toEqual(sourceName);
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("name does include query params", () => {
      const sourceName = "name.js?bar=3";

      const source1 = makeMockDisplaySource(
        `https://example.com/foo/${sourceName}`,
        "actor1"
      );

      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source1);
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("does not attempt to add two of the same directory", () => {
      const sources = [
        {
          id: "server1.conn13.child1/39",
          url: "https://davidwalsh.name/wp-content/prism.js",
        },
        {
          id: "server1.conn13.child1/37",
          url: "https://davidwalsh.name/",
        },
      ];

      const sourceMap = { FakeThread: createSourcesMap(sources) };
      const tree = createTree({
        sources: sourceMap,
        mainThreadHost: "",
        threads: [
          {
            actor: "FakeThread",
            name: "FakeThread",
            url: "https://davidwalsh.name",
            targetType: "worker",
            isTopLevel: false,
          },
        ],
      }).sourceTree;

      expect(tree.contents[0].contents).toHaveLength(1);
      const subtree = tree.contents[0].contents[0];
      expect(subtree.contents).toHaveLength(2);
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("supports data URLs", () => {
      const sources = [
        {
          id: "server1.conn13.child1/39",
          url: "data:text/html,<script>console.log(123)</script>",
        },
      ];

      const sourceMap = { FakeThread: createSourcesMap(sources) };
      const tree = createTree({
        sources: sourceMap,
        mainThreadHost: "",
        threads: [
          {
            actor: "FakeThread",
            url: "https://davidwalsh.name",
            targetType: "worker",
            name: "FakeThread",
            isTopLevel: false,
          },
        ],
      }).sourceTree;
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("does not attempt to add two of the same file", () => {
      const sources = [
        {
          id: "server1.conn13.child1/39",
          url: "https://davidwalsh.name/",
        },
        {
          id: "server1.conn13.child1/37",
          url: "https://davidwalsh.name/util.js",
        },
      ];
      const sources2 = [
        {
          id: "server1.conn13.child1/37",
          url: "https://davidwalsh.name/util.js",
          thread: "FakeThread2",
        },
      ];

      const sourceMap = {
        FakeThread: createSourcesMap(sources),
        FakeThread2: createSourcesMap(sources2),
      };

      const tree = createTree({
        sources: sourceMap,
        mainThreadHost: "davidwalsh.name",
        threads: [
          {
            actor: "FakeThread",
            name: "FakeThread",
            url: "https://davidwalsh.name",
            targetType: "worker",
            isTopLevel: false,
          },
          {
            actor: "FakeThread2",
            name: "FakeThread2",
            url: "https://davidwalsh.name/WorkerA.js",
            targetType: "worker",
            isTopLevel: false,
          },
        ],
      }).sourceTree;

      expect(tree.contents[0].contents).toHaveLength(1);
      const subtree = tree.contents[0].contents[0];
      expect(subtree.contents).toHaveLength(2);
      const subtree2 = tree.contents[1].contents[0];
      expect(subtree2.contents).toHaveLength(1);
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("correctly parses file sources", () => {
      const source = makeMockDisplaySource("file:///a/b.js", "actor1");
      const tree = createDirectoryNode("root", "", []);

      addToTree(tree, source);
      expect(tree.contents).toHaveLength(1);

      const base = tree.contents[0].contents[0];
      expect(base.name).toBe("file://");
      expect(base.contents).toHaveLength(1);

      const aNode = base.contents[0];
      expect(aNode.name).toBe("a");
      expect(aNode.contents).toHaveLength(1);

      const bNode = aNode.contents[0];
      expect(bNode.name).toBe("b.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("can add a file to an intermediate directory", () => {
      const testData = [
        {
          id: "server1.conn13.child1/39",
          url: "https://unpkg.com/codemirror@5.1/mode/xml/xml.js",
        },
        {
          id: "server1.conn13.child1/37",
          url: "https://unpkg.com/codemirror@5.1",
        },
      ];

      const sources = createSourcesList(testData);
      const tree = createDirectoryNode("root", "", []);
      sources.forEach(source => addToTree(tree, source));
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("replaces a file with a directory", () => {
      const testData = [
        {
          id: "server1.conn13.child1/37",
          url: "https://unpkg.com/codemirror@5.1",
        },

        {
          id: "server1.conn13.child1/39",
          url: "https://unpkg.com/codemirror@5.1/mode/xml/xml.js",
        },
      ];

      const sources = createSourcesList(testData);
      const tree = createDirectoryNode("root", "", []);
      sources.forEach(source => addToTree(tree, source));
      expect(formatTree(tree)).toMatchSnapshot();
    });
  });
});
