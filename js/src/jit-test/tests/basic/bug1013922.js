String.prototype.repeat = function(num) {
    return new Array(num + 1).join(this);
}

function set_to_length(length, frag_size)
{
    var fragment = "'" + "x".repeat(frag_size) + "' + ";
    var frags = Math.floor((length - 1)/frag_size);
    var code = "var x = " + fragment.repeat(frags) + "'" +
        "x".repeat(length - frags * frag_size) + "';";

    try {
        eval(code);
    }
    catch(err) {
        if (err.message && err.message == "Out of memory")
            return -1;
        if (err == "out of memory")
            return -1;
        throw(err); /* Oops, broke something. */
    }

    return code.length;
}

var first_fail;
var first_pass;
var frag_size;
var pass_code_length;

function search_up()
{
    if (set_to_length(first_fail, frag_size) < 0) {
        setTimeout(binary_search, 0);
        return;
    }

    first_fail *= 2;
}

function binary_search()
{
    if (first_fail - first_pass > 1) {
        var length = (first_pass + first_fail) / 2;
        var code_len = set_to_length(length, frag_size);
        if (code_len > 0) {
            first_pass = length;
            pass_code_length = code_len;
        } else
            first_fail = length;
        setTimeout(binary_search, 0);
        return;
    }
}

function run_find_limit()
{
    frag_size = 64;
    first_fail = 65536; /* A guess */
    first_pass = 0;
    pass_code_length = 0;

    for (var i=0; i<5; i++)
	search_up(0);
}

run_find_limit();
