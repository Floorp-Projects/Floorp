/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types,
} = require("devtools/shared/protocol");

types.addDictType("dommutation", {});

types.addDictType("searchresult", {
  list: "domnodelist",
  // Right now there is isn't anything required for metadata,
  // but it's json so it can be extended with extra data.
  metadata: "array:json",
});

// Some common request/response templates for the dom walker

var nodeArrayMethod = {
  request: {
    node: Arg(0, "domnode"),
    maxNodes: Option(1),
    center: Option(1, "domnode"),
    start: Option(1, "domnode"),
    whatToShow: Option(1),
  },
  response: RetVal(
    types.addDictType("domtraversalarray", {
      nodes: "array:domnode",
    })
  ),
};

var traversalMethod = {
  request: {
    node: Arg(0, "domnode"),
    whatToShow: Option(1),
  },
  response: {
    node: RetVal("nullable:domnode"),
  },
};

const walkerSpec = generateActorSpec({
  typeName: "domwalker",

  events: {
    "new-mutations": {
      type: "newMutations",
    },
    "root-available": {
      type: "root-available",
      node: Arg(0, "nullable:domnode"),
    },
    "root-destroyed": {
      type: "root-destroyed",
      node: Arg(0, "nullable:domnode"),
    },
    "picker-node-picked": {
      type: "pickerNodePicked",
      node: Arg(0, "disconnectedNode"),
    },
    "picker-node-previewed": {
      type: "pickerNodePreviewed",
      node: Arg(0, "disconnectedNode"),
    },
    "picker-node-hovered": {
      type: "pickerNodeHovered",
      node: Arg(0, "disconnectedNode"),
    },
    "picker-node-canceled": {
      type: "pickerNodeCanceled",
    },
    "display-change": {
      type: "display-change",
      nodes: Arg(0, "array:domnode"),
    },
    "scrollable-change": {
      type: "scrollable-change",
      nodes: Arg(0, "array:domnode"),
    },
    // The walker actor emits a useful "resize" event to its front to let
    // clients know when the browser window gets resized. This may be useful
    // for refreshing a DOM node's styles for example, since those may depend on
    // media-queries.
    resize: {
      type: "resize",
    },
  },

  methods: {
    release: {
      release: true,
    },
    document: {
      request: { node: Arg(0, "nullable:domnode") },
      response: { node: RetVal("domnode") },
    },
    documentElement: {
      request: { node: Arg(0, "nullable:domnode") },
      response: { node: RetVal("domnode") },
    },
    retainNode: {
      request: { node: Arg(0, "domnode") },
      response: {},
    },
    unretainNode: {
      request: { node: Arg(0, "domnode") },
      response: {},
    },
    releaseNode: {
      request: {
        node: Arg(0, "domnode"),
        force: Option(1),
      },
    },
    children: nodeArrayMethod,
    nextSibling: traversalMethod,
    previousSibling: traversalMethod,
    findInspectingNode: {
      request: {},
      response: RetVal("disconnectedNode"),
    },
    querySelector: {
      request: {
        node: Arg(0, "domnode"),
        selector: Arg(1),
      },
      response: RetVal("disconnectedNode"),
    },
    querySelectorAll: {
      request: {
        node: Arg(0, "domnode"),
        selector: Arg(1),
      },
      response: {
        list: RetVal("domnodelist"),
      },
    },
    multiFrameQuerySelectorAll: {
      request: {
        selector: Arg(0),
      },
      response: {
        list: RetVal("domnodelist"),
      },
    },
    search: {
      request: {
        query: Arg(0),
      },
      response: {
        list: RetVal("searchresult"),
      },
    },
    getSuggestionsForQuery: {
      request: {
        query: Arg(0),
        completing: Arg(1),
        selectorState: Arg(2),
      },
      response: {
        list: RetVal("array:array:string"),
      },
    },
    addPseudoClassLock: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClass: Arg(1),
        parents: Option(2),
        enabled: Option(2, "boolean"),
      },
      response: {},
    },
    hideNode: {
      request: { node: Arg(0, "domnode") },
    },
    unhideNode: {
      request: { node: Arg(0, "domnode") },
    },
    removePseudoClassLock: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClass: Arg(1),
        parents: Option(2),
      },
      response: {},
    },
    clearPseudoClassLocks: {
      request: {
        node: Arg(0, "nullable:domnode"),
      },
      response: {},
    },
    innerHTML: {
      request: {
        node: Arg(0, "domnode"),
      },
      response: {
        value: RetVal("longstring"),
      },
    },
    setInnerHTML: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
      },
      response: {},
    },
    outerHTML: {
      request: {
        node: Arg(0, "domnode"),
      },
      response: {
        value: RetVal("longstring"),
      },
    },
    setOuterHTML: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
      },
      response: {},
    },
    insertAdjacentHTML: {
      request: {
        node: Arg(0, "domnode"),
        position: Arg(1, "string"),
        value: Arg(2, "string"),
      },
      response: RetVal("disconnectedNodeArray"),
    },
    duplicateNode: {
      request: {
        node: Arg(0, "domnode"),
      },
      response: {},
    },
    removeNode: {
      request: {
        node: Arg(0, "domnode"),
      },
      response: {
        nextSibling: RetVal("nullable:domnode"),
      },
    },
    removeNodes: {
      request: {
        node: Arg(0, "array:domnode"),
      },
      response: {},
    },
    insertBefore: {
      request: {
        node: Arg(0, "domnode"),
        parent: Arg(1, "domnode"),
        sibling: Arg(2, "nullable:domnode"),
      },
      response: {},
    },
    editTagName: {
      request: {
        node: Arg(0, "domnode"),
        tagName: Arg(1, "string"),
      },
      response: {},
    },
    getMutations: {
      request: {
        cleanup: Option(0),
      },
      response: {
        mutations: RetVal("array:dommutation"),
      },
    },
    isInDOMTree: {
      request: { node: Arg(0, "domnode") },
      response: { attached: RetVal("boolean") },
    },
    getNodeActorFromObjectActor: {
      request: {
        objectActorID: Arg(0, "string"),
      },
      response: {
        nodeFront: RetVal("nullable:disconnectedNode"),
      },
    },
    getNodeActorFromWindowID: {
      request: {
        windowID: Arg(0, "string"),
      },
      response: {
        nodeFront: RetVal("nullable:disconnectedNode"),
      },
    },
    getNodeActorFromContentDomReference: {
      request: {
        contentDomReference: Arg(0, "json"),
      },
      response: {
        nodeFront: RetVal("nullable:disconnectedNode"),
      },
    },
    getStyleSheetOwnerNode: {
      request: {
        styleSheetActorID: Arg(0, "string"),
      },
      response: {
        ownerNode: RetVal("nullable:disconnectedNode"),
      },
    },
    getNodeFromActor: {
      request: {
        actorID: Arg(0, "string"),
        path: Arg(1, "array:string"),
      },
      response: {
        node: RetVal("nullable:disconnectedNode"),
      },
    },
    getLayoutInspector: {
      request: {},
      response: {
        actor: RetVal("layout"),
      },
    },
    getParentGridNode: {
      request: {
        node: Arg(0, "nullable:domnode"),
      },
      response: {
        node: RetVal("nullable:domnode"),
      },
    },
    getOffsetParent: {
      request: {
        node: Arg(0, "nullable:domnode"),
      },
      response: {
        node: RetVal("nullable:domnode"),
      },
    },
    hasAccessibilityProperties: {
      request: {
        node: Arg(0, "nullable:domnode"),
      },
      response: {
        value: RetVal("boolean"),
      },
    },
    setMutationBreakpoints: {
      request: {
        node: Arg(0, "nullable:domnode"),
        subtree: Option(1, "nullable:boolean"),
        removal: Option(1, "nullable:boolean"),
        attribute: Option(1, "nullable:boolean"),
      },
      response: {},
    },
    getEmbedderElement: {
      request: {
        browsingContextID: Arg(0, "string"),
      },
      response: {
        nodeFront: RetVal("disconnectedNode"),
      },
    },
    watchRootNode: {
      request: {},
      response: {},
    },
    unwatchRootNode: {
      request: {},
      oneway: true,
    },
  },
});

exports.walkerSpec = walkerSpec;
