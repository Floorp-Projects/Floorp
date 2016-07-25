/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

const {
  Arg,
  Option,
  RetVal,
  generateActorSpec,
  types
} = require("devtools/shared/protocol");
const { nodeSpec } = require("devtools/shared/specs/node");
require("devtools/shared/specs/styles");
require("devtools/shared/specs/highlighters");

exports.nodeSpec = nodeSpec;

/**
 * Returned from any call that might return a node that isn't connected to root
 * by nodes the child has seen, such as querySelector.
 */
types.addDictType("disconnectedNode", {
  // The actual node to return
  node: "domnode",

  // Nodes that are needed to connect the node to a node the client has already
  // seen
  newParents: "array:domnode"
});

types.addDictType("disconnectedNodeArray", {
  // The actual node list to return
  nodes: "array:domnode",

  // Nodes that are needed to connect those nodes to the root.
  newParents: "array:domnode"
});

types.addDictType("dommutation", {});

types.addDictType("searchresult", {
  list: "domnodelist",
  // Right now there is isn't anything required for metadata,
  // but it's json so it can be extended with extra data.
  metadata: "array:json"
});

const nodeListSpec = generateActorSpec({
  typeName: "domnodelist",

  methods: {
    item: {
      request: { item: Arg(0) },
      response: RetVal("disconnectedNode")
    },
    items: {
      request: {
        start: Arg(0, "nullable:number"),
        end: Arg(1, "nullable:number")
      },
      response: RetVal("disconnectedNodeArray")
    },
    release: {
      release: true
    }
  }
});

exports.nodeListSpec = nodeListSpec;

// Some common request/response templates for the dom walker

var nodeArrayMethod = {
  request: {
    node: Arg(0, "domnode"),
    maxNodes: Option(1),
    center: Option(1, "domnode"),
    start: Option(1, "domnode"),
    whatToShow: Option(1)
  },
  response: RetVal(types.addDictType("domtraversalarray", {
    nodes: "array:domnode"
  }))
};

var traversalMethod = {
  request: {
    node: Arg(0, "domnode"),
    whatToShow: Option(1)
  },
  response: {
    node: RetVal("nullable:domnode")
  }
};

