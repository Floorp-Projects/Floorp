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
 *  ParameterCheck provides convenient means for parameter
 *  checking.  Class methods in the ParameterCheck class can be
 *  used to verify that method parameters meet certain conditions and to
 *  raise exceptions if the conditions are not met.<p>
 *
 *  Typical usage:
 *  <BLOCKQUOTE><TT>
 *	ParameterCheck.nonNull(myParameter);
 *  </TT></BLOCKQUOTE><P>
 *
 *  This verifies that <TT>myParameter</TT> is not null, throwing an
 *  IllegalArgumentException if it is.<P>
 *
 *  ParameterCheck is intended specifically for checking parameters;
 *  for general condition and invariant testing, use Assert.
 *
 * @version  $Id: ParameterCheck.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $
 * @author   The LFC Team (Rob Davis, Paul Kim, Alan Chung, Ray Ryan, etc)
 * @author   David-John Burrowes (he moved it to the AU package)
 * @see      Assert
 */
final public class ParameterCheck extends Object {
    /**
     * The RCSID for this class.
     */
    private static final String RCSID =
        "$Id: ParameterCheck.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $";

    /**
     * Private constructor prevents instances of this class from being
     * created.
     */
    private ParameterCheck() {
    }

    /**
     * Throws IllegalArgumentException if 'anObject' is null; otherwise, does
     * nothing.
     *
     * @param	anObject	value to verify as non-null
     * @exception  IllegalArgumentException	if 'anObject' is null
     */
    static public void nonNull(Object anObject) {
        if (anObject == null) {
            throw new IllegalArgumentException("Object is null");
        }
    }
    
    /**
     * Throws IllegalArgumentException if 'aString' is null or if
     * 'aString' is an empty string; otherwise, does nothing.
     *
     * @param	aString	         string to verify as non-null and not empty
     * @exception  IllegalArgumentException   if 'aString' is null or empty
     */
    static public void notEmpty(String aString) {
        if (aString == null) {
            throw new IllegalArgumentException("String is null");
        }
	else if (aString.equals("")) {
            throw new IllegalArgumentException("String is empty");
	}
    }
    
    /**
     * Throws RangeException if 'anInt' is less than 'minimum'; otherwise,
     * does nothing.
     *
     * @param	anInt	value to verify as not less than 'minimum'
     * @param	minimum	smallest acceptable value for 'anInt'
     * @exception RangeException if 'anInt' is less than 'minimum'
     */
    static public void noLessThan(int anInt, int minimum) {
	if (anInt < minimum) {
            throw new RangeException("Value " + anInt +
	    " is out of range.  It is should be no less than " + minimum);
	}
    }
    
    /**
     * Throws RangeException if 'anInt' is not greater than 'minimum';
     * otherwise, does nothing.
     *
     * @param	anInt	value to verify as greater than 'minimum'
     * @param	minimum	largest unacceptable value for 'anInt'
     * @exception RangeException  if 'anInt' is not greater than 'minimum'
     */
    static public void greaterThan(int anInt, int minimum) {
	if (anInt <= minimum) {
	    throw new RangeException ("Value " + anInt +
	            " is out of range.  It should be greater than " + minimum);
	}
    }
    
    /**
     * Throws RangeException if 'anInt' is greater than 'maximum'; otherwise,
     * does nothing.
     *
     * @param	anInt	value to verify as not greater than 'maximum'
     * @param	maximum	largest acceptable value for 'anInt'
     * @exception  RangeException	if 'anInt' is greater than 'maximum'
     */
    static public void noGreaterThan(int anInt, int maximum) {
	if (anInt > maximum) {
	    throw new RangeException ("Value " + anInt + "is out of " +
                    "range. It should be no greater than " + maximum);
	}
    }
    
    /**
     * Throws RangeException if 'anInt' is not less than 'maximum';
     * otherwise, does nothing.
     *
     * @param	anInt	value to verify as less than 'minimum'
     * @param	maximum	smallest unacceptable value for 'anInt'
     * @exception  RangeException	if 'anInt' is not less than 'maximum'
     */
    static public void lessThan(int anInt, int maximum) {
	if (anInt >= maximum) {
	    throw new RangeException ("Value " + anInt +
		    " is out of range.  " +
		    "It should be less than " + maximum);
	}
    }
    
