// dialog initialization code

function Startup()
{
  if (!InitEditorShell())
    return;
  dump("EditorShell found for Message dialog\n");

  window.opener.msgResult = 0;

  // Message is wrapped in a <div>
  // We will add child text node(s)
  messageParent = (document.getElementById("message"));
  messageText = window.arguments[1];
  if (messageText && messageText.length > 0)
  {
    // Let the caller use "\n" to cause breaks
    // Translate these into <br> tags
    done = false;
    while (!done) {
      breakIndex =   messageText.search(/\n/);
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
  titleText = window.arguments[2];
  if (titleText.length > 0) {
    dump(titleText+" is the message dialog title\n");
    window.title = titleText;
  }
  
  button1 = document.getElementById("button1");
  button2 = document.getElementById("button2");
  button3 = document.getElementById("button3");
  button4 = document.getElementById("button4");
  // All buttons must have the same parent
  buttonParent = button1.parentNode;

  button1Text = window.arguments[3];
  if (button1Text && button1Text.length > 0)
  {
    dump(button1Text+"\n");
    button1.setAttribute("value", button1Text);
  } else {
    // We must have at least one button!
    window.close();
  }

  button2Text = window.arguments[4];
  if (button2Text && button2Text.length > 0)
  {
    dump(button2Text+"\n");
    button2.setAttribute("value", button2Text);
  } else {
    buttonParent.removeChild(button2);
  }

  button3Text = window.arguments[5];
  if (button3Text && button3Text.length > 0)
  {
    dump(button3Text+"\n");
    button3.setAttribute("value", button3Text);
  } else {
    buttonParent.removeChild(button3);
  }

  button4Text = window.arguments[6];
  if (button4Text && button4Text.length > 0)
  {
    dump(button4Text+"\n");
    button4.setAttribute("value", button4Text);
  } else {
    buttonParent.removeChild(button4);
  }
}

function onButton1()
{
  window.opener.msgResult = 1;
  window.close();
}

function onButton2()
{
  window.opener.msgResult = 2;
  window.close();
}

function onButton3()
{
  window.opener.msgResult = 3;
  window.close();
}

function onButton3()
{
  window.opener.msgResult = 4;
  window.close();
}
