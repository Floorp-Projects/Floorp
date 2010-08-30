const Cc = Components.classes;
const Ci = Components.interfaces;

function createJetpack(args)
{
  var jp = Components.classes["@mozilla.org/jetpack/service;1"].
    getService(Components.interfaces.nsIJetpackService).
    createJetpack();

  if (!args.skipRegisterCleanup)
    do_register_cleanup(function() {
      jp.destroy();
    });

  if (!args.skipRegisterError)
    jp.registerReceiver("core:exception", function(msgName, e) {
      dump("Received exception from remote code: " + uneval(e) + "\n");
      do_check_true(false);
    });

  if (args.scriptFile)
    jp.evalScript(read_file(args.scriptFile));
  
  return jp;
}

const PR_RDONLY = 0x1;

function read_file(f)
{
  var fis = Cc["@mozilla.org/network/file-input-stream;1"]
    .createInstance(Ci.nsIFileInputStream);
  fis.init(f, PR_RDONLY, 0444, Ci.nsIFileInputStream.CLOSE_ON_EOF);

  var lis = Cc["@mozilla.org/intl/converter-input-stream;1"]
    .createInstance(Ci.nsIConverterInputStream);
  lis.init(fis, "UTF-8", 1024, 0);

  var data = "";

  var r = {};
  while (lis.readString(0x0FFFFFFF, r))
    data += r.value;

  return data;
}
