const Ci = Components.interfaces;
const Cu = Components.utils;

Cu.import('resource://gre/modules/XPCOMUtils.jsm');

this.EXPORTED_SYMBOLS = ['Roles', 'Events', 'Relations', 'Filters'];

function ConstantsMap (aObject, aPrefix) {
  let offset = aPrefix.length;
  for (var name in aObject) {
    if (name.indexOf(aPrefix) === 0) {
      this[name.slice(offset)] = aObject[name];
    }
  }
}

XPCOMUtils.defineLazyGetter(
  this, 'Roles',
  function() {
    return new ConstantsMap(Ci.nsIAccessibleRole, 'ROLE_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Events',
  function() {
    return new ConstantsMap(Ci.nsIAccessibleEvent, 'EVENT_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Relations',
  function() {
    return new ConstantsMap(Ci.nsIAccessibleRelation, 'RELATION_');
  });

XPCOMUtils.defineLazyGetter(
  this, 'Filters',
  function() {
    return new ConstantsMap(Ci.nsIAccessibleTraversalRule, 'FILTER_');
  });
