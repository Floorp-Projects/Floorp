/* vim:set ts=2 sw=2 sts=2 et: */
/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Make sure that the property view correctly populates itself.
 */

var gPane = null;
var gTab = null;
var gDebugger = null;
var gVariablesView = null;
var gScope = null;
var gVariable = null;

function test()
{
  debug_tab_pane(TAB1_URL, function(aTab, aDebuggee, aPane) {
    gTab = aTab;
    gPane = aPane;
    gDebugger = gPane.panelWin;
    gVariablesView = gDebugger.DebuggerView.Variables;

    gDebugger.DebuggerView.toggleInstrumentsPane({ visible: true, animated: false });
    testVariablesView();
  });
}

function testVariablesView()
{
  let arr = [
    42,
    true,
    "nasu",
    undefined,
    null,
    [0, 1, 2],
    { prop1: 9, prop2: 8 }
  ];

  let obj = {
    "p0": 42,
    "p1": true,
    "p2": "nasu",
    "p3": undefined,
    "p4": null,
    "p5": [3, 4, 5],
    "p6": { prop1: 7, prop2: 6 },
    get p7() { return arr; },
    set p8(value) { arr[0] = value }
  };

  let test = {
    someProp0: 42,
    someProp1: true,
    someProp2: "nasu",
    someProp3: undefined,
    someProp4: null,
    someProp5: arr,
    someProp6: obj,
    get someProp7() { return arr; },
    set someProp7(value) { arr[0] = value }
  };

  gVariablesView.eval = function() {};
  gVariablesView.switch = function() {};
  gVariablesView.delete = function() {};
  gVariablesView.rawObject = test;

  testHierarchy();
  testHeader();
  testFirstLevelContents();
  testSecondLevelContents();
  testThirdLevelContents();
  testIntegrity(arr, obj);

  let fooScope = gVariablesView.addScope("foo");
  let anonymousVar = fooScope.addVar();

  let anonymousScope = gVariablesView.addScope();
  let barVar = anonymousScope.addVar("bar");
  let bazProperty = barVar.addProperty("baz");

  testAnonymousHeaders(fooScope, anonymousVar, anonymousScope, barVar, bazProperty);
  testPropertyInheritance(fooScope, anonymousVar, anonymousScope, barVar, bazProperty);

  executeSoon(function() {
    testKeyboardAccessibility(function() {
      testClearHierarchy();

      closeDebuggerAndFinish();
    });
  });
}

function testHierarchy() {
  is(gVariablesView._currHierarchy.size, 13,
    "There should be 1 scope, 1 var, 1 proto, 8 props, 1 getter and 1 setter.");

  gScope = gVariablesView._currHierarchy.get("");
  gVariable = gVariablesView._currHierarchy.get("[\"\"]");

  is(gVariablesView._store.size, 1,
    "There should be only one scope in the view");
  is(gScope._store.size, 1,
    "There should be only one variable in the scope");
  is(gVariable._store.size, 9,
    "There should be 1 __proto__ and 8 properties in the variable");
}

function testHeader() {
  is(gScope.header, false,
    "The scope title header should be hidden");
  is(gVariable.header, false,
    "The variable title header should be hidden");

  gScope.showHeader();
  gVariable.showHeader();

  is(gScope.header, false,
    "The scope title header should still not be visible");
  is(gVariable.header, false,
    "The variable title header should still not be visible");

  gScope.hideHeader();
  gVariable.hideHeader();

  is(gScope.header, false,
    "The scope title header should now still be hidden");
  is(gVariable.header, false,
    "The variable title header should now still be hidden");
}

