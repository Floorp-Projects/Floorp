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
 * The Original Code is ChatZilla.
 *
 * The Initial Developer of the Original Code is
 * ____________________________________________.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   James Ross, <twpol@aol.com>, original author
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the LGPL or the GPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

const nsIFilePicker = Components.interfaces.nsIFilePicker;

function Init()
{
    parent.initPanel("chrome://chatzilla/content/prefpanel/sound.xul");
    
    var edit = document.getElementById("czSound1");
    selectSound(1, edit.getAttribute("prefvalue"));
    edit = document.getElementById("czSound2");
    selectSound(2, edit.getAttribute("prefvalue"));
    edit = document.getElementById("czSound3");
    selectSound(3, edit.getAttribute("prefvalue"));
}

function onChooseFile(index)
{
    try
    {
        var edit = document.getElementById("czSound" + index);
        var oldValue = edit.getAttribute("prefvalue");
        
        var fpClass = Components.classes["@mozilla.org/filepicker;1"];
        var fp = fpClass.createInstance(nsIFilePicker);
        fp.init(window, getMsg("file_browse_Wave"), nsIFilePicker.modeOpen);
        
        fp.appendFilter(getMsg("file_browse_Wave_spec"), "*.wav");
        fp.appendFilters(nsIFilePicker.filterAll);
        
        if ((fp.show() == nsIFilePicker.returnOK) && fp.fileURL.spec
                && fp.fileURL.spec.length > 0)
            selectSound(index, fp.fileURL.spec);
        else
            selectSound(index, oldValue);
    }
    catch(ex)
    {
        dump ("caught exception in `onChooseFile`\n" + ex);
    }
}

function selectSound(index, value)
{
    var edit = document.getElementById("czSound" + index);
    var custom = document.getElementById("czSound" + index + "Custom");
    
    edit.setAttribute("prefvalue", value);
    
    var opts = edit.childNodes[0].childNodes;
    for (var i = 0; i < opts.length; i++)
    {
        if (opts[i].getAttribute("url") == value)
        {
            edit.selectedIndex = i;
            return true;
        }
    }
    
    custom.hidden = false;
    custom.label = value;
    custom.setAttribute("url", value);
    edit.selectedIndex = 4;
    
    return true;
}
