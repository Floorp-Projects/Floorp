"use strict";

// Fix the environment to run Contacts tests
if (SpecialPowers.isMainProcess()) {
  SpecialPowers.Cu.import("resource://gre/modules/ContactService.jsm");
  SpecialPowers.Cu.import("resource://gre/modules/PermissionPromptHelper.jsm");
}

SpecialPowers.addPermission("contacts-write", true, document);
SpecialPowers.addPermission("contacts-read", true, document);
SpecialPowers.addPermission("contacts-create", true, document);

// Some helpful global vars
var isAndroid = (navigator.userAgent.indexOf("Android") !== -1);

var defaultOptions = {
  sortBy: "givenName",
};

var mozContacts = navigator.mozContacts;

// To test sorting
var c1 = {
  name: ["a a"],
  familyName: ["a"],
  givenName: ["a"],
};

var c2 = {
  name: ["b b"],
  familyName: ["b"],
  givenName: ["b"],
};

var c3 = {
  name: ["c c", "a a", "b b"],
  familyName: ["c","a","b"],
  givenName: ["c","a","b"],
};

var c4 = {
  name: ["c c", "a a", "c c"],
  familyName: ["c","a","c"],
  givenName: ["c","a","c"],
};

var c5 = {
  familyName: [],
  givenName: [],
};

var c6 = {
  name: ["e"],
  familyName: ["e","e","e"],
  givenName: ["e","e","e"],
};

var c7 = {
  name: ["e"],
  familyName: ["e","e","e"],
  givenName: ["e","e","e"],
};

var c8 = {
  name: ["e"],
  familyName: ["e","e","e"],
  givenName: ["e","e","e"],
};

var adr1 = {
  type: ["work"],
  streetAddress: "street 1",
  locality: "locality 1",
  region: "region 1",
  postalCode: "postal code 1",
  countryName: "country 1"
};

var adr2 = {
  type: ["home, fax"],
  streetAddress: "street2",
  locality: "locality2",
  region: "region2",
  postalCode: "postal code2",
  countryName: "country2"
};

var properties1 = {
  // please keep capital letters at the start of these names
  name: ["Test1 TestFamilyName", "Test2 Wagner"],
  familyName: ["TestFamilyName","Wagner"],
  givenName: ["Test1","Test2"],
  phoneticFamilyName: ["TestphoneticFamilyName1","TestphoneticFamilyName2"],
  phoneticGivenName: ["TestphoneticGivenName1","TestphoneticGivenName2"],
  nickname: ["nicktest"],
  tel: [{type: ["work"], value: "123456", carrier: "testCarrier"} , {type: ["home", "fax"], value: "+55 (31) 9876-3456"}, {type: ["home"], value: "+49 451 491934"}],
  adr: [adr1],
  email: [{type: ["work"], value: "x@y.com"}],
};

var properties2 = {
  name: ["dummyHonorificPrefix dummyGivenName dummyFamilyName dummyHonorificSuffix", "dummyHonorificPrefix2"],
  familyName: ["dummyFamilyName"],
  givenName: ["dummyGivenName"],
  phoneticFamilyName: ["dummyphoneticFamilyName"],
  phoneticGivenName: ["dummyphoneticGivenName"],
  honorificPrefix: ["dummyHonorificPrefix","dummyHonorificPrefix2"],
  honorificSuffix: ["dummyHonorificSuffix"],
  additionalName: ["dummyadditionalName"],
  nickname: ["dummyNickname"],
  tel: [{type: ["test"], value: "7932012345", carrier: "myCarrier", pref: 1},{type: ["home", "custom"], value: "7932012346", pref: 0}],
  email: [{type: ["test"], value: "a@b.c"}, {value: "b@c.d", pref: 1}],
  adr: [adr1, adr2],
  impp: [{type: ["aim"], value:"im1", pref: 1}, {value: "im2"}],
  org: ["org1", "org2"],
  jobTitle: ["boss", "superboss"],
  note: ["test note"],
  category: ["cat1", "cat2"],
  url: [{type: ["work", "work2"], value: "www.1.com", pref: 1}, {value:"www2.com"}],
  bday: new Date("1980, 12, 01"),
  anniversary: new Date("2000, 12, 01"),
  sex: "male",
  genderIdentity: "test",
  key: ["ERPJ394GJJWEVJ0349GJ09W3H4FG0WFW80VHW3408GH30WGH348G3H"]
};

