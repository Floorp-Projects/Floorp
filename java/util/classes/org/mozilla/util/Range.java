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

import java.io.Serializable;
import java.lang.Cloneable;
import java.lang.Math;

/**
 * An Range is an object representing a range of integer values.  A 
 * range has a start index and a count.  The count must be greater than or 
 * equal to 0. Instances of this class are immutable, but subclasses may 
 * introduce mutability.  A mutable range will return true from 
 * isMutable().  
 *
 * @version  $Id: Range.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $
 * @author   The LFC Team (Rob Davis, Paul Kim, Alan Chung, Ray Ryan, etc)
 * @author   David-John Burrowes (he moved it to the AU package)
 * @see Range#isMutable()
 * <!-- see MutableRange -->
 */

public class Range extends Object implements Cloneable, Serializable {

    //
    // STATIC VARIABLES
    //
 
    /**
     * The RCSID for this class.
     */
    private static final String RCSID =
        "$Id: Range.java,v 1.1 1999/07/30 01:02:58 edburns%acm.org Exp $";

    /**
     *   A zero range
     */
    static public final Range ZeroRange = new Range(-1, 0);
    
    //
    // Instance Variables
    //

    /**
     * The start index of the range
     */
    protected int start;

    /**
     * The length of the range
     */
    protected int count;
       
    //
    //  Constructors
    //
    
    /**
     * Creates an instance of Range from another Range object 
     * otherRange which must be non-null.
     *
     * @param	range	the reference range to create this range from	
     * @exception  IllegalArgumentException if otherRange is null
     */
    public Range(Range otherRange) {
	//
	// Do our own construction rather than calling Range(int,int)
	// because we want to be able to do the parameter check.  There is
	// another way that allows us to use the other constructor, but
	// this class is too simple to be worth the exta work.
	//
	super();
	ParameterCheck.nonNull(otherRange);

	this.start = otherRange.getStart();
	this.count = otherRange.getCount();
    }

    /**
     * Creates an instance of Range with a start index newStart and 
     * an extent newCount.  newCount must be greater than or equal to 
     * 0.
     *
     * @param	newStart	the start index of the range
     * @param	newCount	the number of elements in the range
     * @exception  RangeException	if newCount is less than 0.
     */
    public Range(int newStart, int newCount) {
	super();

	this.start = newStart;
	this.count = newCount;
    }
 
    //
    // PROPERTY ACCESS
    //
        
    /**
     * Returns the start index of the range.
     *
     * @return	the start index of this range
     */
    public synchronized int getStart() {
	return this.start;
    }
    
    /**
     * Returns the number of elements in this range.
     *
     * @return	the number of elements in this range
     */
    public synchronized int getCount() {
	return this.count;
    }
    
    /**
     * Returns the the last index contained within the range.
     *
     * @return	the end index of this range
     */
    public synchronized int getEnd() {
	return this.getStart() + this.getCount() - 1;
    }
    
    /**
     * Returns the max index (the index at start + count).
     *
     * @return	the max index.
     */
    public synchronized int getMax() {
	return this.getStart() + this.getCount();
    }
    
    /**
     * Returns a number guaranteed to be within this range, including 
     * endpoints
     *
     * @return a number within the range, including endpoints.
     */
    protected int getConstrainedInt(int number) {
	int rc;
	
	if (number < this.getStart()) {
	    rc = this.getStart();
	}
	else {
	    if (number > this.getEnd()) {
		rc = this.getEnd();
	    } else {
		rc = number;
	    }
	}
	return rc;
    }

    //
    // METHODS
    //

    /**
     * Returns true if and only if index lies within this range.
     * Note that if the receiver is of zero length, then this method will return
     * false.
     *
     * @return	true if and only if and only if index lies within this 
     * 		range
     */
    public synchronized boolean containsIndex(int index) {
	if (this.getCount() == 0) {
	    return false;
	} else {
	    return (index >= this.getStart()) && (index <= this.getEnd());
	}
    }
    
