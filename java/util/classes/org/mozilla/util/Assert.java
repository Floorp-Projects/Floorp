/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * 
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS IS"
 * basis, WITHOUT WARRANTY OF ANY KIND, either express or implied. See
 * the License for the specific language governing rights and limitations
 * under the License.
 *
 * The original code is "The Lighthouse Foundation Classes (LFC)"
 * 
 * The Initial Developer of the Original Code is Sun Microsystems,
 * Inc. Portions created by Sun are Copyright (C) 1997, 1998, 1999 Sun
 * Microsystems, Inc. All Rights Reserved.
 */

package org.mozilla.util;


/**
 *  The <B>Assert</B> class provides convenient means for condition testing.
 *  Class methods in the <B>Assert</B> class can be used to verify certain
 *  conditions and to raise exceptions if the conditions are not met.
 *  Assertions are particularly useful during the development of a
 *  project and may be turned off in production code.  Turning off
 *  exceptions causes <B>Assert</B> not to raise exceptions when conditions
 *  are not met.<P>
 *
 *  Typical usage:
 *  <BLOCKQUOTE><TT>
 *	Assert.assert(Assert.enabled && mytest(), "my test failed");
 *  </TT></BLOCKQUOTE><P>
 *
 *  Such usage prevents <TT>mytest()</TT> from being executed if assertions
 *  are disabled.  This techinique allows assertions to do time-consuming
 *  checks (such as <TT>myArray.containsObject(myObject)</TT>) without
 *  worrying about them impacting the performance of the final release.<P>
 *
 *  If you know that the condition being tested is fast, you may omit the
 *  <I>enabled</I> flag:
 *  <BLOCKQUOTE><TT>
 *	Assert.assert(myValue != null);
 *  </TT></BLOCKQUOTE><P>
 *
 *  Note that even in this second usage, if assertions are disabled,
 *  the <B>assert()</B> method will do nothing.<P>
 *
 *  Another usage that is more efficient but bulkier:
 *  <BLOCKQUOTE><TT>
 *	if (Assert.enabled && Assert.assert(mytest(), "my test failed"));
 *  </TT></BLOCKQUOTE><P>
 *
 *  Such usage prevents not only <TT>mytest()</TT> from being executed if
 *  assertions are disabled but also the assert method itself, and some
 *  compilers can remove the entire statement if assertions are disabled.<P>
 *
 *  <B>Assert</B> is intended for general condition and invariant testing;
 *  for parameter testing, use <B>ParameterCheck</B>.
 *
 * @see	ParameterCheck
 */
final public class Assert extends Object {

//
//  Public Constants
//

    /**
     * True if failed assertions should raise exceptions, or false if they
     * should do nothing.
     */
    static public boolean	enabled = true;


/**
 *  Private constructor prevents instances of this class from being created.
 */
private Assert() {}

/**
 *  Sets <I>enabled</I> to <I>newEnabled</I>.
 *
 * @param	newEnabled	true if and only if assertions should be 
 *  				enabled
 */
static public void setEnabled(boolean newEnabled) {
    enabled = newEnabled;
}

/**
 * Throws <B>AssertionFailureException</B> with a message of <I>message</I>
 * if <I>enabled</I> is true and <I>test</I> is false; otherwise, does nothing.
 *
 * @param	test	value to verify
 * @param	message	message of exception thrown if <I>test</I> is false
 * @exception	AssertionFailureException	if <I>test</I> is false
 * @return	true
 */
static public boolean assert (boolean test, String message) {
    if (enabled && !test) {
	throw new AssertionFailureException (message);
    }
    return true;
}

/**
 * Throws <B>AssertionFailureException</B> if <I>enabled</I> is true and
 * <I>test</I> is false; otherwise, does nothing.
 *
 * @param	test	value to verify
 * @exception	AssertionFailureException	if <I>test</I> is false
 * @return	true
 */
static public boolean assert (boolean test) {
    if (enabled && !test) {
	throw new AssertionFailureException ();
    }
    return true;
}



} // End of class Assert
