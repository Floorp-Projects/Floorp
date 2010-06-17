/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is trench.js.
 *
 * The Initial Developer of the Original Code is
 * Michael Yoshitaka Erlewine <mitcho@mitcho.com>.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

// **********
// Title: trench.js 

// Class: Trench
// Class for drag-snapping regions; called "trenches" as they are long and narrow.
var Trench = function(element, xory, type, edge) {
	this.id = Trenches.nextId++;
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
			this.visibleTrench = iQ("<div/>").css({position: 'absolute', zIndex:-102, opacity: 0.05});
		var visibleTrench = this.visibleTrench;

		if (!this.activeVisibleTrench)
			this.activeVisibleTrench = iQ("<div/>").css({position: 'absolute', zIndex:-101});
		var activeVisibleTrench = this.activeVisibleTrench;

		if (this.active)
			activeVisibleTrench.css({opacity: 0.45});
		else
			activeVisibleTrench.css({opacity: 0});
			
		if (this.type == "border") {
			visibleTrench.css({backgroundColor:'red'});
			activeVisibleTrench.css({backgroundColor:'red'});			
		} else {
			visibleTrench.css({backgroundColor:'blue'});
			activeVisibleTrench.css({backgroundColor:'blue'});
		}

		visibleTrench.css(this.rect);
		activeVisibleTrench.css(this.activeRect || this.rect);
		iQ("body").append(visibleTrench);
		iQ("body").append(activeVisibleTrench);
	},
	hide: function Trench_hide() {
		if (this.visibleTrench)
			this.visibleTrench.remove();
	},
	rectOverlaps: function Trench_rectOverlaps(rect,edge,assumeConstantSize,keepProportional) {
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
	nextId: 0,
	preferTop: true,
	preferLeft: true,
	activeTrenches: {},
	trenches: [],
	getById: function Trenches_getById(id) {
		return this.trenches[id];
	},
	register: function Trenches_register(element, xory, type, edge) {
		var trench = new Trench(element, xory, type, edge);
		this.trenches[trench.id] = trench;
		return trench.id;
	},
	unregister: function Trenches_unregister(ids) {
		if (!iQ.isArray(ids))
			ids = [ids];
		var self = this;
		ids.forEach(function(id){
			self.trenches[id].hide();
			delete self.trenches[id];
		});
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
		var aT = this.activeTrenches;
		
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