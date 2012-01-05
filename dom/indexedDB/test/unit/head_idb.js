/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

const Ci = Components.interfaces;
const nsIIndexedDatabaseManager =
  Ci.nsIIndexedDatabaseManager;
var idbManager = Components.classes["@mozilla.org/dom/indexeddb/manager;1"]
                   .getService(nsIIndexedDatabaseManager);
idbManager.initWindowless(this);
// in xpcshell profile are not default
do_get_profile();
// oddly, if ProfD is requested from some worker thread first instead of the main thread it is crashing... so:
var dirSvc = Components.classes["@mozilla.org/file/directory_service;1"].getService(Components.interfaces.nsIProperties);
var file = dirSvc.get("ProfD", Ci.nsIFile);

const IDBDatabaseException = Ci.nsIIDBDatabaseException;
const IDBCursor = Ci.nsIIDBCursor;
const IDBTransaction = Ci.nsIIDBTransaction;
const IDBOpenDBRequest = Ci.nsIIDBOpenDBRequest;
const IDBVersionChangeEvent = Ci.nsIIDBVersionChangeEvent
const IDBDatabase = Ci.nsIIDBDatabase
const IDBFactory = Ci.nsIIDBFactory
const IDBIndex = Ci.nsIIDBIndex
const IDBObjectStore = Ci.nsIIDBObjectStore
const IDBRequest = Ci.nsIIDBRequest



function is(a, b, msg) {
  if(a != b)
    dump(msg);
  do_check_eq(a, b, Components.stack.caller);
}

function ok(cond, msg) {
  if( !cond )
    dump(msg);
  do_check_true(!!cond, Components.stack.caller); 
}

function isnot(a, b, msg) {
  if( a == b )
    dump(msg);
  do_check_neq(a, b, Components.stack.caller); 
}

function executeSoon(fun) {
  do_execute_soon(fun);
}

function todo(condition, name, diag) {
  dump("TODO: ", diag);
}

function run_test() {
  runTest();
};

function runTest()
{
  do_test_pending();
  testGenerator.next();
}

function finishTest()
{
  do_execute_soon(function(){
    testGenerator.close();
    do_test_finished();
  })
}

function grabEventAndContinueHandler(event)
{
  testGenerator.send(event);
}

function continueToNextStep()
{
  do_execute_soon(function() {
    testGenerator.next();
  });
}

function errorHandler(event)
{
  dump("indexedDB error, code " + event.target.errorCode);
  do_check_true(false);
  finishTest();
}

function unexpectedSuccessHandler()
{
  do_check_true(false);
  finishTest();
}

function ExpectError(code)
{
  this._code = code;
}
ExpectError.prototype = {
  handleEvent: function(event)
  {
    do_check_eq(event.type, "error");
    do_check_eq(this._code, event.target.errorCode);
    event.preventDefault();
    grabEventAndContinueHandler(event);
  }
};

function continueToNextStepSync()
{
  testGenerator.next();
}

function compareKeys(k1, k2) {
  let t = typeof k1;
  if (t != typeof k2)
    return false;

  if (t !== "object")
    return k1 === k2;

  if (k1 instanceof Date) {
    return (k2 instanceof Date) &&
      k1.getTime() === k2.getTime();
  }

  if (k1 instanceof Array) {
    if (!(k2 instanceof Array) ||
        k1.length != k2.length)
      return false;
    
    for (let i = 0; i < k1.length; ++i) {
      if (!compareKeys(k1[i], k2[i]))
        return false;
    }
    
    return true;
  }

  return false;
}

function addPermission(permission, url)
{
  throw "addPermission";
}

function removePermission(permission, url)
{
  throw "removePermission";
}

function setQuota(quota)
{
  throw "setQuota";
}

function allowIndexedDB(url)
{
  throw "allowIndexedDB";
}

function disallowIndexedDB(url)
{
  throw "disallowIndexedDB";
}

function allowUnlimitedQuota(url)
{
  throw "allowUnlimitedQuota";
}

function disallowUnlimitedQuota(url)
{
  throw "disallowUnlimitedQuota";
}
