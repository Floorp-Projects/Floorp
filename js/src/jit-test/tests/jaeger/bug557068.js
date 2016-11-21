// |jit-test| need-for-each

for each(let x in [0, {}, 0, {}]) {
  x.valueOf
}

// don't crash