    /**
     * Throws RangeException if 'aDouble' is less than 'minimum'; otherwise,
     * does nothing.
     *
     * @param	aDouble	value to verify as not less than 'minimum'
     * @param	minimum	smallest acceptable value for 'aDouble'
     * @exception  RangeException	if 'aDouble' is less than 'minimum'
     */
    static public void noLessThan(double aDouble, double minimum) {
	if (aDouble < minimum) {
	    throw new RangeException ("Value " + aDouble +
		    " is out of range.  " +
		    "It should be no less than " + minimum);
	}
    }
    
    /**
     * Throws RangeException if 'aDouble' is not greater than 'minimum';
     * otherwise, does nothing.
     *
     * @param	aDouble	value to verify as greater than 'minimum'
     * @param	minimum	largest unacceptable value for 'aDouble'
     * @exception  RangeException  if 'aDouble' is not greater than 'minimum'
     */
    static public void greaterThan(double aDouble, double minimum) {
	if (aDouble <= minimum) {
	    throw new RangeException ("Value " + aDouble +
	            " is out of range.  " +
	            "It should be greater than " + minimum);
	}
    }
    
    /**
     * Throws RangeException if 'aDouble' is greater than
     * 'maximum'; otherwise, does nothing.
     *
     * @param	aDouble	value to verify as not greater than 'maximum'
     * @param	maximum	largest acceptable value for 'aDouble'
     * @exception  RangeException	if 'aDouble' is greater than 'maximum'
     */
    static public void noGreaterThan(double aDouble, double maximum) {
	if (aDouble > maximum) {
	    throw new RangeException ("Value " + aDouble +
	    	    " is out of range.  " +
		    "It should be no greater than " + maximum);
	}
    }
    
    /**
     * Throws RangeException if 'aDouble' is not less than 'maximum';
     * otherwise, does nothing.
     *
     * @param	aDouble	value to verify as less than 'minimum'
     * @param	maximum	smallest unacceptable value for 'aDouble'
     * @exception  RangeException	if 'aDouble' is not less than 'maximum'
     */
    static public void lessThan(double aDouble, double maximum) {
	if (aDouble >= maximum) {
	    throw new RangeException ("Value " + aDouble +
	            " is out of range.  It should be less than " + maximum);
	}
    }
    
    /**
     * Throws RangeException if 'anInt' is less than 'minimum' or greater
     * than 'maximum'; otherwise, does nothing.
     *
     * @param	anInt	value to verify as not greater than 'maximum' and
     *			not less than 'minimum'
     * @param	minimum	smallest acceptable value for 'anInt'
     * @param	maximum	largest acceptable value for 'anInt'
     * @exception  RangeException	if 'anInt' is less than 'minimum' or
     *					greater than 'maximum'
     */
    static public void withinRange(int anInt, int minimum, int maximum) {
	ParameterCheck.noLessThan(anInt, minimum);
	ParameterCheck.noGreaterThan(anInt, maximum);
    }
    
