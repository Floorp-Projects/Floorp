/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */

function createHTML(tag) {
    return document.createElementWithNameSpace(tag, "http://www.w3.org/TR/REC-html40");
}

function createXUL(tag) {
    return document.createElementWithNameSpace(tag, "http://www.mozilla.org/keymaster/gatekeeper/there.is.only.xul");
}

function createOption(label,value) {
    var opt = new Option(label, value);
    return opt;
}

function createTextField() {
    var text = createHTML("INPUT");
    text.type = "text";

    return text;
}

function createStringSelect() {
    var sel = createHTML("select");

    addStringComparators(sel);
    
    return sel;
}

function addStringComparators(selWidget) {
    var opt1=createOption("contains", "contains");
    var opt2=createOption("doesn't contain", "nocontain");

    selWidget.add(opt1, null);
    selWidget.add(opt2, null);
}

function addDateComparators(selWidget) {
    var opt1=createOption("is before", "before");
    var opt2=createOption("is after",  "after");

    selWidget.add(opt1, null);
    selWidget.add(opt2, null);
}

function addNumericComparators(selWidget) {
    var opt1=createOption("is", "is");
    var opt2=createOption("is greater than", "gt");
    var opt3=createOption("is less than", "lt");

    selWidget.add(opt1, null);
    selWidget.add(opt2, null);
    selWidget.add(opt3, null);
    
}
// eventually this will come from the search service
function createFieldSelect() {
    var sel = createHTML("select");
    var opt1=createOption("subject", "subject");
    var opt2=createOption("sender", "sender");
    var opt3=createOption("date", "date");
    var opt4=createOption("text", "any text");
    var opt5=createOption("keyword", "keyword");
    var opt6=createOption("age in days", "age");

    sel.add(opt1, null);
    sel.add(opt2, null);
    sel.add(opt3, null);
    sel.add(opt4, null);
    sel.add(opt5, null);
    sel.add(opt6, null);
    sel.onChange = OnFieldSelect;
    sel.onchange = OnFieldSelect;
    return sel;
}

function createRow() {
    var result = createHTML("DIV");

    var fieldSelect = createFieldSelect();
    var containSelect = createStringSelect();
    var fieldText = createTextField();
    
    fieldText.setAttribute("flex", "100%");
    
    result.appendChild(fieldSelect);
    result.appendChild(containSelect);
    result.appendChild(fieldText);
    return result;
}

function OnMore(event) {
    var parentBox = document.getElementById("criteriabox");
    parentBox.appendChild(createRow());

    var sel = document.getElementById("zoober");
}

function OnLess(event) {
    var parentBox = document.getElementById("criteriabox");
    if (parentBox.childNodes.length > 1) {
        parentBox.removeChild(parentBox.lastChild);
    }
}
function OnFieldSelect(event) {
    var selWidget = event.target;
    var selectedValue = selWidget.value;
    dump(selectedValue + " was just selected\n");

    dump("event.target is " + event.target + "\n");
    var comp = selWidget.parentNode;
    while (selWidget.length)
        selWidget.remove(0);
    
    if (selectedValue == "subject" ||
        selectedValue == "sender" ||
        selectedValue == "text" ||
        selectedValue == "keyword") {
        addStringComparators(comp);

    } else if (selectedValue == "age") {
        addNumericComparators(comp);
    } else if (selectedValue == "date") {
        addDateComparators(comp);
    }
}

function OnComparatorSelect(event) {
    var selWidget = event.target;

    var selectedValue = selWidget.value;
    dump(selectedValue + " was just selected\n");
    
}
// onchange="OnFieldSelect(event);"

function onOptions(event) {
    window.openDialog("chrome://messenger/content/SearchOptions.xul", "options", "chrome,modal,height=100,width=70");
}
