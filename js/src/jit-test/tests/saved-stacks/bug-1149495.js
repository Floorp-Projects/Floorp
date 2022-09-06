try {
  offThreadCompileToStencil('Error()', { lineNumber: (4294967295)});
  var stencil = finishOffThreadStencil();
  evalStencil(stencil).stack;
} catch (e) {
  // Ignore "Error: Can't use offThreadCompileToStencil with --no-threads"
}
