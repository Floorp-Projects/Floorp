// |jit-test| error: ReferenceError
with({}) {
  let x = x += undefined
}
