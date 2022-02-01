// |jit-test| skip-if: !('oomTest' in this)

/x/;
oomTest(function(){
    offThreadCompileToStencil('');
})
