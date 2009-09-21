/*
 * JS region library. This deals in rectangles which are arrays of 4 elements:
 * left, top, right, bottom.
 *
 * The constructor constructs a region from a list of rects. If no list is
 * passed, then we return an empty region.
 */
function Region(rects) {
  this._rects = rects || [];
}

(function() {
function dumpRect(r) {
  return "{" + r.join(",") + "}";
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

function rectContainsRect(r1, r2) {
  return r1[0] <= r2[0] && r1[2] >= r2[2] &&
         r1[1] <= r2[1] && r1[3] >= r2[3];
}

function rectIntersectsRect(r1, r2) {
  return Math.max(r1[0], r2[0]) < Math.min(r1[2], r2[2]) &&
         Math.max(r1[1], r2[1]) < Math.min(r1[3], r2[3]);
}

function subtractRect(r1, r2, rlist) {
  var spanX = generateSpan([r1[0], r1[2], r2[0], r2[2]]);
  var spanY = generateSpan([r1[1], r1[3], r2[1], r2[3]]);
  for (var i = 1; i < spanX.length; ++i) {
    for (var j = 1; j < spanY.length; ++j) {
      var subrect = [spanX[i - 1], spanY[j - 1], spanX[i], spanY[j]];
      if (rectContainsRect(r1, subrect) && !rectContainsRect(r2, subrect)) {
        rlist.push(subrect);
      }
    }
  }
}

// Return the contents of the region as a string
Region.prototype.toString = function Region_toString() {
  var s = [];
  for (var i = 0; i < this._rects.length; ++i) {
    var r = this._rects[i];
    s.push(dumpRect(r));
  }
  return "Region(" + s.join(",") + ")";
};

// Returns true if the region contains the entire rectangle r
Region.prototype.containsRect = function Region_containsRect(r) {
  var rectList = [r];
  for (var i = 0; i < this._rects.length; ++i) {
    var newList = [];
    for (var j = 0; j < rectList.length; ++j) {
      subtractRect(rectList[j], this._rects[i], newList);
    }
    if (newList.length == 0)
      return true;
    rectList = newList;
  }
  return false;
};

Region.prototype.isEmpty = function Region_isEmpty() {
  for (var i = 0; i < this._rects.length; ++i) {
    var r = this._rects[i];
    if (r[0] < r[2] && r[1] < r[3])
      return false;
  }
  return true;
};

Region.prototype.intersectsRect = function Region_intersectsRect(r) {
  for (var i = 0; i < this._rects.length; ++i) {
    if (rectIntersectsRect(this._rects[i], r))
      return true;
  }
  return false;
};

// Returns true if the region contains the entire region rgn
Region.prototype.containsRegion = function Region_containsRegion(rgn) {
  for (var i = 0; i < rgn._rects.length; ++i) {
    if (!this.containsRect(rgn._rects[i]))
      return false;
  }
  return true;
};

// Returns true if the region covers exactly the same area as region rgn
Region.prototype.equalsRegion = function Region_equalsRegion(rgn) {
  return this.containsRegion(rgn) && rgn.containsRegion(this);
};
})();
