/* 
# ***** BEGIN LICENSE BLOCK *****
# Version: NPL 1.1/GPL 2.0/LGPL 2.1
#
# The contents of this file are subject to the Netscape Public License
# Version 1.1 (the "License"); you may not use this file except in
# compliance with the License. You may obtain a copy of the License at
# http://www.mozilla.org/NPL/
#
# Software distributed under the License is distributed on an "AS IS" basis,
# WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
# for the specific language governing rights and limitations under the
# License.
#
# The Original Code is mozilla.org code.
#
# The Initial Developer of the Original Code is 
# Netscape Communications Corporation.
# Portions created by the Initial Developer are Copyright (C) 1998
# the Initial Developer. All Rights Reserved.
#
# Contributor(s):
#
# Alternatively, the contents of this file may be used under the terms of
# either the GNU General Public License Version 2 or later (the "GPL"), or 
# the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
# in which case the provisions of the GPL or the LGPL are applicable instead
# of those above. If you wish to allow use of your version of this file only
# under the terms of either the GPL or the LGPL, and not to allow others to
# use your version of this file under the terms of the NPL, indicate your
# decision by deleting the provisions above and replace them with the notice
# and other provisions required by the GPL or the LGPL. If you do not delete
# the provisions above, a recipient may use your version of this file under
# the terms of any one of the NPL, the GPL or the LGPL.
#
# ***** END LICENSE BLOCK *****
*/

top.MAX_RECIPIENTS = 0;

var inputElementType = "";
var selectElementType = "";
var selectElementIndexTable = null;

var gNumberOfCols = 0;

var gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
gDragService = gDragService.QueryInterface(Components.interfaces.nsIDragService);
var gMimeHeaderParser = null;

/**
 * global variable inherited from MsgComposeCommands.js
 *
 
 var sPrefs;
 var gPromptService;
 var gMsgCompose;
 
 */

var test_addresses_sequence = false;
try {
  if (sPrefs)
    test_addresses_sequence = sPrefs.getBoolPref("mail.debug.test_addresses_sequence");
}
catch (ex) {}

function awGetMaxRecipients()
{
  return top.MAX_RECIPIENTS;
}

// used to clear out any stored addressing widget data

function awResetAllRows()
{
  var ccField = document.getElementById("ccField");
  var bccField = document.getElementById("bccField");
  var toField = document.getElementById("toField");

  toField.value = "";
  bccField.value = "";
  ccField.value = "";
}

function awGetNumberOfCols()
{
  if (gNumberOfCols == 0)
  {
    var listbox = document.getElementById('addressingWidget');
    var listCols = listbox.getElementsByTagName('listcol');
    gNumberOfCols = listCols.length;
    if (!gNumberOfCols)
      gNumberOfCols = 1;  /* if no cols defined, that means we have only one! */
  }

  return gNumberOfCols;
}

function Recipients2CompFields(msgCompFields)
{
  if (msgCompFields)
  {
    var toValue = document.getElementById("toField").value;
    try {
      msgCompFields.to = gMimeHeaderParser.reformatUnquotedAddresses(toValue);
    } catch (ex) {
      msgCompFields.to = toValue;
    }

    var ccValue = document.getElementById("ccField").value;
    try {
      msgCompFields.cc = gMimeHeaderParser.reformatUnquotedAddresses(ccValue);
    } catch (ex) {
      msgCompFields.cc = ccValue;
    }

    var bccValue = document.getElementById("bccField").value;
    try {
      msgCompFields.bcc = gMimeHeaderParser.reformatUnquotedAddresses(bccValue);
    } catch (ex) {
      msgCompFields.bcc = bccValue;
    }

    // msgCompFields.replyTo = addrReply;
    // msgCompFields.newsgroups = addrNg;
    // msgCompFields.followupTo = addrFollow;
    // msgCompFields.otherRandomHeaders = addrOther;

    gMimeHeaderParser = null;
  }
  else
    dump("Message Compose Error: msgCompFields is null (ExtractRecipients)");
}

