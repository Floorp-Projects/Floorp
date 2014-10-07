/**
 * Delegates "is" evaluation back to main thread.
 */
function is(actual, expected, message) {
  var rtnObj = new Object();
  rtnObj.actual = actual;
  rtnObj.expected = expected;
  rtnObj.message = message;
  postMessage(rtnObj);
}

/**
 * Tries to write to property.
 */
function writeProperty(file, property) {
  try {
    var oldValue = file[property];
    file[property] = -1;
    is(false, true, "Should have thrown an exception setting a read only property.");
  } catch (ex) {
    is(true, true, "Should have thrown an exception setting a read only property.");
  }
}

/**
 * Passes junk arguments to FileReaderSync methods and expects an exception to
 * be thrown.
 */
function fileReaderJunkArgument(blob) {
  var fileReader = new FileReaderSync();

  try {
    fileReader.readAsBinaryString(blob);
    is(false, true, "Should have thrown an exception calling readAsBinaryString.");
  } catch(ex) {
    is(true, true, "Should have thrown an exception.");
  }

  try {
    fileReader.readAsDataURL(blob);
    is(false, true, "Should have thrown an exception calling readAsDataURL.");
  } catch(ex) {
    is(true, true, "Should have thrown an exception.");
  }

  try {
    fileReader.readAsArrayBuffer(blob);
    is(false, true, "Should have thrown an exception calling readAsArrayBuffer.");
  } catch(ex) {
    is(true, true, "Should have thrown an exception.");
  }

  try {
    fileReader.readAsText(blob);
    is(false, true, "Should have thrown an exception calling readAsText.");
  } catch(ex) {
    is(true, true, "Should have thrown an exception.");
  }
}

onmessage = function(event) {
  var file = event.data;

  // Test read only properties.
  writeProperty(file, "size");
  writeProperty(file, "type");
  writeProperty(file, "name");
  writeProperty(file, "mozFullPath");

  // Bad types.
  fileReaderJunkArgument(undefined);
  fileReaderJunkArgument(-1);
  fileReaderJunkArgument(1);
  fileReaderJunkArgument(new Object());
  fileReaderJunkArgument("hello");

    // Post undefined to indicate that testing has finished.
  postMessage(undefined);
};
