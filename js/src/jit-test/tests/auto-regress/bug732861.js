// |jit-test| error:ReferenceError

// Binary: cache/js-dbg-64-1fd6c40d3852-linux
// Flags: --ion-eager
//

j = 0;
out1:
for ((exception) = 0; ; j++)
    if (j == 50)
        break out1;
while (dbgeval("[]").proto ? g : 50) {}
