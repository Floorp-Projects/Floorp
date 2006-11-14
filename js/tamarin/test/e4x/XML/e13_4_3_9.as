/* -*- Mode: java; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Rhino code, released
 * May 6, 1999.
 *
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation.  Portions created by Netscape are
 * Copyright (C) 1997-2000 Netscape Communications Corporation. All
 * Rights Reserved.
 *
 * Contributor(s):
 * Igor Bukanov
 * Ethan Hugg
 * Milen Nankov
 *
 * Alternatively, the contents of this file may be used under the
 * terms of the GNU Public License (the "GPL"), in which case the
 * provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only
 * under the terms of the GPL and not to allow others to use your
 * version of this file under the NPL, indicate your decision by
 * deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL.  If you do not delete
 * the provisions above, a recipient may use your version of this
 * file under either the NPL or the GPL.
 */

START("13.4.3.9 - XML.defaultSettings()");

AddTestCase( "settings = XML.defaultSettings(), settings.ignoreComments", true, 
             (settings = XML.defaultSettings(), settings.ignoreComments) );
AddTestCase( "settings = XML.defaultSettings(), settings.ignoreProcessingInstructions", true, 
             (settings = XML.defaultSettings(), settings.ignoreProcessingInstructions) );
AddTestCase( "settings = XML.defaultSettings(), settings.ignoreWhitespace", true, 
             (settings = XML.defaultSettings(), settings.ignoreWhitespace) );
AddTestCase( "settings = XML.defaultSettings(), settings.prettyPrinting", true, 
             (settings = XML.defaultSettings(), settings.prettyPrinting) );
AddTestCase( "settings = XML.defaultSettings(), settings.prettyIndent", 2, 
             (settings = XML.defaultSettings(), settings.prettyIndent) );

var tempSettings = XML.settings();
tempSettings.ignoreComments = false;
tempSettings.ignoreProcessingInstructions = false;
tempSettings.ignoreWhitespace = false;
tempSettings.prettyPrinting = false;
tempSettings.prettyIndent = 4;

AddTestCase( "XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreComments", true, 
             (XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreComments) );
AddTestCase( "XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreProcessingInstructions", true, 
             (XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreProcessingInstructions) );
AddTestCase( "XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreWhitespace", true, 
             (XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.ignoreWhitespace) );
AddTestCase( "XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.prettyPrinting", true, 
             (XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.prettyPrinting) );
AddTestCase( "XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.prettyIndent", 2, 
             (XML.setSettings(tempSettings), XML.setSettings(XML.defaultSettings()), XML.prettyIndent) );

END();
