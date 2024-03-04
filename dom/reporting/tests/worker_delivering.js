fetch("delivering.sjs?task=header&worker=true")
  .then(r => r.text())
  .then(() => {
    postMessage("All good!");
  });
