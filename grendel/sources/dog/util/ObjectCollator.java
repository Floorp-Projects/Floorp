/*
 * ObjectCollator.java
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
 * The Original Code is Knife.
 *
 * The Initial Developer of the Original Code is dog.
 * Portions created by dog are
 * Copyright (C) 1998 dog <dog@dog.net.uk>. All
 * Rights Reserved.
 *
 * Contributor(s): n/a.
 *
 * You may retrieve the latest version of this package at the knife home page,
 * located at http://www.dog.net.uk/knife/
 */

package dog.util;

import java.text.CollationKey;
import java.text.Collator;
import java.util.Date;
import java.util.Locale;

/**
 * A class for comparing objects in a tree.
 *
 * @author dog@dog.net.uk
 * @version 0.3
 */
public class ObjectCollator extends Collator {

	protected Collator collator;
	
	protected boolean descending = false;

	/**
	 * Constructs a ObjectCollator for the default locale.
	 */
	public ObjectCollator() {
		collator = Collator.getInstance();
	}
	
	/**
	 * Constructs a ObjectCollator for the specified locale.
	 */
	public ObjectCollator(Locale locale) {
		collator = Collator.getInstance(locale);
	}
	
	/**
	 * Indicates whether this collator returns the opposite of any comparison.
	 * @see #setDescending
	 */
	public boolean isDescending() { return descending; }
	
	/**
	 * Sets whether this collator returns the opposite of any comparison.
	 * @param descending true if this collator returns the opposite of any comparison, false otherwise
	 * @see #setDescending
	 */
	public void setDescending(boolean descending) { this.descending = descending; }
	
	/**
	 * Utility method to return the opposite of a comparison if descending is true.
	 */
	protected int applyDescending(int comparison) {
		return (!descending ? comparison : -comparison);
	}
	
	/**
	 * Compares the source string to the target string according to the collation rules for this Collator.
	 * Returns an integer less than, equal to or greater than zero depending on whether the source String 
	 * is less than, equal to or greater than the target string. 
	 * @param source the source string. 
	 * @param target the target string. 
	 * @see java.text.CollationKey
	 */
	public int compare(String source, String target) {
		return applyDescending(collator.compare(source, target));
	}
	
	/**
	 * Compares the source object to the target object according to the collation rules for this Collator.
	 * Returns an integer less than, equal to or greater than zero depending on whether the source object
	 * is less than, equal to or greater than the target object.
	 * Objects that can validly be compared by such means include:<ul>
	 * <li>Boolean - false is less than true, can only be compared with another Boolean
	 * <li>Byte
	 * <li>Date - before is less than after, can only be compared with another Date
	 * <li>Double
	 * <li>Float
	 * <li>Long
	 * <li>Integer
	 * <li>Short
	 * <li>String
	 * @param source the source object.
	 * @param target the target object.
	 */
	public int compare(Object source, Object target) {
		if (source instanceof String) {
			if (target instanceof String)
				return applyDescending(collator.compare((String)source, (String)target));
			else if (target instanceof Number)
				return applyDescending(collator.compare((String)source, target.toString()));
		} else if (source instanceof Integer) {
			if (target instanceof Number)
				return applyDescending(((Number)target).intValue()-((Integer)source).intValue());
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Double) {
			if (target instanceof Number)
				return applyDescending((int)(((Number)target).doubleValue()-((Double)source).doubleValue()));
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Long) {
			if (target instanceof Number)
				return applyDescending((int)(((Number)target).longValue()-((Long)source).longValue()));
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Float) {
			if (target instanceof Number)
				return applyDescending((int)(((Number)target).floatValue()-((Float)source).floatValue()));
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Byte) {
			if (target instanceof Number)
				return applyDescending((int)(((Number)target).byteValue()-((Byte)source).byteValue()));
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Short) {
			if (target instanceof Number)
				return applyDescending((int)(((Number)target).shortValue()-((Short)source).shortValue()));
			else if (target instanceof String)
				return applyDescending(collator.compare(source.toString(), (String)target));
		} else if (source instanceof Boolean) {
			if (target instanceof Boolean) {
				boolean s = ((Boolean)source).booleanValue(), t = ((Boolean)target).booleanValue();
				if (!s && t) return applyDescending(-1); else if (s && !t) return applyDescending(1);
			}
		} else if (source instanceof Date) {
			if (target instanceof Date) {
				Date s = (Date)source, t = (Date)target;
				if (s.before(t)) return applyDescending(-1); else if (s.after(t)) return applyDescending(1);
			}
		}
		return 0;
	}
	
