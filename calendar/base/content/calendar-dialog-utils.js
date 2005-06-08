/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
/* ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Oracle Corporation code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Stuart Parmenter <stuart.parmenter@oracle.com>
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

/* utility functions */

function setElementValue(elementName, value, name)
{
    var element = document.getElementById(elementName);
    if (!element) {
        dump("unable to find " + elementName + "\n");
        return;
    }

    if (value === false) {
        element.removeAttribute(name ? name : "value");
    } else if (name) {
        //dump("element.setAttribute(" + name + ", " + value + ")\n");
        element.setAttribute(name, value);
    } else {
        //dump("element.value = " + value + "\n");
        element.value = value;
    }
}

function getElementValue(elementName, name)
{
    var element = document.getElementById(elementName);
    if (!element) {
        dump("unable to find " + elementName + "\n");
        return null;
    }

    if (name)
        return element[name];

    return element.value;
}

function enableElement(elementId)
{
    setElementValue(elementId, false, "disabled");
}

function disableElement(elementId)
{
    setElementValue(elementId, "true", "disabled");
}

/* use with textfields oninput to only allow integers */
function validateIntegers(event)
{
    if (isNaN(Number(event.target.value))) {
        var newValue = parseInt(event.target.value);
        event.target.value = isNaN(newValue) ? "" : newValue;
        event.preventDefault();
    }
}