    /**
     * Returns true if and only if the start index of this range is
     * greater than index.
     *
     * @return	true if and only if the start index of this range is
     *		greater than index
     */
    public synchronized boolean isAfterIndex(int index){
	return (this.getStart() > index);
    }
    
    
    /**
     * Return true if and only if the end index of this range is less than 
     * index.
     *
     * @return	true if and only if the end index of this range is less than 
     * 		index
     */
    public synchronized boolean isBeforeIndex(int index)
    {
	return (this.getEnd() < index);
    }
    
    /**
     * Returns true if and only if every element in otherRange is contained
     * in this range.  If the receiver is a zero length range, then
     * this method will return false.  Note that containment can apply to
     * zero length ranges; a non-zero-length range contains any zero-length
     * range.  Contrast with range intersection.
     *
     * @see     #intersectsWithRange(Range otherRange)
     * @return 	true if and only if every element in otherRange is 
     * 		contained in this range	
     * @exception IllegalArgumentException if otherRange is null
     *
     */
    public synchronized boolean containsRange (Range otherRange)
    {
	ParameterCheck.nonNull(otherRange);

	if (this.getCount() == 0) {
	    return false;
	} else if (otherRange.getCount() == 0) {
	    return true;
	}
	return ((this.getStart() <= otherRange.getStart()) &&
		(this.getEnd() >= otherRange.getEnd()));
    }
    
    /**
     * Returns true if and only if otherRange intersects with this range. A
     * zero-length range intersects with no range.
     *
     * @return	true if and only if otherRange intersects with this range
     * @exception  IllegalArgumentException if otherRange is null
     *
     */
    public synchronized boolean intersectsWithRange(Range otherRange) {
	return this.overlapWithRange(otherRange) > 0;
    }
    
    /**
     * Returns the number of elements that are in both this range and
     * otherRange. A zero-length range has no overlapping elements
     * with any range.
     *
     * @param	a range to check overlaps with this range
     * @exception  IllegalArgumentException if otherRange is null
     * @return	number of elements in both this range and otherRange
     */
    public synchronized int overlapWithRange(Range otherRange) {
	if (this.getCount() == 0 || otherRange.getCount() == 0) {
	    return 0;
	} else {
	    return Math.max(0, (Math.min(this.getEnd(), otherRange.getEnd()) -
				Math.max(this.getStart(), 
				otherRange.getStart())) + 1);
	}
    }
    
    /**
     * Returns true if and only if otherRange is adjacent to this range; 
     * two ranges are adjacent if the max of one range is equal to the start of 
     * the other.  A zero length range is adjacent to no range.
     *
     * @return	true if and only if otherRange is adjacent to this range
     * @exception  IllegalArgumentException if otherRange is null
     */
    public synchronized boolean isAdjacentToRange(Range otherRange) {
	ParameterCheck.nonNull(otherRange);

	if (this.getCount() == 0 || otherRange.getCount() == 0) {
	    return false;
	}
	return (Math.max(this.getStart(), otherRange.getStart()) ==
		(Math.min(this.getEnd(), otherRange.getEnd()) + 1));
    }

    /**
     *
     * Returns true if and only if this range is before otherRange
     * and the two ranges do not overlap.
     * 
     * @return true if and only if this range is before otherRange
     *        and the two ranges do not overlap.
     * @exception  IllegalArgumentException if otherRange is null
     */
    public synchronized boolean isBeforeRange(Range otherRange) {
	ParameterCheck.nonNull(otherRange);
	
	if (this.getEnd() < otherRange.getStart()) {
	    return true;
	} else {
	    return false;
	}
    }
    
