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
 * The Original Code is Mozilla Communicator client code, released
 * March 31, 1998.
 * 
 * The Initial Developer of the Original Code is Netscape
 * Communications Corporation. Portions created by Netscape are
 * Copyright (C) 1998-1999 Netscape Communications Corporation. All
 * Rights Reserved.
 * 
 * Contributor(s): 
 */

// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditorShell found for Message dialog\n");

  window.opener.msgResult = 0;

  // Message is wrapped in a <div>
  // We will add child text node(s)
  var messageParent = (document.getElementById("message"));
  var messageText = String(window.arguments[1]);

  if (StringExists(messageText)) {
    var messageFragment;

    // Let the caller use "\n" to cause breaks
    // Translate these into <br> tags

    done = false;
    while (!done) {
      breakIndex =   messageText.indexOf('\n');
      if (breakIndex == 0) {
        // Ignore break at the first character
        messageText = messageText.slice(1);
        dump("Found break at begining\n");
        messageFragment = "";
      } else if (breakIndex > 0) {
        // The fragment up to the break
        messageFragment = messageText.slice(0, breakIndex);

        // Chop off fragment we just found from remaining string
        messageText = messageText.slice(breakIndex+1);
      } else {
        // "\n" not found. We're done
        done = true;
        messageFragment = messageText;
      }
      messageNode = document.createTextNode(messageFragment);
      if (messageNode)
        messageParent.appendChild(messageNode);

      // This is needed when the default namespace of the document is XUL
      breakNode = document.createElementWithNameSpace("BR", "http://www.w3.org/TR/REC-html40");
      if (breakNode)
        messageParent.appendChild(breakNode);
    }
  } else {
    // We must have a message
    window.close();
  }

  titleText = String(window.arguments[2]);
  if (StringExists(titleText)) {
    dump(titleText+" is the message dialog title\n");
    window.title = titleText;
  }
  // Set the button text from dialog arguments  
  //  if first button doesn't have text, use "OK"
  InitButton(3,"button1", true);
  InitButton(4,"button2", false);
  InitButton(5,"button3", false);
  InitButton(6,"button4", false);
  
  // Resize content to adjust for added message
  window.sizeToContent();
}

function InitButton(argIndex, buttonID, useOK)
{
  var button = document.getElementById(buttonID);
  var text = String(window.arguments[argIndex]);
  var exists = StringExists(text);
  if (!exists && useOK) {
    text = "OK";
    exists = true;
  }

  if (exists)
  {
    dump(text+"\n");
    button.setAttribute("value", text);
  } else {
    var buttonParent = document.getElementById(buttonID).parentNode;
    buttonParent.removeChild(button);
  }
}

function onButton(buttonNumber)
{
  window.opener.msgResult = buttonNumber;
  window.close();
}
