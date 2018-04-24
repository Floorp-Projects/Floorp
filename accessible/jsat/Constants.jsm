ChromeUtils.import("resource://gre/modules/XPCOMUtils.jsm");

var EXPORTED_SYMBOLS = ["Roles", "Events", "Relations",
                        "Filters", "States", "Prefilters", "AndroidEvents"];

const AndroidEvents = {
  ANDROID_VIEW_CLICKED: 0x01,
  ANDROID_VIEW_LONG_CLICKED: 0x02,
  ANDROID_VIEW_SELECTED: 0x04,
  ANDROID_VIEW_FOCUSED: 0x08,
  ANDROID_VIEW_TEXT_CHANGED: 0x10,
  ANDROID_WINDOW_STATE_CHANGED: 0x20,
  ANDROID_VIEW_HOVER_ENTER: 0x80,
  ANDROID_VIEW_HOVER_EXIT: 0x100,
  ANDROID_VIEW_SCROLLED: 0x1000,
  ANDROID_VIEW_TEXT_SELECTION_CHANGED: 0x2000,
  ANDROID_ANNOUNCEMENT: 0x4000,
  ANDROID_VIEW_ACCESSIBILITY_FOCUSED: 0x8000,
  ANDROID_VIEW_TEXT_TRAVERSED_AT_MOVEMENT_GRANULARITY: 0x20000,
};

function ConstantsMap(aObject, aPrefix, aMap = {}, aModifier = null) {
  let offset = aPrefix.length;
  for (var name in aObject) {
    if (name.indexOf(aPrefix) === 0) {
      aMap[name.slice(offset)] = aModifier ?
        aModifier(aObject[name]) : aObject[name];
    }
  }

  return aMap;
}

XPCOMUtils.defineLazyGetter(
  this, "Roles",
  function() {
    return ConstantsMap(Ci.nsIAccessibleRole, "ROLE_");
  });

XPCOMUtils.defineLazyGetter(
  this, "Events",
  function() {
    return ConstantsMap(Ci.nsIAccessibleEvent, "EVENT_");
  });

XPCOMUtils.defineLazyGetter(
  this, "Relations",
  function() {
    return ConstantsMap(Ci.nsIAccessibleRelation, "RELATION_");
  });

XPCOMUtils.defineLazyGetter(
  this, "Prefilters",
  function() {
    return ConstantsMap(Ci.nsIAccessibleTraversalRule, "PREFILTER_");
  });

XPCOMUtils.defineLazyGetter(
  this, "Filters",
  function() {
    return ConstantsMap(Ci.nsIAccessibleTraversalRule, "FILTER_");
  });

XPCOMUtils.defineLazyGetter(
  this, "States",
  function() {
    let statesMap = ConstantsMap(Ci.nsIAccessibleStates, "STATE_", {},
                                 (val) => { return { base: val, extended: 0 }; });
    ConstantsMap(Ci.nsIAccessibleStates, "EXT_STATE_", statesMap,
                 (val) => { return { base: 0, extended: val }; });
    return statesMap;
  });
