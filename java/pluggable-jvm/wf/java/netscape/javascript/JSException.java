/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 *  
 * The Original Code is The Waterfall Java Plugin Module
 *  
 * The Initial Developer of the Original Code is Sun Microsystems Inc
 * Portions created by Sun Microsystems Inc are Copyright (C) 2001
 * All Rights Reserved.
 * 
 * $Id: JSException.java,v 1.1 2001/07/12 20:25:44 edburns%acm.org Exp $
 * 
 * Contributor(s):
 * 
 *     Nikolay N. Igotti <nikolay.igotti@Sun.Com>
 */ 

package netscape.javascript;

// exception that can be thrown by LiveConnect in Mozilla.

public class JSException extends RuntimeException {

    // Exception type supported by JavaScript 1.4 in Mozilla
    public static final int EXCEPTION_TYPE_EMPTY = -1;
    public static final int EXCEPTION_TYPE_VOID = 0;
    public static final int EXCEPTION_TYPE_OBJECT = 1;
    public static final int EXCEPTION_TYPE_FUNCTION = 2;
    public static final int EXCEPTION_TYPE_STRING = 3;
    public static final int EXCEPTION_TYPE_NUMBER = 4;
    public static final int EXCEPTION_TYPE_BOOLEAN = 5;
    public static final int EXCEPTION_TYPE_ERROR = 6;

    public JSException() {
	this(null);
    }

    public JSException(String s)  {
	this(s, null, -1, null, -1);
    }

    
    public JSException(String s, String filename, int lineno, String source, 
		       int tokenIndex)  {
	super(s);
	this.message = s;
	this.filename = filename;
	this.lineno = lineno;
	this.source = source;
	this.tokenIndex = tokenIndex;	
	this.wrappedExceptionType = EXCEPTION_TYPE_EMPTY;
    }

    public JSException(int wrappedExceptionType, Object wrappedException) {
	this();
	this.wrappedExceptionType = wrappedExceptionType;
	this.wrappedException = wrappedException;
    }

    protected String message = null;

    protected String filename = null;

    protected int lineno = -1;

    protected String source = null;

    protected int tokenIndex = -1;

    private int wrappedExceptionType = -1;

    private Object wrappedException = null;

    public int getWrappedExceptionType() {
	return wrappedExceptionType;
    }

    public Object getWrappedException() {
	return wrappedException;
    }
}
