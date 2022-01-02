try { (function() {
    var o = {};
    with (o) o='recorder not started, ';
    ('arguments' in o, false,
                  "property deletion observable")
})() } catch (e) {}
