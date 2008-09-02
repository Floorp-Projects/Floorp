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

  var elmObj = { value: null };
  this.mAcc = getAccessible(aElmOrID, nsIAccessibleEditableText, elmObj);

  this.mElm = elmObj.value;
  this.mID = aElmOrID;
}