const walkerSpec = generateActorSpec({
  typeName: "domwalker",

  events: {
    "new-mutations": {
      type: "newMutations"
    },
    "picker-node-picked": {
      type: "pickerNodePicked",
      node: Arg(0, "disconnectedNode")
    },
    "picker-node-hovered": {
      type: "pickerNodeHovered",
      node: Arg(0, "disconnectedNode")
    },
    "picker-node-canceled": {
      type: "pickerNodeCanceled"
    },
    "highlighter-ready": {
      type: "highlighter-ready"
    },
    "highlighter-hide": {
      type: "highlighter-hide"
    },
    "display-change": {
      type: "display-change",
      nodes: Arg(0, "array:domnode")
    },
    // The walker actor emits a useful "resize" event to its front to let
    // clients know when the browser window gets resized. This may be useful
    // for refreshing a DOM node's styles for example, since those may depend on
    // media-queries.
    "resize": {
      type: "resize"
    }
  },

  methods: {
    release: {
      release: true
    },
    pick: {
      request: {},
      response: RetVal("disconnectedNode")
    },
    cancelPick: {},
    highlight: {
      request: {node: Arg(0, "nullable:domnode")}
    },
    document: {
      request: { node: Arg(0, "nullable:domnode") },
      response: { node: RetVal("domnode") },
    },
    documentElement: {
      request: { node: Arg(0, "nullable:domnode") },
      response: { node: RetVal("domnode") },
    },
    parents: {
      request: {
        node: Arg(0, "domnode"),
        sameDocument: Option(1),
        sameTypeRootTreeItem: Option(1)
      },
      response: {
        nodes: RetVal("array:domnode")
      },
    },
    retainNode: {
      request: { node: Arg(0, "domnode") },
      response: {}
    },
    unretainNode: {
      request: { node: Arg(0, "domnode") },
      response: {},
    },
    releaseNode: {
      request: {
        node: Arg(0, "domnode"),
        force: Option(1)
      }
    },
    children: nodeArrayMethod,
    siblings: nodeArrayMethod,
    nextSibling: traversalMethod,
    previousSibling: traversalMethod,
    findInspectingNode: {
      request: {},
      response: RetVal("disconnectedNode")
    },
    querySelector: {
      request: {
        node: Arg(0, "domnode"),
        selector: Arg(1)
      },
      response: RetVal("disconnectedNode")
    },
    querySelectorAll: {
      request: {
        node: Arg(0, "domnode"),
        selector: Arg(1)
      },
      response: {
        list: RetVal("domnodelist")
      }
    },
    multiFrameQuerySelectorAll: {
      request: {
        selector: Arg(0)
      },
      response: {
        list: RetVal("domnodelist")
      }
    },
    search: {
      request: {
        query: Arg(0),
      },
      response: {
        list: RetVal("searchresult"),
      }
    },
    getSuggestionsForQuery: {
      request: {
        query: Arg(0),
        completing: Arg(1),
        selectorState: Arg(2)
      },
      response: {
        list: RetVal("array:array:string")
      }
    },
    addPseudoClassLock: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClass: Arg(1),
        parents: Option(2)
      },
      response: {}
    },
    hideNode: {
      request: { node: Arg(0, "domnode") }
    },
    unhideNode: {
      request: { node: Arg(0, "domnode") }
    },
    removePseudoClassLock: {
      request: {
        node: Arg(0, "domnode"),
        pseudoClass: Arg(1),
        parents: Option(2)
      },
      response: {}
    },
    clearPseudoClassLocks: {
      request: {
        node: Arg(0, "nullable:domnode")
      },
      response: {}
    },
    innerHTML: {
      request: {
        node: Arg(0, "domnode")
      },
      response: {
        value: RetVal("longstring")
      }
    },
    setInnerHTML: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
      },
      response: {}
    },
    outerHTML: {
      request: {
        node: Arg(0, "domnode")
      },
      response: {
        value: RetVal("longstring")
      }
    },
    setOuterHTML: {
      request: {
        node: Arg(0, "domnode"),
        value: Arg(1, "string"),
      },
      response: {}
    },
    insertAdjacentHTML: {
      request: {
        node: Arg(0, "domnode"),
        position: Arg(1, "string"),
        value: Arg(2, "string")
      },
      response: RetVal("disconnectedNodeArray")
    },
    duplicateNode: {
      request: {
        node: Arg(0, "domnode")
      },
      response: {}
    },
    removeNode: {
      request: {
        node: Arg(0, "domnode")
      },
      response: {
        nextSibling: RetVal("nullable:domnode")
      }
    },
    removeNodes: {
      request: {
        node: Arg(0, "array:domnode")
      },
      response: {}
    },
    insertBefore: {
      request: {
        node: Arg(0, "domnode"),
        parent: Arg(1, "domnode"),
        sibling: Arg(2, "nullable:domnode")
      },
      response: {}
    },
    editTagName: {
      request: {
        node: Arg(0, "domnode"),
        tagName: Arg(1, "string")
      },
      response: {}
    },
    getMutations: {
      request: {
        cleanup: Option(0)
      },
      response: {
        mutations: RetVal("array:dommutation")
      }
    },
    isInDOMTree: {
      request: { node: Arg(0, "domnode") },
      response: { attached: RetVal("boolean") }
    },
    getNodeActorFromObjectActor: {
      request: {
        objectActorID: Arg(0, "string")
      },
      response: {
        nodeFront: RetVal("nullable:disconnectedNode")
      }
    },
    getStyleSheetOwnerNode: {
      request: {
        styleSheetActorID: Arg(0, "string")
      },
      response: {
        ownerNode: RetVal("nullable:disconnectedNode")
      }
    },
    getNodeFromActor: {
      request: {
        actorID: Arg(0, "string"),
        path: Arg(1, "array:string")
      },
      response: {
        node: RetVal("nullable:disconnectedNode")
      }
    }
  }
});

exports.walkerSpec = walkerSpec;

const inspectorSpec = generateActorSpec({
  typeName: "inspector",

  events: {
    "color-picked": {
      type: "colorPicked",
      color: Arg(0, "string")
    },
    "color-pick-canceled": {
      type: "colorPickCanceled"
    }
  },

  methods: {
    getWalker: {
      request: {
        options: Arg(0, "nullable:json")
      },
      response: {
        walker: RetVal("domwalker")
      }
    },
    getPageStyle: {
      request: {},
      response: {
        pageStyle: RetVal("pagestyle")
      }
    },
    getHighlighter: {
      request: {
        autohide: Arg(0, "boolean")
      },
      response: {
        highligter: RetVal("highlighter")
      }
    },
    getHighlighterByType: {
      request: {
        typeName: Arg(0)
      },
      response: {
        highlighter: RetVal("nullable:customhighlighter")
      }
    },
    getImageDataFromURL: {
      request: {url: Arg(0), maxDim: Arg(1, "nullable:number")},
      response: RetVal("imageData")
    },
    resolveRelativeURL: {
      request: {url: Arg(0, "string"), node: Arg(1, "nullable:domnode")},
      response: {value: RetVal("string")}
    },
    pickColorFromPage: {
      request: {options: Arg(0, "nullable:json")},
      response: {}
    },
    cancelPickColorFromPage: {
      request: {},
      response: {}
    }
  }
});

exports.inspectorSpec = inspectorSpec;
