
var versionMaj = 1 ;
var versionMin = 5 ;
var versionRel = 0 ;

var CurrentVersion = "1.5" ;

// Example: var versionBld = 60511 ;
// Product: Netscape Visual JavaScript Debugger
//
// This is used only for debugging when execution needs to be
//   slowed down to follow the output to the Java Console.
function delay(amount) {
   if( (debugOutput) && (amount > 0) )
   {    
      var time = new Date() ;
      var seconds = time.getSeconds() ;
      var startCount = 80 ;
      var newSeconds = 70 ;

      startCount = (seconds + amount ) % 60 ;
      time = new Date() ;
      newSeconds = time.getSeconds() ;
      while (newSeconds != startCount ) {
	 time = new Date() ;
	 newSeconds = time.getSeconds() ;
      }
   }
}

// Conditionally displays a message only when debugging.
function dbgIfMsg(condition, message) {
   if(debugOutput && condition)
      java.lang.System.out.println(message) ;
}

// Displays a message only when debugging.
function dbgMsg(message) {
   if(debugOutput)
      java.lang.System.out.println(message) ;
}

// Pops an alert window only when NOT installing in silent mode.
function cAlert(message) {
   if(!this.silent)
      alert(message) ;
}

// Checks OS and version information
function checkSystemEnvironment() {
   var err = 0 ;

   if(debugOutput) {
      registry_vi = netscape.softupdate.Trigger.GetVersionInfo("JavaScript Debugger") ;
      dbgIfMsg( (registry_vi == null ), WarningMsg1) ;
      if (registry_vi != null) {
		dbgIfMsg( (vi.compareTo(registry_vi) <= 0), WarningMsg2) ;
      }
   }

   // This install only works on Windows platforms right now.
   dbgMsg(Msg2 + navigator.platform) ;           
   return err ;
}



// A wrapper for calling the AddSubcomponent() function.
function newSub(fName, jarFilePath, tgtVI, tgtFolder, tgtFilePath) {
   dbgMsg(Msg3 + jarFilePath) ;
   var err = su.AddSubcomponent(fName, tgtVI, jarFilePath, tgtFolder, tgtFilePath, this.force) ;
   dbgIfMsg( (err != 0), ErrorMsg2 + fName + ": " + err) ;
   if(err != 0) {
      newSubFailure = true ;
   }
   delay(FileDelayTime) ;
}

