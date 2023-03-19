import("./empty-worklet-script.js")
  .then(() => {
    console.log("Fail");
  })
  .catch(e => {
    console.log(e.name + ": Success");
  });
