const Ci = Components.interfaces;
const Cc = Components.classes;

const kUrlBarElm = document.getElementById('urlbar');
const kSearchBarElm = document.getElementById('searchbar');
const kTestString = "  hello hello  \n  world\nworld  ";

function testPaste(name, element, expected) {
  element.focus();
  listener.expected = expected;
  listener.name = name;
  // Pasting is async because the Accel+V codepath ends up going through
  // DocumentViewerImpl::FireClipboardEvent.
  EventUtils.synthesizeKey("v", { accelKey: true });
}

var listener = {
  expected: "",
  name: "",
  handleEvent: function(event) {
    var element = event.target;
    is(element.value, this.expected, this.name);
    switch (element) {
      case kUrlBarElm:
        continue_test();
      case kSearchBarElm:
        finish_test();
    }
  }
}

// test bug 23485 and bug 321000
// urlbar should strip newlines,
// search bar should replace newlines with spaces
function test() {
  waitForExplicitFinish();

  // register listeners
  kUrlBarElm.addEventListener("input", listener, true);
  kSearchBarElm.addEventListener("input", listener, true);

  // Put a multi-line string in the clipboard
  Components.classes["@mozilla.org/widget/clipboardhelper;1"]
            .getService(Components.interfaces.nsIClipboardHelper)
            .copyString(kTestString);

  // Setting the clipboard value is an async OS operation, so we need to poll
  // the clipboard for valid data before going on.
  setTimeout(poll_clipboard, 100);
}

var runCount = 0;
function poll_clipboard() {
  // Poll for a maximum of 5s
  if (++runCount > 50) {
    // Log the failure
    ok(false, "Timed out while polling clipboard for pasted data");
    // Cleanup and interrupt the test
    finish_test();
    return;
  }

  var clip = Components.classes["@mozilla.org/widget/clipboard;1"].
             getService(Components.interfaces.nsIClipboard);
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].
              createInstance(Components.interfaces.nsITransferable);
  trans.addDataFlavor("text/unicode");
  var str = new Object();
  try {
    // This code could throw if the clipboard is not set
    clip.getData(trans,clip.kGlobalClipboard);
    trans.getTransferData("text/unicode",str,{});
    str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
  } catch (ex) {}

  if (kTestString == str) {
    testPaste('urlbar strips newlines and surrounding whitespace',
              kUrlBarElm,
              kTestString.replace(/\s*\n\s*/g,''));
  }
  else
    setTimeout(poll_clipboard, 100);
}

function continue_test() {
  testPaste('searchbar replaces newlines with spaces', 
            kSearchBarElm,
            kTestString.replace('\n',' ','g'));
}

function finish_test() {
  kUrlBarElm.removeEventListener("input", listener, true);
  kSearchBarElm.removeEventListener("input", listener, true);
  // Clear the clipboard, emptyClipboard would not clear the native one, so
  // setting it to an empty string.
  Components.classes["@mozilla.org/widget/clipboardhelper;1"]
            .getService(Components.interfaces.nsIClipboardHelper)
            .copyString("");
  // Clear fields
  kUrlBarElm.value="";
  kSearchBarElm.value="";
  finish();
}
