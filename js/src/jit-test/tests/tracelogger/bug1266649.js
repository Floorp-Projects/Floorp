
var du = new Debugger();
if (typeof du.setupTraceLogger === "function" &&
    typeof oomTest === 'function')
{
    du.setupTraceLogger({
        Scripts: true
    })
    oomTest(() => function(){});
}
