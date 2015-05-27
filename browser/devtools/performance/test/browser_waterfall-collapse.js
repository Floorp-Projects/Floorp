/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Tests if the waterfall collapsing logic works properly.
 */

function test() {
  const WaterfallUtils = devtools.require("devtools/performance/waterfall-utils");

  let rootMarkerNode = WaterfallUtils.makeEmptyMarkerNode("(root)");

  WaterfallUtils.collapseMarkersIntoNode({
    markerNode: rootMarkerNode,
    markersList: gTestMarkers
  });

  is(rootMarkerNode.toSource(), gExpectedOutput.toSource(),
    "The markers didn't collapse properly.");

  finish();
}

const gTestMarkers = [
// Test collapsing Style markers
{
  start: 1,
  end: 2,
  name: "Styles"
},
{
  start: 3,
  end: 4,
  name: "Styles"
},
// Test collapsing Reflow markers
{
  start: 5,
  end: 6,
  name: "Reflow"
},
{
  start: 7,
  end: 8,
  name: "Reflow"
},
// Test collapsing Paint markers
{
  start: 9,
  end: 10,
  name: "Paint"
}, {
  start: 11,
  end: 12,
  name: "Paint"
},
// Test standalone DOMEvent markers followed by a different marker
{
  start: 13,
  end: 14,
  name: "DOMEvent",
  eventPhase: 1,
  type: "foo1"
},
{
  start: 15,
  end: 16,
  name: "TimeStamp"
},
// Test a DOMEvent marker followed by a Javascript marker.
{
  start: 17,
  end: 18,
  name: "DOMEvent",
  eventPhase: 2,
  type: "foo2"
}, {
  start: 19,
  end: 20,
  name: "Javascript",
  stack: 1,
  endStack: 2
},
// Test another DOMEvent marker followed by a Javascript marker.
{
  start: 21,
  end: 22,
  name: "DOMEvent",
  eventPhase: 3,
  type: "foo3"
}, {
  start: 23,
  end: 24,
  name: "Javascript",
  stack: 3,
  endStack: 4
},
// Test a DOMEvent marker followed by multiple Javascript markers.
{
  start: 25,
  end: 26,
  name: "DOMEvent",
  eventPhase: 4,
  type: "foo4"
}, {
  start: 27,
  end: 28,
  name: "Javascript",
  stack: 5,
  endStack: 6
}, {
  start: 29,
  end: 30,
  name: "Javascript",
  stack: 7,
  endStack: 8
}, {
  start: 31,
  end: 32,
  name: "Javascript",
  stack: 9,
  endStack: 10
},
// Test multiple DOMEvent markers followed by multiple Javascript markers.
{
  start: 33,
  end: 34,
  name: "DOMEvent",
  eventPhase: 5,
  type: "foo5"
}, {
  start: 35,
  end: 36,
  name: "DOMEvent",
  eventPhase: 6,
  type: "foo6"
}, {
  start: 37,
  end: 38,
  name: "DOMEvent",
  eventPhase: 7,
  type: "foo6"
}, {
  start: 39,
  end: 40,
  name: "Javascript",
  stack: 11,
  endStack: 12
}, {
  start: 41,
  end: 42,
  name: "Javascript",
  stack: 13,
  endStack: 14
}, {
  start: 43,
  end: 44,
  name: "Javascript",
  stack: 15,
  endStack: 16
},
// Test a lonely marker at the end.
{
  start: 45,
  end: 46,
  name: "GarbageCollection"
}
];

const gExpectedOutput = {
  name: "(root)",
  start: (void 0),
  end: (void 0),
  submarkers: [{
    name: "Styles",
    start: 1,
    end: 4,
    submarkers: [{
      start: 1,
      end: 2,
      name: "Styles"
    }, {
      start: 3,
      end: 4,
      name: "Styles"
    }]
  }, {
    name: "Reflow",
    start: 5,
    end: 8,
    submarkers: [{
      start: 5,
      end: 6,
      name: "Reflow"
    }, {
      start: 7,
      end: 8,
      name: "Reflow"
    }]
  }, {
    name: "Paint",
    start: 9,
    end: 12,
    submarkers: [{
      start: 9,
      end: 10,
      name: "Paint"
    }, {
      start: 11,
      end: 12,
      name: "Paint"
    }]
  }, {
    start: 13,
    end: 14,
    name: "DOMEvent",
    eventPhase: 1,
    type: "foo1"
  }, {
    start: 15,
    end: 16,
    name: "TimeStamp"
  }, {
    name: "meta::DOMEvent+JS",
    start: 17,
    end: 20,
    submarkers: [{
      start: 17,
      end: 18,
      name: "DOMEvent",
      eventPhase: 2,
      type: "foo2"
    }, {
      start: 19,
      end: 20,
      name: "Javascript",
      stack: 1,
      endStack: 2
    }],
    type: "foo2",
    eventPhase: 2,
    stack: 1,
    endStack: 2
  }, {
    name: "meta::DOMEvent+JS",
    start: 21,
    end: 24,
    submarkers: [{
      start: 21,
      end: 22,
      name: "DOMEvent",
      eventPhase: 3,
      type: "foo3"
    }, {
      start: 23,
      end: 24,
      name: "Javascript",
      stack: 3,
      endStack: 4
    }],
    type: "foo3",
    eventPhase: 3,
    stack: 3,
    endStack: 4
  }, {
    name: "meta::DOMEvent+JS",
    start: 25,
    end: 28,
    submarkers: [{
      start: 25,
      end: 26,
      name: "DOMEvent",
      eventPhase: 4,
      type: "foo4"
    }, {
      start: 27,
      end: 28,
      name: "Javascript",
      stack: 5,
      endStack: 6
    }],
    type: "foo4",
    eventPhase: 4,
    stack: 5,
    endStack: 6
  }, {
    name: "Javascript",
    start: 29,
    end: 32,
    submarkers: [{
      start: 29,
      end: 30,
      name: "Javascript",
      stack: 7,
      endStack: 8
    }, {
      start: 31,
      end: 32,
      name: "Javascript",
      stack: 9,
      endStack: 10
    }]
  }, {
    start: 33,
    end: 34,
    name: "DOMEvent",
    eventPhase: 5,
    type: "foo5"
  }, {
    start: 35,
    end: 36,
    name: "DOMEvent",
    eventPhase: 6,
    type: "foo6"
  }, {
    name: "meta::DOMEvent+JS",
    start: 37,
    end: 40,
    submarkers: [{
      start: 37,
      end: 38,
      name: "DOMEvent",
      eventPhase: 7,
      type: "foo6"
    }, {
      start: 39,
      end: 40,
      name: "Javascript",
      stack: 11,
      endStack: 12
    }],
    type: "foo6",
    eventPhase: 7,
    stack: 11,
    endStack: 12
  }, {
    name: "Javascript",
    start: 41,
    end: 44,
    submarkers: [{
      start: 41,
      end: 42,
      name: "Javascript",
      stack: 13,
      endStack: 14
    }, {
      start: 43,
      end: 44,
      name: "Javascript",
      stack: 15,
      endStack: 16
    }]
  }, {
    start: 45,
    end: 46,
    name: "GarbageCollection"
  }]
};
