/* -*- Mode: Java; tab-width: 8; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 *
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either of the GNU General Public License Version 2 or later (the "GPL"),
 * or the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
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

package netscape.javascript;

/**
 * JSException is an exception which is thrown when JavaScript code
 * returns an error.
 */

public
class JSException extends RuntimeException {
    public static final int EXCEPTION_TYPE_EMPTY = -1;
    public static final int EXCEPTION_TYPE_VOID = 0;
    public static final int EXCEPTION_TYPE_OBJECT = 1;
    public static final int EXCEPTION_TYPE_FUNCTION = 2;
    public static final int EXCEPTION_TYPE_STRING = 3;
    public static final int EXCEPTION_TYPE_NUMBER = 4;
    public static final int EXCEPTION_TYPE_BOOLEAN = 5;
    public static final int EXCEPTION_TYPE_ERROR = 6;

    String filename;
    int lineno;
    String source;
    int tokenIndex;
    private int wrappedExceptionType;
    private Object wrappedException;

    /**
     * Constructs a JSException without a detail message.
     * A detail message is a String that describes this particular exception.
     *
     * @deprecated Not for public use in future versions.
     */
    public JSException() {
	super();
        filename = "unknown";
        lineno = 0;
        source = "";
        tokenIndex = 0;
	wrappedExceptionType = EXCEPTION_TYPE_EMPTY;
    }

    /**
     * Constructs a JSException with a detail message.
     * A detail message is a String that describes this particular exception.
     * @param s the detail message
     *
     * @deprecated Not for public use in future versions.
     */
    public JSException(String s) {
	super(s);
        filename = "unknown";
        lineno = 0;
        source = "";
        tokenIndex = 0;
	wrappedExceptionType = EXCEPTION_TYPE_EMPTY;
    }

    /**
     * Constructs a JSException with a wrapped JavaScript exception object.
     * This constructor needs to be public so that Java users can throw 
     * exceptions to JS cleanly.
     */
    private JSException(int wrappedExceptionType, Object wrappedException) {
	super();
	this.wrappedExceptionType = wrappedExceptionType;
	this.wrappedException = wrappedException;
    }
    
    /**
     * Constructs a JSException with a detail message and all the
     * other info that usually comes with a JavaScript error.
     * @param s the detail message
     *
     * @deprecated Not for public use in future versions.
     */
    public JSException(String s, String filename, int lineno,
                       String source, int tokenIndex) {
	super(s);
        this.filename = filename;
        this.lineno = lineno;
        this.source = source;
        this.tokenIndex = tokenIndex;
	wrappedExceptionType = EXCEPTION_TYPE_EMPTY;
    }

    /**
     * Instance method getWrappedExceptionType returns the int mapping of the
     * type of the wrappedException Object.
     */
    public int getWrappedExceptionType() {
	return wrappedExceptionType;
    }

    /**
     * Instance method getWrappedException.
     */
    public Object getWrappedException() {
	return wrappedException;
    }

}

