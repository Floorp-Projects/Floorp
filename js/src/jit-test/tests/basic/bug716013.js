f = (function() {
    for (x in [arguments, arguments]) yield(gczeal(4, function(){}))
})
for (i in f()) {}
