/* -*- Mode: Java; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */
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
 * The Original Code is Mozilla Communicator client code.
 *
 * The Initial Developer of the Original Code is 
 * Netscape Communications Corporation.
 * Portions created by the Initial Developer are Copyright (C) 1998
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Alec Flett <alecf@netscape.com>
 *   Seth Spitzer <sspitzer@netscape.com>
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

var gTotalSearchTerms=0;
var gSearchTermList;
var gSearchTerms = new Array;
var gSearchRemovedTerms = new Array;
var gSearchScope;
var gSearchLessButton;
var gSearchBooleanRadiogroup;

// cache these so we don't have to hit the string bundle for them
var gBooleanOrText;
var gBooleanAndText;
var gBooleanInitialText;

var gSearchDateFormat = 0;
var gSearchDateSeparator;

//
function searchTermContainer() {}

searchTermContainer.prototype = {
    internalSearchTerm : '',
    internalBooleanAnd : '',

    // this.searchTerm: the actual nsIMsgSearchTerm object
    get searchTerm() { return this.internalSearchTerm; },
    set searchTerm(val) {
        this.internalSearchTerm = val;

        var term = val;
        // val is a nsIMsgSearchTerm
        var searchAttribute=this.searchattribute;
        var searchOperator=this.searchoperator;
        var searchValue=this.searchvalue;

        // now reflect all attributes of the searchterm into the widgets
        if (searchAttribute) searchAttribute.value = term.attrib;
        if (searchOperator) searchOperator.value = val.op;
        if (searchValue) searchValue.value = term.value;

        this.booleanAnd = val.booleanAnd;
        return val;
    },

    // searchscope - just forward to the searchattribute
    get searchScope() {
        if (this.searchattribute)
          return this.searchattribute.searchScope;
        return undefined;
    },
    set searchScope(val) {
        var searchAttribute = this.searchattribute;
        if (searchAttribute) searchAttribute.searchScope=val;
        return val;
    },

    saveId: function (element, slot) {
        this[slot] = element.id;
    },

    getElement: function (slot) {
        return document.getElementById(this[slot]);
    },

    // three well-defined properties:
    // searchattribute, searchoperator, searchvalue
    // the trick going on here is that we're storing the Element's Id,
    // not the element itself, because the XBL object may change out
    // from underneath us
    get searchattribute() { return this.getElement("internalSearchAttributeId"); },
    set searchattribute(val) {
        this.saveId(val, "internalSearchAttributeId");
        return val;
    },
    get searchoperator() { return this.getElement("internalSearchOperatorId"); },
    set searchoperator(val) {
        this.saveId(val, "internalSearchOperatorId");
        return val;
    },
    get searchvalue() { return this.getElement("internalSearchValueId"); },
    set searchvalue(val) {
        this.saveId(val, "internalSearchValueId");
        return val;
    },

    booleanNodes: null,
    stringBundle: document.getElementById("bundle_search"),
    get booleanAnd() { return this.internalBooleanAnd; },
    set booleanAnd(val) {
        // whenever you set this, all nodes in booleanNodes
        // are updated to reflect the string

        if (this.internalBooleanAnd == val) return val;
        this.internalBooleanAnd = val;

        var booleanNodes = this.booleanNodes;
        if (!booleanNodes) return val;

        var stringBundle = this.stringBundle;
        var andString = val ? "And" : "Or";
        for (var i=0; i<booleanNodes.length; i++) {
            try {
                var staticString =
                    stringBundle.getString("search" + andString + i);
                if (staticString && staticString.length>0)
                    booleanNodes[i].setAttribute("label", staticString);
            } catch (ex) { /* no error, means string not found */}
        }
        return val;
    },

    save: function () {
        var searchTerm = this.searchTerm;

        searchTerm.attrib = this.searchattribute.value;
        var nsMsgSearchAttrib = Components.interfaces.nsMsgSearchAttrib;
        if (this.searchattribute.value > nsMsgSearchAttrib.OtherHeader && this.searchattribute.value < nsMsgSearchAttrib.kNumMsgSearchAttributes) 
          searchTerm.arbitraryHeader = this.searchattribute.label;
        searchTerm.op = this.searchoperator.value;
        if (this.searchvalue.value)
            this.searchvalue.save();
        else
            this.searchvalue.saveTo(searchTerm.value);
        searchTerm.value = this.searchvalue.value;
        searchTerm.booleanAnd = this.booleanAnd;
    },
    // if you have a search term element with no search term
    saveTo: function(searchTerm) {
        this.internalSearchTerm = searchTerm;
        this.save();
    }
}

