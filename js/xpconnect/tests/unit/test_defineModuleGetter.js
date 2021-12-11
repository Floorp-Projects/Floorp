"use strict";

function assertIsGetter(obj, prop) {
  let desc = Object.getOwnPropertyDescriptor(obj, prop);

  ok(desc, `Property ${prop} exists on object`);
  equal(typeof desc.get, "function", `Getter function exists for property ${prop}`);
  equal(typeof desc.set, "function", `Setter function exists for property ${prop}`);
  equal(desc.enumerable, true, `Property ${prop} is enumerable`);
  equal(desc.configurable, true, `Property ${prop} is configurable`);
}

function assertIsValue(obj, prop, value) {
  let desc = Object.getOwnPropertyDescriptor(obj, prop);

  ok(desc, `Property ${prop} exists on object`);

  ok("value" in desc, `${prop} is a data property`);
  equal(desc.value, value, `${prop} has the expected value`);

  equal(desc.enumerable, true, `Property ${prop} is enumerable`);
  equal(desc.configurable, true, `Property ${prop} is configurable`);
  equal(desc.writable, true, `Property ${prop} is writable`);
}

add_task(async function() {
  let temp = {};
  ChromeUtils.import("resource://gre/modules/Services.jsm", temp);

  let obj = {};
  let child = Object.create(obj);
  let sealed = Object.seal(Object.create(obj));


  // Test valid import

  ChromeUtils.defineModuleGetter(obj, "Services",
                                 "resource://gre/modules/Services.jsm");

  assertIsGetter(obj, "Services");
  equal(child.Services, temp.Services, "Getter works on descendent object");
  assertIsValue(child, "Services", temp.Services);
  assertIsGetter(obj, "Services");

  Assert.throws(() => sealed.Services, /Object is not extensible/,
                "Cannot access lazy getter from sealed object");
  Assert.throws(() => sealed.Services = null, /Object is not extensible/,
                "Cannot access lazy setter from sealed object");
  assertIsGetter(obj, "Services");

  equal(obj.Services, temp.Services, "Getter works on object");
  assertIsValue(obj, "Services", temp.Services);


  // Test overwriting via setter

  child = Object.create(obj);

  ChromeUtils.defineModuleGetter(obj, "Services",
                                 "resource://gre/modules/Services.jsm");

  assertIsGetter(obj, "Services");

  child.Services = "foo";
  assertIsValue(child, "Services", "foo");
  assertIsGetter(obj, "Services");

  obj.Services = "foo";
  assertIsValue(obj, "Services", "foo");


  // Test import missing property

  ChromeUtils.defineModuleGetter(obj, "meh",
                                 "resource://gre/modules/Services.jsm");
  assertIsGetter(obj, "meh");
  equal(obj.meh, undefined, "Missing property returns undefined");
  assertIsValue(obj, "meh", undefined);


  // Test import broken module

  ChromeUtils.defineModuleGetter(obj, "broken",
                                 "resource://test/bogus_exports_type.jsm");
  assertIsGetter(obj, "broken");

  let errorPattern = /EXPORTED_SYMBOLS is not an array/;
  Assert.throws(() => child.broken, errorPattern,
                "Broken import throws on child");
  Assert.throws(() => child.broken, errorPattern,
                "Broken import throws on child again");
  Assert.throws(() => sealed.broken, errorPattern,
                "Broken import throws on sealed child");
  Assert.throws(() => obj.broken, errorPattern,
                "Broken import throws on object");
  assertIsGetter(obj, "broken");


  // Test import missing module

  ChromeUtils.defineModuleGetter(obj, "missing",
                                 "resource://test/does_not_exist.jsm");
  assertIsGetter(obj, "missing");

  Assert.throws(() => obj.missing, /NS_ERROR_FILE_NOT_FOUND/,
                "missing import throws on object");
  assertIsGetter(obj, "missing");


  // Test overwriting broken import via setter

  assertIsGetter(obj, "broken");
  obj.broken = "foo";
  assertIsValue(obj, "broken", "foo");
});
