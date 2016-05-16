/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */
"use strict";

/**
 * Generates a generalized profile with some samples.
 */
exports.synthesizeProfile = () => {
  const { CATEGORY_MASK } = require("devtools/client/performance/modules/categories");
  const RecordingUtils = require("devtools/shared/performance/recording-utils");

  return RecordingUtils.deflateProfile({
    meta: { version: 2 },
    threads: [{
      samples: [{
        time: 1,
        frames: [
          { category: CATEGORY_MASK("other"),  location: "(root)" },
          { category: CATEGORY_MASK("other"),  location: "A (http://foo/bar/baz:12)" },
          { category: CATEGORY_MASK("css"),    location: "B (http://foo/bar/baz:34)" },
          { category: CATEGORY_MASK("js"),     location: "C (http://foo/bar/baz:56)" }
        ]
      }, {
        time: 1 + 1,
        frames: [
          { category: CATEGORY_MASK("other"),  location: "(root)" },
          { category: CATEGORY_MASK("other"),  location: "A (http://foo/bar/baz:12)" },
          { category: CATEGORY_MASK("css"),    location: "B (http://foo/bar/baz:34)" },
          { category: CATEGORY_MASK("gc", 1),  location: "D (http://foo/bar/baz:78:9)" }
        ]
      }, {
        time: 1 + 1 + 2,
        frames: [
          { category: CATEGORY_MASK("other"),  location: "(root)" },
          { category: CATEGORY_MASK("other"),  location: "A (http://foo/bar/baz:12)" },
          { category: CATEGORY_MASK("css"),    location: "B (http://foo/bar/baz:34)" },
          { category: CATEGORY_MASK("gc", 1),  location: "D (http://foo/bar/baz:78:9)" }
        ]
      }, {
        time: 1 + 1 + 2 + 3,
        frames: [
          { category: CATEGORY_MASK("other"),   location: "(root)" },
          { category: CATEGORY_MASK("other"),   location: "A (http://foo/bar/baz:12)" },
          { category: CATEGORY_MASK("gc", 2),   location: "E (http://foo/bar/baz:90)" },
          { category: CATEGORY_MASK("network"), location: "F (http://foo/bar/baz:99)" }
        ]
      }]
    }]
  });
};

/**
 * Generates a simple implementation for a tree class.
 */
exports.synthesizeCustomTreeClass = () => {
  const { Cu } = require("chrome");
  const { AbstractTreeItem } = Cu.import("resource://devtools/client/shared/widgets/AbstractTreeItem.jsm", {});
  const { Heritage } = require("devtools/client/shared/widgets/view-helpers");

  function MyCustomTreeItem(dataSrc, properties) {
    AbstractTreeItem.call(this, properties);
    this.itemDataSrc = dataSrc;
  }

  MyCustomTreeItem.prototype = Heritage.extend(AbstractTreeItem.prototype, {
    _displaySelf: function(document, arrowNode) {
      let node = document.createElement("hbox");
      node.marginInlineStart = (this.level * 10) + "px";
      node.appendChild(arrowNode);
      node.appendChild(document.createTextNode(this.itemDataSrc.label));
      return node;
    },

    _populateSelf: function(children) {
      for (let childDataSrc of this.itemDataSrc.children) {
        children.push(new MyCustomTreeItem(childDataSrc, {
          parent: this,
          level: this.level + 1
        }));
      }
    }
  });

  const myDataSrc = {
    label: "root",
    children: [{
      label: "foo",
      children: []
    }, {
      label: "bar",
      children: [{
        label: "baz",
        children: []
      }]
    }]
  };

  return { MyCustomTreeItem, myDataSrc };
};
