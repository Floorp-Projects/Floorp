// |jit-test| need-for-each

for each(let w in [[], 0, [], 0]) {
  w.unwatch()
}