// To test sorting(CJK)
var c9 = {
  phoneticFamilyName: ["a"],
  phoneticGivenName: ["a"],
};

var c10 = {
  phoneticFamilyName: ["b"],
  phoneticGivenName: ["b"],
};

var c11 = {
  phoneticFamilyName: ["c","a","b"],
  phoneticGivenName: ["c","a","b"],
};

var c12 = {
  phoneticFamilyName: ["c","a","c"],
  phoneticGivenName: ["c","a","c"],
};

var c13 = {
  phoneticFamilyName: [],
  phoneticGivenName: [],
};

var c14 = {
  phoneticFamilyName: ["e","e","e"],
  phoneticGivenName: ["e","e","e"],
};

var c15 = {
  phoneticFamilyName: ["e","e","e"],
  phoneticGivenName: ["e","e","e"],
};

var c16 = {
  phoneticFamilyName: ["e","e","e"],
  phoneticGivenName: ["e","e","e"],
};

var properties3 = {
  // please keep capital letters at the start of these names
  name: ["Taro Yamada", "Ichiro Suzuki"],
  familyName: ["Yamada","Suzuki"],
  givenName: ["Taro","Ichiro"],
  phoneticFamilyName: ["TestPhoneticFamilyYamada","TestPhoneticFamilySuzuki"],
  phoneticGivenName: ["TestPhoneticGivenTaro","TestPhoneticGivenIchiro"],
  nickname: ["phoneticNicktest"],
  tel: [{type: ["work"], value: "123456", carrier: "testCarrier"} , {type: ["home", "fax"], value: "+55 (31) 9876-3456"}, {type: ["home"], value: "+49 451 491934"}],
  adr: [adr1],
  email: [{type: ["work"], value: "x@y.com"}],
};

var properties4 = {
  name: ["dummyHonorificPrefix dummyTaro dummyYamada dummyHonorificSuffix", "dummyHonorificPrefix2"],
  familyName: ["dummyYamada"],
  givenName: ["dummyTaro"],
  phoneticFamilyName: ["dummyTestPhoneticFamilyYamada"],
  phoneticGivenName: ["dummyTestPhoneticGivenTaro"],
  honorificPrefix: ["dummyPhoneticHonorificPrefix","dummyPhoneticHonorificPrefix2"],
  honorificSuffix: ["dummyPhoneticHonorificSuffix"],
  additionalName: ["dummyPhoneticAdditionalName"],
  nickname: ["dummyPhoneticNickname"],
  tel: [{type: ["test"], value: "7932012345", carrier: "myCarrier", pref: 1},{type: ["home", "custom"], value: "7932012346", pref: 0}],
  email: [{type: ["test"], value: "a@b.c"}, {value: "b@c.d", pref: 1}],
  adr: [adr1, adr2],
  impp: [{type: ["aim"], value:"im1", pref: 1}, {value: "im2"}],
  org: ["org1", "org2"],
  jobTitle: ["boss", "superboss"],
  note: ["test note"],
  category: ["cat1", "cat2"],
  url: [{type: ["work", "work2"], value: "www.1.com", pref: 1}, {value:"www2.com"}],
  bday: new Date("1980, 12, 01"),
  anniversary: new Date("2000, 12, 01"),
  sex: "male",
  genderIdentity: "test",
  key: ["ERPJ394GJJWEVJ0349GJ09W3H4FG0WFW80VHW3408GH30WGH348G3H"]
};

var sample_id1;
var sample_id2;

var createResult1;
var createResult2;

var findResult1;
var findResult2;

// DOMRequest helper functions
function onUnwantedSuccess() {
  ok(false, "onUnwantedSuccess: shouldn't get here");
}

function onFailure() {
  ok(false, "in on Failure!");
  next();
}

// Validation helper functions
function checkStr(str1, str2, msg) {
  if (str1 ^ str2) {
    ok(false, "Expected both strings to be either present or absent");
    return;
  }
  if (!str1 || str1 == "null") {
    str1 = null;
  }
  if (!str2 || str2 == "null") {
    str2 = null;
  }
  is(str1, str2, msg);
}

function checkStrArray(str1, str2, msg) {
  function normalize_falsy(v) {
    if (!v || v == "null" || v == "undefined") {
      return "";
    }
    return v;
  }
  function optArray(val) {
    return Array.isArray(val) ? val : [val];
  }
  str1 = optArray(str1).map(normalize_falsy).filter(v => v != "");
  str2 = optArray(str2).map(normalize_falsy).filter(v => v != "");
  ise(JSON.stringify(str1), JSON.stringify(str2), msg);
}