var nsIMsgSearchTerm = Components.interfaces.nsIMsgSearchTerm;

function initializeSearchWidgets() 
{    
    gSearchBooleanRadiogroup = document.getElementById("booleanAndGroup");
    gSearchTermList = document.getElementById("searchTermList");
    gSearchLessButton = document.getElementById("less");
    if (!gSearchLessButton)
        dump("I couldn't find less button!");

    // initialize some strings
    var bundle = document.getElementById('bundle_search');
    gBooleanOrText = bundle.getString('orSearchText');
    gBooleanAndText = bundle.getString('andSearchText');
    gBooleanInitialText = bundle.getString('initialSearchText');
}

function initializeBooleanWidgets() 
{
    var booleanAnd = true;
    // get the boolean value from the first term
    var firstTerm = gSearchTerms[0].searchTerm;
    if (firstTerm)
        booleanAnd = firstTerm.booleanAnd;

    // target radio items have value="and" or value="or"
    var targetValue = "or";
    if (booleanAnd) targetValue = "and";

    var targetElement = gSearchBooleanRadiogroup.getElementsByAttribute("value", targetValue)[0];

    gSearchBooleanRadiogroup.selectedItem = targetElement;

    for (var i=1; i<gSearchTerms.length; i++) 
    {
      if (booleanAnd)
        document.getElementById('boolOp' + i).setAttribute('value', gBooleanAndText);
      else
        document.getElementById('boolOp' + i).setAttribute('value', gBooleanOrText);
    }
}

function initializeSearchRows(scope, searchTerms)
{
    gTotalSearchTerms = searchTerms.Count();
    for (var i=0; i<gTotalSearchTerms; i++) {
        var searchTerm = searchTerms.QueryElementAt(i, nsIMsgSearchTerm);
        createSearchRow(i, scope, searchTerm);
    }
    initializeBooleanWidgets();
}

function scrollToLastSearchTerm(index)
{
    if (index > 0)
      gSearchTermList.ensureIndexIsVisible(index-1);
}
 
function onMore(event)
{
    if(gTotalSearchTerms==1)
      gSearchLessButton.removeAttribute("disabled", "false");
    createSearchRow(gTotalSearchTerms++, gSearchScope, null);
    // the user just added a term, so scroll to it
    scrollToLastSearchTerm(gTotalSearchTerms);
}

function onLess(event)
{
    if (gTotalSearchTerms>1)
        removeSearchRow(--gTotalSearchTerms);
    if (gTotalSearchTerms==1)
        gSearchLessButton .setAttribute("disabled", "true");

    // the user removed a term, so scroll to the bottom so they are aware of it
    scrollToLastSearchTerm(gTotalSearchTerms);
}

// set scope on all visible searchattribute tags
function setSearchScope(scope) {
    gSearchScope = scope;
    for (var i=0; i<gSearchTerms.length; i++) {
        gSearchTerms[i].obj.searchattribute.searchScope = scope;
        gSearchTerms[i].scope = scope;
    }
}

function updateSearchAttributes()
{
    for (var i=0; i<gSearchTerms.length; i++) {
        gSearchTerms[i].obj.searchattribute.refreshList();
    }
}

function booleanChanged(event) {
    // when boolean changes, we have to update all the attributes on the
    // search terms

    var newBoolValue = (event.target.getAttribute("value") == "and") ? true : false;
    for (var i=0; i<gSearchTerms.length; i++) {
        var searchTerm = gSearchTerms[i].obj;
        searchTerm.booleanAnd = newBoolValue;
        if (i)
        {
          if (newBoolValue)
            document.getElementById('boolOp' + i).setAttribute('value', gBooleanAndText);
          else
            document.getElementById('boolOp' + i).setAttribute('value', gBooleanOrText);
        }
    }
}


