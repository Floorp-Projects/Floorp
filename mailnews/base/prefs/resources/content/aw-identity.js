/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 2 -*-
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
 * Alec Flett <alecf@netscape.com>
 * Seth Spitzer <sspitzer@netscape.com>
 */

var Bundle = srGetStrBundle("chrome://messenger/locale/prefs.properties");

function validate(data)
{
  var email = document.getElementById("identity.email").value;
  var name = document.getElementById("identity.fullName").value;

  if (! name || name=="") {
    var alertText = Bundle.GetStringFromName("enterName");
    window.alert(alertText);
    return false;
  }

  var emailArray = email.split('@');
  if (emailArray.length != 2 ||
      emailArray[0] == "" ||
      emailArray[1] == "") {
    var alertText = Bundle.GetStringFromName("enterValidEmail");
    window.alert(alertText);
    return false;
  }
  dump("emailArray[0] = '" + emailArray[0] + "'\n");
  dump("emailArray[1] = '" + emailArray[1] + "'\n");
  return true;
}
