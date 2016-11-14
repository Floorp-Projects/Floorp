/**
 * Returns a Promise that resolves once a crash report has
 * been submitted. This function will also test the crash
 * reports extra data to see if it matches expectedExtra.
 *
 * @param expectedExtra (object)
 *        An Object whose key-value pairs will be compared
 *        against the key-value pairs in the extra data of the
 *        crash report. A test failure will occur if there is
 *        a mismatch.
 *
 *        If the value of the key-value pair is "null", this will
 *        be interpreted as "this key should not be included in the
 *        extra data", and will cause a test failure if it is detected
 *        in the crash report.
 *
 *        Note that this will ignore any keys that are not included
 *        in expectedExtra. It's possible that the crash report
 *        will contain other extra information that is not
 *        compared against.
 * @returns Promise
 */
function promiseCrashReport(expectedExtra={}) {
  return Task.spawn(function*() {
    info("Starting wait on crash-report-status");
    let [subject, ] =
      yield TestUtils.topicObserved("crash-report-status", (unused, data) => {
        return data == "success";
      });
    info("Topic observed!");

    if (!(subject instanceof Ci.nsIPropertyBag2)) {
      throw new Error("Subject was not a Ci.nsIPropertyBag2");
    }

    let remoteID = getPropertyBagValue(subject, "serverCrashID");
    if (!remoteID) {
      throw new Error("Report should have a server ID");
    }

    let file = Cc["@mozilla.org/file/local;1"]
                 .createInstance(Ci.nsILocalFile);
    file.initWithPath(Services.crashmanager._submittedDumpsDir);
    file.append(remoteID + ".txt");
    if (!file.exists()) {
      throw new Error("Report should have been received by the server");
    }

    file.remove(false);

    let extra = getPropertyBagValue(subject, "extra");
    if (!(extra instanceof Ci.nsIPropertyBag2)) {
      throw new Error("extra was not a Ci.nsIPropertyBag2");
    }

    info("Iterating crash report extra keys");
    let enumerator = extra.enumerator;
    while (enumerator.hasMoreElements()) {
      let key = enumerator.getNext().QueryInterface(Ci.nsIProperty).name;
      let value = extra.getPropertyAsAString(key);
      if (key in expectedExtra) {
        if (expectedExtra[key] == null) {
          ok(false, `Got unexpected key ${key} with value ${value}`);
        } else {
          is(value, expectedExtra[key],
             `Crash report had the right extra value for ${key}`);
        }
      }
    }
  });
}


/**
 * For an nsIPropertyBag, returns the value for a given
 * key.
 *
 * @param bag
 *        The nsIPropertyBag to retrieve the value from
 * @param key
 *        The key that we want to get the value for from the
 *        bag
 * @returns The value corresponding to the key from the bag,
 *          or null if the value could not be retrieved (for
 *          example, if no value is set at that key).
*/
function getPropertyBagValue(bag, key) {
  try {
    let val = bag.getProperty(key);
    return val;
  } catch (e) {
    if (e.result != Cr.NS_ERROR_FAILURE) {
      throw e;
    }
  }

  return null;
}
