/*
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.0 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is JSIRC Library
 *
 * The Initial Developer of the Original Code is Josh Gough. Portions created
 * by Josh Gough. Copyright (C) 1999 Josh Gough. All Rights Reserved.
 */
 
 /**
  * Implements a history for a text box. 
  * Usage:
  * <input id="some_input_element">
  * var entry = document.getElementById("some_input_element)"
  * var HistObj = new EntryHistory(max_lines, widget);
  *
  * By default, the constructor assigns the default onkeyup function
  * handler to the passed in widget, but you can also do:
  * entry.onkeyup = f; (where f = function(event) { etc } )  
  *
  * Date: August 24 1999
  * Author: Josh Gough <exv@randomc.com>
  */
function EntryHistory(max_items, entry_widget, perform_func) {
	this.max_items = max_items || 10;
	this.entry_widget = entry_widget;
	this.perform_func = perform_func || null;
	
//	this.items = new Array(this.max_items);
	this.items = new Array();
	this.current_index = 0;
	
	this.addItem = addItem;
	this.getItem = getItem;
	
	this.entry_widget.onkeyup = EntryHistory.DEFAULT_KEYUP_HANDLER;
	this.entry_widget.HistoryManager = this;
	
	function addItem(str) {
		if (this.items.length + 1 > this.max_items) 
			this.items.pop();
		dump("Got this: " + str + "\n");
		this.items.unshift(str);
		this.current_index = -1;
	}

	function getItem(direction) {
		if (this.items.length == 0)
			return "";
		else {
		if (direction == EntryHistory.UP) {
			if (this.current_index < this.items.length- 1) 				
				this.current_index++;
			else 
				this.current_index = this.items.length -1;
		} else if (direction == EntryHistory.DOWN) {
			if (this.current_index >= 1)
				this.current_index--;
			else 
				this.current_index = 0;			
		}
		}		
		dump("The index: " + this.current_index + "\n");
		return this.items[this.current_index];
	}	
	
}
EntryHistory.UP = 1;
EntryHistory.DOWN = 0;
EntryHistory.DEFAULT_KEYUP_HANDLER = function(event) {
	var textval = this.value;
	if (event.which == 13) {
		this.HistoryManager.addItem(this.value);
        	this.value = "";
     	         if (this.HistoryManager.perform_func) 
			this.HistoryManager.perform_func(textval);

         }  
         else if (event.which == 33)
         	this.value = this.HistoryManager.getItem(EntryHistory.UP);
         else if (event.which == 34)
                this.value = this.HistoryManager.getItem(EntryHistory.DOWN);
}
