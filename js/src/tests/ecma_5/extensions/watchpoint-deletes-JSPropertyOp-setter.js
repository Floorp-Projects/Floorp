/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function make_watcher(name) {
    return function (id, oldv, newv) {
        print("watched " + name + "[0]");
    };
}

var o, p;
function f(flag) {
    if (flag) {
        o = arguments;
    } else {
        p = arguments;
        o.watch(0, make_watcher('o'));
        p.watch(0, make_watcher('p'));

        /*
         * Previously, the watchpoint implementation actually substituted its magic setter
         * functions for the setters of shared shapes, and then 1) carefully ignored calls
         * to its magic setter from unrelated objects, and 2) avoided restoring the
         * original setter until all watchpoints on that shape had been removed.
         * 
         * However, when the watchpoint code began using JSObject::changeProperty and
         * js_ChangeNativePropertyAttrs to change shapes' setters, the shape tree code
         * became conscious of the presence of watchpoints, and shared shapes between
         * objects only when their watchpoint nature coincided. Clearing the magic setter
         * from one object's shape would not affect other objects, because the
         * watchpointed and non-watchpointed shapes were distinct if they were shared.
         * 
         * Thus, the first unwatch call must go ahead and fix p's shape, even though a
         * watchpoint exists on the same shape in o. o's watchpoint's presence shouldn't
         * cause 'unwatch' to leave p's magic setter in place.
         */

        /* DropWatchPointAndUnlock would see o's watchpoint, and not change p's property. */
        p.unwatch(0);

        /* DropWatchPointAndUnlock would fix o's property, but not p's; p's setter would be gone. */
        o.unwatch(0);

        /* This would fail to invoke the arguments object's setter. */
        p[0] = 4;

        /* And the formal parameter would not get updated. */
        assertEq(flag, 4);
    }
}

f(true);
f(false);

reportCompare(true, true);