function testFirstLevelContents() {
  let someProp0 = gVariable.get("someProp0");
  let someProp1 = gVariable.get("someProp1");
  let someProp2 = gVariable.get("someProp2");
  let someProp3 = gVariable.get("someProp3");
  let someProp4 = gVariable.get("someProp4");
  let someProp5 = gVariable.get("someProp5");
  let someProp6 = gVariable.get("someProp6");
  let someProp7 = gVariable.get("someProp7");
  let __proto__ = gVariable.get("__proto__");

  is(someProp0.visible, true, "The first property visible state is correct.");
  is(someProp1.visible, true, "The second property visible state is correct.");
  is(someProp2.visible, true, "The third property visible state is correct.");
  is(someProp3.visible, true, "The fourth property visible state is correct.");
  is(someProp4.visible, true, "The fifth property visible state is correct.");
  is(someProp5.visible, true, "The sixth property visible state is correct.");
  is(someProp6.visible, true, "The seventh property visible state is correct.");
  is(someProp7.visible, true, "The eight property visible state is correct.");
  is(__proto__.visible, true, "The __proto__ property visible state is correct.");

  is(someProp0.expanded, false, "The first property expanded state is correct.");
  is(someProp1.expanded, false, "The second property expanded state is correct.");
  is(someProp2.expanded, false, "The third property expanded state is correct.");
  is(someProp3.expanded, false, "The fourth property expanded state is correct.");
  is(someProp4.expanded, false, "The fifth property expanded state is correct.");
  is(someProp5.expanded, false, "The sixth property expanded state is correct.");
  is(someProp6.expanded, false, "The seventh property expanded state is correct.");
  is(someProp7.expanded, true, "The eight property expanded state is correct.");
  is(__proto__.expanded, false, "The __proto__ property expanded state is correct.");

  is(someProp0.header, true, "The first property header state is correct.");
  is(someProp1.header, true, "The second property header state is correct.");
  is(someProp2.header, true, "The third property header state is correct.");
  is(someProp3.header, true, "The fourth property header state is correct.");
  is(someProp4.header, true, "The fifth property header state is correct.");
  is(someProp5.header, true, "The sixth property header state is correct.");
  is(someProp6.header, true, "The seventh property header state is correct.");
  is(someProp7.header, true, "The eight property header state is correct.");
  is(__proto__.header, true, "The __proto__ property header state is correct.");

  is(someProp0.twisty, false, "The first property twisty state is correct.");
  is(someProp1.twisty, false, "The second property twisty state is correct.");
  is(someProp2.twisty, false, "The third property twisty state is correct.");
  is(someProp3.twisty, false, "The fourth property twisty state is correct.");
  is(someProp4.twisty, false, "The fifth property twisty state is correct.");
  is(someProp5.twisty, true, "The sixth property twisty state is correct.");
  is(someProp6.twisty, true, "The seventh property twisty state is correct.");
  is(someProp7.twisty, true, "The eight property twisty state is correct.");
  is(__proto__.twisty, true, "The __proto__ property twisty state is correct.");

  is(someProp0.name, "someProp0", "The first property name is correct.");
  is(someProp1.name, "someProp1", "The second property name is correct.");
  is(someProp2.name, "someProp2", "The third property name is correct.");
  is(someProp3.name, "someProp3", "The fourth property name is correct.");
  is(someProp4.name, "someProp4", "The fifth property name is correct.");
  is(someProp5.name, "someProp5", "The sixth property name is correct.");
  is(someProp6.name, "someProp6", "The seventh property name is correct.");
  is(someProp7.name, "someProp7", "The eight property name is correct.");
  is(__proto__.name, "__proto__", "The __proto__ property name is correct.");

  is(someProp0.value, 42, "The first property value is correct.");
  is(someProp1.value, true, "The second property value is correct.");
  is(someProp2.value, "nasu", "The third property value is correct.");
  is(someProp3.value.type, "undefined", "The fourth property value is correct.");
  is(someProp4.value.type, "null", "The fifth property value is correct.");
  is(someProp5.value.type, "object", "The sixth property value type is correct.");
  is(someProp5.value.class, "Array", "The sixth property value class is correct.");
  is(someProp6.value.type, "object", "The seventh property value type is correct.");
  is(someProp6.value.class, "Object", "The seventh property value class is correct.");
  is(someProp7.value, null, "The eight property value is correct.");
  isnot(someProp7.getter, null, "The eight property getter is correct.");
  isnot(someProp7.setter, null, "The eight property setter is correct.");
  is(someProp7.getter.type, "object", "The eight property getter type is correct.");
  is(someProp7.getter.class, "Function", "The eight property getter class is correct.");
  is(someProp7.setter.type, "object", "The eight property setter type is correct.");
  is(someProp7.setter.class, "Function", "The eight property setter class is correct.");
  is(__proto__.value.type, "object", "The __proto__ property value type is correct.");
  is(__proto__.value.class, "Object", "The __proto__ property value class is correct.");


  someProp0.expand();
  someProp1.expand();
  someProp2.expand();
  someProp3.expand();
  someProp4.expand();
  someProp7.expand();

  ok(!someProp0.get("__proto__"), "Number primitives should not have a prototype");
  ok(!someProp1.get("__proto__"), "Boolean primitives should not have a prototype");
  ok(!someProp2.get("__proto__"), "String literals should not have a prototype");
  ok(!someProp3.get("__proto__"), "Undefined values should not have a prototype");
  ok(!someProp4.get("__proto__"), "Null values should not have a prototype");
  ok(!someProp7.get("__proto__"), "Getter properties should not have a prototype");
}

