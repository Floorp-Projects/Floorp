var du = new Debugger();
if (typeof du.setupTraceLogger === "function")
    du.setupTraceLogger({Scripts: true});
(function() {
    for (var i = 0; i < 15; ++i) {}
})();
