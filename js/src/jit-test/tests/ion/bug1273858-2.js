// |jit-test| --no-threads

function t1() {
    let x = [];

    try
    {
        for (let k = 0; k < 100; ++k)
        {
            let w = () => k; // Lexical capture

            if (w() > 10)
            {
                throw () => w; // Lexical capture
            }

            x[k] = w;
        }
    }
    catch (e)
    {
        // 'w' and 'k' should leave scope as exception unwinds

        try {
            eval("k");
            throw false;
        }
        catch (e) {
            if (!(e instanceof ReferenceError))
                throw "Loop index escaped block";
        }

        try {
            eval("w");
            throw false;
        }
        catch (e) {
            if (!(e instanceof ReferenceError))
                throw "Local name escaped block";
        }
    }
}
t1();
t1();
t1();
t1();