function testSecondLevelContents() {
  let someProp5 = gVariable.get("someProp5");

  is(someProp5._store.size, 0, "No properties should be in someProp5 before expanding");
  someProp5.expand();
  is(someProp5._store.size, 9, "Some properties should be in someProp5 before expanding");

  let arrayItem0 = someProp5.get("0");
  let arrayItem1 = someProp5.get("1");
  let arrayItem2 = someProp5.get("2");
  let arrayItem3 = someProp5.get("3");
  let arrayItem4 = someProp5.get("4");
  let arrayItem5 = someProp5.get("5");
  let arrayItem6 = someProp5.get("6");
  let __proto__ = someProp5.get("__proto__");

  is(arrayItem0.visible, true, "The first array item visible state is correct.");
  is(arrayItem1.visible, true, "The second array item visible state is correct.");
  is(arrayItem2.visible, true, "The third array item visible state is correct.");
  is(arrayItem3.visible, true, "The fourth array item visible state is correct.");
  is(arrayItem4.visible, true, "The fifth array item visible state is correct.");
  is(arrayItem5.visible, true, "The sixth array item visible state is correct.");
  is(arrayItem6.visible, true, "The seventh array item visible state is correct.");
  is(__proto__.visible, true, "The __proto__ property visible state is correct.");

  is(arrayItem0.expanded, false, "The first array item expanded state is correct.");
  is(arrayItem1.expanded, false, "The second array item expanded state is correct.");
  is(arrayItem2.expanded, false, "The third array item expanded state is correct.");
  is(arrayItem3.expanded, false, "The fourth array item expanded state is correct.");
  is(arrayItem4.expanded, false, "The fifth array item expanded state is correct.");
  is(arrayItem5.expanded, false, "The sixth array item expanded state is correct.");
  is(arrayItem6.expanded, false, "The seventh array item expanded state is correct.");
  is(__proto__.expanded, false, "The __proto__ property expanded state is correct.");

  is(arrayItem0.header, true, "The first array item header state is correct.");
  is(arrayItem1.header, true, "The second array item header state is correct.");
  is(arrayItem2.header, true, "The third array item header state is correct.");
  is(arrayItem3.header, true, "The fourth array item header state is correct.");
  is(arrayItem4.header, true, "The fifth array item header state is correct.");
  is(arrayItem5.header, true, "The sixth array item header state is correct.");
  is(arrayItem6.header, true, "The seventh array item header state is correct.");
  is(__proto__.header, true, "The __proto__ property header state is correct.");

  is(arrayItem0.twisty, false, "The first array item twisty state is correct.");
  is(arrayItem1.twisty, false, "The second array item twisty state is correct.");
  is(arrayItem2.twisty, false, "The third array item twisty state is correct.");
  is(arrayItem3.twisty, false, "The fourth array item twisty state is correct.");
  is(arrayItem4.twisty, false, "The fifth array item twisty state is correct.");
  is(arrayItem5.twisty, true, "The sixth array item twisty state is correct.");
  is(arrayItem6.twisty, true, "The seventh array item twisty state is correct.");
  is(__proto__.twisty, true, "The __proto__ property twisty state is correct.");

  is(arrayItem0.name, "0", "The first array item name is correct.");
  is(arrayItem1.name, "1", "The second array item name is correct.");
  is(arrayItem2.name, "2", "The third array item name is correct.");
  is(arrayItem3.name, "3", "The fourth array item name is correct.");
  is(arrayItem4.name, "4", "The fifth array item name is correct.");
  is(arrayItem5.name, "5", "The sixth array item name is correct.");
  is(arrayItem6.name, "6", "The seventh array item name is correct.");
  is(__proto__.name, "__proto__", "The __proto__ property name is correct.");

  is(arrayItem0.value, 42, "The first array item value is correct.");
  is(arrayItem1.value, true, "The second array item value is correct.");
  is(arrayItem2.value, "nasu", "The third array item value is correct.");
  is(arrayItem3.value.type, "undefined", "The fourth array item value is correct.");
  is(arrayItem4.value.type, "null", "The fifth array item value is correct.");
  is(arrayItem5.value.type, "object", "The sixth array item value type is correct.");
  is(arrayItem5.value.class, "Array", "The sixth array item value class is correct.");
  is(arrayItem6.value.type, "object", "The seventh array item value type is correct.");
  is(arrayItem6.value.class, "Object", "The seventh array item value class is correct.");
  is(__proto__.value.type, "object", "The __proto__ property value type is correct.");
  is(__proto__.value.class, "Array", "The __proto__ property value class is correct.");


  let someProp6 = gVariable.get("someProp6");

  is(someProp6._store.size, 0, "No properties should be in someProp6 before expanding");
  someProp6.expand();
  is(someProp6._store.size, 10, "Some properties should be in someProp6 before expanding");

  let objectItem0 = someProp6.get("p0");
  let objectItem1 = someProp6.get("p1");
  let objectItem2 = someProp6.get("p2");
  let objectItem3 = someProp6.get("p3");
  let objectItem4 = someProp6.get("p4");
  let objectItem5 = someProp6.get("p5");
  let objectItem6 = someProp6.get("p6");
  let objectItem7 = someProp6.get("p7");
  let objectItem8 = someProp6.get("p8");
  let __proto__ = someProp6.get("__proto__");

  is(objectItem0.visible, true, "The first object item visible state is correct.");
  is(objectItem1.visible, true, "The second object item visible state is correct.");
  is(objectItem2.visible, true, "The third object item visible state is correct.");
  is(objectItem3.visible, true, "The fourth object item visible state is correct.");
  is(objectItem4.visible, true, "The fifth object item visible state is correct.");
  is(objectItem5.visible, true, "The sixth object item visible state is correct.");
  is(objectItem6.visible, true, "The seventh object item visible state is correct.");
  is(objectItem7.visible, true, "The eight object item visible state is correct.");
  is(objectItem8.visible, true, "The ninth object item visible state is correct.");
  is(__proto__.visible, true, "The __proto__ property visible state is correct.");

  is(objectItem0.expanded, false, "The first object item expanded state is correct.");
  is(objectItem1.expanded, false, "The second object item expanded state is correct.");
  is(objectItem2.expanded, false, "The third object item expanded state is correct.");
  is(objectItem3.expanded, false, "The fourth object item expanded state is correct.");
  is(objectItem4.expanded, false, "The fifth object item expanded state is correct.");
  is(objectItem5.expanded, false, "The sixth object item expanded state is correct.");
  is(objectItem6.expanded, false, "The seventh object item expanded state is correct.");
  is(objectItem7.expanded, true, "The eight object item expanded state is correct.");
  is(objectItem8.expanded, true, "The ninth object item expanded state is correct.");
  is(__proto__.expanded, false, "The __proto__ property expanded state is correct.");

  is(objectItem0.header, true, "The first object item header state is correct.");
  is(objectItem1.header, true, "The second object item header state is correct.");
  is(objectItem2.header, true, "The third object item header state is correct.");
  is(objectItem3.header, true, "The fourth object item header state is correct.");
  is(objectItem4.header, true, "The fifth object item header state is correct.");
  is(objectItem5.header, true, "The sixth object item header state is correct.");
  is(objectItem6.header, true, "The seventh object item header state is correct.");
  is(objectItem7.header, true, "The eight object item header state is correct.");
  is(objectItem8.header, true, "The ninth object item header state is correct.");
  is(__proto__.header, true, "The __proto__ property header state is correct.");

  is(objectItem0.twisty, false, "The first object item twisty state is correct.");
  is(objectItem1.twisty, false, "The second object item twisty state is correct.");
  is(objectItem2.twisty, false, "The third object item twisty state is correct.");
  is(objectItem3.twisty, false, "The fourth object item twisty state is correct.");
  is(objectItem4.twisty, false, "The fifth object item twisty state is correct.");
  is(objectItem5.twisty, true, "The sixth object item twisty state is correct.");
  is(objectItem6.twisty, true, "The seventh object item twisty state is correct.");
  is(objectItem7.twisty, true, "The eight object item twisty state is correct.");
  is(objectItem8.twisty, true, "The ninth object item twisty state is correct.");
  is(__proto__.twisty, true, "The __proto__ property twisty state is correct.");

  is(objectItem0.name, "p0", "The first object item name is correct.");
  is(objectItem1.name, "p1", "The second object item name is correct.");
  is(objectItem2.name, "p2", "The third object item name is correct.");
  is(objectItem3.name, "p3", "The fourth object item name is correct.");
  is(objectItem4.name, "p4", "The fifth object item name is correct.");
  is(objectItem5.name, "p5", "The sixth object item name is correct.");
  is(objectItem6.name, "p6", "The seventh object item name is correct.");
  is(objectItem7.name, "p7", "The eight seventh object item name is correct.");
  is(objectItem8.name, "p8", "The ninth seventh object item name is correct.");
  is(__proto__.name, "__proto__", "The __proto__ property name is correct.");

  is(objectItem0.value, 42, "The first object item value is correct.");
  is(objectItem1.value, true, "The second object item value is correct.");
  is(objectItem2.value, "nasu", "The third object item value is correct.");
  is(objectItem3.value.type, "undefined", "The fourth object item value is correct.");
  is(objectItem4.value.type, "null", "The fifth object item value is correct.");
  is(objectItem5.value.type, "object", "The sixth object item value type is correct.");
  is(objectItem5.value.class, "Array", "The sixth object item value class is correct.");
  is(objectItem6.value.type, "object", "The seventh object item value type is correct.");
  is(objectItem6.value.class, "Object", "The seventh object item value class is correct.");
  is(objectItem7.value, null, "The eight object item value is correct.");
  isnot(objectItem7.getter, null, "The eight object item getter is correct.");
  isnot(objectItem7.setter, null, "The eight object item setter is correct.");
  is(objectItem7.setter.type, "undefined", "The eight object item setter type is correct.");
  is(objectItem7.getter.type, "object", "The eight object item getter type is correct.");
  is(objectItem7.getter.class, "Function", "The eight object item getter class is correct.");
  is(objectItem8.value, null, "The ninth object item value is correct.");
  isnot(objectItem8.getter, null, "The ninth object item getter is correct.");
  isnot(objectItem8.setter, null, "The ninth object item setter is correct.");
  is(objectItem8.getter.type, "undefined", "The eight object item getter type is correct.");
  is(objectItem8.setter.type, "object", "The ninth object item setter type is correct.");
  is(objectItem8.setter.class, "Function", "The ninth object item setter class is correct.");
  is(__proto__.value.type, "object", "The __proto__ property value type is correct.");
  is(__proto__.value.class, "Object", "The __proto__ property value class is correct.");
}

