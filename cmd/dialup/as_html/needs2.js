/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.0 (the "NPL"); you may not use this file except in
 * compliance with the NPL.  You may obtain a copy of the NPL at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the NPL is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the NPL
 * for the specific language governing rights and limitations under the
 * NPL.
 *
 * The Initial Developer of this code under the NPL is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1998 Netscape Communications Corporation.  All Rights
 * Reserved.
 */
<!--  to hide script contents from old browsers



function go(msg)
{
    if (parent.parent.globals.document.vars.editMode.value == "yes")
        return true;
    else
        return(checkData());
}



function checkData()
{
    return(true);
}



function loadData()
{
    if (parent.controls.generateControls)   parent.controls.generateControls();
}



function saveData()
{
}



function displaySupportNumber()
{
    netscape.security.PrivilegeManager.enablePrivilege("AccountSetup");

    if (parent.parent.globals.document.vars.providername.value != "" && parent.parent.globals.document.vars.providerFilename.value != "")   {
        var pathName = parent.parent.globals.getConfigFolder(self);
        providerFilename = parent.parent.globals.document.vars.providerFilename.value;
        if (providerFilename != "") {
            var ispName=parent.parent.globals.document.setupPlugin.GetNameValuePair(providerFilename,"Dial-In Configuration","SiteName");
            var supportNumber = parent.parent.globals.GetNameValuePair(providerFilename,"Dial-In Configuration","SupportPhone");
            if (supportNumber != null && supportNumber != "")   {
                document.writeln("<P ID='buttontext'><B>" + ispName + " Technical Support: " + supportNumber + "</B></P>");
                }
            }
        }
}



// end hiding contents from old browsers  -->

