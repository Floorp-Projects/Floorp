/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSource } from "../../../reducers/sources";

import {
  collapseTree,
  sortEntireTree,
  formatTree,
  addToTree,
  createDirectoryNode
} from "../index";

const abcSource = createSource({
  url: "http://example.com/a/b/c.js",
  id: "actor1"
});
const abcdeSource = createSource({
  url: "http://example.com/a/b/c/d/e.js",
  id: "actor2"
});
const abxSource = createSource({
  url: "http://example.com/a/b/x.js",
  id: "actor3"
});

describe("sources tree", () => {
  describe("collapseTree", () => {
    it("can collapse a single source", () => {
      const fullTree = createDirectoryNode("root", "", []);
      addToTree(fullTree, abcSource, "http://example.com/");
      expect(fullTree.contents).toHaveLength(1);
      const tree = collapseTree(fullTree);

      const host = tree.contents[0];
      expect(host.name).toBe("example.com");
      expect(host.contents).toHaveLength(1);

      const abFolder = host.contents[0];
      expect(abFolder.name).toBe("a/b");
      expect(abFolder.contents).toHaveLength(1);

      const abcNode = abFolder.contents[0];
      expect(abcNode.name).toBe("c.js");
      expect(abcNode.path).toBe("example.com/a/b/c.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("correctly merges in a collapsed source with a deeper level", () => {
      const fullTree = createDirectoryNode("root", "", []);
      addToTree(fullTree, abcSource, "http://example.com/");
      addToTree(fullTree, abcdeSource, "http://example.com/");
      const tree = collapseTree(fullTree);

      sortEntireTree(tree);
      expect(tree.contents).toHaveLength(1);

      const host = tree.contents[0];
      expect(host.name).toBe("example.com");
      expect(host.contents).toHaveLength(1);

      const abFolder = host.contents[0];
      expect(abFolder.name).toBe("a/b");
      expect(abFolder.contents).toHaveLength(2);

      const [cdFolder, abcNode] = abFolder.contents;
      expect(abcNode.name).toBe("c.js");
      expect(abcNode.path).toBe("example.com/a/b/c.js");
      expect(cdFolder.name).toBe("c/d");

      const [abcdeNode] = cdFolder.contents;
      expect(abcdeNode.name).toBe("e.js");
      expect(abcdeNode.path).toBe("example.com/a/b/c/d/e.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("correctly merges in a collapsed source with a shallower level", () => {
      const fullTree = createDirectoryNode("root", "", []);
      addToTree(fullTree, abcSource, "http://example.com/");
      addToTree(fullTree, abxSource, "http://example.com/");
      const tree = collapseTree(fullTree);

      expect(tree.contents).toHaveLength(1);

      const host = tree.contents[0];
      expect(host.name).toBe("example.com");
      expect(host.contents).toHaveLength(1);

      const abFolder = host.contents[0];
      expect(abFolder.name).toBe("a/b");
      expect(abFolder.contents).toHaveLength(2);

      const [abcNode, abxNode] = abFolder.contents;
      expect(abcNode.name).toBe("c.js");
      expect(abcNode.path).toBe("example.com/a/b/c.js");
      expect(abxNode.name).toBe("x.js");
      expect(abxNode.path).toBe("example.com/a/b/x.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("correctly merges in a collapsed source with the same level", () => {
      const fullTree = createDirectoryNode("root", "", []);
      addToTree(fullTree, abcdeSource, "http://example.com/");
      addToTree(fullTree, abcSource, "http://example.com/");
      const tree = collapseTree(fullTree);

      expect(tree.contents).toHaveLength(1);

      const host = tree.contents[0];
      expect(host.name).toBe("example.com");
      expect(host.contents).toHaveLength(1);

      const abFolder = host.contents[0];
      expect(abFolder.name).toBe("a/b");
      expect(abFolder.contents).toHaveLength(2);

      const [cdFolder, abcNode] = abFolder.contents;
      expect(abcNode.name).toBe("c.js");
      expect(abcNode.path).toBe("example.com/a/b/c.js");
      expect(cdFolder.name).toBe("c/d");

      const [abcdeNode] = cdFolder.contents;
      expect(abcdeNode.name).toBe("e.js");
      expect(abcdeNode.path).toBe("example.com/a/b/c/d/e.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });
  });
});
