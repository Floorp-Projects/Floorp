// |reftest| skip-if(!this.hasOwnProperty("oomTest"))

oomTest(function() {
    grayRoot();
    gczeal(8);
    gcslice(this);
});

this.reportCompare && reportCompare(true, true, 'An OOM while gray buffering should not leave the GC half-done');
