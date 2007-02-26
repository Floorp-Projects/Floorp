/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * New Dimensions Consulting, Inc.
 * Portions created by the Initial Developer are Copyright (C) 1999
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Robert Ginda, rginda@ndcico.com, original author
 *   Samuel Sieb, samuel@sieb.net, MIRC color codes
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

/* Constructs a new munger entry, using a regexp or lambda match function, and
 * a class name (to be applied by the munger itself) or lambda replace
 * function, and the default enabled state and a start priority (used if two
 * rules match at the same index), as well as a default tag (when the munger
 * adds it based on the class name) name.
 *
 * Regular Expressions for matching should ensure that the first capturing
 * group is the one that contains the matched text. Non-capturing groups, of
 * zero-width or otherwise can be used before and after, to ensure the right
 * things are matched (e.g. to ensure whitespace before something).
 *
 * Note that for RegExp matching, the munger will search for the matched text
 * (from the first capturing group) from the leftmost point of the entire
 * match. This means that if the text that matched the first group occurs in
 * any part of the match before the group, the munger will apply to the wrong
 * bit. This is not usually a problem, but if it is you should use a
 * lambdaMatch function and be sure to return the new style return value,
 * which specifically indicates the start.
 *
 * The lambda match and lambda replace functions have this signature:
 *   lambdaMatch(text, containerTag, data, mungerEntry)
 *   lambdaReplace(text, containerTag, data, mungerEntry)
 *     - text is the entire text to find a match in/that has matched
 *     - containerTag is the element containing the text (not useful?)
 *     - data is a generic object containing properties kept throughout
 *     - mungerEntry is the CMungerEntry object for the munger itself
 *
 *   The lambdaReplace function is expected to do everything needed to put
 *   |text| into |containerTab| ready for display.
 *
 *   The return value for lambda match functions should be either:
 *     - (old style) just the text that matched
 *       (the munger will search for this text, and uses the first match)
 *     - (new style) an object with properties:
 *       - start (start index, 0 = first character)
 *       - text  (matched text)
 *       (note that |text| must start at index |start|)
 *
 *   The return value for lambda replace functions are not used.
 *
 */

const NS_XHTML = "http://www.w3.org/1999/xhtml";

function CMungerEntry(name, regex, className, priority, startPriority,
                      enable, tagName)
{
    this.name = name;
    if (name[0] != ".")
        this.description = getMsg("munger." + name, null, null);
    this.enabled = (typeof enable == "undefined" ? true : enable);
    this.enabledDefault = this.enabled;
    this.startPriority = (startPriority) ? startPriority : 0;
    this.priority = priority;
    this.tagName = (tagName) ? tagName : "html:span";

    if (isinstance(regex, RegExp))
        this.regex = regex;
    else
        this.lambdaMatch = regex;

    if (typeof className == "function")
        this.lambdaReplace = className;
    else
        this.className = className;
}

function CMunger()
{
    this.entries = new Array();
    this.tagName = "html:span";
    this.enabled = true;
}

CMunger.prototype.enabled = true;

CMunger.prototype.getRule =
function mng_getrule(name)
{
    for (var p in this.entries)
    {
        if (isinstance(this.entries[p], Object))
        {
            if (name in this.entries[p])
                return this.entries[p][name];
        }
    }
}

CMunger.prototype.addRule =
function mng_addrule(name, regex, className, priority, startPriority, enable)
{
    if (typeof this.entries[priority] != "object")
        this.entries[priority] = new Object();
    var entry = new CMungerEntry(name, regex, className, priority,
                                 startPriority, enable);
    this.entries[priority][name] = entry;
}

CMunger.prototype.delRule =
function mng_delrule(name)
{
    for (var i in this.entries)
    {
        if (typeof this.entries[i] == "object")
        {
            if (name in this.entries[i])
                delete this.entries[i][name];
        }
    }
}

CMunger.prototype.munge =
function mng_munge(text, containerTag, data)
{

    if (!containerTag)
        containerTag = document.createElementNS(NS_XHTML, this.tagName);

    // Starting from the top, for each valid priority, check all the rules,
    // return as soon as something matches.
    if (this.enabled)
    {
        for (var i = this.entries.length - 1; i >= 0; i--)
        {
            if (i in this.entries)
            {
                if (this.mungePriority(i, text, containerTag, data))
                    return containerTag;
            }
        }
    }

    // If nothing matched, we don't have to do anything,
    // just insert text (if any).
    if (text)
        insertText(text, containerTag, data);
    return containerTag;
}

