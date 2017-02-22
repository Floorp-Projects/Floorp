if (helperThreadCount() == 0)
    quit();
function eval(source) {
    offThreadCompileModule(source);
}
var N = 10000;
var left = repeat_str('(1&', N);
var right = repeat_str(')', N);
var str = 'actual = '.concat(left, '1', right, ';');
eval(str);
function repeat_str(str, repeat_count) {
    var arr = new Array(--repeat_count);
    while (repeat_count != 0) arr[--repeat_count] = str;
    return str.concat.apply(str, arr);
}