    /**
     * Throws RangeException if 'aDouble' is less than 'minimum' or greater
     * than 'maximum'; otherwise, does nothing.
     *
     * @param	aDouble	value to verify as not greater than 'maximum' and
     *			not less than 'minimum'
     * @param	minimum	smallest acceptable value for 'aDouble'
     * @param	maximum	largest acceptable value for 'aDouble'
     * @exception  RangeException	if 'aDouble' is less than 'minimum' or
     *					greater than 'maximum'
     */
    static public void withinRange(double aDouble, double minimum,
                                   double maximum) {
	ParameterCheck.noLessThan(aDouble, minimum);
	ParameterCheck.noGreaterThan(aDouble, maximum);
    }
    
    
    /**
     * Throws RangeException if 'anInt' is not within 'aRange'; otherwise,
     * does nothing.
     *
     * @param	anInt	value to verify as within 'aRange'
     * @param	aRange	acceptable range for 'anInt'
     * @exception  RangeException	if 'anInt' is not within 'aRange'
     */
    static public void withinRange(int anInt, Range aRange) {
	if (!aRange.containsIndex(anInt)) {
	    throw new RangeException ("Value " + anInt +
	            " should be in range " + aRange);
	}
    }
    
    
    /**
     * Throws RangeException if 'anInt' is not within a sequence that starts
     * at 0 and has a length of 'count'; otherwise does nothing.
     *
     * (e.g. withinCount(myInt, array.length) will verify that 'myInt' is not
     * less than 0 and not greater than 'array.length'-1)
     *
     * @param	anInt	value to verify as between 0 and 'count' - 1, inclusive
     * @param	count	length of sequence
     * @exception  RangeException	if 'anInt' is out of bounds
     */
    static public void withinCount(int anInt, int count) {
	ParameterCheck.withinRange(anInt, 0, count-1);
    }
    
    
    /**
     * Throws RangeException if 'aRange' is not completely between 'minimum'
     * and 'maximum', inclusive; otherwise, does nothing.
     *
     * @param	aRange	range to verify as between 'minimum' and 'maximum'
     * @param	minimum	smallest acceptable index in 'aRange'
     * @param	maximum	largest acceptable index in 'aRange'
     * @exception  RangeException	if 'aRange' is out of bounds
     */
    static public void rangeWithinBounds(Range aRange, int minimum,
                                         int maximum) {
	if (aRange.getStart() < minimum) {
	    throw new RangeException ("Range " + aRange +
		    " should not contain indices less than " +
		    minimum);
	}
	
	if (aRange.getEnd() > maximum) {
	    throw new RangeException ("Range " + aRange +
		    " should not contain indices greater than "
		    + maximum);
	}
    }
    
    /**
     * Throws RangeException if 'aRange' is not completely within a sequence
     * that starts at 0 and has a length of 'count'; otherwise does nothing.
     *
     * (e.g. rangeWithinCount(range, array.length) will verify that 'range'
     * does not start before 0 and does not end after 'array.length'-1).
     *
     * @param	aRange	range to verify as between 0 and 'count' - 1, inclusive
     * @param	count	length of sequence
     * @exception  RangeException	if 'aRange' is out of bounds
     */
    static public void rangeWithinCount(Range aRange, int count) {
        ParameterCheck.rangeWithinBounds(aRange, 0, count-1);
    }
    
    /**
     * Checks a string and a range which is intended to indicate a substring.
     * Throws IllegalArgumentException if either argument is null. Throws
     * RangeException if <I>aRange</I> does not indicate a valid substring of
     * <I>aString</I>.
     *
     * @param       aRange          A range which must be contained within
     *                              the string.
     * @param       aString         A string to use for verifying the range. 
     * @exception   IllegalArgumentException	if either argument is null
     * @exception   RangeException		if the range is not contained
     *						within the string
     */
    static public void rangeWithinString(Range aRange, String aString) {
	ParameterCheck.nonNull(aString);
	ParameterCheck.nonNull(aRange);

	if (aRange.getStart() < 0) {
	    throw new RangeException("Range has negative start");
	}
	if(aRange.getMax() > aString.length()) {
	    throw new RangeException("Range extends beyond end of String");
	}
    }
    
    /**
     * Throws IllegalArgumentException if 'generalTest' is false; otherwise,
     * does nothing.  'message' will be the message of the exception. 'message'
     * may be null, but use of a meaningful message is recommended.
     *
     * @param	generalTest	value to verify as true
     * @param	message		message describing the failure
     * @exception  IllegalArgumentException	if 'generalTest' is false
     * @see #isFalse(boolean generalTest, String message)
     */
    static public void isTrue(boolean generalTest, String message) {
	if (!generalTest) {
	    throw new IllegalArgumentException(message);
	}
    }
    
    /**
     * Identical to isTrue, except the test is inverted.
     *
     * @param	generalTest	value to verify as false
     * @param	message		message describing the failure
     * @exception  IllegalArgumentException	if 'generalTest' is false
     * @see #isTrue(boolean generalTest, String message)
     */
    static public void isFalse(boolean generalTest, String message) {
	if (generalTest) {
	    throw new IllegalArgumentException(message);
	}
    }

} // End of class ParameterCheck