function checkPref(pref1, pref2) {
  // If on Android treat one preference as 0 and the other as undefined as matching
  if (isAndroid) {
    if ((!pref1 && pref2 == undefined) || (pref1 == undefined && !pref2)) {
      pref1 = false;
      pref2 = false;
    }
  }
  ise(!!pref1, !!pref2, "Same pref");
}

function checkAddress(adr1, adr2) {
  if (adr1 ^ adr2) {
    ok(false, "Expected both adrs to be either present or absent");
    return;
  }
  checkStrArray(adr1.type, adr2.type, "Same type");
  checkStr(adr1.streetAddress, adr2.streetAddress, "Same streetAddress");
  checkStr(adr1.locality, adr2.locality, "Same locality");
  checkStr(adr1.region, adr2.region, "Same region");
  checkStr(adr1.postalCode, adr2.postalCode, "Same postalCode");
  checkStr(adr1.countryName, adr2.countryName, "Same countryName");
  checkPref(adr1.pref, adr2.pref);
}

function checkField(field1, field2) {
  if (field1 ^ field2) {
    ok(false, "Expected both fields to be either present or absent");
    return;
  }
  checkStrArray(field1.type, field2.type, "Same type");
  checkStr(field1.value, field2.value, "Same value");
  checkPref(field1.pref, field2.pref);
}

function checkTel(tel1, tel2) {
  if (tel1 ^ tel2) {
    ok(false, "Expected both tels to be either present or absent");
    return;
  }
  checkField(tel1, tel2);
  checkStr(tel1.carrier, tel2.carrier, "Same carrier");
}

function checkCategory(category1, category2) {
  // Android adds contacts to the a default category. This should be removed from the
  // results before comparing them
  if (isAndroid) {
    category1 = removeAndroidDefaultCategory(category1);
    category2 = removeAndroidDefaultCategory(category2);
  }
  checkStrArray(category1, category2, "Same Category")
}

function removeAndroidDefaultCategory(category) {
  if (!category) {
    return category;
  }

  var result = [];

  for (var i of category) {
    // Some devices may return the full group name (prefixed with "System Group: ")
    if (i != "My Contacts" && i != "System Group: My Contacts") {
      result.push(i);
    }
  }

  return result;
}

function checkArrayField(array1, array2, func, msg) {
  if (!!array1 ^ !!array2) {
    ok(false, "Expected both arrays to be either present or absent: " + JSON.stringify(array1) + " vs. " + JSON.stringify(array2) + ". (" + msg + ")");
    return;
  }
  if (!array1 && !array2)  {
    ok(true, msg);
    return;
  }
  ise(array1.length, array2.length, "Same length");
  for (var i = 0; i < array1.length; ++i) {
    func(array1[i], array2[i], msg);
  }
}

function checkContacts(contact1, contact2) {
  if (!!contact1 ^ !!contact2) {
    ok(false, "Expected both contacts to be either present or absent");
    return;
  }
  checkStrArray(contact1.name, contact2.name, "Same name");
  checkStrArray(contact1.honorificPrefix, contact2.honorificPrefix, "Same honorificPrefix");
  checkStrArray(contact1.givenName, contact2.givenName, "Same givenName");
  checkStrArray(contact1.additionalName, contact2.additionalName, "Same additionalName");
  checkStrArray(contact1.familyName, contact2.familyName, "Same familyName");
  checkStrArray(contact1.phoneticFamilyName, contact2.phoneticFamilyName, "Same phoneticFamilyName");
  checkStrArray(contact1.phoneticGivenName, contact2.phoneticGivenName, "Same phoneticGivenName");
  checkStrArray(contact1.honorificSuffix, contact2.honorificSuffix, "Same honorificSuffix");
  checkStrArray(contact1.nickname, contact2.nickname, "Same nickname");
  checkCategory(contact1.category, contact2.category);
  checkStrArray(contact1.org, contact2.org, "Same org");
  checkStrArray(contact1.jobTitle, contact2.jobTitle, "Same jobTitle");
  is(contact1.bday ? contact1.bday.valueOf() : null, contact2.bday ? contact2.bday.valueOf() : null, "Same birthday");
  checkStrArray(contact1.note, contact2.note, "Same note");
  is(contact1.anniversary ? contact1.anniversary.valueOf() : null , contact2.anniversary ? contact2.anniversary.valueOf() : null, "Same anniversary");
  checkStr(contact1.sex, contact2.sex, "Same sex");
  checkStr(contact1.genderIdentity, contact2.genderIdentity, "Same genderIdentity");
  checkStrArray(contact1.key, contact2.key, "Same key");

  checkArrayField(contact1.adr, contact2.adr, checkAddress, "Same adr");
  checkArrayField(contact1.tel, contact2.tel, checkTel, "Same tel");
  checkArrayField(contact1.email, contact2.email, checkField, "Same email");
  checkArrayField(contact1.url, contact2.url, checkField, "Same url");
  checkArrayField(contact1.impp, contact2.impp, checkField, "Same impp");
}

