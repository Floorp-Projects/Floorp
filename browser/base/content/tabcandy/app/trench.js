// Class: Trench
// Class for drag-snapping regions; called "trenches" as they are long and narrow.
var Trench = function(element, xory, type, edge) {
	this.el = element;
	this.$el = iQ(this.el);
	this.xory = xory; // either "x" or "y"
	this.type = type; // either "border" or "guide"
	this.edge = edge; // "top", "left", "bottom", or "right"

	this.active = false;
	this.gutter = 15;

	// position is the position that we should snap to
	this.position = 0;
	// radius is how far away we should snap from
	this.radius = 10;
	// active range - this is along the perpendicular axis
	this.range = {min: 0, max: 10000};
};
Trench.prototype = {
	setPosition: function Trench_setPos(position, range) {
		this.position = position;
		
		// optionally, set the range.
		if (range && 'min' in range && 'max' in range) {
			this.range.min = range.min;
			this.range.max = range.max;
		}
		
		// set the appropriate bounds as a rect.
		if ( this.xory == "x" ) // horizontal
			this.rect = new Rect ( this.position - this.radius, this.range.min, 2 * this.radius, this.range.max - this.range.min );
		else
			this.rect = new Rect ( this.range.min, this.position - this.radius, this.range.max - this.range.min, 2 * this.radius );

		this.show(); // DEBUG

	},
	setWithRect: function Trench_setWithRect(rect) {
		if (this.type == "border") {
			// border trenches have a range.
			if (this.xory == "x")
				var range = {min: rect.top - this.gutter, max: rect.top + rect.height + this.gutter};
			else
				var range = {min: rect.left - this.gutter, max: rect.left + rect.width + this.gutter};
			
			if (this.edge == "left")
				this.setPosition(rect.left - this.gutter, range);
			else if (this.edge == "right")
				this.setPosition(rect.left + rect.width + this.gutter, range);
			else if (this.edge == "top")
				this.setPosition(rect.top - this.gutter, range);
			else if (this.edge == "bottom")
				this.setPosition(rect.top + rect.height + this.gutter, range);
		} else if (this.type == "guide") {
			// guide trenches have no range.
			if (this.edge == "left")		
				this.setPosition(rect.left);
			else if (this.edge == "right")
				this.setPosition(rect.left + rect.width);
			else if (this.edge == "top")
				this.setPosition(rect.top);
			else if (this.edge == "bottom")
				this.setPosition(rect.top + rect.height);
		}
	},
	show: function Trench_show() { // DEBUG
		if (!iQ('#showTrenches:checked').length) {
			this.hide();
			return;
		}

		if (!this.visibleTrench)
			this.visibleTrench = iQ("<div/>").css({position: 'absolute', zIndex:-101});
		var visibleTrench = this.visibleTrench;

		if (this.active)
			visibleTrench.css({opacity: 0.5});
		else
			visibleTrench.css({opacity: 0.05});
			
		if (this.type == "border")
			visibleTrench.css({backgroundColor:'red'});
		else
			visibleTrench.css({backgroundColor:'blue'});

		visibleTrench.css(this.rect);
		iQ("body").append(visibleTrench);
	},
	hide: function Trench_hide() {
		if (this.visibleTrench)
			this.visibleTrench.remove();
	},
	rectOverlaps: function Trench_rectOverlaps(rect, assumeConstantSize, keepProportional) {
		var xRange = {min: rect.left, max: rect.left + rect.width};
		var yRange = {min: rect.top, max: rect.top + rect.height};
		
		var edgeToCheck;
		if (this.type == "border") {
			if (this.edge == "left")
				edgeToCheck = "right";
			else if (this.edge == "right")
				edgeToCheck = "left";
			else if (this.edge == "top")
				edgeToCheck = "bottom";
			else if (this.edge == "bottom")
				edgeToCheck = "top";
		} else if (this.type == "guide") {
			edgeToCheck = this.edge;
		}

		rect.adjustedEdge = edgeToCheck;

		switch (edgeToCheck) {
			case "left":
				if (this.ruleOverlaps(rect.left, yRange)) {
					rect.left = this.position;
					return rect;
				}
				break;
			case "right":
				if (this.ruleOverlaps(rect.left + rect.width, yRange)) {
					if (assumeConstantSize) {
						rect.left = this.position - rect.width;
					} else {
						var newWidth = this.position - rect.left;
						if (keepProportional)
							rect.height = rect.height * newWidth / rect.width;
						rect.width = newWidth;
					}
					return rect;
				}
				break;
			case "top":
				if (this.ruleOverlaps(rect.top, xRange)) {
					rect.top = this.position;
					return rect;
				}
				break;
			case "bottom":
				if (this.ruleOverlaps(rect.top + rect.height, xRange)) {
					if (assumeConstantSize) {
						rect.top = this.position - rect.height;
					} else {
						var newHeight = this.position - rect.top;
						if (keepProportional)
							rect.width = rect.width * newHeight / rect.height;
						rect.height = newHeight;
					}
					return rect;
				}
		}
			
		return false;
	},
	ruleOverlaps: function Trench_ruleOverlaps(position, range) {
		return (this.position - this.radius <= position && position <= this.position + this.radius
						&& range.min <= this.range.max && this.range.min <= range.max);
	}
};

// global Trenches
// used to track "trenches" in which the edges will snap.
var Trenches = {
	preferTop: true,
	preferLeft: true,
	trenches: [],
	register: function Trenches_register(element, xory, type, edge) {
		var trench = new Trench(element, xory, type, edge);
		this.trenches.push(trench);
		return trench;
	},
	unregister: function Trenches_unregister(element) {
		for (let i in this.trenches) {
			if (this.trenches[i].el === element) {
				this.trenches[i].hide();
				delete this.trenches[i];
			}
		}
	},
	activateOthersTrenches: function Trenches_activateOthersTrenches(element) {
		this.trenches.forEach(function(t) {
			if (t.el === element)
				return;
			t.active = true;
			t.show(); // debug
		});
	},
	disactivate: function Trenches_disactivate() {
		this.trenches.forEach(function(t) {
			t.active = false;
			t.show();
		});
	},
	snap: function Trenches_snap(rect,assumeConstantSize,keepProportional) {
		var updated = false;
		var updatedX = false;
		var updatedY = false;
		for (let i in this.trenches) {
			var t = this.trenches[i];
			if (!t.active)
				continue;
			// newRect will be a new rect, or false
			var newRect = t.rectOverlaps(rect,assumeConstantSize,keepProportional);

			if (newRect) {
				if (assumeConstantSize && updatedX && updatedY)
					break;
				if (assumeConstantSize && updatedX && (newRect.adjustedEdge == "left"||newRect.adjustedEdge == "right"))
					continue;
				if (assumeConstantSize && updatedY && (newRect.adjustedEdge == "top"||newRect.adjustedEdge == "bottom"))
					continue;
				rect = newRect;
				updated = true;
	
				// if updatedX, we don't need to update x any more.
				if (newRect.adjustedEdge == "left" && this.preferLeft)
					updatedX = true;
				if (newRect.adjustedEdge == "right" && !this.preferLeft)
					updatedX = true;
	
				// if updatedY, we don't need to update x any more.
				if (newRect.adjustedEdge == "top" && this.preferTop)
					updatedY = true;
				if (newRect.adjustedEdge == "bottom" && !this.preferTop)
					updatedY = true;

			}
		}
		if (updated)
			return rect;
		else
			return false;
	},
	show: function Trenches_show() {
		this.trenches.forEach(function(t){
			t.show();
		});
	}
};

iQ('#showTrenches').change(function() {
	Trenches.show();
});