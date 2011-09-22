

const nsIFilePicker = Components.interfaces.nsIFilePicker;
const nsILayoutRegressionTester = Components.interfaces.nsILayoutRegressionTester;

const kTestTypeBaseline         = 1;
const kTestTypeVerify           = 2;
const kTestTypeVerifyAndCompare = 3;
const kTestTypeCompare          = 4;

const kTestSourceSingleFile     = 1;
const kTestSourceDirList        = 2;

var gTestcaseDirArray = new Array;    // array of nsILocalFiles

var gBaselineOutputDir;       // nsIFile
var gVerifyOutputDir;         // nsIFile

var gBaselineFileExtension;   // string
var gVerifyFileExtension;     // string

var gTestType;                // baseline, verify, compare etc.

var gTestWindow;
var gTestURLs = new Array;
var gTestURLsIndex;

function DoOnload()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  // clear any values that the form manager may have unhelpfully filled in
  document.testForm.singleTestFileInput.value = "";
  document.testForm.baselineOutputDir.value = "";
  document.testForm.verifyOutputDir.value = "";
  
  InitFormFromPrefs();
  
  UpdateRunTestsButton();
}

function InitFormFromPrefs()
{
  // load prefs
  try {
    var testURL = GetStringPref("nglayout.debug.testcaseURL");
    document.testForm.singleTestFileInput.value = testURL;
    
    var baselineDirURL = GetStringPref("nglayout.debug.baselineDirURL");
    gBaselineOutputDir = GetFileFromURISpec(baselineDirURL);
    document.testForm.baselineOutputDir.value = gBaselineOutputDir.path;
    
    var verifyDirURL = GetStringPref("nglayout.debug.verifyDirURL");
    gVerifyOutputDir = GetFileFromURISpec(verifyDirURL);
    document.testForm.verifyOutputDir.value = gVerifyOutputDir.path;
    
    var dirIndex = 0;
    while (true)    // we'll throw  when we reach a nonexistent pref
    {
      var curDir = GetStringPref("nglayout.debug.testcaseDir" + dirIndex);
      var dirFileSpec = GetFileFromURISpec(curDir);
      gTestcaseDirArray.push(dirFileSpec);
      dirIndex ++;
    }
  }
  catch(e)
  {
  }
  
  RebuildTestDirsSelect();
}

function SaveFormToPrefs()
{
   SaveStringPref("nglayout.debug.testcaseURL", document.testForm.singleTestFileInput.value);
  
  // save prefs
  if (gBaselineOutputDir)
  {
    var baselineDirURL = GetURISpecFromFile(gBaselineOutputDir);
    SaveStringPref("nglayout.debug.baselineDirURL", baselineDirURL);
  }
  
  if (gVerifyOutputDir)
  {
    var verifyDirURL = GetURISpecFromFile(gVerifyOutputDir);
    SaveStringPref("nglayout.debug.verifyDirURL", verifyDirURL);
  }
  
  var dirIndex;
  for (dirIndex = 0; dirIndex < gTestcaseDirArray.length; dirIndex ++)
  {
    var curURL = GetURISpecFromFile(gTestcaseDirArray[dirIndex]);
    SaveStringPref("nglayout.debug.testcaseDir" + dirIndex, curURL);
  }
  try
  {
    // clear prefs for higher indices until we throw
    while (1)
    {
      ClearPref("nglayout.debug.testcaseDir" + dirIndex);
    }
  }
  catch(e)
  {
  }
  
}

function GetURISpecFromFile(inFile)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  return fileHandler.getURLSpecFromFile(inFile);
}

function GetFileFromURISpec(uriSpec)
{
  var ioService = Components.classes["@mozilla.org/network/io-service;1"].getService(Components.interfaces.nsIIOService);
  var fileHandler = ioService.getProtocolHandler("file").QueryInterface(Components.interfaces.nsIFileProtocolHandler);
  return fileHandler.getFileFromURLSpec(uriSpec);
}

