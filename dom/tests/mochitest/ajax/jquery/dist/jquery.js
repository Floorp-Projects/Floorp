// prevent execution of jQuery if included more than once
if(typeof window.jQuery == "undefined") {
/*
 * jQuery 1.1.3a - New Wave Javascript
 *
 * Copyright (c) 2007 John Resig (jquery.com)
 * Dual licensed under the MIT (MIT-LICENSE.txt)
 * and GPL (GPL-LICENSE.txt) licenses.
 *
 * $Date: 2007/06/29 15:10:42 $
 * $Rev: 1961 $
 */

// Global undefined variable
window.undefined = window.undefined;

/**
 * Create a new jQuery Object
 *
 * @constructor
 * @private
 * @name jQuery
 * @param String|Function|Element|Array<Element>|jQuery a selector
 * @param jQuery|Element|Array<Element> c context
 * @cat Core
 */
var jQuery = function(a,c) {
	// If the context is global, return a new object
	if ( window == this )
		return new jQuery(a,c);
	
	return this.init(a,c);
};

// Map over the $ in case of overwrite
if ( typeof $ != "undefined" )
	jQuery._$ = $;
	
// Map the jQuery namespace to the '$' one
var $ = jQuery;

/**
 * This function accepts a string containing a CSS or
 * basic XPath selector which is then used to match a set of elements.
 *
 * The core functionality of jQuery centers around this function.
 * Everything in jQuery is based upon this, or uses this in some way.
 * The most basic use of this function is to pass in an expression
 * (usually consisting of CSS or XPath), which then finds all matching
 * elements.
 *
 * By default, if no context is specified, $() looks for DOM elements within the context of the
 * current HTML document. If you do specify a context, such as a DOM
 * element or jQuery object, the expression will be matched against
 * the contents of that context.
 *
 * See [[DOM/Traversing/Selectors]] for the allowed CSS/XPath syntax for expressions.
 *
 * @example $("div > p")
 * @desc Finds all p elements that are children of a div element.
 * @before <p>one</p> <div><p>two</p></div> <p>three</p>
 * @result [ <p>two</p> ]
 *
 * @example $("input:radio", document.forms[0])
 * @desc Searches for all inputs of type radio within the first form in the document
 *
 * @example $("div", xml.responseXML)
 * @desc This finds all div elements within the specified XML document.
 *
 * @name $
 * @param String expr An expression to search with
 * @param Element|jQuery context (optional) A DOM Element, Document or jQuery to use as context
 * @cat Core
 * @type jQuery
 * @see $(Element)
 * @see $(Element<Array>)
 */
 
/**
 * Create DOM elements on-the-fly from the provided String of raw HTML.
 *
 * @example $("<div><p>Hello</p></div>").appendTo("body")
 * @desc Creates a div element (and all of its contents) dynamically, 
 * and appends it to the body element. Internally, an
 * element is created and its innerHTML property set to the given markup.
 * It is therefore both quite flexible and limited. 
 *
 * @name $
 * @param String html A string of HTML to create on the fly.
 * @cat Core
 * @type jQuery
 * @see appendTo(String)
 */

/**
 * Wrap jQuery functionality around a single or multiple DOM Element(s).
 *
 * This function also accepts XML Documents and Window objects
 * as valid arguments (even though they are not DOM Elements).
 *
 * @example $(document.body).css( "background", "black" );
 * @desc Sets the background color of the page to black.
 *
 * @example $( myForm.elements ).hide()
 * @desc Hides all the input elements within a form
 *
 * @name $
 * @param Element|Array<Element> elems DOM element(s) to be encapsulated by a jQuery object.
 * @cat Core
 * @type jQuery
 */

/**
 * A shorthand for $(document).ready(), allowing you to bind a function
 * to be executed when the DOM document has finished loading. This function
 * behaves just like $(document).ready(), in that it should be used to wrap
 * other $() operations on your page that depend on the DOM being ready to be
 * operated on. While this function is, technically, chainable - there really
 * isn't much use for chaining against it.
 *
 * You can have as many $(document).ready events on your page as you like.
 *
 * See ready(Function) for details about the ready event. 
 * 
 * @example $(function(){
 *   // Document is ready
 * });
 * @desc Executes the function when the DOM is ready to be used.
 *
 * @example jQuery(function($) {
 *   // Your code using failsafe $ alias here...
 * });
 * @desc Uses both the shortcut for $(document).ready() and the argument
 * to write failsafe jQuery code using the $ alias, without relying on the
 * global alias.
 *
 * @name $
 * @param Function fn The function to execute when the DOM is ready.
 * @cat Core
 * @type jQuery
 * @see ready(Function)
 */

jQuery.fn = jQuery.prototype = {
	/**
	 * Initialize a new jQuery object
	 *
	 * @private
	 * @name init
	 * @param String|Function|Element|Array<Element>|jQuery a selector
	 * @param jQuery|Element|Array<Element> c context
	 * @cat Core
	 */
	init: function(a,c) {
		// Make sure that a selection was provided
		a = a || document;

		// HANDLE: $(function)
		// Shortcut for document ready
		if ( jQuery.isFunction(a) )
			return new jQuery(document)[ jQuery.fn.ready ? "ready" : "load" ]( a );

		// Handle HTML strings
		if ( typeof a  == "string" ) {
			// HANDLE: $(html) -> $(array)
			var m = /^[^<]*(<(.|\s)+>)[^>]*$/.exec(a);
			if ( m )
				a = jQuery.clean( [ m[1] ] );

			// HANDLE: $(expr)
			else
				return new jQuery( c ).find( a );
		}

		return this.setArray(
			// HANDLE: $(array)
			a.constructor == Array && a ||

			// HANDLE: $(arraylike)
			// Watch for when an array-like object is passed as the selector
			(a.jquery || a.length && a != window && !a.nodeType && a[0] != undefined && a[0].nodeType) && jQuery.makeArray( a ) ||

			// HANDLE: $(*)
			[ a ] );
	},
	
	/**
	 * The current version of jQuery.
	 *
	 * @private
	 * @property
	 * @name jquery
	 * @type String
	 * @cat Core
	 */
	jquery: "1.1.3a",

	/**
	 * The number of elements currently matched. The size function will return the same value.
	 *
	 * @example $("img").length;
	 * @before <img src="test1.jpg"/> <img src="test2.jpg"/>
	 * @result 2
	 *
	 * @property
	 * @name length
	 * @type Number
	 * @cat Core
	 */

	/**
	 * Get the number of elements currently matched. This returns the same
	 * number as the 'length' property of the jQuery object.
	 *
	 * @example $("img").size();
	 * @before <img src="test1.jpg"/> <img src="test2.jpg"/>
	 * @result 2
	 *
	 * @name size
	 * @type Number
	 * @cat Core
	 */
	size: function() {
		return this.length;
	},
	
	length: 0,

	/**
	 * Access all matched DOM elements. This serves as a backwards-compatible
	 * way of accessing all matched elements (other than the jQuery object
	 * itself, which is, in fact, an array of elements).
	 *
	 * It is useful if you need to operate on the DOM elements themselves instead of using built-in jQuery functions.
	 *
	 * @example $("img").get();
	 * @before <img src="test1.jpg"/> <img src="test2.jpg"/>
	 * @result [ <img src="test1.jpg"/> <img src="test2.jpg"/> ]
	 * @desc Selects all images in the document and returns the DOM Elements as an Array
	 *
	 * @name get
	 * @type Array<Element>
	 * @cat Core
	 */

	/**
	 * Access a single matched DOM element at a specified index in the matched set.
	 * This allows you to extract the actual DOM element and operate on it
	 * directly without necessarily using jQuery functionality on it.
	 *
	 * @example $("img").get(0);
	 * @before <img src="test1.jpg"/> <img src="test2.jpg"/>
	 * @result <img src="test1.jpg"/>
	 * @desc Selects all images in the document and returns the first one
	 *
	 * @name get
	 * @type Element
	 * @param Number num Access the element in the Nth position.
	 * @cat Core
	 */
	get: function( num ) {
		return num == undefined ?

			// Return a 'clean' array
			jQuery.makeArray( this ) :

			// Return just the object
			this[num];
	},
	
	/**
	 * Set the jQuery object to an array of elements, while maintaining
	 * the stack.
	 *
	 * @example $("img").pushStack([ document.body ]);
	 * @result $("img").pushStack() == [ document.body ]
	 *
	 * @private
	 * @name pushStack
	 * @type jQuery
	 * @param Elements elems An array of elements
	 * @cat Core
	 */
	pushStack: function( a ) {
		var ret = jQuery(a);
		ret.prevObject = this;
		return ret;
	},
	
	/**
	 * Set the jQuery object to an array of elements. This operation is
	 * completely destructive - be sure to use .pushStack() if you wish to maintain
	 * the jQuery stack.
	 *
	 * @example $("img").setArray([ document.body ]);
	 * @result $("img").setArray() == [ document.body ]
	 *
	 * @private
	 * @name setArray
	 * @type jQuery
	 * @param Elements elems An array of elements
	 * @cat Core
	 */
	setArray: function( a ) {
		this.length = 0;
		[].push.apply( this, a );
		return this;
	},

	/**
	 * Execute a function within the context of every matched element.
	 * This means that every time the passed-in function is executed
	 * (which is once for every element matched) the 'this' keyword
	 * points to the specific DOM element.
	 *
	 * Additionally, the function, when executed, is passed a single
	 * argument representing the position of the element in the matched
	 * set (integer, zero-index).
	 *
	 * @example $("img").each(function(i){
	 *   this.src = "test" + i + ".jpg";
	 * });
	 * @before <img/><img/>
	 * @result <img src="test0.jpg"/><img src="test1.jpg"/>
	 * @desc Iterates over two images and sets their src property
	 *
	 * @name each
	 * @type jQuery
	 * @param Function fn A function to execute
	 * @cat Core
	 */
	each: function( fn, args ) {
		return jQuery.each( this, fn, args );
	},

	/**
	 * Searches every matched element for the object and returns
	 * the index of the element, if found, starting with zero. 
	 * Returns -1 if the object wasn't found.
	 *
	 * @example $("*").index( $('#foobar')[0] ) 
	 * @before <div id="foobar"><b></b><span id="foo"></span></div>
	 * @result 0
	 * @desc Returns the index for the element with ID foobar
	 *
	 * @example $("*").index( $('#foo')[0] ) 
	 * @before <div id="foobar"><b></b><span id="foo"></span></div>
	 * @result 2
	 * @desc Returns the index for the element with ID foo within another element
	 *
	 * @example $("*").index( $('#bar')[0] ) 
	 * @before <div id="foobar"><b></b><span id="foo"></span></div>
	 * @result -1
	 * @desc Returns -1, as there is no element with ID bar
	 *
	 * @name index
	 * @type Number
	 * @param Element subject Object to search for
	 * @cat Core
	 */
	index: function( obj ) {
		var pos = -1;
		this.each(function(i){
			if ( this == obj ) pos = i;
		});
		return pos;
	},

	/**
	 * Access a property on the first matched element.
	 * This method makes it easy to retrieve a property value
	 * from the first matched element.
	 *
	 * If the element does not have an attribute with such a
 	 * name, undefined is returned.
	 *
	 * @example $("img").attr("src");
	 * @before <img src="test.jpg"/>
	 * @result test.jpg
	 * @desc Returns the src attribute from the first image in the document.
	 *
	 * @name attr
	 * @type Object
	 * @param String name The name of the property to access.
	 * @cat DOM/Attributes
	 */

	/**
	 * Set a key/value object as properties to all matched elements.
	 *
	 * This serves as the best way to set a large number of properties
	 * on all matched elements.
	 *
	 * @example $("img").attr({ src: "test.jpg", alt: "Test Image" });
	 * @before <img/>
	 * @result <img src="test.jpg" alt="Test Image"/>
	 * @desc Sets src and alt attributes to all images.
	 *
	 * @name attr
	 * @type jQuery
	 * @param Map properties Key/value pairs to set as object properties.
	 * @cat DOM/Attributes
	 */

	/**
	 * Set a single property to a value, on all matched elements.
	 *
	 * Note that you can't set the name property of input elements in IE.
	 * Use $(html) or .append(html) or .html(html) to create elements
	 * on the fly including the name property.
	 *
	 * @example $("img").attr("src","test.jpg");
	 * @before <img/>
	 * @result <img src="test.jpg"/>
	 * @desc Sets src attribute to all images.
	 *
	 * @name attr
	 * @type jQuery
	 * @param String key The name of the property to set.
	 * @param Object value The value to set the property to.
	 * @cat DOM/Attributes
	 */
	 
	/**
	 * Set a single property to a computed value, on all matched elements.
	 *
	 * Instead of supplying a string value as described
	 * [[DOM/Attributes#attr.28_key.2C_value_.29|above]],
	 * a function is provided that computes the value.
	 *
	 * @example $("img").attr("title", function() { return this.src });
	 * @before <img src="test.jpg" />
	 * @result <img src="test.jpg" title="test.jpg" />
	 * @desc Sets title attribute from src attribute.
	 *
	 * @example $("img").attr("title", function(index) { return this.title + (i + 1); });
	 * @before <img title="pic" /><img title="pic" /><img title="pic" />
	 * @result <img title="pic1" /><img title="pic2" /><img title="pic3" />
	 * @desc Enumerate title attribute.
	 *
	 * @name attr
	 * @type jQuery
	 * @param String key The name of the property to set.
	 * @param Function value A function returning the value to set.
	 * 	 	  Scope: Current element, argument: Index of current element
	 * @cat DOM/Attributes
	 */
	attr: function( key, value, type ) {
		var obj = key;
		
		// Look for the case where we're accessing a style value
		if ( key.constructor == String )
			if ( value == undefined )
				return this.length && jQuery[ type || "attr" ]( this[0], key ) || undefined;
			else {
				obj = {};
				obj[ key ] = value;
			}
		
		// Check to see if we're setting style values
		return this.each(function(index){
			// Set all the styles
			for ( var prop in obj )
				jQuery.attr(
					type ? this.style : this,
					prop, jQuery.prop(this, obj[prop], type, index, prop)
				);
		});
	},

	/**
	 * Access a style property on the first matched element.
	 * This method makes it easy to retrieve a style property value
	 * from the first matched element.
	 *
	 * @example $("p").css("color");
	 * @before <p style="color:red;">Test Paragraph.</p>
	 * @result "red"
	 * @desc Retrieves the color style of the first paragraph
	 *
	 * @example $("p").css("font-weight");
	 * @before <p style="font-weight: bold;">Test Paragraph.</p>
	 * @result "bold"
	 * @desc Retrieves the font-weight style of the first paragraph.
	 *
	 * @name css
	 * @type String
	 * @param String name The name of the property to access.
	 * @cat CSS
	 */

	/**
	 * Set a key/value object as style properties to all matched elements.
	 *
	 * This serves as the best way to set a large number of style properties
	 * on all matched elements.
	 *
	 * @example $("p").css({ color: "red", background: "blue" });
	 * @before <p>Test Paragraph.</p>
	 * @result <p style="color:red; background:blue;">Test Paragraph.</p>
	 * @desc Sets color and background styles to all p elements.
	 *
	 * @name css
	 * @type jQuery
	 * @param Map properties Key/value pairs to set as style properties.
	 * @cat CSS
	 */

	/**
	 * Set a single style property to a value, on all matched elements.
	 * If a number is provided, it is automatically converted into a pixel value.
	 *
	 * @example $("p").css("color","red");
	 * @before <p>Test Paragraph.</p>
	 * @result <p style="color:red;">Test Paragraph.</p>
	 * @desc Changes the color of all paragraphs to red
	 *
	 * @example $("p").css("left",30);
	 * @before <p>Test Paragraph.</p>
	 * @result <p style="left:30px;">Test Paragraph.</p>
	 * @desc Changes the left of all paragraphs to "30px"
	 *
	 * @name css
	 * @type jQuery
	 * @param String key The name of the property to set.
	 * @param String|Number value The value to set the property to.
	 * @cat CSS
	 */
	css: function( key, value ) {
		return this.attr( key, value, "curCSS" );
	},

	/**
	 * Get the text contents of all matched elements. The result is
	 * a string that contains the combined text contents of all matched
	 * elements. This method works on both HTML and XML documents.
	 *
	 * @example $("p").text();
	 * @before <p><b>Test</b> Paragraph.</p><p>Paraparagraph</p>
	 * @result Test Paragraph.Paraparagraph
	 * @desc Gets the concatenated text of all paragraphs
	 *
	 * @name text
	 * @type String
	 * @cat DOM/Attributes
	 */

	/**
	 * Set the text contents of all matched elements.
	 *
	 * Similar to html(), but escapes HTML (replace "<" and ">" with their
	 * HTML entities).
	 *
	 * @example $("p").text("<b>Some</b> new text.");
	 * @before <p>Test Paragraph.</p>
	 * @result <p>&lt;b&gt;Some&lt;/b&gt; new text.</p>
	 * @desc Sets the text of all paragraphs.
	 *
	 * @example $("p").text("<b>Some</b> new text.", true);
	 * @before <p>Test Paragraph.</p>
	 * @result <p>Some new text.</p>
	 * @desc Sets the text of all paragraphs.
	 *
	 * @name text
	 * @type String
	 * @param String val The text value to set the contents of the element to.
	 * @cat DOM/Attributes
	 */
	text: function(e) {
		if ( typeof e == "string" )
			return this.empty().append( document.createTextNode( e ) );

		var t = "";
		jQuery.each( e || this, function(){
			jQuery.each( this.childNodes, function(){
				if ( this.nodeType != 8 )
					t += this.nodeType != 1 ?
						this.nodeValue : jQuery.fn.text([ this ]);
			});
		});
		return t;
	},

	/**
	 * Wrap all matched elements with a structure of other elements.
	 * This wrapping process is most useful for injecting additional
	 * stucture into a document, without ruining the original semantic
	 * qualities of a document.
	 *
	 * This works by going through the first element
	 * provided (which is generated, on the fly, from the provided HTML)
	 * and finds the deepest ancestor element within its
	 * structure - it is that element that will en-wrap everything else.
	 *
	 * This does not work with elements that contain text. Any necessary text
	 * must be added after the wrapping is done.
	 *
	 * @example $("p").wrap("<div class='wrap'></div>");
	 * @before <p>Test Paragraph.</p>
	 * @result <div class='wrap'><p>Test Paragraph.</p></div>
	 * 
	 * @name wrap
	 * @type jQuery
	 * @param String html A string of HTML, that will be created on the fly and wrapped around the target.
	 * @cat DOM/Manipulation
	 */

	/**
	 * Wrap all matched elements with a structure of other elements.
	 * This wrapping process is most useful for injecting additional
	 * stucture into a document, without ruining the original semantic
	 * qualities of a document.
	 *
	 * This works by going through the first element
	 * provided and finding the deepest ancestor element within its
	 * structure - it is that element that will en-wrap everything else.
	 *
 	 * This does not work with elements that contain text. Any necessary text
	 * must be added after the wrapping is done.
	 *
	 * @example $("p").wrap( document.getElementById('content') );
	 * @before <p>Test Paragraph.</p><div id="content"></div>
	 * @result <div id="content"><p>Test Paragraph.</p></div>
	 *
	 * @name wrap
	 * @type jQuery
	 * @param Element elem A DOM element that will be wrapped around the target.
	 * @cat DOM/Manipulation
	 */
	wrap: function() {
		// The elements to wrap the target around
		var a, args = arguments;

		// Wrap each of the matched elements individually
		return this.each(function(){
			if ( !a )
				a = jQuery.clean(args, this.ownerDocument);

			// Clone the structure that we're using to wrap
			var b = a[0].cloneNode(true);

			// Insert it before the element to be wrapped
			this.parentNode.insertBefore( b, this );

			// Find the deepest point in the wrap structure
			while ( b.firstChild )
				b = b.firstChild;

			// Move the matched element to within the wrap structure
			b.appendChild( this );
		});
	},

	/**
	 * Append content to the inside of every matched element.
	 *
	 * This operation is similar to doing an appendChild to all the
	 * specified elements, adding them into the document.
	 *
	 * @example $("p").append("<b>Hello</b>");
	 * @before <p>I would like to say: </p>
	 * @result <p>I would like to say: <b>Hello</b></p>
	 * @desc Appends some HTML to all paragraphs.
	 *
	 * @example $("p").append( $("#foo")[0] );
	 * @before <p>I would like to say: </p><b id="foo">Hello</b>
	 * @result <p>I would like to say: <b id="foo">Hello</b></p>
	 * @desc Appends an Element to all paragraphs.
	 *
	 * @example $("p").append( $("b") );
	 * @before <p>I would like to say: </p><b>Hello</b>
	 * @result <p>I would like to say: <b>Hello</b></p>
	 * @desc Appends a jQuery object (similar to an Array of DOM Elements) to all paragraphs.
	 *
	 * @name append
	 * @type jQuery
	 * @param <Content> content Content to append to the target
	 * @cat DOM/Manipulation
	 * @see prepend(<Content>)
	 * @see before(<Content>)
	 * @see after(<Content>)
	 */
	append: function() {
		return this.domManip(arguments, true, 1, function(a){
			this.appendChild( a );
		});
	},

	/**
	 * Prepend content to the inside of every matched element.
	 *
	 * This operation is the best way to insert elements
	 * inside, at the beginning, of all matched elements.
	 *
	 * @example $("p").prepend("<b>Hello</b>");
	 * @before <p>I would like to say: </p>
	 * @result <p><b>Hello</b>I would like to say: </p>
	 * @desc Prepends some HTML to all paragraphs.
	 *
	 * @example $("p").prepend( $("#foo")[0] );
	 * @before <p>I would like to say: </p><b id="foo">Hello</b>
	 * @result <p><b id="foo">Hello</b>I would like to say: </p>
	 * @desc Prepends an Element to all paragraphs.
	 *	
	 * @example $("p").prepend( $("b") );
	 * @before <p>I would like to say: </p><b>Hello</b>
	 * @result <p><b>Hello</b>I would like to say: </p>
	 * @desc Prepends a jQuery object (similar to an Array of DOM Elements) to all paragraphs.
	 *
	 * @name prepend
	 * @type jQuery
	 * @param <Content> content Content to prepend to the target.
	 * @cat DOM/Manipulation
	 * @see append(<Content>)
	 * @see before(<Content>)
	 * @see after(<Content>)
	 */
	prepend: function() {
		return this.domManip(arguments, true, -1, function(a){
			this.insertBefore( a, this.firstChild );
		});
	},
	
	/**
	 * Insert content before each of the matched elements.
	 *
	 * @example $("p").before("<b>Hello</b>");
	 * @before <p>I would like to say: </p>
	 * @result <b>Hello</b><p>I would like to say: </p>
	 * @desc Inserts some HTML before all paragraphs.
	 *
	 * @example $("p").before( $("#foo")[0] );
	 * @before <p>I would like to say: </p><b id="foo">Hello</b>
	 * @result <b id="foo">Hello</b><p>I would like to say: </p>
	 * @desc Inserts an Element before all paragraphs.
	 *
	 * @example $("p").before( $("b") );
	 * @before <p>I would like to say: </p><b>Hello</b>
	 * @result <b>Hello</b><p>I would like to say: </p>
	 * @desc Inserts a jQuery object (similar to an Array of DOM Elements) before all paragraphs.
	 *
	 * @name before
	 * @type jQuery
	 * @param <Content> content Content to insert before each target.
	 * @cat DOM/Manipulation
	 * @see append(<Content>)
	 * @see prepend(<Content>)
	 * @see after(<Content>)
	 */
	before: function() {
		return this.domManip(arguments, false, 1, function(a){
			this.parentNode.insertBefore( a, this );
		});
	},

	/**
	 * Insert content after each of the matched elements.
	 *
	 * @example $("p").after("<b>Hello</b>");
	 * @before <p>I would like to say: </p>
	 * @result <p>I would like to say: </p><b>Hello</b>
	 * @desc Inserts some HTML after all paragraphs.
	 *
	 * @example $("p").after( $("#foo")[0] );
	 * @before <b id="foo">Hello</b><p>I would like to say: </p>
	 * @result <p>I would like to say: </p><b id="foo">Hello</b>
	 * @desc Inserts an Element after all paragraphs.
	 *
	 * @example $("p").after( $("b") );
	 * @before <b>Hello</b><p>I would like to say: </p>
	 * @result <p>I would like to say: </p><b>Hello</b>
	 * @desc Inserts a jQuery object (similar to an Array of DOM Elements) after all paragraphs.
	 *
	 * @name after
	 * @type jQuery
	 * @param <Content> content Content to insert after each target.
	 * @cat DOM/Manipulation
	 * @see append(<Content>)
	 * @see prepend(<Content>)
	 * @see before(<Content>)
	 */
	after: function() {
		return this.domManip(arguments, false, -1, function(a){
			this.parentNode.insertBefore( a, this.nextSibling );
		});
	},

	/**
	 * Revert the most recent 'destructive' operation, changing the set of matched elements
	 * to its previous state (right before the destructive operation).
	 *
	 * If there was no destructive operation before, an empty set is returned.
	 *
	 * A 'destructive' operation is any operation that changes the set of
	 * matched jQuery elements. These functions are: <code>add</code>,
	 * <code>children</code>, <code>clone</code>, <code>filter</code>,
	 * <code>find</code>, <code>not</code>, <code>next</code>,
	 * <code>parent</code>, <code>parents</code>, <code>prev</code> and <code>siblings</code>.
	 *
	 * @example $("p").find("span").end();
	 * @before <p><span>Hello</span>, how are you?</p>
	 * @result [ <p>...</p> ]
	 * @desc Selects all paragraphs, finds span elements inside these, and reverts the
	 * selection back to the paragraphs.
	 *
	 * @name end
	 * @type jQuery
	 * @cat DOM/Traversing
	 */
	end: function() {
		return this.prevObject || jQuery([]);
	},

	/**
	 * Searches for all elements that match the specified expression.
	 
	 * This method is a good way to find additional descendant
	 * elements with which to process.
	 *
	 * All searching is done using a jQuery expression. The expression can be
	 * written using CSS 1-3 Selector syntax, or basic XPath.
	 *
	 * @example $("p").find("span");
	 * @before <p><span>Hello</span>, how are you?</p>
	 * @result [ <span>Hello</span> ]
	 * @desc Starts with all paragraphs and searches for descendant span
	 * elements, same as $("p span")
	 *
	 * @name find
	 * @type jQuery
	 * @param String expr An expression to search with.
	 * @cat DOM/Traversing
	 */
	find: function(t) {
		return this.pushStack( jQuery.unique( jQuery.map( this, function(a){
			return jQuery.find(t,a);
		}) ), t );
	},

	/**
	 * Clone matched DOM Elements and select the clones. 
	 *
	 * This is useful for moving copies of the elements to another
	 * location in the DOM.
	 *
	 * @example $("b").clone().prependTo("p");
	 * @before <b>Hello</b><p>, how are you?</p>
	 * @result <b>Hello</b><p><b>Hello</b>, how are you?</p>
	 * @desc Clones all b elements (and selects the clones) and prepends them to all paragraphs.
	 *
	 * @name clone
	 * @type jQuery
	 * @param Boolean deep (Optional) Set to false if you don't want to clone all descendant nodes, in addition to the element itself.
	 * @cat DOM/Manipulation
	 */
	clone: function(deep) {
		// Need to remove events on the element and its descendants
		var $this = this.add(this.find("*"));
		$this.each(function() {
			this._$events = {};
			for (var type in this.$events)
				this._$events[type] = jQuery.extend({},this.$events[type]);
		}).unbind();

		// Do the clone
		var r = this.pushStack( jQuery.map( this, function(a){
			return a.cloneNode( deep != undefined ? deep : true );
		}) );

		// Add the events back to the original and its descendants
		$this.each(function() {
			var events = this._$events;
			for (var type in events)
				for (var handler in events[type])
					jQuery.event.add(this, type, events[type][handler], events[type][handler].data);
			this._$events = null;
		});

		// Return the cloned set
		return r;
	},

	/**
	 * Removes all elements from the set of matched elements that do not
	 * match the specified expression(s). This method is used to narrow down
	 * the results of a search.
	 *
	 * Provide a comma-separated list of expressions to apply multiple filters at once.
	 *
	 * @example $("p").filter(".selected")
	 * @before <p class="selected">Hello</p><p>How are you?</p>
	 * @result [ <p class="selected">Hello</p> ]
	 * @desc Selects all paragraphs and removes those without a class "selected".
	 *
	 * @example $("p").filter(".selected, :first")
	 * @before <p>Hello</p><p>Hello Again</p><p class="selected">And Again</p>
	 * @result [ <p>Hello</p>, <p class="selected">And Again</p> ]
	 * @desc Selects all paragraphs and removes those without class "selected" and being the first one.
	 *
	 * @name filter
	 * @type jQuery
	 * @param String expression Expression(s) to search with.
	 * @cat DOM/Traversing
	 */
	 
	/**
	 * Removes all elements from the set of matched elements that do not
	 * pass the specified filter. This method is used to narrow down
	 * the results of a search.
	 *
	 * @example $("p").filter(function(index) {
	 *   return $("ol", this).length == 0;
	 * })
	 * @before <p><ol><li>Hello</li></ol></p><p>How are you?</p>
	 * @result [ <p>How are you?</p> ]
	 * @desc Remove all elements that have a child ol element
	 *
	 * @name filter
	 * @type jQuery
	 * @param Function filter A function to use for filtering
	 * @cat DOM/Traversing
	 */
	filter: function(t) {
		return this.pushStack(
			jQuery.isFunction( t ) &&
			jQuery.grep(this, function(el, index){
				return t.apply(el, [index])
			}) ||

			jQuery.multiFilter(t,this) );
	},

	/**
	 * Removes the specified Element from the set of matched elements. This
	 * method is used to remove a single Element from a jQuery object.
	 *
	 * @example $("p").not( $("#selected")[0] )
	 * @before <p>Hello</p><p id="selected">Hello Again</p>
	 * @result [ <p>Hello</p> ]
	 * @desc Removes the element with the ID "selected" from the set of all paragraphs.
	 *
	 * @name not
	 * @type jQuery
	 * @param Element el An element to remove from the set
	 * @cat DOM/Traversing
	 */

	/**
	 * Removes elements matching the specified expression from the set
	 * of matched elements. This method is used to remove one or more
	 * elements from a jQuery object.
	 *
	 * @example $("p").not("#selected")
	 * @before <p>Hello</p><p id="selected">Hello Again</p>
	 * @result [ <p>Hello</p> ]
	 * @desc Removes the element with the ID "selected" from the set of all paragraphs.
	 *
	 * @name not
	 * @type jQuery
	 * @param String expr An expression with which to remove matching elements
	 * @cat DOM/Traversing
	 */

	/**
	 * Removes any elements inside the array of elements from the set
	 * of matched elements. This method is used to remove one or more
	 * elements from a jQuery object.
	 *
	 * Please note: the expression cannot use a reference to the
	 * element name. See the two examples below.
	 *
	 * @example $("p").not( $("div p.selected") )
	 * @before <div><p>Hello</p><p class="selected">Hello Again</p></div>
	 * @result [ <p>Hello</p> ]
	 * @desc Removes all elements that match "div p.selected" from the total set of all paragraphs.
	 *
	 * @name not
	 * @type jQuery
	 * @param jQuery elems A set of elements to remove from the jQuery set of matched elements.
	 * @cat DOM/Traversing
	 */
	not: function(t) {
		return this.pushStack(
			t.constructor == String &&
			jQuery.multiFilter(t, this, true) ||

			jQuery.grep(this, function(a) {
				return ( t.constructor == Array || t.jquery )
					? jQuery.inArray( a, t ) < 0
					: a != t;
			})
		);
	},

	/**
	 * Adds more elements, matched by the given expression,
	 * to the set of matched elements.
	 *
	 * @example $("p").add("span")
	 * @before (HTML) <p>Hello</p><span>Hello Again</span>
	 * @result (jQuery object matching 2 elements) [ <p>Hello</p>, <span>Hello Again</span> ]
	 * @desc Compare the above result to the result of <code>$('p')</code>,
	 * which would just result in <code><nowiki>[ <p>Hello</p> ]</nowiki></code>.
	 * Using add(), matched elements of <code>$('span')</code> are simply
	 * added to the returned jQuery-object.
	 *
	 * @name add
	 * @type jQuery
	 * @param String expr An expression whose matched elements are added
	 * @cat DOM/Traversing
	 */
	 
	/**
	 * Adds more elements, created on the fly, to the set of
	 * matched elements.
	 *
	 * @example $("p").add("<span>Again</span>")
	 * @before <p>Hello</p>
	 * @result [ <p>Hello</p>, <span>Again</span> ]
	 *
	 * @name add
	 * @type jQuery
	 * @param String html A string of HTML to create on the fly.
	 * @cat DOM/Traversing
	 */

	/**
	 * Adds one or more Elements to the set of matched elements.
	 *
	 * @example $("p").add( document.getElementById("a") )
	 * @before <p>Hello</p><p><span id="a">Hello Again</span></p>
	 * @result [ <p>Hello</p>, <span id="a">Hello Again</span> ]
	 *
	 * @example $("p").add( document.forms[0].elements )
	 * @before <p>Hello</p><p><form><input/><button/></form>
	 * @result [ <p>Hello</p>, <input/>, <button/> ]
	 *
	 * @name add
	 * @type jQuery
	 * @param Element|Array<Element> elements One or more Elements to add
	 * @cat DOM/Traversing
	 */
	add: function(t) {
		return this.pushStack( jQuery.merge(
			this.get(),
			t.constructor == String ?
				jQuery(t).get() :
				t.length != undefined && (!t.nodeName || t.nodeName == "FORM") ?
					t : [t] )
		);
	},

	/**
	 * Checks the current selection against an expression and returns true,
	 * if at least one element of the selection fits the given expression.
	 *
	 * Does return false, if no element fits or the expression is not valid.
	 *
	 * filter(String) is used internally, therefore all rules that apply there
	 * apply here, too.
	 *
	 * @example $("input[@type='checkbox']").parent().is("form")
	 * @before <form><input type="checkbox" /></form>
	 * @result true
	 * @desc Returns true, because the parent of the input is a form element
	 * 
	 * @example $("input[@type='checkbox']").parent().is("form")
	 * @before <form><p><input type="checkbox" /></p></form>
	 * @result false
	 * @desc Returns false, because the parent of the input is a p element
	 *
	 * @name is
	 * @type Boolean
	 * @param String expr The expression with which to filter
	 * @cat DOM/Traversing
	 */
	is: function(expr) {
		return expr ? jQuery.multiFilter(expr,this).length > 0 : false;
	},
	
	/**
	 * Get the content of the value attribute of the first matched element.
	 *
	 * Use caution when relying on this function to check the value of
	 * multiple-select elements and checkboxes in a form. While it will
	 * still work as intended, it may not accurately represent the value
	 * the server will receive because these elements may send an array
	 * of values. For more robust handling of field values, see the
	 * [http://www.malsup.com/jquery/form/#fields fieldValue function of the Form Plugin].
	 *
	 * @example $("input").val();
	 * @before <input type="text" value="some text"/>
	 * @result "some text"
	 *
	 * @name val
	 * @type String
	 * @cat DOM/Attributes
	 */
	
	/**
	 * 	Set the value attribute of every matched element.
	 *
	 * @example $("input").val("test");
	 * @before <input type="text" value="some text"/>
	 * @result <input type="text" value="test"/>
	 *
	 * @name val
	 * @type jQuery
	 * @param String val Set the property to the specified value.
	 * @cat DOM/Attributes
	 */
	val: function( val ) {
		return val == undefined ?
			( this.length ? this[0].value : null ) :
			this.attr( "value", val );
	},
	
	/**
	 * Get the html contents of the first matched element.
	 * This property is not available on XML documents.
	 *
	 * @example $("div").html();
	 * @before <div><input/></div>
	 * @result <input/>
	 *
	 * @name html
	 * @type String
	 * @cat DOM/Attributes
	 */
	
	/**
	 * Set the html contents of every matched element.
	 * This property is not available on XML documents.
	 *
	 * @example $("div").html("<b>new stuff</b>");
	 * @before <div><input/></div>
	 * @result <div><b>new stuff</b></div>
	 *
	 * @name html
	 * @type jQuery
	 * @param String val Set the html contents to the specified value.
	 * @cat DOM/Attributes
	 */
	html: function( val ) {
		return val == undefined ?
			( this.length ? this[0].innerHTML : null ) :
			this.empty().append( val );
	},
	
	/**
	 * @private
	 * @name domManip
	 * @param Array args
	 * @param Boolean table Insert TBODY in TABLEs if one is not found.
	 * @param Number dir If dir<0, process args in reverse order.
	 * @param Function fn The function doing the DOM manipulation.
	 * @type jQuery
	 * @cat Core
	 */
	domManip: function(args, table, dir, fn){
		var clone = this.length > 1, a; 

		return this.each(function(){
			if ( !a ) {
				a = jQuery.clean(args, this.ownerDocument);
				if ( dir < 0 )
					a.reverse();
			}

			var obj = this;

			if ( table && jQuery.nodeName(this, "table") && jQuery.nodeName(a[0], "tr") )
				obj = this.getElementsByTagName("tbody")[0] || this.appendChild(document.createElement("tbody"));

			jQuery.each( a, function(){
				fn.apply( obj, [ clone ? this.cloneNode(true) : this ] );
			});

		});
	}
};

/**
 * Extends the jQuery object itself. Can be used to add functions into
 * the jQuery namespace and to [[Plugins/Authoring|add plugin methods]] (plugins).
 * 
 * @example jQuery.fn.extend({
 *   check: function() {
 *     return this.each(function() { this.checked = true; });
 *   },
 *   uncheck: function() {
 *     return this.each(function() { this.checked = false; });
 *   }
 * });
 * $("input[@type=checkbox]").check();
 * $("input[@type=radio]").uncheck();
 * @desc Adds two plugin methods.
 *
 * @example jQuery.extend({
 *   min: function(a, b) { return a < b ? a : b; },
 *   max: function(a, b) { return a > b ? a : b; }
 * });
 * @desc Adds two functions into the jQuery namespace
 *
 * @name $.extend
 * @param Object prop The object that will be merged into the jQuery object
 * @type Object
 * @cat Core
 */

/**
 * Extend one object with one or more others, returning the original,
 * modified, object. This is a great utility for simple inheritance.
 * 
 * @example var settings = { validate: false, limit: 5, name: "foo" };
 * var options = { validate: true, name: "bar" };
 * jQuery.extend(settings, options);
 * @result settings == { validate: true, limit: 5, name: "bar" }
 * @desc Merge settings and options, modifying settings
 *
 * @example var defaults = { validate: false, limit: 5, name: "foo" };
 * var options = { validate: true, name: "bar" };
 * var settings = jQuery.extend({}, defaults, options);
 * @result settings == { validate: true, limit: 5, name: "bar" }
 * @desc Merge defaults and options, without modifying the defaults
 *
 * @name $.extend
 * @param Object target The object to extend
 * @param Object prop1 The object that will be merged into the first.
 * @param Object propN (optional) More objects to merge into the first
 * @type Object
 * @cat JavaScript
 */
jQuery.extend = jQuery.fn.extend = function() {
	// copy reference to target object
	var target = arguments[0], a = 1;

	// extend jQuery itself if only one argument is passed
	if ( arguments.length == 1 ) {
		target = this;
		a = 0;
	}
	var prop;
	while ( (prop = arguments[a++]) != null )
		// Extend the base object
		for ( var i in prop ) target[i] = prop[i];

	// Return the modified object
	return target;
};

jQuery.extend({
	/**
	 * Run this function to give control of the $ variable back
	 * to whichever library first implemented it. This helps to make 
	 * sure that jQuery doesn't conflict with the $ object
	 * of other libraries.
	 *
	 * By using this function, you will only be able to access jQuery
	 * using the 'jQuery' variable. For example, where you used to do
	 * $("div p"), you now must do jQuery("div p").
	 *
	 * @example jQuery.noConflict();
	 * // Do something with jQuery
	 * jQuery("div p").hide();
	 * // Do something with another library's $()
	 * $("content").style.display = 'none';
	 * @desc Maps the original object that was referenced by $ back to $
	 *
	 * @example jQuery.noConflict();
	 * (function($) { 
	 *   $(function() {
	 *     // more code using $ as alias to jQuery
	 *   });
	 * })(jQuery);
	 * // other code using $ as an alias to the other library
	 * @desc Reverts the $ alias and then creates and executes a
	 * function to provide the $ as a jQuery alias inside the functions
	 * scope. Inside the function the original $ object is not available.
	 * This works well for most plugins that don't rely on any other library.
	 * 
	 *
	 * @name $.noConflict
	 * @type undefined
	 * @cat Core 
	 */
	noConflict: function() {
		if ( jQuery._$ )
			$ = jQuery._$;
		return jQuery;
	},

	// This may seem like some crazy code, but trust me when I say that this
	// is the only cross-browser way to do this. --John
	isFunction: function( fn ) {
		return !!fn && typeof fn != "string" && !fn.nodeName && 
			fn.constructor != Array && /function/i.test( fn + "" );
	},
	
	// check if an element is in a XML document
	isXMLDoc: function(elem) {
		return elem.tagName && elem.ownerDocument && !elem.ownerDocument.body;
	},

	nodeName: function( elem, name ) {
		return elem.nodeName && elem.nodeName.toUpperCase() == name.toUpperCase();
	},

	/**
	 * A generic iterator function, which can be used to seamlessly
	 * iterate over both objects and arrays. This function is not the same
	 * as $().each() - which is used to iterate, exclusively, over a jQuery
	 * object. This function can be used to iterate over anything.
	 *
	 * The callback has two arguments:the key (objects) or index (arrays) as first
	 * the first, and the value as the second.
	 *
	 * @example $.each( [0,1,2], function(i, n){
	 *   alert( "Item #" + i + ": " + n );
	 * });
	 * @desc This is an example of iterating over the items in an array,
	 * accessing both the current item and its index.
	 *
	 * @example $.each( { name: "John", lang: "JS" }, function(i, n){
	 *   alert( "Name: " + i + ", Value: " + n );
	 * });
	 *
	 * @desc This is an example of iterating over the properties in an
	 * Object, accessing both the current item and its key.
	 *
	 * @name $.each
	 * @param Object obj The object, or array, to iterate over.
	 * @param Function fn The function that will be executed on every object.
	 * @type Object
	 * @cat JavaScript
	 */
	// args is for internal usage only
	each: function( obj, fn, args ) {
		if ( obj.length == undefined )
			for ( var i in obj )
				fn.apply( obj[i], args || [i, obj[i]] );
		else
			for ( var i = 0, ol = obj.length; i < ol; i++ )
				if ( fn.apply( obj[i], args || [i, obj[i]] ) === false ) break;
		return obj;
	},
	
	prop: function(elem, value, type, index, prop){
			// Handle executable functions
			if ( jQuery.isFunction( value ) )
				value = value.call( elem, [index] );
				
			// exclude the following css properties to add px
			var exclude = /z-?index|font-?weight|opacity|zoom|line-?height/i;

			// Handle passing in a number to a CSS property
			return value && value.constructor == Number && type == "curCSS" && !exclude.test(prop) ?
				value + "px" :
				value;
	},

	className: {
		// internal only, use addClass("class")
		add: function( elem, c ){
			jQuery.each( c.split(/\s+/), function(i, cur){
				if ( !jQuery.className.has( elem.className, cur ) )
					elem.className += ( elem.className ? " " : "" ) + cur;
			});
		},

		// internal only, use removeClass("class")
		remove: function( elem, c ){
			elem.className = c != undefined ?
				jQuery.grep( elem.className.split(/\s+/), function(cur){
					return !jQuery.className.has( c, cur );	
				}).join(" ") : "";
		},

		// internal only, use is(".class")
		has: function( t, c ) {
			return jQuery.inArray( c, (t.className || t).toString().split(/\s+/) ) > -1;
		}
	},

	/**
	 * Swap in/out style options.
	 * @private
	 */
	swap: function(e,o,f) {
		for ( var i in o ) {
			e.style["old"+i] = e.style[i];
			e.style[i] = o[i];
		}
		f.apply( e, [] );
		for ( var i in o )
			e.style[i] = e.style["old"+i];
	},

	css: function(e,p) {
		if ( p == "height" || p == "width" ) {
			var old = {}, oHeight, oWidth, d = ["Top","Bottom","Right","Left"];

			jQuery.each( d, function(){
				old["padding" + this] = 0;
				old["border" + this + "Width"] = 0;
			});

			jQuery.swap( e, old, function() {
				if ( jQuery(e).is(':visible') ) {
					oHeight = e.offsetHeight;
					oWidth = e.offsetWidth;
				} else {
					e = jQuery(e.cloneNode(true))
						.find(":radio").removeAttr("checked").end()
						.css({
							visibility: "hidden", position: "absolute", display: "block", right: "0", left: "0"
						}).appendTo(e.parentNode)[0];

					var parPos = jQuery.css(e.parentNode,"position") || "static";
					if ( parPos == "static" )
						e.parentNode.style.position = "relative";

					oHeight = e.clientHeight;
					oWidth = e.clientWidth;

					if ( parPos == "static" )
						e.parentNode.style.position = "static";

					e.parentNode.removeChild(e);
				}
			});

			return p == "height" ? oHeight : oWidth;
		}

		return jQuery.curCSS( e, p );
	},

	curCSS: function(elem, prop, force) {
		var ret;

		if (prop == "opacity" && jQuery.browser.msie) {
			ret = jQuery.attr(elem.style, "opacity");
			return ret == "" ? "1" : ret;
		}
		
		if (prop.match(/float/i))
			prop = jQuery.browser.msie ? "styleFloat" : "cssFloat";

		if (!force && elem.style[prop])
			ret = elem.style[prop];

		else if (document.defaultView && document.defaultView.getComputedStyle) {

			if (prop.match(/float/i))
				prop = "float";

			prop = prop.replace(/([A-Z])/g,"-$1").toLowerCase();
			var cur = document.defaultView.getComputedStyle(elem, null);

			if ( cur )
				ret = cur.getPropertyValue(prop);
			else if ( prop == "display" )
				ret = "none";
			else
				jQuery.swap(elem, { display: "block" }, function() {
				    var c = document.defaultView.getComputedStyle(this, "");
				    ret = c && c.getPropertyValue(prop) || "";
				});

		} else if (elem.currentStyle) {
			var newProp = prop.replace(/\-(\w)/g,function(m,c){return c.toUpperCase();});
			ret = elem.currentStyle[prop] || elem.currentStyle[newProp];
		}

		return ret;
	},
	
	clean: function(a, doc) {
		var r = [];
		doc = doc || document;

		jQuery.each( a, function(i,arg){
			if ( !arg ) return;

			if ( arg.constructor == Number )
				arg = arg.toString();
			
			// Convert html string into DOM nodes
			if ( typeof arg == "string" ) {
				// Trim whitespace, otherwise indexOf won't work as expected
				var s = jQuery.trim(arg).toLowerCase(), div = doc.createElement("div"), tb = [];

				var wrap =
					// option or optgroup
					!s.indexOf("<opt") &&
					[1, "<select>", "</select>"] ||
					
					!s.indexOf("<leg") &&
					[1, "<fieldset>", "</fieldset>"] ||
					
					(!s.indexOf("<thead") || !s.indexOf("<tbody") || !s.indexOf("<tfoot") || !s.indexOf("<colg")) &&
					[1, "<table>", "</table>"] ||
					
					!s.indexOf("<tr") &&
					[2, "<table><tbody>", "</tbody></table>"] ||
					
				 	// <thead> matched above
					(!s.indexOf("<td") || !s.indexOf("<th")) &&
					[3, "<table><tbody><tr>", "</tr></tbody></table>"] ||
					
					!s.indexOf("<col") &&
					[2, "<table><colgroup>", "</colgroup></table>"] ||
					
					[0,"",""];

				// Go to html and back, then peel off extra wrappers
				div.innerHTML = wrap[1] + arg + wrap[2];
				
				// Move to the right depth
				while ( wrap[0]-- )
					div = div.firstChild;
				
				// Remove IE's autoinserted <tbody> from table fragments
				if ( jQuery.browser.msie ) {
					
					// String was a <table>, *may* have spurious <tbody>
					if ( !s.indexOf("<table") && s.indexOf("<tbody") < 0 ) 
						tb = div.firstChild && div.firstChild.childNodes;
						
					// String was a bare <thead> or <tfoot>
					else if ( wrap[1] == "<table>" && s.indexOf("<tbody") < 0 )
						tb = div.childNodes;

					for ( var n = tb.length-1; n >= 0 ; --n )
						if ( jQuery.nodeName(tb[n], "tbody") && !tb[n].childNodes.length )
							tb[n].parentNode.removeChild(tb[n]);
					
				}
				
				arg = jQuery.makeArray( div.childNodes );
			}

			if ( 0 === arg.length && !jQuery(arg).is("form, select") )
				return;

			if ( arg[0] == undefined || jQuery.nodeName(arg, "form") || arg.options )
				r.push( arg );
			else
				r = jQuery.merge( r, arg );

		});

		return r;
	},
	
	attr: function(elem, name, value){
		var fix = jQuery.isXMLDoc(elem) ? {} : {
			"for": "htmlFor",
			"class": "className",
			"float": jQuery.browser.msie ? "styleFloat" : "cssFloat",
			cssFloat: jQuery.browser.msie ? "styleFloat" : "cssFloat",
			styleFloat: jQuery.browser.msie ? "styleFloat" : "cssFloat",
			innerHTML: "innerHTML",
			className: "className",
			value: "value",
			disabled: "disabled",
			checked: "checked",
			readonly: "readOnly",
			selected: "selected",
			maxlength: "maxLength"
		};
		
		// IE actually uses filters for opacity ... elem is actually elem.style
		if ( name == "opacity" && jQuery.browser.msie ) {
			if ( value != undefined ) {
				// IE has trouble with opacity if it does not have layout
				// Force it by setting the zoom level
				elem.zoom = 1; 

				// Set the alpha filter to set the opacity
				elem.filter = (elem.filter || "").replace(/alpha\([^)]*\)/,"") +
					(parseFloat(value).toString() == "NaN" ? "" : "alpha(opacity=" + value * 100 + ")");
			}

			return elem.filter ? 
				(parseFloat( elem.filter.match(/opacity=([^)]*)/)[1] ) / 100).toString() : "";
		}
		
		// Certain attributes only work when accessed via the old DOM 0 way
		if ( fix[name] ) {
			if ( value != undefined ) elem[fix[name]] = value;
			return elem[fix[name]];

		} else if ( value == undefined && jQuery.browser.msie && jQuery.nodeName(elem, "form") && (name == "action" || name == "method") )
			return elem.getAttributeNode(name).nodeValue;

		// IE elem.getAttribute passes even for style
		else if ( elem.tagName ) {
			if ( value != undefined ) elem.setAttribute( name, value );
			if ( jQuery.browser.msie && /href|src/.test(name) && !jQuery.isXMLDoc(elem) ) 
				return elem.getAttribute( name, 2 );
			return elem.getAttribute( name );

		// elem is actually elem.style ... set the style
		} else {
			name = name.replace(/-([a-z])/ig,function(z,b){return b.toUpperCase();});
			if ( value != undefined ) elem[name] = value;
			return elem[name];
		}
	},
	
	/**
	 * Remove the whitespace from the beginning and end of a string.
	 *
	 * @example $.trim("  hello, how are you?  ");
	 * @result "hello, how are you?"
	 *
	 * @name $.trim
	 * @type String
	 * @param String str The string to trim.
	 * @cat JavaScript
	 */
	trim: function(t){
		return t.replace(/^\s+|\s+$/g, "");
	},

	makeArray: function( a ) {
		var r = [];

		// Need to use typeof to fight Safari childNodes crashes
		if ( typeof a != "array" )
			for ( var i = 0, al = a.length; i < al; i++ )
				r.push( a[i] );
		else
			r = a.slice( 0 );

		return r;
	},

	inArray: function( b, a ) {
		for ( var i = 0, al = a.length; i < al; i++ )
			if ( a[i] == b )
				return i;
		return -1;
	},

	/**
	 * Merge two arrays together by concatenating them.
	 *
	 * @example $.merge( [0,1,2], [2,3,4] )
	 * @result [0,1,2,2,3,4]
	 * @desc Merges two arrays.
	 *
	 * @name $.merge
	 * @type Array
	 * @param Array first The first array to merge, the elements of second are added.
	 * @param Array second The second array to append to the first, unaltered.
	 * @cat JavaScript
	 */
	merge: function(first, second) {
		// We have to loop this way because IE & Opera overwrite the length
		// expando of getElementsByTagName
		for ( var i = 0; second[i]; i++ )
			first.push(second[i]);
		return first;
	},

	/**
	 * Reduce an array (of jQuery objects only) to its unique elements.
	 *
	 * @example $.unique( [x1, x2, x3, x2, x3] )
	 * @result [x1, x2, x3]
	 * @desc Reduces the arrays of jQuery objects to unique elements by removing the duplicates of x2 and x3
	 *
	 * @name $.unique
	 * @type Array
	 * @param Array array The array to reduce to its unique jQuery objects.
	 * @cat JavaScript
	 */
	unique: function(first) {
		var r = [], num = jQuery.mergeNum++;

		for ( var i = 0, fl = first.length; i < fl; i++ )
			if ( num != first[i].mergeNum ) {
				first[i].mergeNum = num;
				r.push(first[i]);
			}

		return r;
	},

	mergeNum: 0,

	/**
	 * Filter items out of an array, by using a filter function.
	 *
	 * The specified function will be passed two arguments: The
	 * current array item and the index of the item in the array. The
	 * function must return 'true' to keep the item in the array, 
	 * false to remove it.
	 *
	 * @example $.grep( [0,1,2], function(i){
	 *   return i > 0;
	 * });
	 * @result [1, 2]
	 *
	 * @name $.grep
	 * @type Array
	 * @param Array array The Array to find items in.
	 * @param Function fn The function to process each item against.
	 * @param Boolean inv Invert the selection - select the opposite of the function.
	 * @cat JavaScript
	 */
	grep: function(elems, fn, inv) {
		// If a string is passed in for the function, make a function
		// for it (a handy shortcut)
		if ( typeof fn == "string" )
			fn = new Function("a","i","return " + fn);

		var result = [];

		// Go through the array, only saving the items
		// that pass the validator function
		for ( var i = 0, el = elems.length; i < el; i++ )
			if ( !inv && fn(elems[i],i) || inv && !fn(elems[i],i) )
				result.push( elems[i] );

		return result;
	},

	/**
	 * Translate all items in an array to another array of items.
	 *
	 * The translation function that is provided to this method is 
	 * called for each item in the array and is passed one argument: 
	 * The item to be translated.
	 *
	 * The function can then return the translated value, 'null'
	 * (to remove the item), or  an array of values - which will
	 * be flattened into the full array.
	 *
	 * @example $.map( [0,1,2], function(i){
	 *   return i + 4;
	 * });
	 * @result [4, 5, 6]
	 * @desc Maps the original array to a new one and adds 4 to each value.
	 *
	 * @example $.map( [0,1,2], function(i){
	 *   return i > 0 ? i + 1 : null;
	 * });
	 * @result [2, 3]
	 * @desc Maps the original array to a new one and adds 1 to each
	 * value if it is bigger then zero, otherwise it's removed-
	 * 
	 * @example $.map( [0,1,2], function(i){
	 *   return [ i, i + 1 ];
	 * });
	 * @result [0, 1, 1, 2, 2, 3]
	 * @desc Maps the original array to a new one, each element is added
	 * with it's original value and the value plus one.
	 *
	 * @name $.map
	 * @type Array
	 * @param Array array The Array to translate.
	 * @param Function fn The function to process each item against.
	 * @cat JavaScript
	 */
	map: function(elems, fn) {
		// If a string is passed in for the function, make a function
		// for it (a handy shortcut)
		if ( typeof fn == "string" )
			fn = new Function("a","return " + fn);

		var result = [];

		// Go through the array, translating each of the items to their
		// new value (or values).
		for ( var i = 0, el = elems.length; i < el; i++ ) {
			var val = fn(elems[i],i);

			if ( val !== null && val != undefined ) {
				if ( val.constructor != Array ) val = [val];
				result = result.concat( val );
			}
		}

		return result;
	}
});

