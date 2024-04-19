/* Any copyright is dedicated to the Public Domain.
https://creativecommons.org/publicdomain/zero/1.0/ */

add_task(function () {
  class TestClass {
    QueryInterface = ChromeUtils.generateQI([
      "nsIXPCTestInterfaceA",
      "nsIClassInfo",
    ]);

    interfaces = [Ci.nsIXPCTestInterfaceA, Ci.nsIClassInfo, Ci.nsISupports];
    contractID = "@mozilla.org/test/class;1";
    classDescription = "description";
    classID = Components.ID("{4da556d4-00fa-451a-a280-d2aec7c5f265}");
    flags = 0;

    name = "this is a test";
  }

  let instance = new TestClass();
  Assert.ok(instance, "can create an instance");
  Assert.ok(instance.QueryInterface(Ci.nsIClassInfo), "can QI to nsIClassInfo");

  let registrar = Components.manager.QueryInterface(Ci.nsIComponentRegistrar);
  registrar.registerFactory(
    instance.classID,
    instance.classDescription,
    instance.contractID,
    {
      createInstance(iid) {
        return instance.QueryInterface(iid);
      },
    }
  );
  Assert.ok(true, "successfully registered the factory");

  let otherInstance = Cc["@mozilla.org/test/class;1"].createInstance(
    Ci.nsIXPCTestInterfaceA
  );
  Assert.ok(otherInstance, "can create an instance via xpcom");
});
