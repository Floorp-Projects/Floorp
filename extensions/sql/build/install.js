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
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is Jan Varga
 * Portions created by the Initial Developer are Copyright (C) 2003
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
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

// this function verifies disk space in kilobytes
function verifyDiskSpace(dirPath, spaceRequired)
{
  var spaceAvailable;

  // Get the available disk space on the given path
  spaceAvailable = fileGetDiskSpaceAvailable(dirPath);

  // Convert the available disk space into kilobytes
  spaceAvailable = parseInt(spaceAvailable / 1024);

  // do the verification
  if(spaceAvailable < spaceRequired)
  {
    logComment("Insufficient disk space: " + dirPath);
    logComment("  required : " + spaceRequired + " K");
    logComment("  available: " + spaceAvailable + " K");
    return(false);
  }

  return(true);
}

var srDest = 200;

var err = initInstall("SQL Support", "SQL", "0.1"); 
logComment("initInstall: " + err);

var fProgram = getFolder("Program");
logComment("fProgram: " + fProgram);

if (verifyDiskSpace(fProgram, srDest))
{
  err = addDirectory("Program", "0.1", "bin", fProgram, "", true);

  logComment("addDirectory() returned: " + err);

  var chromeFolder = getFolder("Chrome", "sql.jar");
  registerChrome(CONTENT | DELAYED_CHROME, chromeFolder, "content/sql/");
  registerChrome(LOCALE | DELAYED_CHROME, chromeFolder, "locale/en-US/sql/");

  err = getLastError();
  if (err == ACCESS_DENIED) {
    alert("Unable to write to program directory " + fProgram + ".\n You will need to restart the browser with administrator/root privileges to install this software. After installing as root (or administrator), you will need to restart the browser one more time to register the installed software.\n After the second restart, you can go back to running the browser without privileges!");
    cancelInstall(err);
    logComment("cancelInstall() due to error: " + err);
  }
  else if (err != SUCCESS) {
    cancelInstall(err);
    logComment("cancelInstall() due to error: " + err);
  }
  else {
    performInstall(); 
    logComment("performInstall() returned: " + err);
  }
}
else
  cancelInstall(INSUFFICIENT_DISK_SPACE);