// This prepares the specific files to be installs
function setupFiles(su) {
   var err = 0 ;

   if (su == null) {
      dbgMsg(ErrorMsg3 + err) ;
      return -1 ;
   }

   err = su.StartInstall("JavaScript Debugger",        // Package name
			 vi,
			 netscape.softupdate.SoftwareUpdate.FULL_INSTALL);
   if (err !=0) {
      dbgMsg(ErrorMsg4 + err) ;
      return err ;
   }

   // Get folders
   CommFolder = su.GetFolder("Program") ;
   if (CommFolder == null) {
      dbgMsg(ErrorMsg5 + "Program") ;
      return -1 ;
   }
 dbgMsg("ComFolder is ...." + CommFolder);

 newSub("jsdeb15.jar", "jsdeb15.jar", vi, CommFolder, "JSDebug/jsdeb15.jar") ;
 newSub("readme.txt", "readme.txt", vi, CommFolder, "JSDebug/readme.txt") ;
 newSub("license.html", "license.html", vi, CommFolder, "JSDebug/license.html") ;
 newSub("jsdebugger.html", "jsdebugger.html", vi, CommFolder, "JSDebug/jsdebugger.html") ;
 newSub("jsdebugger4server.html", "jsdebugger4server.html", vi, CommFolder, "JSDebug/jsdebugger4server.html") ;
 newSub("AbortTool.gif", "images/AbortTool.gif", vi, CommFolder, "JSDebug/images/AbortTool.gif") ;
 newSub("BreakpointTool.gif", "images/BreakpointTool.gif", vi, CommFolder, "JSDebug/images/BreakpointTool.gif") ;
 newSub("CopyTool.gif", "images/CopyTool.gif", vi, CommFolder, "JSDebug/images/CopyTool.gif") ;
 newSub("EvalTool.gif", "images/EvalTool.gif", vi, CommFolder, "JSDebug/images/EvalTool.gif") ;
 newSub("HalfSpaceTool.gif", "images/HalfSpaceTool.gif", vi, CommFolder, "JSDebug/images/HalfSpaceTool.gif") ;
 newSub("InterruptTool.gif", "images/InterruptTool.gif", vi, CommFolder, "JSDebug/images/InterruptTool.gif") ;
 newSub("NarrowSpaceTool.gif", "images/NarrowSpaceTool.gif", vi, CommFolder, "JSDebug/images/NarrowSpaceTool.gif") ;
 newSub("Nautical.gif", "images/Nautical.gif", vi, CommFolder, "JSDebug/images/Nautical.gif") ;
 newSub("PageListTool.gif", "images/PageListTool.gif", vi, CommFolder, "JSDebug/images/PageListTool.gif") ;
 newSub("PasteTool.gif", "images/PasteTool.gif", vi, CommFolder, "JSDebug/images/PasteTool.gif") ;
 newSub("RefreshTool.gif", "images/RefreshTool.gif", vi, CommFolder, "JSDebug/images/RefreshTool.gif") ;
 newSub("RunTool.gif", "images/RunTool.gif", vi, CommFolder, "JSDebug/images/RunTool.gif") ;
 newSub("StepIntoTool.gif", "images/StepIntoTool.gif", vi, CommFolder, "JSDebug/images/StepIntoTool.gif") ;
 newSub("StepOutTool.gif", "images/StepOutTool.gif", vi, CommFolder, "JSDebug/images/StepOutTool.gif") ;
 newSub("StepOverTool.gif", "images/StepOverTool.gif", vi, CommFolder, "JSDebug/images/StepOverTool.gif") ;
 newSub("ToolbarCollapser.gif", "images/ToolbarCollapser.gif", vi, CommFolder, "JSDebug/images/ToolbarCollapser.gif") ;
 newSub("droplet.au", "sounds/droplet.au", vi, CommFolder, "JSDebug/sounds/droplet.au") ;

 newSub("jsd.gif", "images/jsd.gif", vi, CommFolder, "JSDebug/images/jsd.gif") ; 
 newSub("item1.gif", "samples/images/item1.gif", vi, CommFolder, "JSDebug/samples/images/item1.gif") ;
 newSub("item2.gif", "samples/images/item2.gif", vi, CommFolder, "JSDebug/samples/images/item2.gif") ;
 newSub("item3.gif", "samples/images/item3.gif", vi, CommFolder, "JSDebug/samples/images/item3.gif") ;
 newSub("item4.gif", "samples/images/item4.gif", vi, CommFolder, "JSDebug/samples/images/item4.gif") ;
 newSub("layers.html", "samples/layers.html", vi, CommFolder, "JSDebug/samples/layers.html") ;

 newSub("index.html", "samples/index.html", vi, CommFolder, "JSDebug/samples/index.html") ;
 newSub("props.html", "samples/props.html", vi, CommFolder, "JSDebug/samples/props.html") ;
 newSub("recursion.html", "samples/recursion.html", vi, CommFolder, "JSDebug/samples/recursion.html") ;
 newSub("runerr.html", "samples/runerr.html", vi, CommFolder, "JSDebug/samples/runerr.html") ;
 newSub("misc.html", "samples/misc.html", vi, CommFolder, "JSDebug/samples/misc.html") ;
 newSub("badlines.html", "samples/badlines.html", vi, CommFolder, "JSDebug/samples/badlines.html") ;
 newSub("include.js", "samples/include.js", vi, CommFolder, "JSDebug/samples/include.js") ;



   // If any subcomponent failed, the install aborts here.
   if (newSubFailure) {
      dbgMsg(ErrorMsg6) ;
      abortMe() ;
    }

   return 0 ;
}



// Launchs HTML file with JavaScript Debugger startup instructions.
function showStartupInfo() {
   if(!this.silent) {

      var tmpStr = "" + CommFolder ;

      // Strip the colons out of CommFolder path for Mac.
      if(navigator.platform == "MacPPC") {
         i = tmpStr.indexOf(":") ;
         while (i != -1) {
     	    tmpStr = tmpStr.substr(0,i) + "/" + tmpStr.substr(i+1) ;
            i = tmpStr.indexOf(":") ;
         }
      }

      // jsd_start_url is declared globally to be used by 
      // window_open_err_handler() in case the window.open below fails

      var end_name = "JSDebug/jsdebugger.html";

      if (navigator.platform == "Win32") {
        jsd_start_url = "file:///" + tmpStr + end_name;
      }
      else if (navigator.platform == "MacPPC")  {
        jsd_start_url = "file:///" + escape(tmpStr) + end_name;
      }
      else {
        jsd_start_url = "file://" + escape(tmpStr) + end_name;
      }
        
      var old_handler = window.onerror;
      window.onerror = window_open_err_handler;

      window.open(jsd_start_url, "_jsd_start");

      window.onerror = old_handler;
   }
}

