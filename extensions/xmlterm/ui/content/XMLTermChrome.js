// XMLTerm Chrome Commands

function StartupXMLTerm() {
   //dump("StartupXMLTerm:"+window.frames.length+"\n");
   //dump("StartupXMLTerm:"+window.frames[0].name+"\n");

   if (window.frames.length == 2) {
      xmltwin = window.frames[1];
      xmltwin.xmltbrowser = window.frames[0];
   } else {
      xmltwin = window.frames[0];
   }

   // Determine telnet URL, if any, from query portion of chrome URL
   // For security reasons, only the protocol/host portion of the URL should be
   // used to open a new connection
   var url = "";
   if (document.location.search) {
      url = document.location.search.substr(1);
   }
   //dump("StartupXMLterm: URL="+url+"\n");

   //dump("StartupXMLterm: WINDOW.ARGUMENTS="+window.arguments+"\n");

   dump("Trying to make an XMLTerm Shell through the component manager...\n");

   var xmltshell = Components.classes["@mozilla.org/xmlterm/xmltermshell;1"].createInstance();

   xmltshell = xmltshell.QueryInterface(Components.interfaces.mozIXMLTermShell);
   //dump("Interface xmltshell2 = " + xmltshell + "\n");

   if (!xmltshell) {
     dump("Failed to create XMLTerm shell\n");
     window.close();
     return;
   }
  
   // Store the XMLTerm shell in current window and in the XMLTerm frame
   window.xmlterm = xmltshell;
   xmltwin.xmlterm = xmltshell;

  if (window.arguments != null)
     document.title = "xmlterm: "+window.arguments;

   // Initialize XMLTerm shell in content window with argvals
   window.xmlterm.init(xmltwin, "", window.arguments);
}
