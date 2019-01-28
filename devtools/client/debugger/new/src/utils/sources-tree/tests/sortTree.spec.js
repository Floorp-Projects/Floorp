/* eslint max-nested-callbacks: ["error", 4]*/
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at <http://mozilla.org/MPL/2.0/>. */

import { createSource } from "../../../reducers/sources";

import {
  addToTree,
  sortEntireTree,
  createDirectoryNode,
  formatTree
} from "../index";

describe("sources-tree", () => {
  describe("sortEntireTree", () => {
    it("alphabetically sorts children", () => {
      const source1 = createSource({
        url: "http://example.com/source1.js",
        id: "actor1"
      });
      const source2 = createSource({
        url: "http://example.com/foo/b_source2.js",
        id: "actor2"
      });
      const source3 = createSource({
        url: "http://example.com/foo/a_source3.js",
        id: "actor3"
      });
      const _tree = createDirectoryNode("root", "", []);

      addToTree(_tree, source1, "http://example.com/");
      addToTree(_tree, source2, "http://example.com/");
      addToTree(_tree, source3, "http://example.com/");
      const tree = sortEntireTree(_tree);

      const base = tree.contents[0];
      const fooNode = base.contents[0];
      expect(fooNode.name).toBe("foo");
      expect(fooNode.contents).toHaveLength(2);

      const source1Node = base.contents[1];
      expect(source1Node.name).toBe("source1.js");

      // source2 should be after source1 alphabetically
      const source2Node = fooNode.contents[1];
      const source3Node = fooNode.contents[0];
      expect(source2Node.name).toBe("b_source2.js");
      expect(source3Node.name).toBe("a_source3.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("sorts folders first", () => {
      const sources = [
        createSource({
          url: "http://example.com/a.js",
          id: "actor1"
        }),
        createSource({
          url: "http://example.com/b.js/b_source.js",
          id: "actor2"
        }),
        createSource({
          url: "http://example.com/c.js",
          id: "actor3"
        }),
        createSource({
          url: "http://example.com",
          id: "actor4"
        }),
        createSource({
          url: "http://example.com/d/d_source.js",
          id: "actor5"
        }),
        createSource({
          url: "http://example.com/b2",
          id: "actor6"
        })
      ];

      const _tree = createDirectoryNode("root", "", []);
      sources.forEach(source =>
        addToTree(_tree, source, "http://example.com/")
      );
      const tree = sortEntireTree(_tree);
      const domain = tree.contents[0];

      const [
        indexNode,
        bFolderNode,
        dFolderNode,
        aFileNode,
        b2FileNode,
        cFileNode
      ] = domain.contents;

      expect(formatTree(tree)).toMatchSnapshot();
      expect(indexNode.name).toBe("(index)");
      expect(bFolderNode.name).toBe("b.js");
      expect(bFolderNode.contents).toHaveLength(1);
      expect(bFolderNode.contents[0].name).toBe("b_source.js");

      expect(b2FileNode.name).toBe("b2");

      expect(dFolderNode.name).toBe("d");
      expect(dFolderNode.contents).toHaveLength(1);
      expect(dFolderNode.contents[0].name).toBe("d_source.js");

      expect(aFileNode.name).toBe("a.js");

      expect(cFileNode.name).toBe("c.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("puts folder at the top of the sort", () => {
      const sources = [
        createSource({
          url: "http://example.com/folder/a.js",
          id: "actor1"
        }),
        createSource({
          url: "http://example.com/folder/b/b.js",
          id: "actor2"
        }),
        createSource({
          url: "http://example.com/folder/c/",
          id: "actor3"
        })
      ];

      const _tree = createDirectoryNode("root", "", []);
      sources.forEach(source =>
        addToTree(_tree, source, "http://example.com/")
      );
      const tree = sortEntireTree(_tree);
      const [
        bFolderNode,
        cFolderNode,
        aFileNode
      ] = tree.contents[0].contents[0].contents;

      expect(bFolderNode.name).toBe("b");
      expect(bFolderNode.contents).toHaveLength(1);
      expect(bFolderNode.contents[0].name).toBe("b.js");

      expect(cFolderNode.name).toBe("c");
      expect(cFolderNode.contents).toHaveLength(1);
      expect(cFolderNode.contents[0].name).toBe("(index)");

      expect(aFileNode.name).toBe("a.js");
      expect(formatTree(tree)).toMatchSnapshot();
    });

    it("puts root debugee url at the top of the sort", () => {
      const sources = [
        createSource({
          url: "http://api.example.com/a.js",
          id: "actor1"
        }),
        createSource({
          url: "http://example.com/b.js",
          id: "actor2"
        }),
        createSource({
          url: "http://demo.com/c.js",
          id: "actor3"
        })
      ];

      const rootA = "http://example.com/path/to/file.html";
      const rootB = "https://www.demo.com/index.html";
      const _treeA = createDirectoryNode("root", "", []);
      const _treeB = createDirectoryNode("root", "", []);
      sources.forEach(source => {
        addToTree(_treeA, source, rootA);
        addToTree(_treeB, source, rootB);
      });
      const treeA = sortEntireTree(_treeA, rootA);
      const treeB = sortEntireTree(_treeB, rootB);

      expect(treeA.contents[0].contents[0].name).toBe("b.js");
      expect(treeA.contents[1].contents[0].name).toBe("a.js");

      expect(treeB.contents[0].contents[0].name).toBe("c.js");
      expect(treeB.contents[1].contents[0].name).toBe("a.js");
      expect(formatTree(treeA)).toMatchSnapshot();
      expect(formatTree(treeB)).toMatchSnapshot();
    });
  });
});
