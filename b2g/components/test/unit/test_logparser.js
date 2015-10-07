/* jshint moz: true */

var {utils: Cu, classes: Cc, interfaces: Ci} = Components;

function debug(msg) {
  var timestamp = Date.now();
  dump("LogParser: " + timestamp + ": " + msg + "\n");
}

function run_test() {
  Cu.import("resource:///modules/LogParser.jsm");
  debug("Starting");
  run_next_test();
}

function makeStream(file) {
  var fileStream = Cc["@mozilla.org/network/file-input-stream;1"]
                .createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, -1, -1, 0);
  var bis = Cc["@mozilla.org/binaryinputstream;1"]
                .createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fileStream);
  return bis;
}

add_test(function test_parse_logfile() {
  let loggerFile = do_get_file("data/test_logger_file");

  let loggerStream = makeStream(loggerFile);

  // Initialize arrays to hold the file contents (lengths are hardcoded)
  let loggerArray = new Uint8Array(loggerStream.readByteArray(4037));

  loggerStream.close();

  let logMessages = LogParser.parseLogArray(loggerArray);

  ok(logMessages.length === 58, "There should be 58 messages in the log");

  let expectedLogEntry = {
    processId: 271, threadId: 271,
    seconds: 790796, nanoseconds: 620000001, time: 790796620.000001,
    priority: 4, tag: "Vold",
    message: "Vold 2.1 (the revenge) firing up\n"
  };

  deepEqual(expectedLogEntry, logMessages[0]);
  run_next_test();
});

add_test(function test_print_properties() {
  let properties = {
    "ro.secure": "1",
    "sys.usb.state": "diag,serial_smd,serial_tty,rmnet_bam,mass_storage,adb"
  };

  let logMessagesRaw = LogParser.prettyPrintPropertiesArray(properties);
  let logMessages = new TextDecoder("utf-8").decode(logMessagesRaw);
  let logMessagesArray = logMessages.split("\n");

  ok(logMessagesArray.length === 3, "There should be 3 lines in the log.");
  notEqual(logMessagesArray[0], "", "First line should not be empty");
  notEqual(logMessagesArray[1], "", "Second line should not be empty");
  equal(logMessagesArray[2], "", "Last line should be empty");

  let expectedLog = [
    "[ro.secure]: [1]",
    "[sys.usb.state]: [diag,serial_smd,serial_tty,rmnet_bam,mass_storage,adb]",
    ""
  ].join("\n");

  deepEqual(expectedLog, logMessages);

  run_next_test();
});
