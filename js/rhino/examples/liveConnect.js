/**
 * liveConnect.js: a simple demonstration of JavaScript-to-Java connectivity
 */

// Create a new StringBuffer. Note that the class name must be fully qualified
// by its package. Packages other than "java" must start with "Packages", i.e., 
// "Packages.javax.servlet...".
var sb = new java.lang.StringBuffer();

// Now add some stuff to the buffer.
sb.append("hi, mom");
sb.append(3);	// this will add "3.0" to the buffer since all JS numbers
		// are doubles by default
sb.append(true);

// Now print it out. (The toString() method of sb is automatically called
// to convert the buffer to a string.)
// Should print "hi, mom3.0true".
print(sb);	