function createSearchRow(index, scope, searchTerm)
{
    var searchAttr = document.createElement("searchattribute");
    var searchOp = document.createElement("searchoperator");
    var searchVal = document.createElement("searchvalue");
    var enclosingBox = document.createElement('vbox');
    var boolOp = document.createElement('label');

    enclosingBox.setAttribute('align', 'right');

    // now set up ids:
    searchAttr.id = "searchAttr" + index;
    searchOp.id  = "searchOp" + index;
    searchVal.id = "searchVal" + index;
    boolOp.id = "boolOp" + index;
    if (index == 0)
      boolOp.setAttribute('value', gBooleanInitialText);
    else if ( gSearchBooleanRadiogroup.selectedItem.value == 'and')
      boolOp.setAttribute('value', gBooleanAndText);
    else
      boolOp.setAttribute('value', gBooleanOrText);
    enclosingBox.appendChild(boolOp);

    searchAttr.setAttribute("for", searchOp.id + "," + searchVal.id);

    var rowdata = new Array(enclosingBox, searchAttr,
                            null, searchOp,
                            null, searchVal,
                            null);
    var searchrow = constructRow(rowdata);
    searchrow.id = "searchRow" + index;

    var searchTermObj = new searchTermContainer;

    gSearchTerms[gSearchTerms.length] = {obj:searchTermObj, scope:scope, searchTerm:searchTerm, initialized:false};

    searchTermObj.searchattribute = searchAttr;
    searchTermObj.searchoperator = searchOp;
    searchTermObj.searchvalue = searchVal;

    // now invalidate the newly created items because they've been inserted
    // into the document, and XBL bindings will be inserted in their place
    //searchAttr = searchOp = searchVal = undefined;

    // and/or string handling:
    // this is scary - basically we want to take every other
    // listcell, (note the i+=2) which will be a text label,
    // and set the searchTermObj's
    // booleanNodes to that
    var stringNodes = new Array;
    var listcells = searchrow.childNodes;
    var j=0;
    for (var i=0; i<listcells.length; i+=2) {
        stringNodes[j++] = listcells[i];
    }
    searchTermObj.booleanNodes = stringNodes;

    gSearchTermList.appendChild(searchrow);
}

function initializeTermFromId(id)
{
    // id is of the form searchAttr<n>
    // strlen("searchAttr") == 10
    // turn searchAttr<n> -> <n>
    var index = parseInt(id.slice(10)); 
    initializeTermFromIndex(index)
}

function initializeTermFromIndex(index)
{
    var searchTermObj = gSearchTerms[index].obj;

    searchTermObj.searchScope = gSearchTerms[index].scope;
    // the search term will initialize the searchTerm element, including
    // .booleanAnd
    if (gSearchTerms[index].searchTerm)
        searchTermObj.searchTerm = gSearchTerms[index].searchTerm;
    // here, we don't have a searchTerm, so it's probably a new element -
    // we'll initialize the .booleanAnd from the existing setting in
    // the UI
    else
    {
      searchTermObj.booleanAnd = getBooleanAnd();
      if (index)
      {
        // if we weren't pre-initialized with a searchTerm then steal the search attribute from the 
        // previous row.
        searchTermObj.searchattribute.value =  gSearchTerms[index - 1].obj.searchattribute.value;
      }
    }

    gSearchTerms[index].initialized = true;
}

// creates a <listitem> using the array children as
// the children of each listcell
function constructRow(children)
{
    var listitem = document.createElement("listitem");
    listitem.setAttribute("allowevents", "true");
    for (var i = 0; i < children.length; i++) {
      var listcell = document.createElement("listcell");

      // it's ok to have empty cells
      if (children[i]) {
          children[i].setAttribute("flex", "1");
          listcell.appendChild(children[i]);
      }
      listitem.appendChild(listcell);
    }
    return listitem;
}

function removeSearchRow(index)
{
    var searchTermObj = gSearchTerms[index].obj;
    if (!searchTermObj) {
        return;
    }

    // if it is an existing (but offscreen) term,
    // make sure it is initialized before we remove it.
    if (!gSearchTerms[index].searchTerm && !gSearchTerms[index].initialized)
        initializeTermFromIndex(index);

    // need to remove row from list, so walk upwards from the
    // searchattribute to find the first <listitem>
    var listitem = searchTermObj.searchattribute;
    while (listitem) {
        if (listitem.localName == "listitem") break;
        listitem = listitem.parentNode;
    }

    if (!listitem) {
        dump("Error: couldn't find parent listitem!\n");
        return;
    }


    if (searchTermObj.searchTerm) {
        gSearchRemovedTerms[gSearchRemovedTerms.length] = searchTermObj.searchTerm;
    } else {
        //dump("That wasn't real. ignoring \n");
    }

    listitem.parentNode.removeChild(listitem);
    // remove it from the list of terms - XXX this does it?
    // remove the last element
    gSearchTerms.length--;
}

