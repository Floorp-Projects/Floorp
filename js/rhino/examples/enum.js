/*
	enum.js
	
	Implementing the interface java.util.Enumeration using the new syntax.
	Note that this syntax is experimental only, and hasn't been approved
	by ECMA. 
	The same functionality can be had without the new syntax using the
	uglier syntax:

		var elements = new JavaAdapter(java.util.Enumeration, {
						index: 0, elements: array,
						hasMoreElements: function ...
						nextElement: function ...
		});
	
	by Patrick C. Beard.
 */

// an array to enumerate.
var array = [0, 1, 2];

// create an array enumeration.
var elements = new java.util.Enumeration() {
	index: 0, elements: array,
	hasMoreElements: function() {
		return (this.index < this.elements.length);
	},
	nextElement: function() {
		return this.elements[this.index++];
	}
};

// now print out the array by enumerating through the Enumeration
while (elements.hasMoreElements())
	print(elements.nextElement());