function testThirdLevelContents() {
  (function() {
    let someProp5 = gVariable.get("someProp5");
    let arrayItem5 = someProp5.get("5");
    let arrayItem6 = someProp5.get("6");

    is(arrayItem5._store.size, 0, "No properties should be in arrayItem5 before expanding");
    arrayItem5.expand();
    is(arrayItem5._store.size, 5, "Some properties should be in arrayItem5 before expanding");

    is(arrayItem6._store.size, 0, "No properties should be in arrayItem6 before expanding");
    arrayItem6.expand();
    is(arrayItem6._store.size, 3, "Some properties should be in arrayItem6 before expanding");

    let arraySubItem0 = arrayItem5.get("0");
    let arraySubItem1 = arrayItem5.get("1");
    let arraySubItem2 = arrayItem5.get("2");
    let objectSubItem0 = arrayItem6.get("prop1");
    let objectSubItem1 = arrayItem6.get("prop2");

    is(arraySubItem0.value, 0, "The first array sub-item value is correct.");
    is(arraySubItem1.value, 1, "The second array sub-item value is correct.");
    is(arraySubItem2.value, 2, "The third array sub-item value is correct.");

    is(objectSubItem0.value, 9, "The first object sub-item value is correct.");
    is(objectSubItem1.value, 8, "The second object sub-item value is correct.");

    let array__proto__ = arrayItem5.get("__proto__");
    let object__proto__ = arrayItem6.get("__proto__");

    ok(array__proto__, "The array should have a __proto__ property");
    ok(object__proto__, "The object should have a __proto__ property");
  })();

  (function() {
    let someProp6 = gVariable.get("someProp6");
    let objectItem5 = someProp6.get("p5");
    let objectItem6 = someProp6.get("p6");

    is(objectItem5._store.size, 0, "No properties should be in objectItem5 before expanding");
    objectItem5.expand();
    is(objectItem5._store.size, 5, "Some properties should be in objectItem5 before expanding");

    is(objectItem6._store.size, 0, "No properties should be in objectItem6 before expanding");
    objectItem6.expand();
    is(objectItem6._store.size, 3, "Some properties should be in objectItem6 before expanding");

    let arraySubItem0 = objectItem5.get("0");
    let arraySubItem1 = objectItem5.get("1");
    let arraySubItem2 = objectItem5.get("2");
    let objectSubItem0 = objectItem6.get("prop1");
    let objectSubItem1 = objectItem6.get("prop2");

    is(arraySubItem0.value, 3, "The first array sub-item value is correct.");
    is(arraySubItem1.value, 4, "The second array sub-item value is correct.");
    is(arraySubItem2.value, 5, "The third array sub-item value is correct.");

    is(objectSubItem0.value, 7, "The first object sub-item value is correct.");
    is(objectSubItem1.value, 6, "The second object sub-item value is correct.");

    let array__proto__ = objectItem5.get("__proto__");
    let object__proto__ = objectItem6.get("__proto__");

    ok(array__proto__, "The array should have a __proto__ property");
    ok(object__proto__, "The object should have a __proto__ property");
  })();
}