function getBooleanAnd()
{
    if (gSearchBooleanRadiogroup.selectedItem)
        return (gSearchBooleanRadiogroup.selectedItem.getAttribute("value") == "and") ? true : false;

    // default to false
    return false;
}

// save the search terms from the UI back to the actual search terms
// searchTerms: nsISupportsArray of terms
// termOwner:   object which can contain and create the terms
//              (will be unnecessary if we just make terms creatable
//               via XPCOM)
function saveSearchTerms(searchTerms, termOwner)
{
    var i;
    for (i = 0; i<gSearchTerms.length; i++) {
        try {
            var searchTerm = gSearchTerms[i].obj.searchTerm;

            // the term might be an offscreen one we haven't initialized yet
            // if so, don't bother saving it.
            if (!searchTerm && !gSearchTerms[i].initialized) {
                // is an existing term, but not initialize, so skip saving
                continue;
            }

            if (searchTerm)
                gSearchTerms[i].obj.save();
            else {
                // need to create a new searchTerm, and somehow save it to that
                searchTerm = termOwner.createTerm();
                gSearchTerms[i].obj.saveTo(searchTerm);
                termOwner.appendTerm(searchTerm);
            }
        } catch (ex) {
            dump("** Error saving element " + i + ": " + ex + "\n");
        }
    }

    // now remove the queued elements
    for (i=0; i<gSearchRemovedTerms.length; i++) {
        // this is so nasty, we have to iterate through
        // because GetIndexOf is acting funny
        var searchTermSupports =
            gSearchRemovedTerms[i].QueryInterface(Components.interfaces.nsISupports);
        searchTerms.RemoveElement(searchTermSupports);
    }

}

function onReset(event)
{
    while (gTotalSearchTerms>0)
        removeSearchRow(--gTotalSearchTerms);
    onMore(event);
}

// Get the short date format option of the current locale.
// This supports the common case which the date separator is
// either '/', '-', '.' and using Christian year.
function getLocaleShortDateFormat()
{
  // default to mm/dd/yyyy
  gSearchDateFormat = 3;
  gSearchDateSeparator = "/";

  try {
    var dateFormatService = Components.classes["@mozilla.org/intl/scriptabledateformat;1"]
                                    .getService(Components.interfaces.nsIScriptableDateFormat);
    var dateString = dateFormatService.FormatDate("", 
                                                  dateFormatService.dateFormatShort, 
                                                  1999, 
                                                  12, 
                                                  31);
    // find out the separator
    var arrayOfStrings = dateString.split("/");
    if (arrayOfStrings.length == 3)
      gSearchDateSeparator = "/";
    else
    {
      arrayOfStrings = dateString.split("-");
      if (arrayOfStrings.length == 3)
        gSearchDateSeparator = "-";
      else
      {
        arrayOfStrings = dateString.split(".");
        if (arrayOfStrings.length == 3)
          gSearchDateSeparator = ".";
      }
    }

    // check the format option
    if (arrayOfStrings.length == 3)
    {
      switch (arrayOfStrings[0])
      {
        case "1999":
          if (arrayOfStrings[1] == "12" &&
              arrayOfStrings[2] == "31")
            gSearchDateFormat = 1;
          else if (arrayOfStrings[1] == "31" &&
              arrayOfStrings[2] == "12")
            gSearchDateFormat = 2;
          break;
        case "12":
          if (arrayOfStrings[1] == "31" &&
              arrayOfStrings[2] == "1999")
            gSearchDateFormat = 3;
          else if (arrayOfStrings[1] == "1999" &&
              arrayOfStrings[2] == "31")
            gSearchDateFormat = 4;
          break;
        case "31":
          if (arrayOfStrings[1] == "12" &&
              arrayOfStrings[2] == "1999")
            gSearchDateFormat = 5;
          else if (arrayOfStrings[1] == "1999" &&
              arrayOfStrings[2] == "12")
            gSearchDateFormat = 6;
          break;
      }
    }
  }
  catch (e) {}
}

