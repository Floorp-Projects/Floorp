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
var gFileBox;
var gData;
var gType;
var gServer;
var gServerNeedPass;
var gServerAlso;
var gChannel;
var gChannelIsNick;
var gChannelNeedKey;

function Init()
{
    gType = document.getElementById("czURLType");
    gServer = document.getElementById("czServer");
    gServerNeedPass = document.getElementById("czServerNeedPass");
    gServerAlso = document.getElementById("czServerAlso");
    gChannel = document.getElementById("czChannel");
    gChannelIsNick = document.getElementById("czChannelIsNick");
    gChannelNeedKey = document.getElementById("czChannelNeedKey");
    
    if ("arguments" in window && window.arguments[0])
        gData = window.arguments[0];
    
    if (! gData)
        gData = new Object();
    if (!("url" in gData) || !gData.url)
        gData.url = "irc:///";
    
    if (gData.url == "irc:" || gData.url == "irc:/" || gData.url == "irc://")
    {
        gType.selectedIndex = 1;
    }
    else
    {
        gType.selectedIndex = 0;
        gChannelIsNick.selectedIndex = 0;
        
        // Split it up into server/channel parts...
        var params = gData.url.match(/^irc:\/\/(.*)\/([^,]*)(.*)?$/);
        gServer.value = params[1];
        gChannel.value = params[2];
        gServerAlso.checked = (gChannel.value != '');
        
        if (arrayHasElementAt(params, 3))
        {
            var modifiers = params[3].split(/,/);
            
            for (m in modifiers)
            {
                if (modifiers[m] == 'needpass')
                    gServerNeedPass.checked = true;
                
                if (modifiers[m] == 'isnick')
                    gChannelIsNick.selectedIndex = 1;
                
                if (modifiers[m] == 'needkey')
                    gChannelNeedKey.checked = true;
            }
        }
    }
    
    updateType();
    
    gData.ok = false;
}

function updateType()
{
    if (gType.value == "normal")
    {
        gServer.removeAttribute("disabled");
        gServerNeedPass.removeAttribute("disabled");
        gServerAlso.removeAttribute("disabled");
    }
    else
    {
        gServer.setAttribute("disabled", "true");
        gServerNeedPass.setAttribute("disabled", "true");
        gServerAlso.setAttribute("disabled", "true");
    }
    if ((gType.value == "normal") && gServerAlso.checked)
    {
        gChannel.removeAttribute("disabled");
        gChannelIsNick.childNodes[1].removeAttribute("disabled");
        gChannelIsNick.childNodes[5].removeAttribute("disabled");
        if (gChannelIsNick.value == "channel")
            gChannelNeedKey.removeAttribute("disabled");
        else
            gChannelNeedKey.setAttribute("disabled", "true");
        gServer.focus();
    }
    else
    {
        gChannel.setAttribute("disabled", "true");
        gChannelIsNick.childNodes[1].setAttribute("disabled", "true");
        gChannelIsNick.childNodes[5].setAttribute("disabled", "true");
        gChannelNeedKey.setAttribute("disabled", "true");
    }
}

function onOK()
{
    if (gType.value == "normal")
    {
        gData.url = "irc://" + gServer.value + "/";
        
        if (gServerAlso.checked)
        {
            gData.url += gChannel.value;
            
            if ((gChannelIsNick.value == "channel") && 
                    (gChannelNeedKey.checked))
                gData.url += ",needkey";
            
            if (gChannelIsNick.value == "nickname")
                gData.url += ",isnick";
        }
        if (gServerNeedPass.checked)
            gData.url += ",needpass";
    }
    else
    {
        gData.url = "irc://";
    }
    gData.ok = true;
    
    return true;
}
