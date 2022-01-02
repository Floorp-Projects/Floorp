var { Services } = ChromeUtils.import("resource://gre/modules/Services.jsm");

ChromeUtils.defineModuleGetter(
  this,
  "SessionStartup",
  "resource:///modules/sessionstore/SessionStartup.jsm"
);

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
  const { CrashMonitor } = ChromeUtils.import(
    "resource://gre/modules/CrashMonitor.jsm"
  );
  CrashMonitor.init();

  // Start sessionstartup initialization.
  SessionStartup.init();
}

// Compress the source file using lz4 and put the result to destination file.
// After that, source file is deleted.
async function writeCompressedFile(source, destination) {
  let s = await IOUtils.read(source);
  await IOUtils.write(destination, s, { compress: true });
  await IOUtils.remove(source);
}
