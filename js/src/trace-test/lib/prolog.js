/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

const HAVE_TM = 'tracemonkey' in this;

const HOTLOOP = HAVE_TM ? tracemonkey.HOTLOOP : 8;
const RECORDLOOP = HOTLOOP;
const RUNLOOP = HOTLOOP + 1;

var checkStats;
if (HAVE_TM) {
    checkStats = function(stats)
    {
        // Temporarily disabled while we work on heuristics.
        return;
        function jit(on)
        {
          if (on && !options().match(/tracejit/))
          {
            options('tracejit');
          }
          else if (!on && options().match(/tracejit/))
          {
            options('tracejit');
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

if (!("gczeal" in this)) {
  gczeal = function() { }
}

