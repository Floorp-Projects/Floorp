/*
 * StatusEvent.java
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

import java.util.*;

/**
 * A status message.
 *
 * @author dog@dog.net.uk
 * @version 0.1
 */
public class StatusEvent extends EventObject {

	public static final int OPERATION_START = 0;

	public static final int OPERATION_UPDATE = 1;

	public static final int OPERATION_END = 2;

	public static final int UNKNOWN = -1;

	protected int type;

	protected String operation;

	protected int minimum = UNKNOWN;
	
	protected int maximum = UNKNOWN;
	
	protected int value = UNKNOWN;
	
	/**
	 * Creates a new status event with the specified type and operation.
	 */
	public StatusEvent(Object source, int type, String operation) {
		super(source);
		switch (type) {
		  case OPERATION_START:
		  case OPERATION_UPDATE:
		  case OPERATION_END:
			this.type = type;
			break;
		  default:
			throw new IllegalArgumentException("Illegal event type: "+type);
		}
		this.operation = operation;
	}
	
	/**
	 * Creates a new status event representing an update of the specified operation.
	 */
	public StatusEvent(Object source, int type, String operation, int minimum, int maximum, int value) {
		super(source);
		switch (type) {
		  case OPERATION_START:
		  case OPERATION_UPDATE:
		  case OPERATION_END:
			this.type = type;
			break;
		  default:
			throw new IllegalArgumentException("Illegal event type: "+type);
		}
		this.operation = operation;
		this.minimum = minimum;
		this.maximum = maximum;
		this.value = value;
	}

	/**
	 * Returns the type of event (OPERATION_START, OPERATION_UPDATE, or OPERATION_END).
	 */
	public int getType() { return type; }

	/**
	 * Returns a string describing the operation being performed.
	 */
	public String getOperation() { return operation; }

	/**
	 * Returns the start point of the operation.
	 */
	public int getMinimum() { return minimum; }

	/**
	 * Returns the end point of the operation.
	 */
	public int getMaximum() { return maximum; }

	/**
	 * Returns the current point in the operation.
	 */
	public int getValue() { return value; }

}