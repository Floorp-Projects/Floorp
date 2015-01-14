// |reftest| skip-if(!xulRuntime.shell)
/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/*
 * Any copyright is dedicated to the Public Domain.
 * http://creativecommons.org/licenses/publicdomain/
 */

function garbage() {
  var x;
  for (var i = 0; i < 100000; i++)
    x = { 'i': i };
}

setGCCallback({
  action: "majorGC",
  depth: 1,
  phases: "both"
});

gc();
garbage();

setGCCallback({
  action: "majorGC",
  depth: 2,
  phases: "both"
});

gc();
garbage();

setGCCallback({
  action: "majorGC",
  depth: 10,
  phases: "begin"
});

gc();
garbage();

setGCCallback({
  action: "minorGC",
  phases: "both"
});

gc();
garbage();

var caught = false;
try {
  setGCCallback({
    action: "majorGC",
    depth: 10000,
    phases: "begin"
  });
} catch (e) {
  caught = ((""+e).indexOf("Nesting depth too large") >= 0);
}

reportCompare(caught, true);