/**
 * Contains flags for the useragent, read from navigator.userAgent.
 * Available flags are: safari, opera, msie, mozilla
 *
 * This property is available before the DOM is ready, therefore you can
 * use it to add ready events only for certain browsers.
 *
 * There are situations where object detections is not reliable enough, in that
 * cases it makes sense to use browser detection. Simply try to avoid both!
 *
 * A combination of browser and object detection yields quite reliable results.
 *
 * @example $.browser.msie
 * @desc Returns true if the current useragent is some version of microsoft's internet explorer
 *
 * @example if($.browser.safari) { $( function() { alert("this is safari!"); } ); }
 * @desc Alerts "this is safari!" only for safari browsers
 *
 * @property
 * @name $.browser
 * @type Boolean
 * @cat JavaScript
 */
 
/*
 * Whether the W3C compliant box model is being used.
 *
 * @property
 * @name $.boxModel
 * @type Boolean
 * @cat JavaScript
 */
new function() {
	var b = navigator.userAgent.toLowerCase();

	// Figure out what browser is being used
	jQuery.browser = {
		version: b.match(/.+(?:rv|it|ra|ie)[\/: ]([\d.]+)/)[1],
		safari: /webkit/.test(b),
		opera: /opera/.test(b),
		msie: /msie/.test(b) && !/opera/.test(b),
		mozilla: /mozilla/.test(b) && !/(compatible|webkit)/.test(b)
	};

	// Check to see if the W3C box model is being used
	jQuery.boxModel = !jQuery.browser.msie || document.compatMode == "CSS1Compat";
};

