// XMLTerm Chrome Commands

function StartupXMLTerm() {
   dump("StartupXMLTerm:\n");
   dump("StartupXMLTerm:"+window.frames.length+"\n");
   dump("StartupXMLTerm:"+window.frames[0].name+"\n");

   if (window.frames.length == 2) {
      xmltwin = window.frames[1];
      xmltwin.xmltbrowser = window.frames[0];
   } else {
      xmltwin = window.frames[0];
   }

   dump("StartupXMLterm: WINDOW.ARGUMENTS="+window.arguments+"\n");

   dump("Trying to make an XMLTerm Shell through the component manager...\n");

   var xmltshell = Components.classes["component://mozilla/xmlterm/xmltermshell"].createInstance();

   dump("Interface xmltshell1 = " + xmltshell + "\n");

   xmltshell = xmltshell.QueryInterface(Components.interfaces.mozIXMLTermShell);
   dump("Interface xmltshell2 = " + xmltshell + "\n");

   if (!xmltshell) {
     dump("Failed to create XMLTerm shell\n");
     window.close();
     return;
   }
  
   // Store the XMLTerm shell in current window and in the XMLTerm frame
   window.xmlterm = xmltshell;
   xmltwin.xmlterm = xmltshell;

   // Initialize XMLTerm shell in content window with argvals
   window.xmlterm.Init(xmltwin, "", window.arguments);
}
