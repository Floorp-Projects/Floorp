/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
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
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. Copyright (C) 1999
 * New Dimenstions Consulting, Inc. All Rights Reserved.
 *
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 */

function CListBox ()
{

    this.listContainer = document.createElement ("html:a");
    this.listContainer.setAttribute ("class", "list");

}

CListBox.prototype.clear =
function lbox_clear (e)
{
    var obj = this.listContainer.firstChild;

    while (obj)
    {
        this.listContainer.removeChild (obj);
        obj = obj.nextSibling;    
    }    

}

CListBox.prototype.clickHandler =
function lbox_chandler (e)
{
    var lm = this.listManager;
    
    e.target = this;
    if (lm && typeof lm.onClick == "function")
        lm.onClick ({realTarget: this, event: e});
    
}   

CListBox.prototype.onClick =
function lbox_chandler (e)
{

    dd ("onclick: \n" + dumpObjectTree (e, 1));
    
}

CListBox.prototype.add =
function lbox_add (stuff, tag)
{
    var option = document.createElement ("html:a");
    option.setAttribute ("class", "list-option");

    option.appendChild (stuff);
    option.appendChild (document.createElement("html:br"));
    option.onclick = this.clickHandler;
    option.listManager = this;
    option.tag = tag;
    this.listContainer.appendChild (option);
    
    return true;
    
}

CListBox.prototype.prepend =
function lbox_prepend (stuff, oldstuff, tag)
{
    if (!this.listContainer.firstChild)
        return this.add (stuff, tag);

    var nextOption = this._findOption (oldstuff);
    if (!nextOption)
        return false;
    
    var option = document.createElement ("html:a");
    option.setAttribute ("class", "list-option");

    option.appendChild (stuff);
    option.appendChild (document.createElement("html:br"));
    option.onclick = this.clickHandler;
    option.listManager = this;
    option.tag = tag;
    this.listContainer.insertBefore (option, nextOption);
    
}

CListBox.prototype.insert =
function lbox_Insert (stuff, tag)
{
    this.prepend (stuff, this.listContainer.firstChild, tag);
}

CListBox.prototype.remove =
function lbox_remove (stuff)
{
    var option = this._findOption (stuff);

    if (option)
        this.listContainer.removeChild (option);

    return;
}

CListBox.prototype._findOption =
function lbox_remove (stuff)
{
    var option = this.listContainer.firstChild;

    while (option)
    {
        if (option.firstChild == stuff)
            return option;
        option = option.nextSibling;
    }

    return;
}

CListBox.prototype.enumerateElements =
function lbox_enum (callback)
{
    var i = 0;
    var current = this.listContainer.firstChild;
    
    while (current)
    {
        callback (current, i++);
        current = current.nextSibling;
    }
    
}
