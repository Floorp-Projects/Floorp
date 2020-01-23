function blackboxed(...args) {
  for (const arg of args) {
    arg();
  }
}
