/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

const HAVE_TM = 'tracemonkey' in this;

const HOTLOOP = HAVE_TM ? tracemonkey.HOTLOOP : 2;
const RECORDLOOP = HOTLOOP;
const RUNLOOP = HOTLOOP + 1;

var checkStats;
if (HAVE_TM) {
    checkStats = function(stats)
    {
        function jit(on)
        {
          if (on && !options().match(/jit/))
          {
            options('jit');
          }
          else if (!on && options().match(/jit/))
          {
            options('jit');
          }
        }

        jit(false);
        for (var name in stats) {
            var expected = stats[name];
            var actual = tracemonkey[name];
            if (expected != actual) {
                print('Trace stats check failed: got ' + actual + ', expected ' + expected + ' for ' + name);
            }
        }
        jit(true);
    };
} else {
    checkStats = function() {};
}

var appendToActual = function(s) {
    actual += s + ',';
}
