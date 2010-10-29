// |jit-test| error: TypeError
x = 0
'a'.replace(/a/, x.toLocaleString)

