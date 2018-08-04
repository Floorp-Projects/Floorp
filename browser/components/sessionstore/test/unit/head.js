ChromeUtils.import("resource://gre/modules/Services.jsm");
const {OS} = ChromeUtils.import("resource://gre/modules/osfile.jsm", {});
ChromeUtils.defineModuleGetter(this, "SessionStartup",
  "resource:///modules/sessionstore/SessionStartup.jsm");

// Call a function once initialization of SessionStartup is complete
function afterSessionStartupInitialization(cb) {
  info("Waiting for session startup initialization");
  let observer = function() {
    try {
      info("Session startup initialization observed");
      Services.obs.removeObserver(observer, "sessionstore-state-finalized");
      cb();
    } catch (ex) {
      do_throw(ex);
    }
  };
  Services.obs.addObserver(observer, "sessionstore-state-finalized");

  // We need the Crash Monitor initialized for sessionstartup to run
  // successfully.
  ChromeUtils.import("resource://gre/modules/CrashMonitor.jsm");
  CrashMonitor.init();

  // Start sessionstartup initialization.
  SessionStartup.init();
}

// Compress the source file using lz4 and put the result to destination file.
// After that, source file is deleted.
async function writeCompressedFile(source, destination) {
  let s = await OS.File.read(source);
  await OS.File.writeAtomic(destination, s, {compression: "lz4"});
  await OS.File.remove(source);
}