function SaveStringPref(inPrefName, inPrefValue)
{
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  prefs.setCharPref(inPrefName, inPrefValue);
}

function GetStringPref(inPrefName)
{
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  return prefs.getCharPref(inPrefName); 
}

function ClearPref(inPrefName)
{
  var prefs = Components.classes["@mozilla.org/preferences-service;1"].getService(Components.interfaces.nsIPrefBranch);
  prefs.clearUserPref(inPrefName); 
}

function WriteOutput(aText, aReplace, aColorString)
{
  var outputDiv = document.getElementById("results");

  if (aReplace)
    ClearOutput();
  
  var childDiv = document.createElement("div");
  var textNode = document.createTextNode(aText);
  childDiv.appendChild(textNode);
  childDiv.setAttribute("style", "color: " + aColorString + ";");
  outputDiv.appendChild(childDiv);
}

function ClearOutput()
{
  var outputDiv = document.getElementById("results");
  var curChild;
  while (curChild = outputDiv.firstChild)
    outputDiv.removeChild(curChild);
}

// returns an nsIFile
function PickDirectory()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, "Pick a directory", nsIFilePicker.modeGetFolder);
  var result = fp.show();
  if (result == nsIFilePicker.returnCancel)
    throw("User cancelled");
  
  var chosenDir = fp.file;
  return chosenDir;
}


// returns a url string
function PickFileURL()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
  fp.init(window, "Pick a directory", nsIFilePicker.modeOpen);
  fp.appendFilters(nsIFilePicker.filterHTML + nsIFilePicker.filterText);
  
  var result = fp.show();
  if (result == nsIFilePicker.returnCancel)
    throw("User cancelled");
   
  return fp.fileURL.spec;
}

function RebuildTestDirsSelect()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var dirsSelect = document.getElementById("testDirsSelect");
  
  // rebuild it from gTestcaseDirArray
  while (dirsSelect.length)
    dirsSelect.remove(0);
  
  var i;
  for (i = 0; i < gTestcaseDirArray.length; i ++)
  {
    var curDir = gTestcaseDirArray[i];
    
    var optionElement = document.createElement("option");
    var textNode = document.createTextNode(curDir.leafName);
    
    optionElement.appendChild(textNode);
    dirsSelect.add(optionElement, null);
  }
  
  UpdateRunTestsButton();
}

// set the 'single testcase' file
function ChooseTestcaseFile()
{
  var dirInput = document.getElementById("singleTestFileInput");
  dirInput.value = PickFileURL();
  
  UpdateRunTestsButton();
}

function AppendTestcaseDir()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var chosenDir = PickDirectory();
  // does the array already contain this dir?
  var i;
  for (i = 0; i < gTestcaseDirArray.length; i ++)
  {
    var curElement = gTestcaseDirArray[i];
    if (curElement.equals(chosenDir))    // nsIFile::Equals
      return;
  }
  
  gTestcaseDirArray[gTestcaseDirArray.length] = chosenDir;
  RebuildTestDirsSelect();
}

function RemoveTestcaseDir()
{
  var dirsSelect = document.getElementById("testDirsSelect");
  if (dirsSelect.selectedIndex != -1)
  {
    gTestcaseDirArray.splice(dirsSelect.selectedIndex, 1);
    RebuildTestDirsSelect();
  }
}

function InputOptionsValid()
{
  if (document.testForm.testType[0].checked)
  {
    // need a test file
    var testcaseURL = document.testForm.singleTestFileInput.value;
    if (testcaseURL.length == 0) return false;
  }
  else if (document.testForm.testType[1].checked)
  {
    // need at least one dir
    if (gTestcaseDirArray.length == 0) return false;
  }
  else
    return false;

  return true;
}

function OutputOptionsValid()
{
  var testType = GetTestType();
  
  switch (testType)
  {
    case kTestTypeBaseline:
      if (!gBaselineOutputDir) return false;
      break;
      
    case kTestTypeVerify:
     if (!gVerifyOutputDir) return false;
      break;
      
    case kTestTypeVerifyAndCompare:
    case kTestTypeCompare:
     if (!gBaselineOutputDir  || !gVerifyOutputDir) return false;
      break;
  }

  return true;
}