function testIntegrity(arr, obj) {
  is(arr[0], 42, "The first array item should not have changed");
  is(arr[1], true, "The second array item should not have changed");
  is(arr[2], "nasu", "The third array item should not have changed");
  is(arr[3], undefined, "The fourth array item should not have changed");
  is(arr[4], null, "The fifth array item should not have changed");
  ok(arr[5] instanceof Array, "The sixth array item should be an Array");
  is(arr[5][0], 0, "The sixth array item should not have changed");
  is(arr[5][1], 1, "The sixth array item should not have changed");
  is(arr[5][2], 2, "The sixth array item should not have changed");
  ok(arr[6] instanceof Object, "The seventh array item should be an Object");
  is(arr[6].prop1, 9, "The seventh array item should not have changed");
  is(arr[6].prop2, 8, "The seventh array item should not have changed");

  is(obj.p0, 42, "The first object property should not have changed");
  is(obj.p1, true, "The first object property should not have changed");
  is(obj.p2, "nasu", "The first object property should not have changed");
  is(obj.p3, undefined, "The first object property should not have changed");
  is(obj.p4, null, "The first object property should not have changed");
  ok(obj.p5 instanceof Array, "The sixth object property should be an Array");
  is(obj.p5[0], 3, "The sixth object property should not have changed");
  is(obj.p5[1], 4, "The sixth object property should not have changed");
  is(obj.p5[2], 5, "The sixth object property should not have changed");
  ok(obj.p6 instanceof Object, "The seventh object property should be an Object");
  is(obj.p6.prop1, 7, "The seventh object property should not have changed");
  is(obj.p6.prop2, 6, "The seventh object property should not have changed");
}