    /**
     *
     * Returns true if this range is after otherRange and the two
     * ranges do not overlap.
     *
     * @return true if this range is after otherRange and the two
     *        ranges do not overlap.
     * @exception  IllegalArgumentException if otherRange is null
     */
    protected boolean isAfterRange(Range otherRange) {
	ParameterCheck.nonNull(otherRange);
	
	return otherRange.isBeforeRange(this);
    }
    
    
    /**
     * Returns the intersection of this range and otherRange. If the
     * ranges do not intersect then Range.ZeroRange is returned.
     * Note that a zero length range has no intersection with a non-zero
     * length range.
     *
     * @return	the intersection of this range and otherRange
     * @exception  IllegalArgumentException if otherRange is null
     */
    public synchronized Range rangeFromIntersection(Range otherRange) {
	ParameterCheck.nonNull(otherRange);
    
	if (this.intersectsWithRange(otherRange)) {
	    int newStart = Math.max(this.getStart(), otherRange.getStart());
	    int newEnd = Math.min(this.getEnd(), otherRange.getEnd());
	    return new Range(newStart, newEnd - newStart + 1);
	}
	return Range.ZeroRange;
    }
    
    /**
     * Returns the union of this range and otherRange.
     * Returns the other range if one of them are zero length range.
     * Returns Range.ZeroRange if both are zero length range.Note that the 
     * union of a non-zero length range with a zero length range is merely the 
     * non-zero length range.
     *
     * @return	the union of this range and otherRange if both are
     *		non-zero range
     * @exception  IllegalArgumentException if otherRange is null
     */
    public synchronized Range rangeFromUnion(Range otherRange) {
	ParameterCheck.nonNull(otherRange);
	Range retRange;
	
	int thisRangeCount = this.getCount();
	int otherRangeCount = otherRange.getCount();
	
	if (thisRangeCount == 0 && otherRangeCount == 0) {
	    retRange = ZeroRange;
	} else if (thisRangeCount == 0) {
	    retRange = new Range(otherRange);
	} else if (otherRangeCount == 0) {
	    retRange = new Range(this);
	} else {
	    int newStart;
	    int newEnd;

	    newStart = Math.min(this.getStart(), otherRange.getStart());
	    newEnd = Math.max(this.getEnd(), otherRange.getEnd());
	    return new Range(newStart, newEnd - newStart + 1);
	}
	return retRange;
    }
    
    /**
     * Returns this range, with its start shifted by offset.
     *
     * @return	this range, with its start offset by offset.
     */
    public synchronized Range rangeShiftedByOffset(int offset) {
	return new Range(this.getStart() + offset, this.getCount());
    }
    
    /**
     * Returns a String representation of this Range. 
     *
     * @return	string representation of this range
     */
    public synchronized String toString() {
	return "start = " + this.getStart() + ", count = " + this.getCount();
    }
    
    
    /**
     * Returns true only if this instance can change after it is created.<P>
     *
     * The default implementation returns false because instances of this
     * class can't change; subclasses that introduce mutability should
     * override this method to return true.
     *
     * @return	true if and only if the range may change
     */
    public boolean isMutable() {
	return false;
    }
    
    
    /**
     * Creates and returns an Range that is identical to this one.
     *
     * @return	a reference to the immutable instance or
     *          a copy of the mutable instance
     */
    public synchronized Object clone() {
	if (this.isMutable()) {
	    Range newRange;
	    
	    try {
		newRange = (Range)super.clone();
	    }
	    catch (CloneNotSupportedException e) {
		// Won't happen because we implement Cloneable
		throw new InternalError(e.toString());
	    }
	    return newRange;
	}
	else {
	    // No reason to clone an immutable object
	    return this;
	}
    }
    
    /**
     * Returns true if and only if otherRange is an Range
     * with the same start and count as this range.
     *
     * @return	true if and only if otherRange equals this range
     */
    public synchronized boolean equals(Object otherRange) {
	return ((otherRange == this) ||
		((otherRange != null) &&
		    (otherRange instanceof Range) &&
		    (this.getStart() == ((Range)otherRange).getStart()) &&
		    (this.getEnd() == ((Range)otherRange).getEnd())));
    }
    
    /**
     * Overridden because equals() is overridden.  Returns a hash code 
     * that is based on the start and count of this range.
     *
     * @return	a hash code based on the start index and count of this range
     */ 
    public synchronized int hashCode() {
	return Math.round(this.getStart()) ^ Math.round(this.getCount());
    }

} // End of class Range

