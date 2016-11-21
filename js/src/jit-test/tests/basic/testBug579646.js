// |jit-test| need-for-each

if (typeof gczeal != "function")
    gczeal = function() {}

for (a = 0; a < 9; a++)
    for (b = 0; b < 1; b++)
        for (c = 0; c < 2; c++)
            gczeal();

for each(e in [NaN])
    for (d = 0; d < 1; d++)
        z = 0;

for (w in [0, 0])
    {}

x = 0;

for (e = 0; e < 3; e++)
    for (f = 0; f < 4; f++)
        x = -x

// don't crash
