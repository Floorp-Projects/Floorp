/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil -*-
 *
 * The contents of this file are subject to the Netscape Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/NPL/
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
 * Copyright (C) 1998 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
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

    // make sure all data objects/element exists and valid; otherwise, reload.  SUCKS!
    if (((document.forms[0].publishURL == "undefined") || (document.forms[0].publishURL == "[object InputArray]")) ||
        ((document.forms[0].publishPassword == "undefined") || (document.forms[0].publishPassword == "[object InputArray]")) ||
        ((document.forms[0].viewURL == "undefined") || (document.forms[0].viewURL == "[object InputArray]")))
    {
        parent.controls.reloadDocument();
        return;
    }

    document.forms[0].publishURL.value = parent.parent.globals.document.vars.publishURL.value;
    document.forms[0].publishPassword.value = parent.parent.globals.document.vars.publishPassword.value;
    document.forms[0].viewURL.value = parent.parent.globals.document.vars.viewURL.value;
    parent.parent.globals.setFocus(document.forms[0].publishURL);
    if (parent.controls.generateControls)   parent.controls.generateControls();
}



function saveData()
{
    // make sure all form element are valid objects, otherwise just skip & return!
    if (((document.forms[0].publishURL == "undefined") || (document.forms[0].publishURL == "[object InputArray]")) ||
        ((document.forms[0].publishPassword == "undefined") || (document.forms[0].publishPassword == "[object InputArray]")) ||
        ((document.forms[0].viewURL == "undefined") || (document.forms[0].viewURL == "[object InputArray]")))
    {
        parent.controls.reloadDocument();
        return(true);
    }

    parent.parent.globals.document.vars.publishURL.value = document.forms[0].publishURL.value;
    parent.parent.globals.document.vars.publishPassword.value = document.forms[0].publishPassword.value;
    parent.parent.globals.document.vars.viewURL.value = document.forms[0].viewURL.value;
    return(true);
}



// end hiding contents from old browsers  -->