function initializeSearchDateFormat()
{
  if (gSearchDateFormat)
    return;

  // get a search date format option and a seprator
  try {
    var pref = Components.classes["@mozilla.org/preferences-service;1"]
                                 .getService(Components.interfaces.nsIPrefBranch);
    gSearchDateFormat =
               pref.getComplexValue("mailnews.search_date_format", 
                                    Components.interfaces.nsIPrefLocalizedString);
    gSearchDateFormat = parseInt(gSearchDateFormat);

    // if the option is 0 then try to use the format of the current locale
    if (gSearchDateFormat == 0)
      getLocaleShortDateFormat();
    else
    {
      if (gSearchDateFormat < 1 || gSearchDateFormat > 6)
        gSearchDateFormat = 3;

      gSearchDateSeparator =
                 pref.getComplexValue("mailnews.search_date_separator", 
                                      Components.interfaces.nsIPrefLocalizedString);
    }
  } catch (ex) {
    // set to mm/dd/yyyy in case of error
    gSearchDateFormat = 3;
    gSearchDateSeparator = "/";
  }
}

function convertPRTimeToString(tm)
{
  var time = new Date();
  // PRTime is in microseconds, Javascript time is in seconds
  // so divide by 1000 when converting
  time.setTime(tm / 1000);
  
  return convertDateToString(time);
}

function convertDateToString(time)
{
  var year, month, date;
  initializeSearchDateFormat();

  year = 1900 + time.getYear();
  month = time.getMonth() + 1;  // since js month is 0-11
  date = time.getDate();

  var dateStr;
  var sep = gSearchDateSeparator;

  switch (gSearchDateFormat)
  {
    case 1:
      dateStr = year + sep + month + sep + date;
      break;
    case 2:
      dateStr = year + sep + date + sep + month;
      break;
    case 3:
     dateStr = month + sep + date + sep + year;
      break;
    case 4:
      dateStr = month + sep + year + sep + date;
      break;
    case 5:
      dateStr = date + sep + month + sep + year;
      break;
    case 6:
      dateStr = date + sep + year + sep + month;
      break;
    default:
      dump("valid search date format option is 1-6\n");
  }

  return dateStr;
}

function convertStringToPRTime(str)
{
  initializeSearchDateFormat();

  var arrayOfStrings = str.split(gSearchDateSeparator);
  var year, month, date;

  // set year, month, date based on the format option
  switch (gSearchDateFormat)
  {
    case 1:
      year = arrayOfStrings[0];
      month = arrayOfStrings[1];
      date = arrayOfStrings[2];
      break;
    case 2:
      year = arrayOfStrings[0];
      month = arrayOfStrings[2];
      date = arrayOfStrings[1];
      break;
    case 3:
      year = arrayOfStrings[2];
      month = arrayOfStrings[0];
      date = arrayOfStrings[1];
      break;
    case 4:
      year = arrayOfStrings[1];
      month = arrayOfStrings[0];
      date = arrayOfStrings[2];
      break;
    case 5:
      year = arrayOfStrings[2];
      month = arrayOfStrings[1];
      date = arrayOfStrings[0];
      break;
    case 6:
      year = arrayOfStrings[1];
      month = arrayOfStrings[2];
      date = arrayOfStrings[0];
      break;
    default:
      dump("valid search date format option is 1-6\n");
  }

  month -= 1; // since js month is 0-11

  var time = new Date();
  time.setSeconds(0);
  time.setMinutes(0);
  time.setHours(0);
  time.setDate(date);
  time.setMonth(month);
  time.setYear(year);

  // Javascript time is in seconds, PRTime is in microseconds
  // so multiply by 1000 when converting
  return (time.getTime() * 1000);
}

// this is a helper routine used by our search term xbl widget
var gLabelStrings = new Array;
function GetLabelStrings()
{
  if (!gLabelStrings.length)
  {
    var prefString;
    var pref = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
    for (var index = 0; index < 6; index++)
    {
      prefString = pref.getComplexValue("mailnews.labels.description." + index,  Components.interfaces.nsIPrefLocalizedString);
      gLabelStrings[index] = prefString;
    }
  }
  return gLabelStrings;
}
