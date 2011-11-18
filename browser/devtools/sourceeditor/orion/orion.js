/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: 
 *		Felipe Heidrich (IBM Corporation) - initial API and implementation
 *		Silenio Quarti (IBM Corporation) - initial API and implementation
 ******************************************************************************/

/*global window define */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/**
 * Constructs a new key binding with the given key code and modifiers.
 * 
 * @param {String|Number} keyCode the key code.
 * @param {Boolean} mod1 the primary modifier (usually Command on Mac and Control on other platforms).
 * @param {Boolean} mod2 the secondary modifier (usually Shift).
 * @param {Boolean} mod3 the third modifier (usually Alt).
 * @param {Boolean} mod4 the fourth modifier (usually Control on the Mac).
 * 
 * @class A KeyBinding represents of a key code and a modifier state that can be triggered by the user using the keyboard.
 * @name orion.textview.KeyBinding
 * 
 * @property {String|Number} keyCode The key code.
 * @property {Boolean} mod1 The primary modifier (usually Command on Mac and Control on other platforms).
 * @property {Boolean} mod2 The secondary modifier (usually Shift).
 * @property {Boolean} mod3 The third modifier (usually Alt).
 * @property {Boolean} mod4 The fourth modifier (usually Control on the Mac).
 *
 * @see orion.textview.TextView#setKeyBinding
 */
orion.textview.KeyBinding = (function() {
	var isMac = window.navigator.platform.indexOf("Mac") !== -1;
	/** @private */
	function KeyBinding (keyCode, mod1, mod2, mod3, mod4) {
		if (typeof(keyCode) === "string") {
			this.keyCode = keyCode.toUpperCase().charCodeAt(0);
		} else {
			this.keyCode = keyCode;
		}
		this.mod1 = mod1 !== undefined && mod1 !== null ? mod1 : false;
		this.mod2 = mod2 !== undefined && mod2 !== null ? mod2 : false;
		this.mod3 = mod3 !== undefined && mod3 !== null ? mod3 : false;
		this.mod4 = mod4 !== undefined && mod4 !== null ? mod4 : false;
	}
	KeyBinding.prototype = /** @lends orion.textview.KeyBinding.prototype */ {
		/**
		 * Returns whether this key binding matches the given key event.
		 * 
		 * @param e the key event.
		 * @returns {Boolean} <code>true</code> whether the key binding matches the key event.
		 */
		match: function (e) {
			if (this.keyCode === e.keyCode) {
				var mod1 = isMac ? e.metaKey : e.ctrlKey;
				if (this.mod1 !== mod1) { return false; }
				if (this.mod2 !== e.shiftKey) { return false; }
				if (this.mod3 !== e.altKey) { return false; }
				if (isMac && this.mod4 !== e.ctrlKey) { return false; }
				return true;
			}
			return false;
		},
		/**
		 * Returns whether this key binding is the same as the given parameter.
		 * 
		 * @param {orion.textview.KeyBinding} kb the key binding to compare with.
		 * @returns {Boolean} whether or not the parameter and the receiver describe the same key binding.
		 */
		equals: function(kb) {
			if (!kb) { return false; }
			if (this.keyCode !== kb.keyCode) { return false; }
			if (this.mod1 !== kb.mod1) { return false; }
			if (this.mod2 !== kb.mod2) { return false; }
			if (this.mod3 !== kb.mod3) { return false; }
			if (this.mod4 !== kb.mod4) { return false; }
			return true;
		} 
	};
	return KeyBinding;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.textview;
	});
}

/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation
 ******************************************************************************/

/*global window define setTimeout clearTimeout setInterval clearInterval Node */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/**
 * Constructs a new ruler. 
 * <p>
 * The default implementation does not implement all the methods in the interface
 * and is useful only for objects implementing rulers.
 * <p/>
 * 
 * @param {orion.textview.AnnotationModel} annotationModel the annotation model for the ruler.
 * @param {String} [rulerLocation="left"] the location for the ruler.
 * @param {String} [rulerOverview="page"] the overview for the ruler.
 * @param {orion.textview.Style} [rulerStyle] the style for the ruler. 
 * 
 * @class This interface represents a ruler for the text view.
 * <p>
 * A Ruler is a graphical element that is placed either on the left or on the right side of 
 * the view. It can be used to provide the view with per line decoration such as line numbering,
 * bookmarks, breakpoints, folding disclosures, etc. 
 * </p><p>
 * There are two types of rulers: page and document. A page ruler only shows the content for the lines that are
 * visible, while a document ruler always shows the whole content.
 * </p>
 * <b>See:</b><br/>
 * {@link orion.textview.LineNumberRuler}<br/>
 * {@link orion.textview.AnnotationRuler}<br/>
 * {@link orion.textview.OverviewRuler}<br/> 
 * {@link orion.textview.TextView}<br/>
 * {@link orion.textview.TextView#addRuler}
 * </p>		 
 * @name orion.textview.Ruler
 */
orion.textview.Ruler = (function() {
	/** @private */
	function Ruler (annotationModel, rulerLocation, rulerOverview, rulerStyle) {
		this._location = rulerLocation || "left";
		this._overview = rulerOverview || "page";
		this._rulerStyle = rulerStyle;
		this._types = [];
		this._view = null;
		var self = this;
		this._annotationModelListener = {
			onChanged: function(e) {
				self._onAnnotationModelChanged(e);
			}
		};
		this.setAnnotationModel(annotationModel);
	}
	Ruler.prototype = /** @lends orion.textview.Ruler.prototype */ {
		/**
		 * Adds an annotation type to the ruler.
		 * <p>
		 * Only annotations of the specified types will be shown by
		 * this ruler.
		 * </p>
		 *
		 * @param type {Object} the annotation type to be shown
		 * 
		 * @see #removeAnnotationType
		 * @see #isAnnotationTypeVisible
		 */
		addAnnotationType: function(type) {
			this._types.push(type);
		},
		/**
		 * Returns the annotations for a given line range merging multiple
		 * annotations when necessary.
		 * <p>
		 * This method is called by the text view when the ruler is redrawn.
		 * </p>
		 *
		 * @param {Number} startLine the start line index
		 * @param {Number} endLine the end line index
		 * @return {orion.textview.Annotation[]} the annotations for the line range. The array might be sparse.
		 */
		getAnnotations: function(startLine, endLine) {
			var annotationModel = this._annotationModel;
			if (!annotationModel) { return []; }
			var model = this._view.getModel();
			var start = model.getLineStart(startLine);
			var end = model.getLineEnd(endLine - 1);
			var baseModel = model;
			if (model.getBaseModel) {
				baseModel = model.getBaseModel();
				start = model.mapOffset(start);
				end = model.mapOffset(end);
			}
			var annotations = annotationModel.getAnnotations(start, end);
			var result = [];
			while (annotations.hasNext()) {
				var annotation = annotations.next();
				if (!this.isAnnotationTypeVisible(annotation.type)) { continue; }
				var annotationLineStart = baseModel.getLineAtOffset(annotation.start);
				var annotationLineEnd = baseModel.getLineAtOffset(annotation.end - 1);
				for (var lineIndex = annotationLineStart; lineIndex<=annotationLineEnd; lineIndex++) {
					var visualLineIndex = lineIndex;
					if (model !== baseModel) {
						var ls = baseModel.getLineStart(lineIndex);
						ls = model.mapOffset(ls, true);
						if (ls === -1) { continue; }
						visualLineIndex = model.getLineAtOffset(ls);
					}
					if (!(startLine <= visualLineIndex && visualLineIndex < endLine)) { continue; }
					var rulerAnnotation = this._mergeAnnotation(result[visualLineIndex], annotation, lineIndex - annotationLineStart, annotationLineEnd - annotationLineStart + 1);
					if (rulerAnnotation) {
						result[visualLineIndex] = rulerAnnotation;
					}
				}
			}
			if (!this._multiAnnotation && this._multiAnnotationOverlay) {
				for (var k in result) {
					if (result[k]._multiple) {
						result[k].html = result[k].html + this._multiAnnotationOverlay.html;
					}
				}
			}
			return result;
		},
		/**
		 * Returns the annotation model.
		 *
		 * @returns {orion.textview.AnnotationModel} the ruler annotation model.
		 *
		 * @see #setAnnotationModel
		 */
		getAnnotationModel: function() {
			return this._annotationModel;
		},
		/**
		 * Returns the ruler location.
		 *
		 * @returns {String} the ruler location, which is either "left" or "right".
		 *
		 * @see #getOverview
		 */
		getLocation: function() {
			return this._location;
		},
		/**
		 * Returns the ruler overview type.
		 *
		 * @returns {String} the overview type, which is either "page" or "document".
		 *
		 * @see #getLocation
		 */
		getOverview: function() {
			return this._overview;
		},
		/**
		 * Returns the style information for the ruler.
		 *
		 * @returns {orion.textview.Style} the style information.
		 */
		getRulerStyle: function() {
			return this._rulerStyle;
		},
		/**
		 * Returns the widest annotation which determines the width of the ruler.
		 * <p>
		 * If the ruler does not have a fixed width it should provide the widest
		 * annotation to avoid the ruler from changing size as the view scrolls.
		 * </p>
		 * <p>
		 * This method is called by the text view when the ruler is redrawn.
		 * </p>
		 *
		 * @returns {orion.textview.Annotation} the widest annotation.
		 *
		 * @see #getAnnotations
		 */
		getWidestAnnotation: function() {
			return null;
		},
		/**
		 * Returns whether the ruler shows annotations of the specified type.
		 *
		 * @param {Object} type the annotation type 
		 * @returns {Boolean} whether the specified annotation type is shown
		 * 
		 * @see #addAnnotationType
		 * @see #removeAnnotationType
		 */
		isAnnotationTypeVisible: function(type) {
			for (var i = 0; i < this._types.length; i++) {
				if (this._types[i] === type) {
					return true;
				}
			}
			return false;
		},
		/**
		 * Removes an annotation type from the ruler.
		 *
		 * @param {Object} type the annotation type to be removed
		 * 
		 * @see #addAnnotationType
		 * @see #isAnnotationTypeVisible
		 */
		removeAnnotationType: function(type) {
			for (var i = 0; i < this._types.length; i++) {
				if (this._types[i] === type) {
					this._types.splice(i, 1);
					break;
				}
			}
		},
		/**
		 * Sets the annotation model for the ruler.
		 *
		 * @param {orion.textview.AnnotationModel} annotationModel the annotation model.
		 *
		 * @see #getAnnotationModel
		 */
		setAnnotationModel: function (annotationModel) {
			if (this._annotationModel) {
				this._annotationModel.removeListener(this._annotationModelListener); 
			}
			this._annotationModel = annotationModel;
			if (this._annotationModel) {
				this._annotationModel.addListener(this._annotationModelListener); 
			}
		},
		/**
		 * Sets the annotation that is displayed when a given line contains multiple
		 * annotations.  This annotation is used when there are different types of
		 * annotations in a given line.
		 *
		 * @param {orion.textview.Annotation} annotation the annotation for lines with multiple annotations.
		 * 
		 * @see #setMultiAnnotationOverlay
		 */
		setMultiAnnotation: function(annotation) {
			this._multiAnnotation = annotation;
		},
		/**
		 * Sets the annotation that overlays a line with multiple annotations.  This annotation is displayed on
		 * top of the computed annotation for a given line when there are multiple annotations of the same type
		 * in the line. It is also used when the multiple annotation is not set.
		 *
		 * @param {orion.textview.Annotation} annotation the annotation overlay for lines with multiple annotations.
		 * 
		 * @see #setMultiAnnotation
		 */
		setMultiAnnotationOverlay: function(annotation) {
			this._multiAnnotationOverlay = annotation;
		},
		/**
		 * Sets the view for the ruler.
		 * <p>
		 * This method is called by the text view when the ruler
		 * is added to the view.
		 * </p>
		 *
		 * @param {orion.textview.TextView} view the text view.
		 */
		setView: function (view) {
			if (this._onTextModelChanged && this._view) {
				this._view.removeEventListener("ModelChanged", this, this._onTextModelChanged); 
			}
			this._view = view;
			if (this._onTextModelChanged && this._view) {
				this._view.addEventListener("ModelChanged", this, this._onTextModelChanged);
			}
		},
		/**
		 * This event is sent when the user clicks a line annotation.
		 *
		 * @event
		 * @param {Number} lineIndex the line index of the annotation under the pointer.
		 * @param {DOMEvent} e the click event.
		 */
		onClick: function(lineIndex, e) {
		},
		/**
		 * This event is sent when the user double clicks a line annotation.
		 *
		 * @event
		 * @param {Number} lineIndex the line index of the annotation under the pointer.
		 * @param {DOMEvent} e the double click event.
		 */
		onDblClick: function(lineIndex, e) {
		},
		/**
		 * This event is sent when the user moves the mouse over a line annotation.
		 *
		 * @event
		 * @param {Number} lineIndex the line index of the annotation under the pointer.
		 * @param {DOMEvent} e the mouse move event.
		 */
		onMouseMove: function(lineIndex, e) {
			var tooltip = orion.textview.Tooltip.getTooltip(this._view);
			if (!tooltip) { return; }
			if (tooltip.isVisible() && this._tooltipLineIndex === lineIndex) { return; }
			this._tooltipLineIndex = lineIndex;
			var self = this;
			tooltip.setTarget({
				y: e.clientY,
				getTooltipInfo: function() {
					return self._getTooltipInfo(self._tooltipLineIndex, this.y);
				}
			});
		},
		/**
		 * This event is sent when the mouse pointer enters a line annotation.
		 *
		 * @event
		 * @param {Number} lineIndex the line index of the annotation under the pointer.
		 * @param {DOMEvent} e the mouse over event.
		 */
		onMouseOver: this._onMouseMove,
		/**
		 * This event is sent when the mouse pointer exits a line annotation.
		 *
		 * @event
		 * @param {Number} lineIndex the line index of the annotation under the pointer.
		 * @param {DOMEvent} e the mouse out event.
		 */
		onMouseOut: function(lineIndex, e) {
			var tooltip = orion.textview.Tooltip.getTooltip(this._view);
			if (!tooltip) { return; }
			tooltip.setTarget(null);
		},
		/** @ignore */
		_getTooltipInfo: function(lineIndex, y) {
			if (lineIndex === undefined) { return; }
			var view = this._view;
			var model = view.getModel();
			var annotationModel = this._annotationModel;
			var annotations = [];
			if (annotationModel) {
				var start = model.getLineStart(lineIndex);
				var end = model.getLineEnd(lineIndex);
				if (model.getBaseModel) {
					start = model.mapOffset(start);
					end = model.mapOffset(end);
				}
				var iter = annotationModel.getAnnotations(start, end);
				var annotation;
				while (iter.hasNext()) {
					annotation = iter.next();
					if (!this.isAnnotationTypeVisible(annotation.type)) { continue; }
					annotations.push(annotation);
				}
			}
			var contents = this._getTooltipContents(lineIndex, annotations);
			if (!contents) { return null; }
			var info = {
				contents: contents,
				anchor: this.getLocation()
			};
			var rect = view.getClientArea();
			if (this.getOverview() === "document") {
				rect.y = view.convert({y: y}, "view", "document").y;
			} else {
				rect.y = view.getLocationAtOffset(model.getLineStart(lineIndex)).y;
			}
			view.convert(rect, "document", "page");
			info.x = rect.x;
			info.y = rect.y;
			if (info.anchor === "right") {
				info.x += rect.width;
			}
			info.maxWidth = rect.width;
			info.maxHeight = rect.height - (rect.y - view._parent.getBoundingClientRect().top);
			return info;
		},
		/** @ignore */
		_getTooltipContents: function(lineIndex, annotations) {
			return annotations;
		},
		/** @ignore */
		_onAnnotationModelChanged: function(e) {
			var view = this._view;
			if (!view) { return; }
			var model = view.getModel(), self = this;
			var lineCount = model.getLineCount();
			if (e.textModelChangedEvent) {
				var start = e.textModelChangedEvent.start;
				if (model.getBaseModel) { start = model.mapOffset(start, true); }
				var startLine = model.getLineAtOffset(start);
				view.redrawLines(startLine, lineCount, self);
				return;
			}
			function redraw(changes) {
				for (var i = 0; i < changes.length; i++) {
					if (!self.isAnnotationTypeVisible(changes[i].type)) { continue; }
					var start = changes[i].start;
					var end = changes[i].end;
					if (model.getBaseModel) {
						start = model.mapOffset(start, true);
						end = model.mapOffset(end, true);
					}
					if (start !== -1 && end !== -1) {
						view.redrawLines(model.getLineAtOffset(start), model.getLineAtOffset(Math.max(start, end - 1)) + 1, self);
					}
				}
			}
			redraw(e.added);
			redraw(e.removed);
			redraw(e.changed);
		},
		/** @ignore */
		_mergeAnnotation: function(result, annotation, annotationLineIndex, annotationLineCount) {
			if (!result) { result = {}; }
			if (annotationLineIndex === 0) {
				if (result.html && annotation.html) {
					if (annotation.html !== result.html) {
						if (!result._multiple && this._multiAnnotation) {
							result.html = this._multiAnnotation.html;
						}
					} 
					result._multiple = true;
				} else {
					result.html = annotation.html;
				}
			}
			result.style = this._mergeStyle(result.style, annotation.style);
			return result;
		},
		/** @ignore */
		_mergeStyle: function(result, style) {
			if (style) {
				if (!result) { result = {}; }
				if (result.styleClass && style.styleClass && result.styleClass !== style.styleClass) {
					result.styleClass += " " + style.styleClass;
				} else {
					result.styleClass = style.styleClass;
				}
				var prop;
				if (style.style) {
					if (!result.style) { result.style  = {}; }
					for (prop in style.style) {
						if (!result.style[prop]) {
							result.style[prop] = style.style[prop];
						}
					}
				}
				if (style.attributes) {
					if (!result.attributes) { result.attributes  = {}; }
					for (prop in style.attributes) {
						if (!result.attributes[prop]) {
							result.attributes[prop] = style.attributes[prop];
						}
					}
				}
			}
			return result;
		}
	};
	return Ruler;
}());

/**
 * Constructs a new line numbering ruler. 
 *
 * @param {String} [rulerLocation="left"] the location for the ruler.
 * @param {orion.textview.Style} [rulerStyle=undefined] the style for the ruler.
 * @param {orion.textview.Style} [oddStyle={style: {backgroundColor: "white"}] the style for lines with odd line index.
 * @param {orion.textview.Style} [evenStyle={backgroundColor: "white"}] the style for lines with even line index.
 *
 * @augments orion.textview.Ruler
 * @class This objects implements a line numbering ruler.
 *
 * <p><b>See:</b><br/>
 * {@link orion.textview.Ruler}
 * </p>
 * @name orion.textview.LineNumberRuler
 */
orion.textview.LineNumberRuler = (function() {
	/** @private */
	function LineNumberRuler (annotationModel, rulerLocation, rulerStyle, oddStyle, evenStyle) {
		orion.textview.Ruler.call(this, annotationModel, rulerLocation, "page", rulerStyle);
		this._oddStyle = oddStyle || {style: {backgroundColor: "white"}};
		this._evenStyle = evenStyle || {style: {backgroundColor: "white"}};
		this._numOfDigits = 0;
	}
	LineNumberRuler.prototype = new orion.textview.Ruler(); 
	/** @ignore */
	LineNumberRuler.prototype.getAnnotations = function(startLine, endLine) {
		var result = orion.textview.Ruler.prototype.getAnnotations.call(this, startLine, endLine);
		var model = this._view.getModel();
		for (var lineIndex = startLine; lineIndex < endLine; lineIndex++) {
			var style = lineIndex & 1 ? this._oddStyle : this._evenStyle;
			var mapLine = lineIndex;
			if (model.getBaseModel) {
				var lineStart = model.getLineStart(mapLine);
				mapLine = model.getBaseModel().getLineAtOffset(model.mapOffset(lineStart));
			}
			if (!result[lineIndex]) { result[lineIndex] = {}; }
			result[lineIndex].html = (mapLine + 1) + "";
			if (!result[lineIndex].style) { result[lineIndex].style = style; }
		}
		return result;
	};
	/** @ignore */
	LineNumberRuler.prototype.getWidestAnnotation = function() {
		var lineCount = this._view.getModel().getLineCount();
		return this.getAnnotations(lineCount - 1, lineCount)[lineCount - 1];
	};
	/** @ignore */
	LineNumberRuler.prototype._onTextModelChanged = function(e) {
		var start = e.start;
		var model = this._view.getModel();
		var lineCount = model.getBaseModel ? model.getBaseModel().getLineCount() : model.getLineCount();
		var numOfDigits = (lineCount+"").length;
		if (this._numOfDigits !== numOfDigits) {
			this._numOfDigits = numOfDigits;
			var startLine = model.getLineAtOffset(start);
			this._view.redrawLines(startLine,  model.getLineCount(), this);
		}
	};
	return LineNumberRuler;
}());
/** 
 * @class This is class represents an annotation for the AnnotationRuler. 
 * <p> 
 * <b>See:</b><br/> 
 * {@link orion.textview.AnnotationRuler}
 * </p> 
 * 
 * @name orion.textview.Annotation 
 * 
 * @property {String} [html=""] The html content for the annotation, typically contains an image.
 * @property {orion.textview.Style} [style] the style for the annotation.
 * @property {orion.textview.Style} [overviewStyle] the style for the annotation in the overview ruler.
 */ 
/**
 * Contructs a new annotation ruler. 
 *
 * @param {String} [rulerLocation="left"] the location for the ruler.
 * @param {orion.textview.Style} [rulerStyle=undefined] the style for the ruler.
 * @param {orion.textview.Annotation} [defaultAnnotation] the default annotation.
 *
 * @augments orion.textview.Ruler
 * @class This objects implements an annotation ruler.
 *
 * <p><b>See:</b><br/>
 * {@link orion.textview.Ruler}<br/>
 * {@link orion.textview.Annotation}
 * </p>
 * @name orion.textview.AnnotationRuler
 */
orion.textview.AnnotationRuler = (function() {
	/** @private */
	function AnnotationRuler (annotationModel, rulerLocation, rulerStyle) {
		orion.textview.Ruler.call(this, annotationModel, rulerLocation, "page", rulerStyle);
	}
	AnnotationRuler.prototype = new orion.textview.Ruler();
	
	return AnnotationRuler;
}());

/**
 * Contructs an overview ruler. 
 * <p>
 * The overview ruler is used in conjunction with a AnnotationRuler, for each annotation in the 
 * AnnotationRuler this ruler displays a mark in the overview. Clicking on the mark causes the 
 * view to scroll to the annotated line.
 * </p>
 *
 * @param {String} [rulerLocation="left"] the location for the ruler.
 * @param {orion.textview.Style} [rulerStyle=undefined] the style for the ruler.
 * @param {orion.textview.AnnotationRuler} [annotationRuler] the annotation ruler for the overview.
 *
 * @augments orion.textview.Ruler
 * @class This objects implements an overview ruler.
 *
 * <p><b>See:</b><br/>
 * {@link orion.textview.AnnotationRuler} <br/>
 * {@link orion.textview.Ruler} 
 * </p>
 * @name orion.textview.OverviewRuler
 */
orion.textview.OverviewRuler = (function() {
	/** @private */
	function OverviewRuler (annotationModel, rulerLocation, rulerStyle) {
		orion.textview.Ruler.call(this, annotationModel, rulerLocation, "document", rulerStyle);
	}
	OverviewRuler.prototype = new orion.textview.Ruler();
	
	/** @ignore */
	OverviewRuler.prototype.getRulerStyle = function() {
		var result = {style: {lineHeight: "1px", fontSize: "1px"}};
		result = this._mergeStyle(result, this._rulerStyle);
		return result;
	};
	/** @ignore */	
	OverviewRuler.prototype.onClick = function(lineIndex, e) {
		if (lineIndex === undefined) { return; }
		this._view.setTopIndex(lineIndex);
	};
	/** @ignore */
	OverviewRuler.prototype._getTooltipContents = function(lineIndex, annotations) {
		if (annotations.length === 0) {
			var model = this._view.getModel();
			var mapLine = lineIndex;
			if (model.getBaseModel) {
				var lineStart = model.getLineStart(mapLine);
				mapLine = model.getBaseModel().getLineAtOffset(model.mapOffset(lineStart));
			}
			return "Line: " + (mapLine + 1);
		}
		return orion.textview.Ruler.prototype._getTooltipContents.call(this, lineIndex, annotations);
	};
	/** @ignore */
	OverviewRuler.prototype._mergeAnnotation = function(previousAnnotation, annotation, annotationLineIndex, annotationLineCount) {
		if (annotationLineIndex !== 0) { return undefined; }
		var result = previousAnnotation;
		if (!result) {
			//TODO annotationLineCount does not work when there are folded lines
			var height = 3 * annotationLineCount;
			result = {html: "&nbsp;", style: { style: {height: height + "px"}}};
			result.style = this._mergeStyle(result.style, annotation.overviewStyle);
		}
		return result;
	};
	return OverviewRuler;
}());

orion.textview.FoldingRuler = (function() {
	/** @private */
	function FoldingRuler (annotationModel, rulerLocation, rulerStyle) {
		orion.textview.AnnotationRuler.call(this, annotationModel, rulerLocation, rulerStyle);
	}
	FoldingRuler.prototype = new orion.textview.AnnotationRuler();
	
	/** @ignore */
	FoldingRuler.prototype.onClick =  function(lineIndex, e) {
		if (lineIndex === undefined) { return; }
		var annotationModel = this._annotationModel;
		if (!annotationModel) { return; }
		var view = this._view;
		var model = view.getModel();
		var start = model.getLineStart(lineIndex);
		var end = model.getLineEnd(lineIndex, true);
		if (model.getBaseModel) {
			start = model.mapOffset(start);
			end = model.mapOffset(end);
		}
		var annotation, iter = annotationModel.getAnnotations(start, end);
		while (!annotation && iter.hasNext()) {
			var a = iter.next();
			if (!this.isAnnotationTypeVisible(a.type)) { continue; }
			annotation = a;
		}
		if (annotation) {
			var tooltip = orion.textview.Tooltip.getTooltip(this._view);
			if (tooltip) {
				tooltip.setTarget(null);
			}
			if (annotation.expanded) {
				annotation.collapse();
			} else {
				annotation.expand();
			}
			this._annotationModel.modifyAnnotation(annotation);
		}
	};
	/** @ignore */
	FoldingRuler.prototype._getTooltipContents = function(lineIndex, annotations) {
		if (annotations.length === 1) {
			if (annotations[0].expanded) {
				return null;
			}
		}
		return orion.textview.AnnotationRuler.prototype._getTooltipContents.call(this, lineIndex, annotations);
	};
	/** @ignore */
	FoldingRuler.prototype._onAnnotationModelChanged = function(e) {
		if (e.textModelChangedEvent) {
			orion.textview.AnnotationRuler.prototype._onAnnotationModelChanged.call(this, e);
			return;
		}
		var view = this._view;
		if (!view) { return; }
		var model = view.getModel(), self = this, i;
		var lineCount = model.getLineCount(), lineIndex = lineCount;
		function redraw(changes) {
			for (i = 0; i < changes.length; i++) {
				if (!self.isAnnotationTypeVisible(changes[i].type)) { continue; }
				var start = changes[i].start;
				if (model.getBaseModel) {
					start = model.mapOffset(start, true);
				}
				if (start !== -1) {
					lineIndex = Math.min(lineIndex, model.getLineAtOffset(start));
				}
			}
		}
		redraw(e.added);
		redraw(e.removed);
		redraw(e.changed);
		var rulers = view.getRulers();
		for (i = 0; i < rulers.length; i++) {
			view.redrawLines(lineIndex, lineCount, rulers[i]);
		}
	};
	
	return FoldingRuler;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define(['orion/textview/tooltip'], function() {
		return orion.textview;
	});
}
/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation
 ******************************************************************************/

/*global window define */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/**
 * Constructs a new UndoStack on a text view.
 *
 * @param {orion.textview.TextView} view the text view for the undo stack.
 * @param {Number} [size=100] the size for the undo stack.
 *
 * @name orion.textview.UndoStack
 * @class The UndoStack is used to record the history of a text model associated to an view. Every
 * change to the model is added to stack, allowing the application to undo and redo these changes.
 *
 * <p>
 * <b>See:</b><br/>
 * {@link orion.textview.TextView}<br/>
 * </p>
 */
orion.textview.UndoStack = (function() {
	/** 
	 * Constructs a new Change object.
	 * 
	 * @class 
	 * @name orion.textview.Change
	 * @private
	 */
	var Change = (function() {
		function Change(offset, text, previousText) {
			this.offset = offset;
			this.text = text;
			this.previousText = previousText;
		}
		Change.prototype = {
			/** @ignore */
			undo: function (view, select) {
				this._doUndoRedo(this.offset, this.previousText, this.text, view, select);
			},
			/** @ignore */
			redo: function (view, select) {
				this._doUndoRedo(this.offset, this.text, this.previousText, view, select);
			},
			_doUndoRedo: function(offset, text, previousText, view, select) {
				var model = view.getModel();
				/* 
				* TODO UndoStack should be changing the text in the base model.
				* This is code needs to change when modifications in the base
				* model are supported properly by the projection model.
				*/
				if (model.mapOffset && view.annotationModel) {
					var mapOffset = model.mapOffset(offset, true);
					if (mapOffset < 0) {
						var annotationModel = view.annotationModel;
						var iter = annotationModel.getAnnotations(offset, offset + 1);
						while (iter.hasNext()) {
							var annotation = iter.next();
							if (annotation.type === "orion.annotation.folding") {
								annotation.expand();
								mapOffset = model.mapOffset(offset, true);
								break;
							}
						}
					}
					if (mapOffset < 0) { return; }
					offset = mapOffset;
				}
				view.setText(text, offset, offset + previousText.length);
				if (select) {
					view.setSelection(offset, offset + text.length);
				}
			}
		};
		return Change;
	}());

	/** 
	 * Constructs a new CompoundChange object.
	 * 
	 * @class 
	 * @name orion.textview.CompoundChange
	 * @private
	 */
	var CompoundChange = (function() {
		function CompoundChange (selection, caret) {
			this.selection = selection;
			this.caret = caret;
			this.changes = [];
		}
		CompoundChange.prototype = {
			/** @ignore */
			add: function (change) {
				this.changes.push(change);
			},
			/** @ignore */
			undo: function (view, select) {
				for (var i=this.changes.length - 1; i >= 0; i--) {
					this.changes[i].undo(view, false);
				}
				if (select) {
					var start = this.selection.start;
					var end = this.selection.end;
					view.setSelection(this.caret ? start : end, this.caret ? end : start);
				}
			},
			/** @ignore */
			redo: function (view, select) {
				for (var i = 0; i < this.changes.length; i++) {
					this.changes[i].redo(view, false);
				}
				if (select) {
					var start = this.selection.start;
					var end = this.selection.end;
					view.setSelection(this.caret ? start : end, this.caret ? end : start);
				}
			}
		};
		return CompoundChange;
	}());

	/** @private */
	function UndoStack (view, size) {
		this.view = view;
		this.size = size !== undefined ? size : 100;
		this.reset();
		var model = view.getModel();
		if (model.getBaseModel) {
			model = model.getBaseModel();
		}
		this.model = model;
		var self = this;
		this._modelListener = {
			onChanging: function(e) {
				self._onModelChanging(e);
			}
		};
		model.addListener(this._modelListener);
		view._undoStack = this;
		view.addEventListener("Destroy", this, this._onDestroy);
	}
	UndoStack.prototype = /** @lends orion.textview.UndoStack.prototype */ {
		/**
		 * Adds a change to the stack.
		 * 
		 * @param change the change to add.
		 * @param {Number} change.offset the offset of the change
		 * @param {String} change.text the new text of the change
		 * @param {String} change.previousText the previous text of the change
		 */
		add: function (change) {
			if (this.compoundChange) {
				this.compoundChange.add(change);
			} else {
				var length = this.stack.length;
				this.stack.splice(this.index, length-this.index, change);
				this.index++;
				if (this.stack.length > this.size) {
					this.stack.shift();
					this.index--;
					this.cleanIndex--;
				}
			}
		},
		/** 
		 * Marks the current state of the stack as clean.
		 *
		 * <p>
		 * This function is typically called when the content of view associated with the stack is saved.
		 * </p>
		 *
		 * @see #isClean
		 */
		markClean: function() {
			this.endCompoundChange();
			this._commitUndo();
			this.cleanIndex = this.index;
		},
		/**
		 * Returns true if current state of stack is the same
		 * as the state when markClean() was called.
		 *
		 * <p>
		 * For example, the application calls markClean(), then calls undo() four times and redo() four times.
		 * At this point isClean() returns true.  
		 * </p>
		 * <p>
		 * This function is typically called to determine if the content of the view associated with the stack
		 * has changed since the last time it was saved.
		 * </p>
		 *
		 * @return {Boolean} returns if the state is the same as the state when markClean() was called.
		 *
		 * @see #markClean
		 */
		isClean: function() {
			return this.cleanIndex === this.getSize().undo;
		},
		/**
		 * Returns true if there is at least one change to undo.
		 *
		 * @return {Boolean} returns true if there is at least one change to undo.
		 *
		 * @see #canRedo
		 * @see #undo
		 */
		canUndo: function() {
			return this.getSize().undo > 0;
		},
		/**
		 * Returns true if there is at least one change to redo.
		 *
		 * @return {Boolean} returns true if there is at least one change to redo.
		 *
		 * @see #canUndo
		 * @see #redo
		 */
		canRedo: function() {
			return this.getSize().redo > 0;
		},
		/**
		 * Finishes a compound change.
		 *
		 * @see #startCompoundChange
		 */
		endCompoundChange: function() {
			this.compoundChange = undefined;
		},
		/**
		 * Returns the sizes of the stack.
		 *
		 * @return {object} a object where object.undo is the number of changes that can be un-done, 
		 *  and object.redo is the number of changes that can be re-done.
		 *
		 * @see #canUndo
		 * @see #canRedo
		 */
		getSize: function() {
			var index = this.index;
			var length = this.stack.length;
			if (this._undoStart !== undefined) {
				index++;
			}
			return {undo: index, redo: (length - index)};
		},
		/**
		 * Undo the last change in the stack.
		 *
		 * @return {Boolean} returns true if a change was un-done.
		 *
		 * @see #redo
		 * @see #canUndo
		 */
		undo: function() {
			this._commitUndo();
			if (this.index <= 0) {
				return false;
			}
			var change = this.stack[--this.index];
			this._ignoreUndo = true;
			change.undo(this.view, true);
			this._ignoreUndo = false;
			return true;
		},
		/**
		 * Redo the last change in the stack.
		 *
		 * @return {Boolean} returns true if a change was re-done.
		 *
		 * @see #undo
		 * @see #canRedo
		 */
		redo: function() {
			this._commitUndo();
			if (this.index >= this.stack.length) {
				return false;
			}
			var change = this.stack[this.index++];
			this._ignoreUndo = true;
			change.redo(this.view, true);
			this._ignoreUndo = false;
			return true;
		},
		/**
		 * Reset the stack to its original state. All changes in the stack are thrown away.
		 */
		reset: function() {
			this.index = this.cleanIndex = 0;
			this.stack = [];
			this._undoStart = undefined;
			this._undoText = "";
			this._ignoreUndo = false;
			this._compoundChange = undefined;
		},
		/**
		 * Starts a compound change. 
		 * <p>
		 * All changes added to stack from the time startCompoundChange() is called
		 * to the time that endCompoundChange() is called are compound on one change that can be un-done or re-done
		 * with one single call to undo() or redo().
		 * </p>
		 *
		 * @see #endCompoundChange
		 */
		startCompoundChange: function() {
			this._commitUndo();
			var change = new CompoundChange(this.view.getSelection(), this.view.getCaretOffset());
			this.add(change);
			this.compoundChange = change;
		},
		_commitUndo: function () {
			if (this._undoStart !== undefined) {
				if (this._undoStart < 0) {
					this.add(new Change(-this._undoStart, "", this._undoText, ""));
				} else {
					this.add(new Change(this._undoStart, this._undoText, ""));
				}
				this._undoStart = undefined;
				this._undoText = "";
			}
		},
		_onDestroy: function() {
			this.model.removeListener(this._modelListener);
			this.view.removeEventListener("Destroy", this, this._onDestroy);
			this.view._undoStack = null;
		},
		_onModelChanging: function(e) {
			var newText = e.text;
			var start = e.start;
			var removedCharCount = e.removedCharCount;
			var addedCharCount = e.addedCharCount;
			if (this._ignoreUndo) {
				return;
			}
			if (this._undoStart !== undefined && 
				!((addedCharCount === 1 && removedCharCount === 0 && start === this._undoStart + this._undoText.length) ||
					(addedCharCount === 0 && removedCharCount === 1 && (((start + 1) === -this._undoStart) || (start === -this._undoStart)))))
			{
				this._commitUndo();
			}
			if (!this.compoundChange) {
				if (addedCharCount === 1 && removedCharCount === 0) {
					if (this._undoStart === undefined) {
						this._undoStart = start;
					}
					this._undoText = this._undoText + newText;
					return;
				} else if (addedCharCount === 0 && removedCharCount === 1) {
					var deleting = this._undoText.length > 0 && -this._undoStart === start;
					this._undoStart = -start;
					if (deleting) {
						this._undoText = this._undoText + this.model.getText(start, start + removedCharCount);
					} else {
						this._undoText = this.model.getText(start, start + removedCharCount) + this._undoText;
					}
					return;
				}
			}
			this.add(new Change(start, newText, this.model.getText(start, start + removedCharCount)));
		}
	};
	return UndoStack;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.textview;
	});
}
/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: 
 *		Felipe Heidrich (IBM Corporation) - initial API and implementation
 *		Silenio Quarti (IBM Corporation) - initial API and implementation
 ******************************************************************************/
 
/*global window define */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/**
 * Constructs a new TextModel with the given text and default line delimiter.
 *
 * @param {String} [text=""] the text that the model will store
 * @param {String} [lineDelimiter=platform delimiter] the line delimiter used when inserting new lines to the model.
 *
 * @name orion.textview.TextModel
 * @class The TextModel is an interface that provides text for the view. Applications may
 * implement the TextModel interface to provide a custom store for the view content. The
 * view interacts with its text model in order to access and update the text that is being
 * displayed and edited in the view. This is the default implementation.
 * <p>
 * <b>See:</b><br/>
 * {@link orion.textview.TextView}<br/>
 * {@link orion.textview.TextView#setModel}
 * </p>
 */
orion.textview.TextModel = (function() {
	var isWindows = window.navigator.platform.indexOf("Win") !== -1;

	/** @private */
	function TextModel(text, lineDelimiter) {
		this._listeners = [];
		this._lastLineIndex = -1;
		this._text = [""];
		this._lineOffsets = [0];
		this.setText(text);
		this.setLineDelimiter(lineDelimiter);
	}

	TextModel.prototype = /** @lends orion.textview.TextModel.prototype */ {
		/**
		 * Adds a listener to the model.
		 * 
		 * @param {Object} listener the listener to add.
		 * @param {Function} [listener.onChanged] see {@link #onChanged}.
		 * @param {Function} [listener.onChanging] see {@link #onChanging}.
		 * 
		 * @see removeListener
		 */
		addListener: function(listener) {
			this._listeners.push(listener);
		},
		/**
		 * Removes a listener from the model.
		 * 
		 * @param {Object} listener the listener to remove
		 * 
		 * @see #addListener
		 */
		removeListener: function(listener) {
			for (var i = 0; i < this._listeners.length; i++) {
				if (this._listeners[i] === listener) {
					this._listeners.splice(i, 1);
					return;
				}
			}
		},
		/**
		 * Returns the number of characters in the model.
		 *
		 * @returns {Number} the number of characters in the model.
		 */
		getCharCount: function() {
			var count = 0;
			for (var i = 0; i<this._text.length; i++) {
				count += this._text[i].length;
			}
			return count;
		},
		/**
		 * Returns the text of the line at the given index.
		 * <p>
		 * The valid indices are 0 to line count exclusive.  Returns <code>null</code> 
		 * if the index is out of range. 
		 * </p>
		 *
		 * @param {Number} lineIndex the zero based index of the line.
		 * @param {Boolean} [includeDelimiter=false] whether or not to include the line delimiter. 
		 * @returns {String} the line text or <code>null</code> if out of range.
		 *
		 * @see #getLineAtOffset
		 */
		getLine: function(lineIndex, includeDelimiter) {
			var lineCount = this.getLineCount();
			if (!(0 <= lineIndex && lineIndex < lineCount)) {
				return null;
			}
			var start = this._lineOffsets[lineIndex];
			if (lineIndex + 1 < lineCount) {
				var text = this.getText(start, this._lineOffsets[lineIndex + 1]);
				if (includeDelimiter) {
					return text;
				}
				var end = text.length, c;
				while (((c = text.charCodeAt(end - 1)) === 10) || (c === 13)) {
					end--;
				}
				return text.substring(0, end);
			} else {
				return this.getText(start); 
			}
		},
		/**
		 * Returns the line index at the given character offset.
		 * <p>
		 * The valid offsets are 0 to char count inclusive. The line index for
		 * char count is <code>line count - 1</code>. Returns <code>-1</code> if
		 * the offset is out of range.
		 * </p>
		 *
		 * @param {Number} offset a character offset.
		 * @returns {Number} the zero based line index or <code>-1</code> if out of range.
		 */
		getLineAtOffset: function(offset) {
			var charCount = this.getCharCount();
			if (!(0 <= offset && offset <= charCount)) {
				return -1;
			}
			var lineCount = this.getLineCount();
			if (offset === charCount) {
				return lineCount - 1; 
			}
			var lineStart, lineEnd;
			var index = this._lastLineIndex;
			if (0 <= index && index < lineCount) {
				lineStart = this._lineOffsets[index];
				lineEnd = index + 1 < lineCount ? this._lineOffsets[index + 1] : charCount;
				if (lineStart <= offset && offset < lineEnd) {
					return index;
				}
			}
			var high = lineCount;
			var low = -1;
			while (high - low > 1) {
				index = Math.floor((high + low) / 2);
				lineStart = this._lineOffsets[index];
				lineEnd = index + 1 < lineCount ? this._lineOffsets[index + 1] : charCount;
				if (offset <= lineStart) {
					high = index;
				} else if (offset < lineEnd) {
					high = index;
					break;
				} else {
					low = index;
				}
			}
			this._lastLineIndex = high;
			return high;
		},
		/**
		 * Returns the number of lines in the model.
		 * <p>
		 * The model always has at least one line.
		 * </p>
		 *
		 * @returns {Number} the number of lines.
		 */
		getLineCount: function() {
			return this._lineOffsets.length;
		},
		/**
		 * Returns the line delimiter that is used by the view
		 * when inserting new lines. New lines entered using key strokes 
		 * and paste operations use this line delimiter.
		 *
		 * @return {String} the line delimiter that is used by the view when inserting new lines.
		 */
		getLineDelimiter: function() {
			return this._lineDelimiter;
		},
		/**
		 * Returns the end character offset for the given line. 
		 * <p>
		 * The end offset is not inclusive. This means that when the line delimiter is included, the 
		 * offset is either the start offset of the next line or char count. When the line delimiter is
		 * not included, the offset is the offset of the line delimiter.
		 * </p>
		 * <p>
		 * The valid indices are 0 to line count exclusive.  Returns <code>-1</code> 
		 * if the index is out of range. 
		 * </p>
		 *
		 * @param {Number} lineIndex the zero based index of the line.
		 * @param {Boolean} [includeDelimiter=false] whether or not to include the line delimiter. 
		 * @return {Number} the line end offset or <code>-1</code> if out of range.
		 *
		 * @see #getLineStart
		 */
		getLineEnd: function(lineIndex, includeDelimiter) {
			var lineCount = this.getLineCount();
			if (!(0 <= lineIndex && lineIndex < lineCount)) {
				return -1;
			}
			if (lineIndex + 1 < lineCount) {
				var end = this._lineOffsets[lineIndex + 1];
				if (includeDelimiter) {
					return end;
				}
				var text = this.getText(Math.max(this._lineOffsets[lineIndex], end - 2), end);
				var i = text.length, c;
				while (((c = text.charCodeAt(i - 1)) === 10) || (c === 13)) {
					i--;
				}
				return end - (text.length - i);
			} else {
				return this.getCharCount();
			}
		},
		/**
		 * Returns the start character offset for the given line.
		 * <p>
		 * The valid indices are 0 to line count exclusive.  Returns <code>-1</code> 
		 * if the index is out of range. 
		 * </p>
		 *
		 * @param {Number} lineIndex the zero based index of the line.
		 * @return {Number} the line start offset or <code>-1</code> if out of range.
		 *
		 * @see #getLineEnd
		 */
		getLineStart: function(lineIndex) {
			if (!(0 <= lineIndex && lineIndex < this.getLineCount())) {
				return -1;
			}
			return this._lineOffsets[lineIndex];
		},
		/**
		 * Returns the text for the given range.
		 * <p>
		 * The end offset is not inclusive. This means that character at the end offset
		 * is not included in the returned text.
		 * </p>
		 *
		 * @param {Number} [start=0] the zero based start offset of text range.
		 * @param {Number} [end=char count] the zero based end offset of text range.
		 *
		 * @see #setText
		 */
		getText: function(start, end) {
			if (start === undefined) { start = 0; }
			if (end === undefined) { end = this.getCharCount(); }
			if (start === end) { return ""; }
			var offset = 0, chunk = 0, length;
			while (chunk<this._text.length) {
				length = this._text[chunk].length; 
				if (start <= offset + length) { break; }
				offset += length;
				chunk++;
			}
			var firstOffset = offset;
			var firstChunk = chunk;
			while (chunk<this._text.length) {
				length = this._text[chunk].length; 
				if (end <= offset + length) { break; }
				offset += length;
				chunk++;
			}
			var lastOffset = offset;
			var lastChunk = chunk;
			if (firstChunk === lastChunk) {
				return this._text[firstChunk].substring(start - firstOffset, end - lastOffset);
			}
			var beforeText = this._text[firstChunk].substring(start - firstOffset);
			var afterText = this._text[lastChunk].substring(0, end - lastOffset);
			return beforeText + this._text.slice(firstChunk+1, lastChunk).join("") + afterText; 
		},
		/**
		 * Notifies all listeners that the text is about to change.
		 * <p>
		 * This notification is intended to be used only by the view. Application clients should
		 * use {@link orion.textview.TextView#event:onModelChanging}.
		 * </p>
		 * <p>
		 * NOTE: This method is not meant to called directly by application code. It is called internally by the TextModel
		 * as part of the implementation of {@link #setText}. This method is included in the public API for documentation
		 * purposes and to allow integration with other toolkit frameworks.
		 * </p>
		 *
		 * @param {orion.textview.ModelChangingEvent} modelChangingEvent the changing event
		 */
		onChanging: function(modelChangingEvent) {
			for (var i = 0; i < this._listeners.length; i++) {
				var l = this._listeners[i]; 
				if (l && l.onChanging) { 
					l.onChanging(modelChangingEvent);
				}
			}
		},
		/**
		 * Notifies all listeners that the text has changed.
		 * <p>
		 * This notification is intended to be used only by the view. Application clients should
		 * use {@link orion.textview.TextView#event:onModelChanged}.
		 * </p>
		 * <p>
		 * NOTE: This method is not meant to called directly by application code. It is called internally by the TextModel
		 * as part of the implementation of {@link #setText}. This method is included in the public API for documentation
		 * purposes and to allow integration with other toolkit frameworks.
		 * </p>
		 *
		 * @param {orion.textview.ModelChangedEvent} modelChangedEvent the changed event
		 */
		onChanged: function(modelChangedEvent) {
			for (var i = 0; i < this._listeners.length; i++) {
				var l = this._listeners[i]; 
				if (l && l.onChanged) { 
					l.onChanged(modelChangedEvent);
				}
			}
		},
		/**
		 * Sets the line delimiter that is used by the view
		 * when new lines are inserted in the model due to key
		 * strokes  and paste operations.
		 * <p>
		 * If lineDelimiter is "auto", the delimiter is computed to be
		 * the first delimiter found the in the current text. If lineDelimiter
		 * is undefined or if there are no delimiters in the current text, the
		 * platform delimiter is used.
		 * </p>
		 *
		 * @param {String} lineDelimiter the line delimiter that is used by the view when inserting new lines.
		 */
		setLineDelimiter: function(lineDelimiter) {
			if (lineDelimiter === "auto") {
				lineDelimiter = undefined;
				if (this.getLineCount() > 1) {
					lineDelimiter = this.getText(this.getLineEnd(0), this.getLineEnd(0, true));
				}
			}
			this._lineDelimiter = lineDelimiter ? lineDelimiter : (isWindows ? "\r\n" : "\n"); 
		},
		/**
		 * Replaces the text in the given range with the given text.
		 * <p>
		 * The end offset is not inclusive. This means that the character at the 
		 * end offset is not replaced.
		 * </p>
		 * <p>
		 * The text model must notify the listeners before and after the
		 * the text is changed by calling {@link #onChanging} and {@link #onChanged}
		 * respectively. 
		 * </p>
		 *
		 * @param {String} [text=""] the new text.
		 * @param {Number} [start=0] the zero based start offset of text range.
		 * @param {Number} [end=char count] the zero based end offset of text range.
		 *
		 * @see #getText
		 */
		setText: function(text, start, end) {
			if (text === undefined) { text = ""; }
			if (start === undefined) { start = 0; }
			if (end === undefined) { end = this.getCharCount(); }
			var startLine = this.getLineAtOffset(start);
			var endLine = this.getLineAtOffset(end);
			var eventStart = start;
			var removedCharCount = end - start;
			var removedLineCount = endLine - startLine;
			var addedCharCount = text.length;
			var addedLineCount = 0;
			var lineCount = this.getLineCount();
			
			var cr = 0, lf = 0, index = 0;
			var newLineOffsets = [];
			while (true) {
				if (cr !== -1 && cr <= index) { cr = text.indexOf("\r", index); }
				if (lf !== -1 && lf <= index) { lf = text.indexOf("\n", index); }
				if (lf === -1 && cr === -1) { break; }
				if (cr !== -1 && lf !== -1) {
					if (cr + 1 === lf) {
						index = lf + 1;
					} else {
						index = (cr < lf ? cr : lf) + 1;
					}
				} else if (cr !== -1) {
					index = cr + 1;
				} else {
					index = lf + 1;
				}
				newLineOffsets.push(start + index);
				addedLineCount++;
			}
		
			var modelChangingEvent = {
				text: text,
				start: eventStart,
				removedCharCount: removedCharCount,
				addedCharCount: addedCharCount,
				removedLineCount: removedLineCount,
				addedLineCount: addedLineCount
			};
			this.onChanging(modelChangingEvent);
			
			//TODO this should be done the loops below to avoid getText()
			if (newLineOffsets.length === 0) {
				var startLineOffset = this.getLineStart(startLine), endLineOffset;
				if (endLine + 1 < lineCount) {
					endLineOffset = this.getLineStart(endLine + 1);
				} else {
					endLineOffset = this.getCharCount();
				}
				if (start !== startLineOffset) {
					text = this.getText(startLineOffset, start) + text;
					start = startLineOffset;
				}
				if (end !== endLineOffset) {
					text = text + this.getText(end, endLineOffset);
					end = endLineOffset;
				}
			}
			
			var changeCount = addedCharCount - removedCharCount;
			for (var j = startLine + removedLineCount + 1; j < lineCount; j++) {
				this._lineOffsets[j] += changeCount;
			}
			var args = [startLine + 1, removedLineCount].concat(newLineOffsets);
			Array.prototype.splice.apply(this._lineOffsets, args);
			
			var offset = 0, chunk = 0, length;
			while (chunk<this._text.length) {
				length = this._text[chunk].length; 
				if (start <= offset + length) { break; }
				offset += length;
				chunk++;
			}
			var firstOffset = offset;
			var firstChunk = chunk;
			while (chunk<this._text.length) {
				length = this._text[chunk].length; 
				if (end <= offset + length) { break; }
				offset += length;
				chunk++;
			}
			var lastOffset = offset;
			var lastChunk = chunk;
			var firstText = this._text[firstChunk];
			var lastText = this._text[lastChunk];
			var beforeText = firstText.substring(0, start - firstOffset);
			var afterText = lastText.substring(end - lastOffset);
			var params = [firstChunk, lastChunk - firstChunk + 1];
			if (beforeText) { params.push(beforeText); }
			if (text) { params.push(text); }
			if (afterText) { params.push(afterText); }
			Array.prototype.splice.apply(this._text, params);
			if (this._text.length === 0) { this._text = [""]; }
			
			var modelChangedEvent = {
				start: eventStart,
				removedCharCount: removedCharCount,
				addedCharCount: addedCharCount,
				removedLineCount: removedLineCount,
				addedLineCount: addedLineCount
			};
			this.onChanged(modelChangedEvent);
		}
	};
	
	return TextModel;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.textview;
	});
}
/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation
 ******************************************************************************/

/*global window define setTimeout clearTimeout setInterval clearInterval Node */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/** @ignore */
orion.textview.Tooltip = (function() {
	/** @private */
	function Tooltip (view) {
		this._view = view;
		//TODO add API to get the parent of the view
		this._create(view._parent.ownerDocument);
		view.addEventListener("Destroy", this, this.destroy);
	}
	Tooltip.getTooltip = function(view) {
		if (!view._tooltip) {
			 view._tooltip = new Tooltip(view);
		}
		return view._tooltip;
	};
	Tooltip.prototype = /** @lends orion.textview.Tooltip.prototype */ {
		_create: function(document) {
			if (this._domNode) { return; }
			this._document = document;
			var domNode = this._domNode = document.createElement("DIV");
			domNode.className = "viewTooltip";
			var viewParent = this._viewParent = document.createElement("DIV");
			domNode.appendChild(viewParent);
			var htmlParent = this._htmlParent = document.createElement("DIV");
			domNode.appendChild(htmlParent);
			document.body.appendChild(domNode);
			this.hide();
		},
		destroy: function() {
			if (!this._domNode) { return; }
			if (this._contentsView) {
				this._contentsView.destroy();
				this._contentsView = null;
				this._emptyModel = null;
			}
			var parent = this._domNode.parentNode;
			if (parent) { parent.removeChild(this._domNode); }
			this._domNode = null;
		},
		hide: function() {
			if (this._contentsView) {
				this._contentsView.setModel(this._emptyModel);
			}
			if (this._viewParent) {
				this._viewParent.style.left = "-10000px";
				this._viewParent.style.position = "fixed";
				this._viewParent.style.visibility = "hidden";
			}
			if (this._htmlParent) {
				this._htmlParent.style.left = "-10000px";
				this._htmlParent.style.position = "fixed";
				this._htmlParent.style.visibility = "hidden";
				this._htmlParent.innerHTML = "";
			}
			if (this._domNode) {
				this._domNode.style.visibility = "hidden";
			}
			if (this._showTimeout) {
				clearTimeout(this._showTimeout);
				this._showTimeout = null;
			}
			if (this._hideTimeout) {
				clearTimeout(this._hideTimeout);
				this._hideTimeout = null;
			}
			if (this._fadeTimeout) {
				clearInterval(this._fadeTimeout);
				this._fadeTimeout = null;
			}
		},
		isVisible: function() {
			return this._domNode && this._domNode.style.visibility === "visible";
		},
		setTarget: function(target) {
			if (this.target === target) { return; }
			this._target = target;
			this.hide();
			if (target) {
				var self = this;
				self._showTimeout = setTimeout(function() {
					self.show(true);
				}, 1000);
			}
		},
		show: function(autoHide) {
			if (!this._target) { return; }
			var info = this._target.getTooltipInfo();
			if (!info) { return; }
			var domNode = this._domNode;
			domNode.style.left = domNode.style.right = domNode.style.width = domNode.style.height = "auto";
			var contents = info.contents, contentsDiv;
			if (contents instanceof Array) {
				contents = this._getAnnotationContents(contents);
			}
			if (typeof contents === "string") {
				(contentsDiv = this._htmlParent).innerHTML = contents;
			} else if (contents instanceof Node) {
				(contentsDiv = this._htmlParent).appendChild(contents);
			} else if (contents instanceof orion.textview.ProjectionTextModel) {
				if (!this._contentsView) {
					this._emptyModel = new orion.textview.TextModel("");
					//TODO need hook into setup.js (or editor.js) to create a text view (and styler)
					var newView = this._contentsView = new orion.textview.TextView({
						model: this._emptyModel,
						parent: this._viewParent,
						tabSize: 4,
						stylesheet: ["/orion/textview/tooltip.css", "/orion/textview/rulers.css",
							"/examples/textview/textstyler.css", "/css/default-theme.css"]
					});
					//TODO this is need to avoid IE from getting focus
					newView._clientDiv.contentEditable = false;
					//TODO need to find a better way of sharing the styler for multiple views
					var view = this._view;
					newView.addEventListener("LineStyle", view, view.onLineStyle);
				}
				var contentsView = this._contentsView;
				contentsView.setModel(contents);
				var size = contentsView.computeSize();
				contentsDiv = this._viewParent;
				//TODO always make the width larger than the size of the scrollbar to avoid bug in updatePage
				contentsDiv.style.width = (size.width + 20) + "px";
				contentsDiv.style.height = size.height + "px";
			} else {
				return;
			}
			contentsDiv.style.left = "auto";
			contentsDiv.style.position = "static";
			contentsDiv.style.visibility = "visible";
			var left = parseInt(this._getNodeStyle(domNode, "padding-left", "0"), 10);
			left += parseInt(this._getNodeStyle(domNode, "border-left-width", "0"), 10);
			if (info.anchor === "right") {
				var right = parseInt(this._getNodeStyle(domNode, "padding-right", "0"), 10);
				right += parseInt(this._getNodeStyle(domNode, "border-right-width", "0"), 10);
				domNode.style.right = (domNode.ownerDocument.body.getBoundingClientRect().right - info.x + left + right) + "px";
			} else {
				domNode.style.left = (info.x - left) + "px";
			}
			var top = parseInt(this._getNodeStyle(domNode, "padding-top", "0"), 10);
			top += parseInt(this._getNodeStyle(domNode, "border-top-width", "0"), 10);
			domNode.style.top = (info.y - top) + "px";
			domNode.style.maxWidth = info.maxWidth + "px";
			domNode.style.maxHeight = info.maxHeight + "px";
			domNode.style.opacity = "1";
			domNode.style.visibility = "visible";
			if (autoHide) {
				var self = this;
				self._hideTimeout = setTimeout(function() {
					var opacity = parseFloat(self._getNodeStyle(domNode, "opacity", "1"));
					self._fadeTimeout = setInterval(function() {
						if (domNode.style.visibility === "visible" && opacity > 0) {
							opacity -= 0.1;
							domNode.style.opacity = opacity;
							return;
						}
						self.hide();
					}, 50);
				}, 5000);
			}
		},
		_getAnnotationContents: function(annotations) {
			if (annotations.length === 0) {
				return null;
			}
			var model = this._view.getModel(), annotation;
			var baseModel = model.getBaseModel ? model.getBaseModel() : model;
			function getText(start, end) {
				var textStart = baseModel.getLineStart(baseModel.getLineAtOffset(start));
				var textEnd = baseModel.getLineEnd(baseModel.getLineAtOffset(end), true);
				return baseModel.getText(textStart, textEnd);
			}
			var title;
			if (annotations.length === 1) {
				annotation = annotations[0];
				if (annotation.title) {
					title = annotation.title.replace(/</g, "&lt;").replace(/>/g, "&gt;");
					return annotation.html + "&nbsp;" + title;
				} else {
					var newModel = new orion.textview.ProjectionTextModel(baseModel);
					var lineStart = baseModel.getLineStart(baseModel.getLineAtOffset(annotation.start));
					newModel.addProjection({start: annotation.end, end: newModel.getCharCount()});
					newModel.addProjection({start: 0, end: lineStart});
					return newModel;
				}
			} else {
				var tooltipHTML = "<em>Multiple annotations:</em><br>";
				for (var i = 0; i < annotations.length; i++) {
					annotation = annotations[i];
					title = annotation.title;
					if (!title) {
						title = getText(annotation.start, annotation.end);
					}
					title = title.replace(/</g, "&lt;").replace(/>/g, "&gt;");
					tooltipHTML += annotation.html + "&nbsp;" + title + "<br>";
				}
				return tooltipHTML;
			}
		},
		_getNodeStyle: function(node, prop, defaultValue) {
			var value;
			if (node) {
				value = node.style[prop];
				if (!value) {
					if (node.currentStyle) {
						var index = 0, p = prop;
						while ((index = p.indexOf("-", index)) !== -1) {
							p = p.substring(0, index) + p.substring(index + 1, index + 2).toUpperCase() + p.substring(index + 2);
						}
						value = node.currentStyle[p];
					} else {
						var css = node.ownerDocument.defaultView.getComputedStyle(node, null);
						value = css ? css.getPropertyValue(prop) : null;
					}
				}
			}
			return value || defaultValue;
		}
	};
	return Tooltip;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.textview;
	});
}/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: 
 *		Felipe Heidrich (IBM Corporation) - initial API and implementation
 *		Silenio Quarti (IBM Corporation) - initial API and implementation
 *		Mihai Sucan (Mozilla Foundation) - fix for Bug#334583 Bug#348471 Bug#349485 Bug#350595 Bug#360726 Bug#361180 Bug#358623 Bug#362286 Bug#362107 Bug#362428 Bug#362835 Bug#363508
 ******************************************************************************/

/*global window document navigator setTimeout clearTimeout XMLHttpRequest define */

/**
 * @namespace The global container for Orion APIs.
 */ 
var orion = orion || {};
/**
 * @namespace The container for textview APIs.
 */ 
orion.textview = orion.textview || {};

/**
 * Constructs a new text view.
 * 
 * @param options the view options.
 * @param {String|DOMElement} options.parent the parent element for the view, it can be either a DOM element or an ID for a DOM element.
 * @param {orion.textview.TextModel} [options.model] the text model for the view. If this options is not set the view creates an empty {@link orion.textview.TextModel}.
 * @param {Boolean} [options.readonly=false] whether or not the view is read-only.
 * @param {Boolean} [options.fullSelection=true] whether or not the view is in full selection mode.
 * @param {String|String[]} [options.stylesheet] one or more stylesheet URIs for the view.
 * @param {Number} [options.tabSize] The number of spaces in a tab.
 * 
 * @class A TextView is a user interface for editing text.
 * @name orion.textview.TextView
 */
orion.textview.TextView = (function() {

	/** @private */
	function addHandler(node, type, handler, capture) {
		if (typeof node.addEventListener === "function") {
			node.addEventListener(type, handler, capture === true);
		} else {
			node.attachEvent("on" + type, handler);
		}
	}
	/** @private */
	function removeHandler(node, type, handler, capture) {
		if (typeof node.removeEventListener === "function") {
			node.removeEventListener(type, handler, capture === true);
		} else {
			node.detachEvent("on" + type, handler);
		}
	}
	var isIE = document.selection && window.ActiveXObject && /MSIE/.test(navigator.userAgent) ? document.documentMode : undefined;
	var isFirefox = parseFloat(navigator.userAgent.split("Firefox/")[1] || navigator.userAgent.split("Minefield/")[1]) || undefined;
	var isOpera = navigator.userAgent.indexOf("Opera") !== -1;
	var isChrome = navigator.userAgent.indexOf("Chrome") !== -1;
	var isSafari = navigator.userAgent.indexOf("Safari") !== -1;
	var isWebkit = navigator.userAgent.indexOf("WebKit") !== -1;
	var isPad = navigator.userAgent.indexOf("iPad") !== -1;
	var isMac = navigator.platform.indexOf("Mac") !== -1;
	var isWindows = navigator.platform.indexOf("Win") !== -1;
	var isLinux = navigator.platform.indexOf("Linux") !== -1;
	var isW3CEvents = typeof window.document.documentElement.addEventListener === "function";
	var isRangeRects = (!isIE || isIE >= 9) && typeof window.document.createRange().getBoundingClientRect === "function";
	var isDnD = isFirefox || isWebkit; // drag and drop support
	var platformDelimiter = isWindows ? "\r\n" : "\n";
	
	/** 
	 * Constructs a new Selection object.
	 * 
	 * @class A Selection represents a range of selected text in the view.
	 * @name orion.textview.Selection
	 */
	var Selection = (function() {
		/** @private */
		function Selection (start, end, caret) {
			/**
			 * The selection start offset.
			 *
			 * @name orion.textview.Selection#start
			 */
			this.start = start;
			/**
			 * The selection end offset.
			 *
			 * @name orion.textview.Selection#end
			 */
			this.end = end;
			/** @private */
			this.caret = caret; //true if the start, false if the caret is at end
		}
		Selection.prototype = /** @lends orion.textview.Selection.prototype */ {
			/** @private */
			clone: function() {
				return new Selection(this.start, this.end, this.caret);
			},
			/** @private */
			collapse: function() {
				if (this.caret) {
					this.end = this.start;
				} else {
					this.start = this.end;
				}
			},
			/** @private */
			extend: function (offset) {
				if (this.caret) {
					this.start = offset;
				} else {
					this.end = offset;
				}
				if (this.start > this.end) {
					var tmp = this.start;
					this.start = this.end;
					this.end = tmp;
					this.caret = !this.caret;
				}
			},
			/** @private */
			setCaret: function(offset) {
				this.start = offset;
				this.end = offset;
				this.caret = false;
			},
			/** @private */
			getCaret: function() {
				return this.caret ? this.start : this.end;
			},
			/** @private */
			toString: function() {
				return "start=" + this.start + " end=" + this.end + (this.caret ? " caret is at start" : " caret is at end");
			},
			/** @private */
			isEmpty: function() {
				return this.start === this.end;
			},
			/** @private */
			equals: function(object) {
				return this.caret === object.caret && this.start === object.start && this.end === object.end;
			}
		};
		return Selection;
	}());

	/** 
	 * Constructs a new EventTable object.
	 * 
	 * @class 
	 * @name orion.textview.EventTable
	 * @private
	 */
	var EventTable = (function() {
		/** @private */
		function EventTable(){
		    this._types = {};
		}
		EventTable.prototype = /** @lends EventTable.prototype */ {
			/** @private */
			addEventListener: function(type, context, func, data) {
				var state = this._types[type];
				if (!state) {
					state = this._types[type] = {level: 0, listeners: []};
				}
				var listener = {
					context: context,
					func: func,
					data: data
				};
				var listeners = state.listeners;
				listeners.push(listener);
			},
			/** @private */
			sendEvent: function(type, event) {
				var state = this._types[type];
				if (state) {
					var listeners = state.listeners;
					try {
						state.level++;
						if (listeners) {
							for (var i=0, len=listeners.length; i < len; i++) {
								var l = listeners[i];
								if (l && l.context && l.func) {
									l.func.call(l.context, event, l.data);
								}
							}
						}
					} finally {
						state.level--;
						if (state.compact && state.level === 0) {
							for (var j=listeners.length - 1; j >= 0; j--) {
								if (!listeners[j]) {
									listeners.splice(j, 1);
								}
							}
							state.compact = false;
						}
					}
				}
			},
			/** @private */
			removeEventListener: function(type, context, func, data){
				var state = this._types[type];
				if (state) {
					var listeners = state.listeners;
					for (var i=0, len=listeners.length; i < len; i++) {
						var l = listeners[i];
						if (l && l.context === context && l.func === func && l.data === data) {
							if (state.level !== 0) {
								listeners[i] = null;
								state.compact = true;
							} else {
								listeners.splice(i, 1);
							}
							break;
						}
					}
				}
			}
		};
		return EventTable;
	}());
	
	/** @private */
	function TextView (options) {
		this._init(options);
	}
	
	TextView.prototype = /** @lends orion.textview.TextView.prototype */ {
		/**
		 * Adds an event listener to the text view.
		 * 
		 * @param {String} type the event type. The supported events are:
		 * <ul>
		 * <li>"Modify" See {@link #onModify} </li>
		 * <li>"Selection" See {@link #onSelection} </li>
		 * <li>"Scroll" See {@link #onScroll} </li>
		 * <li>"Verify" See {@link #onVerify} </li>
		 * <li>"Destroy" See {@link #onDestroy} </li>
		 * <li>"LineStyle" See {@link #onLineStyle} </li>
		 * <li>"ModelChanging" See {@link #onModelChanging} </li>
		 * <li>"ModelChanged" See {@link #onModelChanged} </li>
		 * </ul>
		 * @param {Object} context the context of the function.
		 * @param {Function} func the function that will be executed when the event happens. 
		 *   The function should take an event as the first parameter and optional data as the second parameter.
		 * @param {Object} [data] optional data passed to the function.
		 * 
		 * @see #removeEventListener
		 */
		addEventListener: function(type, context, func, data) {
			this._eventTable.addEventListener(type, context, func, data);
		},
		/**
		 * Adds a ruler to the text view.
		 *
		 * @param {orion.textview.Ruler} ruler the ruler.
		 */
		addRuler: function (ruler) {
			this._rulers.push(ruler);
			ruler.setView(this);
			this._createRuler(ruler);
			this._updatePage();
		},
		computeSize: function() {
			var w = 0, h = 0;
			var model = this._model, clientDiv = this._clientDiv;
			var clientWidth = clientDiv.style.width;
			/*
			* Feature in WekKit. Webkit limits the width of the lines
			* computed below to the width of the client div.  This causes
			* the lines to be wrapped even though "pre" is set.  The fix
			* is to set the width of the client div to a larger number
			* before computing the lines width.  Note that this value is
			* reset to the appropriate value further down.
			*/
			if (isWebkit) {
				clientDiv.style.width = (0x7FFFF).toString() + "px";
			}
			var lineCount = model.getLineCount();
			var document = this._frameDocument;
			for (var lineIndex=0; lineIndex<lineCount; lineIndex++) {
				var child = this._getLineNode(lineIndex), dummy = null;
				if (!child || child.lineChanged) {
					child = dummy = this._createLine(clientDiv, null, document, lineIndex, model);
				}
				var rect = this._getLineBoundingClientRect(child);
				w = Math.max(w, rect.right - rect.left);
				h += rect.bottom - rect.top;
				if (dummy) { clientDiv.removeChild(dummy); }
			}
			if (isWebkit) {
				clientDiv.style.width = clientWidth;
			}
			var viewPadding = this._getViewPadding();
			w += viewPadding.right - viewPadding.left;
			h += viewPadding.bottom - viewPadding.top;
			return {width: w, height: h};
		},
		/**
		 * Converts the given rectangle from one coordinate spaces to another.
		 * <p>The supported coordinate spaces are:
		 * <ul>
		 *   <li>"document" - relative to document, the origin is the top-left corner of first line</li>
		 *   <li>"page" - relative to html page that contains the text view</li>
		 *   <li>"view" - relative to text view, the origin is the top-left corner of the view container</li>
		 * </ul>
		 * </p>
		 * <p>All methods in the view that take or return a position are in the document coordinate space.</p>
		 *
		 * @param rect the rectangle to convert.
		 * @param rect.x the x of the rectangle.
		 * @param rect.y the y of the rectangle.
		 * @param rect.width the width of the rectangle.
		 * @param rect.height the height of the rectangle.
		 * @param {String} from the source coordinate space.
		 * @param {String} to the destination coordinate space.
		 *
		 * @see #getLocationAtOffset
		 * @see #getOffsetAtLocation
		 * @see #getTopPixel
		 * @see #setTopPixel
		 */
		convert: function(rect, from, to) {
			var scroll = this._getScroll();
			var viewPad = this._getViewPadding();
			var frame = this._frame.getBoundingClientRect();
			var viewRect = this._viewDiv.getBoundingClientRect();
			switch(from) {
				case "document":
					if (rect.x !== undefined) {
						rect.x += - scroll.x + viewRect.left + viewPad.left;
					}
					if (rect.y !== undefined) {
						rect.y += - scroll.y + viewRect.top + viewPad.top;
					}
					break;
				case "page":
					if (rect.x !== undefined) {
						rect.x += - frame.left;
					}
					if (rect.y !== undefined) {
						rect.y += - frame.top;
					}
					break;
			}
			//At this point rect is in the widget coordinate space
			switch (to) {
				case "document":
					if (rect.x !== undefined) {
						rect.x += scroll.x - viewRect.left - viewPad.left;
					}
					if (rect.y !== undefined) {
						rect.y += scroll.y - viewRect.top - viewPad.top;
					}
					break;
				case "page":
					if (rect.x !== undefined) {
						rect.x += frame.left;
					}
					if (rect.y !== undefined) {
						rect.y += frame.top;
					}
					break;
			}
			return rect;
		},
		/**
		 * Destroys the text view. 
		 * <p>
		 * Removes the view from the page and frees all resources created by the view.
		 * Calling this function causes the "Destroy" event to be fire so that all components
		 * attached to view can release their references.
		 * </p>
		 *
		 * @see #onDestroy
		 */
		destroy: function() {
			/* Destroy rulers*/
			for (var i=0; i< this._rulers.length; i++) {
				this._rulers[i].setView(null);
			}
			this.rulers = null;
			
			/*
			* Note that when the frame is removed, the unload event is trigged
			* and the view contents and handlers is released properly by
			* destroyView().
			*/
			this._destroyFrame();

			var e = {};
			this.onDestroy(e);

			this._dragStartSelection = null;
			this._dropDestination = null;
			this._parent = null;
			this._parentDocument = null;
			this._model = null;
			this._selection = null;
			this._doubleClickSelection = null;
			this._eventTable = null;
			this._keyBindings = null;
			this._actions = null;
		},
		/**
		 * Gives focus to the text view.
		 */
		focus: function() {
			if (!this._clientDiv) { return; }
			/*
			* Feature in Chrome. When focus is called in the clientDiv without
			* setting selection the browser will set the selection to the first dom 
			* element, which can be above the client area. When this happen the 
			* browser also scrolls the window to show that element.
			* The fix is to call _updateDOMSelection() before calling focus().
			*/
			this._updateDOMSelection();
			if (isPad) {
				this._textArea.focus();
			} else {
				if (isOpera) { this._clientDiv.blur(); }
				this._clientDiv.focus();
			}
			/*
			* Feature in Safari. When focus is called the browser selects the clientDiv
			* itself. The fix is to call _updateDOMSelection() after calling focus().
			*/
			this._updateDOMSelection();
		},
		/**
		 * Returns all action names defined in the text view.
		 * <p>
		 * There are two types of actions, the predefined actions of the view 
		 * and the actions added by application code.
		 * </p>
		 * <p>
		 * The predefined actions are:
		 * <ul>
		 *   <li>Navigation actions. These actions move the caret collapsing the selection.</li>
		 *     <ul>
		 *       <li>"lineUp" - moves the caret up by one line</li>
		 *       <li>"lineDown" - moves the caret down by one line</li>
		 *       <li>"lineStart" - moves the caret to beginning of the current line</li>
		 *       <li>"lineEnd" - moves the caret to end of the current line </li>
		 *       <li>"charPrevious" - moves the caret to the previous character</li>
		 *       <li>"charNext" - moves the caret to the next character</li>
		 *       <li>"pageUp" - moves the caret up by one page</li>
		 *       <li>"pageDown" - moves the caret down by one page</li>
		 *       <li>"wordPrevious" - moves the caret to the previous word</li>
		 *       <li>"wordNext" - moves the caret to the next word</li>
		 *       <li>"textStart" - moves the caret to the beginning of the document</li>
		 *       <li>"textEnd" - moves the caret to the end of the document</li>
		 *     </ul>
		 *   <li>Selection actions. These actions move the caret extending the selection.</li>
		 *     <ul>
		 *       <li>"selectLineUp" - moves the caret up by one line</li>
		 *       <li>"selectLineDown" - moves the caret down by one line</li>
		 *       <li>"selectLineStart" - moves the caret to beginning of the current line</li>
		 *       <li>"selectLineEnd" - moves the caret to end of the current line </li>
		 *       <li>"selectCharPrevious" - moves the caret to the previous character</li>
		 *       <li>"selectCharNext" - moves the caret to the next character</li>
		 *       <li>"selectPageUp" - moves the caret up by one page</li>
		 *       <li>"selectPageDown" - moves the caret down by one page</li>
		 *       <li>"selectWordPrevious" - moves the caret to the previous word</li>
		 *       <li>"selectWordNext" - moves the caret to the next word</li>
		 *       <li>"selectTextStart" - moves the caret to the beginning of the document</li>
		 *       <li>"selectTextEnd" - moves the caret to the end of the document</li>
		 *       <li>"selectAll" - selects the entire document</li>
		 *     </ul>
		 *   <li>Edit actions. These actions modify the text view text</li>
		 *     <ul>
		 *       <li>"deletePrevious" - deletes the character preceding the caret</li>
		 *       <li>"deleteNext" - deletes the charecter following the caret</li>
		 *       <li>"deleteWordPrevious" - deletes the word preceding the caret</li>
		 *       <li>"deleteWordNext" - deletes the word following the caret</li>
		 *       <li>"tab" - inserts a tab character at the caret</li>
		 *       <li>"enter" - inserts a line delimiter at the caret</li>
		 *     </ul>
		 *   <li>Clipboard actions.</li>
		 *     <ul>
		 *       <li>"copy" - copies the selected text to the clipboard</li>
		 *       <li>"cut" - copies the selected text to the clipboard and deletes the selection</li>
		 *       <li>"paste" - replaces the selected text with the clipboard contents</li>
		 *     </ul>
		 * </ul>
		 * </p>
		 *
		 * @param {Boolean} [defaultAction=false] whether or not the predefined actions are included.
		 * @returns {String[]} an array of action names defined in the text view.
		 *
		 * @see #invokeAction
		 * @see #setAction
		 * @see #setKeyBinding
		 * @see #getKeyBindings
		 */
		getActions: function (defaultAction) {
			var result = [];
			var actions = this._actions;
			for (var i = 0; i < actions.length; i++) {
				if (!defaultAction && actions[i].defaultHandler) { continue; }
				result.push(actions[i].name);
			}
			return result;
		},
		/**
		 * Returns the bottom index.
		 * <p>
		 * The bottom index is the line that is currently at the bottom of the view.  This
		 * line may be partially visible depending on the vertical scroll of the view. The parameter
		 * <code>fullyVisible</code> determines whether to return only fully visible lines. 
		 * </p>
		 *
		 * @param {Boolean} [fullyVisible=false] if <code>true</code>, returns the index of the last fully visible line. This
		 *    parameter is ignored if the view is not big enough to show one line.
		 * @returns {Number} the index of the bottom line.
		 *
		 * @see #getTopIndex
		 * @see #setTopIndex
		 */
		getBottomIndex: function(fullyVisible) {
			return this._getBottomIndex(fullyVisible);
		},
		/**
		 * Returns the bottom pixel.
		 * <p>
		 * The bottom pixel is the pixel position that is currently at
		 * the bottom edge of the view.  This position is relative to the
		 * beginning of the document.
		 * </p>
		 *
		 * @returns {Number} the bottom pixel.
		 *
		 * @see #getTopPixel
		 * @see #setTopPixel
		 * @see #convert
		 */
		getBottomPixel: function() {
			return this._getScroll().y + this._getClientHeight();
		},
		/**
		 * Returns the caret offset relative to the start of the document.
		 *
		 * @returns the caret offset relative to the start of the document.
		 *
		 * @see #setCaretOffset
		 * @see #setSelection
		 * @see #getSelection
		 */
		getCaretOffset: function () {
			var s = this._getSelection();
			return s.getCaret();
		},
		/**
		 * Returns the client area.
		 * <p>
		 * The client area is the portion in pixels of the document that is visible. The
		 * client area position is relative to the beginning of the document.
		 * </p>
		 *
		 * @returns the client area rectangle {x, y, width, height}.
		 *
		 * @see #getTopPixel
		 * @see #getBottomPixel
		 * @see #getHorizontalPixel
		 * @see #convert
		 */
		getClientArea: function() {
			var scroll = this._getScroll();
			return {x: scroll.x, y: scroll.y, width: this._getClientWidth(), height: this._getClientHeight()};
		},
		/**
		 * Returns the horizontal pixel.
		 * <p>
		 * The horizontal pixel is the pixel position that is currently at
		 * the left edge of the view.  This position is relative to the
		 * beginning of the document.
		 * </p>
		 *
		 * @returns {Number} the horizontal pixel.
		 *
		 * @see #setHorizontalPixel
		 * @see #convert
		 */
		getHorizontalPixel: function() {
			return this._getScroll().x;
		},
		/**
		 * Returns all the key bindings associated to the given action name.
		 *
		 * @param {String} name the action name.
		 * @returns {orion.textview.KeyBinding[]} the array of key bindings associated to the given action name.
		 *
		 * @see #setKeyBinding
		 * @see #setAction
		 */
		getKeyBindings: function (name) {
			var result = [];
			var keyBindings = this._keyBindings;
			for (var i = 0; i < keyBindings.length; i++) {
				if (keyBindings[i].name === name) {
					result.push(keyBindings[i].keyBinding);
				}
			}
			return result;
		},
		/**
		 * Returns the line height for a given line index.  Returns the default line
		 * height if the line index is not specified.
		 *
		 * @param {Number} [lineIndex] the line index.
		 * @returns {Number} the height of the line in pixels.
		 *
		 * @see #getLinePixel
		 */
		getLineHeight: function(lineIndex) {
			return this._getLineHeight();
		},
		/**
		 * Returns the top pixel position of a given line index relative to the beginning
		 * of the document.
		 * <p>
		 * Clamps out of range indices.
		 * </p>
		 *
		 * @param {Number} lineIndex the line index.
		 * @returns {Number} the pixel position of the line.
		 *
		 * @see #setTopPixel
		 * @see #convert
		 */
		getLinePixel: function(lineIndex) {
			lineIndex = Math.min(Math.max(0, lineIndex), this._model.getLineCount());
			var lineHeight = this._getLineHeight();
			return lineHeight * lineIndex;
		},
		/**
		 * Returns the {x, y} pixel location of the top-left corner of the character
		 * bounding box at the specified offset in the document.  The pixel location
		 * is relative to the document.
		 * <p>
		 * Clamps out of range offsets.
		 * </p>
		 *
		 * @param {Number} offset the character offset
		 * @returns the {x, y} pixel location of the given offset.
		 *
		 * @see #getOffsetAtLocation
		 * @see #convert
		 */
		getLocationAtOffset: function(offset) {
			var model = this._model;
			offset = Math.min(Math.max(0, offset), model.getCharCount());
			var lineIndex = model.getLineAtOffset(offset);
			var scroll = this._getScroll();
			var viewRect = this._viewDiv.getBoundingClientRect();
			var viewPad = this._getViewPadding();
			var x = this._getOffsetToX(offset) + scroll.x - viewRect.left - viewPad.left;
			var y = this.getLinePixel(lineIndex);
			return {x: x, y: y};
		},
		/**
		 * Returns the text model of the text view.
		 *
		 * @returns {orion.textview.TextModel} the text model of the view.
		 */
		getModel: function() {
			return this._model;
		},
		/**
		 * Returns the character offset nearest to the given pixel location.  The
		 * pixel location is relative to the document.
		 *
		 * @param x the x of the location
		 * @param y the y of the location
		 * @returns the character offset at the given location.
		 *
		 * @see #getLocationAtOffset
		 */
		getOffsetAtLocation: function(x, y) {
			var scroll = this._getScroll();
			var viewRect = this._viewDiv.getBoundingClientRect();
			var viewPad = this._getViewPadding();
			var lineIndex = this._getYToLine(y - scroll.y);
			x += -scroll.x + viewRect.left + viewPad.left;
			var offset = this._getXToOffset(lineIndex, x);
			return offset;
		},
		/**
		 * Get the view rulers.
		 *
		 * @returns the view rulers
		 *
		 * @see #addRuler
		 */
		getRulers: function() {
			return this._rulers.slice(0);
		},
		/**
		 * Returns the text view selection.
		 * <p>
		 * The selection is defined by a start and end character offset relative to the
		 * document. The character at end offset is not included in the selection.
		 * </p>
		 * 
		 * @returns {orion.textview.Selection} the view selection
		 *
		 * @see #setSelection
		 */
		getSelection: function () {
			var s = this._getSelection();
			return {start: s.start, end: s.end};
		},
		/**
		 * Returns the text for the given range.
		 * <p>
		 * The text does not include the character at the end offset.
		 * </p>
		 *
		 * @param {Number} [start=0] the start offset of text range.
		 * @param {Number} [end=char count] the end offset of text range.
		 *
		 * @see #setText
		 */
		getText: function(start, end) {
			var model = this._model;
			return model.getText(start, end);
		},
		/**
		 * Returns the top index.
		 * <p>
		 * The top index is the line that is currently at the top of the view.  This
		 * line may be partially visible depending on the vertical scroll of the view. The parameter
		 * <code>fullyVisible</code> determines whether to return only fully visible lines. 
		 * </p>
		 *
		 * @param {Boolean} [fullyVisible=false] if <code>true</code>, returns the index of the first fully visible line. This
		 *    parameter is ignored if the view is not big enough to show one line.
		 * @returns {Number} the index of the top line.
		 *
		 * @see #getBottomIndex
		 * @see #setTopIndex
		 */
		getTopIndex: function(fullyVisible) {
			return this._getTopIndex(fullyVisible);
		},
		/**
		 * Returns the top pixel.
		 * <p>
		 * The top pixel is the pixel position that is currently at
		 * the top edge of the view.  This position is relative to the
		 * beginning of the document.
		 * </p>
		 *
		 * @returns {Number} the top pixel.
		 *
		 * @see #getBottomPixel
		 * @see #setTopPixel
		 * @see #convert
		 */
		getTopPixel: function() {
			return this._getScroll().y;
		},
		/**
		 * Executes the action handler associated with the given name.
		 * <p>
		 * The application defined action takes precedence over predefined actions unless
		 * the <code>defaultAction</code> paramater is <code>true</code>.
		 * </p>
		 * <p>
		 * If the application defined action returns <code>false</code>, the text view predefined
		 * action is executed if present.
		 * </p>
		 *
		 * @param {String} name the action name.
		 * @param {Boolean} [defaultAction] whether to always execute the predefined action.
		 * @returns {Boolean} <code>true</code> if the action was executed.
		 *
		 * @see #setAction
		 * @see #getActions
		 */
		invokeAction: function (name, defaultAction) {
			var actions = this._actions;
			for (var i = 0; i < actions.length; i++) {
				var a = actions[i];
				if (a.name && a.name === name) {
					if (!defaultAction && a.userHandler) {
						if (a.userHandler()) { return; }
					}
					if (a.defaultHandler) { return a.defaultHandler(); }
					return false;
				}
			}
			return false;
		},
		/** 
		 * @class This is the event sent when the user right clicks or otherwise invokes the context menu of the view. 
		 * <p> 
		 * <b>See:</b><br/> 
		 * {@link orion.textview.TextView}<br/> 
		 * {@link orion.textview.TextView#event:onContextMenu} 
		 * </p> 
		 * 
		 * @name orion.textview.ContextMenuEvent 
		 * 
		 * @property {Number} x The pointer location on the x axis, relative to the document the user is editing. 
		 * @property {Number} y The pointer location on the y axis, relative to the document the user is editing. 
		 * @property {Number} screenX The pointer location on the x axis, relative to the screen. This is copied from the DOM contextmenu event.screenX property. 
		 * @property {Number} screenY The pointer location on the y axis, relative to the screen. This is copied from the DOM contextmenu event.screenY property. 
		 */ 
		/** 
		 * This event is sent when the user invokes the view context menu. 
		 * 
		 * @event 
		 * @param {orion.textview.ContextMenuEvent} contextMenuEvent the event 
		 */ 
		onContextMenu: function(contextMenuEvent) { 
		  this._eventTable.sendEvent("ContextMenu", contextMenuEvent); 
		}, 
		/**
		 * @class This is the event sent when the text view is destroyed.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onDestroy}
		 * </p>
		 * @name orion.textview.DestroyEvent
		 */
		/**
		 * This event is sent when the text view has been destroyed.
		 *
		 * @event
		 * @param {orion.textview.DestroyEvent} destroyEvent the event
		 *
		 * @see #destroy
		 */
		onDestroy: function(destroyEvent) {
			this._eventTable.sendEvent("Destroy", destroyEvent);
		},
		/**
		 * @class This object is used to define style information for the text view.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onLineStyle}
		 * </p>		 
		 * @name orion.textview.Style
		 * 
		 * @property {String} styleClass A CSS class name.
		 * @property {Object} style An object with CSS properties.
		 * @property {String} tagName A DOM tag name.
		 * @property {Object} attributes An object with DOM attributes.
		 */
		/**
		 * @class This object is used to style range.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onLineStyle}
		 * </p>		 
		 * @name orion.textview.StyleRange
		 * 
		 * @property {Number} start The start character offset, relative to the document, where the style should be applied.
		 * @property {Number} end The end character offset (exclusive), relative to the document, where the style should be applied.
		 * @property {orion.textview.Style} style The style for the range.
		 */
		/**
		 * @class This is the event sent when the text view needs the style information for a line.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onLineStyle}
		 * </p>		 
		 * @name orion.textview.LineStyleEvent
		 * 
		 * @property {orion.textview.TextView} textView The text view.		 
		 * @property {Number} lineIndex The line index.
		 * @property {String} lineText The line text.
		 * @property {Number} lineStart The character offset, relative to document, of the first character in the line.
		 * @property {orion.textview.Style} style The style for the entire line (output argument).
		 * @property {orion.textview.StyleRange[]} ranges An array of style ranges for the line (output argument).		 
		 */
		/**
		 * This event is sent when the text view needs the style information for a line.
		 *
		 * @event
		 * @param {orion.textview.LineStyleEvent} lineStyleEvent the event
		 */
		onLineStyle: function(lineStyleEvent) {
			this._eventTable.sendEvent("LineStyle", lineStyleEvent);
		},
		/**
		 * @class This is the event sent when the text in the model has changed.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onModelChanged}<br/>
		 * {@link orion.textview.TextModel#onChanged}
		 * </p>
		 * @name orion.textview.ModelChangedEvent
		 * 
		 * @property {Number} start The character offset in the model where the change has occurred.
		 * @property {Number} removedCharCount The number of characters removed from the model.
		 * @property {Number} addedCharCount The number of characters added to the model.
		 * @property {Number} removedLineCount The number of lines removed from the model.
		 * @property {Number} addedLineCount The number of lines added to the model.
		 */
		/**
		 * This event is sent when the text in the model has changed.
		 *
		 * @event
		 * @param {orion.textview.ModelChangedEvent} modelChangedEvent the event
		 */
		onModelChanged: function(modelChangedEvent) {
			this._eventTable.sendEvent("ModelChanged", modelChangedEvent);
		},
		/**
		 * @class This is the event sent when the text in the model is about to change.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onModelChanging}<br/>
		 * {@link orion.textview.TextModel#onChanging}
		 * </p>
		 * @name orion.textview.ModelChangingEvent
		 * 
		 * @property {String} text The text that is about to be inserted in the model.
		 * @property {Number} start The character offset in the model where the change will occur.
		 * @property {Number} removedCharCount The number of characters being removed from the model.
		 * @property {Number} addedCharCount The number of characters being added to the model.
		 * @property {Number} removedLineCount The number of lines being removed from the model.
		 * @property {Number} addedLineCount The number of lines being added to the model.
		 */
		/**
		 * This event is sent when the text in the model is about to change.
		 *
		 * @event
		 * @param {orion.textview.ModelChangingEvent} modelChangingEvent the event
		 */
		onModelChanging: function(modelChangingEvent) {
			this._eventTable.sendEvent("ModelChanging", modelChangingEvent);
		},
		/**
		 * @class This is the event sent when the text is modified by the text view.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onModify}
		 * </p>
		 * @name orion.textview.ModifyEvent
		 */
		/**
		 * This event is sent when the text view has changed text in the model.
		 * <p>
		 * If the text is changed directly through the model API, this event
		 * is not sent.
		 * </p>
		 *
		 * @event
		 * @param {orion.textview.ModifyEvent} modifyEvent the event
		 */
		onModify: function(modifyEvent) {
			this._eventTable.sendEvent("Modify", modifyEvent);
		},
		/**
		 * @class This is the event sent when the selection changes in the text view.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onSelection}
		 * </p>		 
		 * @name orion.textview.SelectionEvent
		 * 
		 * @property {orion.textview.Selection} oldValue The old selection.
		 * @property {orion.textview.Selection} newValue The new selection.
		 */
		/**
		 * This event is sent when the text view selection has changed.
		 *
		 * @event
		 * @param {orion.textview.SelectionEvent} selectionEvent the event
		 */
		onSelection: function(selectionEvent) {
			this._eventTable.sendEvent("Selection", selectionEvent);
		},
		/**
		 * @class This is the event sent when the text view scrolls.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onScroll}
		 * </p>		 
		 * @name orion.textview.ScrollEvent
		 * 
		 * @property oldValue The old scroll {x,y}.
		 * @property newValue The new scroll {x,y}.
		 */
		/**
		 * This event is sent when the text view scrolls vertically or horizontally.
		 *
		 * @event
		 * @param {orion.textview.ScrollEvent} scrollEvent the event
		 */
		onScroll: function(scrollEvent) {
			this._eventTable.sendEvent("Scroll", scrollEvent);
		},
		/**
		 * @class This is the event sent when the text is about to be modified by the text view.
		 * <p>
		 * <b>See:</b><br/>
		 * {@link orion.textview.TextView}<br/>
		 * {@link orion.textview.TextView#event:onVerify}
		 * </p>
		 * @name orion.textview.VerifyEvent
		 * 
		 * @property {String} text The text being inserted.
		 * @property {Number} start The start offset of the text range to be replaced.
		 * @property {Number} end The end offset (exclusive) of the text range to be replaced.
		 */
		/**
		 * This event is sent when the text view is about to change text in the model.
		 * <p>
		 * If the text is changed directly through the model API, this event
		 * is not sent.
		 * </p>
		 * <p>
		 * Listeners are allowed to change these parameters. Setting text to null
		 * or undefined stops the change.
		 * </p>
		 *
		 * @event
		 * @param {orion.textview.VerifyEvent} verifyEvent the event
		 */
		onVerify: function(verifyEvent) {
			this._eventTable.sendEvent("Verify", verifyEvent);
		},
		/**
		 * Redraws the text in the given line range.
		 * <p>
		 * The line at the end index is not redrawn.
		 * </p>
		 *
		 * @param {Number} [startLine=0] the start line
		 * @param {Number} [endLine=line count] the end line
		 */
		redrawLines: function(startLine, endLine, ruler) {
			if (this._redrawCount > 0) { return; }
			if (startLine === undefined) { startLine = 0; }
			if (endLine === undefined) { endLine = this._model.getLineCount(); }
			if (startLine === endLine) { return; }
			var div = this._clientDiv;
			if (!div) { return; }
			if (ruler) {
				var location = ruler.getLocation();//"left" or "right"
				var divRuler = location === "left" ? this._leftDiv : this._rightDiv;
				var cells = divRuler.firstChild.rows[0].cells;
				for (var i = 0; i < cells.length; i++) {
					if (cells[i].firstChild._ruler === ruler) {
						div = cells[i].firstChild;
						break;
					}
				}
			}
			if (ruler) {
				div.rulerChanged = true;
			}
			if (!ruler || ruler.getOverview() === "page") {
				var child = div.firstChild;
				while (child) {
					var lineIndex = child.lineIndex;
					if (startLine <= lineIndex && lineIndex < endLine) {
						child.lineChanged = true;
					}
					child = child.nextSibling;
				}
			}
			if (!ruler) {
				if (startLine <= this._maxLineIndex && this._maxLineIndex < endLine) {
					this._checkMaxLineIndex = this._maxLineIndex;
					this._maxLineIndex = -1;
					this._maxLineWidth = 0;
				}
			}
			this._queueUpdatePage();
		},
		/**
		 * Redraws the text in the given range.
		 * <p>
		 * The character at the end offset is not redrawn.
		 * </p>
		 *
		 * @param {Number} [start=0] the start offset of text range
		 * @param {Number} [end=char count] the end offset of text range
		 */
		redrawRange: function(start, end) {
			var model = this._model;
			if (start === undefined) { start = 0; }
			if (end === undefined) { end = model.getCharCount(); }
			if (start === end) { return; }
			var startLine = model.getLineAtOffset(start);
			var endLine = model.getLineAtOffset(Math.max(0, end - 1)) + 1;
			this.redrawLines(startLine, endLine);
		},
		/**
		 * Removes an event listener from the text view.
		 * <p>
		 * All the parameters must be the same ones used to add the listener.
		 * </p>
		 * 
		 * @param {String} type the event type.
		 * @param {Object} context the context of the function.
		 * @param {Function} func the function that will be executed when the event happens. 
		 * @param {Object} [data] optional data passed to the function.
		 * 
		 * @see #addEventListener
		 */
		removeEventListener: function(type, context, func, data) {
			this._eventTable.removeEventListener(type, context, func, data);
		},
		/**
		 * Removes a ruler from the text view.
		 *
		 * @param {orion.textview.Ruler} ruler the ruler.
		 */
		removeRuler: function (ruler) {
			var rulers = this._rulers;
			for (var i=0; i<rulers.length; i++) {
				if (rulers[i] === ruler) {
					rulers.splice(i, 1);
					ruler.setView(null);
					this._destroyRuler(ruler);
					this._updatePage();
					break;
				}
			}
		},
		/**
		 * Associates an application defined handler to an action name.
		 * <p>
		 * If the action name is a predefined action, the given handler executes before
		 * the default action handler.  If the given handler returns <code>true</code>, the
		 * default action handler is not called.
		 * </p>
		 *
		 * @param {String} name the action name.
		 * @param {Function} handler the action handler.
		 *
		 * @see #getActions
		 * @see #invokeAction
		 */
		setAction: function(name, handler) {
			if (!name) { return; }
			var actions = this._actions;
			for (var i = 0; i < actions.length; i++) {
				var a = actions[i];
				if (a.name === name) {
					a.userHandler = handler;
					return;
				}
			}
			actions.push({name: name, userHandler: handler});
		},
		/**
		 * Associates a key binding with the given action name. Any previous
		 * association with the specified key binding is overwriten. If the
		 * action name is <code>null</code>, the association is removed.
		 * 
		 * @param {orion.textview.KeyBinding} keyBinding the key binding
		 * @param {String} name the action
		 */
		setKeyBinding: function(keyBinding, name) {
			var keyBindings = this._keyBindings;
			for (var i = 0; i < keyBindings.length; i++) {
				var kb = keyBindings[i]; 
				if (kb.keyBinding.equals(keyBinding)) {
					if (name) {
						kb.name = name;
					} else {
						if (kb.predefined) {
							kb.name = null;
						} else {
							var oldName = kb.name; 
							keyBindings.splice(i, 1);
							var index = 0;
							while (index < keyBindings.length && oldName !== keyBindings[index].name) {
								index++;
							}
							if (index === keyBindings.length) {
								/* <p>
								 * Removing all the key bindings associated to an user action will cause
								 * the user action to be removed. TextView predefined actions are never
								 * removed (so they can be reinstalled in the future). 
								 * </p>
								 */
								var actions = this._actions;
								for (var j = 0; j < actions.length; j++) {
									if (actions[j].name === oldName) {
										if (!actions[j].defaultHandler) {
											actions.splice(j, 1);
										}
									}
								}
							}
						}
					}
					return;
				}
			}
			if (name) {
				keyBindings.push({keyBinding: keyBinding, name: name});
			}
		},
		/**
		 * Sets the caret offset relative to the start of the document.
		 *
		 * @param {Number} caret the caret offset relative to the start of the document.
		 * @param {Boolean} [show=true] if <code>true</code>, the view will scroll if needed to show the caret location.
		 *
		 * @see #getCaretOffset
		 * @see #setSelection
		 * @see #getSelection
		 */
		setCaretOffset: function(offset, show) {
			var charCount = this._model.getCharCount();
			offset = Math.max(0, Math.min (offset, charCount));
			var selection = new Selection(offset, offset, false);
			this._setSelection (selection, show === undefined || show);
		},
		/**
		 * Sets the horizontal pixel.
		 * <p>
		 * The horizontal pixel is the pixel position that is currently at
		 * the left edge of the view.  This position is relative to the
		 * beginning of the document.
		 * </p>
		 *
		 * @param {Number} pixel the horizontal pixel.
		 *
		 * @see #getHorizontalPixel
		 * @see #convert
		 */
		setHorizontalPixel: function(pixel) {
			pixel = Math.max(0, pixel);
			this._scrollView(pixel - this._getScroll().x, 0);
		},
		setRedraw: function(redraw) {
			if (redraw) {
				if (--this._redrawCount === 0) {
					var lineCount = this._model.getLineCount();
					var rulers = this.getRulers();
					for (var i = 0; i < rulers.length; i++) {
						this.redrawLines(0, lineCount, rulers[i]);
					}
					this.redrawLines(0, lineCount); 
					this._queueUpdatePage();
				}
			} else {
				this._redrawCount++;
			}
		},
		/**
		 * Sets the text model of the text view.
		 *
		 * @param {orion.textview.TextModel} model the text model of the view.
		 */
		setModel: function(model) {
			if (!model) { return; }
			if (model === this._model) { return; }
			this._model.removeListener(this._modelListener);
			var oldLineCount = this._model.getLineCount();
			var oldCharCount = this._model.getCharCount();
			var newLineCount = model.getLineCount();
			var newCharCount = model.getCharCount();
			var newText = model.getText();
			var e = {
				text: newText,
				start: 0,
				removedCharCount: oldCharCount,
				addedCharCount: newCharCount,
				removedLineCount: oldLineCount,
				addedLineCount: newLineCount
			};
			this.onModelChanging(e);
			this._model = model;
			e = {
				start: 0,
				removedCharCount: oldCharCount,
				addedCharCount: newCharCount,
				removedLineCount: oldLineCount,
				addedLineCount: newLineCount
			};
			this.onModelChanged(e); 
			this._model.addListener(this._modelListener);
			this._reset();
			this._updatePage();
		},
		/**
		 * Sets the text view selection.
		 * <p>
		 * The selection is defined by a start and end character offset relative to the
		 * document. The character at end offset is not included in the selection.
		 * </p>
		 * <p>
		 * The caret is always placed at the end offset. The start offset can be
		 * greater than the end offset to place the caret at the beginning of the
		 * selection.
		 * </p>
		 * <p>
		 * Clamps out of range offsets.
		 * </p>
		 * 
		 * @param {Number} start the start offset of the selection
		 * @param {Number} end the end offset of the selection
		 * @param {Boolean} [show=true] if <code>true</code>, the view will scroll if needed to show the caret location.
		 *
		 * @see #getSelection
		 */
		setSelection: function (start, end, show) {
			var caret = start > end;
			if (caret) {
				var tmp = start;
				start = end;
				end = tmp;
			}
			var charCount = this._model.getCharCount();
			start = Math.max(0, Math.min (start, charCount));
			end = Math.max(0, Math.min (end, charCount));
			var selection = new Selection(start, end, caret);
			this._setSelection(selection, show === undefined || show);
		},
		/**
		 * Replaces the text in the given range with the given text.
		 * <p>
		 * The character at the end offset is not replaced.
		 * </p>
		 * <p>
		 * When both <code>start</code> and <code>end</code> parameters
		 * are not specified, the text view places the caret at the beginning
		 * of the document and scrolls to make it visible.
		 * </p>
		 *
		 * @param {String} text the new text.
		 * @param {Number} [start=0] the start offset of text range.
		 * @param {Number} [end=char count] the end offset of text range.
		 *
		 * @see #getText
		 */
		setText: function (text, start, end) {
			var reset = start === undefined && end === undefined;
			if (start === undefined) { start = 0; }
			if (end === undefined) { end = this._model.getCharCount(); }
			this._modifyContent({text: text, start: start, end: end, _code: true}, !reset);
			if (reset) {
				this._columnX = -1;
				this._setSelection(new Selection (0, 0, false), true);
				
				/*
				* Bug in Firefox.  For some reason, the caret does not show after the
				* view is refreshed.  The fix is to toggle the contentEditable state and
				* force the clientDiv to loose and receive focus if the it is focused.
				*/
				if (isFirefox) {
					var clientDiv = this._clientDiv;
					if (clientDiv) {
						var hasFocus = this._hasFocus;
						if (hasFocus) { clientDiv.blur(); }
						clientDiv.contentEditable = false;
						clientDiv.contentEditable = true;
						if (hasFocus) { clientDiv.focus(); }
					}
				}
			}
		},
		/**
		 * Sets the top index.
		 * <p>
		 * The top index is the line that is currently at the top of the text view.  This
		 * line may be partially visible depending on the vertical scroll of the view.
		 * </p>
		 *
		 * @param {Number} topIndex the index of the top line.
		 *
		 * @see #getBottomIndex
		 * @see #getTopIndex
		 */
		setTopIndex: function(topIndex) {
			var model = this._model;
			if (model.getCharCount() === 0) {
				return;
			}
			var lineCount = model.getLineCount();
			var lineHeight = this._getLineHeight();
			var pageSize = Math.max(1, Math.min(lineCount, Math.floor(this._getClientHeight () / lineHeight)));
			if (topIndex < 0) {
				topIndex = 0;
			} else if (topIndex > lineCount - pageSize) {
				topIndex = lineCount - pageSize;
			}
			var pixel = topIndex * lineHeight - this._getScroll().y;
			this._scrollView(0, pixel);
		},
		/**
		 * Sets the top pixel.
		 * <p>
		 * The top pixel is the pixel position that is currently at
		 * the top edge of the view.  This position is relative to the
		 * beginning of the document.
		 * </p>
		 *
		 * @param {Number} pixel the top pixel.
		 *
		 * @see #getBottomPixel
		 * @see #getTopPixel
		 * @see #convert
		 */
		setTopPixel: function(pixel) {
			var lineHeight = this._getLineHeight();
			var clientHeight = this._getClientHeight();
			var lineCount = this._model.getLineCount();
			pixel = Math.min(Math.max(0, pixel), lineHeight * lineCount - clientHeight);
			this._scrollView(0, pixel - this._getScroll().y);
		},
		/**
		 * Scrolls the selection into view if needed.
		 *
		 * @returns true if the view was scrolled. 
		 *
		 * @see #getSelection
		 * @see #setSelection
		 */
		showSelection: function() {
			return this._showCaret(true);
		},
		
		/**************************************** Event handlers *********************************/
		_handleBodyMouseDown: function (e) {
			if (!e) { e = window.event; }
			/*
			 * Prevent clicks outside of the view from taking focus 
			 * away the view. Note that in Firefox and Opera clicking on the 
			 * scrollbar also take focus from the view. Other browsers
			 * do not have this problem and stopping the click over the 
			 * scrollbar for them causes mouse capture problems.
			 */
			var topNode = isOpera ? this._clientDiv : this._overlayDiv || this._viewDiv;
			
			var temp = e.target ? e.target : e.srcElement;
			while (temp) {
				if (topNode === temp) {
					return;
				}
				temp = temp.parentNode;
			}
			if (e.preventDefault) { e.preventDefault(); }
			if (e.stopPropagation){ e.stopPropagation(); }
			if (!isW3CEvents) {
				/* In IE 8 is not possible to prevent the default handler from running
				*  during mouse down event using usual API. The workaround is to use
				*  setCapture/releaseCapture. 
				*/ 
				topNode.setCapture();
				setTimeout(function() { topNode.releaseCapture(); }, 0);
			}
		},
		_handleBlur: function (e) {
			if (!e) { e = window.event; }
			this._hasFocus = false;
			/*
			* Bug in IE 8 and earlier. For some reason when text is deselected
			* the overflow selection at the end of some lines does not get redrawn.
			* The fix is to create a DOM element in the body to force a redraw.
			*/
			if (isIE < 9) {
				if (!this._getSelection().isEmpty()) {
					var document = this._frameDocument;
					var child = document.createElement("DIV");
					var body = document.body;
					body.appendChild(child);
					body.removeChild(child);
				}
			}
			if (isFirefox || isIE) {
				if (this._selDiv1) {
					var color = isIE ? "transparent" : "#AFAFAF";
					this._selDiv1.style.background = color;
					this._selDiv2.style.background = color;
					this._selDiv3.style.background = color;
				}
			}
		},
		_handleContextMenu: function (e) {
			if (!e) { e = window.event; }
			var scroll = this._getScroll(); 
			var viewRect = this._viewDiv.getBoundingClientRect(); 
			var viewPad = this._getViewPadding(); 
			var x = e.clientX + scroll.x - viewRect.left - viewPad.left; 
			var y = e.clientY + scroll.y - viewRect.top - viewPad.top; 
			this.onContextMenu({x: x, y: y, screenX: e.screenX, screenY: e.screenY}); 
			if (e.preventDefault) { e.preventDefault(); }
			return false;
		},
		_handleCopy: function (e) {
			if (this._ignoreCopy) { return; }
			if (!e) { e = window.event; }
			if (this._doCopy(e)) {
				if (e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handleCut: function (e) {
			if (!e) { e = window.event; }
			if (this._doCut(e)) {
				if (e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handleDOMAttrModified: function (e) {
			if (!e) { e = window.event; }
			var ancestor = false;
			var parent = this._parent;
			while (parent) {
				if (parent === e.target) {
					ancestor = true;
					break;
				}
				parent = parent.parentNode;
			}
			if (!ancestor) { return; }
			var state = this._getVisible();
			if (state === "visible") {
				this._createView();
			} else if (state === "hidden") {
				this._destroyView();
			}
		},
		_handleDataModified: function(e) {
			this._startIME();
		},
		_handleDblclick: function (e) {
			if (!e) { e = window.event; }
			var time = e.timeStamp ? e.timeStamp : new Date().getTime();
			this._lastMouseTime = time;
			if (this._clickCount !== 2) {
				this._clickCount = 2;
				this._handleMouse(e);
			}
		},
		_handleDragStart: function (e) {
			if (!e) { e = window.event; }
			if (isDnD) {
				var sel = this._getSelection();
				var text = !sel.isEmpty() ? this._getBaseText(sel.start, sel.end) : "";
				if (text) {
					e.dataTransfer.effectAllowed = "copyMove";
					e.dataTransfer.setData("text/plain", text);
					// TODO: generate a drag image to be a better visual indicatator of the drag operation.
					this._dragStartSelection = {start: sel.start, end: sel.end};
					this.focus();
					return;
				}
			}
			if (e.preventDefault) { e.preventDefault(); }
			return false;
		},
		_handleDragEnd: function (e) {
			if (!e) { e = window.event; }
			if (e.preventDefault) { e.preventDefault(); }
			var startSel = this._dragStartSelection;
			var drop = this._dropDestination;
			if (startSel && e.dataTransfer.dropEffect === "move") {
				var offset = 0;
				if (drop && drop.offset < Math.min(startSel.start, startSel.end)) {
					offset = drop.length;
				}
				var change = {
					text: "",
					start: startSel.start + offset,
					end: startSel.end + offset
				};
				this._modifyContent(change, false);
			}
			if (this._undoStack && drop) {
				this._undoStack.endCompoundChange();
			}
			this._dragNode.draggable = false;
			this._dragStartSelection = null;
			this._dropDestination = null;
			return false;
		},
		_handleDragEnter: function (e) {
			if (!e) { e = window.event; }
			if (e.preventDefault) { e.preventDefault(); }
			var types = e.dataTransfer.types;
			var allowed = false;
			var types = isDnD ? e.dataTransfer.types : null;
			if (types) {
				// Firefox gives a .types of type StringList, while Webkit gives us an actual string.
				allowed = types.contains ? types.contains("text/plain") : types.indexOf("text/plain");
			}
			if (allowed) {
				e.dataTransfer.dropEffect = "copyMove";
				this.focus();
				return true;
			}
			e.dataTransfer.dropEffect = "none";
			return false;
		},
		_handleDragOver: function (e) {
			if (!e) { e = window.event; }
			if (e.preventDefault) { e.preventDefault(); }
			var allowed = false;
			var types = isDnD ? e.dataTransfer.types : null;
			if (types) {
				allowed = types.contains ? types.contains("text/plain") : types.indexOf("text/plain");
			}
			if (!allowed) {
				e.dataTransfer.dropEffect = "none";
				return false;
			}

			var destLine = this._getYToLine(e.clientY);
			var destOffset = this._getXToOffset(destLine, e.clientX);

			var startSel = this._dragStartSelection;
			if (startSel && startSel.start <= destOffset && destOffset <= startSel.end) {
				e.dataTransfer.dropEffect = "none";
				return false;
			}

			if (!startSel) {
				// Hide the selection when the user drags something coming from the outside.
				// TODO: make sure the cursor is actually visible. It's not visible in Firefox during drag, only in Chrome...
				this.setSelection(destOffset, destOffset, true);
			}

			return true;
		},
		_handleDrop: function (e) {
			if (!e) { e = window.event; }
			if (e.preventDefault) { e.preventDefault(); }
			var allowed = false;
			var types = isDnD ? e.dataTransfer.types : null;
			if (types) {
				allowed = types.contains ? types.contains("text/plain") : types.indexOf("text/plain");
			}
			if (!allowed) {
				return false;
			}

			var destLine = this._getYToLine(e.clientY);
			var destOffset = this._getXToOffset(destLine, e.clientX);
			var startSel = this._dragStartSelection;

			if (startSel && startSel.start <= destOffset && destOffset <= startSel.end) {
				return false;
			}

			var text = e.dataTransfer.getData("text/plain");
			this.setSelection(destOffset, destOffset, true);

			if (startSel) {
				this._dropDestination = {offset: destOffset, length: text.length};
				if (this._undoStack) {
					this._undoStack.startCompoundChange();
				}
			} else {
				this._dragNode.draggable = false;
			}

			this._doContent(text);
			this.focus();
			return true;
		},
		_handleDocFocus: function (e) {
			if (!e) { e = window.event; }
			this._clientDiv.focus();
		},
		_handleFocus: function (e) {
			if (!e) { e = window.event; }
			this._hasFocus = true;
			/*
			* Feature in IE.  The selection is not restored when the
			* view gets focus and the caret is always placed at the
			* beginning of the document.  The fix is to update the DOM
			* selection during the focus event.
			*/
			if (isIE) {
				this._updateDOMSelection();
			}
			if (isFirefox || isIE) {
				if (this._selDiv1) {
					var color = this._hightlightRGB;
					this._selDiv1.style.background = color;
					this._selDiv2.style.background = color;
					this._selDiv3.style.background = color;
				}
			}
		},
		_handleKeyDown: function (e) {
			if (!e) { e = window.event; }
			if (isPad) {
				if (e.keyCode === 8) {
					this._doBackspace({});
					e.preventDefault();
				}
				return;
			}
			switch (e.keyCode) {
				case 16: /* Shift */
				case 17: /* Control */
				case 18: /* Alt */
				case 91: /* Command */
					break;
				default:
					this._setLinksVisible(false);
			}
			if (e.keyCode === 229) {
				if (this.readonly) {
					if (e.preventDefault) { e.preventDefault(); }
					return false;
				}
				this._startIME();
			} else {
				this._commitIME();
			}
			/*
			* Feature in Firefox. When a key is held down the browser sends 
			* right number of keypress events but only one keydown. This is
			* unexpected and causes the view to only execute an action
			* just one time. The fix is to ignore the keydown event and 
			* execute the actions from the keypress handler.
			* Note: This only happens on the Mac and Linux (Firefox 3.6).
			*
			* Feature in Opera.  Opera sends keypress events even for non-printable
			* keys.  The fix is to handle actions in keypress instead of keydown.
			*/
			if (((isMac || isLinux) && isFirefox < 4) || isOpera) {
				this._keyDownEvent = e;
				return true;
			}
			
			if (this._doAction(e)) {
				if (e.preventDefault) {
					e.preventDefault(); 
				} else {
					e.cancelBubble = true;
					e.returnValue = false;
					e.keyCode = 0;
				}
				return false;
			}
		},
		_handleKeyPress: function (e) {
			if (!e) { e = window.event; }
			/*
			* Feature in Embedded WebKit.  Embedded WekKit on Mac runs in compatibility mode and
			* generates key press events for these Unicode values (Function keys).  This does not
			* happen in Safari or Chrome.  The fix is to ignore these key events.
			*/
			if (isMac && isWebkit) {
				if ((0xF700 <= e.keyCode && e.keyCode <= 0xF7FF) || e.keyCode === 13 || e.keyCode === 8) {
					if (e.preventDefault) { e.preventDefault(); }
					return false;
				}
			}
			if (((isMac || isLinux) && isFirefox < 4) || isOpera) {
				if (this._doAction(this._keyDownEvent)) {
					if (e.preventDefault) { e.preventDefault(); }
					return false;
				}
			}
			var ctrlKey = isMac ? e.metaKey : e.ctrlKey;
			if (e.charCode !== undefined) {
				if (ctrlKey) {
					switch (e.charCode) {
						/*
						* In Firefox and Safari if ctrl+v, ctrl+c ctrl+x is canceled
						* the clipboard events are not sent. The fix to allow
						* the browser to handles these key events.
						*/
						case 99://c
						case 118://v
						case 120://x
							return true;
					}
				}
			}
			var ignore = false;
			if (isMac) {
				if (e.ctrlKey || e.metaKey) { ignore = true; }
			} else {
				if (isFirefox) {
					//Firefox clears the state mask when ALT GR generates input
					if (e.ctrlKey || e.altKey) { ignore = true; }
				} else {
					//IE and Chrome only send ALT GR when input is generated
					if (e.ctrlKey ^ e.altKey) { ignore = true; }
				}
			}
			if (!ignore) {
				var key = isOpera ? e.which : (e.charCode !== undefined ? e.charCode : e.keyCode);
				if (key > 31) {
					this._doContent(String.fromCharCode (key));
					if (e.preventDefault) { e.preventDefault(); }
					return false;
				}
			}
		},
		_handleKeyUp: function (e) {
			if (!e) { e = window.event; }
			var ctrlKey = isMac ? e.metaKey : e.ctrlKey;
			if (!ctrlKey) {
				this._setLinksVisible(false);
			}
			// don't commit for space (it happens during JP composition)  
			if (e.keyCode === 13) {
				this._commitIME();
			}
		},
		_handleLinkClick: function (e) {
			if (!e) { e = window.event; }
			var ctrlKey = isMac ? e.metaKey : e.ctrlKey;
			if (!ctrlKey) {
				if (e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handleLoad: function (e) {
			var state = this._getVisible();
			if (state === "visible" || (state === "hidden" && isWebkit)) {
				this._createView(!e);
			}
		},
		_handleMouse: function (e) {
			var target = this._frameWindow;
			if (isIE) { target = this._clientDiv; }
			if (this._overlayDiv) {
				var self = this;
				setTimeout(function () {
					self.focus();
				}, 0);
			}
			if (this._clickCount === 1) {
				this._setGrab(target);
				this._setSelectionTo(e.clientX, e.clientY, e.shiftKey);
			} else {
				/*
				* Feature in IE8 and older, the sequence of events in the IE8 event model
				* for a doule-click is:
				*
				*	down
				*	up
				*	up
				*	dblclick
				*
				* Given that the mouse down/up events are not balanced, it is not possible to
				* grab on mouse down and ungrab on mouse up.  The fix is to grab on the first
				* mouse down and ungrab on mouse move when the button 1 is not set.
				*/
				if (isW3CEvents) { this._setGrab(target); }
				
				this._doubleClickSelection = null;
				this._setSelectionTo(e.clientX, e.clientY, e.shiftKey);
				this._doubleClickSelection = this._getSelection();
			}
		},
		_handleMouseDown: function (e) {
			if (!e) { e = window.event; }
			if (this._linksVisible) {
				var target = e.target || e.srcElement;
				if (target.tagName !== "A") {
					this._setLinksVisible(false);
				} else {
					return;
				}
			}
			var left = e.which ? e.button === 0 : e.button === 1;
			this._commitIME();
			if (left) {
				var deltaX = Math.abs(this._lastMouseX - e.clientX);
				var deltaY = Math.abs(this._lastMouseY - e.clientY);
				var time = e.timeStamp ? e.timeStamp : new Date().getTime();  
				if ((time - this._lastMouseTime) <= this._clickTime && deltaX <= this._clickDist && deltaY <= this._clickDist) {
					this._clickCount++;
				} else {
					this._clickCount = 1;
				}
				this._lastMouseX = e.clientX;
				this._lastMouseY = e.clientY;
				this._lastMouseTime = time;

				// Selection drag support
				if (isDnD && this._clickCount === 1) {
					var inSelection = false;
					var selection = this._getSelection();
					if (!selection.isEmpty()) {
						var clickLine = this._getYToLine(e.clientY);
						var clickOffset = this._getXToOffset(clickLine, e.clientX);
						inSelection = selection.start < clickOffset && clickOffset < selection.end;
					}

					// Webkit fails to allow dragging if .draggable is set to true during mousedown.
					// But Firefox makes it a requirement to set .draggable to true.
					this._dragNode.draggable = !isWebkit && inSelection;

					if (inSelection) {
						return; // allow the dragstart event
					}
				}
				if (this._dragNode && this._dragNode.draggable) {
					this._dragNode.draggable = false;
				}

				this._isMouseDown = true;
				this._handleMouse(e);
				if (isOpera || isChrome) {
					if (!this._hasFocus) {
						this.focus();
					}
				}
				if (e.preventDefault) {
					e.preventDefault();
				}
			}
		},
		_handleMouseMove: function (e) {
			if (!e) { e = window.event; }
			this._setLinksVisible(!this._isMouseDown && (isMac ? e.metaKey : e.ctrlKey));
			if (!this._isMouseDown) {
				return;
			}
			/*
			* Feature in IE8 and older, the sequence of events in the IE8 event model
			* for a doule-click is:
			*
			*	down
			*	up
			*	up
			*	dblclick
			*
			* Given that the mouse down/up events are not balanced, it is not possible to
			* grab on mouse down and ungrab on mouse up.  The fix is to grab on the first
			* mouse down and ungrab on mouse move when the button 1 is not set.
			*
			* In order to detect double-click and drag gestures, it is necessary to send
			* a mouse down event from mouse move when the button is still down and isMouseDown
			* flag is not set.
			*/
			if (!isW3CEvents) {
				if (e.button === 0) {
					this._setGrab(null);
					return true;
				}
				if (!this._isMouseDown && e.button === 1 && (this._clickCount & 1) !== 0) {
					this._clickCount = 2;
					return this._handleMouse(e, this._clickCount);
				}
			}
			
			var x = e.clientX;
			var y = e.clientY;
			if (isChrome) {
				if (e.currentTarget !== this._frameWindow) {
					var rect = this._frame.getBoundingClientRect();
					x -= rect.left;
					y -= rect.top;
				}
			}
			var viewPad = this._getViewPadding();
			var viewRect = this._viewDiv.getBoundingClientRect();
			var width = this._getClientWidth (), height = this._getClientHeight();
			var leftEdge = viewRect.left + viewPad.left;
			var topEdge = viewRect.top + viewPad.top;
			var rightEdge = viewRect.left + viewPad.left + width;
			var bottomEdge = viewRect.top + viewPad.top + height;
			var model = this._model;
			var caretLine = model.getLineAtOffset(this._getSelection().getCaret());
			if (y < topEdge && caretLine !== 0) {
				this._doAutoScroll("up", x, y - topEdge);
			} else if (y > bottomEdge && caretLine !== model.getLineCount() - 1) {
				this._doAutoScroll("down", x, y - bottomEdge);
			} else if (x < leftEdge) {
				this._doAutoScroll("left", x - leftEdge, y);
			} else if (x > rightEdge) {
				this._doAutoScroll("right", x - rightEdge, y);
			} else {
				this._endAutoScroll();
				this._setSelectionTo(x, y, true);
				/*
				* Feature in IE. IE does redraw the selection background right
				* away after the selection changes because of mouse move events.
				* The fix is to call getBoundingClientRect() on the
				* body element to force the selection to be redraw. Some how
				* calling this method forces a redraw.
				*/
				if (isIE) {
					var body = this._frameDocument.body;
					body.getBoundingClientRect();
				}
			}
		},
		_handleMouseUp: function (e) {
			if (!e) { e = window.event; }
			if (this._linksVisible) {
				return;
			}
			var left = e.which ? e.button === 0 : e.button === 1;
			if (left) {
				if (this._dragNode && this._dragNode.draggable) {
					this._dragNode.draggable = false;
          if (!this._dragStartSelection) {
            this._setSelectionTo(e.clientX, e.clientY, false);
          }
					this.focus();
				}

				this._isMouseDown = false;
				this._endAutoScroll();
				
				/*
				* Feature in IE8 and older, the sequence of events in the IE8 event model
				* for a doule-click is:
				*
				*	down
				*	up
				*	up
				*	dblclick
				*
				* Given that the mouse down/up events are not balanced, it is not possible to
				* grab on mouse down and ungrab on mouse up.  The fix is to grab on the first
				* mouse down and ungrab on mouse move when the button 1 is not set.
				*/
				if (isW3CEvents) { this._setGrab(null); }
			}
		},
		_handleMouseWheel: function (e) {
			if (!e) { e = window.event; }
			var lineHeight = this._getLineHeight();
			var pixelX = 0, pixelY = 0;
			// Note: On the Mac the correct behaviour is to scroll by pixel.
			if (isFirefox) {
				var pixel;
				if (isMac) {
					pixel = e.detail * 3;
				} else {
					var limit = 256;
					pixel = Math.max(-limit, Math.min(limit, e.detail)) * lineHeight;
				}
				if (e.axis === e.HORIZONTAL_AXIS) {
					pixelX = pixel;
				} else {
					pixelY = pixel;
				}
			} else {
				//Webkit
				if (isMac) {
					/*
					* In Safari, the wheel delta is a multiple of 120. In order to
					* convert delta to pixel values, it is necessary to divide delta
					* by 40.
					*
					* In Chrome and Safari 5, the wheel delta depends on the type of the
					* mouse. In general, it is the pixel value for Mac mice and track pads,
					* but it is a multiple of 120 for other mice. There is no presise
					* way to determine if it is pixel value or a multiple of 120.
					* 
					* Note that the current approach does not calculate the correct
					* pixel value for Mac mice when the delta is a multiple of 120.
					*/
					var denominatorX = 40, denominatorY = 40;
					if (e.wheelDeltaX % 120 !== 0) { denominatorX = 1; }
					if (e.wheelDeltaY % 120 !== 0) { denominatorY = 1; }
					pixelX = -e.wheelDeltaX / denominatorX;
					if (-1 < pixelX && pixelX < 0) { pixelX = -1; }
					if (0 < pixelX && pixelX < 1) { pixelX = 1; }
					pixelY = -e.wheelDeltaY / denominatorY;
					if (-1 < pixelY && pixelY < 0) { pixelY = -1; }
					if (0 < pixelY && pixelY < 1) { pixelY = 1; }
				} else {
					pixelX = -e.wheelDeltaX;
					var linesToScroll = 8;
					pixelY = (-e.wheelDeltaY / 120 * linesToScroll) * lineHeight;
				}
			}
			/* 
			* Feature in Safari. If the event target is removed from the DOM 
			* safari stops smooth scrolling. The fix is keep the element target
			* in the DOM and remove it on a later time. 
			*
			* Note: Using a timer is not a solution, because the timeout needs to
			* be at least as long as the gesture (which is too long).
			*/
			if (isSafari) {
				var lineDiv = e.target;
				while (lineDiv && lineDiv.lineIndex === undefined) {
					lineDiv = lineDiv.parentNode;
				}
				this._mouseWheelLine = lineDiv;
			}
			var oldScroll = this._getScroll();
			this._scrollView(pixelX, pixelY);
			var newScroll = this._getScroll();
			if (isSafari) { this._mouseWheelLine = null; }
			if (oldScroll.x !== newScroll.x || oldScroll.y !== newScroll.y) {
				if (e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handlePaste: function (e) {
			if (this._ignorePaste) { return; }
			if (!e) { e = window.event; }
			if (this._doPaste(e)) {
				if (isIE) {
					/*
					 * Bug in IE,  
					 */
					var self = this;
					setTimeout(function() {self._updateDOMSelection();}, 0);
				}
				if (e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handleResize: function (e) {
			if (!e) { e = window.event; }
			var element = this._frameDocument.documentElement;
			var newWidth = element.clientWidth;
			var newHeight = element.clientHeight;
			if (this._frameWidth !== newWidth || this._frameHeight !== newHeight) {
				this._frameWidth = newWidth;
				this._frameHeight = newHeight;
				this._updatePage();
			}
		},
		_handleRulerEvent: function (e) {
			if (!e) { e = window.event; }
			var target = e.target ? e.target : e.srcElement;
			var lineIndex = target.lineIndex;
			var element = target;
			while (element && !element._ruler) {
				if (lineIndex === undefined && element.lineIndex !== undefined) {
					lineIndex = element.lineIndex;
				}
				element = element.parentNode;
			}
			var ruler = element ? element._ruler : null;
			if (lineIndex === undefined && ruler && ruler.getOverview() === "document") {
				var buttonHeight = isPad ? 0 : 17;
				var clientHeight = this._getClientHeight ();
				var lineCount = this._model.getLineCount ();
				var viewPad = this._getViewPadding();
				var trackHeight = clientHeight + viewPad.top + viewPad.bottom - 2 * buttonHeight;
				lineIndex = Math.floor((e.clientY - buttonHeight) * lineCount / trackHeight);
				if (!(0 <= lineIndex && lineIndex < lineCount)) {
					lineIndex = undefined;
				}
			}
			if (ruler) {
				switch (e.type) {
					case "click":
						if (ruler.onClick) { ruler.onClick(lineIndex, e); }
						break;
					case "dblclick": 
						if (ruler.onDblClick) { ruler.onDblClick(lineIndex, e); }
						break;
					case "mousemove": 
						if (ruler.onMouseMove) { ruler.onMouseMove(lineIndex, e); }
						break;
					case "mouseover": 
						if (ruler.onMouseOver) { ruler.onMouseOver(lineIndex, e); }
						break;
					case "mouseout": 
						if (ruler.onMouseOut) { ruler.onMouseOut(lineIndex, e); }
						break;
				}
			}
		},
		_handleScroll: function () {
			var scroll = this._getScroll();
			var oldX = this._hScroll;
			var oldY = this._vScroll;
			if (oldX !== scroll.x || oldY !== scroll.y) {
				this._hScroll = scroll.x;
				this._vScroll = scroll.y;
				this._commitIME();
				this._updatePage();
				var e = {
					oldValue: {x: oldX, y: oldY},
					newValue: scroll
				};
				this.onScroll(e);
			}
		},
		_handleSelectStart: function (e) {
			if (!e) { e = window.event; }
			if (this._ignoreSelect) {
				if (e && e.preventDefault) { e.preventDefault(); }
				return false;
			}
		},
		_handleUnload: function (e) {
			if (!e) { e = window.event; }
			this._destroyView();
		},
		_handleInput: function (e) {
			var textArea = this._textArea;
			this._doContent(textArea.value);
			textArea.selectionStart = textArea.selectionEnd = 0;
			textArea.value = "";
			e.preventDefault();
		},
		_handleTextInput: function (e) {
			this._doContent(e.data);
			e.preventDefault();
		},
		_touchConvert: function (touch) {
			var rect = this._frame.getBoundingClientRect();
			var body = this._parentDocument.body;
			return {left: touch.clientX - rect.left - body.scrollLeft, top: touch.clientY - rect.top - body.scrollTop};
		},
		_handleTextAreaClick: function (e) {
			var pt = this._touchConvert(e);	
			this._clickCount = 1;
			this._ignoreDOMSelection = false;
			this._setSelectionTo(pt.left, pt.top, false);
			var textArea = this._textArea;
			textArea.focus();
		},
		_handleTouchStart: function (e) {
			var touches = e.touches, touch, pt, sel;
			this._touchMoved = false;
			this._touchStartScroll = undefined;
			if (touches.length === 1) {
				touch = touches[0];
				var pageX = touch.pageX;
				var pageY = touch.pageY;
				this._touchStartX = pageX;
				this._touchStartY = pageY;
				this._touchStartTime = e.timeStamp;
				this._touchStartScroll = this._getScroll();
				sel = this._getSelection();
				pt = this._touchConvert(touches[0]);
				this._touchGesture = "none";
				if (!sel.isEmpty()) {
					if (this._hitOffset(sel.end, pt.left, pt.top)) {
						this._touchGesture = "extendEnd";
					} else if (this._hitOffset(sel.start, pt.left, pt.top)) {
						this._touchGesture = "extendStart";
					}
				}
				if (this._touchGesture === "none") {
					var textArea = this._textArea;
					textArea.value = "";
					textArea.style.left = "-1000px";
					textArea.style.top = "-1000px";
					textArea.style.width = "3000px";
					textArea.style.height = "3000px";
				}
			} else if (touches.length === 2) {
				this._touchGesture = "select";
				if (this._touchTimeout) {
					clearTimeout(this._touchTimeout);
					this._touchTimeout = null;
				}
				pt = this._touchConvert(touches[0]);
				var offset1 = this._getXToOffset(this._getYToLine(pt.top), pt.left);
				pt = this._touchConvert(touches[1]);
				var offset2 = this._getXToOffset(this._getYToLine(pt.top), pt.left);
				sel = this._getSelection();
				sel.setCaret(offset1);
				sel.extend(offset2);
				this._setSelection(sel, true, true);
			}
			//Cannot prevent to show magnifier
//			e.preventDefault();
		},
		_handleTouchMove: function (e) {
			this._touchMoved = true;
			var touches = e.touches, pt, sel;
			if (touches.length === 1) {
				var touch = touches[0];
				var pageX = touch.pageX;
				var pageY = touch.pageY;
				var deltaX = this._touchStartX - pageX;
				var deltaY = this._touchStartY - pageY;
				pt = this._touchConvert(touch);
				sel = this._getSelection();
				if (this._touchGesture === "none") {
					if ((e.timeStamp - this._touchStartTime) < 200 && (Math.abs(deltaX) > 5 || Math.abs(deltaY) > 5)) {
						this._touchGesture = "scroll";
					} else {
						this._touchGesture = "caret";
					}
				}
				if (this._touchGesture === "select") {
					if (this._hitOffset(sel.end, pt.left, pt.top)) {
						this._touchGesture = "extendEnd";
					} else if (this._hitOffset(sel.start, pt.left, pt.top)) {
						this._touchGesture = "extendStart";
					} else {
						this._touchGesture = "caret";
					}
				}
				switch (this._touchGesture) {
					case "scroll":
						this._touchStartX = pageX;
						this._touchStartY = pageY;
						this._scrollView(deltaX, deltaY);
						break;
					case "extendStart":
					case "extendEnd":
						this._clickCount = 1;
						var lineIndex = this._getYToLine(pt.top);
						var offset = this._getXToOffset(lineIndex, pt.left);
						sel.setCaret(this._touchGesture === "extendStart" ? sel.end : sel.start);
						sel.extend(offset);
						if (offset >= sel.end && this._touchGesture === "extendStart") {
							this._touchGesture = "extendEnd";
						}
						if (offset <= sel.start && this._touchGesture === "extendEnd") {
							this._touchGesture = "extendStart";
						}
						this._setSelection(sel, true, true);
						break;
					case "caret":
						this._setSelectionTo(pt.left, pt.top, false);
						break;
				}
			} else if (touches.length === 2) {
				pt = this._touchConvert(touches[0]);
				var offset1 = this._getXToOffset(this._getYToLine(pt.top), pt.left);
				pt = this._touchConvert(touches[1]);
				var offset2 = this._getXToOffset(this._getYToLine(pt.top), pt.left);
				sel = this._getSelection();
				sel.setCaret(offset1);
				sel.extend(offset2);
				this._setSelection(sel, true, true);
			}
			e.preventDefault();
		},
		_handleTouchEnd: function (e) {
			var self = this;
			if (!this._touchMoved) {
				if (e.touches.length === 0 && e.changedTouches.length === 1) {
					var touch = e.changedTouches[0];
					var pt = this._touchConvert(touch);
					var textArea = this._textArea;
					textArea.value = "";
					textArea.style.left = "-1000px";
					textArea.style.top = "-1000px";
					textArea.style.width = "3000px";
					textArea.style.height = "3000px";
					setTimeout(function() {
						self._clickCount = 1;
						self._ignoreDOMSelection = false;
						self._setSelectionTo(pt.left, pt.top, false);
					}, 300);
				}
			}
			if (e.touches.length === 0) {
				setTimeout(function() {
					var selection = self._getSelection();
					var text = self._model.getText(selection.start, selection.end);
					var textArea = self._textArea;
					textArea.value = text;
					textArea.selectionStart = 0;
					textArea.selectionEnd = text.length;
					if (!selection.isEmpty()) {
						var touchRect = self._touchDiv.getBoundingClientRect();
						var bounds = self._getOffsetBounds(selection.start);
						textArea.style.left = (touchRect.width / 2) + "px";
						textArea.style.top = ((bounds.top > 40 ? bounds.top - 30 : bounds.top + 30)) + "px";
					}
				}, 0);
			}
//			e.preventDefault();
		},

		/************************************ Actions ******************************************/
		_doAction: function (e) {
			var keyBindings = this._keyBindings;
			for (var i = 0; i < keyBindings.length; i++) {
				var kb = keyBindings[i];
				if (kb.keyBinding.match(e)) {
					if (kb.name) {
						var actions = this._actions;
						for (var j = 0; j < actions.length; j++) {
							var a = actions[j];
							if (a.name === kb.name) {
								if (a.userHandler) {
									if (!a.userHandler()) {
										if (a.defaultHandler) {
											a.defaultHandler();
										} else {
											return false;
										}
									}
								} else if (a.defaultHandler) {
									a.defaultHandler();
								}
								break;
							}
						}
					}
					return true;
				}
			}
			return false;
		},
		_doBackspace: function (args) {
			var selection = this._getSelection();
			if (selection.isEmpty()) {
				var model = this._model;
				var caret = selection.getCaret();
				var lineIndex = model.getLineAtOffset(caret);
				if (caret === model.getLineStart(lineIndex)) {
					if (lineIndex > 0) {
						selection.extend(model.getLineEnd(lineIndex - 1));
					}
				} else {
					selection.extend(this._getOffset(caret, args.unit, -1));
				}
			}
			this._modifyContent({text: "", start: selection.start, end: selection.end}, true);
			return true;
		},
		_doContent: function (text) {
			var selection = this._getSelection();
			this._modifyContent({text: text, start: selection.start, end: selection.end, _ignoreDOMSelection: true}, true);
		},
		_doCopy: function (e) {
			var selection = this._getSelection();
			if (!selection.isEmpty()) {
				var text = this._getBaseText(selection.start, selection.end);
				return this._setClipboardText(text, e);
			}
			return true;
		},
		_doCursorNext: function (args) {
			if (!args.select) {
				if (this._clearSelection("next")) { return true; }
			}
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var lineIndex = model.getLineAtOffset(caret);
			if (caret === model.getLineEnd(lineIndex)) {
				if (lineIndex + 1 < model.getLineCount()) {
					selection.extend(model.getLineStart(lineIndex + 1));
				}
			} else {
				selection.extend(this._getOffset(caret, args.unit, 1));
			}
			if (!args.select) { selection.collapse(); }
			this._setSelection(selection, true);
			return true;
		},
		_doCursorPrevious: function (args) {
			if (!args.select) {
				if (this._clearSelection("previous")) { return true; }
			}
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var lineIndex = model.getLineAtOffset(caret);
			if (caret === model.getLineStart(lineIndex)) {
				if (lineIndex > 0) {
					selection.extend(model.getLineEnd(lineIndex - 1));
				}
			} else {
				selection.extend(this._getOffset(caret, args.unit, -1));
			}
			if (!args.select) { selection.collapse(); }
			this._setSelection(selection, true);
			return true;
		},
		_doCut: function (e) {
			var selection = this._getSelection();
			if (!selection.isEmpty()) {
				var text = this._getBaseText(selection.start, selection.end);
				this._doContent("");
				return this._setClipboardText(text, e);
			}
			return true;
		},
		_doDelete: function (args) {
			var selection = this._getSelection();
			if (selection.isEmpty()) {
				var model = this._model;
				var caret = selection.getCaret();
				var lineIndex = model.getLineAtOffset(caret);
				if (caret === model.getLineEnd (lineIndex)) {
					if (lineIndex + 1 < model.getLineCount()) {
						selection.extend(model.getLineStart(lineIndex + 1));
					}
				} else {
					selection.extend(this._getOffset(caret, args.unit, 1));
				}
			}
			this._modifyContent({text: "", start: selection.start, end: selection.end}, true);
			return true;
		},
		_doEnd: function (args) {
			var selection = this._getSelection();
			var model = this._model;
			if (args.ctrl) {
				selection.extend(model.getCharCount());
			} else {
				var lineIndex = model.getLineAtOffset(selection.getCaret());
				selection.extend(model.getLineEnd(lineIndex)); 
			}
			if (!args.select) { selection.collapse(); }
			this._setSelection(selection, true);
			return true;
		},
		_doEnter: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			this._doContent(model.getLineDelimiter()); 
			if (args && args.noCursor) {
				selection.end = selection.start;
				this._setSelection(selection);
			}
			return true;
		},
		_doHome: function (args) {
			var selection = this._getSelection();
			var model = this._model;
			if (args.ctrl) {
				selection.extend(0);
			} else {
				var lineIndex = model.getLineAtOffset(selection.getCaret());
				selection.extend(model.getLineStart(lineIndex)); 
			}
			if (!args.select) { selection.collapse(); }
			this._setSelection(selection, true);
			return true;
		},
		_doLineDown: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var lineIndex = model.getLineAtOffset(caret);
			if (lineIndex + 1 < model.getLineCount()) {
				var scrollX = this._getScroll().x;
				var x = this._columnX;
				if (x === -1 || args.select || args.wholeLine) {
					var offset = args.wholeLine ? model.getLineEnd(lineIndex + 1) : caret;
					x = this._getOffsetToX(offset) + scrollX;
				}
				selection.extend(this._getXToOffset(lineIndex + 1, x - scrollX));
				if (!args.select) { selection.collapse(); }
				this._setSelection(selection, true, true);
				this._columnX = x;
			}
			return true;
		},
		_doLineUp: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var lineIndex = model.getLineAtOffset(caret);
			if (lineIndex > 0) {
				var scrollX = this._getScroll().x;
				var x = this._columnX;
				if (x === -1 || args.select || args.wholeLine) {
					var offset = args.wholeLine ? model.getLineStart(lineIndex - 1) : caret;
					x = this._getOffsetToX(offset) + scrollX;
				}
				selection.extend(this._getXToOffset(lineIndex - 1, x - scrollX));
				if (!args.select) { selection.collapse(); }
				this._setSelection(selection, true, true);
				this._columnX = x;
			}
			return true;
		},
		_doPageDown: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var caretLine = model.getLineAtOffset(caret);
			var lineCount = model.getLineCount();
			if (caretLine < lineCount - 1) {
				var scroll = this._getScroll();
				var clientHeight = this._getClientHeight();
				var lineHeight = this._getLineHeight();
				var lines = Math.floor(clientHeight / lineHeight);
				var scrollLines = Math.min(lineCount - caretLine - 1, lines);
				scrollLines = Math.max(1, scrollLines);
				var x = this._columnX;
				if (x === -1 || args.select) {
					x = this._getOffsetToX(caret) + scroll.x;
				}
				selection.extend(this._getXToOffset(caretLine + scrollLines, x - scroll.x));
				if (!args.select) { selection.collapse(); }
				var verticalMaximum = lineCount * lineHeight;
				var scrollOffset = scroll.y + scrollLines * lineHeight;
				if (scrollOffset + clientHeight > verticalMaximum) {
					scrollOffset = verticalMaximum - clientHeight;
				}
				this._setSelection(selection, true, true, scrollOffset - scroll.y);
				this._columnX = x;
			}
			return true;
		},
		_doPageUp: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			var caret = selection.getCaret();
			var caretLine = model.getLineAtOffset(caret);
			if (caretLine > 0) {
				var scroll = this._getScroll();
				var clientHeight = this._getClientHeight();
				var lineHeight = this._getLineHeight();
				var lines = Math.floor(clientHeight / lineHeight);
				var scrollLines = Math.max(1, Math.min(caretLine, lines));
				var x = this._columnX;
				if (x === -1 || args.select) {
					x = this._getOffsetToX(caret) + scroll.x;
				}
				selection.extend(this._getXToOffset(caretLine - scrollLines, x - scroll.x));
				if (!args.select) { selection.collapse(); }
				var scrollOffset = Math.max(0, scroll.y - scrollLines * lineHeight);
				this._setSelection(selection, true, true, scrollOffset - scroll.y);
				this._columnX = x;
			}
			return true;
		},
		_doPaste: function(e) {
			var text = this._getClipboardText(e);
			if (text) {
				this._doContent(text);
			}
			return text !== null;
		},
		_doScroll: function (args) {
			var type = args.type;
			var model = this._model;
			var lineCount = model.getLineCount();
			var clientHeight = this._getClientHeight();
			var lineHeight = this._getLineHeight();
			var verticalMaximum = lineCount * lineHeight;
			var verticalScrollOffset = this._getScroll().y;
			var pixel;
			switch (type) {
				case "textStart": pixel = 0; break;
				case "textEnd": pixel = verticalMaximum - clientHeight; break;
				case "pageDown": pixel = verticalScrollOffset + clientHeight; break;
				case "pageUp": pixel = verticalScrollOffset - clientHeight; break;
				case "centerLine":
					var selection = this._getSelection();
					var lineStart = model.getLineAtOffset(selection.start);
					var lineEnd = model.getLineAtOffset(selection.end);
					var selectionHeight = (lineEnd - lineStart + 1) * lineHeight;
					pixel = (lineStart * lineHeight) - (clientHeight / 2) + (selectionHeight / 2);
					break;
			}
			if (pixel !== undefined) {
				pixel = Math.min(Math.max(0, pixel), verticalMaximum - clientHeight);
				this._scrollView(0, pixel - verticalScrollOffset);
			}
		},
		_doSelectAll: function (args) {
			var model = this._model;
			var selection = this._getSelection();
			selection.setCaret(0);
			selection.extend(model.getCharCount());
			this._setSelection(selection, false);
			return true;
		},
		_doTab: function (args) {
			this._doContent("\t"); 
			return true;
		},
		
		/************************************ Internals ******************************************/
		_applyStyle: function(style, node) {
			if (!style) {
				return;
			}
			if (style.styleClass) {
				node.className = style.styleClass;
			}
			var properties = style.style;
			if (properties) {
				for (var s in properties) {
					if (properties.hasOwnProperty(s)) {
						node.style[s] = properties[s];
					}
				}
			}
			var attributes = style.attributes;
			if (attributes) {
				for (var a in attributes) {
					if (attributes.hasOwnProperty(a)) {
						node.setAttribute(a, attributes[a]);
					}
				}
			}
		},
		_autoScroll: function () {
			var selection = this._getSelection();
			var line;
			var x = this._autoScrollX;
			if (this._autoScrollDir === "up" || this._autoScrollDir === "down") {
				var scroll = this._autoScrollY / this._getLineHeight();
				scroll = scroll < 0 ? Math.floor(scroll) : Math.ceil(scroll);
				line = this._model.getLineAtOffset(selection.getCaret());
				line = Math.max(0, Math.min(this._model.getLineCount() - 1, line + scroll));
			} else if (this._autoScrollDir === "left" || this._autoScrollDir === "right") {
				line = this._getYToLine(this._autoScrollY);
				x += this._getOffsetToX(selection.getCaret());
			}
			selection.extend(this._getXToOffset(line, x));
			this._setSelection(selection, true);
		},
		_autoScrollTimer: function () {
			this._autoScroll();
			var self = this;
			this._autoScrollTimerID = setTimeout(function () {self._autoScrollTimer();}, this._AUTO_SCROLL_RATE);
		},
		_calculateLineHeight: function() {
			var parent = this._clientDiv;
			var document = this._frameDocument;
			var c = " ";
			var line = document.createElement("DIV");
			line.style.position = "fixed";
			line.style.left = "-1000px";
			var span1 = document.createElement("SPAN");
			span1.appendChild(document.createTextNode(c));
			line.appendChild(span1);
			var span2 = document.createElement("SPAN");
			span2.style.fontStyle = "italic";
			span2.appendChild(document.createTextNode(c));
			line.appendChild(span2);
			var span3 = document.createElement("SPAN");
			span3.style.fontWeight = "bold";
			span3.appendChild(document.createTextNode(c));
			line.appendChild(span3);
			var span4 = document.createElement("SPAN");
			span4.style.fontWeight = "bold";
			span4.style.fontStyle = "italic";
			span4.appendChild(document.createTextNode(c));
			line.appendChild(span4);
			parent.appendChild(line);
			var lineRect = line.getBoundingClientRect();
			var spanRect1 = span1.getBoundingClientRect();
			var spanRect2 = span2.getBoundingClientRect();
			var spanRect3 = span3.getBoundingClientRect();
			var spanRect4 = span4.getBoundingClientRect();
			var h1 = spanRect1.bottom - spanRect1.top;
			var h2 = spanRect2.bottom - spanRect2.top;
			var h3 = spanRect3.bottom - spanRect3.top;
			var h4 = spanRect4.bottom - spanRect4.top;
			var fontStyle = 0;
			var lineHeight = lineRect.bottom - lineRect.top;
			if (h2 > h1) {
				fontStyle = 1;
			}
			if (h3 > h2) {
				fontStyle = 2;
			}
			if (h4 > h3) {
				fontStyle = 3;
			}
			this._largestFontStyle = fontStyle;
			parent.removeChild(line);
			return lineHeight;
		},
		_calculatePadding: function() {
			var document = this._frameDocument;
			var parent = this._clientDiv;
			var pad = this._getPadding(this._viewDiv);
			var div1 = document.createElement("DIV");
			div1.style.position = "fixed";
			div1.style.left = "-1000px";
			div1.style.paddingLeft = pad.left + "px";
			div1.style.paddingTop = pad.top + "px";
			div1.style.paddingRight = pad.right + "px";
			div1.style.paddingBottom = pad.bottom + "px";
			div1.style.width = "100px";
			div1.style.height = "100px";
			var div2 = document.createElement("DIV");
			div2.style.width = "100%";
			div2.style.height = "100%";
			div1.appendChild(div2);
			parent.appendChild(div1);
			var rect1 = div1.getBoundingClientRect();
			var rect2 = div2.getBoundingClientRect();
			parent.removeChild(div1);
			pad = {
				left: rect2.left - rect1.left,
				top: rect2.top - rect1.top,
				right: rect1.right - rect2.right,
				bottom: rect1.bottom - rect2.bottom
			};
			return pad;
		},
		_clearSelection: function (direction) {
			var selection = this._getSelection();
			if (selection.isEmpty()) { return false; }
			if (direction === "next") {
				selection.start = selection.end;
			} else {
				selection.end = selection.start;
			}
			this._setSelection(selection, true);
			return true;
		},
		_commitIME: function () {
			if (this._imeOffset === -1) { return; }
			// make the state of the IME match the state the view expects it be in
			// when the view commits the text and IME also need to be committed
			// this can be accomplished by changing the focus around
			this._scrollDiv.focus();
			this._clientDiv.focus();
			
			var model = this._model;
			var lineIndex = model.getLineAtOffset(this._imeOffset);
			var lineStart = model.getLineStart(lineIndex);
			var newText = this._getDOMText(lineIndex);
			var oldText = model.getLine(lineIndex);
			var start = this._imeOffset - lineStart;
			var end = start + newText.length - oldText.length;
			if (start !== end) {
				var insertText = newText.substring(start, end);
				this._doContent(insertText);
			}
			this._imeOffset = -1;
		},
		_convertDelimiter: function (text, addTextFunc, addDelimiterFunc) {
				var cr = 0, lf = 0, index = 0, length = text.length;
				while (index < length) {
					if (cr !== -1 && cr <= index) { cr = text.indexOf("\r", index); }
					if (lf !== -1 && lf <= index) { lf = text.indexOf("\n", index); }
					var start = index, end;
					if (lf === -1 && cr === -1) {
						addTextFunc(text.substring(index));
						break;
					}
					if (cr !== -1 && lf !== -1) {
						if (cr + 1 === lf) {
							end = cr;
							index = lf + 1;
						} else {
							end = cr < lf ? cr : lf;
							index = (cr < lf ? cr : lf) + 1;
						}
					} else if (cr !== -1) {
						end = cr;
						index = cr + 1;
					} else {
						end = lf;
						index = lf + 1;
					}
					addTextFunc(text.substring(start, end));
					addDelimiterFunc();
				}
		},
		_createActions: function () {
			var KeyBinding = orion.textview.KeyBinding;
			//no duplicate keybindings
			var bindings = this._keyBindings = [];

			// Cursor Navigation
			bindings.push({name: "lineUp",		keyBinding: new KeyBinding(38), predefined: true});
			bindings.push({name: "lineDown",	keyBinding: new KeyBinding(40), predefined: true});
			bindings.push({name: "charPrevious",	keyBinding: new KeyBinding(37), predefined: true});
			bindings.push({name: "charNext",	keyBinding: new KeyBinding(39), predefined: true});
			if (isMac) {
				bindings.push({name: "scrollPageUp",		keyBinding: new KeyBinding(33), predefined: true});
				bindings.push({name: "scrollPageDown",	keyBinding: new KeyBinding(34), predefined: true});
				bindings.push({name: "pageUp",		keyBinding: new KeyBinding(33, null, null, true), predefined: true});
				bindings.push({name: "pageDown",	keyBinding: new KeyBinding(34, null, null, true), predefined: true});
				bindings.push({name: "lineStart",	keyBinding: new KeyBinding(37, true), predefined: true});
				bindings.push({name: "lineEnd",		keyBinding: new KeyBinding(39, true), predefined: true});
				bindings.push({name: "wordPrevious",	keyBinding: new KeyBinding(37, null, null, true), predefined: true});
				bindings.push({name: "wordNext",	keyBinding: new KeyBinding(39, null, null, true), predefined: true});
				bindings.push({name: "scrollTextStart",	keyBinding: new KeyBinding(36), predefined: true});
				bindings.push({name: "scrollTextEnd",		keyBinding: new KeyBinding(35), predefined: true});
				bindings.push({name: "textStart",	keyBinding: new KeyBinding(38, true), predefined: true});
				bindings.push({name: "textEnd",		keyBinding: new KeyBinding(40, true), predefined: true});
			} else {
				bindings.push({name: "pageUp",		keyBinding: new KeyBinding(33), predefined: true});
				bindings.push({name: "pageDown",	keyBinding: new KeyBinding(34), predefined: true});
				bindings.push({name: "lineStart",	keyBinding: new KeyBinding(36), predefined: true});
				bindings.push({name: "lineEnd",		keyBinding: new KeyBinding(35), predefined: true});
				if (isLinux) {
						bindings.push({name: "lineStartUp",    keyBinding: new KeyBinding(38, true), predefined: true});
						bindings.push({name: "lineEndDown",    keyBinding: new KeyBinding(40, true), predefined: true});
				}
				bindings.push({name: "wordPrevious",	keyBinding: new KeyBinding(37, true), predefined: true});
				bindings.push({name: "wordNext",	keyBinding: new KeyBinding(39, true), predefined: true});
				bindings.push({name: "textStart",	keyBinding: new KeyBinding(36, true), predefined: true});
				bindings.push({name: "textEnd",		keyBinding: new KeyBinding(35, true), predefined: true});
			}

			// Select Cursor Navigation
			bindings.push({name: "selectLineUp",		keyBinding: new KeyBinding(38, null, true), predefined: true});
			bindings.push({name: "selectLineDown",		keyBinding: new KeyBinding(40, null, true), predefined: true});
			bindings.push({name: "selectCharPrevious",	keyBinding: new KeyBinding(37, null, true), predefined: true});
			bindings.push({name: "selectCharNext",		keyBinding: new KeyBinding(39, null, true), predefined: true});
			bindings.push({name: "selectPageUp",		keyBinding: new KeyBinding(33, null, true), predefined: true});
			bindings.push({name: "selectPageDown",		keyBinding: new KeyBinding(34, null, true), predefined: true});
			if (isMac) {
				bindings.push({name: "selectLineStart",	keyBinding: new KeyBinding(37, true, true), predefined: true});
				bindings.push({name: "selectLineEnd",		keyBinding: new KeyBinding(39, true, true), predefined: true});
				bindings.push({name: "selectWordPrevious",	keyBinding: new KeyBinding(37, null, true, true), predefined: true});
				bindings.push({name: "selectWordNext",	keyBinding: new KeyBinding(39, null, true, true), predefined: true});
				bindings.push({name: "selectTextStart",	keyBinding: new KeyBinding(36, null, true), predefined: true});
				bindings.push({name: "selectTextEnd",		keyBinding: new KeyBinding(35, null, true), predefined: true});
				bindings.push({name: "selectTextStart",	keyBinding: new KeyBinding(38, true, true), predefined: true});
				bindings.push({name: "selectTextEnd",		keyBinding: new KeyBinding(40, true, true), predefined: true});
			} else {
				if (isLinux) {
					bindings.push({name: "selectWholeLineUp",		keyBinding: new KeyBinding(38, true, true), predefined: true});
					bindings.push({name: "selectWholeLineDown",		keyBinding: new KeyBinding(40, true, true), predefined: true});
				}
				bindings.push({name: "selectLineStart",		keyBinding: new KeyBinding(36, null, true), predefined: true});
				bindings.push({name: "selectLineEnd",		keyBinding: new KeyBinding(35, null, true), predefined: true});
				bindings.push({name: "selectWordPrevious",	keyBinding: new KeyBinding(37, true, true), predefined: true});
				bindings.push({name: "selectWordNext",		keyBinding: new KeyBinding(39, true, true), predefined: true});
				bindings.push({name: "selectTextStart",		keyBinding: new KeyBinding(36, true, true), predefined: true});
				bindings.push({name: "selectTextEnd",		keyBinding: new KeyBinding(35, true, true), predefined: true});
			}

			//Misc
			bindings.push({name: "deletePrevious",		keyBinding: new KeyBinding(8), predefined: true});
			bindings.push({name: "deletePrevious",		keyBinding: new KeyBinding(8, null, true), predefined: true});
			bindings.push({name: "deleteNext",		keyBinding: new KeyBinding(46), predefined: true});
			bindings.push({name: "deleteWordPrevious",	keyBinding: new KeyBinding(8, true), predefined: true});
			bindings.push({name: "deleteWordPrevious",	keyBinding: new KeyBinding(8, true, true), predefined: true});
			bindings.push({name: "deleteWordNext",		keyBinding: new KeyBinding(46, true), predefined: true});
			bindings.push({name: "tab",			keyBinding: new KeyBinding(9), predefined: true});
			bindings.push({name: "enter",			keyBinding: new KeyBinding(13), predefined: true});
			bindings.push({name: "enter",			keyBinding: new KeyBinding(13, null, true), predefined: true});
			bindings.push({name: "selectAll",		keyBinding: new KeyBinding('a', true), predefined: true});
			if (isMac) {
				bindings.push({name: "deleteNext",		keyBinding: new KeyBinding(46, null, true), predefined: true});
				bindings.push({name: "deleteWordPrevious",	keyBinding: new KeyBinding(8, null, null, true), predefined: true});
				bindings.push({name: "deleteWordNext",		keyBinding: new KeyBinding(46, null, null, true), predefined: true});
			}
				
			/*
			* Feature in IE/Chrome: prevent ctrl+'u', ctrl+'i', and ctrl+'b' from applying styles to the text.
			*
			* Note that Chrome applies the styles on the Mac with Ctrl instead of Cmd.
			*/
			if (!isFirefox) {
				var isMacChrome = isMac && isChrome;
				bindings.push({name: null, keyBinding: new KeyBinding('u', !isMacChrome, false, false, isMacChrome), predefined: true});
				bindings.push({name: null, keyBinding: new KeyBinding('i', !isMacChrome, false, false, isMacChrome), predefined: true});
				bindings.push({name: null, keyBinding: new KeyBinding('b', !isMacChrome, false, false, isMacChrome), predefined: true});
			}

			if (isFirefox) {
				bindings.push({name: "copy", keyBinding: new KeyBinding(45, true), predefined: true});
				bindings.push({name: "paste", keyBinding: new KeyBinding(45, null, true), predefined: true});
				bindings.push({name: "cut", keyBinding: new KeyBinding(46, null, true), predefined: true});
			}

			// Add the emacs Control+ ... key bindings.
			if (isMac) {
				bindings.push({name: "lineStart", keyBinding: new KeyBinding("a", false, false, false, true), predefined: true});
				bindings.push({name: "lineEnd", keyBinding: new KeyBinding("e", false, false, false, true), predefined: true});
				bindings.push({name: "lineUp", keyBinding: new KeyBinding("p", false, false, false, true), predefined: true});
				bindings.push({name: "lineDown", keyBinding: new KeyBinding("n", false, false, false, true), predefined: true});
				bindings.push({name: "charPrevious", keyBinding: new KeyBinding("b", false, false, false, true), predefined: true});
				bindings.push({name: "charNext", keyBinding: new KeyBinding("f", false, false, false, true), predefined: true});
				bindings.push({name: "deletePrevious", keyBinding: new KeyBinding("h", false, false, false, true), predefined: true});
				bindings.push({name: "deleteNext", keyBinding: new KeyBinding("d", false, false, false, true), predefined: true});
				bindings.push({name: "deleteLineEnd", keyBinding: new KeyBinding("k", false, false, false, true), predefined: true});
				if (isFirefox) {
					bindings.push({name: "scrollPageDown", keyBinding: new KeyBinding("v", false, false, false, true), predefined: true});
					bindings.push({name: "deleteLineStart", keyBinding: new KeyBinding("u", false, false, false, true), predefined: true});
					bindings.push({name: "deleteWordPrevious", keyBinding: new KeyBinding("w", false, false, false, true), predefined: true});
				} else {
					bindings.push({name: "pageDown", keyBinding: new KeyBinding("v", false, false, false, true), predefined: true});
					bindings.push({name: "centerLine", keyBinding: new KeyBinding("l", false, false, false, true), predefined: true});
					bindings.push({name: "enterNoCursor", keyBinding: new KeyBinding("o", false, false, false, true), predefined: true});
					//TODO implement: y (yank), t (transpose)
				}
			}

			//1 to 1, no duplicates
			var self = this;
			this._actions = [
				{name: "lineUp",		defaultHandler: function() {return self._doLineUp({select: false});}},
				{name: "lineDown",		defaultHandler: function() {return self._doLineDown({select: false});}},
				{name: "lineStart",		defaultHandler: function() {return self._doHome({select: false, ctrl:false});}},
				{name: "lineEnd",		defaultHandler: function() {return self._doEnd({select: false, ctrl:false});}},
				{name: "lineStartUp",    defaultHandler: function() {return self._doLineUp({select: false, wholeLine:true});}},
				{name: "lineEndDown",    defaultHandler: function() {return self._doLineDown({select: false, wholeLine:true});}},
				{name: "charPrevious",		defaultHandler: function() {return self._doCursorPrevious({select: false, unit:"character"});}},
				{name: "charNext",		defaultHandler: function() {return self._doCursorNext({select: false, unit:"character"});}},
				{name: "pageUp",		defaultHandler: function() {return self._doPageUp({select: false});}},
				{name: "pageDown",		defaultHandler: function() {return self._doPageDown({select: false});}},
				{name: "scrollPageUp",		defaultHandler: function() {return self._doScroll({type: "pageUp"});}},
				{name: "scrollPageDown",		defaultHandler: function() {return self._doScroll({type: "pageDown"});}},
				{name: "wordPrevious",		defaultHandler: function() {return self._doCursorPrevious({select: false, unit:"word"});}},
				{name: "wordNext",		defaultHandler: function() {return self._doCursorNext({select: false, unit:"word"});}},
				{name: "textStart",		defaultHandler: function() {return self._doHome({select: false, ctrl:true});}},
				{name: "textEnd",		defaultHandler: function() {return self._doEnd({select: false, ctrl:true});}},
				{name: "scrollTextStart",	defaultHandler: function() {return self._doScroll({type: "textStart"});}},
				{name: "scrollTextEnd",		defaultHandler: function() {return self._doScroll({type: "textEnd"});}},
				{name: "centerLine",		defaultHandler: function() {return self._doScroll({type: "centerLine"});}},
				
				{name: "selectLineUp",		defaultHandler: function() {return self._doLineUp({select: true});}},
				{name: "selectLineDown",	defaultHandler: function() {return self._doLineDown({select: true});}},
				{name: "selectWholeLineUp",		defaultHandler: function() {return self._doLineUp({select: true, wholeLine: true});}},
				{name: "selectWholeLineDown",	defaultHandler: function() {return self._doLineDown({select: true, wholeLine: true});}},
				{name: "selectLineStart",	defaultHandler: function() {return self._doHome({select: true, ctrl:false});}},
				{name: "selectLineEnd",		defaultHandler: function() {return self._doEnd({select: true, ctrl:false});}},
				{name: "selectCharPrevious",	defaultHandler: function() {return self._doCursorPrevious({select: true, unit:"character"});}},
				{name: "selectCharNext",	defaultHandler: function() {return self._doCursorNext({select: true, unit:"character"});}},
				{name: "selectPageUp",		defaultHandler: function() {return self._doPageUp({select: true});}},
				{name: "selectPageDown",	defaultHandler: function() {return self._doPageDown({select: true});}},
				{name: "selectWordPrevious",	defaultHandler: function() {return self._doCursorPrevious({select: true, unit:"word"});}},
				{name: "selectWordNext",	defaultHandler: function() {return self._doCursorNext({select: true, unit:"word"});}},
				{name: "selectTextStart",	defaultHandler: function() {return self._doHome({select: true, ctrl:true});}},
				{name: "selectTextEnd",		defaultHandler: function() {return self._doEnd({select: true, ctrl:true});}},

				{name: "deletePrevious",	defaultHandler: function() {return self._doBackspace({unit:"character"});}},
				{name: "deleteNext",		defaultHandler: function() {return self._doDelete({unit:"character"});}},
				{name: "deleteWordPrevious",	defaultHandler: function() {return self._doBackspace({unit:"word"});}},
				{name: "deleteWordNext",	defaultHandler: function() {return self._doDelete({unit:"word"});}},
				{name: "deleteLineStart",	defaultHandler: function() {return self._doBackspace({unit: "line"});}},
				{name: "deleteLineEnd",	defaultHandler: function() {return self._doDelete({unit: "line"});}},
				{name: "tab",			defaultHandler: function() {return self._doTab();}},
				{name: "enter",			defaultHandler: function() {return self._doEnter();}},
				{name: "enterNoCursor",	defaultHandler: function() {return self._doEnter({noCursor:true});}},
				{name: "selectAll",		defaultHandler: function() {return self._doSelectAll();}},
				{name: "copy",			defaultHandler: function() {return self._doCopy();}},
				{name: "cut",			defaultHandler: function() {return self._doCut();}},
				{name: "paste",			defaultHandler: function() {return self._doPaste();}}
			];
		},
		_createLine: function(parent, sibling, document, lineIndex, model) {
			var lineText = model.getLine(lineIndex);
			var lineStart = model.getLineStart(lineIndex);
			var e = {textView: this, lineIndex: lineIndex, lineText: lineText, lineStart: lineStart};
			this.onLineStyle(e);
			var child = document.createElement("DIV");
			child.lineIndex = lineIndex;
			this._applyStyle(e.style, child);
			if (lineText.length !== 0) {
				var start = 0;
				var tabSize = this._customTabSize;
				if (tabSize && tabSize !== 8) {
					var tabIndex = lineText.indexOf("\t"), ignoreChars = 0;
					while (tabIndex !== -1) {
						this._createRanges(child, document, e.ranges, start, tabIndex, lineText, lineStart);
						var spacesCount = tabSize - ((tabIndex + ignoreChars) % tabSize);
						var spaces = "\u00A0";
						for (var i = 1; i < spacesCount; i++) {
							spaces += " ";
						}
						var tabSpan = document.createElement("SPAN");
						tabSpan.appendChild(document.createTextNode(spaces));
						tabSpan.ignoreChars = spacesCount - 1;
						ignoreChars += tabSpan.ignoreChars;
						if (e.ranges) {
							for (var j = 0; j < e.ranges.length; j++) {
								var range = e.ranges[j];
								var styleStart = range.start - lineStart;
								var styleEnd = range.end - lineStart;
								if (styleStart > tabIndex) { break; } 
								if (styleStart <= tabIndex && tabIndex < styleEnd) {
									this._applyStyle(range.style, tabSpan);
									break;
								}
							}
						} 
						child.appendChild(tabSpan);
						start = tabIndex + 1;
						tabIndex = lineText.indexOf("\t", start);
					}
				}
				this._createRanges(child, document, e.ranges, start, lineText.length, lineText, lineStart);
			}
			
			/*
			* A trailing span with a whitespace is added for three different reasons:
			* 1. Make sure the height of each line is the largest of the default font
			* in normal, italic, bold, and italic-bold.
			* 2. When full selection is off, Firefox, Opera and IE9 do not extend the 
			* selection at the end of the line when the line is fully selected. 
			* 3. The height of a div with only an empty span is zero.
			*/
			var span = document.createElement("SPAN");
			span.ignoreChars = 1;
			if ((this._largestFontStyle & 1) !== 0) {
				span.style.fontStyle = "italic";
			}
			if ((this._largestFontStyle & 2) !== 0) {
				span.style.fontWeight = "bold";
			}
			var c = " ";
			if (!this._fullSelection && isIE < 9) {
				/* 
				* IE8 already selects extra space at end of a line fully selected,
				* adding another space at the end of the line causes the selection 
				* to look too big. The fix is to use a zero-width space (\uFEFF) instead. 
				*/
				c = "\uFEFF";
			}
			if (isWebkit) {
				/*
				* Feature in WekKit. Adding a regular white space to the line will
				* cause the longest line in the view to wrap even though "pre" is set.
				* The fix is to use the zero-width non-joiner character (\u200C) instead.
				* Note: To not use \uFEFF because in old version of Chrome this character 
				* shows a glyph;
				*/
				c = "\u200C";
			}
			span.appendChild(document.createTextNode(c));
			child.appendChild(span);
			
			parent.insertBefore(child, sibling);
			return child;
		},
		_createRanges: function(parent, document, ranges, start, end, text, lineStart) {
			if (start >= end) { return; }
			if (ranges) {
				for (var i = 0; i < ranges.length; i++) {
					var range = ranges[i];
					if (range.end <= lineStart + start) { continue; }
					var styleStart = Math.max(lineStart + start, range.start) - lineStart;
					if (styleStart >= end) { break; }
					var styleEnd = Math.min(lineStart + end, range.end) - lineStart;
					if (styleStart < styleEnd) {
						styleStart = Math.max(start, styleStart);
						styleEnd = Math.min(end, styleEnd);
						if (start < styleStart) {
							parent.appendChild(this._createRange(parent, document, text.substring(start, styleStart), null));
						}
						parent.appendChild(this._createRange(parent, document, text.substring(styleStart, styleEnd), range.style));
						start = styleEnd;
					}
				}
			}
			if (start < end) {
				parent.appendChild(this._createRange(parent, document, text.substring(start, end), null));
			}
		},
		_createRange: function(parent, document, text, style) {
			var isLink = style && style.tagName === "A";
			if (isLink) { parent.hasLink = true; }
			var tagName = isLink && this._linksVisible ? "A" : "SPAN";
			var child = document.createElement(tagName);
			child.appendChild(document.createTextNode(text));
			this._applyStyle(style, child);
			if (tagName === "A") {
				var self = this;
				addHandler(child, "click", function(e) { return self._handleLinkClick(e); }, false);
			}
			child.viewStyle = style;
			return child;
		},
		_createRuler: function(ruler) {
			if (!this._clientDiv) { return; }
			var document = this._frameDocument;
			var body = document.body;
			var side = ruler.getLocation();
			var rulerParent = side === "left" ? this._leftDiv : this._rightDiv;
			if (!rulerParent) {
				rulerParent = document.createElement("DIV");
				rulerParent.style.overflow = "hidden";
				rulerParent.style.MozUserSelect = "none";
				rulerParent.style.WebkitUserSelect = "none";
				if (isIE) {
					rulerParent.attachEvent("onselectstart", function() {return false;});
				}
				rulerParent.style.position = "absolute";
				rulerParent.style.top = "0px";
				rulerParent.style.cursor = "default";
				body.appendChild(rulerParent);
				if (side === "left") {
					this._leftDiv = rulerParent;
					rulerParent.className = "viewLeftRuler";
				} else {
					this._rightDiv = rulerParent;
					rulerParent.className = "viewRightRuler";
				}
				var table = document.createElement("TABLE");
				rulerParent.appendChild(table);
				table.cellPadding = "0px";
				table.cellSpacing = "0px";
				table.border = "0px";
				table.insertRow(0);
				var self = this;
				addHandler(rulerParent, "click", function(e) { self._handleRulerEvent(e); });
				addHandler(rulerParent, "dblclick", function(e) { self._handleRulerEvent(e); });
				addHandler(rulerParent, "mousemove", function(e) { self._handleRulerEvent(e); });
				addHandler(rulerParent, "mouseover", function(e) { self._handleRulerEvent(e); });
				addHandler(rulerParent, "mouseout", function(e) { self._handleRulerEvent(e); });
			}
			var div = document.createElement("DIV");
			div._ruler = ruler;
			div.rulerChanged = true;
			div.style.position = "relative";
			var row = rulerParent.firstChild.rows[0];
			var index = row.cells.length;
			var cell = row.insertCell(index);
			cell.vAlign = "top";
			cell.appendChild(div);
		},
		_createFrame: function() {
			if (this.frame) { return; }
			var parent = this._parent;
			while (parent.hasChildNodes()) { parent.removeChild(parent.lastChild); }
			var parentDocument = parent.ownerDocument;
			this._parentDocument = parentDocument;
			var frame = parentDocument.createElement("IFRAME");
			this._frame = frame;
			frame.frameBorder = "0px";//for IE, needs to be set before the frame is added to the parent
			frame.style.border = "0px";
			frame.style.width = "100%";
			frame.style.height = "100%";
			frame.scrolling = "no";
			var self = this;
			/*
			* Note that it is not possible to create the contents of the frame if the
			* parent is not connected to the document.  Only create it when the load
			* event is trigged.
			*/
			addHandler(frame, "load", this._loadHandler = function(e) { self._handleLoad(e); });
			if (!isWebkit) {
				/*
				* Feature in IE and Firefox.  It is not possible to get the style of an
				* element if it is not layed out because one of the ancestor has
				* style.display = none.  This means that the view cannot be created in this
				* situations, since no measuring can be performed.  The fix is to listen
				* for DOMAttrModified and create or destroy the view when the style.display
				* attribute changes.
				*/
				addHandler(parentDocument, "DOMAttrModified", this._attrModifiedHandler = function(e) {
					self._handleDOMAttrModified(e);
				});
			}
			parent.appendChild(frame);
			/* create synchronously if possible */
			this._handleLoad();
		},
		_getFrameHTML: function(sync) {
			var html = [];
			html.push("<!DOCTYPE html>");
			html.push("<html>");
			html.push("<head>");
			if (isIE < 9) {
				html.push("<meta http-equiv='X-UA-Compatible' content='IE=EmulateIE7'/>");
			}
			html.push("<style>");
			html.push(".viewContainer {font-family: monospace; font-size: 10pt;}");
			html.push(".view {padding: 1px 2px;}");
			html.push(".viewContent {}");
			html.push("</style>");
			if (this._stylesheet) {
				var stylesheet = typeof(this._stylesheet) === "string" ? [this._stylesheet] : this._stylesheet;
				for (var i = 0; i < stylesheet.length; i++) {
					if (sync) {
						try {
							//Force CSS to be loaded synchronously so lineHeight can be calculated
							var objXml = new XMLHttpRequest();
							if (objXml.overrideMimeType) {
								objXml.overrideMimeType("text/css");
							}
							objXml.open("GET", stylesheet[i], false);
							objXml.send(null);
							html.push("<style>");
							html.push(objXml.responseText);
							html.push("</style>");
							continue;
						} catch (e) {}
					}
					html.push("<link rel='stylesheet' type='text/css' href='");
					html.push(stylesheet[i]);
					html.push("'></link>");
				}
			}
			html.push("</head>");
			html.push("<body spellcheck='false'></body>");
			html.push("</html>");
			return html.join("");
		},
		_createView: function(sync) {
			if (this._clientDiv) { return; }
			if (this._ignoreCreate) { return; }
			this._ignoreCreate = true;

			var frame = this._frame;
			var parent = this._parent;
			var parentDocument = this._parentDocument;
			var frameWindow = frame.contentWindow;
			this._frameWindow = frameWindow;
			var frameDocument = frameWindow.document;
			this._frameDocument = frameDocument;
			frameDocument.open();
			frameDocument.write(this._getFrameHTML(true));
			frameDocument.close();
			
			var body = frameDocument.body;
			body.className = "viewContainer";
			body.style.margin = "0px";
			body.style.borderWidth = "0px";
			body.style.padding = "0px";
			
			var textArea;
			if (isPad) {
				var touchDiv = parentDocument.createElement("DIV");
				this._touchDiv = touchDiv;
				touchDiv.style.position = "absolute";
				touchDiv.style.border = "0px";
				touchDiv.style.padding = "0px";
				touchDiv.style.margin = "0px";
				touchDiv.style.zIndex = "2";
				touchDiv.style.overflow = "hidden";
				touchDiv.style.background="transparent";
				touchDiv.style.WebkitUserSelect = "none";
				parent.appendChild(touchDiv);

				textArea = parentDocument.createElement("TEXTAREA");
				this._textArea = textArea;
				textArea.style.position = "absolute";
				textArea.style.whiteSpace = "pre";
				textArea.style.left = "-1000px";
				textArea.tabIndex = 1;
				textArea.autocapitalize = false;
				textArea.autocorrect = false;
				textArea.className = "viewContainer";
				textArea.style.background = "transparent";
				textArea.style.color = "transparent";
				textArea.style.border = "0px";
				textArea.style.padding = "0px";
				textArea.style.margin = "0px";
				textArea.style.borderRadius = "0px";
				textArea.style.WebkitAppearance = "none";
				textArea.style.WebkitTapHighlightColor = "transparent";
				touchDiv.appendChild(textArea);
			}
			if (isFirefox) {
				var clipboardDiv = frameDocument.createElement("DIV");
				this._clipboardDiv = clipboardDiv;
				clipboardDiv.style.position = "fixed";
				clipboardDiv.style.whiteSpace = "pre";
				clipboardDiv.style.left = "-1000px";
				body.appendChild(clipboardDiv);
			}

			var viewDiv = frameDocument.createElement("DIV");
			viewDiv.className = "view";
			this._viewDiv = viewDiv;
			viewDiv.id = "viewDiv";
			viewDiv.tabIndex = -1;
			viewDiv.style.overflow = "auto";
			viewDiv.style.position = "absolute";
			viewDiv.style.top = "0px";
			viewDiv.style.borderWidth = "0px";
			viewDiv.style.margin = "0px";
			viewDiv.style.MozOutline = "none";
			viewDiv.style.outline = "none";
			body.appendChild(viewDiv);
				
			var scrollDiv = frameDocument.createElement("DIV");
			this._scrollDiv = scrollDiv;
			scrollDiv.id = "scrollDiv";
			scrollDiv.style.margin = "0px";
			scrollDiv.style.borderWidth = "0px";
			scrollDiv.style.padding = "0px";
			viewDiv.appendChild(scrollDiv);

			if (isPad || (this._fullSelection && !isWebkit)) {
				this._hightlightRGB = "Highlight";
				var selDiv1 = frameDocument.createElement("DIV");
				this._selDiv1 = selDiv1;
				selDiv1.id = "selDiv1";
				selDiv1.style.position = "fixed";
				selDiv1.style.borderWidth = "0px";
				selDiv1.style.margin = "0px";
				selDiv1.style.padding = "0px";
				selDiv1.style.MozOutline = "none";
				selDiv1.style.outline = "none";
				selDiv1.style.background = this._hightlightRGB;
				selDiv1.style.width="0px";
				selDiv1.style.height="0px";
				scrollDiv.appendChild(selDiv1);
				var selDiv2 = frameDocument.createElement("DIV");
				this._selDiv2 = selDiv2;
				selDiv2.id = "selDiv2";
				selDiv2.style.position = "fixed";
				selDiv2.style.borderWidth = "0px";
				selDiv2.style.margin = "0px";
				selDiv2.style.padding = "0px";
				selDiv2.style.MozOutline = "none";
				selDiv2.style.outline = "none";
				selDiv2.style.background = this._hightlightRGB;
				selDiv2.style.width="0px";
				selDiv2.style.height="0px";
				scrollDiv.appendChild(selDiv2);
				var selDiv3 = frameDocument.createElement("DIV");
				this._selDiv3 = selDiv3;
				selDiv3.id = "selDiv3";
				selDiv3.style.position = "fixed";
				selDiv3.style.borderWidth = "0px";
				selDiv3.style.margin = "0px";
				selDiv3.style.padding = "0px";
				selDiv3.style.MozOutline = "none";
				selDiv3.style.outline = "none";
				selDiv3.style.background = this._hightlightRGB;
				selDiv3.style.width="0px";
				selDiv3.style.height="0px";
				scrollDiv.appendChild(selDiv3);
				
				/*
				* Bug in Firefox. The Highlight color is mapped to list selection
				* background instead of the text selection background.  The fix
				* is to map known colors using a table or fallback to light blue.
				*/
				if (isFirefox && isMac) {
					var style = this._frameWindow.getComputedStyle(selDiv3, null);
					var rgb = style.getPropertyValue("background-color");
					switch (rgb) {
						case "rgb(119, 141, 168)": rgb = "rgb(199, 208, 218)"; break;
						case "rgb(127, 127, 127)": rgb = "rgb(198, 198, 198)"; break;
						case "rgb(255, 193, 31)": rgb = "rgb(250, 236, 115)"; break;
						case "rgb(243, 70, 72)": rgb = "rgb(255, 176, 139)"; break;
						case "rgb(255, 138, 34)": rgb = "rgb(255, 209, 129)"; break;
						case "rgb(102, 197, 71)": rgb = "rgb(194, 249, 144)"; break;
						case "rgb(140, 78, 184)": rgb = "rgb(232, 184, 255)"; break;
						default: rgb = "rgb(180, 213, 255)"; break;
					}
					this._hightlightRGB = rgb;
					selDiv1.style.background = rgb;
					selDiv2.style.background = rgb;
					selDiv3.style.background = rgb;
					var styleSheet = frameDocument.styleSheets[0];
					styleSheet.insertRule("::-moz-selection {background: " + rgb + "; }", 0);
				}
			}

			var clientDiv = frameDocument.createElement("DIV");
			clientDiv.className = "viewContent";
			this._clientDiv = clientDiv;
			clientDiv.id = "clientDiv";
			clientDiv.style.whiteSpace = "pre";
			clientDiv.style.position = "fixed";
			clientDiv.style.borderWidth = "0px";
			clientDiv.style.margin = "0px";
			clientDiv.style.padding = "0px";
			clientDiv.style.MozOutline = "none";
			clientDiv.style.outline = "none";
			if (isPad) {
				clientDiv.style.WebkitTapHighlightColor = "transparent";
			}
			scrollDiv.appendChild(clientDiv);

			if (isFirefox) {
				var overlayDiv = frameDocument.createElement("DIV");
				this._overlayDiv = overlayDiv;
				overlayDiv.id = "overlayDiv";
				overlayDiv.style.position = clientDiv.style.position;
				overlayDiv.style.borderWidth = clientDiv.style.borderWidth;
				overlayDiv.style.margin = clientDiv.style.margin;
				overlayDiv.style.padding = clientDiv.style.padding;
				overlayDiv.style.cursor = "text";
				overlayDiv.style.zIndex = "1";
				scrollDiv.appendChild(overlayDiv);
			}
			if (!isPad) {
				clientDiv.contentEditable = "true";
			}
			if (isDnD) {
				this._dragNode = this._overlayDiv || this._clientDiv;
			}
			this._lineHeight = this._calculateLineHeight();
			this._viewPadding = this._calculatePadding();
			if (isIE) {
				body.style.lineHeight = this._lineHeight + "px";
			}
			if (this._tabSize) {
				if (isOpera) {
					clientDiv.style.OTabSize = this._tabSize+"";
				} else if (isFirefox >= 4) {
					clientDiv.style.MozTabSize = this._tabSize+"";
				} else if (this._tabSize !== 8) {
					this._customTabSize = this._tabSize;
				}
			}
			this._hookEvents();
			var rulers = this._rulers;
			for (var i=0; i<rulers.length; i++) {
				this._createRuler(rulers[i]);
			}
			if (sync) {
				this._updatePage();
			} else {
				this._queueUpdatePage();
				var h = this._hScroll, v = this._vScroll;
				this._vScroll = this._hScroll = 0;
				if (h > 0 || v > 0) {
					var self = this;
					setTimeout(function() {
						self._scrollView(h, v);
					}, 0);
				}
			}
			this._ignoreCreate = false;
		},
		_destroyFrame: function() {
			var frame = this._frame;
			if (!frame) { return; }
			if (this._loadHandler) {
				removeHandler(frame, "load", this._loadHandler);
				this._loadHandler = null;
			}
			if (this._attrModifiedHandler) {
				removeHandler(this._parentDocument, "DOMAttrModified", this._attrModifiedHandler);
				this._attrModifiedHandler = null;
			}
			frame.parentNode.removeChild(frame);
			this._frame = null;
			this._frameDocument = null;
			this._frameWindow = null;
		},
		_destroyRuler: function(ruler) {
			var side = ruler.getLocation();
			var rulerParent = side === "left" ? this._leftDiv : this._rightDiv;
			if (rulerParent) {
				var row = rulerParent.firstChild.rows[0];
				var cells = row.cells;
				for (var index = 0; index < cells.length; index++) {
					var cell = cells[index];
					if (cell.firstChild._ruler === ruler) { break; }
				}
				if (index === cells.length) { return; }
				row.cells[index]._ruler = undefined;
				row.deleteCell(index);
			}
		},
		_destroyView: function() {
			var clientDiv = this._clientDiv;
			if (!clientDiv) { return; }
			this._setGrab(null);
			this._unhookEvents();

			/* Destroy timers */
			if (this._autoScrollTimerID) {
				clearTimeout(this._autoScrollTimerID);
				this._autoScrollTimerID = null;
			}
			if (this._updateTimer) {
				clearTimeout(this._updateTimer);
				this._updateTimer = null;
			}

			/* Destroy DOM */
			var parent = this._frameDocument.body;
			while (parent.hasChildNodes()) { parent.removeChild(parent.lastChild); }
			if (this._touchDiv) {
				this._parent.removeChild(this._touchDiv);
				this._touchDiv = null;
			}
			this._selDiv1 = null;
			this._selDiv2 = null;
			this._selDiv3 = null;
			this._textArea = null;
			this._clipboardDiv = null;
			this._scrollDiv = null;
			this._viewDiv = null;
			this._clientDiv = null;
			this._overlayDiv = null;
			this._dragNode = null;
			this._leftDiv = null;
			this._rightDiv = null;
		},
		_doAutoScroll: function (direction, x, y) {
			this._autoScrollDir = direction;
			this._autoScrollX = x;
			this._autoScrollY = y;
			if (!this._autoScrollTimerID) {
				this._autoScrollTimer();
			}
		},
		_endAutoScroll: function () {
			if (this._autoScrollTimerID) { clearTimeout(this._autoScrollTimerID); }
			this._autoScrollDir = undefined;
			this._autoScrollTimerID = undefined;
		},
		_getBaseText: function(start, end) {
			var model = this._model;
			/* This is the only case the view access the base model, alternatively the view could use a event to application to customize the text */
			if (model.getBaseModel) {
				start = model.mapOffset(start);
				end = model.mapOffset(end);
				model = model.getBaseModel();
			}
			return model.getText(start, end);
		},
		_getBoundsAtOffset: function (offset) {
			var model = this._model;
			var document = this._frameDocument;
			var clientDiv = this._clientDiv;
			var lineIndex = model.getLineAtOffset(offset);
			var dummy;
			var child = this._getLineNode(lineIndex);
			if (!child) {
				child = dummy = this._createLine(clientDiv, null, document, lineIndex, model);
			}
			var result = null;
			if (offset < model.getLineEnd(lineIndex)) {
				var lineOffset = model.getLineStart(lineIndex);
				var lineChild = child.firstChild;
				while (lineChild) {
					var textNode = lineChild.firstChild;
					var nodeLength = textNode.length; 
					if (lineChild.ignoreChars) {
						nodeLength -= lineChild.ignoreChars;
					}
					if (lineOffset + nodeLength > offset) {
						var index = offset - lineOffset;
						var range;
						if (isRangeRects) {
							range = document.createRange();
							range.setStart(textNode, index);
							range.setEnd(textNode, index + 1);
							result = range.getBoundingClientRect();
						} else if (isIE) {
							range = document.body.createTextRange();
							range.moveToElementText(lineChild);
							range.collapse();
							range.moveEnd("character", index + 1);
							range.moveStart("character", index);
							result = range.getBoundingClientRect();
						} else {
							var text = textNode.data;
							lineChild.removeChild(textNode);
							lineChild.appendChild(document.createTextNode(text.substring(0, index)));
							var span = document.createElement("SPAN");
							span.appendChild(document.createTextNode(text.substring(index, index + 1)));
							lineChild.appendChild(span);
							lineChild.appendChild(document.createTextNode(text.substring(index + 1)));
							result = span.getBoundingClientRect();
							lineChild.innerHTML = "";
							lineChild.appendChild(textNode);
							if (!dummy) {
								/*
								 * Removing the element node that holds the selection start or end
								 * causes the selection to be lost. The fix is to detect this case
								 * and restore the selection. 
								 */
								var s = this._getSelection();
								if ((lineOffset <= s.start && s.start < lineOffset + nodeLength) ||  (lineOffset <= s.end && s.end < lineOffset + nodeLength)) {
									this._updateDOMSelection();
								}
							}
						}
						if (isIE) {
							var logicalXDPI = window.screen.logicalXDPI;
							var deviceXDPI = window.screen.deviceXDPI;
							result.left = result.left * logicalXDPI / deviceXDPI;
							result.right = result.right * logicalXDPI / deviceXDPI;
						}
						break;
					}
					lineOffset += nodeLength;
					lineChild = lineChild.nextSibling;
				}
			}
			if (!result) {
				var rect = this._getLineBoundingClientRect(child);
				result = {left: rect.right, right: rect.right};
			}
			if (dummy) { clientDiv.removeChild(dummy); }
			return result;
		},
		_getBottomIndex: function (fullyVisible) {
			var child = this._bottomChild;
			if (fullyVisible && this._getClientHeight() > this._getLineHeight()) {
				var rect = child.getBoundingClientRect();
				var clientRect = this._clientDiv.getBoundingClientRect();
				if (rect.bottom > clientRect.bottom) {
					child = this._getLinePrevious(child) || child;
				}
			}
			return child.lineIndex;
		},
		_getFrameHeight: function() {
			return this._frameDocument.documentElement.clientHeight;
		},
		_getFrameWidth: function() {
			return this._frameDocument.documentElement.clientWidth;
		},
		_getClientHeight: function() {
			var viewPad = this._getViewPadding();
			return Math.max(0, this._viewDiv.clientHeight - viewPad.top - viewPad.bottom);
		},
		_getClientWidth: function() {
			var viewPad = this._getViewPadding();
			return Math.max(0, this._viewDiv.clientWidth - viewPad.left - viewPad.right);
		},
		_getClipboardText: function (event) {
			var delimiter = this._model.getLineDelimiter();
			var clipboadText, text;
			if (this._frameWindow.clipboardData) {
				//IE
				clipboadText = [];
				text = this._frameWindow.clipboardData.getData("Text");
				this._convertDelimiter(text, function(t) {clipboadText.push(t);}, function() {clipboadText.push(delimiter);});
				return clipboadText.join("");
			}
			if (isFirefox) {
				var document = this._frameDocument;
				var clipboardDiv = this._clipboardDiv;
				clipboardDiv.innerHTML = "<pre contenteditable=''></pre>";
				clipboardDiv.firstChild.focus();
				var self = this;
				var _getText = function() {
					var text = self._getTextFromElement(clipboardDiv);
					clipboardDiv.innerHTML = "";
					clipboadText = [];
					self._convertDelimiter(text, function(t) {clipboadText.push(t);}, function() {clipboadText.push(delimiter);});
					return clipboadText.join("");
				};
				
				/* Try execCommand first. Works on firefox with clipboard permission. */
				var result = false;
				this._ignorePaste = true;
				try {
					result = document.execCommand("paste", false, null);
				} catch (ex) {
					// Firefox can throw even when execCommand() works, see bug 362835
					result = clipboardDiv.childNodes.length > 1 || clipboardDiv.firstChild && clipboardDiv.firstChild.childNodes.length > 0;
				}
				this._ignorePaste = false;
				if (!result) {
					/*
					* Try native paste in DOM, works for firefox during the paste event.
					*/
					if (event) {
						setTimeout(function() {
							self.focus();
							var text = _getText();
							if (text) { self._doContent(text); }
						}, 0);
						return null;
					} else {
						/* no event and no clipboard permission, paste can't be performed */
						this.focus();
						return "";
					}
				}
				this.focus();
				return _getText();
			}
			//webkit
			if (event && event.clipboardData) {
				/*
				* Webkit (Chrome/Safari) allows getData during the paste event
				* Note: setData is not allowed, not even during copy/cut event
				*/
				clipboadText = [];
				text = event.clipboardData.getData("text/plain");
				this._convertDelimiter(text, function(t) {clipboadText.push(t);}, function() {clipboadText.push(delimiter);});
				return clipboadText.join("");
			} else {
				//TODO try paste using extension (Chrome only)
			}
			return "";
		},
		_getDOMText: function(lineIndex) {
			var child = this._getLineNode(lineIndex);
			var lineChild = child.firstChild;
			var text = "";
			while (lineChild) {
				var textNode = lineChild.firstChild;
				while (textNode) {
					if (lineChild.ignoreChars) {
						for (var i = 0; i < textNode.length; i++) {
							var ch = textNode.data.substring(i, i + 1);
							if (ch !== " ") {
								text += ch;
							}
						}
					} else {
						text += textNode.data;
					}
					textNode = textNode.nextSibling;
				}
				lineChild = lineChild.nextSibling;
			}
			return text;
		},
		_getTextFromElement: function(element) {
			var document = element.ownerDocument;
			var window = document.defaultView;
			if (!window.getSelection) {
				return element.innerText || element.textContent;
			}

			var newRange = document.createRange();
			newRange.selectNode(element);

			var selection = window.getSelection();
			var oldRanges = [];
			for (var i = 0; i < selection.rangeCount; i++) {
				oldRanges.push(selection.getRangeAt(i));
			}

			this._ignoreSelect = true;
			selection.removeAllRanges();
			selection.addRange(newRange);

			var text = selection.toString();

			selection.removeAllRanges();
			for (var i = 0; i < oldRanges.length; i++) {
				selection.addRange(oldRanges[i]);
			}

			this._ignoreSelect = false;
			return text;
		},
		_getViewPadding: function() {
			return this._viewPadding;
		},
		_getLineBoundingClientRect: function (child) {
			var rect = child.getBoundingClientRect();
			var lastChild = child.lastChild;
			//Remove any artificial trailing whitespace in the line
			while (lastChild && lastChild.ignoreChars === lastChild.firstChild.length) {
				lastChild = lastChild.previousSibling;
			}
			if (!lastChild) {
				return {left: rect.left, top: rect.top, right: rect.left, bottom: rect.bottom};
			}
			var lastRect = lastChild.getBoundingClientRect();
			return {left: rect.left, top: rect.top, right: lastRect.right, bottom: rect.bottom};
		},
		_getLineHeight: function() {
			return this._lineHeight;
		},
		_getLineNode: function (lineIndex) {
			var clientDiv = this._clientDiv;
			var child = clientDiv.firstChild;
			while (child) {
				if (lineIndex === child.lineIndex) {
					return child;
				}
				child = child.nextSibling;
			}
			return undefined;
		},
		_getLineNext: function (lineNode) {
			var node = lineNode ? lineNode.nextSibling : this._clientDiv.firstChild;
			while (node && node.lineIndex === -1) {
				node = node.nextSibling;
			}
			return node;
		},
		_getLinePrevious: function (lineNode) {
			var node = lineNode ? lineNode.previousSibling : this._clientDiv.lastChild;
			while (node && node.lineIndex === -1) {
				node = node.previousSibling;
			}
			return node;
		},
		_getOffset: function (offset, unit, direction) {
			if (unit === "line") {
				var model = this._model;
				var lineIndex = model.getLineAtOffset(offset);
				if (direction > 0) {
					return model.getLineEnd(lineIndex);
				}
				return model.getLineStart(lineIndex);
			}
			if (unit === "wordend") {
				return this._getOffset_W3C(offset, unit, direction);
			}
			return isIE ? this._getOffset_IE(offset, unit, direction) : this._getOffset_W3C(offset, unit, direction);
		},
		_getOffset_W3C: function (offset, unit, direction) {
			function _isPunctuation(c) {
				return (33 <= c && c <= 47) || (58 <= c && c <= 64) || (91 <= c && c <= 94) || c === 96 || (123 <= c && c <= 126);
			}
			function _isWhitespace(c) {
				return c === 32 || c === 9;
			}
			if (unit === "word" || unit === "wordend") {
				var model = this._model;
				var lineIndex = model.getLineAtOffset(offset);
				var lineText = model.getLine(lineIndex);
				var lineStart = model.getLineStart(lineIndex);
				var lineEnd = model.getLineEnd(lineIndex);
				var lineLength = lineText.length;
				var offsetInLine = offset - lineStart;
				
				
				var c, previousPunctuation, previousLetterOrDigit, punctuation, letterOrDigit;
				if (direction > 0) {
					if (offsetInLine === lineLength) { return lineEnd; }
					c = lineText.charCodeAt(offsetInLine);
					previousPunctuation = _isPunctuation(c); 
					previousLetterOrDigit = !previousPunctuation && !_isWhitespace(c);
					offsetInLine++;
					while (offsetInLine < lineLength) {
						c = lineText.charCodeAt(offsetInLine);
						punctuation = _isPunctuation(c);
						if (unit === "wordend") {
							if (!punctuation && previousPunctuation) { break; }
						} else {
							if (punctuation && !previousPunctuation) { break; }
						}
						letterOrDigit  = !punctuation && !_isWhitespace(c);
						if (unit === "wordend") {
							if (!letterOrDigit && previousLetterOrDigit) { break; }
						} else {
							if (letterOrDigit && !previousLetterOrDigit) { break; }
						}
						previousLetterOrDigit = letterOrDigit;
						previousPunctuation = punctuation;
						offsetInLine++;
					}
				} else {
					if (offsetInLine === 0) { return lineStart; }
					offsetInLine--;
					c = lineText.charCodeAt(offsetInLine);
					previousPunctuation = _isPunctuation(c); 
					previousLetterOrDigit = !previousPunctuation && !_isWhitespace(c);
					while (0 < offsetInLine) {
						c = lineText.charCodeAt(offsetInLine - 1);
						punctuation = _isPunctuation(c);
						if (unit === "wordend") {
							if (punctuation && !previousPunctuation) { break; }
						} else {
							if (!punctuation && previousPunctuation) { break; }
						}
						letterOrDigit  = !punctuation && !_isWhitespace(c);
						if (unit === "wordend") {
							if (letterOrDigit && !previousLetterOrDigit) { break; }
						} else {
							if (!letterOrDigit && previousLetterOrDigit) { break; }
						}
						previousLetterOrDigit = letterOrDigit;
						previousPunctuation = punctuation;
						offsetInLine--;
					}
				}
				return lineStart + offsetInLine;
			}
			return offset + direction;
		},
		_getOffset_IE: function (offset, unit, direction) {
			var document = this._frameDocument;
			var model = this._model;
			var lineIndex = model.getLineAtOffset(offset);
			var clientDiv = this._clientDiv;
			var dummy;
			var child = this._getLineNode(lineIndex);
			if (!child) {
				child = dummy = this._createLine(clientDiv, null, document, lineIndex, model);
			}
			var result = 0, range, length;
			var lineOffset = model.getLineStart(lineIndex);
			if (offset === model.getLineEnd(lineIndex)) {
				range = document.body.createTextRange();
				range.moveToElementText(child.lastChild);
				length = range.text.length;
				range.moveEnd(unit, direction);
				result = offset + range.text.length - length;
			} else if (offset === lineOffset && direction < 0) {
				result = lineOffset;
			} else {
				var lineChild = child.firstChild;
				while (lineChild) {
					var textNode = lineChild.firstChild;
					var nodeLength = textNode.length;
					if (lineChild.ignoreChars) {
						nodeLength -= lineChild.ignoreChars;
					}
					if (lineOffset + nodeLength > offset) {
						range = document.body.createTextRange();
						if (offset === lineOffset && direction < 0) {
							range.moveToElementText(lineChild.previousSibling);
						} else {
							range.moveToElementText(lineChild);
							range.collapse();
							range.moveEnd("character", offset - lineOffset);
						}
						length = range.text.length;
						range.moveEnd(unit, direction);
						result = offset + range.text.length - length;
						break;
					}
					lineOffset = nodeLength + lineOffset;
					lineChild = lineChild.nextSibling;
				}
			}
			if (dummy) { clientDiv.removeChild(dummy); }
			return result;
		},
		_getOffsetToX: function (offset) {
			return this._getBoundsAtOffset(offset).left;
		},
		_getPadding: function (node) {
			var left,top,right,bottom;
			if (node.currentStyle) {
				left = node.currentStyle.paddingLeft;
				top = node.currentStyle.paddingTop;
				right = node.currentStyle.paddingRight;
				bottom = node.currentStyle.paddingBottom;
			} else if (this._frameWindow.getComputedStyle) {
				var style = this._frameWindow.getComputedStyle(node, null);
				left = style.getPropertyValue("padding-left");
				top = style.getPropertyValue("padding-top");
				right = style.getPropertyValue("padding-right");
				bottom = style.getPropertyValue("padding-bottom");
			}
			return {
					left: parseInt(left, 10), 
					top: parseInt(top, 10),
					right: parseInt(right, 10),
					bottom: parseInt(bottom, 10)
			};
		},
		_getScroll: function() {
			var viewDiv = this._viewDiv;
			return {x: viewDiv.scrollLeft, y: viewDiv.scrollTop};
		},
		_getSelection: function () {
			return this._selection.clone();
		},
		_getTopIndex: function (fullyVisible) {
			var child = this._topChild;
			if (fullyVisible && this._getClientHeight() > this._getLineHeight()) {
				var rect = child.getBoundingClientRect();
				var viewPad = this._getViewPadding();
				var viewRect = this._viewDiv.getBoundingClientRect();
				if (rect.top < viewRect.top + viewPad.top) {
					child = this._getLineNext(child) || child;
				}
			}
			return child.lineIndex;
		},
		_getXToOffset: function (lineIndex, x) {
			var model = this._model;
			var lineStart = model.getLineStart(lineIndex);
			var lineEnd = model.getLineEnd(lineIndex);
			if (lineStart === lineEnd) {
				return lineStart;
			}
			var document = this._frameDocument;
			var clientDiv = this._clientDiv;
			var dummy;
			var child = this._getLineNode(lineIndex);
			if (!child) {
				child = dummy = this._createLine(clientDiv, null, document, lineIndex, model);
			}
			var lineRect = this._getLineBoundingClientRect(child);
			if (x < lineRect.left) { x = lineRect.left; }
			if (x > lineRect.right) { x = lineRect.right; }
			/*
			* Bug in IE 8 and earlier. The coordinates of getClientRects() are relative to
			* the browser window.  The fix is to convert to the frame window before using it. 
			*/
			var deltaX = 0, rects;
			if (isIE < 9) {
				rects = child.getClientRects();
				var minLeft = rects[0].left;
				for (var i=1; i<rects.length; i++) {
					minLeft = Math.min(rects[i].left, minLeft);
				}
				deltaX = minLeft - lineRect.left;
			}
			var scrollX = this._getScroll().x;
			function _getClientRects(element) {
				var rects, newRects, i, r;
				if (!element._rectsCache) {
					rects = element.getClientRects();
					newRects = [rects.length];
					for (i = 0; i<rects.length; i++) {
						r = rects[i];
						newRects[i] = {left: r.left - deltaX + scrollX, top: r.top, right: r.right - deltaX + scrollX, bottom: r.bottom};
					}
					element._rectsCache = newRects; 
				}
				rects = element._rectsCache;
				newRects = [rects.length];
				for (i = 0; i<rects.length; i++) {
					r = rects[i];
					newRects[i] = {left: r.left - scrollX, top: r.top, right: r.right - scrollX, bottom: r.bottom};
				}
				return newRects;
			}
			var logicalXDPI = isIE ? window.screen.logicalXDPI : 1;
			var deviceXDPI = isIE ? window.screen.deviceXDPI : 1;
			var offset = lineStart;
			var lineChild = child.firstChild;
			done:
			while (lineChild) {
				var textNode = lineChild.firstChild;
				var nodeLength = textNode.length;
				if (lineChild.ignoreChars) {
					nodeLength -= lineChild.ignoreChars;
				}
				rects = _getClientRects(lineChild);
				for (var j = 0; j < rects.length; j++) {
					var rect = rects[j];
					if (rect.left <= x && x < rect.right) {
						var range, start, end;
						if (isIE || isRangeRects) {
							range = isRangeRects ? document.createRange() : document.body.createTextRange();
							var high = nodeLength;
							var low = -1;
							while ((high - low) > 1) {
								var mid = Math.floor((high + low) / 2);
								start = low + 1;
								end = mid === nodeLength - 1 && lineChild.ignoreChars ? textNode.length : mid + 1;
								if (isRangeRects) {
									range.setStart(textNode, start);
									range.setEnd(textNode, end);
								} else {
									range.moveToElementText(lineChild);
									range.move("character", start);
									range.moveEnd("character", end - start);
								}
								rects = range.getClientRects();
								var found = false;
								for (var k = 0; k < rects.length; k++) {
									rect = rects[k];
									var rangeLeft = rect.left * logicalXDPI / deviceXDPI - deltaX;
									var rangeRight = rect.right * logicalXDPI / deviceXDPI - deltaX;
									if (rangeLeft <= x && x < rangeRight) {
										found = true;
										break;
									}
								}
								if (found) {
									high = mid;
								} else {
									low = mid;
								}
							}
							offset += high;
							start = high;
							end = high === nodeLength - 1 && lineChild.ignoreChars ? textNode.length : Math.min(high + 1, textNode.length);
							if (isRangeRects) {
								range.setStart(textNode, start);
								range.setEnd(textNode, end);
							} else {
								range.moveToElementText(lineChild);
								range.move("character", start);
								range.moveEnd("character", end - start);
							}
							rect = range.getClientRects()[0];
							//TODO test for character trailing (wrong for bidi)
							if (x > ((rect.left * logicalXDPI / deviceXDPI - deltaX) + ((rect.right - rect.left) * logicalXDPI / deviceXDPI / 2))) {
								offset++;
							}
						} else {
							var newText = [];
							for (var q = 0; q < nodeLength; q++) {
								newText.push("<span>");
								if (q === nodeLength - 1) {
									newText.push(textNode.data.substring(q));
								} else {
									newText.push(textNode.data.substring(q, q + 1));
								}
								newText.push("</span>");
							}
							lineChild.innerHTML = newText.join("");
							var rangeChild = lineChild.firstChild;
							while (rangeChild) {
								rect = rangeChild.getBoundingClientRect();
								if (rect.left <= x && x < rect.right) {
									//TODO test for character trailing (wrong for bidi)
									if (x > rect.left + (rect.right - rect.left) / 2) {
										offset++;
									}
									break;
								}
								offset++;
								rangeChild = rangeChild.nextSibling;
							}
							if (!dummy) {
								lineChild.innerHTML = "";
								lineChild.appendChild(textNode);
								/*
								 * Removing the element node that holds the selection start or end
								 * causes the selection to be lost. The fix is to detect this case
								 * and restore the selection. 
								 */
								var s = this._getSelection();
								if ((offset <= s.start && s.start < offset + nodeLength) || (offset <= s.end && s.end < offset + nodeLength)) {
									this._updateDOMSelection();
								}
							}
						}
						break done;
					}
				}
				offset += nodeLength;
				lineChild = lineChild.nextSibling;
			}
			if (dummy) { clientDiv.removeChild(dummy); }
			return Math.min(lineEnd, Math.max(lineStart, offset));
		},
		_getYToLine: function (y) {
			var viewPad = this._getViewPadding();
			var viewRect = this._viewDiv.getBoundingClientRect();
			y -= viewRect.top + viewPad.top;
			var lineHeight = this._getLineHeight();
			var lineIndex = Math.floor((y + this._getScroll().y) / lineHeight);
			var lineCount = this._model.getLineCount();
			return Math.max(0, Math.min(lineCount - 1, lineIndex));
		},
		_getOffsetBounds: function(offset) {
			var model = this._model;
			var lineIndex = model.getLineAtOffset(offset);
			var lineHeight = this._getLineHeight();
			var scroll = this._getScroll();
			var viewPad = this._getViewPadding();
			var viewRect = this._viewDiv.getBoundingClientRect();
			var bounds = this._getBoundsAtOffset(offset);
			var left = bounds.left;
			var right = bounds.right;
			var top = (lineIndex * lineHeight) - scroll.y + viewRect.top + viewPad.top;
			var bottom = top + lineHeight;
			return {left: left, top: top, right: right, bottom: bottom};
		},
		_getVisible: function() {
			var temp = this._parent;
			var parentDocument = temp.ownerDocument;
			while (temp !== parentDocument) {
				var hidden;
				if (isIE < 9) {
					hidden = temp.currentStyle && temp.currentStyle.display === "none";
				} else {
					var tempStyle = parentDocument.defaultView.getComputedStyle(temp, null);
					hidden = tempStyle && tempStyle.getPropertyValue("display") === "none";
				}
				if (hidden) { return "hidden"; }
				temp =  temp.parentNode;
				if (!temp) { return "disconnected"; }
			}
			return "visible";
		},
		_hitOffset: function (offset, x, y) {
			var bounds = this._getOffsetBounds(offset);
			var left = bounds.left;
			var right = bounds.right;
			var top = bounds.top;
			var bottom = bounds.bottom;
			var area = 20;
			left -= area;
			top -= area;
			right += area;
			bottom += area;
			return (left <= x && x <= right && top <= y && y <= bottom);
		},
		_hookEvents: function() {
			var self = this;
			this._modelListener = {
				/** @private */
				onChanging: function(modelChangingEvent) {
					self._onModelChanging(modelChangingEvent);
				},
				/** @private */
				onChanged: function(modelChangedEvent) {
					self._onModelChanged(modelChangedEvent);
				}
			};
			this._model.addListener(this._modelListener);
			
			var clientDiv = this._clientDiv;
			var viewDiv = this._viewDiv;
			var body = this._frameDocument.body; 
			var handlers = this._handlers = [];
			var resizeNode = isIE < 9 ? this._frame : this._frameWindow;
			var focusNode = isPad ? this._textArea : (isIE ||  isFirefox ? this._clientDiv: this._frameWindow);
			handlers.push({target: this._frameWindow, type: "unload", handler: function(e) { return self._handleUnload(e);}});
			handlers.push({target: resizeNode, type: "resize", handler: function(e) { return self._handleResize(e);}});
			handlers.push({target: focusNode, type: "blur", handler: function(e) { return self._handleBlur(e);}});
			handlers.push({target: focusNode, type: "focus", handler: function(e) { return self._handleFocus(e);}});
			handlers.push({target: viewDiv, type: "scroll", handler: function(e) { return self._handleScroll(e);}});
			if (isPad) {
				var touchDiv = this._touchDiv;
				var textArea = this._textArea;
				handlers.push({target: textArea, type: "keydown", handler: function(e) { return self._handleKeyDown(e);}});
				handlers.push({target: textArea, type: "input", handler: function(e) { return self._handleInput(e); }});
				handlers.push({target: textArea, type: "textInput", handler: function(e) { return self._handleTextInput(e); }});
				handlers.push({target: textArea, type: "click", handler: function(e) { return self._handleTextAreaClick(e); }});
				handlers.push({target: touchDiv, type: "touchstart", handler: function(e) { return self._handleTouchStart(e); }});
				handlers.push({target: touchDiv, type: "touchmove", handler: function(e) { return self._handleTouchMove(e); }});
				handlers.push({target: touchDiv, type: "touchend", handler: function(e) { return self._handleTouchEnd(e); }});
			} else {
				var topNode = this._overlayDiv || this._clientDiv;
				var dragNode = this._dragNode || topNode;
				var grabNode = isIE ? clientDiv : this._frameWindow;
				handlers.push({target: clientDiv, type: "keydown", handler: function(e) { return self._handleKeyDown(e);}});
				handlers.push({target: clientDiv, type: "keypress", handler: function(e) { return self._handleKeyPress(e);}});
				handlers.push({target: clientDiv, type: "keyup", handler: function(e) { return self._handleKeyUp(e);}});
				handlers.push({target: clientDiv, type: "selectstart", handler: function(e) { return self._handleSelectStart(e);}});
				handlers.push({target: clientDiv, type: "contextmenu", handler: function(e) { return self._handleContextMenu(e);}});
				handlers.push({target: clientDiv, type: "copy", handler: function(e) { return self._handleCopy(e);}});
				handlers.push({target: clientDiv, type: "cut", handler: function(e) { return self._handleCut(e);}});
				handlers.push({target: clientDiv, type: "paste", handler: function(e) { return self._handlePaste(e);}});
				handlers.push({target: clientDiv, type: "mousedown", handler: function(e) { return self._handleMouseDown(e);}});
				handlers.push({target: grabNode, type: "mouseup", handler: function(e) { return self._handleMouseUp(e);}});
				handlers.push({target: grabNode, type: "mousemove", handler: function(e) { return self._handleMouseMove(e);}});
				handlers.push({target: body, type: "mousedown", handler: function(e) { return self._handleBodyMouseDown(e);}});
				handlers.push({target: dragNode, type: "dragstart", handler: function(e) { return self._handleDragStart(e);}});
				if (isDnD) {
					handlers.push({target: dragNode, type: "dragend", handler: function(e) { return self._handleDragEnd(e);}});
					handlers.push({target: dragNode, type: "dragenter", handler: function(e) { return self._handleDragEnter(e);}});
				}
				handlers.push({target: dragNode, type: "dragover", handler: function(e) { return self._handleDragOver(e);}});
				handlers.push({target: dragNode, type: "drop", handler: function(e) { return self._handleDrop(e);}});
				if (isChrome) {
					handlers.push({target: this._parentDocument, type: "mousemove", handler: function(e) { return self._handleMouseMove(e);}});
					handlers.push({target: this._parentDocument, type: "mouseup", handler: function(e) { return self._handleMouseUp(e);}});
				}
				if (isIE) {
					handlers.push({target: this._frameDocument, type: "activate", handler: function(e) { return self._handleDocFocus(e); }});
				}
				if (isFirefox) {
					handlers.push({target: this._frameDocument, type: "focus", handler: function(e) { return self._handleDocFocus(e); }});
				}
				if (!isIE && !isOpera) {
					var wheelEvent = isFirefox ? "DOMMouseScroll" : "mousewheel";
					handlers.push({target: this._viewDiv, type: wheelEvent, handler: function(e) { return self._handleMouseWheel(e); }});
				}
				if (isFirefox && !isWindows) {
					handlers.push({target: this._clientDiv, type: "DOMCharacterDataModified", handler: function (e) { return self._handleDataModified(e); }});
				}
				if (this._overlayDiv) {
					handlers.push({target: this._overlayDiv, type: "mousedown", handler: function(e) { return self._handleMouseDown(e);}});
					handlers.push({target: this._overlayDiv, type: "contextmenu", handler: function(e) { return self._handleContextMenu(e); }});
				}
				if (!isW3CEvents) {
					handlers.push({target: this._clientDiv, type: "dblclick", handler: function(e) { return self._handleDblclick(e); }});
				}
			}
			for (var i=0; i<handlers.length; i++) {
				var h = handlers[i];
				addHandler(h.target, h.type, h.handler, h.capture);
			}
		},
		_init: function(options) {
			var parent = options.parent;
			if (typeof(parent) === "string") {
				parent = window.document.getElementById(parent);
			}
			if (!parent) { throw "no parent"; }
			this._parent = parent;
			this._model = options.model ? options.model : new orion.textview.TextModel();
			this.readonly = options.readonly === true;
			this._fullSelection = options.fullSelection === undefined || options.fullSelection;
			/* 
			* Bug in IE 8. For some reason, during scrolling IE does not reflow the elements
			* that are used to compute the location for the selection divs. This causes the
			* divs to be placed at the wrong location. The fix is to disabled full selection for IE8.
			*/
			if (isIE < 9) {
				this._fullSelection = false;
			}
			this._stylesheet = options.stylesheet;
			this._tabSize = options.tabSize;
			this._rulers = [];
			this._selection = new Selection (0, 0, false);
			this._linksVisible = false;
			this._eventTable = new EventTable();
			this._redrawCount = 0;
			this._maxLineWidth = 0;
			this._maxLineIndex = -1;
			this._ignoreSelect = true;
			this._columnX = -1;
			
			this._dragStartSelection = null;
			this._dropDestination = null;

			/* Auto scroll */
			this._autoScrollX = null;
			this._autoScrollY = null;
			this._autoScrollTimerID = null;
			this._AUTO_SCROLL_RATE = 50;
			this._grabControl = null;
			this._moseMoveClosure  = null;
			this._mouseUpClosure = null;
			
			/* Double click */
			this._lastMouseX = 0;
			this._lastMouseY = 0;
			this._lastMouseTime = 0;
			this._clickCount = 0;
			this._clickTime = 250;
			this._clickDist = 5;
			this._isMouseDown = false;
			this._doubleClickSelection = null;
			
			/* Scroll */
			this._hScroll = 0;
			this._vScroll = 0;

			/* IME */
			this._imeOffset = -1;
			
			/* Create elements */
			this._createActions();
			this._createFrame();
		},
		_modifyContent: function(e, updateCaret) {
			if (this.readonly && !e._code) {
				return;
			}

			this.onVerify(e);

			if (e.text === null || e.text === undefined) { return; }
			
			var model = this._model;
			try {
				if (e._ignoreDOMSelection) { this._ignoreDOMSelection = true; }
				model.setText (e.text, e.start, e.end);
			} finally {
				if (e._ignoreDOMSelection) { this._ignoreDOMSelection = false; }
			}
			
			if (updateCaret) {
				var selection = this._getSelection ();
				selection.setCaret(e.start + e.text.length);
				this._setSelection(selection, true);
			}
			this.onModify({});
		},
		_onModelChanged: function(modelChangedEvent) {
			this.onModelChanged(modelChangedEvent);
			var start = modelChangedEvent.start;
			var addedCharCount = modelChangedEvent.addedCharCount;
			var removedCharCount = modelChangedEvent.removedCharCount;
			var addedLineCount = modelChangedEvent.addedLineCount;
			var removedLineCount = modelChangedEvent.removedLineCount;
			var selection = this._getSelection();
			if (selection.end > start) {
				if (selection.end > start && selection.start < start + removedCharCount) {
					// selection intersects replaced text. set caret behind text change
					selection.setCaret(start + addedCharCount);
				} else {
					// move selection to keep same text selected
					selection.start +=  addedCharCount - removedCharCount;
					selection.end +=  addedCharCount - removedCharCount;
				}
				this._setSelection(selection, false, false);
			}
			
			var model = this._model;
			var startLine = model.getLineAtOffset(start);
			var child = this._getLineNext();
			while (child) {
				var lineIndex = child.lineIndex;
				if (startLine <= lineIndex && lineIndex <= startLine + removedLineCount) {
					child.lineChanged = true;
				}
				if (lineIndex > startLine + removedLineCount) {
					child.lineIndex = lineIndex + addedLineCount - removedLineCount;
				}
				child = this._getLineNext(child);
			}
			if (startLine <= this._maxLineIndex && this._maxLineIndex <= startLine + removedLineCount) {
				this._checkMaxLineIndex = this._maxLineIndex;
				this._maxLineIndex = -1;
				this._maxLineWidth = 0;
			}
			this._updatePage();
		},
		_onModelChanging: function(modelChangingEvent) {
			this.onModelChanging(modelChangingEvent);
		},
		_queueUpdatePage: function() {
			if (this._updateTimer) { return; }
			var self = this;
			this._updateTimer = setTimeout(function() { 
				self._updateTimer = null;
				self._updatePage();
			}, 0);
		},
		_reset: function() {
			this._maxLineIndex = -1;
			this._maxLineWidth = 0;
			this._columnX = -1;
			this._topChild = null;
			this._bottomChild = null;
			this._partialY = 0;
			this._setSelection(new Selection (0, 0, false), false, false);
			if (this._viewDiv) {
				this._viewDiv.scrollLeft = 0;
				this._viewDiv.scrollTop = 0;
			}
			var clientDiv = this._clientDiv;
			if (clientDiv) {
				var child = clientDiv.firstChild;
				while (child) {
					child.lineChanged = true;
					child = child.nextSibling;
				}
				/*
				* Bug in Firefox.  For some reason, the caret does not show after the
				* view is refreshed.  The fix is to toggle the contentEditable state and
				* force the clientDiv to loose and receive focus if the it is focused.
				*/
				if (isFirefox) {
					var hasFocus = this._hasFocus;
					if (hasFocus) { clientDiv.blur(); }
					clientDiv.contentEditable = false;
					clientDiv.contentEditable = true;
					if (hasFocus) { clientDiv.focus(); }
				}
			}
		},
		_resizeTouchDiv: function() {
			var viewRect = this._viewDiv.getBoundingClientRect();
			var parentRect = this._frame.getBoundingClientRect();
			var temp = this._frame;
			while (temp) {
				if (temp.style && temp.style.top) { break; }
				temp = temp.parentNode;
			}
			var parentTop = parentRect.top;
			if (temp) {
				parentTop -= temp.getBoundingClientRect().top;
			} else {
				parentTop += this._parentDocument.body.scrollTop;
			}
			temp = this._frame;
			while (temp) {
				if (temp.style && temp.style.left) { break; }
				temp = temp.parentNode;
			}
			var parentLeft = parentRect.left;
			if (temp) {
				parentLeft -= temp.getBoundingClientRect().left;
			} else {
				parentLeft += this._parentDocument.body.scrollLeft;
			}
			var touchDiv = this._touchDiv;
			touchDiv.style.left = (parentLeft + viewRect.left) + "px";
			touchDiv.style.top = (parentTop + viewRect.top) + "px";
			touchDiv.style.width = viewRect.width + "px";
			touchDiv.style.height = viewRect.height + "px";
		},
		_scrollView: function (pixelX, pixelY) {
			/*
			* Always set _ensureCaretVisible to false so that the view does not scroll
			* to show the caret when scrollView is not called from showCaret().
			*/
			this._ensureCaretVisible = false;
			
			/*
			* Scrolling is done only by setting the scrollLeft and scrollTop fields in the
			* view div. This causes an updatePage from the scroll event. In some browsers 
			* this event is asynchromous and forcing update page to run synchronously
			* leads to redraw problems. 
			* On Chrome 11, the view redrawing at times when holding PageDown/PageUp key.
			* On Firefox 4 for Linux, the view redraws the first page when holding 
			* PageDown/PageUp key, but it will not redraw again until the key is released.
			*/
			var viewDiv = this._viewDiv;
			if (pixelX) { viewDiv.scrollLeft += pixelX; }
			if (pixelY) { viewDiv.scrollTop += pixelY; }
		},
		_setClipboardText: function (text, event) {
			var clipboardText;
			if (this._frameWindow.clipboardData) {
				//IE
				clipboardText = [];
				this._convertDelimiter(text, function(t) {clipboardText.push(t);}, function() {clipboardText.push(platformDelimiter);});
				return this._frameWindow.clipboardData.setData("Text", clipboardText.join(""));
			}
			/* Feature in Chrome, clipboardData.setData is no-op on Chrome even though it returns true */
			if (isChrome || isFirefox || !event) {
				var window = this._frameWindow;
				var document = this._frameDocument;
				var child = document.createElement("PRE");
				child.style.position = "fixed";
				child.style.left = "-1000px";
				this._convertDelimiter(text, 
					function(t) {
						child.appendChild(document.createTextNode(t));
					}, 
					function() {
						child.appendChild(document.createElement("BR"));
					}
				);
				child.appendChild(document.createTextNode(" "));
				this._clientDiv.appendChild(child);
				var range = document.createRange();
				range.setStart(child.firstChild, 0);
				range.setEndBefore(child.lastChild);
				var sel = window.getSelection();
				if (sel.rangeCount > 0) { sel.removeAllRanges(); }
				sel.addRange(range);
				var self = this;
				/** @ignore */
				var cleanup = function() {
					if (child && child.parentNode === self._clientDiv) {
						self._clientDiv.removeChild(child);
					}
					self._updateDOMSelection();
				};
				var result = false;
				/* 
				* Try execCommand first, it works on firefox with clipboard permission,
				* chrome 5, safari 4.
				*/
				this._ignoreCopy = true;
				try {
					result = document.execCommand("copy", false, null);
				} catch (e) {}
				this._ignoreCopy = false;
				if (!result) {
					if (event) {
						setTimeout(cleanup, 0);
						return false;
					}
				}
				/* no event and no permission, copy can not be done */
				cleanup();
				return true;
			}
			if (event && event.clipboardData) {
				//webkit
				clipboardText = [];
				this._convertDelimiter(text, function(t) {clipboardText.push(t);}, function() {clipboardText.push(platformDelimiter);});
				return event.clipboardData.setData("text/plain", clipboardText.join("")); 
			}
		},
		_setDOMSelection: function (startNode, startOffset, endNode, endOffset) {
			var window = this._frameWindow;
			var document = this._frameDocument;
			var startLineNode, startLineOffset, endLineNode, endLineOffset;
			var offset = 0;
			var lineChild = startNode.firstChild;
			var node, nodeLength, model = this._model;
			var startLineEnd = model.getLine(startNode.lineIndex).length;
			while (lineChild) {
				node = lineChild.firstChild;
				nodeLength = node.length;
				if (lineChild.ignoreChars) {
					nodeLength -= lineChild.ignoreChars;
				}
				if (offset + nodeLength > startOffset || offset + nodeLength >= startLineEnd) {
					startLineNode = node;
					startLineOffset = startOffset - offset;
					if (lineChild.ignoreChars && nodeLength > 0 && startLineOffset === nodeLength) {
						startLineOffset += lineChild.ignoreChars; 
					}
					break;
				}
				offset += nodeLength;
				lineChild = lineChild.nextSibling;
			}
			offset = 0;
			lineChild = endNode.firstChild;
			var endLineEnd = this._model.getLine(endNode.lineIndex).length;
			while (lineChild) {
				node = lineChild.firstChild;
				nodeLength = node.length;
				if (lineChild.ignoreChars) {
					nodeLength -= lineChild.ignoreChars;
				}
				if (nodeLength + offset > endOffset || offset + nodeLength >= endLineEnd) {
					endLineNode = node;
					endLineOffset = endOffset - offset;
					if (lineChild.ignoreChars && nodeLength > 0 && endLineOffset === nodeLength) {
						endLineOffset += lineChild.ignoreChars; 
					}
					break;
				}
				offset += nodeLength;
				lineChild = lineChild.nextSibling;
			}
			
			this._setDOMFullSelection(startNode, startOffset, startLineEnd, endNode, endOffset, endLineEnd);
			if (isPad) { return; }

			var range;
			if (window.getSelection) {
				//W3C
				range = document.createRange();
				range.setStart(startLineNode, startLineOffset);
				range.setEnd(endLineNode, endLineOffset);
				var sel = window.getSelection();
				this._ignoreSelect = false;
				if (sel.rangeCount > 0) { sel.removeAllRanges(); }
				sel.addRange(range);
				this._ignoreSelect = true;
			} else if (document.selection) {
				//IE < 9
				var body = document.body;

				/*
				* Bug in IE. For some reason when text is deselected the overflow
				* selection at the end of some lines does not get redrawn.  The
				* fix is to create a DOM element in the body to force a redraw.
				*/
				var child = document.createElement("DIV");
				body.appendChild(child);
				body.removeChild(child);
				
				range = body.createTextRange();
				range.moveToElementText(startLineNode.parentNode);
				range.moveStart("character", startLineOffset);
				var endRange = body.createTextRange();
				endRange.moveToElementText(endLineNode.parentNode);
				endRange.moveStart("character", endLineOffset);
				range.setEndPoint("EndToStart", endRange);
				this._ignoreSelect = false;
				range.select();
				this._ignoreSelect = true;
			}
		},
		_setDOMFullSelection: function(startNode, startOffset, startLineEnd, endNode, endOffset, endLineEnd) {
			var model = this._model;
			if (this._selDiv1) {
				var startLineBounds, l;
				startLineBounds = this._getLineBoundingClientRect(startNode);
				if (startOffset === 0) {
					l = startLineBounds.left;
				} else {
					if (startOffset >= startLineEnd) {
						l = startLineBounds.right;
					} else {
						this._ignoreDOMSelection = true;
						l = this._getBoundsAtOffset(model.getLineStart(startNode.lineIndex) + startOffset).left;
						this._ignoreDOMSelection = false;
					}
				}
				var textArea = this._textArea;
				if (textArea && isPad) {
					textArea.selectionStart = textArea.selectionEnd = 0;
					var rect = this._frame.getBoundingClientRect();
					var touchRect = this._touchDiv.getBoundingClientRect();
					var viewBounds = this._viewDiv.getBoundingClientRect();
					if (!(viewBounds.left <= l && l <= viewBounds.left + viewBounds.width &&
						viewBounds.top <= startLineBounds.top && startLineBounds.top <= viewBounds.top + viewBounds.height) ||
						!(startNode === endNode && startOffset === endOffset))
					{
						textArea.style.left = "-1000px";
					} else {
						textArea.style.left = (l - 4 + rect.left - touchRect.left) + "px";
					}
					textArea.style.top = (startLineBounds.top + rect.top - touchRect.top) + "px";
					textArea.style.width = "6px";
					textArea.style.height = (startLineBounds.bottom - startLineBounds.top) + "px";
				}
			
				var selDiv = this._selDiv1;
				selDiv.style.width = "0px";
				selDiv.style.height = "0px";
				selDiv = this._selDiv2;
				selDiv.style.width = "0px";
				selDiv.style.height = "0px";
				selDiv = this._selDiv3;
				selDiv.style.width = "0px";
				selDiv.style.height = "0px";
				if (!(startNode === endNode && startOffset === endOffset)) {
					var handleWidth = isPad ? 2 : 0;
					var handleBorder = handleWidth + "px blue solid";
					var viewPad = this._getViewPadding();
					var clientRect = this._clientDiv.getBoundingClientRect();
					var viewRect = this._viewDiv.getBoundingClientRect();
					var left = viewRect.left + viewPad.left;
					var right = clientRect.right;
					var top = viewRect.top + viewPad.top;
					var bottom = clientRect.bottom;
					var r;
					var endLineBounds = this._getLineBoundingClientRect(endNode);
					if (endOffset === 0) {
						r = endLineBounds.left;
					} else {
						if (endOffset >= endLineEnd) {
							r = endLineBounds.right;
						} else {
							this._ignoreDOMSelection = true;
							r = this._getBoundsAtOffset(model.getLineStart(endNode.lineIndex) + endOffset).left;
							this._ignoreDOMSelection = false;
						}
					}
					var sel1Div = this._selDiv1;
					var sel1Left = Math.min(right, Math.max(left, l));
					var sel1Top = Math.min(bottom, Math.max(top, startLineBounds.top));
					var sel1Right = right;
					var sel1Bottom = Math.min(bottom, Math.max(top, startLineBounds.bottom));
					sel1Div.style.left = sel1Left + "px";
					sel1Div.style.top = sel1Top + "px";
					sel1Div.style.width = Math.max(0, sel1Right - sel1Left) + "px";
					sel1Div.style.height = Math.max(0, sel1Bottom - sel1Top) + (isPad ? 1 : 0) + "px";
					if (isPad) {
						sel1Div.style.borderLeft = handleBorder;
						sel1Div.style.borderRight = "0px";
					}
					if (startNode === endNode) {
						sel1Right = Math.min(r, right);
						sel1Div.style.width = Math.max(0, sel1Right - sel1Left - handleWidth * 2) + "px";
						if (isPad) {
							sel1Div.style.borderRight = handleBorder;
						}
					} else {
						var sel3Left = left;
						var sel3Top = Math.min(bottom, Math.max(top, endLineBounds.top));
						var sel3Right = Math.min(right, Math.max(left, r));
						var sel3Bottom = Math.min(bottom, Math.max(top, endLineBounds.bottom));
						var sel3Div = this._selDiv3;
						sel3Div.style.left = sel3Left + "px";
						sel3Div.style.top = sel3Top + "px";
						sel3Div.style.width = Math.max(0, sel3Right - sel3Left - handleWidth) + "px";
						sel3Div.style.height = Math.max(0, sel3Bottom - sel3Top) + "px";
						if (isPad) {
							sel3Div.style.borderRight = handleBorder;
						}
						if (sel3Top - sel1Bottom > 0) {
							var sel2Div = this._selDiv2;
							sel2Div.style.left = left + "px";
							sel2Div.style.top = sel1Bottom + "px";
							sel2Div.style.width = Math.max(0, right - left) + "px";
							sel2Div.style.height = Math.max(0, sel3Top - sel1Bottom) + (isPad ? 1 : 0) + "px";
						}
					}
				}
			}
		},
		_setGrab: function (target) {
			if (target === this._grabControl) { return; }
			if (target) {
				if (target.setCapture) { target.setCapture(); }
				this._grabControl = target;
			} else {
				if (this._grabControl.releaseCapture) { this._grabControl.releaseCapture(); }
				this._grabControl = null;
			}
		},
		_setLinksVisible: function(visible) {
			if (this._linksVisible === visible) { return; }
			this._linksVisible = visible;
			/*
			* Feature in IE.  The client div looses focus and does not regain it back
			* when the content editable flag is reset. The fix is to remember that it
			* had focus when the flag is cleared and give focus back to the div when
			* the flag is set.
			*/
			if (isIE && visible) {
				this._hadFocus = this._hasFocus;
			}
			var clientDiv = this._clientDiv;
			clientDiv.contentEditable = !visible;
			if (this._hadFocus && !visible) {
				clientDiv.focus();
			}
			if (this._overlayDiv) {
				this._overlayDiv.style.zIndex = visible ? "-1" : "1";
			}
			var document = this._frameDocument;
			var line = this._getLineNext();
			while (line) {
				if (line.hasLink) {
					var lineChild = line.firstChild;
					while (lineChild) {
						var next = lineChild.nextSibling;
						var style = lineChild.viewStyle;
						if (style && style.tagName === "A") {
							line.replaceChild(this._createRange(line, document, lineChild.firstChild.data, style), lineChild);
						}
						lineChild = next;
					}
				}
				line = this._getLineNext(line);
			}
		},
		_setSelection: function (selection, scroll, update, pageScroll) {
			if (selection) {
				this._columnX = -1;
				if (update === undefined) { update = true; }
				var oldSelection = this._selection; 
				if (!oldSelection.equals(selection)) {
					this._selection = selection;
					var e = {
						oldValue: {start:oldSelection.start, end:oldSelection.end},
						newValue: {start:selection.start, end:selection.end}
					};
					this.onSelection(e);
				}
				/* 
				* Always showCaret(), even when the selection is not changing, to ensure the
				* caret is visible. Note that some views do not scroll to show the caret during
				* keyboard navigation when the selection does not chanage. For example, line down
				* when the caret is already at the last line.
				*/
				if (scroll) { update = !this._showCaret(false, pageScroll); }
				
				/* 
				* Sometimes the browser changes the selection 
				* as result of method calls or "leaked" events. 
				* The fix is to set the visual selection even
				* when the logical selection is not changed.
				*/
				if (update) { this._updateDOMSelection(); }
			}
		},
		_setSelectionTo: function (x,y,extent) {
			var model = this._model, offset;
			var selection = this._getSelection();
			var lineIndex = this._getYToLine(y);
			if (this._clickCount === 1) {
				offset = this._getXToOffset(lineIndex, x);
				selection.extend(offset);
				if (!extent) { selection.collapse(); }
			} else {
				var word = (this._clickCount & 1) === 0;
				var start, end;
				if (word) {
					offset = this._getXToOffset(lineIndex, x);
					if (this._doubleClickSelection) {
						if (offset >= this._doubleClickSelection.start) {
							start = this._doubleClickSelection.start;
							end = this._getOffset(offset, "wordend", +1);
						} else {
							start = this._getOffset(offset, "word", -1);
							end = this._doubleClickSelection.end;
						}
					} else {
						start = this._getOffset(offset, "word", -1);
						end = this._getOffset(start, "wordend", +1);
					}
				} else {
					if (this._doubleClickSelection) {
						var doubleClickLine = model.getLineAtOffset(this._doubleClickSelection.start);
						if (lineIndex >= doubleClickLine) {
							start = model.getLineStart(doubleClickLine);
							end = model.getLineEnd(lineIndex);
						} else {
							start = model.getLineStart(lineIndex);
							end = model.getLineEnd(doubleClickLine);
						}
					} else {
						start = model.getLineStart(lineIndex);
						end = model.getLineEnd(lineIndex);
					}
				}
				selection.setCaret(start);
				selection.extend(end);
			} 
			this._setSelection(selection, true, true);
		},
		_showCaret: function (allSelection, pageScroll) {
			if (!this._clientDiv) { return; }
			var model = this._model;
			var selection = this._getSelection();
			var scroll = this._getScroll();
			var caret = selection.getCaret();
			var start = selection.start;
			var end = selection.end;
			var startLine = model.getLineAtOffset(start); 
			var endLine = model.getLineAtOffset(end);
			var endInclusive = Math.max(Math.max(start, model.getLineStart(endLine)), end - 1);
			var viewPad = this._getViewPadding();
			
			var clientWidth = this._getClientWidth();
			var leftEdge = viewPad.left;
			var rightEdge = viewPad.left + clientWidth;
			var bounds = this._getBoundsAtOffset(caret === start ? start : endInclusive);
			var left = bounds.left;
			var right = bounds.right;
			var minScroll = clientWidth / 4;
			if (allSelection && !selection.isEmpty() && startLine === endLine) {
				bounds = this._getBoundsAtOffset(caret === end ? start : endInclusive);
				var selectionWidth = caret === start ? bounds.right - left : right - bounds.left;
				if ((clientWidth - minScroll) > selectionWidth) {
					if (left > bounds.left) { left = bounds.left; }
					if (right < bounds.right) { right = bounds.right; }
				}
			}
			var viewRect = this._viewDiv.getBoundingClientRect(); 
			left -= viewRect.left;
			right -= viewRect.left;
			var pixelX = 0;
			if (left < leftEdge) {
				pixelX = Math.min(left - leftEdge, -minScroll);
			}
			if (right > rightEdge) {
				var maxScroll = this._scrollDiv.scrollWidth - scroll.x - clientWidth;
				pixelX = Math.min(maxScroll,  Math.max(right - rightEdge, minScroll));
			}

			var pixelY = 0;
			var topIndex = this._getTopIndex(true);
			var bottomIndex = this._getBottomIndex(true);
			var caretLine = model.getLineAtOffset(caret);
			var clientHeight = this._getClientHeight();
			if (!(topIndex <= caretLine && caretLine <= bottomIndex)) {
				var lineHeight = this._getLineHeight();
				var selectionHeight = allSelection ? (endLine - startLine) * lineHeight : 0;
				pixelY = caretLine * lineHeight;
				pixelY -= scroll.y;
				if (pixelY + lineHeight > clientHeight) {
					pixelY -= clientHeight - lineHeight;
					if (caret === start && start !== end) {
						pixelY += Math.min(clientHeight - lineHeight, selectionHeight);
					}
				} else {
					if (caret === end) {
						pixelY -= Math.min (clientHeight - lineHeight, selectionHeight);
					}
				}
				if (pageScroll) {
					if (pageScroll > 0) {
						if (pixelY > 0) {
							pixelY = Math.max(pixelY, pageScroll);
						}
					} else {
						if (pixelY < 0) {
							pixelY = Math.min(pixelY, pageScroll);
						}
					}
				}
			}

			if (pixelX !== 0 || pixelY !== 0) {
				this._scrollView (pixelX, pixelY);
				/*
				* When the view scrolls it is possible that one of the scrollbars can show over the caret.
				* Depending on the browser scrolling can be synchronous (Safari), in which case the change 
				* can be detected before showCaret() returns. When scrolling is asynchronous (most browsers), 
				* the detection is done during the next update page.
				*/
				if (clientHeight !== this._getClientHeight() || clientWidth !== this._getClientWidth()) {
					this._showCaret();
				} else {
					this._ensureCaretVisible = true;
				}
				return true;
			}
			return false;
		},
		_startIME: function () {
			if (this._imeOffset !== -1) { return; }
			var selection = this._getSelection();
			if (!selection.isEmpty()) {
				this._modifyContent({text: "", start: selection.start, end: selection.end}, true);
			}
			this._imeOffset = selection.start;
		},
		_unhookEvents: function() {
			this._model.removeListener(this._modelListener);
			this._modelListener = null;
			for (var i=0; i<this._handlers.length; i++) {
				var h = this._handlers[i];
				removeHandler(h.target, h.type, h.handler);
			}
			this._handlers = null;
		},
		_updateDOMSelection: function () {
			if (this._ignoreDOMSelection) { return; }
			if (!this._clientDiv) { return; }
			var selection = this._getSelection();
			var model = this._model;
			var startLine = model.getLineAtOffset(selection.start);
			var endLine = model.getLineAtOffset(selection.end);
			var firstNode = this._getLineNext();
			/*
			* Bug in Firefox. For some reason, after a update page sometimes the 
			* firstChild returns null incorrectly. The fix is to ignore show selection.
			*/
			if (!firstNode) { return; }
			var lastNode = this._getLinePrevious();
			
			var topNode, bottomNode, topOffset, bottomOffset;
			if (startLine < firstNode.lineIndex) {
				topNode = firstNode;
				topOffset = 0;
			} else if (startLine > lastNode.lineIndex) {
				topNode = lastNode;
				topOffset = 0;
			} else {
				topNode = this._getLineNode(startLine);
				topOffset = selection.start - model.getLineStart(startLine);
			}

			if (endLine < firstNode.lineIndex) {
				bottomNode = firstNode;
				bottomOffset = 0;
			} else if (endLine > lastNode.lineIndex) {
				bottomNode = lastNode;
				bottomOffset = 0;
			} else {
				bottomNode = this._getLineNode(endLine);
				bottomOffset = selection.end - model.getLineStart(endLine);
			}
			this._setDOMSelection(topNode, topOffset, bottomNode, bottomOffset);
		},
		_updatePage: function() {
			if (this._redrawCount > 0) { return; }
			if (this._updateTimer) { 
				clearTimeout(this._updateTimer);
				this._updateTimer = null;
			}
			var document = this._frameDocument;
			var viewDiv = this._viewDiv;
			var clientDiv = this._clientDiv;
			if (!clientDiv) { return; }
			var frameWidth = this._getFrameWidth();
			var frameHeight = this._getFrameHeight();
			document.body.style.width = frameWidth + "px";
			document.body.style.height = frameHeight + "px";
			var viewPad = this._getViewPadding();
			
			/* Update view height in order to have client height computed */
			viewDiv.style.height = Math.max(0, (frameHeight - viewPad.top - viewPad.bottom)) + "px";
			
			var model = this._model;
			var lineHeight = this._getLineHeight();
			var scrollY = this._getScroll().y;
			var firstLine = Math.max(0, scrollY) / lineHeight;
			var topIndex = Math.floor(firstLine);
			var lineStart = Math.max(0, topIndex - 1);
			var top = Math.round((firstLine - lineStart) * lineHeight);
			var lineCount = model.getLineCount();
			var clientHeight = this._getClientHeight();
			var partialY = Math.round((firstLine - topIndex) * lineHeight);
			var linesPerPage = Math.floor((clientHeight + partialY) / lineHeight);
			var bottomIndex = Math.min(topIndex + linesPerPage, lineCount - 1);
			var lineEnd = Math.min(bottomIndex + 1, lineCount - 1);
			this._partialY = partialY;
			
			var lineIndex, lineWidth;
			var child = clientDiv.firstChild;
			while (child) {
				lineIndex = child.lineIndex;
				var nextChild = child.nextSibling;
				if (!(lineStart <= lineIndex && lineIndex <= lineEnd) || child.lineChanged || child.lineIndex === -1) {
					if (this._mouseWheelLine === child) {
						child.style.display = "none";
						child.lineIndex = -1;
					} else {
						clientDiv.removeChild(child);
					}
				}
				child = nextChild;
			}

			child = this._getLineNext();
			var frag = document.createDocumentFragment();
			for (lineIndex=lineStart; lineIndex<=lineEnd; lineIndex++) {
				if (!child || child.lineIndex > lineIndex) {
					this._createLine(frag, null, document, lineIndex, model);
				} else {
					if (frag.firstChild) {
						clientDiv.insertBefore(frag, child);
						frag = document.createDocumentFragment();
					}
					child = this._getLineNext(child);
				}
			}
			if (frag.firstChild) { clientDiv.insertBefore(frag, child); }

			/*
			* Feature in WekKit. Webkit limits the width of the lines
			* computed below to the width of the client div.  This causes
			* the lines to be wrapped even though "pre" is set.  The fix
			* is to set the width of the client div to a larger number
			* before computing the lines width.  Note that this value is
			* reset to the appropriate value further down.
			*/ 
			if (isWebkit) {
				clientDiv.style.width = (0x7FFFF).toString() + "px";
			}

			var rect;
			child = this._getLineNext();
			while (child) {
				lineWidth = child.lineWidth;
				if (lineWidth === undefined) {
					rect = this._getLineBoundingClientRect(child);
					lineWidth = child.lineWidth = rect.right - rect.left;
				}
				if (lineWidth >= this._maxLineWidth) {
					this._maxLineWidth = lineWidth;
					this._maxLineIndex = child.lineIndex;
				}
				if (child.lineIndex === topIndex) { this._topChild = child; }
				if (child.lineIndex === bottomIndex) { this._bottomChild = child; }
				if (this._checkMaxLineIndex === child.lineIndex) { this._checkMaxLineIndex = -1; }
				child = this._getLineNext(child);
			}
			if (this._checkMaxLineIndex !== -1) {
				lineIndex = this._checkMaxLineIndex;
				this._checkMaxLineIndex = -1;
				if (0 <= lineIndex && lineIndex < lineCount) {
					var dummy = this._createLine(clientDiv, null, document, lineIndex, model);
					rect = this._getLineBoundingClientRect(dummy);
					lineWidth = rect.right - rect.left;
					if (lineWidth >= this._maxLineWidth) {
						this._maxLineWidth = lineWidth;
						this._maxLineIndex = lineIndex;
					}
					clientDiv.removeChild(dummy);
				}
			}

			// Update rulers
			this._updateRuler(this._leftDiv, topIndex, bottomIndex);
			this._updateRuler(this._rightDiv, topIndex, bottomIndex);
			
			var leftWidth = this._leftDiv ? this._leftDiv.scrollWidth : 0;
			var rightWidth = this._rightDiv ? this._rightDiv.scrollWidth : 0;
			viewDiv.style.left = leftWidth + "px";
			viewDiv.style.width = Math.max(0, frameWidth - leftWidth - rightWidth - viewPad.left - viewPad.right) + "px";
			if (this._rightDiv) {
				this._rightDiv.style.left = (frameWidth - rightWidth) + "px"; 
			}
			
			var scrollDiv = this._scrollDiv;
			/* Need to set the height first in order for the width to consider the vertical scrollbar */
			var scrollHeight = lineCount * lineHeight;
			scrollDiv.style.height = scrollHeight + "px";
			// TODO if frameHeightWithoutHScrollbar < scrollHeight  < frameHeightWithHScrollbar and the horizontal bar is visible, 
			// then the clientWidth is wrong because the vertical scrollbar is showing. To correct code should hide both scrollbars 
			// at this point.
			var clientWidth = this._getClientWidth();
			var width = Math.max(this._maxLineWidth, clientWidth);
			/*
			* Except by IE 8 and earlier, all other browsers are not allocating enough space for the right padding 
			* in the scrollbar. It is possible this a bug since all other paddings are considered.
			*/
			var scrollWidth = width;
			if (!isIE || isIE >= 9) { width += viewPad.right; }
			scrollDiv.style.width = width + "px";

			// Get the left scroll after setting the width of the scrollDiv as this can change the horizontal scroll offset.
			var scroll = this._getScroll();
			var left = scroll.x;
			var clipLeft = left;
			var clipTop = top;
			var clipRight = left + clientWidth;
			var clipBottom = top + clientHeight;
			if (clipLeft === 0) { clipLeft -= viewPad.left; }
			if (clipTop === 0) { clipTop -= viewPad.top; }
			if (clipRight === scrollWidth) { clipRight += viewPad.right; }
			if (scroll.y + clientHeight === scrollHeight) { clipBottom += viewPad.bottom; }
			clientDiv.style.clip = "rect(" + clipTop + "px," + clipRight + "px," + clipBottom + "px," + clipLeft + "px)";
			clientDiv.style.left = (-left + leftWidth + viewPad.left) + "px";
			clientDiv.style.top = (-top + viewPad.top) + "px";
			clientDiv.style.width = (isWebkit ? scrollWidth : clientWidth + left) + "px";
			clientDiv.style.height = (clientHeight + top) + "px";
			var overlayDiv = this._overlayDiv;
			if (overlayDiv) {
				overlayDiv.style.clip = clientDiv.style.clip;
				overlayDiv.style.left = clientDiv.style.left;
				overlayDiv.style.top = clientDiv.style.top;
				overlayDiv.style.width = clientDiv.style.width;
				overlayDiv.style.height = clientDiv.style.height;
			}
			function _updateRulerSize(divRuler) {
				if (!divRuler) { return; }
				var rulerHeight = clientHeight + viewPad.top + viewPad.bottom;
				var cells = divRuler.firstChild.rows[0].cells;
				for (var i = 0; i < cells.length; i++) {
					var div = cells[i].firstChild;
					var offset = lineHeight;
					if (div._ruler.getOverview() === "page") { offset += partialY; }
					div.style.top = -offset + "px";
					div.style.height = (rulerHeight + offset) + "px";
					div = div.nextSibling;
				}
				divRuler.style.height = rulerHeight + "px";
			}
			_updateRulerSize(this._leftDiv);
			_updateRulerSize(this._rightDiv);
			if (isPad) {
				var self = this;
				setTimeout(function() {self._resizeTouchDiv();}, 0);
			}
			this._updateDOMSelection();

			/*
			* If the client height changed during the update page it means that scrollbar has either been shown or hidden.
			* When this happens update page has to run again to ensure that the top and bottom lines div are correct.
			* 
			* Note: On IE, updateDOMSelection() has to be called before getting the new client height because it
			* forces the client area to be recomputed.
			*/
			var ensureCaretVisible = this._ensureCaretVisible;
			this._ensureCaretVisible = false;
			if (clientHeight !== this._getClientHeight()) {
				this._updatePage();
				if (ensureCaretVisible) {
					this._showCaret();
				}
			}
		},
		_updateRuler: function (divRuler, topIndex, bottomIndex) {
			if (!divRuler) { return; }
			var cells = divRuler.firstChild.rows[0].cells;
			var lineHeight = this._getLineHeight();
			var parentDocument = this._frameDocument;
			var viewPad = this._getViewPadding();
			for (var i = 0; i < cells.length; i++) {
				var div = cells[i].firstChild;
				var ruler = div._ruler;
				if (div.rulerChanged) {
					this._applyStyle(ruler.getRulerStyle(), div);
				}
				
				var widthDiv;
				var child = div.firstChild;
				if (child) {
					widthDiv = child;
					child = child.nextSibling;
				} else {
					widthDiv = parentDocument.createElement("DIV");
					widthDiv.style.visibility = "hidden";
					div.appendChild(widthDiv);
				}
				var lineIndex, annotation;
				if (div.rulerChanged) {
					if (widthDiv) {
						lineIndex = -1;
						annotation = ruler.getWidestAnnotation();
						if (annotation) {
							this._applyStyle(annotation.style, widthDiv);
							if (annotation.html) {
								widthDiv.innerHTML = annotation.html;
							}
						}
						widthDiv.lineIndex = lineIndex;
						widthDiv.style.height = (lineHeight + viewPad.top) + "px";
					}
				}

				var overview = ruler.getOverview(), lineDiv, frag, annotations;
				if (overview === "page") {
					annotations = ruler.getAnnotations(topIndex, bottomIndex + 1);
					while (child) {
						lineIndex = child.lineIndex;
						var nextChild = child.nextSibling;
						if (!(topIndex <= lineIndex && lineIndex <= bottomIndex) || child.lineChanged) {
							div.removeChild(child);
						}
						child = nextChild;
					}
					child = div.firstChild.nextSibling;
					frag = parentDocument.createDocumentFragment();
					for (lineIndex=topIndex; lineIndex<=bottomIndex; lineIndex++) {
						if (!child || child.lineIndex > lineIndex) {
							lineDiv = parentDocument.createElement("DIV");
							annotation = annotations[lineIndex];
							if (annotation) {
								this._applyStyle(annotation.style, lineDiv);
								if (annotation.html) {
									lineDiv.innerHTML = annotation.html;
								}
								lineDiv.annotation = annotation;
							}
							lineDiv.lineIndex = lineIndex;
							lineDiv.style.height = lineHeight + "px";
							frag.appendChild(lineDiv);
						} else {
							if (frag.firstChild) {
								div.insertBefore(frag, child);
								frag = parentDocument.createDocumentFragment();
							}
							if (child) {
								child = child.nextSibling;
							}
						}
					}
					if (frag.firstChild) { div.insertBefore(frag, child); }
				} else {
					var buttonHeight = isPad ? 0 : 17;
					var clientHeight = this._getClientHeight ();
					var lineCount = this._model.getLineCount ();
					var contentHeight = lineHeight * lineCount;
					var trackHeight = clientHeight + viewPad.top + viewPad.bottom - 2 * buttonHeight;
					var divHeight;
					if (contentHeight < trackHeight) {
						divHeight = lineHeight;
					} else {
						divHeight = trackHeight / lineCount;
					}
					if (div.rulerChanged) {
						var count = div.childNodes.length;
						while (count > 1) {
							div.removeChild(div.lastChild);
							count--;
						}
						annotations = ruler.getAnnotations(0, lineCount);
						frag = parentDocument.createDocumentFragment();
						for (var prop in annotations) {
							lineIndex = prop >>> 0;
							if (lineIndex < 0) { continue; }
							lineDiv = parentDocument.createElement("DIV");
							annotation = annotations[prop];
							this._applyStyle(annotation.style, lineDiv);
							lineDiv.style.position = "absolute";
							lineDiv.style.top = buttonHeight + lineHeight + Math.floor(lineIndex * divHeight) + "px";
							if (annotation.html) {
								lineDiv.innerHTML = annotation.html;
							}
							lineDiv.annotation = annotation;
							lineDiv.lineIndex = lineIndex;
							frag.appendChild(lineDiv);
						}
						div.appendChild(frag);
					} else if (div._oldTrackHeight !== trackHeight) {
						lineDiv = div.firstChild ? div.firstChild.nextSibling : null;
						while (lineDiv) {
							lineDiv.style.top = buttonHeight + lineHeight + Math.floor(lineDiv.lineIndex * divHeight) + "px";
							lineDiv = lineDiv.nextSibling;
						}
					}
					div._oldTrackHeight = trackHeight;
				}
				div.rulerChanged = false;
				div = div.nextSibling;
			}
		}
	};//end prototype
	
	return TextView;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define(['orion/textview/textModel', 'orion/textview/keyBinding'], function() {
		return orion.textview;
	});
}
/******************************************************************************* 
 * Copyright (c) 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation 
 ******************************************************************************/

/*jslint */
/*global define orion:true window */

var orion = orion || {};

orion.editor = orion.editor || {};

/**
 * Provides a grammar that can do some very rough syntax highlighting for HTML.
 * @class orion.syntax.HtmlGrammar
 */
orion.editor.HtmlGrammar = (function() {
	return {
		/**
		 * What kind of highlight provider we are.
		 * @public
		 * @type String
		 */
		type: "grammar",
		
		/**
		 * The file extensions that we provide rules for.
		 * @public
		 * @type String[]
		 */
		fileTypes: [ "html", "htm" ],
		
		/**
		 * Object containing the grammar rules.
		 * @public
		 * @type Object
		 */
		grammar: {
			"name": "HTML",
			"scopeName": "source.html",
			"uuid": "3B5C76FB-EBB5-D930-F40C-047D082CE99B",
			"patterns": [
				// TODO unicode?
				{
					"match": "<!(doctype|DOCTYPE)[^>]+>",
					"name": "entity.name.tag.doctype.html"
				},
				{
					"begin": "<!--",
					"end": "-->",
					"beginCaptures": {
						"0": { "name": "punctuation.definition.comment.html" }
					},
					"endCaptures": {
						"0": { "name": "punctuation.definition.comment.html" }
					},
					"patterns": [
						{
							"match": "--",
							"name": "invalid.illegal.badcomment.html"
						}
					],
					"contentName": "comment.block.html"
				},
				{ // startDelimiter + tagName
					"match": "<[A-Za-z0-9_\\-:]+(?= ?)",
					"name": "entity.name.tag.html"
				},
				{ "include": "#attrName" },
				{ "include": "#qString" },
				{ "include": "#qqString" },
				// TODO attrName, qString, qqString should be applied first while inside a tag
				{ // startDelimiter + slash + tagName + endDelimiter
					"match": "</[A-Za-z0-9_\\-:]+>",
					"name": "entity.name.tag.html"
				},
				{ // end delimiter of open tag
					"match": ">", 
					"name": "entity.name.tag.html"
				} ],
			"repository": {
				"attrName": { // attribute name
					"match": "[A-Za-z\\-:]+(?=\\s*=\\s*['\"])",
					"name": "entity.other.attribute.name.html"
				},
				"qqString": { // double quoted string
					"match": "(\")[^\"]+(\")",
					"name": "string.quoted.double.html"
				},
				"qString": { // single quoted string
					"match": "(')[^']+(\')",
					"name": "string.quoted.single.html"
				}
			}
		}
	};
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.editor;
	});
}
/******************************************************************************* 
 * Copyright (c) 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation 
 ******************************************************************************/

/*jslint regexp:false laxbreak:true*/
/*global define window*/

var orion = orion || {};
orion.editor = orion.editor || {};

/**
 * @class A styler that does nothing, but can be extended by concrete stylers. To extend, call 
 * {@link orion.editor.AbstractStyler.extend} and provide implementations of one or more of
 * the {@link #_onSelection}, {@link #_onModelChanged}, {@link #_onDestroy} and {@link #_onLineStyle} methods.
 * @name orion.editor.AbstractStyler
 */
orion.editor.AbstractStyler = (function() {
	/** @inner */
	function AbstractStyler() {
	}
	AbstractStyler.prototype = /** @lends orion.editor.AbstractStyler.prototype */ {
		/**
		 * Initializes this styler with a TextView. If you are extending AbstractStyler,
		 * you <b>must</b> call this from your subclass's constructor function.
		 * @param {orion.textview.TextView} textView The TextView to provide styling for.
		 */
		initialize: function(textView) {
			this.textView = textView;
			
			textView.addEventListener("Selection", this, this._onSelection);
			textView.addEventListener("ModelChanged", this, this._onModelChanged);
			textView.addEventListener("Destroy", this, this._onDestroy);
			textView.addEventListener("LineStyle", this, this._onLineStyle);
			textView.redrawLines();
		},
		/** Destroys this styler and removes all listeners. Called by the editor. */
		destroy: function() {
			if (this.textView) {
				this.textView.removeEventListener("Selection", this, this._onSelection);
				this.textView.removeEventListener("ModelChanged", this, this._onModelChanged);
				this.textView.removeEventListener("Destroy", this, this._onDestroy);
				this.textView.removeEventListener("LineStyle", this, this._onLineStyle);
				this.textView = null;
			}
		},
		/** To be overridden by subclass.
		 * @public
		 */
		_onSelection: function(/**eclipse.SelectionEvent*/ selectionEvent) {},
		/** To be overridden by subclass.
		 * @public
		 */
		_onModelChanged: function(/**eclipse.ModelChangedEvent*/ modelChangedEvent) {},
		/** To be overridden by subclass.
		 * @public
		 */
		_onDestroy: function(/**eclipse.DestroyEvent*/ destroyEvent) {},
		/** To be overridden by subclass.
		 * @public
		 */
		_onLineStyle: function(/**eclipse.LineStyleEvent*/ lineStyleEvent) {}
	};
	
	return AbstractStyler;
}());

/**
 * Helper for extending {@link orion.editor.AbstractStyler}.
 * @methodOf orion.editor.AbstractStyler
 * @static
 * @param {Function} subCtor The constructor function for the subclass.
 * @param {Object} [proto] Object to be mixed into the subclass's prototype. This object should 
 * contain your implementation of _onSelection, _onModelChanged, etc.
 */
orion.editor.AbstractStyler.extend = function(subCtor, proto) {
	if (typeof(subCtor) !== "function") { throw new Error("Function expected"); }
	subCtor.prototype = new orion.editor.AbstractStyler();
	subCtor.constructor = subCtor;
	for (var p in proto) {
		if (proto.hasOwnProperty(p)) { subCtor.prototype[p] = proto[p]; }
	}
};

orion.editor.RegexUtil = {
	// Rules to detect some unsupported Oniguruma features
	unsupported: [
		{regex: /\(\?[ims\-]:/, func: function(match) { return "option on/off for subexp"; }},
		{regex: /\(\?<([=!])/, func: function(match) { return (match[1] === "=") ? "lookbehind" : "negative lookbehind"; }},
		{regex: /\(\?>/, func: function(match) { return "atomic group"; }}
	],
	
	/**
	 * @param {String} str String giving a regular expression pattern from a TextMate grammar.
	 * @param {String} [flags] [ismg]+
	 * @returns {RegExp}
	 */
	toRegExp: function(str) {
		function fail(feature, match) {
			throw new Error("Unsupported regex feature \"" + feature + "\": \"" + match[0] + "\" at index: "
					+ match.index + " in " + match.input);
		}
		// Turns an extended regex pattern into a normal one
		function normalize(/**String*/ str) {
			var result = "";
			var insideCharacterClass = false;
			var len = str.length;
			for (var i=0; i < len; ) {
				var chr = str[i];
				if (!insideCharacterClass && chr === "#") {
					// skip to eol
					while (i < len && chr !== "\r" && chr !== "\n") {
						chr = str[++i];
					}
				} else if (!insideCharacterClass && /\s/.test(chr)) {
					// skip whitespace
					while (i < len && /\s/.test(chr)) { 
						chr = str[++i];
					}
				} else if (chr === "\\") {
					result += chr;
					if (!/\s/.test(str[i+1])) {
						result += str[i+1];
						i += 1;
					}
					i += 1;
				} else if (chr === "[") {
					insideCharacterClass = true;
					result += chr;
					i += 1;
				} else if (chr === "]") {
					insideCharacterClass = false;
					result += chr;
					i += 1;
				} else {
					result += chr;
					i += 1;
				}
			}
			return result;
		}
		
		var flags = "";
		var i;
		
		// Handle global "x" flag (whitespace/comments)
		str = orion.editor.RegexUtil.processGlobalFlag("x", str, function(subexp) {
				return normalize(subexp);
			});
		
		// Handle global "i" flag (case-insensitive)
		str = orion.editor.RegexUtil.processGlobalFlag("i", str, function(subexp) {
				flags += "i";
				return subexp;
			});
		
		// Check for remaining unsupported syntax
		for (i=0; i < this.unsupported.length; i++) {
			var match;
			if ((match = this.unsupported[i].regex.exec(str))) {
				fail(this.unsupported[i].func(match), match);
			}
		}
		
		return new RegExp(str, flags);
	},
	
	/**
	 * Checks if flag applies to entire pattern. If so, obtains replacement string by calling processor
	 * on the unwrapped pattern. Handles 2 possible syntaxes: (?f)pat and (?f:pat)
	 */
	processGlobalFlag: function(/**String*/ flag, /**String*/ str, /**Function*/ processor) {
		function getMatchingCloseParen(/*String*/pat, /*Number*/start) {
			var depth = 0,
			    len = pat.length,
			    flagStop = -1;
			for (var i=start; i < len && flagStop === -1; i++) {
				switch (pat[i]) {
					case "\\":
						i++; // escape: skip next char
						break;
					case "(":
						depth++;
						break;
					case ")":
						depth--;
						if (depth === 0) {
							flagStop = i;
						}
						break;
				}
			}
			return flagStop;
		}
		var flag1 = "(?" + flag + ")",
		    flag2 = "(?" + flag + ":";
		if (str.substring(0, flag1.length) === flag1) {
			return processor(str.substring(flag1.length));
		} else if (str.substring(0, flag2.length) === flag2) {
			var flagStop = getMatchingCloseParen(str, 0);
			if (flagStop < str.length-1) {
				throw new Error("Only a " + flag2 + ") group that encloses the entire regex is supported in: " + str);
			}
			return processor(str.substring(flag2.length, flagStop));
		}
		return str;
	},
	
	hasBackReference: function(/**RegExp*/ regex) {
		return (/\\\d+/).test(regex.source);
	},
	
	/** @returns {RegExp} A regex made by substituting any backreferences in <code>regex</code> for the value of the property
	 * in <code>sub</code> with the same name as the backreferenced group number. */
	getSubstitutedRegex: function(/**RegExp*/ regex, /**Object*/ sub, /**Boolean*/ escape) {
		escape = (typeof escape === "undefined") ? true : false;
		var exploded = regex.source.split(/(\\\d+)/g);
		var array = [];
		for (var i=0; i < exploded.length; i++) {
			var term = exploded[i];
			var backrefMatch = /\\(\d+)/.exec(term);
			if (backrefMatch) {
				var text = sub[backrefMatch[1]] || "";
				array.push(escape ? orion.editor.RegexUtil.escapeRegex(text) : text);
			} else {
				array.push(term);
			}
		}
		return new RegExp(array.join(""));
	},
	
	/** @returns {String} The input string with regex special characters escaped. */
	escapeRegex: function(/**String*/ str) {
		return str.replace(/([\\$\^*\/+?\.\(\)|{}\[\]])/g, "\\$&");
	},
	
	/**
	 * Builds a version of <code>regex</code> with every non-capturing term converted into a capturing group. This is a workaround
	 * for JavaScript's lack of API to get the index at which a matched group begins in the input string.<p>
	 * Using the "groupified" regex, we can sum the lengths of matches from <i>consuming groups</i> 1..n-1 to obtain the 
	 * starting index of group n. (A consuming group is a capturing group that is not inside a lookahead assertion).</p>
	 * Example: groupify(/(a+)x+(b+)/) === /(a+)(x+)(b+)/<br />
	 * Example: groupify(/(?:x+(a+))b+/) === /(?:(x+)(a+))(b+)/
	 * @param {RegExp} regex The regex to groupify.
	 * @param {Object} [backRefOld2NewMap] Optional. If provided, the backreference numbers in regex will be updated using the 
	 * properties of this object rather than the new group numbers of regex itself.
	 * <ul><li>[0] {RegExp} The groupified version of the input regex.</li>
	 * <li>[1] {Object} A map containing old-group to new-group info. Each property is a capturing group number of <code>regex</code>
	 * and its value is the corresponding capturing group number of [0].</li>
	 * <li>[2] {Object} A map indicating which capturing groups of [0] are also consuming groups. If a group number is found
	 * as a property in this object, then it's a consuming group.</li></ul>
	 */
	groupify: function(regex, backRefOld2NewMap) {
		var NON_CAPTURING = 1,
		    CAPTURING = 2,
		    LOOKAHEAD = 3,
		    NEW_CAPTURING = 4;
		var src = regex.source,
		    len = src.length;
		var groups = [],
		    lookaheadDepth = 0,
		    newGroups = [],
		    oldGroupNumber = 1,
		    newGroupNumber = 1;
		var result = [],
		    old2New = {},
		    consuming = {};
		for (var i=0; i < len; i++) {
			var curGroup = groups[groups.length-1];
			var chr = src[i];
			switch (chr) {
				case "(":
					// If we're in new capturing group, close it since ( signals end-of-term
					if (curGroup === NEW_CAPTURING) {
						groups.pop();
						result.push(")");
						newGroups[newGroups.length-1].end = i;
					}
					var peek2 = (i + 2 < len) ? (src[i+1] + "" + src[i+2]) : null;
					if (peek2 === "?:" || peek2 === "?=" || peek2 === "?!") {
						// Found non-capturing group or lookahead assertion. Note that we preserve non-capturing groups
						// as such, but any term inside them will become a new capturing group (unless it happens to
						// also be inside a lookahead).
						var groupType;
						if (peek2 === "?:") {
							groupType = NON_CAPTURING;
						} else {
							groupType = LOOKAHEAD;
							lookaheadDepth++;
						}
						groups.push(groupType);
						newGroups.push({ start: i, end: -1, type: groupType /*non capturing*/ });
						result.push(chr);
						result.push(peek2);
						i += peek2.length;
					} else {
						groups.push(CAPTURING);
						newGroups.push({ start: i, end: -1, type: CAPTURING, oldNum: oldGroupNumber, num: newGroupNumber });
						result.push(chr);
						if (lookaheadDepth === 0) {
							consuming[newGroupNumber] = null;
						}
						old2New[oldGroupNumber] = newGroupNumber;
						oldGroupNumber++;
						newGroupNumber++;
					}
					break;
				case ")":
					var group = groups.pop();
					if (group === LOOKAHEAD) { lookaheadDepth--; }
					newGroups[newGroups.length-1].end = i;
					result.push(chr);
					break;
				case "*":
				case "+":
				case "?":
				case "}":
					// Unary operator. If it's being applied to a capturing group, we need to add a new capturing group
					// enclosing the pair
					var op = chr;
					var prev = src[i-1],
					    prevIndex = i-1;
					if (chr === "}") {
						for (var j=i-1; src[j] !== "{" && j >= 0; j--) {}
						prev = src[j-1];
						prevIndex = j-1;
						op = src.substring(j, i+1);
					}
					var lastGroup = newGroups[newGroups.length-1];
					if (prev === ")" && (lastGroup.type === CAPTURING || lastGroup.type === NEW_CAPTURING)) {
						// Shove in the new group's (, increment num/start in from [lastGroup.start .. end]
						result.splice(lastGroup.start, 0, "(");
						result.push(op);
						result.push(")");
						var newGroup = { start: lastGroup.start, end: result.length-1, type: NEW_CAPTURING, num: lastGroup.num };
						for (var k=0; k < newGroups.length; k++) {
							group = newGroups[k];
							if (group.type === CAPTURING || group.type === NEW_CAPTURING) {
								if (group.start >= lastGroup.start && group.end <= prevIndex) {
									group.start += 1;
									group.end += 1;
									group.num = group.num + 1;
									if (group.type === CAPTURING) {
										old2New[group.oldNum] = group.num;
									}
								}
							}
						}
						newGroups.push(newGroup);
						newGroupNumber++;
						break;
					} else {
						// Fallthrough to default
					}
				default:
					if (chr !== "|" && curGroup !== CAPTURING && curGroup !== NEW_CAPTURING) {
						// Not in a capturing group, so make a new one to hold this term.
						// Perf improvement: don't create the new group if we're inside a lookahead, since we don't 
						// care about them (nothing inside a lookahead actually consumes input so we don't need it)
						if (lookaheadDepth === 0) {
							groups.push(NEW_CAPTURING);
							newGroups.push({ start: i, end: -1, type: NEW_CAPTURING, num: newGroupNumber });
							result.push("(");
							consuming[newGroupNumber] = null;
							newGroupNumber++;
						}
					}
					result.push(chr);
					if (chr === "\\") {
						var peek = src[i+1];
						// Eat next so following iteration doesn't think it's a real special character
						result.push(peek);
						i += 1;
					}
					break;
			}
		}
		while (groups.length) {	
			// Close any remaining new capturing groups
			groups.pop();
			result.push(")");
		}
		var newRegex = new RegExp(result.join(""));
		
		// Update backreferences so they refer to the new group numbers. Use backRefOld2NewMap if provided
		var subst = {};
		backRefOld2NewMap = backRefOld2NewMap || old2New;
		for (var prop in backRefOld2NewMap) {
			if (backRefOld2NewMap.hasOwnProperty(prop)) {
				subst[prop] = "\\" + backRefOld2NewMap[prop];
			}
		}
		newRegex = this.getSubstitutedRegex(newRegex, subst, false);
		
		return [newRegex, old2New, consuming];
	},
	
	/** @returns {Boolean} True if the captures object assigns scope to a matching group other than "0". */
	complexCaptures: function(capturesObj) {
		if (!capturesObj) { return false; }
		for (var prop in capturesObj) {
			if (capturesObj.hasOwnProperty(prop)) {
				if (prop !== "0") {
					return true;
				}
			}
		}
		return false;
	}
};

/**
 * @name orion.editor.TextMateStyler
 * @class A styler that knows how to apply a subset of the TextMate grammar format to style a line.
 *
 * <h4>Styling from a grammar:</h4>
 * <p>Each scope name given in the grammar is converted to an array of CSS class names. For example 
 * a region of text with scope <code>keyword.control.php</code> will be assigned the CSS classes<br />
 * <code>keyword, keyword-control, keyword-control-php</code></p>
 *
 * <p>A CSS file can give rules matching any of these class names to provide generic or more specific styling.
 * For example,</p>
 * <p><code>.keyword { font-color: blue; }</code></p>
 * <p>colors all keywords blue, while</p>
 * <p><code>.keyword-control-php { font-weight: bold; }</code></p>
 * <p>bolds only PHP control keywords.</p>
 *
 * <p>This is useful when using grammars that adhere to TextMate's
 * <a href="http://manual.macromates.com/en/language_grammars.html#naming_conventions">scope name conventions</a>,
 * as a single CSS rule can provide consistent styling to similar constructs across different languages.</p>
 * 
 * <h4>Top-level grammar constructs:</h4>
 * <ul><li><code>patterns, repository</code> (with limitations, see "Other Features") are supported.</li>
 * <li><code>scopeName, firstLineMatch, foldingStartMarker, foldingStopMarker</code> are <b>not</b> supported.</li>
 * <li><code>fileTypes</code> is <b>not</b> supported. When using the Orion service registry, the "orion.edit.highlighter"
 * service serves a similar purpose.</li>
 * </ul>
 *
 * <h4>Regular expression constructs:</h4>
 * <ul>
 * <li><code>match</code> patterns are supported.</li>
 * <li><code>begin .. end</code> patterns are supported.</li>
 * <li>The "extended" regex forms <code>(?x)</code> and <code>(?x:...)</code> are supported, but <b>only</b> when they 
 * apply to the entire regex pattern.</li>
 * <li>Matching is done using native JavaScript <code>RegExp</code>s. As a result, many features of the Oniguruma regex
 * engine used by TextMate are <b>not</b> supported.
 * Unsupported features include:
 *   <ul><li>Named captures</li>
 *   <li>Setting flags inside subgroups (eg. <code>(?i:a)b</code>)</li>
 *   <li>Lookbehind and negative lookbehind</li>
 *   <li>Subexpression call</li>
 *   <li>etc.</li>
 *   </ul>
 * </li>
 * </ul>
 * 
 * <h4>Scope-assignment constructs:</h4>
 * <ul>
 * <li><code>captures, beginCaptures, endCaptures</code> are supported.</li>
 * <li><code>name</code> and <code>contentName</code> are supported.</li>
 * </ul>
 * 
 * <h4>Other features:</h4>
 * <ul>
 * <li><code>applyEndPatternLast</code> is supported.</li>
 * <li><code>include</code> is supported, but only when it references a rule in the current grammar's <code>repository</code>.
 * Including <code>$self</code>, <code>$base</code>, or <code>rule.from.another.grammar</code> is <b>not</b> supported.</li>
 * </ul>
 * 
 * @description Creates a new TextMateStyler.
 * @extends orion.editor.AbstractStyler
 * @param {orion.textview.TextView} textView The <code>TextView</code> to provide styling for.
 * @param {Object} grammar The TextMate grammar to use for styling the <code>TextView</code>, as a JavaScript object. You can
 * produce this object by running a PList-to-JavaScript conversion tool on a TextMate <code>.tmLanguage</code> file.
 * @param {Object[]} [externalGrammars] Additional grammar objects that will be used to resolve named rule references.
 */
orion.editor.TextMateStyler = (function() {
	/** @inner */
	function TextMateStyler(textView, grammar, externalGrammars) {
		this.initialize(textView);
		// Copy grammar object(s) since we will mutate them
		this.grammar = this.copy(grammar);
		this.externalGrammars = externalGrammars ? this.copy(externalGrammars) : [];
		
		this._styles = {}; /* key: {String} scopeName, value: {String[]} cssClassNames */
		this._tree = null;
		this._allGrammars = {}; /* key: {String} scopeName of grammar, value: {Object} grammar */
		this.preprocess(this.grammar);
	}
	orion.editor.AbstractStyler.extend(TextMateStyler, /** @lends orion.editor.TextMateStyler.prototype */ {
		/** @private */
		copy: function(obj) {
			return JSON.parse(JSON.stringify(obj));
		},
		/** @private */
		preprocess: function(grammar) {
			var stack = [grammar];
			for (; stack.length !== 0; ) {
				var rule = stack.pop();
				if (rule._resolvedRule && rule._typedRule) {
					continue;
				}
//				console.debug("Process " + (rule.include || rule.name));
				
				// Look up include'd rule, create typed *Rule instance
				rule._resolvedRule = this._resolve(rule);
				rule._typedRule = this._createTypedRule(rule);
				
				// Convert the scope names to styles and cache them for later
				this.addStyles(rule.name);
				this.addStyles(rule.contentName);
				this.addStylesForCaptures(rule.captures);
				this.addStylesForCaptures(rule.beginCaptures);
				this.addStylesForCaptures(rule.endCaptures);
				
				if (rule._resolvedRule !== rule) {
					// Add include target
					stack.push(rule._resolvedRule);
				}
				if (rule.patterns) {
					// Add subrules
					for (var i=0; i < rule.patterns.length; i++) {
						stack.push(rule.patterns[i]);
					}
				}
			}
		},
		
		/**
		 * @private
		 * Adds eclipse.Style objects for scope to our _styles cache.
		 * @param {String} scope A scope name, like "constant.character.php".
		 */
		addStyles: function(scope) {
			if (scope && !this._styles[scope]) {
				this._styles[scope] = [];
				var scopeArray = scope.split(".");
				for (var i = 0; i < scopeArray.length; i++) {
					this._styles[scope].push(scopeArray.slice(0, i + 1).join("-"));
				}
			}
		},
		/** @private */
		addStylesForCaptures: function(/**Object*/ captures) {
			for (var prop in captures) {
				if (captures.hasOwnProperty(prop)) {
					var scope = captures[prop].name;
					this.addStyles(scope);
				}
			}
		},
		/**
		 * A rule that contains subrules ("patterns" in TextMate parlance) but has no "begin" or "end".
		 * Also handles top level of grammar.
		 * @private
		 */
		ContainerRule: (function() {
			function ContainerRule(/**Object*/ rule) {
				this.rule = rule;
				this.subrules = rule.patterns;
			}
			ContainerRule.prototype.valueOf = function() { return "aa"; };
			return ContainerRule;
		}()),
		/**
		 * A rule that is delimited by "begin" and "end" matches, which may be separated by any number of
		 * lines. This type of rule may contain subrules, which apply only inside the begin .. end region.
		 * @private
		 */
		BeginEndRule: (function() {
			function BeginEndRule(/**Object*/ rule) {
				this.rule = rule;
				// TODO: the TextMate blog claims that "end" is optional.
				this.beginRegex = orion.editor.RegexUtil.toRegExp(rule.begin);
				this.endRegex = orion.editor.RegexUtil.toRegExp(rule.end);
				this.subrules = rule.patterns || [];
				
				this.endRegexHasBackRef = orion.editor.RegexUtil.hasBackReference(this.endRegex);
				
				// Deal with non-0 captures
				var complexCaptures = orion.editor.RegexUtil.complexCaptures(rule.captures);
				var complexBeginEnd = orion.editor.RegexUtil.complexCaptures(rule.beginCaptures) || orion.editor.RegexUtil.complexCaptures(rule.endCaptures);
				this.isComplex = complexCaptures || complexBeginEnd;
				if (this.isComplex) {
					var bg = orion.editor.RegexUtil.groupify(this.beginRegex);
					this.beginRegex = bg[0];
					this.beginOld2New = bg[1];
					this.beginConsuming = bg[2];
					
					var eg = orion.editor.RegexUtil.groupify(this.endRegex, this.beginOld2New /*Update end's backrefs to begin's new group #s*/);
					this.endRegex = eg[0];
					this.endOld2New = eg[1];
					this.endConsuming = eg[2];
				}
			}
			BeginEndRule.prototype.valueOf = function() { return this.beginRegex; };
			return BeginEndRule;
		}()),
		/**
		 * A rule with a "match" pattern.
		 * @private
		 */
		MatchRule: (function() {
			function MatchRule(/**Object*/ rule) {
				this.rule = rule;
				this.matchRegex = orion.editor.RegexUtil.toRegExp(rule.match);
				this.isComplex = orion.editor.RegexUtil.complexCaptures(rule.captures);
				if (this.isComplex) {
					var mg = orion.editor.RegexUtil.groupify(this.matchRegex);
					this.matchRegex = mg[0];
					this.matchOld2New = mg[1];
					this.matchConsuming = mg[2];
				}
			}
			MatchRule.prototype.valueOf = function() { return this.matchRegex; };
			return MatchRule;
		}()),
		/**
		 * @param {Object} rule A rule from the grammar.
		 * @returns {MatchRule|BeginEndRule|ContainerRule}
		 * @private
		 */
		_createTypedRule: function(rule) {
			if (rule.match) {
				return new this.MatchRule(rule);
			} else if (rule.begin) {
				return new this.BeginEndRule(rule);
			} else {
				return new this.ContainerRule(rule);
			}
		},
		/**
		 * Resolves a rule from the grammar (which may be an include) into the real rule that it points to.
		 * @private
		 */
		_resolve: function(rule) {
			var resolved = rule;
			if (rule.include) {
				if (rule.begin || rule.end || rule.match) {
					throw new Error("Unexpected regex pattern in \"include\" rule " + rule.include);
				}
				var name = rule.include;
				if (name[0] === "#") {
					resolved = this.grammar.repository && this.grammar.repository[name.substring(1)];
					if (!resolved) { throw new Error("Couldn't find included rule " + name + " in grammar repository"); }
				} else if (name === "$self") {
					resolved = this.grammar;
				} else if (name === "$base") {
					// $base is only relevant when including rules from foreign grammars
					throw new Error("Include \"$base\" is not supported"); 
				} else {
					resolved = this._allGrammars[name];
					if (!resolved) {
						for (var i=0; i < this.externalGrammars.length; i++) {
							var grammar = this.externalGrammars[i];
							if (grammar.scopeName === name) {
								this.preprocess(grammar);
								this._allGrammars[name] = grammar;
								resolved = grammar;
								break;
							}
						}
					}
				}
			}
			return resolved;
		},
		/** @private */
		ContainerNode: (function() {
			function ContainerNode(parent, rule) {
				this.parent = parent;
				this.rule = rule;
				this.children = [];
				
				this.start = null;
				this.end = null;
			}
			ContainerNode.prototype.addChild = function(child) {
				this.children.push(child);
			};
			ContainerNode.prototype.valueOf = function() {
				var r = this.rule;
				return "ContainerNode { " + (r.include || "") + " " + (r.name || "") + (r.comment || "") + "}";
			};
			return ContainerNode;
		}()),
		/** @private */
		BeginEndNode: (function() {
			function BeginEndNode(parent, rule, beginMatch) {
				this.parent = parent;
				this.rule = rule;
				this.children = [];
				
				this.setStart(beginMatch);
				this.end = null; // will be set eventually during parsing (may be EOF)
				this.endMatch = null; // may remain null if we never match our "end" pattern
				
				// Build a new regex if the "end" regex has backrefs since they refer to matched groups of beginMatch
				if (rule.endRegexHasBackRef) {
					this.endRegexSubstituted = orion.editor.RegexUtil.getSubstitutedRegex(rule.endRegex, beginMatch);
				} else {
					this.endRegexSubstituted = null;
				}
			}
			BeginEndNode.prototype.addChild = function(child) {
				this.children.push(child);
			};
			/** @return {Number} This node's index in its parent's "children" list */
			BeginEndNode.prototype.getIndexInParent = function(node) {
				return this.parent ? this.parent.children.indexOf(this) : -1;
			};
			/** @param {RegExp.match} beginMatch */
			BeginEndNode.prototype.setStart = function(beginMatch) {
				this.start = beginMatch.index;
				this.beginMatch = beginMatch;
			};
			/** @param {RegExp.match|Number} endMatchOrLastChar */
			BeginEndNode.prototype.setEnd = function(endMatchOrLastChar) {
				if (endMatchOrLastChar && typeof(endMatchOrLastChar) === "object") {
					var endMatch = endMatchOrLastChar;
					this.endMatch = endMatch;
					this.end = endMatch.index + endMatch[0].length;
				} else {
					var lastChar = endMatchOrLastChar;
					this.endMatch = null;
					this.end = lastChar;
				}
			};
			BeginEndNode.prototype.shiftStart = function(amount) {
				this.start += amount;
				this.beginMatch.index += amount;
			};
			BeginEndNode.prototype.shiftEnd = function(amount) {
				this.end += amount;
				if (this.endMatch) { this.endMatch.index += amount; }
			};
			BeginEndNode.prototype.valueOf = function() {
				return "{" + this.rule.beginRegex + " range=" + this.start + ".." + this.end + "}";
			};
			return BeginEndNode;
		}()),
		/** Pushes rules onto stack such that rules[startFrom] is on top
		 * @private
		 */
		push: function(/**Array*/ stack, /**Array*/ rules) {
			if (!rules) { return; }
			for (var i = rules.length; i > 0; ) {
				stack.push(rules[--i]);
			}
		},
		/** Executes <code>regex</code> on <code>text</code>, and returns the match object with its index 
		 * offset by the given amount.
		 * @returns {RegExp.match}
		 * @private
		 */
		exec: function(/**RegExp*/ regex, /**String*/ text, /**Number*/ offset) {
			var match = regex.exec(text);
			if (match) { match.index += offset; }
			regex.lastIndex = 0; // Just in case
			return match;
		},
		/** @returns {Number} The position immediately following the match.
		 * @private
		 */
		afterMatch: function(/**RegExp.match*/ match) {
			return match.index + match[0].length;
		},
		/**
		 * @returns {RegExp.match} If node is a BeginEndNode and its rule's "end" pattern matches the text.
		 * @private
		 */
		getEndMatch: function(/**Node*/ node, /**String*/ text, /**Number*/ offset) {
			if (node instanceof this.BeginEndNode) {
				var rule = node.rule;
				var endRegex = node.endRegexSubstituted || rule.endRegex;
				if (!endRegex) { return null; }
				return this.exec(endRegex, text, offset);
			}
			return null;
		},
		/** Called once when file is first loaded to build the parse tree. Tree is updated incrementally thereafter 
		 * as buffer is modified.
		 * @private
		 */
		initialParse: function() {
			var last = this.textView.getModel().getCharCount();
			// First time; make parse tree for whole buffer
			var root = new this.ContainerNode(null, this.grammar._typedRule);
			this._tree = root;
			this.parse(this._tree, false, 0);
		},
		_onModelChanged: function(/**eclipse.ModelChangedEvent*/ e) {
			var addedCharCount = e.addedCharCount,
			    addedLineCount = e.addedLineCount,
			    removedCharCount = e.removedCharCount,
			    removedLineCount = e.removedLineCount,
			    start = e.start;
			if (!this._tree) {
				this.initialParse();
			} else {
				var model = this.textView.getModel();
				var charCount = model.getCharCount();
				
				// For rs, we must rewind to the line preceding the line 'start' is on. We can't rely on start's
				// line since it may've been changed in a way that would cause a new beginMatch at its lineStart.
				var rs = model.getLineEnd(model.getLineAtOffset(start) - 1); // may be < 0
				var fd = this.getFirstDamaged(rs, rs);
				rs = rs === -1 ? 0 : rs;
				var stoppedAt;
				if (fd) {
					// [rs, re] is the region we need to verify. If we find the structure of the tree
					// has changed in that area, then we may need to reparse the rest of the file.
					stoppedAt = this.parse(fd, true, rs, start, addedCharCount, removedCharCount);
				} else {
					// FIXME: fd == null ?
					stoppedAt = charCount;
				}
				this.textView.redrawRange(rs, stoppedAt);
			}
		},
		/** @returns {BeginEndNode|ContainerNode} The result of taking the first (smallest "start" value) 
		 * node overlapping [start,end] and drilling down to get its deepest damaged descendant (if any).
		 * @private
		 */
		getFirstDamaged: function(start, end) {
			// If start === 0 we actually have to start from the root because there is no position
			// we can rely on. (First index is damaged)
			if (start < 0) {
				return this._tree;
			}
			
			var nodes = [this._tree];
			var result = null;
			while (nodes.length) {
				var n = nodes.pop();
				if (!n.parent /*n is root*/ || this.isDamaged(n, start, end)) {
					// n is damaged by the edit, so go into its children
					// Note: If a node is damaged, then some of its descendents MAY be damaged
					// If a node is undamaged, then ALL of its descendents are undamaged
					if (n instanceof this.BeginEndNode) {
						result = n;
					}
					// Examine children[0] last
					for (var i=0; i < n.children.length; i++) {
						nodes.push(n.children[i]);
					}
				}
			}
			return result || this._tree;
		},
		/** @returns true If <code>n</code> overlaps the interval [start,end].
		 * @private
		 */
		isDamaged: function(/**BeginEndNode*/ n, start, end) {
			// Note strict > since [2,5] doesn't overlap [5,7]
			return (n.start <= end && n.end > start);
		},
		/**
		 * Builds tree from some of the buffer content
		 *
		 * TODO cleanup params
		 * @param {BeginEndNode|ContainerNode} origNode The deepest node that overlaps [rs,rs], or the root.
		 * @param {Boolean} repairing 
		 * @param {Number} rs See _onModelChanged()
		 * @param {Number} [editStart] Only used for repairing === true
		 * @param {Number} [addedCharCount] Only used for repairing === true
		 * @param {Number} [removedCharCount] Only used for repairing === true
		 * @returns {Number} The end position that redrawRange should be called for.
		 * @private
		 */
		parse: function(origNode, repairing, rs, editStart, addedCharCount, removedCharCount) {
			var model = this.textView.getModel();
			var lastLineStart = model.getLineStart(model.getLineCount() - 1);
			var eof = model.getCharCount();
			var initialExpected = this.getInitialExpected(origNode, rs);
			
			// re is best-case stopping point; if we detect change to tree, we must continue past it
			var re = -1;
			if (repairing) {
				origNode.repaired = true;
				origNode.endNeedsUpdate = true;
				var lastChild = origNode.children[origNode.children.length-1];
				var delta = addedCharCount - removedCharCount;
				var lastChildLineEnd = lastChild ? model.getLineEnd(model.getLineAtOffset(lastChild.end + delta)) : -1;
				var editLineEnd = model.getLineEnd(model.getLineAtOffset(editStart + removedCharCount));
				re = Math.max(lastChildLineEnd, editLineEnd);
			}
			re = (re === -1) ? eof : re;
			
			var expected = initialExpected;
			var node = origNode;
			var matchedChildOrEnd = false;
			var pos = rs;
			var redrawEnd = -1;
			while (node && (!repairing || (pos < re))) {
				var matchInfo = this.getNextMatch(model, node, pos);
				if (!matchInfo) {
					// Go to next line, if any
					pos = (pos >= lastLineStart) ? eof : model.getLineStart(model.getLineAtOffset(pos) + 1);
				}
				var match = matchInfo && matchInfo.match,
				    rule = matchInfo && matchInfo.rule,
				    isSub = matchInfo && matchInfo.isSub,
				    isEnd = matchInfo && matchInfo.isEnd;
				if (isSub) {
					pos = this.afterMatch(match);
					if (rule instanceof this.BeginEndRule) {
						matchedChildOrEnd = true;
						// Matched a child. Did we expect that?
						if (repairing && rule === expected.rule && node === expected.parent) {
							// Yes: matched expected child
							var foundChild = expected;
							foundChild.setStart(match);
							// Note: the 'end' position for this node will either be matched, or fixed up by us post-loop
							foundChild.repaired = true;
							foundChild.endNeedsUpdate = true;
							node = foundChild; // descend
							expected = this.getNextExpected(expected, "begin");
						} else {
							if (repairing) {
								// No: matched unexpected child.
								this.prune(node, expected);
								repairing = false;
							}
							
							// Add the new child (will replace 'expected' in node's children list)
							var subNode = new this.BeginEndNode(node, rule, match);
							node.addChild(subNode);
							node = subNode; // descend
						}
					} else {
						// Matched a MatchRule; no changes to tree required
					}
				} else if (isEnd || pos === eof) {
					if (node instanceof this.BeginEndNode) {
						if (match) {
							matchedChildOrEnd = true;
							redrawEnd = Math.max(redrawEnd, node.end); // if end moved up, must still redraw to its old value
							node.setEnd(match);
							pos = this.afterMatch(match);
							// Matched node's end. Did we expect that?
							if (repairing && node === expected && node.parent === expected.parent) {
								// Yes: found the expected end of node
								node.repaired = true;
								delete node.endNeedsUpdate;
								expected = this.getNextExpected(expected, "end");
							} else {
								if (repairing) {
									// No: found an unexpected end
									this.prune(node, expected);
									repairing = false;
								}
							}
						} else {
							// Force-ending a BeginEndNode that runs until eof
							node.setEnd(eof);
							delete node.endNeedsUpdate;
						}
					}
					node = node.parent; // ascend
				}
				
				if (repairing && pos >= re && !matchedChildOrEnd) {
					// Reached re without matching any begin/end => initialExpected itself was removed => repair fail
					this.prune(origNode, initialExpected);
					repairing = false;
				}
			} // end loop
			// TODO: do this for every node we end?
			this.removeUnrepairedChildren(origNode, repairing, rs);
			
			//console.debug("parsed " + (pos - rs) + " of " + model.getCharCount + "buf");
			this.cleanup(repairing, origNode, rs, re, eof, addedCharCount, removedCharCount);
			if (repairing) {
				return Math.max(redrawEnd, pos);
			} else {
				return pos; // where we stopped reparsing
			}
		},
		/** Helper for parse() in the repair case. To be called when ending a node, as any children that
		 * lie in [rs,node.end] and were not repaired must've been deleted.
		 * @private
		 */
		removeUnrepairedChildren: function(node, repairing, start) {
			if (repairing) {
				var children = node.children;
				var removeFrom = -1;
				for (var i=0; i < children.length; i++) {
					var child = children[i];
					if (!child.repaired && this.isDamaged(child, start, Number.MAX_VALUE /*end doesn't matter*/)) {
						removeFrom = i;
						break;
					}
				}
				if (removeFrom !== -1) {
					node.children.length = removeFrom;
				}
			}
		},
		/** Helper for parse() in the repair case
		 * @private
		 */
		cleanup: function(repairing, origNode, rs, re, eof, addedCharCount, removedCharCount) {
			var i, node, maybeRepairedNodes;
			if (repairing) {
				// The repair succeeded, so update stale begin/end indices by simple translation.
				var delta = addedCharCount - removedCharCount;
				// A repaired node's end can't exceed re, but it may exceed re-delta+1.
				// TODO: find a way to guarantee disjoint intervals for repaired vs unrepaired, then stop using flag
				var maybeUnrepairedNodes = this.getIntersecting(re-delta+1, eof);
				maybeRepairedNodes = this.getIntersecting(rs, re);
				// Handle unrepaired nodes. They are those intersecting [re-delta+1, eof] that don't have the flag
				for (i=0; i < maybeUnrepairedNodes.length; i++) {
					node = maybeUnrepairedNodes[i];
					if (!node.repaired && node instanceof this.BeginEndNode) {
						node.shiftEnd(delta);
						node.shiftStart(delta);
					}
				}
				// Translate 'end' index of repaired node whose 'end' was not matched in loop (>= re)
				for (i=0; i < maybeRepairedNodes.length; i++) {
					node = maybeRepairedNodes[i];
					if (node.repaired && node.endNeedsUpdate) {
						node.shiftEnd(delta);
					}
					delete node.endNeedsUpdate;
					delete node.repaired;
				}
			} else {
				// Clean up after ourself
				maybeRepairedNodes = this.getIntersecting(rs, re);
				for (i=0; i < maybeRepairedNodes.length; i++) {
					delete maybeRepairedNodes[i].repaired;
				}
			}
		},
		/**
		 * @param model {orion.textview.TextModel}
		 * @param node {Node}
		 * @param pos {Number}
		 * @param [matchRulesOnly] {Boolean} Optional, if true only "match" subrules will be considered.
		 * @returns {Object} A match info object with properties:
		 * {Boolean} isEnd
		 * {Boolean} isSub
		 * {RegExp.match} match
		 * {(Match|BeginEnd)Rule} rule
		 * @private
		 */
		getNextMatch: function(model, node, pos, matchRulesOnly) {
			var lineIndex = model.getLineAtOffset(pos);
			var lineEnd = model.getLineEnd(lineIndex);
			var line = model.getText(pos, lineEnd);

			var stack = [],
			    expandedContainers = [],
			    subMatches = [],
			    subrules = [];
			this.push(stack, node.rule.subrules);
			while (stack.length) {
				var next = stack.length ? stack.pop() : null;
				var subrule = next && next._resolvedRule._typedRule;
				if (subrule instanceof this.ContainerRule && expandedContainers.indexOf(subrule) === -1) {
					// Expand ContainerRule by pushing its subrules on
					expandedContainers.push(subrule);
					this.push(stack, subrule.subrules);
					continue;
				}
				if (subrule && matchRulesOnly && !(subrule.matchRegex)) {
					continue;
				}
				var subMatch = subrule && this.exec(subrule.matchRegex || subrule.beginRegex, line, pos);
				if (subMatch) {
					subMatches.push(subMatch);
					subrules.push(subrule);
				}
			}

			var bestSub = Number.MAX_VALUE,
			    bestSubIndex = -1;
			for (var i=0; i < subMatches.length; i++) {
				var match = subMatches[i];
				if (match.index < bestSub) {
					bestSub = match.index;
					bestSubIndex = i;
				}
			}
			
			if (!matchRulesOnly) {
				// See if the "end" pattern of the active begin/end node matches.
				// TODO: The active begin/end node may not be the same as the node that holds the subrules
				var activeBENode = node;
				var endMatch = this.getEndMatch(node, line, pos);
				if (endMatch) {
					var doEndLast = activeBENode.rule.applyEndPatternLast;
					var endWins = bestSubIndex === -1 || (endMatch.index < bestSub) || (!doEndLast && endMatch.index === bestSub);
					if (endWins) {
						return {isEnd: true, rule: activeBENode.rule, match: endMatch};
					}
				}
			}
			return bestSubIndex === -1 ? null : {isSub: true, rule: subrules[bestSubIndex], match: subMatches[bestSubIndex]};
		},
		/**
		 * Gets the node corresponding to the first match we expect to see in the repair.
		 * @param {BeginEndNode|ContainerNode} node The node returned via getFirstDamaged(rs,rs) -- may be the root.
		 * @param {Number} rs See _onModelChanged()
		 * Note that because rs is a line end (or 0, a line start), it will intersect a beginMatch or 
		 * endMatch either at their 0th character, or not at all. (begin/endMatches can't cross lines).
		 * This is the only time we rely on the start/end values from the pre-change tree. After this 
		 * we only look at node ordering, never use the old indices.
		 * @returns {Node}
		 * @private
		 */
		getInitialExpected: function(node, rs) {
			// TODO: Kind of weird.. maybe ContainerNodes should have start & end set, like BeginEndNodes
			var i, child;
			if (node === this._tree) {
				// get whichever of our children comes after rs
				for (i=0; i < node.children.length; i++) {
					child = node.children[i]; // BeginEndNode
					if (child.start >= rs) {
						return child;
					}
				}
			} else if (node instanceof this.BeginEndNode) {
				if (node.endMatch) {
					// Which comes next after rs: our nodeEnd or one of our children?
					var nodeEnd = node.endMatch.index;
					for (i=0; i < node.children.length; i++) {
						child = node.children[i]; // BeginEndNode
						if (child.start >= rs) {
							break;
						}
					}
					if (child && child.start < nodeEnd) {
						return child; // Expect child as the next match
					}
				} else {
					// No endMatch => node goes until eof => it end should be the next match
				}
			}
			return node; // We expect node to end, so it should be the next match
		},
		/**
		 * Helper for repair() to tell us what kind of event we expect next.
		 * @param {Node} expected Last value returned by this method.
		 * @param {String} event "begin" if the last value of expected was matched as "begin",
		 *  or "end" if it was matched as an end.
		 * @returns {Node} The next expected node to match, or null.
		 * @private
		 */
		getNextExpected: function(/**Node*/ expected, event) {
			var node = expected;
			if (event === "begin") {
				var child = node.children[0];
				if (child) {
					return child;
				} else {
					return node;
				}
			} else if (event === "end") {
				var parent = node.parent;
				if (parent) {
					var nextSibling = parent.children[parent.children.indexOf(node) + 1];
					if (nextSibling) {
						return nextSibling;
					} else {
						return parent;
					}
				}
			}
			return null;
		},
		/** Helper for parse() when repairing. Prunes out the unmatched nodes from the tree so we can continue parsing.
		 * @private
		 */
		prune: function(/**BeginEndNode|ContainerNode*/ node, /**Node*/ expected) {
			var expectedAChild = expected.parent === node;
			if (expectedAChild) {
				// Expected child wasn't matched; prune it and all siblings after it
				node.children.length = expected.getIndexInParent();
			} else if (node instanceof this.BeginEndNode) {
				// Expected node to end but it didn't; set its end unknown and we'll match it eventually
				node.endMatch = null;
				node.end = null;
			}
			// Reparsing from node, so prune the successors outside of node's subtree
			if (node.parent) {
				node.parent.children.length = node.getIndexInParent() + 1;
			}
		},
		_onLineStyle: function(/**eclipse.LineStyleEvent*/ e) {
			function byStart(r1, r2) {
				return r1.start - r2.start;
			}
			
			if (!this._tree) {
				// In some cases it seems onLineStyle is called before onModelChanged, so we need to parse here
				this.initialParse();
			}
			var lineStart = e.lineStart,
			    model = this.textView.getModel(),
			    lineEnd = model.getLineEnd(e.lineIndex);
			
			var rs = model.getLineEnd(model.getLineAtOffset(lineStart) - 1); // may be < 0
			var node = this.getFirstDamaged(rs, rs);
			
			var scopes = this.getLineScope(model, node, lineStart, lineEnd);
			e.ranges = this.toStyleRanges(scopes);
//			// Editor requires StyleRanges must be in ascending order by 'start', or else some will be ignored
			e.ranges.sort(byStart);
		},
		/** Runs parse algorithm on [start, end] in the context of node, assigning scope as we find matches.
		 * @private
		 */
		getLineScope: function(model, node, start, end) {
			var pos = start;
			var expected = this.getInitialExpected(node, start);
			var scopes = [],
			    gaps = [];
			while (node && (pos < end)) {
				var matchInfo = this.getNextMatch(model, node, pos);
				if (!matchInfo) { 
					break; // line is over
				}
				var match = matchInfo && matchInfo.match,
				    rule = matchInfo && matchInfo.rule,
				    isSub = matchInfo && matchInfo.isSub,
				    isEnd = matchInfo && matchInfo.isEnd;
				if (match.index !== pos) {
					// gap [pos..match.index]
					gaps.push({ start: pos, end: match.index, node: node});
				}
				if (isSub) {
					pos = this.afterMatch(match);
					if (rule instanceof this.BeginEndRule) {
						// Matched a "begin", assign its scope and descend into it
						this.addBeginScope(scopes, match, rule);
						node = expected; // descend
						expected = this.getNextExpected(expected, "begin");
					} else {
						// Matched a child MatchRule;
						this.addMatchScope(scopes, match, rule);
					}
				} else if (isEnd) {
					pos = this.afterMatch(match);
					// Matched and "end", assign its end scope and go up
					this.addEndScope(scopes, match, rule);
					expected = this.getNextExpected(expected, "end");
					node = node.parent; // ascend
				}
			}
			if (pos < end) {
				gaps.push({ start: pos, end: end, node: node });
			}
			var inherited = this.getInheritedLineScope(gaps, start, end);
			return scopes.concat(inherited);
		},
		/** @private */
		getInheritedLineScope: function(gaps, start, end) {
			var scopes = [];
			for (var i=0; i < gaps.length; i++) {
				var gap = gaps[i];
				var node = gap.node;
				while (node) {
					// if node defines a contentName or name, apply it
					var rule = node.rule.rule;
					var name = rule.name,
					    contentName = rule.contentName;
					// TODO: if both are given, we don't resolve the conflict. contentName always wins
					var scope = contentName || name;
					if (scope) {
						this.addScopeRange(scopes, gap.start, gap.end, scope);
						break;
					}
					node = node.parent;
				}
			}
			return scopes;
		},
		/** @private */
		addBeginScope: function(scopes, match, typedRule) {
			var rule = typedRule.rule;
			this.addCapturesScope(scopes, match, (rule.beginCaptures || rule.captures), typedRule.isComplex, typedRule.beginOld2New, typedRule.beginConsuming);
		},
		/** @private */
		addEndScope: function(scopes, match, typedRule) {
			var rule = typedRule.rule;
			this.addCapturesScope(scopes, match, (rule.endCaptures || rule.captures), typedRule.isComplex, typedRule.endOld2New, typedRule.endConsuming);
		},
		/** @private */
		addMatchScope: function(scopes, match, typedRule) {
			var rule = typedRule.rule,
			    name = rule.name,
			    captures = rule.captures;
			if (captures) {	
				// captures takes priority over name
				this.addCapturesScope(scopes, match, captures, typedRule.isComplex, typedRule.matchOld2New, typedRule.matchConsuming);
			} else {
				this.addScope(scopes, match, name);
			}
		},
		/** @private */
		addScope: function(scopes, match, name) {
			if (!name) { return; }
			scopes.push({start: match.index, end: this.afterMatch(match), scope: name });
		},
		/** @private */
		addScopeRange: function(scopes, start, end, name) {
			if (!name) { return; }
			scopes.push({start: start, end: end, scope: name });
		},
		/** @private */
		addCapturesScope: function(/**Array*/scopes, /*RegExp.match*/ match, /**Object*/captures, /**Boolean*/isComplex, /**Object*/old2New, /**Object*/consuming) {
			if (!captures) { return; }
			if (!isComplex) {
				this.addScope(scopes, match, captures[0] && captures[0].name);
			} else {
				// apply scopes captures[1..n] to matching groups [1]..[n] of match
				
				// Sum up the lengths of preceding consuming groups to get the start offset for each matched group.
				var newGroupStarts = {1: 0};
				var sum = 0;
				for (var num = 1; match[num] !== undefined; num++) {
					if (consuming[num] !== undefined) {
						sum += match[num].length;
					}
					if (match[num+1] !== undefined) {
						newGroupStarts[num + 1] = sum;
					}
				}
				// Map the group numbers referred to in captures object to the new group numbers, and get the actual matched range.
				var start = match.index;
				for (var oldGroupNum = 1; captures[oldGroupNum]; oldGroupNum++) {
					var scope = captures[oldGroupNum].name;
					var newGroupNum = old2New[oldGroupNum];
					var groupStart = start + newGroupStarts[newGroupNum];
					// Not every capturing group defined in regex need match every time the regex is run.
					// eg. (a)|b matches "b" but group 1 is undefined
					if (typeof match[newGroupNum] !== "undefined") {
						var groupEnd = groupStart + match[newGroupNum].length;
						this.addScopeRange(scopes, groupStart, groupEnd, scope);
					}
				}
			}
		},
		/** @returns {Node[]} In depth-first order
		 * @private
		 */
		getIntersecting: function(start, end) {
			var result = [];
			var nodes = this._tree ? [this._tree] : [];
			while (nodes.length) {
				var n = nodes.pop();
				var visitChildren = false;
				if (n instanceof this.ContainerNode) {
					visitChildren = true;
				} else if (this.isDamaged(n, start, end)) {
					visitChildren = true;
					result.push(n);
				}
				if (visitChildren) {
					var len = n.children.length;
//					for (var i=len-1; i >= 0; i--) {
//						nodes.push(n.children[i]);
//					}
					for (var i=0; i < len; i++) {
						nodes.push(n.children[i]);
					}
				}
			}
			return result.reverse();
		},
		_onSelection: function(e) {
		},
		_onDestroy: function(/**eclipse.DestroyEvent*/ e) {
			this.grammar = null;
			this._styles = null;
			this._tree = null;
		},
		/**
		 * Applies the grammar to obtain the {@link eclipse.StyleRange[]} for the given line.
		 * @returns eclipse.StyleRange[]
		 * @private
		 */
		toStyleRanges: function(/**ScopeRange[]*/ scopeRanges) {
			var styleRanges = [];
			for (var i=0; i < scopeRanges.length; i++) {
				var scopeRange = scopeRanges[i];
				var classNames = this._styles[scopeRange.scope];
				if (!classNames) { throw new Error("styles not found for " + scopeRange.scope); }
				var classNamesString = classNames.join(" ");
				styleRanges.push({start: scopeRange.start, end: scopeRange.end, style: {styleClass: classNamesString}});
//				console.debug("{start " + styleRanges[i].start + ", end " + styleRanges[i].end + ", style: " + styleRanges[i].style.styleClass + "}");
			}
			return styleRanges;
		}
	});
	return TextMateStyler;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return orion.editor;
	});
}

/*******************************************************************************
 * Copyright (c) 2010, 2011 IBM Corporation and others.
 * All rights reserved. This program and the accompanying materials are made 
 * available under the terms of the Eclipse Public License v1.0 
 * (http://www.eclipse.org/legal/epl-v10.html), and the Eclipse Distribution 
 * License v1.0 (http://www.eclipse.org/org/documents/edl-v10.html). 
 * 
 * Contributors: IBM Corporation - initial API and implementation
 ******************************************************************************/

/*global document window navigator define orion */

var examples = examples || {};
examples.textview = examples.textview || {};

examples.textview.TextStyler = (function() {

	var JS_KEYWORDS =
		["break",
		 "case", "class", "catch", "continue", "const", 
		 "debugger", "default", "delete", "do",
		 "else", "enum", "export", "extends",  
		 "false", "finally", "for", "function",
		 "if", "implements", "import", "in", "instanceof", "interface", 
		 "let",
		 "new", "null",
		 "package", "private", "protected", "public",
		 "return", 
		 "static", "super", "switch",
		 "this", "throw", "true", "try", "typeof",
		 "undefined",
		 "var", "void",
		 "while", "with",
		 "yield"];

	var JAVA_KEYWORDS =
		["abstract",
		 "boolean", "break", "byte",
		 "case", "catch", "char", "class", "continue",
		 "default", "do", "double",
		 "else", "extends",
		 "false", "final", "finally", "float", "for",
		 "if", "implements", "import", "instanceof", "int", "interface",
		 "long",
		 "native", "new", "null",
		 "package", "private", "protected", "public",
		 "return",
		 "short", "static", "super", "switch", "synchronized",
		 "this", "throw", "throws", "transient", "true", "try",
		 "void", "volatile",
		 "while"];

	var CSS_KEYWORDS =
		["color", "text-align", "text-indent", "text-decoration", 
		 "font", "font-style", "font-family", "font-weight", "font-size", "font-variant", "line-height",
		 "background", "background-color", "background-image", "background-position", "background-repeat", "background-attachment",
		 "list-style", "list-style-image", "list-style-position", "list-style-type", 
		 "outline", "outline-color", "outline-style", "outline-width",
		 "border", "border-left", "border-top", "border-bottom", "border-right", "border-color", "border-width", "border-style",
		 "border-bottom-color", "border-bottom-style", "border-bottom-width",
		 "border-left-color", "border-left-style", "border-left-width",
		 "border-top-color", "border-top-style", "border-top-width",
		 "border-right-color", "border-right-style", "border-right-width",
		 "padding", "padding-left", "padding-top", "padding-bottom", "padding-right",
		 "margin", "margin-left", "margin-top", "margin-bottom", "margin-right",
		 "width", "height", "left", "top", "right", "bottom",
		 "min-width", "max-width", "min-height", "max-height",
		 "display", "visibility",
		 "clip", "cursor", "overflow", "overflow-x", "overflow-y", "position", "z-index",
		 "vertical-align", "horizontal-align",
		 "float", "clear"
		];

	// Scanner constants
	var UNKOWN = 1;
	var KEYWORD = 2;
	var STRING = 3;
	var SINGLELINE_COMMENT = 4;
	var MULTILINE_COMMENT = 5;
	var DOC_COMMENT = 6;
	var WHITE = 7;
	var WHITE_TAB = 8;
	var WHITE_SPACE = 9;
	var HTML_MARKUP = 10;
	var DOC_TAG = 11;
	var TASK_TAG = 12;

	// Styles 
	var isIE = document.selection && window.ActiveXObject && /MSIE/.test(navigator.userAgent) ? document.documentMode : undefined;
	var singleCommentStyle = {styleClass: "token_singleline_comment"};
	var multiCommentStyle = {styleClass: "token_multiline_comment"};
	var docCommentStyle = {styleClass: "token_doc_comment"};
	var htmlMarkupStyle = {styleClass: "token_doc_html_markup"};
	var tasktagStyle = {styleClass: "token_task_tag"};
	var doctagStyle = {styleClass: "token_doc_tag"};
	var stringStyle = {styleClass: "token_string"};
	var keywordStyle = {styleClass: "token_keyword"};
	var spaceStyle = {styleClass: "token_space"};
	var tabStyle = {styleClass: "token_tab"};
	var bracketStyle = {styleClass: isIE < 9 ? "token_bracket" : "token_bracket_outline"};
	var caretLineStyle = {styleClass: "line_caret"};
	
	var Scanner = (function() {
		function Scanner (keywords, whitespacesVisible) {
			this.keywords = keywords;
			this.whitespacesVisible = whitespacesVisible;
			this.setText("");
		}
		
		Scanner.prototype = {
			getOffset: function() {
				return this.offset;
			},
			getStartOffset: function() {
				return this.startOffset;
			},
			getData: function() {
				return this.text.substring(this.startOffset, this.offset);
			},
			getDataLength: function() {
				return this.offset - this.startOffset;
			},
			_default: function(c) {
				var keywords = this.keywords;
				switch (c) {
					case 32: // SPACE
					case 9: // TAB
						if (this.whitespacesVisible) {
							return c === 32 ? WHITE_SPACE : WHITE_TAB;
						}
						do {
							c = this._read();
						} while(c === 32 || c === 9);
						this._unread(c);
						return WHITE;
					case 123: // {
					case 125: // }
					case 40: // (
					case 41: // )
					case 91: // [
					case 93: // ]
					case 60: // <
					case 62: // >
						// BRACKETS
						return c;
					default:
						var isCSS = this.isCSS;
						if ((97 <= c && c <= 122) || (65 <= c && c <= 90) || c === 95 || (48 <= c && c <= 57) || (0x2d === c && isCSS)) { //LETTER OR UNDERSCORE OR NUMBER
							var off = this.offset - 1;
							do {
								c = this._read();
							} while((97 <= c && c <= 122) || (65 <= c && c <= 90) || c === 95 || (48 <= c && c <= 57) || (0x2d === c && isCSS));  //LETTER OR UNDERSCORE OR NUMBER
							this._unread(c);
							if (keywords.length > 0) {
								var word = this.text.substring(off, this.offset);
								//TODO slow
								for (var i=0; i<keywords.length; i++) {
									if (this.keywords[i] === word) { return KEYWORD; }
								}
							}
						}
						return UNKOWN;
				}
			},
			_read: function() {
				if (this.offset < this.text.length) {
					return this.text.charCodeAt(this.offset++);
				}
				return -1;
			},
			_unread: function(c) {
				if (c !== -1) { this.offset--; }
			},
			nextToken: function() {
				this.startOffset = this.offset;
				while (true) {
					var c = this._read();
					switch (c) {
						case -1: return null;
						case 47:	// SLASH -> comment
							c = this._read();
							if (c === 47) { // SLASH -> single line
								while (true) {
									c = this._read();
									if ((c === -1) || (c === 10) || (c === 13)) {
										this._unread(c);
										return SINGLELINE_COMMENT;
									}
								}
							}
							if (c === 42) { // STAR -> multi line 
								c = this._read();
								var token = MULTILINE_COMMENT;
								if (c === 42) {
									token = DOC_COMMENT;
								}
								while (true) {
									while (c === 42) {
										c = this._read();
										if (c === 47) {
											return token;
										}
									}
									if (c === -1) {
										this._unread(c);
										return token;
									}
									c = this._read();
								}
							}
							this._unread(c);
							return UNKOWN;
						case 39:	// SINGLE QUOTE -> char const
							while(true) {
								c = this._read();
								switch (c) {
									case 39:
										return STRING;
									case 13:
									case 10:
									case -1:
										this._unread(c);
										return STRING;
									case 92: // BACKSLASH
										c = this._read();
										break;
								}
							}
							break;
						case 34:	// DOUBLE QUOTE -> string
							while(true) {
								c = this._read();
								switch (c) {
									case 34: // DOUBLE QUOTE
										return STRING;
									case 13:
									case 10:
									case -1:
										this._unread(c);
										return STRING;
									case 92: // BACKSLASH
										c = this._read();
										break;
								}
							}
							break;
						default:
							return this._default(c);
					}
				}
			},
			setText: function(text) {
				this.text = text;
				this.offset = 0;
				this.startOffset = 0;
			}
		};
		return Scanner;
	}());
	
	var WhitespaceScanner = (function() {
		function WhitespaceScanner () {
			Scanner.call(this, null, true);
		}
		WhitespaceScanner.prototype = new Scanner(null);
		WhitespaceScanner.prototype.nextToken = function() {
			this.startOffset = this.offset;
			while (true) {
				var c = this._read();
				switch (c) {
					case -1: return null;
					case 32: // SPACE
						return WHITE_SPACE;
					case 9: // TAB
						return WHITE_TAB;
					default:
						do {
							c = this._read();
						} while(!(c === 32 || c === 9 || c === -1));
						this._unread(c);
						return UNKOWN;
				}
			}
		};
		
		return WhitespaceScanner;
	}());
	
	var CommentScanner = (function() {
		function CommentScanner (whitespacesVisible) {
			Scanner.call(this, null, whitespacesVisible);
		}
		CommentScanner.prototype = new Scanner(null);
		CommentScanner.prototype.setType = function(type) {
			this._type = type;
		};
		CommentScanner.prototype.nextToken = function() {
			this.startOffset = this.offset;
			while (true) {
				var c = this._read();
				switch (c) {
					case -1: return null;
					case 32: // SPACE
					case 9: // TAB
						if (this.whitespacesVisible) {
							return c === 32 ? WHITE_SPACE : WHITE_TAB;
						}
						do {
							c = this._read();
						} while(c === 32 || c === 9);
						this._unread(c);
						return WHITE;
					case 60: // <
						if (this._type === DOC_COMMENT) {
							do {
								c = this._read();
							} while(!(c === 62 || c === -1)); // >
							if (c === 62) {
								return HTML_MARKUP;
							}
						}
						return UNKOWN;
					case 64: // @
						if (this._type === DOC_COMMENT) {
							do {
								c = this._read();
							} while((97 <= c && c <= 122) || (65 <= c && c <= 90) || c === 95 || (48 <= c && c <= 57));  //LETTER OR UNDERSCORE OR NUMBER
							this._unread(c);
							return DOC_TAG;
						}
						return UNKOWN;
					case 84: // T
						if ((c = this._read()) === 79) { // O
							if ((c = this._read()) === 68) { // D
								if ((c = this._read()) === 79) { // O
									c = this._read();
									if (!((97 <= c && c <= 122) || (65 <= c && c <= 90) || c === 95 || (48 <= c && c <= 57))) {
										this._unread(c);
										return TASK_TAG;
									}
									this._unread(c);
								} else {
									this._unread(c);
								}
							} else {
								this._unread(c);
							}
						} else {
							this._unread(c);
						}
						//FALL THROUGH
					default:
						do {
							c = this._read();
						} while(!(c === 32 || c === 9 || c === -1 || c === 60 || c === 64 || c === 84));
						this._unread(c);
						return UNKOWN;
				}
			}
		};
		
		return CommentScanner;
	}());
	
	var FirstScanner = (function() {
		function FirstScanner () {
			Scanner.call(this, null, false);
		}
		FirstScanner.prototype = new Scanner(null);
		FirstScanner.prototype._default = function(c) {
			while(true) {
				c = this._read();
				switch (c) {
					case 47: // SLASH
					case 34: // DOUBLE QUOTE
					case 39: // SINGLE QUOTE
					case -1:
						this._unread(c);
						return UNKOWN;
				}
			}
		};
		
		return FirstScanner;
	}());
	
	function TextStyler (view, lang, annotationModel) {
		this.commentStart = "/*";
		this.commentEnd = "*/";
		var keywords = [];
		switch (lang) {
			case "java": keywords = JAVA_KEYWORDS; break;
			case "js": keywords = JS_KEYWORDS; break;
			case "css": keywords = CSS_KEYWORDS; break;
		}
		this.whitespacesVisible = false;
		this.detectHyperlinks = true;
		this.highlightCaretLine = true;
		this.foldingEnabled = true;
		this.detectTasks = true;
		this._scanner = new Scanner(keywords, this.whitespacesVisible);
		//TODO this scanner is not the best/correct way to parse CSS
		if (lang === "css") {
			this._scanner.isCSS = true;
		}
		this._firstScanner = new FirstScanner();
		this._commentScanner = new CommentScanner(this.whitespacesVisible);
		this._whitespaceScanner = new WhitespaceScanner();
		this.view = view;
		this.annotationModel = annotationModel;
		this._currentBracket = undefined; 
		this._matchingBracket = undefined;
		
		view.addEventListener("Selection", this, this._onSelection);
		var model = view.getModel();
		if (model.getBaseModel) {
			var self = this;
			this._baseModelListener = {
				onChanged: function(modelChangedEvent) {
					self._onModelChanged(modelChangedEvent);
				}
			};
			model.getBaseModel().addListener(this._baseModelListener);
		} else {
			//TODO still needed to keep the event order correct (styler before view)
			view.addEventListener("ModelChanged", this, this._onModelChanged);
		}
		view.addEventListener("Destroy", this, this._onDestroy);
		view.addEventListener("LineStyle", this, this._onLineStyle);
		this._computeComments ();
		this._computeFolding();
		view.redrawLines();
	}
	
	TextStyler.prototype = {
		destroy: function() {
			var view = this.view;
			if (view) {
				var model = view.getModel();
				if (model.getBaseModel) {
					model.getBaseModel().removeListener(this._baseModelListener);
				} else {
					view.removeEventListener("ModelChanged", this, this._onModelChanged);
				}
				view.removeEventListener("Selection", this, this._onSelection);
				view.removeEventListener("Destroy", this, this._onDestroy);
				view.removeEventListener("LineStyle", this, this._onLineStyle);
				this.view = null;
			}
		},
		setHighlightCaretLine: function(highlight) {
			this.highlightCaretLine = highlight;
		},
		setWhitespacesVisible: function(visible) {
			this.whitespacesVisible = visible;
			this._scanner.whitespacesVisible = visible;
			this._commentScanner.whitespacesVisible = visible;
		},
		setDetectHyperlinks: function(enabled) {
			this.detectHyperlinks = enabled;
		},
		setFoldingEnabled: function(enabled) {
			this.foldingEnabled = enabled;
		},
		setDetectTasks: function(enabled) {
			this.detectTasks = enabled;
		},
		_binarySearch: function (array, offset, inclusive, low, high) {
			var index;
			if (low === undefined) { low = -1; }
			if (high === undefined) { high = array.length; }
			while (high - low > 1) {
				index = Math.floor((high + low) / 2);
				if (offset <= array[index].start) {
					high = index;
				} else if (inclusive && offset < array[index].end) {
					high = index;
					break;
				} else {
					low = index;
				}
			}
			return high;
		},
		_computeComments: function() {
			var model = this.view.getModel();
			if (model.getBaseModel) { model = model.getBaseModel(); }
			this.comments = this._findComments(model.getText());
		},
		_computeFolding: function() {
			if (!this.foldingEnabled) { return; }
			var view = this.view;
			var viewModel = view.getModel();
			if (!viewModel.getBaseModel) { return; }
			var annotationModel = this.annotationModel;
			if (!annotationModel) { return; }
			annotationModel.removeAnnotations("orion.annotation.folding");
			var add = [];
			var baseModel = viewModel.getBaseModel();
			var comments = this.comments;
			for (var i=0; i<comments.length; i++) {
				var comment = comments[i];
				var annotation = this._createFoldingAnnotation(viewModel, baseModel, comment.start, comment.end);
				if (annotation) { 
					add.push(annotation);
				}
			}
			annotationModel.replaceAnnotations(null, add);
		},
		_createFoldingAnnotation: function(viewModel, baseModel, start, end) {
			var startLine = baseModel.getLineAtOffset(start);
			var endLine = baseModel.getLineAtOffset(end);
			if (startLine === endLine) {
				return null;
			}
			return new orion.textview.FoldingAnnotation(viewModel, "orion.annotation.folding", start, end,
				"<div class='annotationHTML expanded'></div>", {styleClass: "annotation expanded"}, 
				"<div class='annotationHTML collapsed'></div>", {styleClass: "annotation collapsed"});
		},
		_computeTasks: function(type, commentStart, commentEnd) {
			if (!this.detectTasks) { return; }
			var annotationModel = this.annotationModel;
			if (!annotationModel) { return; }
			var view = this.view;
			var viewModel = view.getModel(), baseModel = viewModel;
			if (viewModel.getBaseModel) { baseModel = viewModel.getBaseModel(); }
			var annotations = annotationModel.getAnnotations(commentStart, commentEnd);
			var remove = [];
			while (annotations.hasNext()) {
				remove.push(annotations.next());
			}
			var add = [];
			var scanner = this._commentScanner;
			scanner.setText(baseModel.getText(commentStart, commentEnd));
			var token;
			while ((token = scanner.nextToken())) {
				var tokenStart = scanner.getStartOffset() + commentStart;
				if (token === TASK_TAG) {
					var end = baseModel.getLineEnd(baseModel.getLineAtOffset(tokenStart));
					if (type !== SINGLELINE_COMMENT) {
						end = Math.min(end, commentEnd - this.commentEnd.length);
					}
					add.push({
						start: tokenStart,
						end: end,
						type: "orion.annotation.task",
						title: baseModel.getText(tokenStart, end),
						style: {styleClass: "annotation task"},
						html: "<div class='annotationHTML task'></div>",
						overviewStyle: {styleClass: "annotationOverview task"}
					});
				}
			}
			annotationModel.replaceAnnotations(remove, add);
		},
		_getLineStyle: function(lineIndex) {
			if (this.highlightCaretLine) {
				var view = this.view;
				var model = view.getModel();
				var selection = view.getSelection();
				if (selection.start === selection.end && model.getLineAtOffset(selection.start) === lineIndex) {
					return caretLineStyle;
				}
			}
			return null;
		},
		_getStyles: function(model, text, start) {
			if (model.getBaseModel) {
				start = model.mapOffset(start);
			}
			var end = start + text.length;
			
			var styles = [];
			
			// for any sub range that is not a comment, parse code generating tokens (keywords, numbers, brackets, line comments, etc)
			var offset = start, comments = this.comments;
			var startIndex = this._binarySearch(comments, start, true);
			for (var i = startIndex; i < comments.length; i++) {
				if (comments[i].start >= end) { break; }
				var commentStart = comments[i].start;
				var commentEnd = comments[i].end;
				if (offset < commentStart) {
					this._parse(text.substring(offset - start, commentStart - start), offset, styles);
				}
				var style = comments[i].type === DOC_COMMENT ? docCommentStyle : multiCommentStyle;
				if (this.whitespacesVisible || this.detectHyperlinks) {
					var s = Math.max(offset, commentStart);
					var e = Math.min(end, commentEnd);
					this._parseComment(text.substring(s - start, e - start), s, styles, style, comments[i].type);
				} else {
					styles.push({start: commentStart, end: commentEnd, style: style});
				}
				offset = commentEnd;
			}
			if (offset < end) {
				this._parse(text.substring(offset - start, end - start), offset, styles);
			}
			if (model.getBaseModel) {
				for (var j = 0; j < styles.length; j++) {
					var length = styles[j].end - styles[j].start;
					styles[j].start = model.mapOffset(styles[j].start, true);
					styles[j].end = styles[j].start + length;
				}
			}
			return styles;
		},
		_parse: function(text, offset, styles) {
			var scanner = this._scanner;
			scanner.setText(text);
			var token;
			while ((token = scanner.nextToken())) {
				var tokenStart = scanner.getStartOffset() + offset;
				var style = null;
				if (tokenStart === this._matchingBracket) {
					style = bracketStyle;
				} else {
					switch (token) {
						case KEYWORD: style = keywordStyle; break;
						case STRING:
							if (this.whitespacesVisible) {
								this._parseString(scanner.getData(), tokenStart, styles, stringStyle);
								continue;
							} else {
								style = stringStyle;
							}
							break;
						case DOC_COMMENT: 
							this._parseComment(scanner.getData(), tokenStart, styles, docCommentStyle, token);
							continue;
						case SINGLELINE_COMMENT:
							this._parseComment(scanner.getData(), tokenStart, styles, singleCommentStyle, token);
							continue;
						case MULTILINE_COMMENT: 
							this._parseComment(scanner.getData(), tokenStart, styles, multiCommentStyle, token);
							continue;
						case WHITE_TAB:
							if (this.whitespacesVisible) {
								style = tabStyle;
							}
							break;
						case WHITE_SPACE:
							if (this.whitespacesVisible) {
								style = spaceStyle;
							}
							break;
					}
				}
				styles.push({start: tokenStart, end: scanner.getOffset() + offset, style: style});
			}
		},
		_parseComment: function(text, offset, styles, s, type) {
			var scanner = this._commentScanner;
			scanner.setText(text);
			scanner.setType(type);
			var token;
			while ((token = scanner.nextToken())) {
				var tokenStart = scanner.getStartOffset() + offset;
				var style = s;
				switch (token) {
					case WHITE_TAB:
						if (this.whitespacesVisible) {
							style = tabStyle;
						}
						break;
					case WHITE_SPACE:
						if (this.whitespacesVisible) {
							style = spaceStyle;
						}
						break;
					case HTML_MARKUP:
						style = htmlMarkupStyle;
						break;
					case DOC_TAG:
						style = doctagStyle;
						break;
					case TASK_TAG:
						style = tasktagStyle;
						break;
					default:
						if (this.detectHyperlinks) {
							style = this._detectHyperlinks(scanner.getData(), tokenStart, styles, style);
						}
				}
				if (style) {
					styles.push({start: tokenStart, end: scanner.getOffset() + offset, style: style});
				}
			}
		},
		_parseString: function(text, offset, styles, s) {
			var scanner = this._whitespaceScanner;
			scanner.setText(text);
			var token;
			while ((token = scanner.nextToken())) {
				var tokenStart = scanner.getStartOffset() + offset;
				var style = s;
				switch (token) {
					case WHITE_TAB:
						if (this.whitespacesVisible) {
							style = tabStyle;
						}
						break;
					case WHITE_SPACE:
						if (this.whitespacesVisible) {
							style = spaceStyle;
						}
						break;
				}
				if (style) {
					styles.push({start: tokenStart, end: scanner.getOffset() + offset, style: style});
				}
			}
		},
		_detectHyperlinks: function(text, offset, styles, s) {
			var href = null, index, linkStyle;
			if ((index = text.indexOf("://")) > 0) {
				href = text;
				var start = index;
				while (start > 0) {
					var c = href.charCodeAt(start - 1);
					if (!((97 <= c && c <= 122) || (65 <= c && c <= 90) || 0x2d === c || (48 <= c && c <= 57))) { //LETTER OR DASH OR NUMBER
						break;
					}
					start--;
				}
				if (start > 0) {
					var brackets = "\"\"''(){}[]<>";
					index = brackets.indexOf(href.substring(start - 1, start));
					if (index !== -1 && (index & 1) === 0 && (index = href.lastIndexOf(brackets.substring(index + 1, index + 2))) !== -1) {
						var end = index;
						linkStyle = this._clone(s);
						linkStyle.tagName = "A";
						linkStyle.attributes = {href: href.substring(start, end)};
						styles.push({start: offset, end: offset + start, style: s});
						styles.push({start: offset + start, end: offset + end, style: linkStyle});
						styles.push({start: offset + end, end: offset + text.length, style: s});
						return null;
					}
				}
			} else if (text.toLowerCase().indexOf("bug#") === 0) {
				href = "https://bugs.eclipse.org/bugs/show_bug.cgi?id=" + parseInt(text.substring(4), 10);
			}
			if (href) {
				linkStyle = this._clone(s);
				linkStyle.tagName = "A";
				linkStyle.attributes = {href: href};
				return linkStyle;
			}
			return s;
		},
		_clone: function(obj) {
			if (!obj) { return obj; }
			var newObj = {};
			for (var p in obj) {
				if (obj.hasOwnProperty(p)) {
					var value = obj[p];
					newObj[p] = value;
				}
			}
			return newObj;
		},
		_findComments: function(text, offset) {
			offset = offset || 0;
			var scanner = this._firstScanner, token;
			scanner.setText(text);
			var result = [];
			while ((token = scanner.nextToken())) {
				if (token === MULTILINE_COMMENT || token === DOC_COMMENT) {
					var comment = {
						start: scanner.getStartOffset() + offset,
						end: scanner.getOffset() + offset,
						type: token
					};
					result.push(comment);
					//TODO can we avoid this work if edition does not overlap comment?
					this._computeTasks(token, scanner.getStartOffset() + offset, scanner.getOffset() + offset);
				}
				if (token === SINGLELINE_COMMENT) {
					//TODO can we avoid this work if edition does not overlap comment?
					this._computeTasks(token, scanner.getStartOffset() + offset, scanner.getOffset() + offset);
				}
			}
			return result;
		}, 
		_findMatchingBracket: function(model, offset) {
			if (model.getBaseModel) { model = model.getBaseModel(); }
			var brackets = "{}()[]<>";
			var bracket = model.getText(offset, offset + 1);
			var bracketIndex = brackets.indexOf(bracket, 0);
			if (bracketIndex === -1) { return -1; }
			var closingBracket;
			if (bracketIndex & 1) {
				closingBracket = brackets.substring(bracketIndex - 1, bracketIndex);
			} else {
				closingBracket = brackets.substring(bracketIndex + 1, bracketIndex + 2);
			}
			var lineIndex = model.getLineAtOffset(offset);
			var lineText = model.getLine(lineIndex);
			var lineStart = model.getLineStart(lineIndex);
			var lineEnd = model.getLineEnd(lineIndex);
			brackets = this._findBrackets(bracket, closingBracket, lineText, lineStart, lineStart, lineEnd);
			for (var i=0; i<brackets.length; i++) {
				var sign = brackets[i] >= 0 ? 1 : -1;
				if (brackets[i] * sign === offset) {
					var level = 1;
					if (bracketIndex & 1) {
						i--;
						for (; i>=0; i--) {
							sign = brackets[i] >= 0 ? 1 : -1;
							level += sign;
							if (level === 0) {
								return brackets[i] * sign;
							}
						}
						lineIndex -= 1;
						while (lineIndex >= 0) {
							lineText = model.getLine(lineIndex);
							lineStart = model.getLineStart(lineIndex);
							lineEnd = model.getLineEnd(lineIndex);
							brackets = this._findBrackets(bracket, closingBracket, lineText, lineStart, lineStart, lineEnd);
							for (var j=brackets.length - 1; j>=0; j--) {
								sign = brackets[j] >= 0 ? 1 : -1;
								level += sign;
								if (level === 0) {
									return brackets[j] * sign;
								}
							}
							lineIndex--;
						}
					} else {
						i++;
						for (; i<brackets.length; i++) {
							sign = brackets[i] >= 0 ? 1 : -1;
							level += sign;
							if (level === 0) {
								return brackets[i] * sign;
							}
						}
						lineIndex += 1;
						var lineCount = model.getLineCount ();
						while (lineIndex < lineCount) {
							lineText = model.getLine(lineIndex);
							lineStart = model.getLineStart(lineIndex);
							lineEnd = model.getLineEnd(lineIndex);
							brackets = this._findBrackets(bracket, closingBracket, lineText, lineStart, lineStart, lineEnd);
							for (var k=0; k<brackets.length; k++) {
								sign = brackets[k] >= 0 ? 1 : -1;
								level += sign;
								if (level === 0) {
									return brackets[k] * sign;
								}
							}
							lineIndex++;
						}
					}
					break;
				}
			}
			return -1;
		},
		_findBrackets: function(bracket, closingBracket, text, textOffset, start, end) {
			var result = [];
			var bracketToken = bracket.charCodeAt(0);
			var closingBracketToken = closingBracket.charCodeAt(0);
			// for any sub range that is not a comment, parse code generating tokens (keywords, numbers, brackets, line comments, etc)
			var offset = start, scanner = this._scanner, token, comments = this.comments;
			var startIndex = this._binarySearch(comments, start, true);
			for (var i = startIndex; i < comments.length; i++) {
				if (comments[i].start >= end) { break; }
				var commentStart = comments[i].start;
				var commentEnd = comments[i].end;
				if (offset < commentStart) {
					scanner.setText(text.substring(offset - start, commentStart - start));
					while ((token = scanner.nextToken())) {
						if (token === bracketToken) {
							result.push(scanner.getStartOffset() + offset - start + textOffset);
						} else if (token === closingBracketToken) {
							result.push(-(scanner.getStartOffset() + offset - start + textOffset));
						}
					}
				}
				offset = commentEnd;
			}
			if (offset < end) {
				scanner.setText(text.substring(offset - start, end - start));
				while ((token = scanner.nextToken())) {
					if (token === bracketToken) {
						result.push(scanner.getStartOffset() + offset - start + textOffset);
					} else if (token === closingBracketToken) {
						result.push(-(scanner.getStartOffset() + offset - start + textOffset));
					}
				}
			}
			return result;
		},
		_onDestroy: function(e) {
			this.destroy();
		},
		_onLineStyle: function (e) {
			if (e.textView === this.view) {
				e.style = this._getLineStyle(e.lineIndex);
			}
			e.ranges = this._getStyles(e.textView.getModel(), e.lineText, e.lineStart);
		},
		_onSelection: function(e) {
			var oldSelection = e.oldValue;
			var newSelection = e.newValue;
			var view = this.view;
			var model = view.getModel();
			var lineIndex;
			var bracket = this._matchingBracket;
			if (bracket !== undefined) {
				if (model.getBaseModel) { bracket = model.mapOffset(bracket, true); }
				lineIndex = model.getLineAtOffset(bracket);
				view.redrawLines(lineIndex, lineIndex + 1);
				this._matchingBracket = this._currentBracket = undefined;
			}
			if (this.highlightCaretLine) {
				var oldLineIndex = model.getLineAtOffset(oldSelection.start);
				lineIndex = model.getLineAtOffset(newSelection.start);
				var newEmpty = newSelection.start === newSelection.end;
				var oldEmpty = oldSelection.start === oldSelection.end;
				if (!(oldLineIndex === lineIndex && oldEmpty && newEmpty)) {
					if (oldEmpty) {
						view.redrawLines(oldLineIndex, oldLineIndex + 1);
					}
					if ((oldLineIndex !== lineIndex || !oldEmpty) && newEmpty) {
						view.redrawLines(lineIndex, lineIndex + 1);
					}
				}
			}
			if (newSelection.start !== newSelection.end || newSelection.start === 0) {
				return;
			}
			var caret = view.getCaretOffset() - 1;
			if (caret < 0) { return; }
			var mapCaret = caret;
			if (model.getBaseModel) {
				mapCaret = model.mapOffset(caret);
			}
			bracket = this._findMatchingBracket(model, mapCaret);
			if (bracket !== -1) {
				this._currentBracket = mapCaret;
				this._matchingBracket = bracket;
				if (model.getBaseModel) { bracket = model.mapOffset(bracket, true); }
				lineIndex = model.getLineAtOffset(bracket);
				view.redrawLines(lineIndex, lineIndex + 1);
			}
		},
		_onModelChanged: function(e) {
			var start = e.start;
			var removedCharCount = e.removedCharCount;
			var addedCharCount = e.addedCharCount;
			var changeCount = addedCharCount - removedCharCount;
			if (this._matchingBracket && start < this._matchingBracket) { this._matchingBracket += changeCount; }
			if (this._currentBracket && start < this._currentBracket) { this._currentBracket += changeCount; }
			var view = this.view;
			var viewModel = view.getModel();
			var baseModel = viewModel.getBaseModel ? viewModel.getBaseModel() : viewModel;
			var end = start + removedCharCount;
			var charCount = baseModel.getCharCount();
			var commentCount = this.comments.length;
			var lineStart = baseModel.getLineStart(baseModel.getLineAtOffset(start));
			var commentStart = this._binarySearch(this.comments, lineStart, true);
			var commentEnd = this._binarySearch(this.comments, end, false, commentStart - 1, commentCount);
			
			var ts;
			if (commentStart < commentCount && this.comments[commentStart].start <= lineStart && lineStart < this.comments[commentStart].end) {
				ts = this.comments[commentStart].start;
				if (ts > start) { ts += changeCount; }
			} else {
				ts = lineStart;
			}
			var te;
			if (commentEnd < commentCount) {
				te = this.comments[commentEnd].end;
				if (te > start) { te += changeCount; }
				commentEnd += 1;
			} else {
				commentEnd = commentCount;
				te = charCount;//TODO could it be smaller?
			}
			var text = baseModel.getText(ts, te), comment;
			var newComments = this._findComments(text, ts), i;
			for (i = commentStart; i < this.comments.length; i++) {
				comment = this.comments[i];
				if (comment.start > start) { comment.start += changeCount; }
				if (comment.start > start) { comment.end += changeCount; }
			}
			var redraw = (commentEnd - commentStart) !== newComments.length;
			if (!redraw) {
				for (i=0; i<newComments.length; i++) {
					comment = this.comments[commentStart + i];
					var newComment = newComments[i];
					if (comment.start !== newComment.start || comment.end !== newComment.end || comment.type !== newComment.type) {
						redraw = true;
						break;
					} 
				}
			}
			var args = [commentStart, commentEnd - commentStart].concat(newComments);
			Array.prototype.splice.apply(this.comments, args);
			if (redraw) {
				var redrawStart = ts;
				var redrawEnd = te;
				if (viewModel !== baseModel) {
					redrawStart = viewModel.mapOffset(redrawStart, true);
					redrawEnd = viewModel.mapOffset(redrawEnd, true);
				}
				view.redrawRange(redrawStart, redrawEnd);
			}

			if (this.foldingEnabled && baseModel !== viewModel && this.annotationModel) {
				var annotationModel = this.annotationModel;
				var iter = annotationModel.getAnnotations(ts, te);
				var remove = [], all = [];
				var annotation;
				while (iter.hasNext()) {
					annotation = iter.next();
					if (annotation.type === "orion.annotation.folding") {
						all.push(annotation);
						for (i = 0; i < newComments.length; i++) {
							if (annotation.start === newComments[i].start && annotation.end === newComments[i].end) {
								break;
							}
						}
						if (i === newComments.length) {
							remove.push(annotation);
							annotation.expand();
						} else {
							var annotationStart = annotation.start;
							var annotationEnd = annotation.end;
							if (annotationStart > start) {
								annotationStart -= changeCount;
							}
							if (annotationEnd > start) {
								annotationEnd -= changeCount;
							}
							if (annotationStart <= start && start < annotationEnd && annotationStart <= end && end < annotationEnd) {
								var startLine = baseModel.getLineAtOffset(annotation.start);
								var endLine = baseModel.getLineAtOffset(annotation.end);
								if (startLine !== endLine) {
									if (!annotation.expanded) {
										annotation.expand();
										annotationModel.modifyAnnotation(annotation);
									}
								} else {
									annotationModel.removeAnnotation(annotation);
								}
							}
						}
					}
				}
				var add = [];
				for (i = 0; i < newComments.length; i++) {
					comment = newComments[i];
					for (var j = 0; j < all.length; j++) {
						if (all[j].start === comment.start && all[j].end === comment.end) {
							break;
						}
					}
					if (j === all.length) {
						annotation = this._createFoldingAnnotation(viewModel, baseModel, comment.start, comment.end);
						if (annotation) {
							add.push(annotation);
						}
					}
				}
				annotationModel.replaceAnnotations(remove, add);
			}
		}
	};
	return TextStyler;
}());

if (typeof window !== "undefined" && typeof window.define !== "undefined") {
	define([], function() {
		return examples.textview;
	});
}
