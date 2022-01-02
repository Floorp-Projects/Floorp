// These tests highlight potential differences between the lazy and non-lazy
// parse modes. The delazification process should be able to assert that script
// flags are consistent.

function dead_inner_function_1() {
    (function() {})
}
dead_inner_function_1();

function dead_inner_function_2() {
    if (false) {
        function inner() {}
    }
}
dead_inner_function_2();

function inner_eval_1() {
    eval("");
}
inner_eval_1();

function inner_eval_2() {
    (function() {
        eval("");
    })
}
inner_eval_2();

function inner_eval_3() {
    (() => eval(""));
}
inner_eval_3();

function inner_delete_1() {
    var x;
    (function() { delete x; })
}
inner_delete_1();

function inner_delete_2() {
    var x;
    (() => delete x);
}
inner_delete_2();

function inner_this_1() {
    (() => this);
}
inner_this_1();

function inner_arguments_1() {
    (() => arguments);
}
inner_arguments_1();

function constructor_wrapper_1() {
    return (function() {
        this.initialize.apply(this, arguments);
    });
}
constructor_wrapper_1();

var runonce_lazy_1 = function (){}();

var runonce_lazy_2 = function* (){}();
