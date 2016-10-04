w = new Worker("rsf_csp_worker.js");
// use the handler function in test_require-sri-for_csp_directive.html
w.onmessage = parent.handler;
