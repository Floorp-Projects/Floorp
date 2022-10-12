"use strict";

const { types } = require("resource://devtools/shared/protocol.js");

function run_test() {
  types.addActorType("myActor1");
  types.addActorType("myActor2");
  types.addActorType("myActor3");

  types.addPolymorphicType("ptype1", ["myActor1", "myActor2"]);
  const ptype1 = types.getType("ptype1");
  Assert.equal(ptype1.name, "ptype1");
  Assert.equal(ptype1.category, "polymorphic");

  types.addPolymorphicType("ptype2", ["myActor1", "myActor2", "myActor3"]);
  const ptype2 = types.getType("ptype2");
  Assert.equal(ptype2.name, "ptype2");
  Assert.equal(ptype2.category, "polymorphic");

  // Polymorphic types only accept actor types
  try {
    types.addPolymorphicType("ptype", ["myActor1", "myActor4"]);
    Assert.ok(false, "getType should fail");
  } catch (ex) {
    Assert.equal(ex.toString(), "Error: Unknown type: myActor4");
  }
  try {
    types.addPolymorphicType("ptype", ["myActor1", "string"]);
    Assert.ok(false, "getType should fail");
  } catch (ex) {
    Assert.equal(
      ex.toString(),
      "Error: In polymorphic type 'myActor1,string', the type 'string' isn't an actor"
    );
  }
  try {
    types.addPolymorphicType("ptype", ["myActor1", "boolean"]);
    Assert.ok(false, "getType should fail");
  } catch (ex) {
    Assert.equal(
      ex.toString(),
      "Error: In polymorphic type 'myActor1,boolean', the type 'boolean' isn't an actor"
    );
  }

  // Polymorphic types are not compatible with array or nullables
  try {
    types.addPolymorphicType("ptype", ["array:myActor1", "myActor2"]);
    Assert.ok(false, "addType should fail");
  } catch (ex) {
    Assert.equal(
      ex.toString(),
      "Error: In polymorphic type 'array:myActor1,myActor2', the type 'array:myActor1' isn't an actor"
    );
  }
  try {
    types.addPolymorphicType("ptype", ["nullable:myActor1", "myActor2"]);
    Assert.ok(false, "addType should fail");
  } catch (ex) {
    Assert.equal(
      ex.toString(),
      "Error: In polymorphic type 'nullable:myActor1,myActor2', the type 'nullable:myActor1' isn't an actor"
    );
  }
}
