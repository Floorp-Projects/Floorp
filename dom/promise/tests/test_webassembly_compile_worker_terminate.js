const sampleURL = "test_webassembly_compile_sample.wasm";

function spawnWork() {
  const N = 50;
  var arr = [];
  for (var i = 0; i < N; i++)
    arr.push(WebAssembly.compileStreaming(fetch(sampleURL)));
  Promise.all(arr).then(spawnWork);
}

spawnWork();
postMessage("ok");
