/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is ChatZilla
 *
 * The Initial Developer of the Original Code is New Dimensions Consulting,
 * Inc. Portions created by New Dimensions Consulting, Inc. are
 * Copyright (C) 1999 New Dimenstions Consulting, Inc. All
 * Rights Reserved.
 *
 * Contributor(s):
 *  Robert Ginda, rginda@ndcico.com, original author
 *  Samuel Sieb, samuel@sieb.net, MIRC color codes
 */

function initMunger()
{
    client.linkRE =
        /((\w[\w-]+):[^<>\[\]()\'\"\s\u201d]+|www(\.[^.<>\[\]()\'\"\s\u201d]+){2,})/;    

    var munger = client.munger = new CMunger();
    munger.addRule ("quote", /(``|'')/, insertQuote);
    munger.addRule ("bold", /(?:\s|^)(\*[^*()]*\*)(?:[\s.,]|$)/, 
                    "chatzilla-bold");
    munger.addRule ("underline", /(?:\s|^)(\_[^_()]*\_)(?:[\s.,]|$)/,
                    "chatzilla-underline");
    munger.addRule ("italic", /(?:\s|^)(\/[^\/()]*\/)(?:[\s.,]|$)/,
                    "chatzilla-italic");
    /* allow () chars inside |code()| blocks */
    munger.addRule ("teletype", /(?:\s|^)(\|[^|]*\|)(?:[\s.,]|$)/,
                    "chatzilla-teletype");
    munger.addRule (".mirc-colors", /(\x03((\d{1,2})(,\d{1,2}|)|))/,
                    mircChangeColor);
    munger.addRule (".mirc-bold", /(\x02)/, mircToggleBold);
    munger.addRule (".mirc-underline", /(\x1f)/, mircToggleUnder);
    munger.addRule (".mirc-color-reset", /(\x0f)/, mircResetColor);
    munger.addRule (".mirc-reverse", /(\x16)/, mircReverseColor);
    munger.addRule ("ctrl-char", /([\x01-\x1f])/, showCtrlChar);
    munger.addRule ("link", client.linkRE, insertLink);
    munger.addRule ("mailto",
       /(?:\s|\W|^)((mailto:)?[^<>\[\]()\'\"\s\u201d]+@[^.<>\[\]()\'\"\s\u201d]+\.[^<>\[\]()\'\"\s\u201d]+)/i,
                    insertMailToLink);
    munger.addRule ("bugzilla-link", /(?:\s|\W|^)(bug\s+#?\d{3,6})/i,
                    insertBugzillaLink);
    munger.addRule ("channel-link",
                /(?:\s|\W|^)[@+]?(#[^<>\[\](){}\"\s\u201d]*[^:,.<>\[\](){}\'\"\s\u201d])/i,
                    insertChannelLink);
    
    munger.addRule ("face",
         /((^|\s)[\<\>]?[\;\=\:]\~?[\-\^\v]?[\)\|\(pP\<\>oO0\[\]\/\\](\s|$))/,
         insertSmiley);
    munger.addRule ("ear", /(?:\s|^)(\(\*)(?:\s|$)/, insertEar, false);
    munger.addRule ("rheet", /(?:\s|\W|^)(rhee+t\!*)(?:\s|$)/i, insertRheet);
    munger.addRule ("word-hyphenator",
                    new RegExp ("(\\S{" + client.MAX_WORD_DISPLAY + ",})"),
                    insertHyphenatedWord);

    client.enableColors = client.prefs["munger.colorCodes"];
    for (var entry in client.munger.entries)
    {
        var branch = client.prefManager.prefBranch;
        if (entry[0] != ".")
        {
            try
            {
                munger.entries[entry].enabled = 
                    branch.getBoolPref("munger." + entry);
            }
            catch (ex)
            {
                // nada
            }
        }
    }
}

function CMungerEntry (name, regex, className, enable, tagName)
{
    this.name = name;
    if (name[0] != ".")
        this.description = getMsg("munger." + name, null, null);
    this.enabled = (typeof enable == "undefined" ? true : enable);
    this.enabledDefault = this.enabled;
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
    this.tagName = "html:span";
    this.enabled = true;
}

CMunger.prototype.enabled = true;

CMunger.prototype.addRule =
function mng_addrule (name, regex, className, enable)
{
    this.entries[name] = new CMungerEntry (name, regex, className, enable);
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
    var wbr, newClass;
    
    if (!containerTag)
    {
        containerTag =
            document.createElementNS ("http://www.w3.org/1999/xhtml",
                                      this.tagName);
    }

    if (this.enabled)
    {
        for (entry in this.entries)
        {
            if (this.entries[entry].enabled)
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
 
                        var subTag = document.createElementNS
                            ("http://www.w3.org/1999/xhtml",
                             this.entries[entry].tagName);

                        newClass = this.entries[entry].className;

                        if ("hasColorInfo" in data)
                        {
                            if ("currFgColor" in data)
                                newClass += " chatzilla-fg" + data.currFgColor;
                            if ("currBgColor" in data)
                                newClass += " chatzilla-bg" + data.currBgColor;
                            if ("isBold" in data)
                                newClass += " chatzilla-bold";
                            if ("isUnderline" in data)
                                newClass += " chatzilla-underline";
                        }

                        subTag.setAttribute ("class", newClass);

                        /* don't let this rule match again */
                        this.entries[entry].enabled = false;
                        this.munge(ary[1], subTag, data);
                        this.entries[entry].enabled = true;
 
                        containerTag.appendChild (subTag);

                        this.munge (text.substr (startPos + ary[1].length,
                                                 text.length), containerTag,
                                                 data);

                        return containerTag;
                    }
                }
            }
        }
    }

    var textNode = document.createTextNode (text);

    if ("hasColorInfo" in data)
    {

        newClass = "";
        if ("currFgColor" in data)
            newClass = "chatzilla-fg" + data.currFgColor;
        if ("currBgColor" in data)
            newClass += " chatzilla-bg" + data.currBgColor;
        if ("isBold" in data)
            newClass += " chatzilla-bold";
        if ("isUnderline" in data)
            newClass += " chatzilla-underline";
        if (newClass != "")
        {
            var newTag = document.createElementNS
                ("http://www.w3.org/1999/xhtml",
                 "html:span");
            newTag.setAttribute ("class", newClass);
            newTag.appendChild (textNode);
            containerTag.appendChild (newTag);
        }
        else
        {
            delete data.hasColorInfo;
            containerTag.appendChild (textNode);
        }
        wbr = document.createElementNS ("http://www.w3.org/1999/xhtml",
                                        "html:wbr");
        containerTag.appendChild (wbr);
    }
    else
        containerTag.appendChild (textNode);

    return containerTag;
}
