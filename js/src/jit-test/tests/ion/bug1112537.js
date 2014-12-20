if (getBuildConfiguration()['asan'])
    quit();

function split_join_overflow()
{
    try {
        var s = "  ";
        for (var i = 1; i < 10; i++)
            s = s.split("").join(s); // 2^(2^i)
    } catch (exn) {
        if (exn != "out of memory")
            assertEq(exn instanceof InternalError, true);
    };
}

for (var i = 0; i < 5; i++)
   split_join_overflow();
