/*
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
 * Copyright (C) 2001 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s): 
 *   Joe Hewitt <hewitt@netscape.com> (original author)
 */

var gVersion = "0.5";

var err = initInstall("DOM Inspector", "inspector", gVersion);
logComment("initInstall: " + err);

var fProgram = getFolder("Program");
logComment("fProgram: " + fProgram);

err = addDirectory("", gVersion, "bin", fProgram, "", true);
logComment("addDirectory: " + err);

registerChrome(CONTENT | DELAYED_CHROME, getFolder("Chrome","inspector.jar"), "content/inspector/");
registerChrome(LOCALE | DELAYED_CHROME, getFolder("Chrome","inspector.jar"), "locale/en-US/inspector/");
registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","inspector.jar"), "skin/modern/inspector/");
registerChrome(SKIN | DELAYED_CHROME, getFolder("Chrome","inspector.jar"), "skin/classic/inspector/");

if (getLastError() == SUCCESS) {
  err = performInstall(); 
  logComment("performInstall: " + err);
} else {
  cancelInstall(err);
}