CMunger.prototype.mungePriority =
function mng_mungePriority(priority, text, containerTag, data)
{
    var matches = new Object();
    var entry;
    // Find all the matches in this priority
    for (entry in this.entries[priority])
    {
        var munger = this.entries[priority][entry];
        if (!munger.enabled)
            continue;

        var match = null;
        if (typeof munger.lambdaMatch == "function")
        {
            var rval = munger.lambdaMatch(text, containerTag, data, munger);
            if (typeof rval == "string")
                match = { start: text.indexOf(rval), text: rval };
            else if (typeof rval == "object")
                match = rval;
        }
        else
        {
            var ary = text.match(munger.regex);
            if ((ary != null) && (ary[1]))
                match = { start: text.indexOf(ary[1]), text: ary[1] };
        }

        if (match && (match.start >= 0))
        {
            match.munger = munger;
            matches[entry] = match;
        }
    }

    // Find the first matching entry...
    var firstMatch = { start: text.length, munger: null };
    var firstPriority = 0;
    for (entry in matches)
    {
        // If it matches before the existing first, or at the same spot but
        // with a higher start-priority, this is a better match.
        if (matches[entry].start < firstMatch.start ||
            ((matches[entry].start == firstMatch.start) &&
             (this.entries[priority][entry].startPriority > firstPriority)))
        {
            firstMatch = matches[entry];
            firstPriority = this.entries[priority][entry].startPriority;
        }
    }

    // Replace it.
    if (firstMatch.munger)
    {
        var munger = firstMatch.munger;
        firstMatch.end = firstMatch.start + firstMatch.text.length;

        // Insert the text before the match if there is any
        if (firstMatch.start > 0)
            insertText(text.substr(0, firstMatch.start), containerTag, data);

        if (typeof munger.lambdaReplace == "function")
        {
            // There is no need to munge the "before" text, as we should
            // have found the earliest matching entry by now.
            // The munger rule itself should take care of munging the 'inside'
            // of the match.
            munger.lambdaReplace(firstMatch.text, containerTag, data, munger);
            this.munge(text.substr(firstMatch.end), containerTag, data);

            return containerTag;
        }
        else
        {
            var tag = document.createElementNS(NS_XHTML, munger.tagName);
            tag.setAttribute("class", munger.className + calcClass(data));

            // Don't let this rule match again when we recurse.
            munger.enabled = false;
            this.munge(firstMatch.text, tag, data);
            munger.enabled = true;

            containerTag.appendChild(tag);

            this.munge(text.substr(firstMatch.end), containerTag, data);

            return containerTag;
        }
    }
    return null;
}

//XXXgijs: this depends on our own data-structures, which are in
// content/mungers.js, but we can't move this function out cause insertText
// depends on it, and the rest of the munger depends on that. Sigh.
function calcClass(data)
{
    var className = "";
    if ("hasColorInfo" in data)
    {
        if ("currFgColor" in data)
            className += " chatzilla-fg" + data.currFgColor;
        if ("currBgColor" in data)
            className += " chatzilla-bg" + data.currBgColor;
        if ("isBold" in data)
            className += " chatzilla-bold";
        if ("isUnderline" in data)
            className += " chatzilla-underline";
    }
    return className;
}

function insertText(text, containerTag, data)
{
    var textNode = document.createTextNode(text);

    if ("hasColorInfo" in data)
    {
        var newClass = calcClass(data);
        if (newClass)
        {
            var newTag = document.createElementNS(NS_XHTML, "html:span");
            newTag.setAttribute("class", newClass);
            newTag.appendChild(textNode);
            containerTag.appendChild(newTag);
        }
        else
        {
            delete data.hasColorInfo;
            containerTag.appendChild(textNode);
        }

        var wbr = document.createElementNS(NS_XHTML, "html:wbr");
        containerTag.appendChild(wbr);
    }
    else
    {
        containerTag.appendChild(textNode);
    }
}

