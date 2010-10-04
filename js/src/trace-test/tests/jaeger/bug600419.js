/* Don't assert. */
(function() {
    var x;
    [1].map(function(){}, x << x);
})()
