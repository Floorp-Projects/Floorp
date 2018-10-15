this.selection = (function () {let exports={}; class Selection {
  constructor(x1, y1, x2, y2) {
    this.x1 = x1;
    this.y1 = y1;
    this.x2 = x2;
    this.y2 = y2;
  }

  get top() {
    return Math.min(this.y1, this.y2);
  }
  set top(val) {
    if (this.y1 < this.y2) {
      this.y1 = val;
    } else {
      this.y2 = val;
    }
  }

  get bottom() {
    return Math.max(this.y1, this.y2);
  }
  set bottom(val) {
    if (this.y1 > this.y2) {
      this.y1 = val;
    } else {
      this.y2 = val;
    }
  }

  get left() {
    return Math.min(this.x1, this.x2);
  }
  set left(val) {
    if (this.x1 < this.x2) {
      this.x1 = val;
    } else {
      this.x2 = val;
    }
  }

  get right() {
    return Math.max(this.x1, this.x2);
  }
  set right(val) {
    if (this.x1 > this.x2) {
      this.x1 = val;
    } else {
      this.x2 = val;
    }
  }

  get width() {
    return Math.abs(this.x2 - this.x1);
  }
  get height() {
    return Math.abs(this.y2 - this.y1);
  }

  rect() {
    return {
      top: Math.floor(this.top),
      left: Math.floor(this.left),
      bottom: Math.floor(this.bottom),
      right: Math.floor(this.right)
    };
  }

  union(other) {
    return new Selection(
      Math.min(this.left, other.left),
      Math.min(this.top, other.top),
      Math.max(this.right, other.right),
      Math.max(this.bottom, other.bottom)
    );
  }

  /** Sort x1/x2 and y1/y2 so x1<x2, y1<y2 */
  sortCoords() {
    if (this.x1 > this.x2) {
      [this.x1, this.x2] = [this.x2, this.x1];
    }
    if (this.y1 > this.y2) {
      [this.y1, this.y2] = [this.y2, this.y1];
    }
  }

  clone() {
    return new Selection(this.x1, this.y1, this.x2, this.y2);
  }

  toJSON() {
    return {
      left: this.left,
      right: this.right,
      top: this.top,
      bottom: this.bottom
    };
  }

  static getBoundingClientRect(el) {
    if (!el.getBoundingClientRect) {
      // Typically the <html> element or somesuch
      return null;
    }
    const rect = el.getBoundingClientRect();
    if (!rect) {
      return null;
    }
    return new Selection(rect.left, rect.top, rect.right, rect.bottom);
  }
}

if (typeof exports !== "undefined") {
  exports.Selection = Selection;
}

return exports;
})();
null;