/**
 * Get a set of elements containing the unique parents of the matched
 * set of elements.
 *
 * You may use an optional expression to filter the set of parent elements that will match.
 *
 * @example $("p").parent()
 * @before <div><p>Hello</p><p>Hello</p></div>
 * @result [ <div><p>Hello</p><p>Hello</p></div> ]
 * @desc Find the parent element of each paragraph.
 *
 * @example $("p").parent(".selected")
 * @before <div><p>Hello</p></div><div class="selected"><p>Hello Again</p></div>
 * @result [ <div class="selected"><p>Hello Again</p></div> ]
 * @desc Find the parent element of each paragraph with a class "selected".
 *
 * @name parent
 * @type jQuery
 * @param String expr (optional) An expression to filter the parents with
 * @cat DOM/Traversing
 */

/**
 * Get a set of elements containing the unique ancestors of the matched
 * set of elements (except for the root element).
 *
 * The matched elements can be filtered with an optional expression.
 *
 * @example $("span").parents()
 * @before <html><body><div><p><span>Hello</span></p><span>Hello Again</span></div></body></html>
 * @result [ <body>...</body>, <div>...</div>, <p><span>Hello</span></p> ]
 * @desc Find all parent elements of each span.
 *
 * @example $("span").parents("p")
 * @before <html><body><div><p><span>Hello</span></p><span>Hello Again</span></div></body></html>
 * @result [ <p><span>Hello</span></p> ]
 * @desc Find all parent elements of each span that is a paragraph.
 *
 * @name parents
 * @type jQuery
 * @param String expr (optional) An expression to filter the ancestors with
 * @cat DOM/Traversing
 */