function UpdateRunTestsButton()
{
  var testType = GetTestType();
  var dataValid = OutputOptionsValid();
  if (testType != kTestTypeCompare)
    dataValid &= InputOptionsValid();
  document.testForm.runTests.disabled = !dataValid;
}

// returns nsIFile, sets the input value
function ChooseOutputDirectory(inputElementID)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");
  var chosenDir = PickDirectory();
  
  var inputElement = document.getElementById(inputElementID);
  inputElement.value = chosenDir.path;
  
  return chosenDir;
}


function CompareFrameDumps(testFileBasename, baselineDir, baselineExt, verifyDir, verifyExt)
{
  var debugObject = Components.classes["@mozilla.org/layout-debug/regressiontester;1"].createInstance(nsILayoutRegressionTester);
  
  var baseFile = baselineDir.clone();
  baseFile.append(testFileBasename + baselineExt);
  
  var verifyFile = verifyDir.clone();
  verifyFile.append(testFileBasename + verifyExt);
  
  var filesDiffer = debugObject.compareFrameModels(baseFile, verifyFile, nsILayoutRegressionTester.COMPARE_FLAGS_BRIEF);
  if (filesDiffer)
  {
    WriteOutput("Test file '" + baseFile.leafName + "' failed", false, "red");
  }
  else
  {
    WriteOutput("Test file '" + baseFile.leafName + "' passed", false, "green");
  }
}

function DumpFrames(testWindow, testFileName, outputDir, outputFileExtension)
{
  var debugObject = Components.classes["@mozilla.org/layout-debug/regressiontester;1"].createInstance(nsILayoutRegressionTester);

  var outputFile = outputDir.clone();
  outputFile.append(testFileName.replace(".html", outputFileExtension));

  dump("Dumping frame model for " + testFileName + " to " + outputFile.leafName + "\n");
  var result = debugObject.dumpFrameModel(testWindow, outputFile, nsILayoutRegressionTester.DUMP_FLAGS_MASK_DEFAULT);
  if (result != 0)
  {
    WriteOutput("dumpFrameModel for " + testFileName + " failed", false, "orange");
  }
}

function LoadTestURL(testWindow, theURL)
{
  dump("Loading test " + theURL + "\n");
  // we use a 1/2 second delay to give time for async reflows to happen
  testWindow.onload = setTimeout("HandleTestWindowLoad(gTestWindow)", 1000);
  testWindow.location.href = theURL;
}

function HandleTestWindowLoad(testWindow)
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  var outputDir;
  var outputFileExtension;
  var runCompare = false;
  
  switch (gTestType)
  {
    case kTestTypeBaseline:
      outputDir = gBaselineOutputDir;
      outputFileExtension = gBaselineFileExtension;
      break;
      
    case kTestTypeVerify:
      outputDir = gVerifyOutputDir;
      outputFileExtension = gVerifyFileExtension;
      break;
      
    case kTestTypeVerifyAndCompare:
      outputDir = gVerifyOutputDir;
      outputFileExtension = gVerifyFileExtension;
      runCompare = true;
      break;
      
    case kTestTypeCompare:
      dump("Should never get here");
      break;
  }
  
  var loadedURL  = testWindow.location.href;
  var loadedFile = loadedURL.substring(loadedURL.lastIndexOf('/') + 1);

  DumpFrames(testWindow, loadedFile, outputDir, outputFileExtension);
  
  if (runCompare)
  {
    var testFileBasename = loadedFile.replace(".html", "");
    CompareFrameDumps(testFileBasename, gBaselineOutputDir, gBaselineFileExtension, gVerifyOutputDir, gVerifyFileExtension);
  }

  // now fire of the next one, if we have one
  var nextURL = gTestURLs[gTestURLsIndex++];
  if (nextURL)
    LoadTestURL(testWindow, nextURL);
  else
    testWindow.close();
}


