top.MAX_RECIPIENTS = 0;

var inputElementType = "";
var selectElementType = "";
var selectElementIndexTable = null;

var gNumberOfCols = 0;

var gDragService = Components.classes["@mozilla.org/widget/dragservice;1"].getService();
gDragService = gDragService.QueryInterface(Components.interfaces.nsIDragService);
gMimeHeaderParser = null;

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

function awInputElementName()
{
    if (inputElementType == "")
        inputElementType = document.getElementById("addressCol2#1").localName;
    return inputElementType;
}

function awSelectElementName()
{
    if (selectElementType == "")
        selectElementType = document.getElementById("addressCol1#1").localName;
    return selectElementType;
}

function awGetSelectItemIndex(itemData)
{
    if (selectElementIndexTable == null)
    {
      selectElementIndexTable = new Object();
      var selectElem = document.getElementById("addressCol1#1");
        for (var i = 0; i < selectElem.childNodes[0].childNodes.length; i ++)
    {
            var aData = selectElem.childNodes[0].childNodes[i].getAttribute("value");
            selectElementIndexTable[aData] = i;
        }
    }
    return selectElementIndexTable[itemData];
}

function Recipients2CompFields(msgCompFields)
{
  if (msgCompFields)
  {
    gMimeHeaderParser = Components.classes["@mozilla.org/messenger/headerparser;1"].getService(Components.interfaces.nsIMsgHeaderParser);


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
    if(msgTo)
      document.getElementById("toField").value = msgTo;
    if(msgCC)
      document.getElementById("ccField").value = msgCC;
    if(msgBCC)
      document.getElementById("bccField").value = msgBCC;
    gMimeHeaderParser = null; //Release the mime parser
  }
}

function awResetAllRows()
{
  var maxRecipients = top.MAX_RECIPIENTS;
  
  for (var row = 1; row <= maxRecipients ; row ++)
  {
    awGetInputElement(row).value = "";
    awGetPopupElement(row).selectedIndex = 0;
  }
}

function awCleanupRows()
{
  var maxRecipients = top.MAX_RECIPIENTS;
  var rowID = 1;

  for (var row = 1; row <= maxRecipients; row ++)
  {
    var inputElem = awGetInputElement(row);
    if (inputElem.value == "" && row < maxRecipients)
      awRemoveRow(row, 1);
    else
    {
      inputElem.setAttribute("id", "addressCol2#" + rowID);
      awGetPopupElement(row).setAttribute("id", "addressCol1#" + rowID);
      rowID ++;
    }
  }

  awTestRowSequence();
}

function awDeleteRow(rowToDelete)
{
  /* When we delete a row, we must reset the id of others row in order to not break the sequence */
  var maxRecipients = top.MAX_RECIPIENTS;
  awRemoveRow(rowToDelete);

  var numberOfCols = awGetNumberOfCols();
  for (var row = rowToDelete + 1; row <= maxRecipients; row ++)
    for (var col = 1; col <= numberOfCols; col++)
      awGetElementByCol(row, col).setAttribute("id", "addressCol" + (col) + "#" + (row-1));

  awTestRowSequence();
}

function awDeleteHit(inputElement)
{
  var row = awGetRowByInputElement(inputElement);

  /* 1. don't delete the row if it's the last one remaining, just reset it! */
  if (top.MAX_RECIPIENTS <= 1)
  {
    inputElement.value = "";
    return;
  }

  /* 2. Set the focus to the previous field if possible */
  if (row > 1)
    awSetFocus(row - 1, awGetInputElement(row - 1))
  else
    awSetFocus(1, awGetInputElement(2))   /* We have to cheat a little bit because the focus will */
                                          /* be set asynchronusly after we delete the current row, */
                                          /* therefore the row number still the same! */

  /* 3. Delete the row */
  awDeleteRow(row);
}

function awInputChanged(inputElement)
{
  //Do we need to add a new row?
  var lastInput = awGetInputElement(top.MAX_RECIPIENTS);
  if ( lastInput && lastInput.value && !top.doNotCreateANewRow)
    awAppendNewRow(false);
  top.doNotCreateANewRow = false;
}

