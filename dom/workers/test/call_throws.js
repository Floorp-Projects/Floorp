function workerMethod() {
  console.log("workerMethod about to throw...");
  throw new Error("Method-Throw-Payload");
}
