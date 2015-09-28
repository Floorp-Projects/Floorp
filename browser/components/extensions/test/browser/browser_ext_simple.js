add_task(function* test_simple() {
  let extension = ExtensionTestUtils.loadExtension("simple");
  info("load complete");
  yield extension.startup();
  info("startup complete");
  yield extension.unload();
  info("extension unloaded successfully");
});

add_task(function* test_background() {
  let extension = ExtensionTestUtils.loadExtension("background");
  info("load complete");
  let [, x] = yield Promise.all([extension.startup(), extension.awaitMessage("running")]);
  is(x, 1, "got correct value from extension");
  info("startup complete");
  extension.sendMessage(10, 20);
  yield extension.awaitFinish();
  info("test complete");
  yield extension.unload();
  info("extension unloaded successfully");
});
