
/**
 * Load the same url in many ways
 */
// 1) Notice in index.html the:
// <script src="same-url.sjs"></script>

// 2) Via a Worker, in a distinct target
const worker = new Worker("same-url.sjs");
worker.postMessage("foo");

// 3) Via a named eval, in the same target
// Hold a global reference to this function to avoid it from being GC-ed.
this.evaled = eval("function sameUrlEval() {}; console.log('eval script'); //# sourceURL=same-url.sjs");

// 4) Via a dynamically inserted <script>, in the same target
const script = document.createElement("script");
script.src = "same-url.sjs";
document.body.appendChild(script);

// 5) Via an iframe, in a distinct target
const iframe = document.createElement("iframe");
const sameUrl = new URL("same-url.sjs", location);
const iframeContent = `<!DOCTYPE html><script src="${sameUrl}"></script>`;
iframe.src = `https://example.org/document-builder.sjs?html=` + encodeURI(iframeContent);
document.body.appendChild(iframe);
