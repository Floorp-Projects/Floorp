try {
  offThreadCompileScript('Error()', { lineNumber: (4294967295)});
  runOffThreadScript().stack;
} catch (e) {
  // Ignore "Error: Can't use offThreadCompileScript with --no-threads"
}