function window_open_err_handler()
{
    var jsd_start = window.open("", "_jsd_start");
    var d = jsd_start.document;
    d.writeln("<html>");
    d.writeln("<head>");
    d.writeln("    <title>Launch Netscape JavaScript Debugger</title>");
    d.writeln("<\/head>");
    d.writeln("<body>");
    d.writeln("<h2>");
    d.writeln("<center>");
    d.writeln("<a href=\""+jsd_start_url+"\">"+"Start Netscape JavaScript Debugger</a>");
    d.writeln("<\/center>");
    d.writeln("<\/h2>");
    d.writeln("<\/body>");
    d.writeln("<\/html>");
    d.close();
    return true;
}    


function getMajorVersion(str)
{
    var b = str.indexOf("b");
    if( -1 == b )
        return str;
    return str.substring(0,b);
}

function getMinorVersion(str)
{
    var b = str.indexOf("b");
    if( -1 == b )
        return "";
    return str.substring(b+1,str.length);
}

function compareStrings( s1, s2 )
{
    if( s1 == s2 )
        return 0;
    return s1 < s2 ? -1 : 1;
}

/*
*
* compareVersions( v1, v2 )
*
* returns:  0 if versions are equal
*          -1 if v1 < v2
*           1 if v1 > v2
*/
                    
function compareVersions( v1, v2 )
{
    var compareVal;

    compareVal = compareStrings( getMajorVersion(v1), getMajorVersion(v2) );
    if( 0 == compareVal )
    {
        var mv1 = getMinorVersion(v1);
        var mv2 = getMinorVersion(v2);

        if( mv1 == "" || mv2 == "" )
        {
            if( mv1 == "" )
            {
                if( mv2 == "" )
                    compareVal = 0;
                else
                    compareVal = 1;
            }
            else
                compareVal = -1;
        }
        else
            compareVal = compareStrings( getMinorVersion(v1), getMinorVersion(v2) );
    }
    return compareVal;
}



// Update Windows Registry
function updateWindowsRegistry() {

	var err = 0 ;

	// remove SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\value\\main\\Install Directory
	subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + value + "\\main" ;
	valname = "Install Directory" ;
	err = winreg.deleteValue(subkey, valname) ;
	
	// remove SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\value\\main
	subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + value + "\\main" ;
	valname = "\\main" ;
	err = winreg.deleteKey(subkey) ;
	
	subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + value ;
	valname = value ;
	err = winreg.deleteKey(subkey) ;
	
	// remove CurrentVersion and create new version
	subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger" ;
	valname = "Current Version" ;
	err = winreg.deleteValue(subkey, valname) ;
	dbgMsg("Remove --- value of Windows registry: " + err) ;
	value = CurrentVersion ;
	err   = winreg.setValueString(subkey, valname, value) ; // Set the new value
	
	// Create New Install Directory
	subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + CurrentVersion + "\\main" ;
	valname = "Install Directory" ;
	value   = CommFolder + "JSDebug" ;
	err     = winreg.setValueString(subkey, valname, value) ; // Set the new value

	return err ;

}


// Check Windows Registry before installing it
function checkWindowsRegistry() {
  var err = 0 ;

  // Only check for Win32
  if (navigator.platform == "Win32") {
	winreg = su.GetWinRegistry() ;
	dbgMsg(" winreg = " + winreg ) ;

	if (winreg != null ) {
		winreg.setRootKey(winreg.HKEY_LOCAL_MACHINE) ;
		subkey = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + CurrentVersion + "\\main" ;
		err = winreg.createKey(subkey, "") ; // Create the above key if it doesn't exist
		// Get value of current version
		subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger" ;
		valname = "Current Version" ;
		value   = winreg.getValueString(subkey, valname) ;
		if (value != null) {
			// Windows Registry already exists
			dbgMsg("value of Current Version from the Windows Registry: " + value);

            err = compareVersions( CurrentVersion, "" + value) ;
			if (err >= 0) {
				//delete the old version, create new version and return to main
				err = updateWindowsRegistry() ;
				return err ;
			}
			else {
				// if Current Version is older than value, ask the user to abort or continue
				msg = "You already have a newer version of Netscape JavaScript Debugger on this machine.  Do you want to continue installation of the older version?";				
				if (confirm(msg)) {
					// Install JSDebug
					err = updateWindowsRegistry() ;	
					return err ;
				}
				else {	
					err = -1 ;
					abortMe(err);
				} // confirm

			} // compareVersions

		}
		else {
		
			// then create the values
			subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger" ;
			valname = "Current Version" ;
			value   = CurrentVersion ;
			err = winreg.setValueString(subkey, valname, value) ; // Set the new value
			
			subkey  = "SOFTWARE\\Netscape\\Netscape JavaScript Debugger\\" + CurrentVersion + "\\main" ;
			valname = "Install Directory" ;
			value   = CommFolder + "JSDebug" ;
			err = winreg.setValueString(subkey, valname, value) ; // Set the new value
			
		} //value

	} //winreg

  } //Win32

  return err ;


}


