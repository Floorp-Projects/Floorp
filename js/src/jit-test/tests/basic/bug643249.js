{
    function x() {}
}
for (i = 0; i < 10; i++) {
    _someglobal_ = /a/;
    (function() {
        return function() {
            return _someglobal_
        } ()
    } () == /a/);
    gc();
    try { _someglobal_ = new Function.__lookupSetter__ } catch (e) {}
}
