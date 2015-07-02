const { classes: Cc, interfaces: Ci, utils: Cu, results: Cr } = Components;

const ACTIVITY_GLUE_CID = Components.ID("{f4cfbe10-a106-4cd1-b04e-0d2a6aac138b}");
const SYS_MSG_GLUE_CID = Components.ID("{b0b6b9af-bc4e-4200-bffe-fb7691065ec9}");

const gRootUrl = "http://test/chrome/dom/activities/tests/mochi/";

function registerComponent(aObject, aDescription, aContract, aCid) {
  info("Registering " + aCid);

  var componentManager =
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.registerFactory(aCid, aDescription, aContract, aObject);

  // Keep the id on the object so we can unregister later.
  aObject.cid = aCid;
}

function unregisterComponent(aObject) {
  info("Unregistering " + aObject.cid);
  var componentManager =
    Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  componentManager.unregisterFactory(aObject.cid, aObject);
}

function cbError(aEvent) {
  ok(false, "Error callback invoked " +
            aEvent.target.error.name + " " + aEvent.target.error.message);
  finish();
}

function unexpectedSuccess(aMsg) {
  return function() {
    ok(false, "Should not have succeeded: " + aMsg);
    finish();
  }
}
