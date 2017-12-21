function testActual(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  Assert.equal(simpleURI instanceof SimpleURIClassByID, true);
}

function testInherited(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  var inherited = Object.create(simpleURI);

  Assert.equal(inherited instanceof SimpleURIClassByID, true);
}

function testInheritedCrossGlobal(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  var sb = new Components.utils.Sandbox(this, { wantComponents: true });
  var inheritedCross = sb.Object.create(simpleURI);

  Assert.equal(inheritedCross instanceof SimpleURIClassByID, true);
}

function testCrossGlobalArbitraryGetPrototype(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  var sb = new Components.utils.Sandbox(this, { wantComponents: true });
  var firstLevel = Object.create(simpleURI);

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
    void (thirdLevel instanceof SimpleURIClassByID);
  }
  catch (e)
  {
    threw = true;
    err = e;
  }

  Assert.equal(threw, true);
  Assert.equal(err, 42);

  obj.shouldThrow = false;

  Assert.equal(thirdLevel instanceof SimpleURIClassByID, true);
}

function run_test() {
  var SimpleURIClassByID = Components.classesByID["{e0da1d70-2f7b-11d3-8cd0-0060b0fc14a3}"];

  testActual(SimpleURIClassByID);
  testInherited(SimpleURIClassByID);
  testInheritedCrossGlobal(SimpleURIClassByID);
  testCrossGlobalArbitraryGetPrototype(SimpleURIClassByID);
}
