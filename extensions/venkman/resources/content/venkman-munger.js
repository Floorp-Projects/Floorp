/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/ 
 * 
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License. 
 *
 * The Original Code is The JavaScript Debugger
 * 
 * The Initial Developer of the Original Code is
 * Netscape Communications Corporation
 * Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the MPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the MPL or the GPL.
 *
 * Contributor(s):
 *  Robert Ginda, <rginda@netscape.com>, original author
 *
 */

function CMungerEntry (name, regex, className, tagName)
{
    
    this.name = name;
    this.tagName = (tagName) ? tagName : "html:span";

    if (regex instanceof RegExp)
        this.regex = regex;
    else
        this.lambdaMatch = regex;
    
    if (typeof className == "function")
        this.lambdaReplace = className;
    else 
        this.className = className;
    
}

function CMunger () 
{
    
    this.entries = new Object();
    
}

CMunger.prototype.enabled = true;

CMunger.prototype.addRule =
function mng_addrule (name, regex, className)
{
    
    this.entries[name] = new CMungerEntry (name, regex, className);
    
}

CMunger.prototype.delRule =
function mng_delrule (name)
{

    delete this.entries[name];
    
}

CMunger.prototype.munge =
function mng_munge (text, containerTag, data)
{
    var entry;
    var ary;    

    if (!text) //(ASSERT(text, "no text to munge"))
        return "";
     
    if (typeof text != "string")
        text = String(text);

    if (!containerTag)
    {
        containerTag =
            document.createElementNS (NS_XHTML, this.tagName);
    }

    if (this.enabled)
    {
        for (entry in this.entries)
        {
            if (typeof this.entries[entry].lambdaMatch == "function")
            {
                var rval;
                
                rval = this.entries[entry].lambdaMatch(text, containerTag,
                                                       data,
                                                       this.entries[entry]);
                if (rval)
                    ary = [(void 0), rval];
                else
                    ary = null;
            }
            else
                ary = text.match(this.entries[entry].regex);
            
            if ((ary != null) && (ary[1]))
            {
                var startPos = text.indexOf(ary[1]);
                
                if (typeof this.entries[entry].lambdaReplace == "function")
                {
                    this.munge (text.substr(0,startPos), containerTag,
                                data);
                    this.entries[entry].lambdaReplace (ary[1], containerTag,
                                                       data,
                                                       this.entries[entry]);
                    this.munge (text.substr (startPos + ary[1].length,
                                             text.length), containerTag,
                                data);
                
                    return containerTag;
                }
                else
                {
                    this.munge (text.substr(0,startPos), containerTag,
                                data);
                    
                    var subTag = 
                        document.createElementNS (NS_XHTML,
                                                  this.entries[entry].tagName);

                    subTag.setAttribute ("class",
                                         this.entries[entry].className);
                    var wordParts = splitLongWord (ary[1],
                                                   client.MAX_WORD_DISPLAY);
                    for (var i in wordParts)
                    {
                        subTag.appendChild (document.createTextNode (wordParts[i]));
                        subTag.appendChild (htmlWBR());
                    }
                    
                    containerTag.appendChild (subTag);
                    this.munge (text.substr (startPos + ary[1].length,
                                             text.length), containerTag, data);

                    return containerTag;
                }
            }
        }
    }

    containerTag.appendChild (document.createTextNode (text));
    return containerTag;
    
}
