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
 * The Original Code is Lightning code.
 *
 * The Initial Developer of the Original Code is Oracle Corporation
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Vladimir Vukicevic <vladimir@pobox.com>
 *   Mike Shaver <shaver@mozilla.org>
 *   Joey Minta <jminta@gmail.com>
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

function ltnCreateInstance(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    try {
        return Components.classes[cid].createInstance(Components.interfaces[iface]);
    } catch(e) {
        dump("#### ltnCreateInstance failed for: " + cid + "\n");
        var frame = Components.stack;
        for (var i = 0; frame && (i < 4); i++) {
            dump(i + ": " + frame + "\n");
            frame = frame.caller;
        }
        throw e;
    }
}

function ltnGetService(cid, iface) {
    if (!iface)
        iface = "nsISupports";
    try {
        return Components.classes[cid].getService(Components.interfaces[iface]);
    } catch(e) {
        dump("#### ltnGetService failed for: " + cid + "\n");
        var frame = Components.stack;
        for (var i = 0; frame && (i < 4); i++) {
            dump(i + ": " + frame + "\n");
            frame = frame.caller;
        }
        throw e;
    }
}

var atomSvc;
function ltnGetAtom(str) {
    if (!atomSvc) {
        atomSvc = ltnGetService("@mozilla.org/atom-service;1", "nsIAtomService");
    }

    return atomSvc.getAtom(str);
}

var CalDateTime = new Components.Constructor("@mozilla.org/calendar/datetime;1",
                                             Components.interfaces.calIDateTime);

// Use these functions, since setting |element.collapsed = true| is NOT reliable
function collapseElement(element) {
    element.style.visibility = "collapse";
}

function uncollapseElement(element) {
    element.style.visibility = "";
}

function updateUndoRedoMenu() {
    //XXX give Lightning some undo/redo UI!
}

/**
 * Gets the value of a string in a .properties file
 *
 * @param aBundleName  the name of the properties file.  It is assumed that the
 *                     file lives in chrome://lightning/locale/
 * @param aStringName the name of the string within the properties file
 */
function ltnGetString(aBundleName, aStringName)
{
    var sbs = Components.classes["@mozilla.org/intl/stringbundle;1"]
                        .getService(Components.interfaces.nsIStringBundleService);
    var props = sbs.createBundle("chrome://lightning/locale/"+aBundleName+".properties");
    return props.GetStringFromName(aStringName);
}