function CompFields2Recipients(msgCompFields, msgType)
{
  if (msgCompFields) {
    gMimeHeaderParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);
   
    top.MAX_RECIPIENTS = 0;
    var msgReplyTo = msgCompFields.replyTo;
    var msgTo = msgCompFields.to;
    var msgCC = msgCompFields.cc;
    var msgBCC = msgCompFields.bcc;
    var msgRandomHeaders = msgCompFields.otherRandomHeaders;
    var msgNewsgroups = msgCompFields.newsgroups;
    var msgFollowupTo = msgCompFields.followupTo;

    if(msgReplyTo)
      awSetInputAndPopupFromArray(msgCompFields.SplitRecipients(msgReplyTo, false), 
                                  "addr_reply", newListBoxNode, templateNode);
    if(msgTo)
      document.getElementById("toField").value = msgTo;

    if(msgCC)
      document.getElementById("ccField").value = msgCC;

    if(msgBCC)
      document.getElementById("bccField").value = msgBCC;

    // if(msgRandomHeaders)
    //  awSetInputAndPopup(msgRandomHeaders, "addr_other", newListBoxNode, templateNode);
    // if(msgNewsgroups)
    //  awSetInputAndPopup(msgNewsgroups, "addr_newsgroups", newListBoxNode, templateNode);
    // if(msgFollowupTo)
    //  awSetInputAndPopup(msgFollowupTo, "addr_followup", newListBoxNode, templateNode);

    gMimeHeaderParser = null; //Release the mime parser
  }
}

function awSetFocus(row, inputElement)
{
  top.awRow = row;
  top.awInputElement = inputElement;
  top.awFocusRetry = 0;
  setTimeout("_awSetFocus();", 0);
}


function awGetNumberOfRecipients()
{
    return top.MAX_RECIPIENTS;
}

function DragOverAddressingWidget(event)
{
  var validFlavor = false;
  var dragSession = dragSession = gDragService.getCurrentSession();

  if (dragSession.isDataFlavorSupported("text/x-moz-address")) 
    validFlavor = true;

  if (validFlavor)
    dragSession.canDrop = true;
}

function DropOnAddressingWidget(event)
{
  var dragSession = gDragService.getCurrentSession();
  
  var trans = Components.classes["@mozilla.org/widget/transferable;1"].createInstance(Components.interfaces.nsITransferable);
  trans.addDataFlavor("text/x-moz-address");

  for ( var i = 0; i < dragSession.numDropItems; ++i )
  {
    dragSession.getData ( trans, i );
    var dataObj = new Object();
    var bestFlavor = new Object();
    var len = new Object();
    trans.getAnyTransferData ( bestFlavor, dataObj, len );
    if ( dataObj )  
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsString);
    if ( !dataObj ) 
      continue;

    // pull the address out of the data object
    var address = dataObj.data.substring(0, len.value);
    if (!address)
      continue;

    DropRecipient(event.target, address);
  }
}

function DropRecipient(target, recipient)
{
  // that will automatically set the focus on a new available row, and make sure is visible
  awClickEmptySpace(null, true);
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  lastInput.value = recipient;
  awAppendNewRow(true);
}

function _awSetAutoComplete(selectElem, inputElem)
{
  inputElem.disableAutocomplete = selectElem.value == 'addr_newsgroups' || selectElem.value == 'addr_followup' || selectElem.value == 'addr_other';
}

function awSetAutoComplete(rowNumber)
{
    var inputElem = awGetInputElement(rowNumber);
    var selectElem = awGetPopupElement(rowNumber);
    _awSetAutoComplete(selectElem, inputElem)
}

// Called when an autocomplete session item is selected and the status of
// the session it was selected from is nsIAutoCompleteStatus::failureItems.
//
// As of this writing, the only way that can happen is when an LDAP 
// autocomplete session returns an error to be displayed to the user.
//
// There are hardcoded messages in here, but these are just fallbacks for
// when string bundles have already failed us.
//
function awRecipientErrorCommand(errItem, element)
{
    // remove the angle brackets from the general error message to construct 
    // the title for the alert.  someday we'll pass this info using a real
    // exception object, and then this code can go away.
    //
    var generalErrString;
    if (errItem.value != "") {
	generalErrString = errItem.value.slice(1, errItem.value.length-1);
    } else {
	generalErrString = "Unknown LDAP server problem encountered";
    }	

    // try and get the string of the specific error to contruct the complete
    // err msg, otherwise fall back to something generic.  This message is
    // handed to us as an nsISupportsString in the param slot of the 
    // autocomplete error item, by agreement documented in 
    // nsILDAPAutoCompFormatter.idl
    //
    var specificErrString = "";
    try {
	var specificError = errItem.param.QueryInterface(
	    Components.interfaces.nsISupportsString);
	specificErrString = specificError.data;
    } catch (ex) {
    }
    if (specificErrString == "") {
	specificErrString = "Internal error";
    }

    if (gPromptService) {
	gPromptService.alert(window, generalErrString, specificErrString);
    } else {
	window.alert(generalErrString + ": " + specificErrString);
    }
}
