/* 
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
 * The Original Code is mozilla.org code.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are 
 * Copyright (C) 2001 Netscape Communications Corporation.  All
 * Rights Reserved.
 * 
 * Contributor(s): 
 * Srilatha Moturi <srilatha@netscape.com>
 */

parent.hPrefWindow.registerOKCallbackFunc( onOK );

function mailnewsOverlayInit() {
    try {
        var mapiRegistry = Components.classes[ "@mozilla.org/mapiregistry;1" ].
                   getService( Components.interfaces.nsIMapiRegistry );
    }
    catch(ex){
        mapiRegistry = null;
    }

    const prefbase = "system.windows.lock_ui.";
    var mailnewsEnableMapi = document.getElementById("mailnewsEnableMapi");
    if (mapiRegistry) {
    // initialise preference component.
    // While the data is coming from the system registry, we use a set
    // of parallel preferences to indicate if the ui should be locked.
        try { 
            var prefService = Components.classes["@mozilla.org/preferences-service;1"]
                          .getService()
                          .QueryInterface(Components.interfaces.nsIPrefService);
            var prefBranch = prefService.getBranch(prefbase);
            if (prefBranch && prefBranch.prefIsLocked("default_mail_client")) {
                if (prefBranch.getBoolPref("default_mail_client"))
                    mapiRegistry.setDefaultMailClient();
                else
                    mapiRegistry.unsetDefaultMailClient();
                mailnewsEnableMapi.setAttribute("disabled", "true");
           }
        }
        catch(ex) {}
        if (mapiRegistry.isDefaultMailClient)
            mailnewsEnableMapi.setAttribute("checked", "true");
        else
            mailnewsEnableMapi.setAttribute("checked", "false");
    }
    else
        mailnewsEnableMapi.setAttribute("disabled", "true");
}

function onOK()
{
    try {
        var mapiRegistry = Components.classes[ "@mozilla.org/mapiregistry;1" ].
                   getService( Components.interfaces.nsIMapiRegistry );
    }
    catch(ex){
        mapiRegistry = null;
    } 
    if (mapiRegistry) { 
        if (document.getElementById("mailnewsEnableMapi").checked)
            mapiRegistry.setDefaultMailClient();
        else
            mapiRegistry.unsetDefaultMailClient();
    }  
}
