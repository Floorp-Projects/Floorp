/* -*- Mode: C++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*-
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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 */
 
// Mostly copied from chrome://pref/content/pref-charset.js

var availCharsetList		= new Array();
var availCharsetDict		= new Array();
var ccm						= null; //Charset Coverter Mgr.
var prefInt				= null; //Preferences Interface
var pref_string   = new String();

function startUp()
{
  try
  {
    prefInt = Components.classes["@mozilla.org/preferences;1"];
    ccm		= Components.classes['@mozilla.org/charset-converter-manager;1'];

    if (ccm) {
      ccm = ccm.getService();
      ccm = ccm.QueryInterface(Components.interfaces.nsICharsetConverterManager2);
      availCharsetList = ccm.GetDecoderList();
      availCharsetList = availCharsetList.QueryInterface(Components.interfaces.nsISupportsArray);

      if (prefInt) {
        prefInt = prefInt.getService();
        prefInt = prefInt.QueryInterface(Components.interfaces.nsIPref);
        pref_string = prefInt.getLocalizedUnicharPref("mailnews.view_default_charset");
      }
    }
  }
  
  catch(ex)
  {
    dump("failed to get prefs or charset mgr. services!\n");
    ccm		= null;
    prefInt = null;
  }

  LoadAvailableCharSets();
}

function createOption(label,value) {
  var opt = new Option(label, value);
  return opt;
}

function LoadAvailableCharSets()
{
  var available_charsets = document.getElementById('mailnews_view_default_charset'); 
  var selIndex = -1;
  var addedItems = 0; // number of items added to the list so far


  if (availCharsetList) for (i = 0; i < availCharsetList.Count(); i++) {

    atom = availCharsetList.GetElementAt(i);
    atom = atom.QueryInterface(Components.interfaces.nsIAtom);

    if (atom) {
      str = atom.GetUnicode();
      try {
      tit = ccm.GetCharsetTitle(atom);
      } catch (ex) {
      tit = str; //don't ignore charset detectors without a title
      }
				
      try {                                  
        visible = ccm.GetCharsetData(atom,'.notForBrowser');
        visible = false;
      } catch (ex) {
        visible = true;
      }
    } //atom

    if (str) if (tit) {

      availCharsetDict[i] = new Array(2);
      availCharsetDict[i][0] = tit;	
      availCharsetDict[i][1] = str;
      availCharsetDict[i][2] = visible;
      if (tit) {}
      else dump('Not label for :' + str + ', ' + tit+'\n');
					
    } //str
  } //for

  availCharsetDict.sort();

  if (availCharsetDict) for (i = 0; i < availCharsetDict.length; i++) {

    try {
      if (availCharsetDict[i][2]) {

        tit = availCharsetDict[i][0];	
        str = availCharsetDict[i][1];	

        var newnode = createOption(tit, str);
        available_charsets.add(newnode, null);
        addedItems++;

        if (pref_string.toUpperCase() == str.toUpperCase()) {
          selIndex = addedItems - 1;
          }

      } //if visible

    } //try

    catch (ex) {
      dump("*** Failed to add Avail. Charset: " + tit + "\n");
      addedItems--;
    } //catch

  } //for

  // set initial selection
  if (selIndex != -1)
    available_charsets.selectedIndex = selIndex;
}

