function nsEditableText(aElmOrID)
{
  this.setTextContents = function setTextContents(aStr)
  {
    try {
      this.mAcc.setTextContents(aStr);
      
      is(this.getValue(), aStr,
         "setTextContents: Can't set " + aStr +
         " to element with ID '" + this.mID + "'");
    } catch (e) {
      ok(false,
         "setTextContents: Can't set " + aStr +
         "to element with ID '" + this.mID +
         "', value '" + this.getValue() + "', exception" + e);
    }
  }
  
  this.insertText = function insertText(aStr, aPos, aResStr)
  {
    try {
      this.mAcc.insertText(aStr, aPos);
      
      is(this.getValue(), aResStr,
         "insertText: Can't insert " + aStr + " at " + aPos +
         " to element with ID '" + this.mID + "'");
    } catch (e) {
      ok(false,
         "insertText: Can't insert " + aStr + " at " + aPos +
         " to element with ID '" + this.mID +
         "', value '" + this.getValue() + "', exception " + e);
    }
  }

  this.copyText = function copyText(aStartPos, aEndPos, aClipboardStr)
  {
    var msg = "copyText from " + aStartPos + " to " + aEndPos +
      " for element " + prettyName(this.mID) + ": ";

    try {
      this.mAcc.copyText(aStartPos, aEndPos);
      is(this.pasteFromClipboard(), aClipboardStr, msg);

    } catch(e) {
      ok(false, msg + e);
    }
  }

  this.copyNPasteText = function copyNPasteText(aStartPos, aEndPos,
                                                aPos, aResStr)
  {
    try {
      this.mAcc.copyText(aStartPos, aEndPos);
      this.mAcc.pasteText(aPos);
      
      is(this.getValue(), aResStr,
         "copyText & pasteText: Can't copy text from " + aStartPos +
         " to " + aEndPos + " and paste to " + aPos +
         " for element with ID '" + this.mID + "'");
    } catch (e) {
      ok(false,
         "copyText & pasteText: Can't copy text from " + aStartPos +
         " to " + aEndPos + " and paste to " + aPos +
         " for element with ID '" + this.mID +
         "', value '" + this.getValue() + "', exception " + e);
    }
  }

  this.cutText = function cutText(aStartPos, aEndPos, aClipboardStr, aResStr)
  {
    var msg = "cutText from " + aStartPos + " to " + aEndPos +
      " for element " + prettyName(this.mID) + ": ";

    try {
      this.mAcc.cutText(aStartPos, aEndPos);

      is(this.pasteFromClipboard(), aClipboardStr,
         msg + "wrong clipboard value");
      is(this.getValue(), aResStr, msg + "wrong control value");

    } catch(e) {
      ok(false, msg + e);
    }
  }

  this.cutNPasteText = function copyNPasteText(aStartPos, aEndPos,
                                               aPos, aResStr)
  {
    try {
      this.mAcc.cutText(aStartPos, aEndPos);
      this.mAcc.pasteText(aPos);
      
      is(this.getValue(), aResStr,
         "cutText & pasteText: Can't cut text from " + aStartPos +
         " to " + aEndPos + " and paste to " + aPos +
         " for element with ID '" + this.mID + "'");
    } catch (e) {
      ok(false,
         "cutText & pasteText: Can't cut text from " + aStartPos +
         " to " + aEndPos + " and paste to " + aPos +
         " for element with ID '" + this.mID +
         "', value '" + this.getValue() + "', exception " + e);
    }
  }

  this.pasteText = function pasteText(aPos, aResStr)
  {
    var msg = "pasteText to " + aPos + " position for element " +
      prettyName(this.mID) + ": ";

    try {
      this.mAcc.pasteText(aPos);

      is(this.getValue(), aResStr, msg + "wrong control value");

    } catch(e) {
      ok(false, msg + e);
    }
  }

  this.deleteText = function deleteText(aStartPos, aEndPos, aResStr)
  {
    try {
      this.mAcc.deleteText(aStartPos, aEndPos);

      is(this.getValue(), aResStr,
         "deleteText: Can't delete text from " + aStartPos +
         " to " + aEndPos + " for element with ID '" + this.mID + "'");
    } catch (e) {
      ok(false,
         "deleteText: Can't delete text from " + aStartPos +
         " to " + aEndPos + " for element with ID '" + this.mID +
         "', value " + this.getValue() + ", exception " + e);
    }
  }

  this.getValue = function getValue()
  {
    if (this.mElm instanceof Components.interfaces.nsIDOMNSEditableElement)
      return this.mElm.value;
    if (this.mElm instanceof Components.interfaces.nsIDOMHTMLDocument)
      return this.mElm.body.textContent;
    return this.mElm.textContent;
  }

  this.pasteFromClipboard = function pasteFromClipboard()
  {
    var clip = Components.classes["@mozilla.org/widget/clipboard;1"].
      getService(Components.interfaces.nsIClipboard);
    if (!clip)
      return;

    var trans = Components.classes["@mozilla.org/widget/transferable;1"].
    createInstance(Components.interfaces.nsITransferable);
    if (!trans)
      return;

    trans.addDataFlavor("text/unicode");
    clip.getData(trans, clip.kGlobalClipboard);

    var str = new Object();
    var strLength = new Object();
    trans.getTransferData("text/unicode", str, strLength);

    if (str)
      str = str.value.QueryInterface(Components.interfaces.nsISupportsString);
    if (str)
      return str.data.substring(0, strLength.value / 2);

    return "";
  }

  var elmObj = { value: null };
  this.mAcc = getAccessible(aElmOrID, nsIAccessibleEditableText, elmObj);

  this.mElm = elmObj.value;
  this.mID = aElmOrID;
}