function awAppendNewRow(setFocus)
{
  var listbox = document.getElementById('addressingWidget');
  var listitem1 = awGetListItem(1);

  if ( listbox && listitem1 )
  {
    var lastRecipientType = awGetPopupElement(top.MAX_RECIPIENTS).selectedItem.getAttribute("value");

    var nextDummy = awGetNextDummyRow();
    var newNode = listitem1.cloneNode(true);
    if (nextDummy)
      listbox.replaceChild(newNode, nextDummy);
    else
      listbox.appendChild(newNode);

    top.MAX_RECIPIENTS++;

    var input = newNode.getElementsByTagName(awInputElementName());
    if ( input && input.length == 1 )
    {
      input[0].setAttribute("value", "");
      input[0].setAttribute("id", "addressCol2#" + top.MAX_RECIPIENTS);
    
      //this copies the autocomplete sessions list from recipient#1 
      input[0].syncSessions(document.getElementById('addressCol2#1'));

  	  // also clone the showCommentColumn setting
  	  //
  	  input[0].showCommentColumn = 
	      document.getElementById("addressCol2#1").showCommentColumn;

      // We always clone the first row.  The problem is that the first row
      // could be focused.  When we clone that row, we end up with a cloned
      // XUL textbox that has a focused attribute set.  Therefore we think
      // we're focused and don't properly refocus.  The best solution to this
      // would be to clone a template row that didn't really have any presentation,
      // rather than using the real visible first row of the listbox.
      //
      // For now we'll just put in a hack that ensures the focused attribute
      // is never copied when the node is cloned.
      if (input[0].getAttribute('focused') != '')
        input[0].removeAttribute('focused');
    }
    var select = newNode.getElementsByTagName(awSelectElementName());
    if ( select && select.length == 1 )
    {
      select[0].selectedItem = select[0].childNodes[0].childNodes[awGetSelectItemIndex(lastRecipientType)];
      select[0].setAttribute("id", "addressCol1#" + top.MAX_RECIPIENTS);
      if (input)
        _awSetAutoComplete(select[0], input[0]);
    }

    // focus on new input widget
    if (setFocus && input[0] )
      awSetFocus(top.MAX_RECIPIENTS, input[0]);
  }
}

// functions for accessing the elements in the addressing widget

function awGetPopupElement(row)
{
    return document.getElementById("addressCol1#" + row);
}

function awGetInputElement(row)
{
    return document.getElementById("addressCol2#" + row);
}

function awGetElementByCol(row, col)
{
  var colID = "addressCol" + col + "#" + row;
  return document.getElementById(colID);
}

function awGetListItem(row)
{
  var listbox = document.getElementById('addressingWidget');

  if ( listbox && row > 0)
  {
    var listitems = listbox.getElementsByTagName('listitem');
    if ( listitems && listitems.length >= row )
      return listitems[row-1];
  }
  return 0;
}

function awGetRowByInputElement(inputElement)
{
  var row = 0;
  if (inputElement) {
    var listitem = inputElement.parentNode.parentNode;
    while (listitem) {
      if (listitem.localName == "listitem")
        ++row;
      listitem = listitem.previousSibling;
    }
  }
  return row;
}


// Copy Node - copy this node and insert ahead of the (before) node.  Append to end if before=0
function awCopyNode(node, parentNode, beforeNode)
{
  var newNode = node.cloneNode(true);

  if ( beforeNode )
    parentNode.insertBefore(newNode, beforeNode);
  else
    parentNode.appendChild(newNode);

    return newNode;
}

// remove row

function awRemoveRow(row)
{
  var listbox = document.getElementById('addressingWidget');

  awRemoveNodeAndChildren(listbox, awGetListItem(row));
  awFitDummyRows();

  top.MAX_RECIPIENTS --;
}

function awRemoveNodeAndChildren(parent, nodeToRemove)
{
  nodeToRemove.parentNode.removeChild(nodeToRemove);
}

//temporary patch for bug 26344 & 26528
function awFinishCopyNode(node)
{
    gMsgCompose.ResetNodeEventHandlers(node);
    return;
}