	/**
	 * Transforms the String into a series of bits that can be compared bitwise to other CollationKeys. 
	 * CollationKeys provide better performance than Collator.compare when Strings are involved in multiple comparisons.
	 * @param source the string to be transformed into a collation key. 
	 * @return the CollationKey for the given String based on this Collator's collation rules. If the source String is null, a null CollationKey is returned. 
	 * @see java.text.CollationKey
	 * @see #compare 
	 */
	public CollationKey getCollationKey(String source) {
		return collator.getCollationKey(source);
	}

	/**
	 * Convenience method for comparing the equality of two strings based on this Collator's collation rules. 
	 * @param source the source string to be compared with. 
	 * @param target the target string to be compared with. 
	 * @return true if the strings are equal according to the collation rules, false otherwise. 
	 * @see #compare 
	 */
	public boolean equals(String source, String target) {
		return collator.equals(source, target);
	}
	
	/**
	 * Returns this Collator's strength property.
	 * The strength property determines the minimum level of difference considered significant during comparison.
	 * See the Collator class description for an example of use. 
	 * @return this Collator's current strength property. 
	 * @see #setStrength
	 */
	public int getStrength() { return collator.getStrength(); }

	/**
	 * Sets this Collator's strength property.
	 * The strength property determines the minimum level of difference considered significant during comparison.
	 * See the Collator class description for an example of use. 
	 * @param newStrength the new strength value. 
	 * @exception IllegalArgumentException if the new strength value is not one of PRIMARY, SECONDARY, TERTIARY or IDENTICAL. 
	 * @see #getStrength
	 */
	public void setStrength(int newStrength) { collator.setStrength(newStrength); }
	
	/**
	 * Get the decomposition mode of this Collator.
	 * Decomposition mode determines how Unicode composed characters are handled.
	 * Adjusting decomposition mode allows the user to select between faster and more complete collation behavior. 
	 * The three values for decomposition mode are: <ul>
	 * <li>NO_DECOMPOSITION, 
	 * <li>CANONICAL_DECOMPOSITION 
	 * <li>FULL_DECOMPOSITION. 
	 * </ul>See the documentation for these three constants for a description of their meaning. 
	 * @return the decomposition mode 
	 * @see #setDecomposition
	 */
	public int getDecomposition() { return collator.getDecomposition(); }

	/**
	 * Set the decomposition mode of this Collator.
	 * See getDecomposition for a description of decomposition mode. 
	 * @param decompositionMode the new decomposition mode 
	 * @throws IllegalArgumentException if the given value is not a valid decomposition mode. 
	 * @see #getDecomposition
	 */
	public void setDecomposition(int decompositionMode) { collator.setDecomposition(decompositionMode); }
		
	public int hashCode() { return collator.hashCode(); }
	
	public boolean equals(Object other) {
		if (other instanceof ObjectCollator) {
			ObjectCollator oc = (ObjectCollator)other;
			return (collator.equals(oc.collator));
		}
		return false;
	}
	
	/**
	 * Provides a string description of this object.
	 */
	public String toString() {	
		StringBuffer buffer = new StringBuffer();
		buffer.append(getClass().getName());
		buffer.append("[");
		buffer.append(paramString().toString());
		buffer.append("]");
		return buffer.toString();
	}
	
	protected StringBuffer paramString() {
		StringBuffer buffer = new StringBuffer();
		buffer.append("strength=");
		switch (getStrength()) {
		case PRIMARY:
			buffer.append("PRIMARY");
			break;
		case SECONDARY:
			buffer.append("SECONDARY");
			break;
		case TERTIARY:
			buffer.append("TERTIARY");
			break;
		case IDENTICAL:
			buffer.append("IDENTICAL");
			break;
		default:
			buffer.append("unknown");
		}
		buffer.append(",decomposition=");
		switch (getDecomposition()) {
		case NO_DECOMPOSITION:
			buffer.append("NO_DECOMPOSITION");
			break;
		case CANONICAL_DECOMPOSITION:
			buffer.append("CANONICAL_DECOMPOSITION");
			break;
		case FULL_DECOMPOSITION:
			buffer.append("FULL_DECOMPOSITION");
			break;
		default:
			buffer.append("unknown");
		}
		if (descending) buffer.append(",descending");
		return buffer;
	}
	
}
