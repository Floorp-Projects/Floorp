/*
 * TreeEvent.java
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
 */

package dog.util;

import java.awt.*;
import java.awt.event.*;
import java.util.EventObject;

/**
 * A tree event.
 *
 * @author dog@dog.net.uk
 * @version 2.0alpha
 */
public class TreeEvent extends ItemEvent {

	/**
	 * The item collapsed state change type.
	 */
	public static final int COLLAPSED = 3;
	
	/** 
	 * The item expanded state change type.
	 */
	public static final int EXPANDED = 4;
	
	/**
	 * Constructs a TreeEvent object with the specified ItemSelectable source,
	 * type, item, and item select state.
	 * @param source the ItemSelectable object where the event originated
	 * @id the event type
	 * @item the item where the event occurred
	 * @stateChange the state change type which caused the event
	 */
	public TreeEvent(ItemSelectable source, int id, Object item, int stateChange) {
		super(source, id, item, stateChange);
	}

	public String paramString() {
		Object item = getItem();
		int stateChange = getStateChange();
		
		String typeStr;
		switch (id) {
		case ITEM_STATE_CHANGED:
			typeStr = "ITEM_STATE_CHANGED";
			break;
		default:
			typeStr = "unknown type";
		}
		
		String stateStr;
		switch (stateChange) {
		case SELECTED:
			stateStr = "SELECTED";
			break;
		case DESELECTED:
			stateStr = "DESELECTED";
			break;
		case COLLAPSED:
			stateStr = "COLLAPSED";
			break;
		case EXPANDED:
			stateStr = "EXPANDED";
			break;
		default:
			stateStr = "unknown type";
		}
		return typeStr + ",item="+item + ",stateChange="+stateStr;
	}
	
}