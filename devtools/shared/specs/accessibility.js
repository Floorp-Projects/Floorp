/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const protocol = require("devtools/shared/protocol");
const { Arg, generateActorSpec, RetVal, types } = protocol;

types.addActorType("accessible");

/**
 * Accessible with children listed in the ancestry structure calculated by the
 * walker.
 */
types.addDictType("accessibleWithChildren", {
  // Accessible
  accessible: "accessible",
  // Accessible's children
  children: "array:accessible"
});

const accessibleSpec = generateActorSpec({
  typeName: "accessible",

  events: {
    "actions-change": {
      type: "actionsChange",
      actions: Arg(0, "array:string")
    },
    "name-change": {
      type: "nameChange",
      name: Arg(0, "string"),
      parent: Arg(1, "nullable:accessible"),
      walker: Arg(2, "nullable:accessiblewalker")
    },
    "value-change": {
      type: "valueChange",
      value: Arg(0, "string")
    },
    "description-change": {
      type: "descriptionChange",
      description: Arg(0, "string")
    },
    "states-change": {
      type: "statesChange",
      states: Arg(0, "array:string")
    },
    "attributes-change": {
      type: "attributesChange",
      attributes: Arg(0, "json")
    },
    "shortcut-change": {
      type: "shortcutChange",
      shortcut: Arg(0, "string")
    },
    "reorder": {
      type: "reorder",
      childCount: Arg(0, "number"),
      walker: Arg(1, "nullable:accessiblewalker")
    },
    "text-change": {
      type: "textChange",
      walker: Arg(0, "nullable:accessiblewalker")
    },
    "index-in-parent-change": {
      type: "indexInParentChange",
      indexInParent: Arg(0, "number")
    }
  },

  methods: {
    children: {
      request: {},
      response: {
        children: RetVal("array:accessible")
      }
    }
  }
});

const accessibleWalkerSpec = generateActorSpec({
  typeName: "accessiblewalker",

  events: {
    "accessible-destroy": {
      type: "accessibleDestroy",
      accessible: Arg(0, "accessible")
    },
    "document-ready": {
      type: "documentReady",
    },
    "picker-accessible-picked": {
      type: "pickerAccessiblePicked",
      accessible: Arg(0, "nullable:accessible")
    },
    "picker-accessible-previewed": {
      type: "pickerAccessiblePreviewed",
      accessible: Arg(0, "nullable:accessible")
    },
    "picker-accessible-hovered": {
      type: "pickerAccessibleHovered",
      accessible: Arg(0, "nullable:accessible")
    },
    "picker-accessible-canceled": {
      type: "pickerAccessibleCanceled"
    }
  },

  methods: {
    children: {
      request: {},
      response: {
        children: RetVal("array:accessible")
      }
    },
    getAccessibleFor: {
      request: { node: Arg(0, "domnode") },
      response: {
        accessible: RetVal("accessible")
      }
    },
    getAncestry: {
      request: { accessible: Arg(0, "accessible") },
      response: {
        ancestry: RetVal("array:accessibleWithChildren")
      }
    },
    highlightAccessible: {
      request: {
        accessible: Arg(0, "accessible"),
        options: Arg(1, "nullable:json")
      },
      response: {
        value: RetVal("nullable:boolean")
      }
    },
    unhighlight: {
      request: {}
    },
    pick: {},
    pickAndFocus: {},
    cancelPick: {}
  }
});

const accessibilitySpec = generateActorSpec({
  typeName: "accessibility",

  events: {
    "init": {
      type: "init"
    },
    "shutdown": {
      type: "shutdown"
    },
    "can-be-disabled-change": {
      type: "canBeDisabledChange",
      canBeDisabled: Arg(0, "boolean")
    },
    "can-be-enabled-change": {
      type: "canBeEnabledChange",
      canBeEnabled: Arg(0, "boolean")
    }
  },

  methods: {
    bootstrap: {
      request: {},
      response: {
        state: RetVal("json")
      }
    },
    getWalker: {
      request: {},
      response: {
        walker: RetVal("accessiblewalker")
      }
    },
    enable: {
      request: {},
      response: {}
    },
    disable: {
      request: {},
      response: {}
    }
  }
});

exports.accessibleSpec = accessibleSpec;
exports.accessibleWalkerSpec = accessibleWalkerSpec;
exports.accessibilitySpec = accessibilitySpec;
