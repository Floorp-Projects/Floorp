var checkClipRegion, checkClipRegionForFrame, checkClipRegionNoBounds;

(function() {
var windowFrameX, windowFrameY;

// Import test API
var is = window.opener.is;
var ok = window.opener.ok;
var todo = window.opener.todo;
var finish = window.opener.SimpleTest.finish;
window.onerror = function (event) { window.opener.onerror(event); window.close(); };

function dumpRect(r) {
  return "{" + r.join(",") + "}";
}

function dumpRegion(rects) {
  var s = [];
  for (var i = 0; i < rects.length; ++i) {
    var r = rects[i];
    s.push(dumpRect(r));
  }
  return s.join(", ");
}

function generateSpan(coords) {
  coords.sort(function(a,b) { return a - b; });
  var result = [coords[0]];
  for (var i = 1; i < coords.length; ++i) {
    if (coords[i] != coords[i - 1]) {
      result.push(coords[i]);
    }
  }
  return result;
}

function containsRect(r1, r2) {
  return r1[0] <= r2[0] && r1[2] >= r2[2] &&
         r1[1] <= r2[1] && r1[3] >= r2[3];
}

function subtractRect(r1, r2, rlist) {
  var spanX = generateSpan([r1[0], r1[2], r2[0], r2[2]]);
  var spanY = generateSpan([r1[1], r1[3], r2[1], r2[3]]);
  for (var i = 1; i < spanX.length; ++i) {
    for (var j = 1; j < spanY.length; ++j) {
      var subrect = [spanX[i - 1], spanY[j - 1], spanX[i], spanY[j]];
      if (containsRect(r1, subrect) && !containsRect(r2, subrect)) {
        rlist.push(subrect);
      }
    }
  }
}

function regionContainsRect(rs, r) {
  var rectList = [r];
  for (var i = 0; i < rs.length; ++i) {
    var newList = [];
    for (var j = 0; j < rectList.length; ++j) {
      subtractRect(rectList[j], rs[i], newList);
    }
    if (newList.length == 0)
      return true;
    rectList = newList;
  }
  return false;
}

function regionContains(r1s, r2s) {
  for (var i = 0; i < r2s.length; ++i) {
    if (!regionContainsRect(r1s, r2s[i]))
      return false;
  }
  return true;
}

function equalRegions(r1s, r2s) {
  return regionContains(r1s, r2s) && regionContains(r2s, r1s);
}

// Checks that a plugin's clip region equals the specified rectangle list.
// The rectangles are given relative to the plugin's top-left. They are in
// [left, top, right, bottom] format.
function checkClipRegionWithDoc(doc, offsetX, offsetY, id, rects, checkBounds) {
  var p = doc.getElementById(id);
  var bounds = p.getBoundingClientRect();
  var pX = p.getEdge(0);
  var pY = p.getEdge(1);

  if (checkBounds) {
    var pWidth = p.getEdge(2) - pX;
    var pHeight = p.getEdge(3) - pY;

    is(pX, windowFrameX + bounds.left + offsetX, id + " plugin X");
    is(pY, windowFrameY + bounds.top + offsetY, id + " plugin Y");
    is(pWidth, bounds.width, id + " plugin width");
    is(pHeight, bounds.height, id + " plugin height");
  }

  // Now check clip region. 'rects' is relative to the plugin's top-left.
  var clipRects = [];
  var n = p.getClipRegionRectCount();
  for (var i = 0; i < n; ++i) {
    // Convert the clip rect to be relative to the plugin's top-left.
    clipRects[i] = [
      p.getClipRegionRectEdge(i, 0) - pX,
      p.getClipRegionRectEdge(i, 1) - pY,
      p.getClipRegionRectEdge(i, 2) - pX,
      p.getClipRegionRectEdge(i, 3) - pY
    ];
  }

  ok(equalRegions(clipRects, rects), "Matching regions for '" + id +
      "': expected " + dumpRegion(rects) + ", got " + dumpRegion(clipRects));
}

checkClipRegion = function checkClipRegion(id, rects) {
  checkClipRegionWithDoc(document, 0, 0, id, rects, true);
}

checkClipRegionForFrame = function checkClipRegionForFrame(fid, id, rects) {
  var f = document.getElementById(fid);
  var bounds = f.getBoundingClientRect();
  checkClipRegionWithDoc(f.contentDocument, bounds.left, bounds.top, id, rects, true);
}

checkClipRegionNoBounds = function checkClipRegionNoBounds(id, rects) {
  checkClipRegionWithDoc(document, 0, 0, id, rects, false);
}

function loaded() {
  var h1 = document.getElementById("h1");
  var h2 = document.getElementById("h2");
  var hwidth = h2.boxObject.screenX - h1.boxObject.screenX;
  if (hwidth != 100) {
    // Maybe it's a DPI issue
    todo(false, "Unexpected DPI?");
    finish();
    window.close();
    return;
  }

  if (!document.getElementById("p1").identifierToStringTest) {
    todo(false, "Test plugin not available");
    finish();
    window.close();
    return;
  }

  var bounds = h1.getBoundingClientRect();
  windowFrameX = h1.boxObject.screenX - bounds.left - window.screenX;
  windowFrameY = h1.boxObject.screenY - bounds.top - window.screenY;

  // Run actual test code
  runTests();
}

// Need to run 'loaded' after painting is unsuppressed, or we'll set clip
// regions to empty.  The timeout must be non-zero on X11 so that
// gtk_container_idle_sizer runs after the GtkSocket gets the plug_window.
window.addEventListener("load",
                        function () { setTimeout(loaded, 1000); }, false);
})();
