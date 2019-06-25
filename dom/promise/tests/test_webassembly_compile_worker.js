const sampleURL = "test_webassembly_compile_sample.wasm";
const sampleExportName = "run";
const sampleResult = 1275;

/* eslint-disable no-throw-literal */

function checkSampleModule(m) {
  if (!(m instanceof WebAssembly.Module))
    throw "not a module";
  var i = new WebAssembly.Instance(m);
  if (!(i instanceof WebAssembly.Instance))
    throw "not an instance";
  if (i.exports[sampleExportName]() !== sampleResult)
    throw "wrong result";
}

function checkSampleInstance(i) {
  if (!(i instanceof WebAssembly.Instance))
    throw "not an instance";
  if (i.exports[sampleExportName]() !== sampleResult)
    throw "wrong result";
}

const initObj = { headers: { "Content-Type": "application/wasm" } };

onmessage = e => {
  WebAssembly.compile(e.data)
  .then(m => checkSampleModule(m))
  .then(() => WebAssembly.instantiate(e.data))
  .then(({module, instance}) => { checkSampleModule(module); checkSampleInstance(instance); })
  .then(() => WebAssembly.compileStreaming(new Response(e.data, initObj)))
  .then(m => checkSampleModule(m))
  .then(() => WebAssembly.instantiateStreaming(new Response(e.data, initObj)))
  .then(({module, instance}) => { checkSampleModule(module); checkSampleInstance(instance); })
  .then(() => WebAssembly.compileStreaming(fetch(sampleURL)))
  .then(m => checkSampleModule(m))
  .then(() => WebAssembly.instantiateStreaming(fetch(sampleURL)))
  .then(({module, instance}) => { checkSampleModule(module); checkSampleInstance(instance); })
  .then(() => postMessage("ok"))
  .catch(err => postMessage("fail: " + err));
};
