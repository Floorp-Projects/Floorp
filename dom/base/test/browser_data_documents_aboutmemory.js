add_task(async function() {
  const doc = new DOMParser().parseFromString("<p>dadada</p>", "text/html");

  let mgr = Cc["@mozilla.org/memory-reporter-manager;1"].getService(
    Ci.nsIMemoryReporterManager
  );

  let amount = 0;
  const handleReport = (aProcess, aPath, aKind, aUnits, aAmount) => {
    const regex = new RegExp(".*/window-objects/.*/data-documents/.*");
    if (regex.test(aPath)) {
      amount += aAmount;
    }
  };

  await new Promise(r =>
    mgr.getReports(handleReport, null, r, null, /* anonymized = */ false)
  );
  ok(amount > 0, "Got data documents amount");
});
