/**
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/publicdomain/zero/1.0/
 */

function continueTest(aEvent)
{
  gGen.send(aEvent);
}

function errorHandler(aEvent)
{
  ok(false, "indexedDB error (" + aEvent.code + "): " + aEvent.message);
  finishTest();
}

function unexpectedSuccessHandler()
{
  ok(false, "Got success, but did not expect it!");
  finishTest();
}

function ExpectError(aCode)
{
  this._code = aCode;
}
ExpectError.prototype = {
  handleEvent: function(aEvent)
  {
    is(this._code, aEvent.code, "Expected error was thrown.");
    continueTest(aEvent);
  },
};

function finishTest()
{
  gGen.close();
  SimpleTest.finish();
}
