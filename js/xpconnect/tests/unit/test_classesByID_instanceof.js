function testActual(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  do_check_eq(simpleURI instanceof SimpleURIClassByID, true);
}

function testInherited(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  var inherited = Object.create(simpleURI);

  do_check_eq(inherited instanceof SimpleURIClassByID, true);
}

function testInheritedCrossGlobal(SimpleURIClassByID)
{
  var simpleURI =
    Components.classes["@mozilla.org/network/simple-uri;1"].createInstance();

  var sb = new Components.utils.Sandbox(this, { wantComponents: true });
  var inheritedCross = sb.Object.create(simpleURI);

  do_check_eq(inheritedCross instanceof SimpleURIClassByID, true);
}

function run_test() {
  var SimpleURIClassByID = Components.classesByID["{e0da1d70-2f7b-11d3-8cd0-0060b0fc14a3}"];

  testActual(SimpleURIClassByID);
  testInherited(SimpleURIClassByID);
  testInheritedCrossGlobal(SimpleURIClassByID);
}
