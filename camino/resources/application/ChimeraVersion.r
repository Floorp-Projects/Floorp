/* -*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* ***** BEGIN LICENSE BLOCK *****
 * Version: NPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Netscape Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/NPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is mozilla.org code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *  Simon Fraser <sfraser@netscape.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or 
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the NPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the NPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */


/*
    This version resource is little more than a placeholder.
    It only exists so that certain plugins (e.g. Shockwave Director)
    which fail if they can't get a 'vers' resource, can initialize
    successfully.
*/


#include <Carbon/Carbon.r>


// Version Numbers //

#define     VERSION_MAJOR           0
#define     VERSION_MINOR           0x60    // revision & fix in BCD
#define     VERSION_KIND            beta    // alpha, beta, or final
#define     VERSION_MICRO           0       // internal stage: alpha or beta number


// Version Strings (Finder's Get Info dialog box) //

#define     VERSION_STRING          "0.8+"

#define     COPYRIGHT_STRING        "© 1998-2003 The Mozilla Foundation"
#define     GETINFO_VERSION         VERSION_STRING ", " COPYRIGHT_STRING
#define     PACKAGE_NAME            "Camino " VERSION_STRING


// Resources definition

resource 'vers' (1, "Program") {
    VERSION_MAJOR,
    VERSION_MINOR,
    VERSION_KIND,
    VERSION_MICRO,
    verUS,
    VERSION_STRING,
    GETINFO_VERSION
};

resource 'vers' (2, "Suite") {
    VERSION_MAJOR,
    VERSION_MINOR,
    VERSION_KIND,
    VERSION_MICRO,
    verUS,
    VERSION_STRING,
    PACKAGE_NAME
};

resource 'STR ' (1000, "Why here?") {
  "This 'vers' resource is required to make the Shockwave Director plugin work."
};


