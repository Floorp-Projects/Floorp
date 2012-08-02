
const Cc = Components.classes;
const Ci = Components.interfaces;

var success = false;
var observerFired = false;

function parseKeyValuePairs(text) {
  var lines = text.split('\n');
  var data = {};
  for (let i = 0; i < lines.length; i++) {
    if (lines[i] == '')
      continue;

    // can't just .split() because the value might contain = characters
    let eq = lines[i].indexOf('=');
    if (eq != -1) {
      let [key, value] = [lines[i].substring(0, eq),
                          lines[i].substring(eq + 1)];
      if (key && value)
        data[key] = value.replace(/\\n/g, "\n").replace(/\\\\/g, "\\");
    }
  }
  return data;
}

function parseKeyValuePairsFromFile(file) {
  var fstream = Cc["@mozilla.org/network/file-input-stream;1"].
                createInstance(Ci.nsIFileInputStream);
  fstream.init(file, -1, 0, 0);
  var is = Cc["@mozilla.org/intl/converter-input-stream;1"].
           createInstance(Ci.nsIConverterInputStream);
  is.init(fstream, "UTF-8", 1024, Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
  var str = {};
  var contents = '';
  while (is.readString(4096, str) != 0) {
    contents += str.value;
  }
  is.close();
  fstream.close();
  return parseKeyValuePairs(contents);
}


var testObserver = {
  idleHang: true,

  observe: function(subject, topic, data) {
    observerFired = true;
    ok(true, "Observer fired");
    is(topic, "plugin-crashed", "Checking correct topic");
    is(data,  null, "Checking null data");
    ok((subject instanceof Ci.nsIPropertyBag2), "got Propbag");
    ok((subject instanceof Ci.nsIWritablePropertyBag2), "got writable Propbag");

    var pluginId = subject.getPropertyAsAString("pluginDumpID");
    isnot(pluginId, "", "got a non-empty plugin crash id");
    var browserId = subject.getPropertyAsAString("browserDumpID");
    isnot(browserId, "", "got a non-empty browser crash id");
    
    // check dump and extra files
    let directoryService =
      Cc["@mozilla.org/file/directory_service;1"].getService(Ci.nsIProperties);
    let profD = directoryService.get("ProfD", Ci.nsIFile);
    profD.append("minidumps");
    let pluginDumpFile = profD.clone();
    pluginDumpFile.append(pluginId + ".dmp");
    ok(pluginDumpFile.exists(), "plugin minidump exists");
    let browserDumpFile = profD.clone();
    browserDumpFile.append(browserId + ".dmp");
    ok(browserDumpFile.exists(), "browser minidump exists");
    let pluginExtraFile = profD.clone();
    pluginExtraFile.append(pluginId + ".extra");
    ok(pluginExtraFile.exists(), "plugin extra file exists");
    let browserExtraFile = profD.clone();
    browserExtraFile.append(browserId + ".extra");
    ok(pluginExtraFile.exists(), "browser extra file exists");
     
    // check cpu usage field
    let extraData = parseKeyValuePairsFromFile(pluginExtraFile);
    ok("PluginCpuUsage" in extraData, "got extra field for plugin cpu usage");
    let cpuUsage = parseFloat(extraData["PluginCpuUsage"].replace(',', '.'));
    if (this.idleHang) {
      ok(cpuUsage == 0, "plugin cpu usage is 0%");
    } else {
      ok(cpuUsage > 0, "plugin cpu usage is >0%");
    }
    
    // check processor count field
    ok("NumberOfProcessors" in extraData, "got extra field for processor count");
    ok(parseInt(extraData["NumberOfProcessors"]) > 0, "number of processors is >0");

    // cleanup, to be nice
    pluginDumpFile.remove(false);
    browserDumpFile.remove(false);
    pluginExtraFile.remove(false);
    browserExtraFile.remove(false);
  },

  QueryInterface: function(iid) {
    if (iid.equals(Ci.nsIObserver) ||
        iid.equals(Ci.nsISupportsWeakReference) ||
        iid.equals(Ci.nsISupports))
      return this;
    throw Components.results.NS_NOINTERFACE;
  }
};


function onPluginCrashed(aEvent) {
  ok(true, "Plugin crashed notification received");
  ok(observerFired, "Observer should have fired first");
  is(aEvent.type, "PluginCrashed", "event is correct type");

  var pluginElement = document.getElementById("plugin1");
  is (pluginElement, aEvent.target, "Plugin crashed event target is plugin element");

  ok(aEvent instanceof Ci.nsIDOMDataContainerEvent,
     "plugin crashed event has the right interface");

  var minidumpID = aEvent.getData("minidumpID");
  isnot(minidumpID, "", "got a non-empty dump ID");
  var pluginName = aEvent.getData("pluginName");
  is(pluginName, "Test Plug-in", "got correct plugin name");
  var pluginFilename = aEvent.getData("pluginFilename");
  isnot(pluginFilename, "", "got a non-empty filename");
  var didReport = aEvent.getData("submittedCrashReport");
  // The app itself may or may not have decided to submit the report, so
  // allow either true or false here.
  ok((didReport == true || didReport == false), "event said crash report was submitted");

  var os = Cc["@mozilla.org/observer-service;1"].
           getService(Ci.nsIObserverService);
  os.removeObserver(testObserver, "plugin-crashed");

  SimpleTest.finish();
}