// Handles catastrophic errors.
function abortMe(err) {
   if(!abortCalled) {
      dbgMsg(Msg4 + err) ;
      cAlert(Msg4 + err) ;
      su.AbortInstall() ;
      abortCalled = true ;
   }
}

//                                                                 
// The following line is generated by the build scripts.           
// ========================= START =========================
// ----------------------------------
// Strings "resourced" for translation.
// ----------------------------------
var updateObjectName = "Netscape JavaScript Debugger v1.5" ;
var Msg1         = "Install Successfully Completed" ;
var Msg2         = "The value of navigator.platform is: " ;
var Msg3         = "Filename in newSub is: " ;
var Msg4         = "Install Aborted. " ;
var Msg5         = "Starting script..." ;
var ErrorMsg1    = "Error: Wrong operating system: " ;
var ErrorMsg2    = "Error Adding SubComponent: " ;
var ErrorMsg3    = "Error passing su to setupFiles: " ;
var ErrorMsg4    = "Error at startInstall: " ;
var ErrorMsg5    = "Error getting folder: " ;
var ErrorMsg6    = "Error adding at least one subcomponent." ;
var ErrorMsg7    = "Error at FinalizeInstall: " ;
var ErrorMsg8    = "Error at setupFiles: " ;
var ErrorMsg9    = "Error at checkSystemEnvironment: " ;
var ErrorMsg10   = "Error: This install requires Communicator Beta6 or later." ;
var SuccessMsg1  = "new SoftwareUpdate succeeded." ;
var SuccessMsg2  = "checkSystemEnvirnoment succeeded." ;
var SuccessMsg3  = "setupFiles succeeded." ;
var WarningMsg1  = "Warning: No registry info for JavaScript Debugger node" ;
var WarningMsg2  = "Warning: Version is no newer than previously installed version." ;
var WarningMsg3  = "Warning: Unable to create the VersionInfo object." ;
var WarningMsg4  = "Warning: Unable to create the SoftwareUpdate object." ;
var WarningMsg5  = "Warning: Communicator does not have a version number";
var WarningMsg6  = "Warning: Installer could not detect version of Communicator.  Be sure you are running Communicator Beta 6 or later." ;


// ----------------------------
// GLOBAL VARIABLE DECLARATIONS
// ----------------------------

var FileDelayTime = 0 ; // Number of seconds delay between subcomponents when debugging

var abortCalled   = false ;
var newSubFailure = false ;
var debugOutput   = false ;  // Turns debugging output on/off (true/false)

var vi = new netscape.softupdate.VersionInfo(versionMaj, versionMin, versionRel, versionBld) ;
    dbgIfMsg( (vi == null), WarningMsg3) ;
var su = new netscape.softupdate.SoftwareUpdate( this, updateObjectName ) ;
    dbgIfMsg( (su == null), WarningMsg4) ;

// this needs to be at global scope
var jsd_start_url = "";

//-------------------------------------------------
// Here is where the main body of execution begins.
//-------------------------------------------------
java.lang.System.out.println(Msg5) ;

var err = 0 ;
if ( (su != null) ) {
   dbgMsg(SuccessMsg1) ;
   err = checkSystemEnvironment() ;
   if ( err == 0 ) {
      dbgMsg(SuccessMsg2) ;    
      err = setupFiles(su) ;
      if (err == 0) {
		dbgMsg(SuccessMsg3) ;
	    err = checkWindowsRegistry() ;
	    if (err == 0) {
		  dbgMsg("Success checking Windows Registry") ;
		  err = su.FinalizeInstall();   // This actually copies all the files.
		  if (err == 0) {               // Install succeeded
			dbgMsg(Msg1) ;
			// cAlert(Msg1) ;
			showStartupInfo() ;
		  }
		  else {                        // Install failed
			dbgMsg(ErrorMsg7 + err) ;
			abortMe(err) ;
		  } //FinalizeInstall
		} //checkWindowsRegistry
		else {
			dbgMsg("checkWindowsRegistry is failed or CurrentVersion is older than Window Registry: " + err) ;
			abortMe(err) ;
		} //checkWindowsRegistry		
      }
      else {
		dbgMsg(ErrorMsg8 + err) ;
		abortMe(err) ;
	  } //setupFiles
   } 
   else {
      dbgMsg(ErrorMsg9 + err) ;
      abortMe(err) ;
   } //checkSystemEnvironment
}
