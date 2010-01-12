actual = '';
expected = 'nocrash,';

function jQuery(a, c) {
}
jQuery.fn = {};
(function() {
    var e =
["ajaxStart","ajaxStop","ajaxComplete","ajaxError","ajaxSuccess","ajaxSend"];
    for (var i = 0; i < e.length; i++) {
        new function() {
            var o = e[i];
            jQuery.fn[o] = function(f) {
                return this.bind(o, f);
            }
        };
    }
})();


appendToActual('nocrash')


assertEq(actual, expected)
