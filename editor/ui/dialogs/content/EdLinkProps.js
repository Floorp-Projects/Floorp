    var appCore;
    var toolkitCore;

    var editElement = null;
    var insertNew = true;

    // NOTE: Use "HREF" instead of "A" to distinguish from Named Anchor
    // The returned nodes will have the "A" tagName
    var tagName = "HREF";

    // dialog initialization code
    function Startup() {
      dump("Doing Startup...\n");
      toolkitCore = GetToolkitCore();
      if(!toolkitCore) {
        dump("toolkitCore not found!!! And we can't close the dialog!\n");
      }
      // NEVER create an appcore here - we must find parent editor's
      appCore = XPAppCoresManager.Find("EditorAppCoreHTML");  
      if(!appCore || !toolkitCore) {
        dump("EditorAppCore not found!!!\n");
        toolkitCore.CloseWindow();
      }
      dump("EditorAppCore found for Link Properties dialog\n");
      
      // Get a single selected element and edit its properites
      editElement = appCore.getSelectedElement(tagName);

      if (editElement) {
        // We found an element and don't need to insert it
        insertNew = false;
      } else {
        // We don't have an element selected, 
        //  so create one with default attributes
        dump("Element not selected - calling createElementWithDefaults\n");
        editElement = appCore.createElementWithDefaults(tagName);
      }

      if(!editElement)
      {
        dump("Failed to get selected element or create a new one!\n");
        toolkitCore.CloseWindow(window);
      }
      dump("GetSelectedElement...\n");

      hrefInput = document.getElementById("textHREF");
      // Set the input element value to current HREF
      if (hrefInput)
      {
        dump("Setting HREF editbox value\n");
        hrefInput.value = editNode.href;
      }
	  }

    function applyChanges() {
      // Set the input element value to current HREF
      hrefInput = document.getElementById("textHREF");
      if (hrefInput)
      {
        dump("Copying edit field HREF value to node attribute\n");
        editElement.setAttribute("href",hrefInput.value);
      }
      // Get text to use for a new link
      if (insertNew)
      {
        textLink = document.getElementById("textLink");
        if (textLink)
        {
          // Append the link text as the last child node 
          //   of the docFrag
          textNode = appCore.editorDocument.createTextNode(textLink.value);
          if (textNode)
          {
            editElement.appendChild(textNode);
            dump("Text for new link appended to HREF node\n");
            newElement = appCore.insertElement(editElement, true);
            if (newElement != editElement)
            {
              dump("Returned element from insertElement is different from orginal element.\n");
            }
          }
        }
        // Once inserted, we can modify properties, but don't insert again
        insertNew = false;
      }
    }

    function onOK() {
      applyChanges();
      toolkitCore.CloseWindow(window);
    }

    function onCancel() {
      dump("Calling CloseWindow...\n");
      toolkitCore.CloseWindow(window);
    }

    function GetToolkitCore() {
      var toolkitCore = XPAppCoresManager.Find("ToolkitCore");
      if (!toolkitCore) {
        toolkitCore = new ToolkitCore();
        if (toolkitCore)
          toolkitCore.Init("ToolkitCore");
      }
      return toolkitCore;
    }
