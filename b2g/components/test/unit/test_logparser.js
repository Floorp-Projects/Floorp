/* jshint moz: true */

const {utils: Cu, classes: Cc, interfaces: Ci} = Components;

function run_test() {
  Cu.import('resource:///modules/LogParser.jsm');

  let propertiesFile = do_get_file('data/test_properties');
  let loggerFile = do_get_file('data/test_logger_file');

  let propertiesStream = makeStream(propertiesFile);
  let loggerStream = makeStream(loggerFile);

  // Initialize arrays to hold the file contents (lengths are hardcoded)
  let propertiesArray = new Uint8Array(propertiesStream.readByteArray(65536));
  let loggerArray = new Uint8Array(loggerStream.readByteArray(4037));

  propertiesStream.close();
  loggerStream.close();

  let properties = LogParser.parsePropertiesArray(propertiesArray);
  let logMessages = LogParser.parseLogArray(loggerArray);

  // Test arbitrary property entries for correctness
  equal(properties['ro.boot.console'], 'ttyHSL0');
  equal(properties['net.tcp.buffersize.lte'],
        '524288,1048576,2097152,262144,524288,1048576');

  ok(logMessages.length === 58, 'There should be 58 messages in the log');

  let expectedLogEntry = {
    processId: 271, threadId: 271,
    seconds: 790796, nanoseconds: 620000001, time: 790796620.000001,
    priority: 4, tag: 'Vold',
    message: 'Vold 2.1 (the revenge) firing up\n'
  };

  deepEqual(expectedLogEntry, logMessages[0]);
}

function makeStream(file) {
  var fileStream = Cc['@mozilla.org/network/file-input-stream;1']
                .createInstance(Ci.nsIFileInputStream);
  fileStream.init(file, -1, -1, 0);
  var bis = Cc['@mozilla.org/binaryinputstream;1']
                .createInstance(Ci.nsIBinaryInputStream);
  bis.setInputStream(fileStream);
  return bis;
}
