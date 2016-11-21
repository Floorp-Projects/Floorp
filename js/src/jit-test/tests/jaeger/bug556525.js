// |jit-test| need-for-each

for each(x in [new Number])
    x.__proto__ = []
++x[x]

// don't assert
