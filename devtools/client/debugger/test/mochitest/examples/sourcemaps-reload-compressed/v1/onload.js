
/**
 * Load the same url in many ways
 */
// 1) Notice in index.html the:
// <script src="same-url.sjs"></script>

// 2) Also the similar <script> tag in iframe.html

// 3) Via a Worker, in a distinct target
const worker = new Worker("same-url.sjs");
worker.postMessage("foo");

// 4) Via a named eval, in the same target
// Hold a global reference to this function to avoid it from being GC-ed.
this.evaled = eval("function sameUrlEval() {}; console.log('eval script'); //# sourceURL=same-url.sjs");

// 5) Via a dynamically inserted <script>, in the same target
const script = document.createElement("script");
script.src = "same-url.sjs";
document.body.appendChild(script);