function AddDirectoryEntriesToTestList(inDirFile, inRequiredExtension)
{
  var enumerator = inDirFile.directoryEntries;
  
  while (enumerator.hasMoreElements())
  {
    var curFile = enumerator.getNext();
    curFile = curFile.QueryInterface(Components.interfaces.nsIFile);

    var leafName = curFile.leafName;
    if (leafName.indexOf(inRequiredExtension) != -1)
    {
      var fileURI = GetURISpecFromFile(curFile);
      gTestURLs.push(fileURI);
    }
  }
}


// returns an array of filenames
function DirectoryEntriesToArray(inDirFile, inRequiredExtension)
{
  var fileArray = new Array;

  var enumerator = inDirFile.directoryEntries;
  while (enumerator.hasMoreElements())
  {
    var curFile = enumerator.getNext();
    curFile = curFile.QueryInterface(Components.interfaces.nsIFile);
    var leafName = curFile.leafName;
    if (leafName.indexOf(inRequiredExtension) != -1)
    {
      fileArray.push(leafName);
    }
  }

  return fileArray;
}


function BuildTestURLsList(testSourceType)
{
  // clear the array
  gTestURLs.splice(0, gTestURLs.length);
  gTestURLsIndex = 0;
  
  if (testSourceType == kTestSourceSingleFile)
  {
    var testURL = document.testForm.singleTestFileInput.value;
    if (testURL.substr(-5) != ".html")
    {
      // append /index.html if we have to
      if (testURL.substr(-1) != "/")
        testURL += "/";
      testURL += "index.html";
    }
    gTestURLs[0] = testURL;
  }
  else
  {
    for (var i = 0; i < gTestcaseDirArray.length; i++)
    {
      var dirFile = gTestcaseDirArray[i];    // nsIFile for the dir
      AddDirectoryEntriesToTestList(dirFile, ".html");
    }
  }
}

function CompareFilesInDir(inBaseDir, inBaseExtension, inVerifyDir, inVerifyExtension)
{
  var comapareFiles = DirectoryEntriesToArray(inBaseDir, inBaseExtension);

  for (var i = 0; i < comapareFiles.length; i ++)
  {
    var curFilename = comapareFiles[i];
    var testFileBasename = curFilename.replace(inBaseExtension, "");
    CompareFrameDumps(testFileBasename, inBaseDir, inBaseExtension, inVerifyDir, inVerifyExtension);
  }
}

function GetTestType()
{
  if (document.testForm.doWhat[0].checked)
    return kTestTypeBaseline;

  if (document.testForm.doWhat[1].checked)
    return kTestTypeVerify;
  
  if (document.testForm.doWhat[2].checked)
    return kTestTypeVerifyAndCompare;
  
  if (document.testForm.doWhat[3].checked)
    return kTestTypeCompare;

  return 0;
}

function RunTests()
{
  netscape.security.PrivilegeManager.enablePrivilege("UniversalXPConnect");

  ClearOutput();
  SaveFormToPrefs();
  
  var testSourceType;
  if (document.testForm.testType[0].checked)
    testSourceType = kTestSourceSingleFile;
  else
    testSourceType = kTestSourceDirList;
  
  gTestType = GetTestType();
  
  gBaselineFileExtension = document.testForm.baselineFileExtension.value;
  gVerifyFileExtension   = document.testForm.verifyFileExtension.value;

  if (gTestType == kTestTypeCompare)
  {
    // to compare, we'll just run through all the files in the
    // baseline and verify dirs, and compare those that exist in
    // both.
    CompareFilesInDir(gBaselineOutputDir, gBaselineFileExtension, gVerifyOutputDir, gVerifyFileExtension);
  }
  else
  {
    BuildTestURLsList(testSourceType);
  
    gTestWindow = window.open("about:blank", "Test window",
          "width=800,height=600,status=yes,toolbars=no");
    
    // start the first load
    var testURL = gTestURLs[0];
    gTestURLsIndex = 1;
    LoadTestURL(gTestWindow, testURL);
  }

}


