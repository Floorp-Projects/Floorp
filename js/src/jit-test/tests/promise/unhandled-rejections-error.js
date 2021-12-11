// |jit-test| error:Error while printing unhandled rejection

Promise.reject({ toSource() { throw "another error"; } });
