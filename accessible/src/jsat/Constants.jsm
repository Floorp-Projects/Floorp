const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

this.EXPORTED_SYMBOLS = ['Roles', 'Events', 'Relations', 'Filters', 'States'];

function ConstantsMap (aObject, aPrefix, aMap = {}, aModifier = null) {
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
  this, 'Roles',
  function() {
    return ConstantsMap(Ci.nsIAccessibleRole, 'ROLE_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Events',
  function() {
    return ConstantsMap(Ci.nsIAccessibleEvent, 'EVENT_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Relations',
  function() {
    return ConstantsMap(Ci.nsIAccessibleRelation, 'RELATION_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Filters',
  function() {
    return ConstantsMap(Ci.nsIAccessibleTraversalRule, 'FILTER_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'States',
  function() {
    let statesMap = ConstantsMap(Ci.nsIAccessibleStates, 'STATE_', {},
                                 (val) => { return { base: val, extended: 0 }; });
    ConstantsMap(Ci.nsIAccessibleStates, 'EXT_STATE_', statesMap,
                 (val) => { return { base: 0, extended: val }; });
    return statesMap;
  });
