/* Insert copyright and license here 19** */

package netscape.javascript;

/**
 * JSException is an exception which is thrown when JavaScript code
 * returns an error.
 */

public
class JSException extends Exception {
    String filename;
    int lineno;
    String source;
    int tokenIndex;

    /**
     * Constructs a JSException without a detail message.
     * A detail message is a String that describes this particular exception.
     */
    public JSException() {
	super();
        filename = "unknown";
        lineno = 0;
        source = "";
        tokenIndex = 0;
    }

    /**
     * Constructs a JSException with a detail message.
     * A detail message is a String that describes this particular exception.
     * @param s the detail message
     */
    public JSException(String s) {
	super(s);
        filename = "unknown";
        lineno = 0;
        source = "";
        tokenIndex = 0;
    }

    /**
     * Constructs a JSException with a detail message and all the
     * other info that usually comes with a JavaScript error.
     * @param s the detail message
     */
    public JSException(String s, String filename, int lineno,
                       String source, int tokenIndex) {
	super(s);
        this.filename = filename;
        this.lineno = lineno;
        this.source = source;
        this.tokenIndex = tokenIndex;
    }
}

