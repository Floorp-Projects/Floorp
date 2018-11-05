ChromeUtils.import("resource://gre/modules/Services.jsm");

Services.prefs.setBoolPref("security.allow_eval_with_system_principal", true);
registerCleanupFunction(() => {
  Services.prefs.clearUserPref("security.allow_eval_with_system_principal");
});

function testActual(CSPContextClassByID)
{
  var cspContext =
    Cc["@mozilla.org/cspcontext;1"].createInstance();

  Assert.equal(cspContext instanceof CSPContextClassByID, true);
}

function testInherited(CSPContextClassByID)
{
  var cspContext =
    Cc["@mozilla.org/cspcontext;1"].createInstance();

  var inherited = Object.create(cspContext);

  Assert.equal(inherited instanceof CSPContextClassByID, true);
}

function testInheritedCrossGlobal(CSPContextClassByID)
{
  var cspContext =
    Cc["@mozilla.org/cspcontext;1"].createInstance();

  var sb = new Cu.Sandbox(this, { wantComponents: true });
  var inheritedCross = sb.Object.create(cspContext);

  Assert.equal(inheritedCross instanceof CSPContextClassByID, true);
}

function testCrossGlobalArbitraryGetPrototype(CSPContextClassByID)
{
  var cspContext =
    Cc["@mozilla.org/cspcontext;1"].createInstance();

  var sb = new Cu.Sandbox(this, { wantComponents: true });
  var firstLevel = Object.create(cspContext);

  var obj = { shouldThrow: false };
  var secondLevel =
    new sb.Proxy(Object.create(firstLevel),
                 {
                   getPrototypeOf: new sb.Function("obj", `return function(t) {
                     if (obj.shouldThrow)
                       throw 42;
                     return Reflect.getPrototypeOf(t);
                   };`)(obj)
                 });
  var thirdLevel = Object.create(secondLevel);

  obj.shouldThrow = true;

  var threw = false;
  var err;
  try
  {
    void (thirdLevel instanceof CSPContextClassByID);
  }
  catch (e)
  {
    threw = true;
    err = e;
  }

  Assert.equal(threw, true);
  Assert.equal(err, 42);

  obj.shouldThrow = false;

  Assert.equal(thirdLevel instanceof CSPContextClassByID, true);
}

function run_test() {
  var CSPContextClassByID = Components.classesByID["{09d9ed1a-e5d4-4004-bfe0-27ceb923d9ac}"];

  testActual(CSPContextClassByID);
  testInherited(CSPContextClassByID);
  testInheritedCrossGlobal(CSPContextClassByID);
  testCrossGlobalArbitraryGetPrototype(CSPContextClassByID);
}
