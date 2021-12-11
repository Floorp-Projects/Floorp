// |jit-test| skip-if: !('oomTest' in this)
o0=r=/x/;
this.toString=(function() {
    evaluate("",({ element:o0 }));
})
oomTest(String.prototype.charCodeAt,{ keepFailing:true })
