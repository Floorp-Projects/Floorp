/*
 * Constants and helper functions for testing BeforeAfterKeyboardEvent.
 */

const kUnknownEvent       = 0x000;
const kKeyDownEvent       = 0x001;
const kKeyUpEvent         = 0x002;
const kBeforeEvent        = 0x010;
const kAfterEvent         = 0x020;
const kParent             = 0x100;
const kChild              = 0x200;

var gCurrentTest;

function frameScript()
{
  function handler(e) {
    var results = sendSyncMessage("forwardevent", { type: e.type });
    if (results[0]) {
      e.preventDefault();
    }
  }
  addEventListener('keydown', handler);
  addEventListener('keyup', handler);
  addEventListener('mozbrowserbeforekeydown', handler);
  addEventListener('mozbrowserbeforekeyup', handler);
  addEventListener('mozbrowserafterkeydown', handler);
  addEventListener('mozbrowserafterkeyup', handler);
}

function prepareTest(useRemote)
{
  setupHandlers(window, embedderHandler);

  var iframe = document.createElement("iframe");
  iframe.id = "embedded";
  iframe.src = "bug989198_embedded.html";
  iframe.setAttribute("remote", useRemote ? "true" : "false");
  SpecialPowers.wrap(iframe).mozbrowser = true;

  iframe.addEventListener("mozbrowserloadend", function onloadend() {
    iframe.removeEventListener("mozbrowserloadend", onloadend);
    iframe.focus();
    var mm = SpecialPowers.getBrowserFrameMessageManager(iframe);
    mm.addMessageListener("forwardevent", function(msg) {
      return embeddedHandler(msg.json);
    });
    mm.loadFrameScript("data:,(" + frameScript.toString() + ")();", false);
    runTests();
    return;
  });

  document.body.appendChild(iframe);
}

function cleanupTest()
{
  teardownHandlers(window, embedderHandler);
  runTests();
}

function setupHandlers(element, handler)
{
  element.addEventListener('keydown', handler);
  element.addEventListener('keyup', handler);
  element.addEventListener('mozbrowserbeforekeydown', handler);
  element.addEventListener('mozbrowserbeforekeyup', handler);
  element.addEventListener('mozbrowserafterkeydown', handler);
  element.addEventListener('mozbrowserafterkeyup', handler);
}

function teardownHandlers(element, handler)
{
  element.removeEventListener('keydown', handler);
  element.removeEventListener('keyup', handler);
  element.removeEventListener('mozbrowserbeforekeydown', handler);
  element.removeEventListener('mozbrowserbeforekeyup', handler);
  element.removeEventListener('mozbrowserafterkeydown', handler);
  element.removeEventListener('mozbrowserafterkeyup', handler);
}

function convertNameToCode(name)
{
  switch (name) {
    case "mozbrowserbeforekeydown":
      return kBeforeEvent | kKeyDownEvent;
    case "mozbrowserafterkeydown":
      return kAfterEvent | kKeyDownEvent;
    case "mozbrowserbeforekeyup":
      return kBeforeEvent | kKeyUpEvent;
    case "mozbrowserafterkeyup":
      return kAfterEvent | kKeyUpEvent;
    case "keydown":
      return kKeyDownEvent;
    case "keyup":
      return kKeyUpEvent;
    default:
      return kUnknownEvent;
  }
}

function classifyEvents(test)
{
  // Categorize resultEvents into KEYDOWN group and KEYUP group.
  for (var i = 0; i < gCurrentTest.resultEvents.length ; i++) {
    var code = test.resultEvents[i];
    if ((code & 0xF) == 0x1) { // KEYDOWN
      test.classifiedEvents[0].push(code);
    } else if ((code & 0xF) == 0x2) { // KEYUP
      test.classifiedEvents[1].push(code);
    } else {
      ok(false, "Invalid code for events");
    }
  }
}

function verifyResults(test)
{
  for (var i = 0; i < gCurrentTest.expectedEvents.length; i++) {
    is(test.classifiedEvents[i].length,
       test.expectedEvents[i].length,
       test.description + ": Wrong number of events");

    for (var j = 0; j < gCurrentTest.classifiedEvents[i].length; j++) {
      var item = test.classifiedEvents[i][j];
      is(item, test.expectedEvents[i][j],
         test.description + ": Wrong order of events");
     }
  }
}

function embeddedHandler(e)
{
  return handler(e, kChild);
}

function embedderHandler(e)
{
  // Verify value of attribute embeddedCancelled
  handler(e, kParent, function checkEmbeddedCancelled(code){
    switch (code) {
      case kBeforeEvent | kKeyDownEvent:
      case kBeforeEvent | kKeyUpEvent:
        is(e.embeddedCancelled, null,
           gCurrentTest.description + ': embeddedCancelled should be null');
        break;
      case kAfterEvent | kKeyDownEvent:
        if ((gCurrentTest.doPreventDefaultAt & 0xFF) == kKeyDownEvent) {
          is(e.embeddedCancelled, true,
             gCurrentTest.description + ': embeddedCancelled should be true');
        } else {
          is(e.embeddedCancelled, false,
             gCurrentTest.description + ': embeddedCancelled should be false');
        }
        break;
      case kAfterEvent | kKeyUpEvent:
        if ((gCurrentTest.doPreventDefaultAt & 0xFF) == kKeyUpEvent) {
          is(e.embeddedCancelled, true,
             gCurrentTest.description + ': embeddedCancelled should be true');
        } else {
          is(e.embeddedCancelled, false,
             gCurrentTest.description + ': embeddedCancelled should be false');
        }
        break;
      default:
        break;
    }
  });
}

function handler(e, highBit, callback)
{
  var code = convertNameToCode(e.type);
  var newCode = highBit | code;
  gCurrentTest.resultEvents.push(newCode);

  if (callback) {
    callback(code);
  }

  // Return and let frameScript to handle
  if (highBit == kChild) {
    return newCode == gCurrentTest.doPreventDefaultAt;
  }

  if (newCode == gCurrentTest.doPreventDefaultAt) {
    e.preventDefault();
  }
}

function runTests()
{
  if (!tests.length) {
    SimpleTest.finish();
    return;
  }

  var test = tests.shift();
  test();
}
