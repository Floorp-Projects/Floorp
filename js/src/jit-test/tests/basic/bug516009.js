var gFutureCalls = [];

function add_future_call(index, func)
{
    if (!(index in gFutureCalls)) {
        gFutureCalls[index] = [];
    }
}

function check_reset_test(time)
{
}

check_reset_test(0);

for (var i = 1; i <= 8; ++i) {
    (function(j) {
        add_future_call(j, function() { check_reset_test(j); });
    })(i);
}
