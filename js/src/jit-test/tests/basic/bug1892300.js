newGlobal().eval(`(function () {
  enableShellAllocationMetadataBuilder();
  return arguments[Symbol.iterator];
})();`);
