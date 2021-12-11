function _build_tree() {
    var $n;
    var $elems = 20;
    while (true) {
        var $tmp18 = $n;
        var $tmp19 = $elems;
        var $cmp = ($n | 0) < ($elems | 0);
        return $cmp;
    }
}
assertEq(_build_tree(), true);

