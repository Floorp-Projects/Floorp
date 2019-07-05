const { parseKeyValuePairsFromFile } = ChromeUtils.import(
  "resource://gre/modules/KeyValueParser.jsm"
);
var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

var success = false;
var observerFired = false;

var testObserver = {
  idleHang: true,

  observe(subject, topic, data) {
    observerFired = true;
    ok(true, "Observer fired");
    is(topic, "plugin-crashed", "Checking correct topic");
    is(data, null, "Checking null data");
    ok(subject instanceof Ci.nsIPropertyBag2, "got Propbag");
    ok(subject instanceof Ci.nsIWritablePropertyBag2, "got writable Propbag");

    var pluginId = subject.getPropertyAsAString("pluginDumpID");
    isnot(pluginId, "", "got a non-empty plugin crash id");

    // check plugin dump and extra files
    let profD = Services.dirsvc.get("ProfD", Ci.nsIFile);
    profD.append("minidumps");
    let pluginDumpFile = profD.clone();
    pluginDumpFile.append(pluginId + ".dmp");
    ok(pluginDumpFile.exists(), "plugin minidump exists");

    let pluginExtraFile = profD.clone();
    pluginExtraFile.append(pluginId + ".extra");
    ok(pluginExtraFile.exists(), "plugin extra file exists");

    let extraData = parseKeyValuePairsFromFile(pluginExtraFile);

    // check additional dumps

    ok(
      "additional_minidumps" in extraData,
      "got field for additional minidumps"
    );
    let additionalDumps = extraData.additional_minidumps.split(",");
    ok(additionalDumps.includes("browser"), "browser in additional_minidumps");

    for (let name of additionalDumps) {
      let file = profD.clone();
      file.append(pluginId + "-" + name + ".dmp");
      ok(file.exists(), "additional dump '" + name + "' exists");
    }

    // check cpu usage field

    ok("PluginCpuUsage" in extraData, "got extra field for plugin cpu usage");
    let cpuUsage = parseFloat(extraData.PluginCpuUsage);
    if (this.idleHang) {
      ok(cpuUsage == 0, "plugin cpu usage is 0%");
    } else {
      ok(cpuUsage > 0, "plugin cpu usage is >0%");
    }
  },

  QueryInterface: ChromeUtils.generateQI([
    "nsIObserver",
    "nsISupportsWeakReference",
  ]),
};

function onPluginCrashed(aEvent) {
  ok(true, "Plugin crashed notification received");
  ok(observerFired, "Observer should have fired first");
  is(aEvent.type, "PluginCrashed", "event is correct type");

  var pluginElement = document.getElementById("plugin1");
  is(
    pluginElement,
    aEvent.target,
    "Plugin crashed event target is plugin element"
  );

  ok(
    aEvent instanceof PluginCrashedEvent,
    "plugin crashed event has the right interface"
  );

  is(typeof aEvent.pluginDumpID, "string", "pluginDumpID is correct type");
  isnot(aEvent.pluginDumpID, "", "got a non-empty dump ID");
  is(typeof aEvent.pluginName, "string", "pluginName is correct type");
  is(aEvent.pluginName, "Test Plug-in", "got correct plugin name");
  is(typeof aEvent.pluginFilename, "string", "pluginFilename is correct type");
  isnot(aEvent.pluginFilename, "", "got a non-empty filename");
  // The app itself may or may not have decided to submit the report, so
  // allow either true or false here.
  ok(
    "submittedCrashReport" in aEvent,
    "submittedCrashReport is a property of event"
  );
  is(
    typeof aEvent.submittedCrashReport,
    "boolean",
    "submittedCrashReport is correct type"
  );

  Services.obs.removeObserver(testObserver, "plugin-crashed");

  SimpleTest.finish();
}