function awFinishCopyNodes()
{
  var listbox = document.getElementById('addressingWidget');
  awFinishCopyNode(listbox);
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
      dataObj = dataObj.value.QueryInterface(Components.interfaces.nsISupportsWString);
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
  if (selectElem.value != 'addr_newsgroups' && selectElem.value != 'addr_followup')
    inputElem.disableAutocomplete = false;
  else
    inputElem.disableAutocomplete = true;
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
    // handed to us as an nsISupportsWString in the param slot of the 
    // autocomplete error item, by agreement documented in 
    // nsILDAPAutoCompFormatter.idl
    //
    var specificErrString = "";
    try {
	var specificError = errItem.param.QueryInterface(
	    Components.interfaces.nsISupportsWString);
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

function awRecipientKeyPress(event, element)
{
  switch(event.keyCode) {
  case 9:
    awTabFromRecipient(element, event);
    break;
  }
}

function awRecipientKeyDown(event, element)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* do not query directly the value of the text field else the autocomplete widget could potentially
       alter it value while doing some internal cleanup, instead, query the value through the first child
    */
    if (!element.value)
      awDeleteHit(element);
    event.preventBubble();  //We need to stop the event else the listbox will receive it and the function
                            //awKeyDown will be executed!
    break;
  }
}

function awKeyDown(event, listboxElement)
{
  switch(event.keyCode) {
  case 46:
  case 8:
    /* Warning, the listboxElement.selectedItems will change everytime we delete a row */
    var selItems = listboxElement.selectedItems;
    var length = listboxElement.selectedItems.length;
    for (var i = 1; i <= length; i++) {
      var inputs = listboxElement.selectedItems[0].getElementsByTagName(awInputElementName());
      if (inputs && inputs.length == 1)
        awDeleteHit(inputs[0]);
    }
    break;
  }
}

/* ::::::::::: addressing widget dummy rows ::::::::::::::::: */

var gAWContentHeight = 0;
var gAWRowHeight = 0;

function awFitDummyRows()
{
  awCalcContentHeight();
  awCreateOrRemoveDummyRows();
}

function awCreateOrRemoveDummyRows()
{
  var listbox = document.getElementById("addressingWidget");
  var listboxHeight = listbox.boxObject.height;

  // remove rows to remove scrollbar
  var kids = listbox.childNodes;
  for (var i = kids.length-1; gAWContentHeight > listboxHeight && i >= 0; --i) {
    if (kids[i].hasAttribute("_isDummyRow")) {
      gAWContentHeight -= gAWRowHeight;
      listbox.removeChild(kids[i]);
    }
  }

  // add rows to fill space
  if (gAWRowHeight) {
    while (gAWContentHeight+gAWRowHeight < listboxHeight) {
      awCreateDummyItem(listbox);
      gAWContentHeight += gAWRowHeight;
    }
  }
}

function awCalcContentHeight()
{
  var listbox = document.getElementById("addressingWidget");
  var items = listbox.getElementsByTagName("listitem");

  gAWContentHeight = 0;
  if (items.length > 0) {
    // all rows are forced to a uniform height in xul listboxes, so
    // find the first listitem with a boxObject and use it as precedent
    var i = 0;
    do {
      gAWRowHeight = items[i].boxObject.height;
      ++i;
    } while (i < items.length && !gAWRowHeight);
    gAWContentHeight = gAWRowHeight*items.length;
  }
}

function awCreateDummyItem(aParent)
{
  var titem = document.createElement("listitem");
  titem.setAttribute("_isDummyRow", "true");
  titem.setAttribute("class", "dummy-row");

  for (var i = awGetNumberOfCols(); i > 0; i--)
    awCreateDummyCell(titem);

  if (aParent)
    aParent.appendChild(titem);

  return titem;
}

function awCreateDummyCell(aParent)
{
  var cell = document.createElement("listcell");
  cell.setAttribute("class", "addressingWidgetCell dummy-row-cell");
  if (aParent)
    aParent.appendChild(cell);

  return cell;
}

function awGetNextDummyRow()
{
  // gets the next row from the top down
  var listbox = document.getElementById("addressingWidget");
  var kids = listbox.childNodes;
  for (var i = 0; i < kids.length; ++i) {
    if (kids[i].hasAttribute("_isDummyRow"))
      return kids[i];
  }
  return null;
}

function awSizerListen()
{
  // when splitter is clicked, fill in necessary dummy rows each time the mouse is moved
  awCalcContentHeight(); // precalculate
  document.addEventListener("mousemove", awSizerMouseMove, true);
  document.addEventListener("mouseup", awSizerMouseUp, false);
}

function awSizerMouseMove()
{
  awCreateOrRemoveDummyRows(2);
}

function awSizerMouseUp()
{
  document.removeEventListener("mousemove", awSizerMouseUp, false);
  document.removeEventListener("mouseup", awSizerMouseUp, false);
}

