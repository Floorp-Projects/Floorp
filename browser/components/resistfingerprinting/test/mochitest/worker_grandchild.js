self.addEventListener("message", function(event) {
  let timeStampCodes = event.data;

  let timeStamps = [];
  for (let timeStampCode of timeStampCodes) {
    timeStamps.push(eval(timeStampCode));
  }
  // Send the timeStamps to the parent.
  postMessage(timeStamps);
});