function addContacts() {
  ok(true, "Adding 40 contacts");
  let req;
  for (let i = 0; i < 39; ++i) {
    properties1.familyName[0] = "Testname" + (i < 10 ? "0" + i : i);
    properties1.name = [properties1.givenName[0] + " " + properties1.familyName[0]];
    createResult1 = new mozContact(properties1);
    req = mozContacts.save(createResult1);
    req.onsuccess = function() {
      ok(createResult1.id, "The contact now has an ID.");
    };
    req.onerror = onFailure;
  };
  properties1.familyName[0] = "Testname39";
  properties1.name = [properties1.givenName[0] + " Testname39"];
  createResult1 = new mozContact(properties1);
  req = mozContacts.save(createResult1);
  req.onsuccess = function() {
    ok(createResult1.id, "The contact now has an ID.");
    checkStrArray(createResult1.name, properties1.name, "Same Name");
    next();
  };
  req.onerror = onFailure;
}

function getOne(msg) {
  return function() {
    ok(true, msg || "Retrieving one contact with getAll");
    let req = mozContacts.getAll({});

    let count = 0;
    req.onsuccess = function(event) {
      ok(true, "on success");
      if (req.result) {
        ok(true, "result is valid");
        count++;
        req.continue();
      } else {
        is(count, 1, "last contact - only one contact returned");
        next();
      }
    };
    req.onerror = onFailure;
  };
}

function getAll(msg) {
  return function() {
    ok(true, msg || "Retrieving 40 contacts with getAll");
    let req = mozContacts.getAll({
      sortBy: "familyName",
      sortOrder: "ascending"
    });
    let count = 0;
    let result;
    let props;
    req.onsuccess = function(event) {
      if (req.result) {
        ok(true, "result is valid");
        result = req.result;
        properties1.familyName[0] = "Testname" + (count < 10 ? "0" + count : count);
        is(result.familyName[0], properties1.familyName[0], "Same familyName");
        count++;
        req.continue();
      } else {
        is(count, 40, "last contact - 40 contacts returned");
        next();
      }
    };
    req.onerror = onFailure;
  };
}

function clearTemps() {
  sample_id1 = null;
  sample_id2 = null;
  createResult1 = null;
  createResult2 = null;
  findResult1 = null;
  findResult2 = null;
}

function clearDatabase() {
  ok(true, "Deleting database");
  let req = mozContacts.clear()
  req.onsuccess = function () {
    ok(true, "Deleted the database");
    next();
  }
  req.onerror = onFailure;
}

function checkCount(count, msg, then) {
  var request = mozContacts.getCount();
  request.onsuccess = function(e) {
    is(e.target.result, count, msg);
    then();
  };
  request.onerror = onFailure;
}

// Helper functions to run tests
var index = 0;

function next() {
  info("Step " + index);
  if (index >= steps.length) {
    ok(false, "Shouldn't get here!");
    return;
  }
  try {
    var i = index++;
    steps[i]();
  } catch(ex) {
    ok(false, "Caught exception", ex);
  }
}

SimpleTest.waitForExplicitFinish();

function start_tests() {
  // Skip tests on Android < 4.0 due to test failures on tbpl (see bugs 897924 & 888891)
  let androidVersion = SpecialPowers.Cc['@mozilla.org/system-info;1']
                                    .getService(SpecialPowers.Ci.nsIPropertyBag2)
                                    .getProperty('version');
  if (!isAndroid || androidVersion >= 14) {
    next();
  } else {
    ok(true, "Skip tests on Android < 4.0 (bugs 897924 & 888891");
    SimpleTest.finish();
  }
}
