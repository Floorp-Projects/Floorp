/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Generates a generalized profile with some samples.
 */
exports.synthesizeProfile = () => {
  const {
    CATEGORY_INDEX,
  } = require("devtools/client/performance/modules/categories");
  const RecordingUtils = require("devtools/shared/performance/recording-utils");

  return RecordingUtils.deflateProfile({
    meta: { version: 2 },
    threads: [
      {
        samples: [
          {
            time: 1,
            frames: [
              { category: CATEGORY_INDEX("other"), location: "(root)" },
              {
                category: CATEGORY_INDEX("js"),
                location: "A (http://foo/bar/baz:12:9)",
              },
              {
                category: CATEGORY_INDEX("layout"),
                location: "B InterruptibleLayout",
              },
              {
                category: CATEGORY_INDEX("js"),
                location: "C (http://foo/bar/baz:56)",
              },
            ],
          },
          {
            time: 1 + 1,
            frames: [
              { category: CATEGORY_INDEX("other"), location: "(root)" },
              {
                category: CATEGORY_INDEX("js"),
                location: "A (http://foo/bar/baz:12:9)",
              },
              {
                category: CATEGORY_INDEX("layout"),
                location: "B InterruptibleLayout",
              },
              {
                category: CATEGORY_INDEX("gc"),
                location: "D INTER_SLICE_GC",
              },
            ],
          },
          {
            time: 1 + 1 + 2,
            frames: [
              { category: CATEGORY_INDEX("other"), location: "(root)" },
              {
                category: CATEGORY_INDEX("js"),
                location: "A (http://foo/bar/baz:12:9)",
              },
              {
                category: CATEGORY_INDEX("layout"),
                location: "B InterruptibleLayout",
              },
              {
                category: CATEGORY_INDEX("gc"),
                location: "D INTER_SLICE_GC",
              },
            ],
          },
          {
            time: 1 + 1 + 2 + 3,
            frames: [
              { category: CATEGORY_INDEX("other"), location: "(root)" },
              {
                category: CATEGORY_INDEX("js"),
                location: "A (http://foo/bar/baz:12:9)",
              },
              {
                category: CATEGORY_INDEX("gc"),
                location: "E",
              },
              {
                category: CATEGORY_INDEX("network"),
                location: "F",
              },
            ],
          },
        ],
      },
    ],
  });
};

/**
 * Generates a simple implementation for a tree class.
 */
exports.synthesizeCustomTreeClass = () => {
  const {
    AbstractTreeItem,
  } = require("resource://devtools/client/shared/widgets/AbstractTreeItem.jsm");
  const { extend } = require("devtools/shared/extend");

  function MyCustomTreeItem(dataSrc, properties) {
    AbstractTreeItem.call(this, properties);
    this.itemDataSrc = dataSrc;
  }

  MyCustomTreeItem.prototype = extend(AbstractTreeItem.prototype, {
    _displaySelf: function(document, arrowNode) {
      const node = document.createXULElement("hbox");
      node.style.marginInlineStart = this.level * 10 + "px";
      node.appendChild(arrowNode);
      node.appendChild(document.createTextNode(this.itemDataSrc.label));
      return node;
    },

    _populateSelf: function(children) {
      for (const childDataSrc of this.itemDataSrc.children) {
        children.push(
          new MyCustomTreeItem(childDataSrc, {
            parent: this,
            level: this.level + 1,
          })
        );
      }
    },
  });

  const myDataSrc = {
    label: "root",
    children: [
      {
        label: "foo",
        children: [],
      },
      {
        label: "bar",
        children: [
          {
            label: "baz",
            children: [],
          },
        ],
      },
    ],
  };

  return { MyCustomTreeItem, myDataSrc };
};