function testAnonymousHeaders(fooScope, anonymousVar, anonymousScope, barVar, bazProperty) {
  is(fooScope.header, true,
    "A named scope should have a header visible.");
  is(fooScope.target.hasAttribute("non-header"), false,
    "The non-header attribute should not be applied to scopes with headers.");

  is(anonymousScope.header, false,
    "An anonymous scope should have a header visible.");
  is(anonymousScope.target.hasAttribute("non-header"), true,
    "The non-header attribute should not be applied to scopes without headers.");

  is(barVar.header, true,
    "A named variable should have a header visible.");
  is(barVar.target.hasAttribute("non-header"), false,
    "The non-header attribute should not be applied to variables with headers.");

  is(anonymousVar.header, false,
    "An anonymous variable should have a header visible.");
  is(anonymousVar.target.hasAttribute("non-header"), true,
    "The non-header attribute should not be applied to variables without headers.");
}

function testPropertyInheritance(fooScope, anonymousVar, anonymousScope, barVar, bazProperty) {
  is(fooScope.editableValueTooltip, gVariablesView.editableValueTooltip,
    "The editableValueTooltip property should persist from the view to all scopes.");
  is(fooScope.editableNameTooltip, gVariablesView.editableNameTooltip,
    "The editableNameTooltip property should persist from the view to all scopes.");
  is(fooScope.deleteButtonTooltip, gVariablesView.deleteButtonTooltip,
    "The deleteButtonTooltip property should persist from the view to all scopes.");
  is(fooScope.descriptorTooltip, gVariablesView.descriptorTooltip,
    "The descriptorTooltip property should persist from the view to all scopes.");
  is(fooScope.contextMenuId, gVariablesView.contextMenuId,
    "The contextMenuId property should persist from the view to all scopes.");
  is(fooScope.separatorStr, gVariablesView.separatorStr,
    "The separatorStr property should persist from the view to all scopes.");
  is(fooScope.eval, gVariablesView.eval,
    "The eval property should persist from the view to all scopes.");
  is(fooScope.switch, gVariablesView.switch,
    "The switch property should persist from the view to all scopes.");
  is(fooScope.delete, gVariablesView.delete,
    "The delete property should persist from the view to all scopes.");
  isnot(fooScope.eval, fooScope.switch,
    "The eval and switch functions got mixed up in the scope.");
  isnot(fooScope.switch, fooScope.delete,
    "The eval and switch functions got mixed up in the scope.");

  is(barVar.editableValueTooltip, gVariablesView.editableValueTooltip,
    "The editableValueTooltip property should persist from the view to all variables.");
  is(barVar.editableNameTooltip, gVariablesView.editableNameTooltip,
    "The editableNameTooltip property should persist from the view to all variables.");
  is(barVar.deleteButtonTooltip, gVariablesView.deleteButtonTooltip,
    "The deleteButtonTooltip property should persist from the view to all variables.");
  is(barVar.descriptorTooltip, gVariablesView.descriptorTooltip,
    "The descriptorTooltip property should persist from the view to all variables.");
  is(barVar.contextMenuId, gVariablesView.contextMenuId,
    "The contextMenuId property should persist from the view to all variables.");
  is(barVar.separatorStr, gVariablesView.separatorStr,
    "The separatorStr property should persist from the view to all variables.");
  is(barVar.eval, gVariablesView.eval,
    "The eval property should persist from the view to all variables.");
  is(barVar.switch, gVariablesView.switch,
    "The switch property should persist from the view to all variables.");
  is(barVar.delete, gVariablesView.delete,
    "The delete property should persist from the view to all variables.");
  isnot(barVar.eval, barVar.switch,
    "The eval and switch functions got mixed up in the variable.");
  isnot(barVar.switch, barVar.delete,
    "The eval and switch functions got mixed up in the variable.");

  is(bazProperty.editableValueTooltip, gVariablesView.editableValueTooltip,
    "The editableValueTooltip property should persist from the view to all properties.");
  is(bazProperty.editableNameTooltip, gVariablesView.editableNameTooltip,
    "The editableNameTooltip property should persist from the view to all properties.");
  is(bazProperty.deleteButtonTooltip, gVariablesView.deleteButtonTooltip,
    "The deleteButtonTooltip property should persist from the view to all properties.");
  is(bazProperty.descriptorTooltip, gVariablesView.descriptorTooltip,
    "The descriptorTooltip property should persist from the view to all properties.");
  is(bazProperty.contextMenuId, gVariablesView.contextMenuId,
    "The contextMenuId property should persist from the view to all properties.");
  is(bazProperty.separatorStr, gVariablesView.separatorStr,
    "The separatorStr property should persist from the view to all properties.");
  is(bazProperty.eval, gVariablesView.eval,
    "The eval property should persist from the view to all properties.");
  is(bazProperty.switch, gVariablesView.switch,
    "The switch property should persist from the view to all properties.");
  is(bazProperty.delete, gVariablesView.delete,
    "The delete property should persist from the view to all properties.");
  isnot(bazProperty.eval, bazProperty.switch,
    "The eval and switch functions got mixed up in the property.");
  isnot(bazProperty.switch, bazProperty.delete,
    "The eval and switch functions got mixed up in the property.");
}