/**
 * Get a set of elements containing the unique next siblings of each of the
 * matched set of elements.
 *
 * It only returns the very next sibling for each element, not all
 * next siblings.
 *
 * You may provide an optional expression to filter the match.
 *
 * @example $("p").next()
 * @before <p>Hello</p><p>Hello Again</p><div><span>And Again</span></div>
 * @result [ <p>Hello Again</p>, <div><span>And Again</span></div> ]
 * @desc Find the very next sibling of each paragraph.
 *
 * @example $("p").next(".selected")
 * @before <p>Hello</p><p class="selected">Hello Again</p><div><span>And Again</span></div>
 * @result [ <p class="selected">Hello Again</p> ]
 * @desc Find the very next sibling of each paragraph that has a class "selected".
 *
 * @name next
 * @type jQuery
 * @param String expr (optional) An expression to filter the next Elements with
 * @cat DOM/Traversing
 */

/**
 * Get a set of elements containing the unique previous siblings of each of the
 * matched set of elements.
 *
 * Use an optional expression to filter the matched set.
 *
 * 	Only the immediately previous sibling is returned, not all previous siblings.
 *
 * @example $("p").prev()
 * @before <p>Hello</p><div><span>Hello Again</span></div><p>And Again</p>
 * @result [ <div><span>Hello Again</span></div> ]
 * @desc Find the very previous sibling of each paragraph.
 *
 * @example $("p").prev(".selected")
 * @before <div><span>Hello</span></div><p class="selected">Hello Again</p><p>And Again</p>
 * @result [ <div><span>Hello</span></div> ]
 * @desc Find the very previous sibling of each paragraph that has a class "selected".
 *
 * @name prev
 * @type jQuery
 * @param String expr (optional) An expression to filter the previous Elements with
 * @cat DOM/Traversing
 */

/**
 * Get a set of elements containing all of the unique siblings of each of the
 * matched set of elements.
 *
 * Can be filtered with an optional expressions.
 *
 * @example $("div").siblings()
 * @before <p>Hello</p><div><span>Hello Again</span></div><p>And Again</p>
 * @result [ <p>Hello</p>, <p>And Again</p> ]
 * @desc Find all siblings of each div.
 *
 * @example $("div").siblings(".selected")
 * @before <div><span>Hello</span></div><p class="selected">Hello Again</p><p>And Again</p>
 * @result [ <p class="selected">Hello Again</p> ]
 * @desc Find all siblings with a class "selected" of each div.
 *
 * @name siblings
 * @type jQuery
 * @param String expr (optional) An expression to filter the sibling Elements with
 * @cat DOM/Traversing
 */

/**
 * Get a set of elements containing all of the unique children of each of the
 * matched set of elements.
 *
 * This set can be filtered with an optional expression that will cause
 * only elements matching the selector to be collected.
 *
 * @example $("div").children()
 * @before <p>Hello</p><div><span>Hello Again</span></div><p>And Again</p>
 * @result [ <span>Hello Again</span> ]
 * @desc Find all children of each div.
 *
 * @example $("div").children(".selected")
 * @before <div><span>Hello</span><p class="selected">Hello Again</p><p>And Again</p></div>
 * @result [ <p class="selected">Hello Again</p> ]
 * @desc Find all children with a class "selected" of each div.
 *
 * @name children
 * @type jQuery
 * @param String expr (optional) An expression to filter the child Elements with
 * @cat DOM/Traversing
 */
jQuery.each({
	parent: "a.parentNode",
	parents: "jQuery.parents(a)",
	next: "jQuery.nth(a,2,'nextSibling')",
	prev: "jQuery.nth(a,2,'previousSibling')",
	siblings: "jQuery.sibling(a.parentNode.firstChild,a)",
	children: "jQuery.sibling(a.firstChild)"
}, function(i,n){
	jQuery.fn[ i ] = function(a) {
		var ret = jQuery.map(this,n);
		if ( a && typeof a == "string" )
			ret = jQuery.multiFilter(a,ret);
		return this.pushStack( ret );
	};
});

/**
 * Append all of the matched elements to another, specified, set of elements.
 * This operation is, essentially, the reverse of doing a regular
 * $(A).append(B), in that instead of appending B to A, you're appending
 * A to B.
 *
 * @example $("p").appendTo("#foo");
 * @before <p>I would like to say: </p><div id="foo"></div>
 * @result <div id="foo"><p>I would like to say: </p></div>
 * @desc Appends all paragraphs to the element with the ID "foo"
 *
 * @name appendTo
 * @type jQuery
 * @param <Content> content Content to append to the selected element to.
 * @cat DOM/Manipulation
 * @see append(<Content>)
 */

/**
 * Prepend all of the matched elements to another, specified, set of elements.
 * This operation is, essentially, the reverse of doing a regular
 * $(A).prepend(B), in that instead of prepending B to A, you're prepending
 * A to B.
 *
 * @example $("p").prependTo("#foo");
 * @before <p>I would like to say: </p><div id="foo"><b>Hello</b></div>
 * @result <div id="foo"><p>I would like to say: </p><b>Hello</b></div>
 * @desc Prepends all paragraphs to the element with the ID "foo"
 *
 * @name prependTo
 * @type jQuery
 * @param <Content> content Content to prepend to the selected element to.
 * @cat DOM/Manipulation
 * @see prepend(<Content>)
 */

/**
 * Insert all of the matched elements before another, specified, set of elements.
 * This operation is, essentially, the reverse of doing a regular
 * $(A).before(B), in that instead of inserting B before A, you're inserting
 * A before B.
 *
 * @example $("p").insertBefore("#foo");
 * @before <div id="foo">Hello</div><p>I would like to say: </p>
 * @result <p>I would like to say: </p><div id="foo">Hello</div>
 * @desc Same as $("#foo").before("p")
 *
 * @name insertBefore
 * @type jQuery
 * @param <Content> content Content to insert the selected element before.
 * @cat DOM/Manipulation
 * @see before(<Content>)
 */

/**
 * Insert all of the matched elements after another, specified, set of elements.
 * This operation is, essentially, the reverse of doing a regular
 * $(A).after(B), in that instead of inserting B after A, you're inserting
 * A after B.
 *
 * @example $("p").insertAfter("#foo");
 * @before <p>I would like to say: </p><div id="foo">Hello</div>
 * @result <div id="foo">Hello</div><p>I would like to say: </p>
 * @desc Same as $("#foo").after("p")
 *
 * @name insertAfter
 * @type jQuery
 * @param <Content> content Content to insert the selected element after.
 * @cat DOM/Manipulation
 * @see after(<Content>)
 */

jQuery.each({
	appendTo: "append",
	prependTo: "prepend",
	insertBefore: "before",
	insertAfter: "after"
}, function(i,n){
	jQuery.fn[ i ] = function(){
		var a = arguments;
		return this.each(function(){
			for ( var j = 0, al = a.length; j < al; j++ )
				jQuery(a[j])[n]( this );
		});
	};
});

/**
 * Remove an attribute from each of the matched elements.
 *
 * @example $("input").removeAttr("disabled")
 * @before <input disabled="disabled"/>
 * @result <input/>
 *
 * @name removeAttr
 * @type jQuery
 * @param String name The name of the attribute to remove.
 * @cat DOM/Attributes
 */

/**
 * Adds the specified class(es) to each of the set of matched elements.
 *
 * @example $("p").addClass("selected")
 * @before <p>Hello</p>
 * @result [ <p class="selected">Hello</p> ]
 *
 * @example $("p").addClass("selected highlight")
 * @before <p>Hello</p>
 * @result [ <p class="selected highlight">Hello</p> ]
 *
 * @name addClass
 * @type jQuery
 * @param String class One or more CSS classes to add to the elements
 * @cat DOM/Attributes
 * @see removeClass(String)
 */

/**
 * Removes all or the specified class(es) from the set of matched elements.
 *
 * @example $("p").removeClass()
 * @before <p class="selected">Hello</p>
 * @result [ <p>Hello</p> ]
 *
 * @example $("p").removeClass("selected")
 * @before <p class="selected first">Hello</p>
 * @result [ <p class="first">Hello</p> ]
 *
 * @example $("p").removeClass("selected highlight")
 * @before <p class="highlight selected first">Hello</p>
 * @result [ <p class="first">Hello</p> ]
 *
 * @name removeClass
 * @type jQuery
 * @param String class (optional) One or more CSS classes to remove from the elements
 * @cat DOM/Attributes
 * @see addClass(String)
 */

/**
 * Adds the specified class if it is not present, removes it if it is
 * present.
 *
 * @example $("p").toggleClass("selected")
 * @before <p>Hello</p><p class="selected">Hello Again</p>
 * @result [ <p class="selected">Hello</p>, <p>Hello Again</p> ]
 *
 * @name toggleClass
 * @type jQuery
 * @param String class A CSS class with which to toggle the elements
 * @cat DOM/Attributes
 */

/**
 * Removes all matched elements from the DOM. This does NOT remove them from the
 * jQuery object, allowing you to use the matched elements further.
 *
 * Can be filtered with an optional expressions.
 *
 * @example $("p").remove();
 * @before <p>Hello</p> how are <p>you?</p>
 * @result how are
 *
 * @example $("p").remove(".hello");
 * @before <p class="hello">Hello</p> how are <p>you?</p>
 * @result how are <p>you?</p>
 *
 * @name remove
 * @type jQuery
 * @param String expr (optional) A jQuery expression to filter elements by.
 * @cat DOM/Manipulation
 */

/**
 * Removes all child nodes from the set of matched elements.
 *
 * @example $("p").empty()
 * @before <p>Hello, <span>Person</span> <a href="#">and person</a></p>
 * @result [ <p></p> ]
 *
 * @name empty
 * @type jQuery
 * @cat DOM/Manipulation
 */

jQuery.each( {
	removeAttr: function( key ) {
		jQuery.attr( this, key, "" );
		this.removeAttribute( key );
	},
	addClass: function(c){
		jQuery.className.add(this,c);
	},
	removeClass: function(c){
		jQuery.className.remove(this,c);
	},
	toggleClass: function( c ){
		jQuery.className[ jQuery.className.has(this,c) ? "remove" : "add" ](this, c);
	},
	remove: function(a){
		if ( !a || jQuery.filter( a, [this] ).r.length )
			this.parentNode.removeChild( this );
	},
	empty: function() {
		while ( this.firstChild )
			this.removeChild( this.firstChild );
	}
}, function(i,n){
	jQuery.fn[ i ] = function() {
		return this.each( n, arguments );
	};
});

/**
 * Reduce the set of matched elements to a single element.
 * The position of the element in the set of matched elements
 * starts at 0 and goes to length - 1.
 *
 * @example $("p").eq(1)
 * @before <p>This is just a test.</p><p>So is this</p>
 * @result [ <p>So is this</p> ]
 *
 * @name eq
 * @type jQuery
 * @param Number pos The index of the element that you wish to limit to.
 * @cat Core
 */

/**
 * Reduce the set of matched elements to all elements before a given position.
 * The position of the element in the set of matched elements
 * starts at 0 and goes to length - 1.
 *
 * @example $("p").lt(1)
 * @before <p>This is just a test.</p><p>So is this</p>
 * @result [ <p>This is just a test.</p> ]
 *
 * @name lt
 * @type jQuery
 * @param Number pos Reduce the set to all elements below this position.
 * @cat Core
 */

/**
 * Reduce the set of matched elements to all elements after a given position.
 * The position of the element in the set of matched elements
 * starts at 0 and goes to length - 1.
 *
 * @example $("p").gt(0)
 * @before <p>This is just a test.</p><p>So is this</p>
 * @result [ <p>So is this</p> ]
 *
 * @name gt
 * @type jQuery
 * @param Number pos Reduce the set to all elements after this position.
 * @cat Core
 */

/**
 * Filter the set of elements to those that contain the specified text.
 *
 * @example $("p").contains("test")
 * @before <p>This is just a test.</p><p>So is this</p>
 * @result [ <p>This is just a test.</p> ]
 *
 * @name contains
 * @type jQuery
 * @param String str The string that will be contained within the text of an element.
 * @cat DOM/Traversing
 */
jQuery.each( [ "eq", "lt", "gt", "contains" ], function(i,n){
	jQuery.fn[ n ] = function(num,fn) {
		return this.filter( ":" + n + "(" + num + ")", fn );
	};
});

/**
 * Get the current computed, pixel, width of the first matched element.
 *
 * @example $("p").width();
 * @before <p>This is just a test.</p>
 * @result 300
 *
 * @name width
 * @type String
 * @cat CSS
 */

/**
 * Set the CSS width of every matched element. If no explicit unit
 * was specified (like 'em' or '%') then "px" is added to the width.
 *
 * @example $("p").width(20);
 * @before <p>This is just a test.</p>
 * @result <p style="width:20px;">This is just a test.</p>
 *
 * @example $("p").width("20em");
 * @before <p>This is just a test.</p>
 * @result <p style="width:20em;">This is just a test.</p>
 *
 * @name width
 * @type jQuery
 * @param String|Number val Set the CSS property to the specified value.
 * @cat CSS
 */
 
/**
 * Get the current computed, pixel, height of the first matched element.
 *
 * @example $("p").height();
 * @before <p>This is just a test.</p>
 * @result 300
 *
 * @name height
 * @type String
 * @cat CSS
 */

/**
 * Set the CSS height of every matched element. If no explicit unit
 * was specified (like 'em' or '%') then "px" is added to the width.
 *
 * @example $("p").height(20);
 * @before <p>This is just a test.</p>
 * @result <p style="height:20px;">This is just a test.</p>
 *
 * @example $("p").height("20em");
 * @before <p>This is just a test.</p>
 * @result <p style="height:20em;">This is just a test.</p>
 *
 * @name height
 * @type jQuery
 * @param String|Number val Set the CSS property to the specified value.
 * @cat CSS
 */

