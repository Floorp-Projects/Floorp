function test() {
  waitForExplicitFinish();
  testNext();
}

let pairs = [
  ["javascript:", ""],
  ["javascript:1+1", "1+1"],
  ["javascript:document.domain", "document.domain"],
  ["data:text/html,<body>hi</body>", "text/html,<body>hi</body>"],
  // Nested things get confusing because some things don't parse as URIs:
  ["javascript:javascript:alert('hi!')", "alert('hi!')"],
  ["data:data:text/html,<body>hi</body>", "text/html,<body>hi</body>"],
  ["javascript:data:javascript:alert('hi!')", "data:javascript:alert('hi!')"],
  ["javascript:data:text/html,javascript:alert('hi!')", "text/html,javascript:alert('hi!')"],
  ["data:data:text/html,javascript:alert('hi!')", "text/html,javascript:alert('hi!')"],
];

let clipboardHelper = Cc["@mozilla.org/widget/clipboardhelper;1"].getService(Ci.nsIClipboardHelper);

function paste(input, cb) {
  waitForClipboard(input, function() {
    clipboardHelper.copyString(input);
  }, function() {
    document.commandDispatcher.getControllerForCommand("cmd_paste").doCommand("cmd_paste");
    cb();
  }, function() {
    ok(false, "Failed to copy string '" + input + "' to clipboard");
    cb();
  });
}

function testNext() {
  gURLBar.value = '';
  if (!pairs.length) {
    finish();
    return;
  }

  let [inputValue, expectedURL] = pairs.shift();

  gURLBar.focus();
  paste(inputValue, function() {
    is(gURLBar.value, expectedURL, "entering '" + inputValue + "' strips relevant bits.");

    setTimeout(testNext, 0);
  });
}