function testKeyboardAccessibility(callback) {
  gDebugger.DebuggerView.Filtering._doVariablesFocus();
  gDebugger.DebuggerView.Variables.pageSize = 5;

  is(gVariablesView.getFocusedItem().name, "someProp0",
    "The someProp0 item should be focused.");

  gVariablesView.focusNextItem();
  is(gVariablesView.getFocusedItem().name, "someProp1",
    "The someProp1 item should be focused.");

  gVariablesView.focusPrevItem();
  is(gVariablesView.getFocusedItem().name, "someProp0",
    "The someProp0 item should be focused again.");


  ok(!gVariablesView._list.querySelector(".element-value-input"),
    "There shouldn't be a value input element created.");

  EventUtils.synthesizeKey("VK_ENTER", {}, gDebugger);
  waitForElement(".element-value-input", true, function() {

    ok(gVariablesView._list.querySelector(".element-value-input"),
      "There should be a value input element created.");

    EventUtils.sendKey("ESCAPE", gDebugger);
    waitForElement(".element-value-input", false, function() {

      ok(!gVariablesView._list.querySelector(".element-value-input"),
        "There shouldn't be a value input element anymore.");

      ok(!gVariablesView._list.querySelector(".element-name-input"),
        "There shouldn't be a name input element created.");

      EventUtils.synthesizeKey("VK_ENTER", { shiftKey: true }, gDebugger);
      waitForElement(".element-name-input", true, function() {

        ok(gVariablesView._list.querySelector(".element-name-input"),
          "There should be a name input element created.");

        EventUtils.sendKey("ESCAPE", gDebugger);
        waitForElement(".element-name-input", false, function() {

          ok(!gVariablesView._list.querySelector(".element-name-input"),
            "There shouldn't be a name input element anymore.");


          EventUtils.sendKey("DOWN", gDebugger);
          executeSoon(function() {
            is(gVariablesView._parent.scrollTop, 0,
              "The variables view shouldn't scroll when pressing the DOWN key.");

            EventUtils.sendKey("UP", gDebugger);
            executeSoon(function() {
              is(gVariablesView._parent.scrollTop, 0,
                "The variables view shouldn't scroll when pressing the UP key.");

              EventUtils.sendKey("PAGE_DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp5",
                "The someProp5 item should be focused now.");

              EventUtils.sendKey("DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "0",
                "The 0 item should be focused now.");

              EventUtils.sendKey("END", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo item should be focused now.");

              EventUtils.sendKey("DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "bar",
                "The bar item should be focused now.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo item should be focused now.");

              EventUtils.sendKey("RIGHT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo item should still be focused now.");

              EventUtils.sendKey("PAGE_DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo item should still be focused now.");

              EventUtils.sendKey("PAGE_UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The __proto__ item should be focused now.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "set",
                "The set item should be focused now.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "get",
                "The get item should be focused now.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "p8",
                "The p8 item should be focused now.");

              EventUtils.sendKey("HOME", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should be focused now.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should still be focused now.");

              EventUtils.sendKey("LEFT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should still be focused now.");

              EventUtils.sendKey("PAGE_UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should still be focused now.");

              for (let i = 0; i < 16; i++) {
                // Advance to the first collapsed __proto__ property.
                EventUtils.sendKey("DOWN", gDebugger);
              }
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The __proto__ item should be focused now.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The __proto__ item shouldn't be expanded yet.");

              EventUtils.sendKey("RIGHT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The __proto__ item should still be focused.");
              is(gVariablesView.getFocusedItem().expanded, true,
                "The __proto__ item should be expanded now.");

              for (let i = 0; i < 3; i++) {
                // Advance to the fifth top-level someProp5 property.
                EventUtils.sendKey("LEFT", gDebugger);
              }
              is(gVariablesView.getFocusedItem().name, "5",
                "The fifth array item should be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The fifth array item should not be expanded now.");

              for (let i = 0; i < 6; i++) {
                // Advance to the fifth top-level someProp5 property.
                EventUtils.sendKey("UP", gDebugger);
              }
              is(gVariablesView.getFocusedItem().name, "someProp5",
                "The someProp5 item should be focused now.");
              is(gVariablesView.getFocusedItem().expanded, true,
                "The someProp5 item should already be expanded.");

              EventUtils.sendKey("LEFT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp5",
                "The someProp5 item should still be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The someProp5 item should not be expanded now.");

              EventUtils.sendKey("LEFT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp5",
                "The someProp5 item should still be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp4",
                "The someProp4 item should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp3",
                "The someProp3 item should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp2",
                "The someProp2 item should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp1",
                "The someProp1 item should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "someProp0",
                "The someProp0 item should still be focused.");

              for (let i = 0; i < 32; i++) {
                // Advance to the last property in this scope.
                EventUtils.sendKey("DOWN", gDebugger);
              }
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The top-level __proto__ item should be focused.");

              EventUtils.sendKey("DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo scope should be focused now.");
              is(gVariablesView.getFocusedItem().expanded, true,
                "The foo scope should already be expanded.");

              EventUtils.sendKey("LEFT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo scope should be focused now.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The foo scope shouldn't be expanded now.");

              EventUtils.sendKey("DOWN", gDebugger);
              is(gVariablesView.getFocusedItem().name, "bar",
                "The bar variable should be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The bar variable shouldn't be expanded.");
              is(gVariablesView.getFocusedItem().visible, true,
                "The bar variable shouldn't be hidden.");

              EventUtils.sendKey("BACK_SPACE", gDebugger);
              is(gVariablesView.getFocusedItem().name, "bar",
                "The bar variable should still be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The bar variable should still not be expanded.");
              is(gVariablesView.getFocusedItem().visible, false,
                "The bar variable should be hidden.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "foo",
                "The foo scope should be focused.");

              EventUtils.sendKey("UP", gDebugger);
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The top-level __proto__ item should be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The top-level __proto__ item should not be expanded.");

              EventUtils.sendKey("RIGHT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The top-level __proto__ item should still be focused.");
              is(gVariablesView.getFocusedItem().expanded, true,
                "The top-level __proto__ item should be expanded.");

              EventUtils.sendKey("LEFT", gDebugger);
              is(gVariablesView.getFocusedItem().name, "__proto__",
                "The top-level __proto__ item should still be focused.");
              is(gVariablesView.getFocusedItem().expanded, false,
                "The top-level __proto__ item should not be expanded.");

              executeSoon(callback);
            });
          });
        });
      });
    });
  });
}

function waitForElement(selector, exists, callback)
{
  // Poll every few milliseconds until the element are retrieved.
  let count = 0;
  let intervalID = window.setInterval(function() {
    info("count: " + count + " ");
    if (++count > 50) {
      ok(false, "Timed out while polling for the element.");
      window.clearInterval(intervalID);
      return closeDebuggerAndFinish();
    }
    if (!!gVariablesView._list.querySelector(selector) != exists) {
      return;
    }
    // We got the element, it's safe to callback.
    window.clearInterval(intervalID);
    callback();
  }, 100);
}

function testClearHierarchy() {
  gVariablesView.clearHierarchy();
  ok(!gVariablesView._prevHierarchy.size,
    "The previous hierarchy should have been cleared.");
  ok(!gVariablesView._currHierarchy.size,
    "The current hierarchy should have been cleared.");
}

registerCleanupFunction(function() {
  removeTab(gTab);
  gPane = null;
  gTab = null;
  gDebugger = null;
  gVariablesView = null;
  gScope = null;
  gVariable = null;
});