jQuery.each( [ "height", "width" ], function(i,n){
	jQuery.fn[ n ] = function(h) {
		return h == undefined ?
			( this.length ? jQuery.css( this[0], n ) : null ) :
			this.css( n, h.constructor == String ? h : h + "px" );
	};
});
jQuery.extend({
	expr: {
		"": "m[2]=='*'||jQuery.nodeName(a,m[2])",
		"#": "a.getAttribute('id')==m[2]",
		":": {
			// Position Checks
			lt: "i<m[3]-0",
			gt: "i>m[3]-0",
			nth: "m[3]-0==i",
			eq: "m[3]-0==i",
			first: "i==0",
			last: "i==r.length-1",
			even: "i%2==0",
			odd: "i%2",

			// Child Checks
			"nth-child": "jQuery.nth(a.parentNode.firstChild,m[3],'nextSibling',a)==a",
			"first-child": "jQuery.nth(a.parentNode.firstChild,1,'nextSibling')==a",
			"last-child": "jQuery.nth(a.parentNode.lastChild,1,'previousSibling')==a",
			"only-child": "jQuery.sibling(a.parentNode.firstChild).length==1",

			// Parent Checks
			parent: "a.firstChild",
			empty: "!a.firstChild",

			// Text Check
			contains: "jQuery.fn.text.apply([a]).indexOf(m[3])>=0",

			// Visibility
			visible: '"hidden"!=a.type&&jQuery.css(a,"display")!="none"&&jQuery.css(a,"visibility")!="hidden"',
			hidden: '"hidden"==a.type||jQuery.css(a,"display")=="none"||jQuery.css(a,"visibility")=="hidden"',

			// Form attributes
			enabled: "!a.disabled",
			disabled: "a.disabled",
			checked: "a.checked",
			selected: "a.selected||jQuery.attr(a,'selected')",

			// Form elements
			text: "'text'==a.type",
			radio: "'radio'==a.type",
			checkbox: "'checkbox'==a.type",
			file: "'file'==a.type",
			password: "'password'==a.type",
			submit: "'submit'==a.type",
			image: "'image'==a.type",
			reset: "'reset'==a.type",
			button: '"button"==a.type||jQuery.nodeName(a,"button")',
			input: "/input|select|textarea|button/i.test(a.nodeName)"
		},
		".": "jQuery.className.has(a,m[2])",
		"@": {
			"=": "z==m[4]",
			"!=": "z!=m[4]",
			"^=": "z&&!z.indexOf(m[4])",
			"$=": "z&&z.substr(z.length - m[4].length,m[4].length)==m[4]",
			"*=": "z&&z.indexOf(m[4])>=0",
			"": "z",
			_resort: function(m){
				return ["", m[1], m[3], m[2], m[5]];
			},
			_prefix: "var z=a[m[3]];if(!z||/href|src/.test(m[3]))z=jQuery.attr(a,m[3]);"
		},
		"[": "jQuery.find(m[2],a).length"
	},
	
	// The regular expressions that power the parsing engine
	parse: [
		// Match: [@value='test'], [@foo]
		/^\[ *(@)([\w-]+) *([!*$^=]*) *('?"?)(.*?)\4 *\]/,

		// Match: [div], [div p]
		/^(\[)\s*(.*?(\[.*?\])?[^[]*?)\s*\]/,

		// Match: :contains('foo')
		/^(:)([\w-]+)\("?'?(.*?(\(.*?\))?[^(]*?)"?'?\)/,

		// Match: :even, :last-chlid, #id, .class
		new RegExp("^([:.#]*)(" + 
			( jQuery.chars = "(?:[\\w\u0128-\uFFFF*_-]|\\\\.)" ) + "+)")
	],

	token: [
		/^(\/?\.\.)/, "a.parentNode",
		/^(>|\/)/, "jQuery.sibling(a.firstChild)",
		/^(\+)/, "jQuery.nth(a,2,'nextSibling')",
		/^(~)/, function(a){
			var s = jQuery.sibling(a.parentNode.firstChild);
			return s.slice(jQuery.inArray(a,s) + 1);
		}
	],

	multiFilter: function( expr, elems, not ) {
		var old, cur = [];

		while ( expr && expr != old ) {
			old = expr;
			var f = jQuery.filter( expr, elems, not );
			expr = f.t.replace(/^\s*,\s*/, "" );
			cur = not ? elems = f.r : jQuery.merge( cur, f.r );
		}

		return cur;
	},

	/**
	 * @name $.find
	 * @type Array<Element>
	 * @private
	 * @cat Core
	 */
	find: function( t, context ) {
		// Quickly handle non-string expressions
		if ( typeof t != "string" )
			return [ t ];

		// Make sure that the context is a DOM Element
		if ( context && !context.nodeType )
			context = null;

		// Set the correct context (if none is provided)
		context = context || document;

		// Handle the common XPath // expression
		if ( !t.indexOf("//") ) {
			context = context.documentElement;
			t = t.substr(2,t.length);

		// And the / root expression
		} else if ( !t.indexOf("/") && !context.ownerDocument ) {
			context = context.documentElement;
			t = t.substr(1,t.length);
			if ( t.indexOf("/") >= 1 )
				t = t.substr(t.indexOf("/"),t.length);
		}

		// Initialize the search
		var ret = [context], done = [], last;

		// Continue while a selector expression exists, and while
		// we're no longer looping upon ourselves
		while ( t && last != t ) {
			var r = [];
			last = t;

			t = jQuery.trim(t).replace( /^\/\//, "" );

			var foundToken = false;

			// An attempt at speeding up child selectors that
			// point to a specific element tag
			var re = new RegExp("^[/>]\\s*(" + jQuery.chars + "+)");
			var m = re.exec(t);

			if ( m ) {
				// Perform our own iteration and filter
				for ( var i = 0; ret[i]; i++ )
					for ( var c = ret[i].firstChild; c; c = c.nextSibling )
						if ( c.nodeType == 1 && ( m[1] == "*" || jQuery.nodeName(c, m[1]) ) )
							r.push( c );

				ret = r;
				t = t.replace( re, "" );
				if ( t.indexOf(" ") == 0 ) continue;
				foundToken = true;
			} else {
				// Look for pre-defined expression tokens
				for ( var i = 0, tl = jQuery.token.length; i < tl; i += 2 ) {
					// Attempt to match each, individual, token in
					// the specified order
					var re = jQuery.token[i], fn = jQuery.token[i+1];
					var m = re.exec(t);

					// If the token match was found
					if ( m ) {
						// Map it against the token's handler
						r = ret = jQuery.map( ret, jQuery.isFunction( fn ) ?
							fn : new Function( "a", "return " + fn ) );

						// And remove the token
						t = jQuery.trim( t.replace( re, "" ) );
						foundToken = true;
						break;
					}
				}
			}

			// See if there's still an expression, and that we haven't already
			// matched a token
			if ( t && !foundToken ) {
				// Handle multiple expressions
				if ( !t.indexOf(",") ) {
					// Clean the result set
					if ( context == ret[0] ) ret.shift();

					// Merge the result sets
					done = jQuery.merge( done, ret );

					// Reset the context
					r = ret = [context];

					// Touch up the selector string
					t = " " + t.substr(1,t.length);

				} else {
					// Optomize for the case nodeName#idName
					var re2 = new RegExp("^(" + jQuery.chars + "+)(#)(" + jQuery.chars + "+)");
					var m = re2.exec(t);
					
					// Re-organize the results, so that they're consistent
					if ( m ) {
					   m = [ 0, m[2], m[3], m[1] ];

					} else {
						// Otherwise, do a traditional filter check for
						// ID, class, and element selectors
						re2 = new RegExp("^([#.]?)(" + jQuery.chars + "*)");
						m = re2.exec(t);
					}

					m[2] = m[2].replace(/\\/g, "");

					var elem = ret[ret.length-1];

					// Try to do a global search by ID, where we can
					if ( m[1] == "#" && elem && elem.getElementById ) {
						// Optimization for HTML document case
						var oid = elem.getElementById(m[2]);
						
						// Do a quick check for the existence of the actual ID attribute
						// to avoid selecting by the name attribute in IE
						// also check to insure id is a string to avoid selecting an element with the name of 'id' inside a form
						if ( (jQuery.browser.msie||jQuery.browser.opera) && oid && typeof oid.id == "string" && oid.id != m[2] )
							oid = jQuery('[@id="'+m[2]+'"]', elem)[0];

						// Do a quick check for node name (where applicable) so
						// that div#foo searches will be really fast
						ret = r = oid && (!m[3] || jQuery.nodeName(oid, m[3])) ? [oid] : [];
					} else {
						// We need to find all descendant elements
						for ( var i = 0; ret[i]; i++ ) {
							// Grab the tag name being searched for
							var tag = m[1] != "" || m[0] == "" ? "*" : m[2];

							// Handle IE7 being really dumb about <object>s
							if ( tag == "*" && ret[i].nodeName.toLowerCase() == "object" )
								tag = "param";

							r = jQuery.merge( r, ret[i].getElementsByTagName( tag ));
						}

						// It's faster to filter by class and be done with it
						if ( m[1] == "." )
							r = jQuery.classFilter( r, m[2] );

						// Same with ID filtering
						if ( m[1] == "#" ) {
							var tmp = [];

							// Try to find the element with the ID
							for ( var i = 0; r[i]; i++ )
								if ( r[i].getAttribute("id") == m[2] ) {
									tmp = [ r[i] ];
									break;
								}

							r = tmp;
						}

						ret = r;
					}

					t = t.replace( re2, "" );
				}

			}

			// If a selector string still exists
			if ( t ) {
				// Attempt to filter it
				var val = jQuery.filter(t,r);
				ret = r = val.r;
				t = jQuery.trim(val.t);
			}
		}

		// An error occurred with the selector;
		// just return an empty set instead
		if ( t )
			ret = [];

		// Remove the root context
		if ( ret && context == ret[0] )
			ret.shift();

		// And combine the results
		done = jQuery.merge( done, ret );

		return done;
	},

	classFilter: function(r,m,not){
		m = " " + m + " ";
		var tmp = [];
		for ( var i = 0; r[i]; i++ ) {
			var pass = (" " + r[i].className + " ").indexOf( m ) >= 0;
			if ( !not && pass || not && !pass )
				tmp.push( r[i] );
		}
		return tmp;
	},

	filter: function(t,r,not) {
		var last;

		// Look for common filter expressions
		while ( t  && t != last ) {
			last = t;

			var p = jQuery.parse, m;

			for ( var i = 0; p[i]; i++ ) {
				m = p[i].exec( t );

				if ( m ) {
					// Remove what we just matched
					t = t.substring( m[0].length );

					// Re-organize the first match
					if ( jQuery.expr[ m[1] ]._resort )
						m = jQuery.expr[ m[1] ]._resort( m );

					m[2] = m[2].replace(/\\/g, "");

					break;
				}
			}

			if ( !m )
				break;

			// :not() is a special case that can be optimized by
			// keeping it out of the expression list
			if ( m[1] == ":" && m[2] == "not" )
				r = jQuery.filter(m[3], r, true).r;

			// We can get a big speed boost by filtering by class here
			else if ( m[1] == "." )
				r = jQuery.classFilter(r, m[2], not);

			// Otherwise, find the expression to execute
			else {
				var f = jQuery.expr[m[1]];
				if ( typeof f != "string" )
					f = jQuery.expr[m[1]][m[2]];

				// Build a custom macro to enclose it
				eval("f = function(a,i){" +
					( jQuery.expr[ m[1] ]._prefix || "" ) +
					"return " + f + "}");

				// Execute it against the current filter
				r = jQuery.grep( r, f, not );
			}
		}

		// Return an array of filtered elements (r)
		// and the modified expression string (t)
		return { r: r, t: t };
	},

	/**
	 * All ancestors of a given element.
	 *
	 * @private
	 * @name $.parents
	 * @type Array<Element>
	 * @param Element elem The element to find the ancestors of.
	 * @cat DOM/Traversing
	 */
	parents: function( elem ){
		var matched = [];
		var cur = elem.parentNode;
		while ( cur && cur != document ) {
			matched.push( cur );
			cur = cur.parentNode;
		}
		return matched;
	},
	
	/**
	 * A handy, and fast, way to traverse in a particular direction and find
	 * a specific element.
	 *
	 * @private
	 * @name $.nth
	 * @type DOMElement
	 * @param DOMElement cur The element to search from.
	 * @param String|Number num The Nth result to match. Can be a number or a string (like 'even' or 'odd').
	 * @param String dir The direction to move in (pass in something like 'previousSibling' or 'nextSibling').
	 * @cat DOM/Traversing
	 */
	nth: function(cur,result,dir,elem){
		result = result || 1;
		var num = 0;
		for ( ; cur; cur = cur[dir] ) {
			if ( cur.nodeType == 1 ) num++;
			if ( num == result || result == "even" && num % 2 == 0 && num > 1 && cur == elem ||
				result == "odd" && num % 2 == 1 && cur == elem ) break;
		}
		return cur;
	},
	
	/**
	 * All elements on a specified axis.
	 *
	 * @private
	 * @name $.sibling
	 * @type Array
	 * @param Element elem The element to find all the siblings of (including itself).
	 * @cat DOM/Traversing
	 */
	sibling: function( n, elem ) {
		var r = [];

		for ( ; n; n = n.nextSibling ) {
			if ( n.nodeType == 1 && (!elem || n != elem) )
				r.push( n );
		}

		return r;
	}
});
/*
 * A number of helper functions used for managing events.
 * Many of the ideas behind this code orignated from 
 * Dean Edwards' addEvent library.
 */
jQuery.event = {

	// Bind an event to an element
	// Original by Dean Edwards
	add: function(element, type, handler, data) {
		// For whatever reason, IE has trouble passing the window object
		// around, causing it to be cloned in the process
		if ( jQuery.browser.msie && element.setInterval != undefined )
			element = window;
		
		// Make sure that the function being executed has a unique ID
		if ( !handler.guid )
			handler.guid = this.guid++;
			
		// if data is passed, bind to handler 
		if( data != undefined ) { 
        	// Create temporary function pointer to original handler 
			var fn = handler; 

			// Create unique handler function, wrapped around original handler 
			handler = function() { 
				// Pass arguments and context to original handler 
				return fn.apply(this, arguments); 
			};

			// Store data in unique handler 
			handler.data = data;

			// Set the guid of unique handler to the same of original handler, so it can be removed 
			handler.guid = fn.guid;
		}

		// Init the element's event structure
		if (!element.$events)
			element.$events = {};
		
		if (!element.$handle)
			element.$handle = function() {
				// returned undefined or false
				var val;

				// Handle the second event of a trigger and when
				// an event is called after a page has unloaded
				if ( typeof jQuery == "undefined" || jQuery.event.triggered )
				  return val;
				
				val = jQuery.event.handle.apply(element, arguments);
				
				return val;
			};

		// Get the current list of functions bound to this event
		var handlers = element.$events[type];

		// Init the event handler queue
		if (!handlers) {
			handlers = element.$events[type] = {};	
			
			// And bind the global event handler to the element
			if (element.addEventListener)
				element.addEventListener(type, element.$handle, false);
			else if (element.attachEvent)
				element.attachEvent("on" + type, element.$handle);
		}

		// Add the function to the element's handler list
		handlers[handler.guid] = handler;

		// Remember the function in a global list (for triggering)
		if (!this.global[type])
			this.global[type] = [];
		// Only add the element to the global list once
		if (jQuery.inArray(element, this.global[type]) == -1)
			this.global[type].push( element );
	},

	guid: 1,
	global: {},

	// Detach an event or set of events from an element
	remove: function(element, type, handler) {
		var events = element.$events, ret, index;

		if ( events ) {
			// type is actually an event object here
			if ( type && type.type ) {
				handler = type.handler;
				type = type.type;
			}
			
			if ( !type ) {
				for ( type in events )
					this.remove( element, type );

			} else if ( events[type] ) {
				// remove the given handler for the given type
				if ( handler )
					delete events[type][handler.guid];
				
				// remove all handlers for the given type
				else
					for ( handler in element.$events[type] )
						delete events[type][handler];

				// remove generic event handler if no more handlers exist
				for ( ret in events[type] ) break;
				if ( !ret ) {
					if (element.removeEventListener)
						element.removeEventListener(type, element.$handle, false);
					else if (element.detachEvent)
						element.detachEvent("on" + type, element.$handle);
					ret = null;
					delete events[type];
					
					// Remove element from the global event type cache
					while ( this.global[type] && ( (index = jQuery.inArray(element, this.global[type])) >= 0 ) )
						delete this.global[type][index];
				}
			}

			// Remove the expando if it's no longer used
			for ( ret in events ) break;
			if ( !ret )
				element.$handle = element.$events = null;
		}
	},

	trigger: function(type, data, element) {
		// Clone the incoming data, if any
		data = jQuery.makeArray(data || []);

		// Handle a global trigger
		if ( !element )
			jQuery.each( this.global[type] || [], function(){
				jQuery.event.trigger( type, data, this );
			});

		// Handle triggering a single element
		else {
			var val, ret, fn = jQuery.isFunction( element[ type ] || null );
			
			// Pass along a fake event
			data.unshift( this.fix({ type: type, target: element }) );

			// Trigger the event
			if ( jQuery.isFunction(element.$handle) && (val = element.$handle.apply( element, data )) !== false )
				this.triggered = true;

			if ( fn && val !== false && !jQuery.nodeName(element, 'a') )
				element[ type ]();

			this.triggered = false;
		}
	},

	handle: function(event) {
		// returned undefined or false
		var val;

		// Empty object is for triggered events with no data
		event = jQuery.event.fix( event || window.event || {} ); 

		var c = this.$events && this.$events[event.type], args = [].slice.call( arguments, 1 );
		args.unshift( event );

		for ( var j in c ) {
			// Pass in a reference to the handler function itself
			// So that we can later remove it
			args[0].handler = c[j];
			args[0].data = c[j].data;

			if ( c[j].apply( this, args ) === false ) {
				event.preventDefault();
				event.stopPropagation();
				val = false;
			}
		}

		// Clean up added properties in IE to prevent memory leak
		if (jQuery.browser.msie)
			event.target = event.preventDefault = event.stopPropagation =
				event.handler = event.data = null;

		return val;
	},

	fix: function(event) {
		// store a copy of the original event object 
		// and clone to set read-only properties
		var originalEvent = event;
		event = jQuery.extend({}, originalEvent);
		
		// add preventDefault and stopPropagation since 
		// they will not work on the clone
		event.preventDefault = function() {
			// if preventDefault exists run it on the original event
			if (originalEvent.preventDefault)
				return originalEvent.preventDefault();
			// otherwise set the returnValue property of the original event to false (IE)
			originalEvent.returnValue = false;
		};
		event.stopPropagation = function() {
			// if stopPropagation exists run it on the original event
			if (originalEvent.stopPropagation)
				return originalEvent.stopPropagation();
			// otherwise set the cancelBubble property of the original event to true (IE)
			originalEvent.cancelBubble = true;
		};
		
		// Fix target property, if necessary
		if ( !event.target && event.srcElement )
			event.target = event.srcElement;
				
		// check if target is a textnode (safari)
		if (jQuery.browser.safari && event.target.nodeType == 3)
			event.target = originalEvent.target.parentNode;

		// Add relatedTarget, if necessary
		if ( !event.relatedTarget && event.fromElement )
			event.relatedTarget = event.fromElement == event.target ? event.toElement : event.fromElement;

		// Calculate pageX/Y if missing and clientX/Y available
		if ( event.pageX == null && event.clientX != null ) {
			var e = document.documentElement || document.body;
			event.pageX = event.clientX + e.scrollLeft;
			event.pageY = event.clientY + e.scrollTop;
		}
			
		// Add which for key events
		if ( !event.which && (event.charCode || event.keyCode) )
			event.which = event.charCode || event.keyCode;
		
		// Add metaKey to non-Mac browsers (use ctrl for PC's and Meta for Macs)
		if ( !event.metaKey && event.ctrlKey )
			event.metaKey = event.ctrlKey;

		// Add which for click: 1 == left; 2 == middle; 3 == right
		// Note: button is not normalized, so don't use it
		if ( !event.which && event.button )
			event.which = (event.button & 1 ? 1 : ( event.button & 2 ? 3 : ( event.button & 4 ? 2 : 0 ) ));
			
		return event;
	}
};

jQuery.fn.extend({

	/**
	 * Binds a handler to a particular event (like click) for each matched element.
	 * The event handler is passed an event object that you can use to prevent
	 * default behaviour. To stop both default action and event bubbling, your handler
	 * has to return false.
	 *
	 * In most cases, you can define your event handlers as anonymous functions
	 * (see first example). In cases where that is not possible, you can pass additional
	 * data as the second parameter (and the handler function as the third), see 
	 * second example.
	 *
	 * Calling bind with an event type of "unload" will automatically
	 * use the one method instead of bind to prevent memory leaks.
	 *
	 * @example $("p").bind("click", function(){
	 *   alert( $(this).text() );
	 * });
	 * @before <p>Hello</p>
	 * @result alert("Hello")
	 *
	 * @example function handler(event) {
	 *   alert(event.data.foo);
	 * }
	 * $("p").bind("click", {foo: "bar"}, handler)
	 * @result alert("bar")
	 * @desc Pass some additional data to the event handler.
	 *
	 * @example $("form").bind("submit", function() { return false; })
	 * @desc Cancel a default action and prevent it from bubbling by returning false
	 * from your function.
	 *
	 * @example $("form").bind("submit", function(event){
	 *   event.preventDefault();
	 * });
	 * @desc Cancel only the default action by using the preventDefault method.
	 *
	 *
	 * @example $("form").bind("submit", function(event){
	 *   event.stopPropagation();
	 * });
	 * @desc Stop only an event from bubbling by using the stopPropagation method.
	 *
	 * @name bind
	 * @type jQuery
	 * @param String type An event type
	 * @param Object data (optional) Additional data passed to the event handler as event.data
	 * @param Function fn A function to bind to the event on each of the set of matched elements
	 * @cat Events
	 */
	bind: function( type, data, fn ) {
		return type == "unload" ? this.one(type, data, fn) : this.each(function(){
			jQuery.event.add( this, type, fn || data, fn && data );
		});
	},
	
	/**
	 * Binds a handler to a particular event (like click) for each matched element.
	 * The handler is executed only once for each element. Otherwise, the same rules
	 * as described in bind() apply.
	 * The event handler is passed an event object that you can use to prevent
	 * default behaviour. To stop both default action and event bubbling, your handler
	 * has to return false.
	 *
	 * In most cases, you can define your event handlers as anonymous functions
	 * (see first example). In cases where that is not possible, you can pass additional
	 * data as the second paramter (and the handler function as the third), see 
	 * second example.
	 *
	 * @example $("p").one("click", function(){
	 *   alert( $(this).text() );
	 * });
	 * @before <p>Hello</p>
	 * @result alert("Hello")
	 *
	 * @name one
	 * @type jQuery
	 * @param String type An event type
	 * @param Object data (optional) Additional data passed to the event handler as event.data
	 * @param Function fn A function to bind to the event on each of the set of matched elements
	 * @cat Events
	 */
	one: function( type, data, fn ) {
		return this.each(function(){
			jQuery.event.add( this, type, function(event) {
				jQuery(this).unbind(event);
				return (fn || data).apply( this, arguments);
			}, fn && data);
		});
	},

	/**
	 * The opposite of bind, removes a bound event from each of the matched
	 * elements.
	 *
	 * Without any arguments, all bound events are removed.
	 *
	 * If the type is provided, all bound events of that type are removed.
	 *
	 * If the function that was passed to bind is provided as the second argument,
	 * only that specific event handler is removed.
	 *
	 * @example $("p").unbind()
	 * @before <p onclick="alert('Hello');">Hello</p>
	 * @result [ <p>Hello</p> ]
	 *
	 * @example $("p").unbind( "click" )
	 * @before <p onclick="alert('Hello');">Hello</p>
	 * @result [ <p>Hello</p> ]
	 *
	 * @example $("p").unbind( "click", function() { alert("Hello"); } )
	 * @before <p onclick="alert('Hello');">Hello</p>
	 * @result [ <p>Hello</p> ]
	 *
	 * @name unbind
	 * @type jQuery
	 * @param String type (optional) An event type
	 * @param Function fn (optional) A function to unbind from the event on each of the set of matched elements
	 * @cat Events
	 */
	unbind: function( type, fn ) {
		return this.each(function(){
			jQuery.event.remove( this, type, fn );
		});
	},

	/**
	 * Trigger a type of event on every matched element. This will also cause
	 * the default action of the browser with the same name (if one exists)
	 * to be executed. For example, passing 'submit' to the trigger()
	 * function will also cause the browser to submit the form. This
	 * default action can be prevented by returning false from one of
	 * the functions bound to the event.
	 *
	 * You can also trigger custom events registered with bind.
	 *
	 * @example $("p").trigger("click")
	 * @before <p click="alert('hello')">Hello</p>
	 * @result alert('hello')
	 *
	 * @example $("p").click(function(event, a, b) {
	 *   // when a normal click fires, a and b are undefined
	 *   // for a trigger like below a refers too "foo" and b refers to "bar"
	 * }).trigger("click", ["foo", "bar"]);
	 * @desc Example of how to pass arbitrary data to an event
	 * 
	 * @example $("p").bind("myEvent",function(event,message1,message2) {
	 * 	alert(message1 + ' ' + message2);
	 * });
	 * $("p").trigger("myEvent",["Hello","World"]);
	 * @result alert('Hello World') // One for each paragraph
	 *
	 * @name trigger
	 * @type jQuery
	 * @param String type An event type to trigger.
	 * @param Array data (optional) Additional data to pass as arguments (after the event object) to the event handler
	 * @cat Events
	 */
	trigger: function( type, data ) {
		return this.each(function(){
			jQuery.event.trigger( type, data, this );
		});
	},

	/**
	 * Toggle between two function calls every other click.
	 * Whenever a matched element is clicked, the first specified function 
	 * is fired, when clicked again, the second is fired. All subsequent 
	 * clicks continue to rotate through the two functions.
	 *
	 * Use unbind("click") to remove.
	 *
	 * @example $("p").toggle(function(){
	 *   $(this).addClass("selected");
	 * },function(){
	 *   $(this).removeClass("selected");
	 * });
	 * 
	 * @name toggle
	 * @type jQuery
	 * @param Function even The function to execute on every even click.
	 * @param Function odd The function to execute on every odd click.
	 * @cat Events
	 */
	toggle: function() {
		// Save reference to arguments for access in closure
		var a = arguments;

		return this.click(function(e) {
			// Figure out which function to execute
			this.lastToggle = 0 == this.lastToggle ? 1 : 0;
			
			// Make sure that clicks stop
			e.preventDefault();
			
			// and execute the function
			return a[this.lastToggle].apply( this, [e] ) || false;
		});
	},
	
	/**
	 * A method for simulating hovering (moving the mouse on, and off,
	 * an object). This is a custom method which provides an 'in' to a 
	 * frequent task.
	 *
	 * Whenever the mouse cursor is moved over a matched 
	 * element, the first specified function is fired. Whenever the mouse 
	 * moves off of the element, the second specified function fires. 
	 * Additionally, checks are in place to see if the mouse is still within 
	 * the specified element itself (for example, an image inside of a div), 
	 * and if it is, it will continue to 'hover', and not move out 
	 * (a common error in using a mouseout event handler).
	 *
	 * @example $("p").hover(function(){
	 *   $(this).addClass("hover");
	 * },function(){
	 *   $(this).removeClass("hover");
	 * });
	 *
	 * @name hover
	 * @type jQuery
	 * @param Function over The function to fire whenever the mouse is moved over a matched element.
	 * @param Function out The function to fire whenever the mouse is moved off of a matched element.
	 * @cat Events
	 */
	hover: function(f,g) {
		
		// A private function for handling mouse 'hovering'
		function handleHover(e) {
			// Check if mouse(over|out) are still within the same parent element
			var p = e.relatedTarget;
	
			// Traverse up the tree
			while ( p && p != this ) try { p = p.parentNode } catch(e) { p = this; };
			
			// If we actually just moused on to a sub-element, ignore it
			if ( p == this ) return false;
			
			// Execute the right function
			return (e.type == "mouseover" ? f : g).apply(this, [e]);
		}
		
		// Bind the function to the two event listeners
		return this.mouseover(handleHover).mouseout(handleHover);
	},
	
	/**
	 * Bind a function to be executed whenever the DOM is ready to be
	 * traversed and manipulated. This is probably the most important 
	 * function included in the event module, as it can greatly improve
	 * the response times of your web applications.
	 *
	 * In a nutshell, this is a solid replacement for using window.onload, 
	 * and attaching a function to that. By using this method, your bound function 
	 * will be called the instant the DOM is ready to be read and manipulated, 
	 * which is when what 99.99% of all JavaScript code needs to run.
	 *
	 * There is one argument passed to the ready event handler: A reference to
	 * the jQuery function. You can name that argument whatever you like, and
	 * can therefore stick with the $ alias without risk of naming collisions.
	 * 
	 * Please ensure you have no code in your &lt;body&gt; onload event handler, 
	 * otherwise $(document).ready() may not fire.
	 *
	 * You can have as many $(document).ready events on your page as you like.
	 * The functions are then executed in the order they were added.
	 *
	 * @example $(document).ready(function(){ Your code here... });
	 *
	 * @example jQuery(function($) {
	 *   // Your code using failsafe $ alias here...
	 * });
	 * @desc Uses both the [[Core#.24.28_fn_.29|shortcut]] for $(document).ready() and the argument
	 * to write failsafe jQuery code using the $ alias, without relying on the
	 * global alias.
	 *
	 * @name ready
	 * @type jQuery
	 * @param Function fn The function to be executed when the DOM is ready.
	 * @cat Events
	 * @see $.noConflict()
	 * @see $(Function)
	 */
	ready: function(f) {
		// If the DOM is already ready
		if ( jQuery.isReady )
			// Execute the function immediately
			f.apply( document, [jQuery] );
			
		// Otherwise, remember the function for later
		else {
			// Add the function to the wait list
			jQuery.readyList.push( function() { return f.apply(this, [jQuery]) } );
		}
	
		return this;
	}
});

jQuery.extend({
	/*
	 * All the code that makes DOM Ready work nicely.
	 */
	isReady: false,
	readyList: [],
	
	// Handle when the DOM is ready
	ready: function() {
		// Make sure that the DOM is not already loaded
		if ( !jQuery.isReady ) {
			// Remember that the DOM is ready
			jQuery.isReady = true;
			
			// If there are functions bound, to execute
			if ( jQuery.readyList ) {
				// Execute all of them
				jQuery.each( jQuery.readyList, function(){
					this.apply( document );
				});
				
				// Reset the list of functions
				jQuery.readyList = null;
			}
			// Remove event listener to avoid memory leak
			if ( jQuery.browser.mozilla || jQuery.browser.opera )
				document.removeEventListener( "DOMContentLoaded", jQuery.ready, false );
			
			// Remove script element used by IE hack
			if( !window.frames.length ) // don't remove if frames are present (#1187)
				jQuery(window).load(function(){ jQuery("#__ie_init").remove(); });
		}
	}
});

new function(){

	/**
	 * Bind a function to the scroll event of each matched element.
	 *
	 * @example $("p").scroll( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onscroll="alert('Hello');">Hello</p>
	 *
	 * @name scroll
	 * @type jQuery
	 * @param Function fn A function to bind to the scroll event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the submit event of each matched element.
	 *
	 * @example $("#myform").submit( function() {
	 *   return $("input", this).val().length > 0;
	 * } );
	 * @before <form id="myform"><input /></form>
	 * @desc Prevents the form submission when the input has no value entered.
	 *
	 * @name submit
	 * @type jQuery
	 * @param Function fn A function to bind to the submit event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Trigger the submit event of each matched element. This causes all of the functions
	 * that have been bound to that submit event to be executed, and calls the browser's
	 * default submit action on the matching element(s). This default action can be prevented
	 * by returning false from one of the functions bound to the submit event.
	 *
	 * Note: This does not execute the submit method of the form element! If you need to
	 * submit the form via code, you have to use the DOM method, eg. $("form")[0].submit();
	 *
	 * @example $("form").submit();
	 * @desc Triggers all submit events registered to the matched form(s), and submits them.
	 *
	 * @name submit
	 * @type jQuery
	 * @cat Events
	 */

	/**
	 * Bind a function to the focus event of each matched element.
	 *
	 * @example $("p").focus( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onfocus="alert('Hello');">Hello</p>
	 *
	 * @name focus
	 * @type jQuery
	 * @param Function fn A function to bind to the focus event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Trigger the focus event of each matched element. This causes all of the functions
	 * that have been bound to thet focus event to be executed.
	 *
	 * Note: This does not execute the focus method of the underlying elements! If you need to
	 * focus an element via code, you have to use the DOM method, eg. $("#myinput")[0].focus();
	 *
	 * @example $("p").focus();
	 * @before <p onfocus="alert('Hello');">Hello</p>
	 * @result alert('Hello');
	 *
	 * @name focus
	 * @type jQuery
	 * @cat Events
	 */

	/**
	 * Bind a function to the keydown event of each matched element.
	 *
	 * @example $("p").keydown( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onkeydown="alert('Hello');">Hello</p>
	 *
	 * @name keydown
	 * @type jQuery
	 * @param Function fn A function to bind to the keydown event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the dblclick event of each matched element.
	 *
	 * @example $("p").dblclick( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p ondblclick="alert('Hello');">Hello</p>
	 *
	 * @name dblclick
	 * @type jQuery
	 * @param Function fn A function to bind to the dblclick event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the keypress event of each matched element.
	 *
	 * @example $("p").keypress( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onkeypress="alert('Hello');">Hello</p>
	 *
	 * @name keypress
	 * @type jQuery
	 * @param Function fn A function to bind to the keypress event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the error event of each matched element.
	 *
	 * @example $("p").error( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onerror="alert('Hello');">Hello</p>
	 *
	 * @name error
	 * @type jQuery
	 * @param Function fn A function to bind to the error event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the blur event of each matched element.
	 *
	 * @example $("p").blur( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onblur="alert('Hello');">Hello</p>
	 *
	 * @name blur
	 * @type jQuery
	 * @param Function fn A function to bind to the blur event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Trigger the blur event of each matched element. This causes all of the functions
	 * that have been bound to that blur event to be executed, and calls the browser's
	 * default blur action on the matching element(s). This default action can be prevented
	 * by returning false from one of the functions bound to the blur event.
	 *
	 * Note: This does not execute the blur method of the underlying elements! If you need to
	 * blur an element via code, you have to use the DOM method, eg. $("#myinput")[0].blur();
	 *
	 * @example $("p").blur();
	 * @before <p onblur="alert('Hello');">Hello</p>
	 * @result alert('Hello');
	 *
	 * @name blur
	 * @type jQuery
	 * @cat Events
	 */

	/**
	 * Bind a function to the load event of each matched element.
	 *
	 * @example $("p").load( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onload="alert('Hello');">Hello</p>
	 *
	 * @name load
	 * @type jQuery
	 * @param Function fn A function to bind to the load event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the select event of each matched element.
	 *
	 * @example $("p").select( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onselect="alert('Hello');">Hello</p>
	 *
	 * @name select
	 * @type jQuery
	 * @param Function fn A function to bind to the select event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Trigger the select event of each matched element. This causes all of the functions
	 * that have been bound to that select event to be executed, and calls the browser's
	 * default select action on the matching element(s). This default action can be prevented
	 * by returning false from one of the functions bound to the select event.
	 *
	 * @example $("p").select();
	 * @before <p onselect="alert('Hello');">Hello</p>
	 * @result alert('Hello');
	 *
	 * @name select
	 * @type jQuery
	 * @cat Events
	 */

	/**
	 * Bind a function to the mouseup event of each matched element.
	 *
	 * @example $("p").mouseup( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onmouseup="alert('Hello');">Hello</p>
	 *
	 * @name mouseup
	 * @type jQuery
	 * @param Function fn A function to bind to the mouseup event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the unload event of each matched element.
	 *
	 * @example $("p").unload( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onunload="alert('Hello');">Hello</p>
	 *
	 * @name unload
	 * @type jQuery
	 * @param Function fn A function to bind to the unload event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the change event of each matched element.
	 *
	 * @example $("p").change( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onchange="alert('Hello');">Hello</p>
	 *
	 * @name change
	 * @type jQuery
	 * @param Function fn A function to bind to the change event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the mouseout event of each matched element.
	 *
	 * @example $("p").mouseout( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onmouseout="alert('Hello');">Hello</p>
	 *
	 * @name mouseout
	 * @type jQuery
	 * @param Function fn A function to bind to the mouseout event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the keyup event of each matched element.
	 *
	 * @example $("p").keyup( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onkeyup="alert('Hello');">Hello</p>
	 *
	 * @name keyup
	 * @type jQuery
	 * @param Function fn A function to bind to the keyup event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the click event of each matched element.
	 *
	 * @example $("p").click( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onclick="alert('Hello');">Hello</p>
	 *
	 * @name click
	 * @type jQuery
	 * @param Function fn A function to bind to the click event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Trigger the click event of each matched element. This causes all of the functions
	 * that have been bound to thet click event to be executed.
	 *
	 * @example $("p").click();
	 * @before <p onclick="alert('Hello');">Hello</p>
	 * @result alert('Hello');
	 *
	 * @name click
	 * @type jQuery
	 * @cat Events
	 */

	/**
	 * Bind a function to the resize event of each matched element.
	 *
	 * @example $("p").resize( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onresize="alert('Hello');">Hello</p>
	 *
	 * @name resize
	 * @type jQuery
	 * @param Function fn A function to bind to the resize event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the mousemove event of each matched element.
	 *
	 * @example $("p").mousemove( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onmousemove="alert('Hello');">Hello</p>
	 *
	 * @name mousemove
	 * @type jQuery
	 * @param Function fn A function to bind to the mousemove event on each of the matched elements.
	 * @cat Events
	 */

	/**
	 * Bind a function to the mousedown event of each matched element.
	 *
	 * @example $("p").mousedown( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onmousedown="alert('Hello');">Hello</p>
	 *
	 * @name mousedown
	 * @type jQuery
	 * @param Function fn A function to bind to the mousedown event on each of the matched elements.
	 * @cat Events
	 */
	 
	/**
	 * Bind a function to the mouseover event of each matched element.
	 *
	 * @example $("p").mouseover( function() { alert("Hello"); } );
	 * @before <p>Hello</p>
	 * @result <p onmouseover="alert('Hello');">Hello</p>
	 *
	 * @name mouseover
	 * @type jQuery
	 * @param Function fn A function to bind to the mousedown event on each of the matched elements.
	 * @cat Events
	 */
	jQuery.each( ("blur,focus,load,resize,scroll,unload,click,dblclick," +
		"mousedown,mouseup,mousemove,mouseover,mouseout,change,select," + 
		"submit,keydown,keypress,keyup,error").split(","), function(i,o){
		
		// Handle event binding
		jQuery.fn[o] = function(f){
			return f ? this.bind(o, f) : this.trigger(o);
		};
			
	});
	
	// If Mozilla is used
	if ( jQuery.browser.mozilla || jQuery.browser.opera )
		// Use the handy event callback
		document.addEventListener( "DOMContentLoaded", jQuery.ready, false );
	
	// If IE is used, use the excellent hack by Matthias Miller
	// http://www.outofhanwell.com/blog/index.php?title=the_window_onload_problem_revisited
	else if ( jQuery.browser.msie ) {
	
		// Only works if you document.write() it
		document.write("<scr" + "ipt id=__ie_init defer=true " + 
			"src=//:><\/script>");
	
		// Use the defer script hack
		var script = document.getElementById("__ie_init");
		
		// script does not exist if jQuery is loaded dynamically
		if ( script ) 
			script.onreadystatechange = function() {
				if ( this.readyState != "complete" ) return;
				jQuery.ready();
			};
	
		// Clear from memory
		script = null;
	
	// If Safari  is used
	} else if ( jQuery.browser.safari )
		// Continually check to see if the document.readyState is valid
		jQuery.safariTimer = setInterval(function(){
			// loaded and complete are both valid states
			if ( document.readyState == "loaded" || 
				document.readyState == "complete" ) {
	
				// If either one are found, remove the timer
				clearInterval( jQuery.safariTimer );
				jQuery.safariTimer = null;
	
				// and execute any waiting functions
				jQuery.ready();
			}
		}, 10); 

	// A fallback to window.onload, that will always work
	jQuery.event.add( window, "load", jQuery.ready );
	
};

// Clean up after IE to avoid memory leaks
if (jQuery.browser.msie)
	jQuery(window).one("unload", function() {
		var global = jQuery.event.global;
		for ( var type in global ) {
			var els = global[type], i = els.length;
			if ( i && type != 'unload' )
				do
					els[i-1] && jQuery.event.remove(els[i-1], type);
				while (--i);
		}
	});
jQuery.fn.extend({

	/**
	 * Load HTML from a remote file and inject it into the DOM, only if it's
	 * been modified by the server.
	 *
	 * @example $("#feeds").loadIfModified("feeds.html");
	 * @before <div id="feeds"></div>
	 * @result <div id="feeds"><b>45</b> feeds found.</div>
	 *
	 * @name loadIfModified
	 * @type jQuery
	 * @param String url The URL of the HTML file to load.
	 * @param Map params (optional) Key/value pairs that will be sent to the server.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded (parameters: responseText, status and response itself).
	 * @cat Ajax
	 */
	loadIfModified: function( url, params, callback ) {
		this.load( url, params, callback, 1 );
	},

	/**
	 * Load HTML from a remote file and inject it into the DOM.
	 *
	 * Note: Avoid to use this to load scripts, instead use $.getScript.
	 * IE strips script tags when there aren't any other characters in front of it.
	 *
	 * @example $("#feeds").load("feeds.html");
	 * @before <div id="feeds"></div>
	 * @result <div id="feeds"><b>45</b> feeds found.</div>
	 *
 	 * @example $("#feeds").load("feeds.html",
 	 *   {limit: 25},
 	 *   function() { alert("The last 25 entries in the feed have been loaded"); }
 	 * );
	 * @desc Same as above, but with an additional parameter
	 * and a callback that is executed when the data was loaded.
	 *
	 * @name load
	 * @type jQuery
	 * @param String url The URL of the HTML file to load.
	 * @param Object params (optional) A set of key/value pairs that will be sent as data to the server.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded (parameters: responseText, status and response itself).
	 * @cat Ajax
	 */
	load: function( url, params, callback, ifModified ) {
		if ( jQuery.isFunction( url ) )
			return this.bind("load", url);

		callback = callback || function(){};

		// Default to a GET request
		var type = "GET";

		// If the second parameter was provided
		if ( params )
			// If it's a function
			if ( jQuery.isFunction( params ) ) {
				// We assume that it's the callback
				callback = params;
				params = null;

			// Otherwise, build a param string
			} else {
				params = jQuery.param( params );
				type = "POST";
			}

		var self = this;

		// Request the remote document
		jQuery.ajax({
			url: url,
			type: type,
			data: params,
			ifModified: ifModified,
			complete: function(res, status){
				if ( status == "success" || !ifModified && status == "notmodified" )
					// Inject the HTML into all the matched elements
					self.attr("innerHTML", res.responseText)
					  // Execute all the scripts inside of the newly-injected HTML
					  .evalScripts()
					  // Execute callback
					  .each( callback, [res.responseText, status, res] );
				else
					callback.apply( self, [res.responseText, status, res] );
			}
		});
		return this;
	},

	/**
	 * Serializes a set of input elements into a string of data.
	 * This will serialize all given elements.
	 *
	 * A serialization similar to the form submit of a browser is
	 * provided by the [http://www.malsup.com/jquery/form/ Form Plugin].
	 * It also takes multiple-selects 
	 * into account, while this method recognizes only a single option.
	 *
	 * @example $("input[@type=text]").serialize();
	 * @before <input type='text' name='name' value='John'/>
	 * <input type='text' name='location' value='Boston'/>
	 * @after name=John&amp;location=Boston
	 * @desc Serialize a selection of input elements to a string
	 *
	 * @name serialize
	 * @type String
	 * @cat Ajax
	 */
	serialize: function() {
		return jQuery.param( this );
	},

	/**
	 * Evaluate all script tags inside this jQuery. If they have a src attribute,
	 * the script is loaded, otherwise it's content is evaluated.
	 *
	 * @name evalScripts
	 * @type jQuery
	 * @private
	 * @cat Ajax
	 */
	evalScripts: function() {
		return this.find("script").each(function(){
			if ( this.src )
				jQuery.getScript( this.src );
			else
				jQuery.globalEval( this.text || this.textContent || this.innerHTML || "" );
		}).end();
	}

});

// Attach a bunch of functions for handling common AJAX events

/**
 * Attach a function to be executed whenever an AJAX request begins
 * and there is none already active.
 *
 * @example $("#loading").ajaxStart(function(){
 *   $(this).show();
 * });
 * @desc Show a loading message whenever an AJAX request starts
 * (and none is already active).
 *
 * @name ajaxStart
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */

/**
 * Attach a function to be executed whenever all AJAX requests have ended.
 *
 * @example $("#loading").ajaxStop(function(){
 *   $(this).hide();
 * });
 * @desc Hide a loading message after all the AJAX requests have stopped.
 *
 * @name ajaxStop
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */

/**
 * Attach a function to be executed whenever an AJAX request completes.
 *
 * The XMLHttpRequest and settings used for that request are passed
 * as arguments to the callback.
 *
 * @example $("#msg").ajaxComplete(function(request, settings){
 *   $(this).append("<li>Request Complete.</li>");
 * });
 * @desc Show a message when an AJAX request completes.
 *
 * @name ajaxComplete
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */

/**
 * Attach a function to be executed whenever an AJAX request completes
 * successfully.
 *
 * The XMLHttpRequest and settings used for that request are passed
 * as arguments to the callback.
 *
 * @example $("#msg").ajaxSuccess(function(request, settings){
 *   $(this).append("<li>Successful Request!</li>");
 * });
 * @desc Show a message when an AJAX request completes successfully.
 *
 * @name ajaxSuccess
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */

/**
 * Attach a function to be executed whenever an AJAX request fails.
 *
 * The XMLHttpRequest and settings used for that request are passed
 * as arguments to the callback. A third argument, an exception object,
 * is passed if an exception occured while processing the request.
 *
 * @example $("#msg").ajaxError(function(request, settings){
 *   $(this).append("<li>Error requesting page " + settings.url + "</li>");
 * });
 * @desc Show a message when an AJAX request fails.
 *
 * @name ajaxError
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */
 
/**
 * Attach a function to be executed before an AJAX request is sent.
 *
 * The XMLHttpRequest and settings used for that request are passed
 * as arguments to the callback.
 *
 * @example $("#msg").ajaxSend(function(request, settings){
 *   $(this).append("<li>Starting request at " + settings.url + "</li>");
 * });
 * @desc Show a message before an AJAX request is sent.
 *
 * @name ajaxSend
 * @type jQuery
 * @param Function callback The function to execute.
 * @cat Ajax
 */
jQuery.each( "ajaxStart,ajaxStop,ajaxComplete,ajaxError,ajaxSuccess,ajaxSend".split(","), function(i,o){
	jQuery.fn[o] = function(f){
		return this.bind(o, f);
	};
});

jQuery.extend({

	/**
	 * Load a remote page using an HTTP GET request.
	 *
	 * This is an easy way to send a simple GET request to a server
	 * without having to use the more complex $.ajax function. It
	 * allows a single callback function to be specified that will
	 * be executed when the request is complete (and only if the response
	 * has a successful response code). If you need to have both error
	 * and success callbacks, you may want to use $.ajax.
	 *
	 * @example $.get("test.cgi");
	 *
	 * @example $.get("test.cgi", { name: "John", time: "2pm" } );
	 *
	 * @example $.get("test.cgi", function(data){
	 *   alert("Data Loaded: " + data);
	 * });
	 *
	 * @example $.get("test.cgi",
	 *   { name: "John", time: "2pm" },
	 *   function(data){
	 *     alert("Data Loaded: " + data);
	 *   }
	 * );
	 *
	 * @name $.get
	 * @type XMLHttpRequest
	 * @param String url The URL of the page to load.
	 * @param Map params (optional) Key/value pairs that will be sent to the server.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded successfully.
	 * @cat Ajax
	 */
	get: function( url, data, callback, type, ifModified ) {
		// shift arguments if data argument was ommited
		if ( jQuery.isFunction( data ) ) {
			callback = data;
			data = null;
		}
		
		return jQuery.ajax({
			type: "GET",
			url: url,
			data: data,
			success: callback,
			dataType: type,
			ifModified: ifModified
		});
	},

	/**
	 * Load a remote page using an HTTP GET request, only if it hasn't
	 * been modified since it was last retrieved.
	 *
	 * @example $.getIfModified("test.html");
	 *
	 * @example $.getIfModified("test.html", { name: "John", time: "2pm" } );
	 *
	 * @example $.getIfModified("test.cgi", function(data){
	 *   alert("Data Loaded: " + data);
	 * });
	 *
	 * @example $.getifModified("test.cgi",
	 *   { name: "John", time: "2pm" },
	 *   function(data){
	 *     alert("Data Loaded: " + data);
	 *   }
	 * );
	 *
	 * @name $.getIfModified
	 * @type XMLHttpRequest
	 * @param String url The URL of the page to load.
	 * @param Map params (optional) Key/value pairs that will be sent to the server.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded successfully.
	 * @cat Ajax
	 */
	getIfModified: function( url, data, callback, type ) {
		return jQuery.get(url, data, callback, type, 1);
	},

	/**
	 * Loads, and executes, a remote JavaScript file using an HTTP GET request.
	 *
	 * Warning: Safari <= 2.0.x is unable to evaluate scripts in a global
	 * context synchronously. If you load functions via getScript, make sure
	 * to call them after a delay.
	 *
	 * @example $.getScript("test.js");
	 *
	 * @example $.getScript("test.js", function(){
	 *   alert("Script loaded and executed.");
	 * });
	 *
	 * @name $.getScript
	 * @type XMLHttpRequest
	 * @param String url The URL of the page to load.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded successfully.
	 * @cat Ajax
	 */
	getScript: function( url, callback ) {
		return jQuery.get(url, null, callback, "script");
	},

	/**
	 * Load JSON data using an HTTP GET request.
	 *
	 * @example $.getJSON("test.js", function(json){
	 *   alert("JSON Data: " + json.users[3].name);
	 * });
	 *
	 * @example $.getJSON("test.js",
	 *   { name: "John", time: "2pm" },
	 *   function(json){
	 *     alert("JSON Data: " + json.users[3].name);
	 *   }
	 * );
	 *
	 * @name $.getJSON
	 * @type XMLHttpRequest
	 * @param String url The URL of the page to load.
	 * @param Map params (optional) Key/value pairs that will be sent to the server.
	 * @param Function callback A function to be executed whenever the data is loaded successfully.
	 * @cat Ajax
	 */
	getJSON: function( url, data, callback ) {
		return jQuery.get(url, data, callback, "json");
	},

	/**
	 * Load a remote page using an HTTP POST request.
	 *
	 * @example $.post("test.cgi");
	 *
	 * @example $.post("test.cgi", { name: "John", time: "2pm" } );
	 *
	 * @example $.post("test.cgi", function(data){
	 *   alert("Data Loaded: " + data);
	 * });
	 *
	 * @example $.post("test.cgi",
	 *   { name: "John", time: "2pm" },
	 *   function(data){
	 *     alert("Data Loaded: " + data);
	 *   }
	 * );
	 *
	 * @name $.post
	 * @type XMLHttpRequest
	 * @param String url The URL of the page to load.
	 * @param Map params (optional) Key/value pairs that will be sent to the server.
	 * @param Function callback (optional) A function to be executed whenever the data is loaded successfully.
	 * @cat Ajax
	 */
	post: function( url, data, callback, type ) {
		if ( jQuery.isFunction( data ) ) {
			callback = data;
			data = {};
		}

		return jQuery.ajax({
			type: "POST",
			url: url,
			data: data,
			success: callback,
			dataType: type
		});
	},

	/**
	 * Set the timeout in milliseconds of all AJAX requests to a specific amount of time.
	 * This will make all future AJAX requests timeout after a specified amount
	 * of time.
	 *
	 * Set to null or 0 to disable timeouts (default).
	 *
	 * You can manually abort requests with the XMLHttpRequest's (returned by
	 * all ajax functions) abort() method.
	 *
	 * Deprecated. Use $.ajaxSetup instead.
	 *
	 * @example $.ajaxTimeout( 5000 );
	 * @desc Make all AJAX requests timeout after 5 seconds.
	 *
	 * @name $.ajaxTimeout
	 * @type undefined
	 * @param Number time How long before an AJAX request times out, in milliseconds.
	 * @cat Ajax
	 */
	ajaxTimeout: function( timeout ) {
		jQuery.ajaxSettings.timeout = timeout;
	},
	
	/**
	 * Setup global settings for AJAX requests.
	 *
	 * See $.ajax for a description of all available options.
	 *
	 * @example $.ajaxSetup( {
	 *   url: "/xmlhttp/",
	 *   global: false,
	 *   type: "POST"
	 * } );
	 * $.ajax({ data: myData });
	 * @desc Sets the defaults for AJAX requests to the url "/xmlhttp/",
	 * disables global handlers and uses POST instead of GET. The following
	 * AJAX requests then sends some data without having to set anything else.
	 *
	 * @name $.ajaxSetup
	 * @type undefined
	 * @param Map settings Key/value pairs to use for all AJAX requests
	 * @cat Ajax
	 */
	ajaxSetup: function( settings ) {
		jQuery.extend( jQuery.ajaxSettings, settings );
	},

	ajaxSettings: {
		global: true,
		type: "GET",
		timeout: 0,
		contentType: "application/x-www-form-urlencoded",
		processData: true,
		async: true,
		data: null
	},
	
	// Last-Modified header cache for next request
	lastModified: {},

	/**
	 * Load a remote page using an HTTP request.
	 *
	 * This is jQuery's low-level AJAX implementation. See $.get, $.post etc. for
	 * higher-level abstractions that are often easier to understand and use,
	 * but don't offer as much functionality (such as error callbacks).
	 *
	 * $.ajax() returns the XMLHttpRequest that it creates. In most cases you won't
	 * need that object to manipulate directly, but it is available if you need to
	 * abort the request manually.
	 *
	 * '''Note:''' If you specify the dataType option described below, make sure
	 * the server sends the correct MIME type in the response (eg. xml as "text/xml").
	 * Sending the wrong MIME type can lead to unexpected problems in your script.
	 * See [[Specifying the Data Type for AJAX Requests]] for more information.
	 *
	 * Supported datatypes are (see dataType option):
	 *
	 * "xml": Returns a XML document that can be processed via jQuery.
	 *
	 * "html": Returns HTML as plain text, included script tags are evaluated.
	 *
	 * "script": Evaluates the response as Javascript and returns it as plain text.
	 *
	 * "json": Evaluates the response as JSON and returns a Javascript Object
	 *
	 * $.ajax() takes one argument, an object of key/value pairs, that are
	 * used to initalize and handle the request. These are all the key/values that can
	 * be used:
	 *
	 * (String) url - The URL to request.
	 *
	 * (String) type - The type of request to make ("POST" or "GET"), default is "GET".
	 *
	 * (String) dataType - The type of data that you're expecting back from
	 * the server. No default: If the server sends xml, the responseXML, otherwise
	 * the responseText is passed to the success callback.
	 *
	 * (Boolean) ifModified - Allow the request to be successful only if the
	 * response has changed since the last request. This is done by checking the
	 * Last-Modified header. Default value is false, ignoring the header.
	 *
	 * (Number) timeout - Local timeout in milliseconds to override global timeout, eg. to give a
	 * single request a longer timeout while all others timeout after 1 second.
	 * See $.ajaxTimeout() for global timeouts.
	 *
	 * (Boolean) global - Whether to trigger global AJAX event handlers for
	 * this request, default is true. Set to false to prevent that global handlers
	 * like ajaxStart or ajaxStop are triggered.
	 *
	 * (Function) error - A function to be called if the request fails. The
	 * function gets passed tree arguments: The XMLHttpRequest object, a
	 * string describing the type of error that occurred and an optional
	 * exception object, if one occured.
	 *
	 * (Function) success - A function to be called if the request succeeds. The
	 * function gets passed one argument: The data returned from the server,
	 * formatted according to the 'dataType' parameter.
	 *
	 * (Function) complete - A function to be called when the request finishes. The
	 * function gets passed two arguments: The XMLHttpRequest object and a
	 * string describing the type of success of the request.
	 *
 	 * (Object|String) data - Data to be sent to the server. Converted to a query
	 * string, if not already a string. Is appended to the url for GET-requests.
	 * See processData option to prevent this automatic processing.
	 *
	 * (String) contentType - When sending data to the server, use this content-type.
	 * Default is "application/x-www-form-urlencoded", which is fine for most cases.
	 *
	 * (Boolean) processData - By default, data passed in to the data option as an object
	 * other as string will be processed and transformed into a query string, fitting to
	 * the default content-type "application/x-www-form-urlencoded". If you want to send
	 * DOMDocuments, set this option to false.
	 *
	 * (Boolean) async - By default, all requests are sent asynchronous (set to true).
	 * If you need synchronous requests, set this option to false.
	 *
	 * (Function) beforeSend - A pre-callback to set custom headers etc., the
	 * XMLHttpRequest is passed as the only argument.
	 *
	 * @example $.ajax({
	 *   type: "GET",
	 *   url: "test.js",
	 *   dataType: "script"
	 * })
	 * @desc Load and execute a JavaScript file.
	 *
	 * @example $.ajax({
	 *   type: "POST",
	 *   url: "some.php",
	 *   data: "name=John&location=Boston",
	 *   success: function(msg){
	 *     alert( "Data Saved: " + msg );
	 *   }
	 * });
	 * @desc Save some data to the server and notify the user once its complete.
	 *
	 * @example var html = $.ajax({
	 *  url: "some.php",
	 *  async: false
	 * }).responseText;
	 * @desc Loads data synchronously. Blocks the browser while the requests is active.
	 * It is better to block user interaction by other means when synchronization is
	 * necessary.
	 *
	 * @example var xmlDocument = [create xml document];
	 * $.ajax({
	 *   url: "page.php",
	 *   processData: false,
	 *   data: xmlDocument,
	 *   success: handleResponse
	 * });
	 * @desc Sends an xml document as data to the server. By setting the processData
	 * option to false, the automatic conversion of data to strings is prevented.
	 * 
	 * @name $.ajax
	 * @type XMLHttpRequest
	 * @param Map properties Key/value pairs to initialize the request with.
	 * @cat Ajax
	 * @see ajaxSetup(Map)
	 */
	ajax: function( s ) {
		// TODO introduce global settings, allowing the client to modify them for all requests, not only timeout
		s = jQuery.extend({}, jQuery.ajaxSettings, s);

		// if data available
		if ( s.data ) {
			// convert data if not already a string
			if (s.processData && typeof s.data != "string")
    			s.data = jQuery.param(s.data);
			// append data to url for get requests
			if( s.type.toLowerCase() == "get" ) {
				// "?" + data or "&" + data (in case there are already params)
				s.url += ((s.url.indexOf("?") > -1) ? "&" : "?") + s.data;
				// IE likes to send both get and post data, prevent this
				s.data = null;
			}
		}

		// Watch for a new set of requests
		if ( s.global && ! jQuery.active++ )
			jQuery.event.trigger( "ajaxStart" );

		var requestDone = false;

		// Create the request object; Microsoft failed to properly
		// implement the XMLHttpRequest in IE7, so we use the ActiveXObject when it is available
		var xml = window.ActiveXObject ? new ActiveXObject("Microsoft.XMLHTTP") : new XMLHttpRequest();

		// Open the socket
		xml.open(s.type, s.url, s.async);

		// Set the correct header, if data is being sent
		if ( s.data )
			xml.setRequestHeader("Content-Type", s.contentType);

		// Set the If-Modified-Since header, if ifModified mode.
		if ( s.ifModified )
			xml.setRequestHeader("If-Modified-Since",
				jQuery.lastModified[s.url] || "Thu, 01 Jan 1970 00:00:00 GMT" );

		// Set header so the called script knows that it's an XMLHttpRequest
		xml.setRequestHeader("X-Requested-With", "XMLHttpRequest");

		// Allow custom headers/mimetypes
		if( s.beforeSend )
			s.beforeSend(xml);
			
		if ( s.global )
		    jQuery.event.trigger("ajaxSend", [xml, s]);

		// Wait for a response to come back
		var onreadystatechange = function(isTimeout){
			// The transfer is complete and the data is available, or the request timed out
			if ( xml && (xml.readyState == 4 || isTimeout == "timeout") ) {
				requestDone = true;
				
				// clear poll interval
				if (ival) {
					clearInterval(ival);
					ival = null;
				}
				
				var status;
				try {
					status = jQuery.httpSuccess( xml ) && isTimeout != "timeout" ?
						s.ifModified && jQuery.httpNotModified( xml, s.url ) ? "notmodified" : "success" : "error";
					// Make sure that the request was successful or notmodified
					if ( status != "error" ) {
						// Cache Last-Modified header, if ifModified mode.
						var modRes;
						try {
							modRes = xml.getResponseHeader("Last-Modified");
						} catch(e) {} // swallow exception thrown by FF if header is not available
	
						if ( s.ifModified && modRes )
							jQuery.lastModified[s.url] = modRes;
	
						// process the data (runs the xml through httpData regardless of callback)
						var data = jQuery.httpData( xml, s.dataType );
	
						// If a local callback was specified, fire it and pass it the data
						if ( s.success )
							s.success( data, status );
	
						// Fire the global callback
						if( s.global )
							jQuery.event.trigger( "ajaxSuccess", [xml, s] );
					} else
						jQuery.handleError(s, xml, status);
				} catch(e) {
					status = "error";
					jQuery.handleError(s, xml, status, e);
				}

				// The request was completed
				if( s.global )
					jQuery.event.trigger( "ajaxComplete", [xml, s] );

				// Handle the global AJAX counter
				if ( s.global && ! --jQuery.active )
					jQuery.event.trigger( "ajaxStop" );

				// Process result
				if ( s.complete )
					s.complete(xml, status);

				// Stop memory leaks
				if(s.async)
					xml = null;
			}
		};
		
		// don't attach the handler to the request, just poll it instead
		var ival = setInterval(onreadystatechange, 13); 

		// Timeout checker
		if ( s.timeout > 0 )
			setTimeout(function(){
				// Check to see if the request is still happening
				if ( xml ) {
					// Cancel the request
					xml.abort();

					if( !requestDone )
						onreadystatechange( "timeout" );
				}
			}, s.timeout);
			
		// Send the data
		try {
			xml.send(s.data);
		} catch(e) {
			jQuery.handleError(s, xml, null, e);
		}
		
		// firefox 1.5 doesn't fire statechange for sync requests
		if ( !s.async )
			onreadystatechange();
		
		// return XMLHttpRequest to allow aborting the request etc.
		return xml;
	},

	handleError: function( s, xml, status, e ) {
		// If a local callback was specified, fire it
		if ( s.error ) s.error( xml, status, e );

		// Fire the global callback
		if ( s.global )
			jQuery.event.trigger( "ajaxError", [xml, s, e] );
	},

	// Counter for holding the number of active queries
	active: 0,

	// Determines if an XMLHttpRequest was successful or not
	httpSuccess: function( r ) {
		try {
			return !r.status && location.protocol == "file:" ||
				( r.status >= 200 && r.status < 300 ) || r.status == 304 ||
				jQuery.browser.safari && r.status == undefined;
		} catch(e){}
		return false;
	},

	// Determines if an XMLHttpRequest returns NotModified
	httpNotModified: function( xml, url ) {
		try {
			var xmlRes = xml.getResponseHeader("Last-Modified");

			// Firefox always returns 200. check Last-Modified date
			return xml.status == 304 || xmlRes == jQuery.lastModified[url] ||
				jQuery.browser.safari && xml.status == undefined;
		} catch(e){}
		return false;
	},

	/* Get the data out of an XMLHttpRequest.
	 * Return parsed XML if content-type header is "xml" and type is "xml" or omitted,
	 * otherwise return plain text.
	 * (String) data - The type of data that you're expecting back,
	 * (e.g. "xml", "html", "script")
	 */
	httpData: function( r, type ) {
		var ct = r.getResponseHeader("content-type");
		var data = !type && ct && ct.indexOf("xml") >= 0;
		data = type == "xml" || data ? r.responseXML : r.responseText;

		// If the type is "script", eval it in global context
		if ( type == "script" )
			jQuery.globalEval( data );

		// Get the JavaScript object, if JSON is used.
		if ( type == "json" )
			data = eval("(" + data + ")");

		// evaluate scripts within html
		if ( type == "html" )
			jQuery("<div>").html(data).evalScripts();

		return data;
	},

	// Serialize an array of form elements or a set of
	// key/values into a query string
	param: function( a ) {
		var s = [];

		// If an array was passed in, assume that it is an array
		// of form elements
		if ( a.constructor == Array || a.jquery )
			// Serialize the form elements
			jQuery.each( a, function(){
				s.push( encodeURIComponent(this.name) + "=" + encodeURIComponent( this.value ) );
			});

		// Otherwise, assume that it's an object of key/value pairs
		else
			// Serialize the key/values
			for ( var j in a )
				// If the value is an array then the key names need to be repeated
				if ( a[j] && a[j].constructor == Array )
					jQuery.each( a[j], function(){
						s.push( encodeURIComponent(j) + "=" + encodeURIComponent( this ) );
					});
				else
					s.push( encodeURIComponent(j) + "=" + encodeURIComponent( a[j] ) );

		// Return the resulting serialization
		return s.join("&");
	},
	
	// evalulates a script in global context
	// not reliable for safari
	globalEval: function( data ) {
		if ( window.execScript )
			window.execScript( data );
		else if ( jQuery.browser.safari )
			// safari doesn't provide a synchronous global eval
			window.setTimeout( data, 0 );
		else
			eval.call( window, data );
	}

});
jQuery.fn.extend({

	/**
	 * Displays each of the set of matched elements if they are hidden.
	 *
	 * @example $("p").show()
	 * @before <p style="display: none">Hello</p>
	 * @result [ <p style="display: block">Hello</p> ]
	 *
	 * @name show
	 * @type jQuery
	 * @cat Effects
	 */
	
	/**
	 * Show all matched elements using a graceful animation and firing an
	 * optional callback after completion.
	 *
	 * The height, width, and opacity of each of the matched elements 
	 * are changed dynamically according to the specified speed.
	 *
	 * @example $("p").show("slow");
	 *
	 * @example $("p").show("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name show
	 * @type jQuery
	 * @param String|Number speed A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see hide(String|Number,Function)
	 */
	show: function(speed,callback){
		return speed ?
			this.animate({
				height: "show", width: "show", opacity: "show"
			}, speed, callback) :
			
			this.filter(":hidden").each(function(){
				this.style.display = this.oldblock ? this.oldblock : "";
				if ( jQuery.css(this,"display") == "none" )
					this.style.display = "block";
			}).end();
	},
	
	/**
	 * Hides each of the set of matched elements if they are shown.
	 *
	 * @example $("p").hide()
	 * @before <p>Hello</p>
	 * @result [ <p style="display: none">Hello</p> ]
	 *
	 * @name hide
	 * @type jQuery
	 * @cat Effects
	 */
	
	/**
	 * Hide all matched elements using a graceful animation and firing an
	 * optional callback after completion.
	 *
	 * The height, width, and opacity of each of the matched elements 
	 * are changed dynamically according to the specified speed.
	 *
	 * @example $("p").hide("slow");
	 *
	 * @example $("p").hide("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name hide
	 * @type jQuery
	 * @param String|Number speed A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see show(String|Number,Function)
	 */
	hide: function(speed,callback){
		return speed ?
			this.animate({
				height: "hide", width: "hide", opacity: "hide"
			}, speed, callback) :
			
			this.filter(":visible").each(function(){
				this.oldblock = this.oldblock || jQuery.css(this,"display");
				if ( this.oldblock == "none" )
					this.oldblock = "block";
				this.style.display = "none";
			}).end();
	},

	// Save the old toggle function
	_toggle: jQuery.fn.toggle,
	
	/**
	 * Toggles each of the set of matched elements. If they are shown,
	 * toggle makes them hidden. If they are hidden, toggle
	 * makes them shown.
	 *
	 * @example $("p").toggle()
	 * @before <p>Hello</p><p style="display: none">Hello Again</p>
	 * @result [ <p style="display: none">Hello</p>, <p style="display: block">Hello Again</p> ]
	 *
	 * @name toggle
	 * @type jQuery
	 * @cat Effects
	 */
	toggle: function( fn, fn2 ){
		return jQuery.isFunction(fn) && jQuery.isFunction(fn2) ?
			this._toggle( fn, fn2 ) :
			fn ?
				this.animate({
					height: "toggle", width: "toggle", opacity: "toggle"
				}, fn, fn2) :
				this.each(function(){
					jQuery(this)[ jQuery(this).is(":hidden") ? "show" : "hide" ]();
				});
	},
	
	/**
	 * Reveal all matched elements by adjusting their height and firing an
	 * optional callback after completion.
	 *
	 * Only the height is adjusted for this animation, causing all matched
	 * elements to be revealed in a "sliding" manner.
	 *
	 * @example $("p").slideDown("slow");
	 *
	 * @example $("p").slideDown("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name slideDown
	 * @type jQuery
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see slideUp(String|Number,Function)
	 * @see slideToggle(String|Number,Function)
	 */
	slideDown: function(speed,callback){
		return this.animate({height: "show"}, speed, callback);
	},
	
	/**
	 * Hide all matched elements by adjusting their height and firing an
	 * optional callback after completion.
	 *
	 * Only the height is adjusted for this animation, causing all matched
	 * elements to be hidden in a "sliding" manner.
	 *
	 * @example $("p").slideUp("slow");
	 *
	 * @example $("p").slideUp("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name slideUp
	 * @type jQuery
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see slideDown(String|Number,Function)
	 * @see slideToggle(String|Number,Function)
	 */
	slideUp: function(speed,callback){
		return this.animate({height: "hide"}, speed, callback);
	},

	/**
	 * Toggle the visibility of all matched elements by adjusting their height and firing an
	 * optional callback after completion.
	 *
	 * Only the height is adjusted for this animation, causing all matched
	 * elements to be hidden in a "sliding" manner.
	 *
	 * @example $("p").slideToggle("slow");
	 *
	 * @example $("p").slideToggle("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name slideToggle
	 * @type jQuery
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see slideDown(String|Number,Function)
	 * @see slideUp(String|Number,Function)
	 */
	slideToggle: function(speed, callback){
		return this.animate({height: "toggle"}, speed, callback);
	},
	
	/**
	 * Fade in all matched elements by adjusting their opacity and firing an
	 * optional callback after completion.
	 *
	 * Only the opacity is adjusted for this animation, meaning that
	 * all of the matched elements should already have some form of height
	 * and width associated with them.
	 *
	 * @example $("p").fadeIn("slow");
	 *
	 * @example $("p").fadeIn("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name fadeIn
	 * @type jQuery
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see fadeOut(String|Number,Function)
	 * @see fadeTo(String|Number,Number,Function)
	 */
	fadeIn: function(speed, callback){
		return this.animate({opacity: "show"}, speed, callback);
	},
	
	/**
	 * Fade out all matched elements by adjusting their opacity and firing an
	 * optional callback after completion.
	 *
	 * Only the opacity is adjusted for this animation, meaning that
	 * all of the matched elements should already have some form of height
	 * and width associated with them.
	 *
	 * @example $("p").fadeOut("slow");
	 *
	 * @example $("p").fadeOut("slow",function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name fadeOut
	 * @type jQuery
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see fadeIn(String|Number,Function)
	 * @see fadeTo(String|Number,Number,Function)
	 */
	fadeOut: function(speed, callback){
		return this.animate({opacity: "hide"}, speed, callback);
	},
	
	/**
	 * Fade the opacity of all matched elements to a specified opacity and firing an
	 * optional callback after completion.
	 *
	 * Only the opacity is adjusted for this animation, meaning that
	 * all of the matched elements should already have some form of height
	 * and width associated with them.
	 *
	 * @example $("p").fadeTo("slow", 0.5);
	 *
	 * @example $("p").fadeTo("slow", 0.5, function(){
	 *   alert("Animation Done.");
	 * });
	 *
	 * @name fadeTo
	 * @type jQuery
	 * @param String|Number speed A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param Number opacity The opacity to fade to (a number from 0 to 1).
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 * @see fadeIn(String|Number,Function)
	 * @see fadeOut(String|Number,Function)
	 */
	fadeTo: function(speed,to,callback){
		return this.animate({opacity: to}, speed, callback);
	},
	
	/**
	 * A function for making your own, custom animations. The key aspect of
	 * this function is the object of style properties that will be animated,
	 * and to what end. Each key within the object represents a style property
	 * that will also be animated (for example: "height", "top", or "opacity").
	 *
	 * Note that properties should be specified using camel case
	 * eg. marginLeft instead of margin-left.
	 *
	 * The value associated with the key represents to what end the property
	 * will be animated. If a number is provided as the value, then the style
	 * property will be transitioned from its current state to that new number.
	 * Otherwise if the string "hide", "show", or "toggle" is provided, a default
	 * animation will be constructed for that property.
	 *
	 * @example $("p").animate({
	 *   height: 'toggle', opacity: 'toggle'
	 * }, "slow");
	 *
	 * @example $("p").animate({
	 *   left: 50, opacity: 'show'
	 * }, 500);
	 *
	 * @example $("p").animate({
	 *   opacity: 'show'
	 * }, "slow", "easein");
	 * @desc An example of using an 'easing' function to provide a different style of animation. This will only work if you have a plugin that provides this easing function (Only "swing" and "linear" are provided by default, with jQuery).
	 *
	 * @name animate
	 * @type jQuery
	 * @param Hash params A set of style attributes that you wish to animate, and to what end.
	 * @param String|Number speed (optional) A string representing one of the three predefined speeds ("slow", "normal", or "fast") or the number of milliseconds to run the animation (e.g. 1000).
	 * @param String easing (optional) The name of the easing effect that you want to use (e.g. "swing" or "linear"). Defaults to "swing".
	 * @param Function callback (optional) A function to be executed whenever the animation completes.
	 * @cat Effects
	 */
	animate: function( prop, speed, easing, callback ) {
		return this.queue(function(){
			var hidden = jQuery(this).is(":hidden"),
				opt = jQuery.speed(speed, easing, callback),
				self = this;
			
			for ( var p in prop )
				if ( prop[p] == "hide" && hidden || prop[p] == "show" && !hidden )
					return jQuery.isFunction(opt.complete) && opt.complete.apply(this);

			this.curAnim = jQuery.extend({}, prop);
			
			jQuery.each( prop, function(name, val){
				var e = new jQuery.fx( self, opt, name );
				if ( val.constructor == Number )
					e.custom( e.cur(), val );
				else
					e[ val == "toggle" ? hidden ? "show" : "hide" : val ]( prop );
			});
		});
	},
	
	/**
	 *
	 * @private
	 */
	queue: function(type,fn){
		if ( !fn ) {
			fn = type;
			type = "fx";
		}
	
		return this.each(function(){
			if ( !this.queue )
				this.queue = {};
	
			if ( !this.queue[type] )
				this.queue[type] = [];
	
			this.queue[type].push( fn );
		
			if ( this.queue[type].length == 1 )
				fn.apply(this);
		});
	}

});

jQuery.extend({
	
	speed: function(speed, easing, fn) {
		var opt = speed && speed.constructor == Object ? speed : {
			complete: fn || !fn && easing || 
				jQuery.isFunction( speed ) && speed,
			duration: speed,
			easing: fn && easing || easing && easing.constructor != Function && easing || (jQuery.easing.swing ? "swing" : "linear")
		};

		opt.duration = (opt.duration && opt.duration.constructor == Number ? 
			opt.duration : 
			{ slow: 600, fast: 200 }[opt.duration]) || 400;
	
		// Queueing
		opt.old = opt.complete;
		opt.complete = function(){
			jQuery.dequeue(this, "fx");
			if ( jQuery.isFunction( opt.old ) )
				opt.old.apply( this );
		};
	
		return opt;
	},
	
	easing: {
		linear: function( p, n, firstNum, diff ) {
			return firstNum + diff * p;
		},
		swing: function( p, n, firstNum, diff ) {
			return ((-Math.cos(p*Math.PI)/2) + 0.5) * diff + firstNum;
		}
	},
	
	queue: {},
	
	dequeue: function(elem,type){
		type = type || "fx";
	
		if ( elem.queue && elem.queue[type] ) {
			// Remove self
			elem.queue[type].shift();
	
			// Get next function
			var f = elem.queue[type][0];
		
			if ( f ) f.apply( elem );
		}
	},

	timers: [],

	/*
	 * I originally wrote fx() as a clone of moo.fx and in the process
	 * of making it small in size the code became illegible to sane
	 * people. You've been warned.
	 */
	
	fx: function( elem, options, prop ){

		var z = this;

		// The styles
		var y = elem.style;
		
		if ( prop == "height" || prop == "width" ) {
			// Store display property
			var oldDisplay = jQuery.css(elem, "display");

			// Make sure that nothing sneaks out
			var oldOverflow = y.overflow;
			y.overflow = "hidden";
		}

		// Simple function for setting a style value
		z.a = function(){
			if ( options.step )
				options.step.apply( elem, [ z.now ] );

			if ( prop == "opacity" )
				jQuery.attr(y, "opacity", z.now); // Let attr handle opacity
			else {
				y[prop] = parseInt(z.now) + "px";
				y.display = "block"; // Set display property to block for animation
			}
		};

		// Figure out the maximum number to run to
		z.max = function(){
			return parseFloat( jQuery.css(elem,prop) );
		};

		// Get the current size
		z.cur = function(){
			var r = parseFloat( jQuery.curCSS(elem, prop) );
			return r && r > -10000 ? r : z.max();
		};

		// Start an animation from one number to another
		z.custom = function(from,to){
			z.startTime = (new Date()).getTime();
			z.now = from;
			z.a();

			jQuery.timers.push(function(){
				return z.step(from, to);
			});

			if ( jQuery.timers.length == 1 ) {
				var timer = setInterval(function(){
					var timers = jQuery.timers;
					
					for ( var i = 0; i < timers.length; i++ )
						if ( !timers[i]() )
							timers.splice(i--, 1);

					if ( !timers.length )
						clearInterval( timer );
				}, 13);
			}
		};

		// Simple 'show' function
		z.show = function(){
			if ( !elem.orig ) elem.orig = {};

			// Remember where we started, so that we can go back to it later
			elem.orig[prop] = jQuery.attr( elem.style, prop );

			options.show = true;

			// Begin the animation
			z.custom(0, this.cur());

			// Make sure that we start at a small width/height to avoid any
			// flash of content
			if ( prop != "opacity" )
				y[prop] = "1px";
			
			// Start by showing the element
			jQuery(elem).show();
		};

		// Simple 'hide' function
		z.hide = function(){
			if ( !elem.orig ) elem.orig = {};

			// Remember where we started, so that we can go back to it later
			elem.orig[prop] = jQuery.attr( elem.style, prop );

			options.hide = true;

			// Begin the animation
			z.custom(this.cur(), 0);
		};

		// Each step of an animation
		z.step = function(firstNum, lastNum){
			var t = (new Date()).getTime();

			if (t > options.duration + z.startTime) {
				z.now = lastNum;
				z.a();

				if (elem.curAnim) elem.curAnim[ prop ] = true;

				var done = true;
				for ( var i in elem.curAnim )
					if ( elem.curAnim[i] !== true )
						done = false;

				if ( done ) {
					if ( oldDisplay != null ) {
						// Reset the overflow
						y.overflow = oldOverflow;
					
						// Reset the display
						y.display = oldDisplay;
						if ( jQuery.css(elem, "display") == "none" )
							y.display = "block";
					}

					// Hide the element if the "hide" operation was done
					if ( options.hide )
						y.display = "none";

					// Reset the properties, if the item has been hidden or shown
					if ( options.hide || options.show )
						for ( var p in elem.curAnim )
							jQuery.attr(y, p, elem.orig[p]);
				}

				// If a callback was provided, execute it
				if ( done && jQuery.isFunction( options.complete ) )
					// Execute the complete function
					options.complete.apply( elem );

				return false;
			} else {
				var n = t - this.startTime;
				// Figure out where in the animation we are and set the number
				var p = n / options.duration;
				
				// Perform the easing function, defaults to swing
				z.now = jQuery.easing[options.easing](p, n, firstNum, (lastNum-firstNum), options.duration);

				// Perform the next step of the animation
				z.a();
			}

			return true;
		};
	
	}
});
}
