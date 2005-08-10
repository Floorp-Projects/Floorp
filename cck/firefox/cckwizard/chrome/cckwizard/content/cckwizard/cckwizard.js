/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is the Client Customization Kit (CCK).
 *
 * The Initial Developer of the Original Code is IBM Corp.
 * Portions created by the Initial Developer are Copyright (C) 2005
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

var currentconfigname;
var currentconfigpath;
var haveplugins = false;
var havesearchplugins = false;


var gPrefBranch = Components.classes["@mozilla.org/preferences-service;1"]
                            .getService(Components.interfaces.nsIPrefBranch);
       
function choosefile(labelname)
{
  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, "Choose File...", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText |
                     nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);

   if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
     var label = document.getElementById(labelname);
     label.value = fp.file.path;
   }
  }
  catch(ex) {
  }
}

function choosedir(labelname)
{
  try {
    var keepgoing = true;
    while (keepgoing) {
      var nsIFilePicker = Components.interfaces.nsIFilePicker;
      var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
      fp.init(window, "Choose File...", nsIFilePicker.modeGetFolder);
      fp.appendFilters(nsIFilePicker.filterHTML | nsIFilePicker.filterText |
                       nsIFilePicker.filterAll | nsIFilePicker.filterImages | nsIFilePicker.filterXML);

      if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
        var label = document.getElementById(labelname);
        label.value = fp.file.path;
      }
      keepgoing = false;
    }
  }
  catch(ex) {
  }
}

function chooseimage(labelname, imagename)
{
  try {
    var nsIFilePicker = Components.interfaces.nsIFilePicker;
    var fp = Components.classes["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, "Choose File...", nsIFilePicker.modeOpen);
    fp.appendFilters(nsIFilePicker.filterImages | nsIFilePicker.filterA);

   if (fp.show() == nsIFilePicker.returnOK && fp.fileURL.spec && fp.fileURL.spec.length > 0) {
     var label = document.getElementById(labelname);
     label.value = fp.file.path;
     document.getElementById(imagename).src = fp.fileURL.spec;
   }
  }
  catch(ex) {
  }
}

function initimage(labelname, imagename)
{
 var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  sourcefile.initWithPath(document.getElementById(labelname).value);
  var ioServ = Components.classes["@mozilla.org/network/io-service;1"]
                       .getService(Components.interfaces.nsIIOService);
  var foo = ioServ.newFileURI(sourcefile);
  document.getElementById(imagename).src = foo.spec;
}



function CreateConfig()
{
  window.openDialog("chrome://cckwizard/content/config.xul","createconfig","chrome,modal");
  updateconfiglist();
}

function CopyConfig()
{
  window.openDialog("chrome://cckwizard/content/config.xul","copyconfig","chrome,modal");
  
  updateconfiglist();
}

function DeleteConfig()
{
  var ps = Components.classes["@mozilla.org/embedcomp/prompt-service;1"]
                     .getService(Components.interfaces.nsIPromptService);
  var rv = ps.confirmEx(window, "WizardMachine", "Are you sure you want to delete this configuration?",
                        ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_0 +
                        ps.BUTTON_TITLE_IS_STRING * ps.BUTTON_POS_1,
                        "Yes", "No", null, null, {});
  if (rv == false) {
    gPrefBranch.deleteBranch("cck.config."+currentconfigname);
    updateconfiglist();
  }
}





function ShowMain()
{
   updateconfiglist();
}

function updateconfiglist()
{
  var menulist = document.getElementById('byb-configs')
  menulist.selectedIndex = -1;
  menulist.removeAllItems();
  var configname;
  var selecteditem = false;


  
  var list = gPrefBranch.getChildList("cck.config.", {});
  for (var i = 0; i < list.length; ++i) {
    configname = list[i].replace(/cck.config./g, "");
    var menulistitem = menulist.appendItem(configname,configname);
    menulistitem.minWidth=menulist.width;
    if (configname == currentconfigname) {
      menulist.selectedItem = menulistitem;
      selecteditem = true;
      document.getElementById('example-window').canAdvance = true;
      document.getElementById('deleteconfig').disabled = false;
    }
  }
  if ((!selecteditem) && (list.length > 0)) {
    menulist.selectedIndex = 0;
    setcurrentconfig(list[0].replace(/cck.config./g, ""));
  }
  if (list.length == 0) {
    document.getElementById('example-window').canAdvance = false;
  document.getElementById('deleteconfig').disabled = true;
  }
}

function setcurrentconfig(newconfig)
{
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);

  if (currentconfigpath) {
    destdir.initWithPath(currentconfigpath);
    CCKWriteConfigFile(destdir);
  }
  currentconfigname = newconfig;
  currentconfigpath = gPrefBranch.getCharPref("cck.config." + currentconfigname);
  destdir.initWithPath(currentconfigpath);
  CCKReadConfigFile(destdir);
}

function saveconfig()
{


  if (currentconfigpath) {
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  


    destdir.initWithPath(currentconfigpath);
    CCKWriteConfigFile(destdir);
  }

}

function OnConfigOK()
{
  var configname = document.getElementById('cnc-name').value;
  var configlocation = document.getElementById('cnc-location').value;
  if ((configname) && (configlocation)) {
    gPrefBranch.setCharPref("cck.config." + configname, configlocation);
    this.opener.setcurrentconfig(configname);
    if (window.name = 'copyconfig') {
/* copy prefs from current config */    
    }
  } else {
    return false;
  }
}

function CreateCCK()
{ 
/* ---------- */
  var destdir = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  destdir.initWithPath(currentconfigpath);
  
  CCKWriteConfigFile(destdir);

  destdir.append("jar");
  destdir.append("content");
  destdir.append("cck");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}
  
  CCKWriteXULOverlay(destdir);
  CCKWriteDTD(destdir);
  CCKWriteCSS(destdir);
  CCKWriteProperties(destdir);
  CCKCopyFile(document.getElementById("iconURL").value, destdir);
  CCKCopyFile(document.getElementById("LargeAnimPath").value, destdir);
  CCKCopyFile(document.getElementById("LargeStillPath").value, destdir);
  CCKCopyChromeToFile("cck.js", destdir)
/* copy/create contents.rdf if 1.0 */
  var zipdir = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
  zipdir.initWithPath(currentconfigpath);
  zipdir.append("jar");
  CCKZip("cck.jar", zipdir, "content");
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("chrome");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  zipdir.append("cck.jar");

  CCKCopyFile(zipdir.path, destdir);

/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("components");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  CCKCopyChromeToFile("cckService.js", destdir);
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("defaults");
  destdir.append("preferences");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  CCKWriteDefaultJS(destdir)
  
/* ---------- */

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("plugins");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}


  haveplugins = CCKCopyFile(document.getElementById("BrowserPluginPath1").value, destdir);
  haveplugins |= CCKCopyFile(document.getElementById("BrowserPluginPath2").value, destdir);
  haveplugins |= CCKCopyFile(document.getElementById("BrowserPluginPath3").value, destdir);
  haveplugins |= CCKCopyFile(document.getElementById("BrowserPluginPath4").value, destdir);
  haveplugins |=CCKCopyFile(document.getElementById("BrowserPluginPath5").value, destdir);

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");
  destdir.append("searchplugins");
  try {
    destdir.create(Components.interfaces.nsIFile.DIRECTORY_TYPE, 0775);
  } catch(ex) {}

  havesearchplugins = CCKCopyFile(document.getElementById("SearchPlugin1").value, destdir);
  CCKCopyFile(document.getElementById("SearchPluginIcon1").value, destdir);
  havesearchplugins |= CCKCopyFile(document.getElementById("SearchPlugin2").value, destdir);
  CCKCopyFile(document.getElementById("SearchPluginIcon2").value, destdir);
  havesearchplugins |= CCKCopyFile(document.getElementById("SearchPlugin3").value, destdir);
  CCKCopyFile(document.getElementById("SearchPluginIcon3").value, destdir);
  havesearchplugins |= CCKCopyFile(document.getElementById("SearchPlugin4").value, destdir);
  CCKCopyFile(document.getElementById("SearchPluginIcon4").value, destdir);
  havesearchplugins |= CCKCopyFile(document.getElementById("SearchPlugin5").value, destdir);
  CCKCopyFile(document.getElementById("SearchPluginIcon5").value, destdir);

  destdir.initWithPath(currentconfigpath);
  destdir.append("xpi");

  CCKCopyChromeToFile("chrome.manifest", destdir)
  CCKWriteInstallRDF(destdir);
  CCKWriteInstallJS(destdir);  
  CCKZip("cck.xpi", destdir,
         "chrome", "components", "defaults", "plugins", "searchplugins", "chrome.manifest", "install.rdf");
}

/* This function takes a file in the chromedir and creates a real file */

function CCKCopyChromeToFile(chromefile, location)
{
  var file = location.clone();
  file.append(chromefile);

  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);

  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/" + chromefile + ".in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  fos.write(str, str.length); 
  fos.close();
}

/* This function creates a given zipfile in a given location */
/* It takes as parameters the names of all the files/directories to be contained in the ZIP file */
/* It works by creating a CMD file to generate the ZIP */

function CCKZip(zipfile, location)
{
  platform = navigator.platform;
  var file = location.clone();
             
  if (navigator.platform == "Win32")
    file.append("ccktemp.cmd");
  else
    file.append("ccktemp.sh");  
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);
  var line;
  line = "cd \"" + location.path + "\"\n";
  fos.write(line, line.length);
  if (navigator.platform == "Win32")
    line = "zip -r \"" + location.path + "\\" + zipfile + "\"";
  else
    line = "zip -r \"" + location.path + "/" + zipfile + "\"";  
  for (var i=2; i < arguments.length; i++) {
    line += " " + arguments[i];
  }
  line += "\n";
  fos.write(line, line.length);  
  fos.close();

  var file = location.clone();
  file.append(zipfile);
  try {
    file.remove(false);
  } catch (ex) {}

  var sh;

  // create an nsILocalFile for the executable
  var file = location.clone();
  if (navigator.platform == "Win32")
    file.append("ccktemp.cmd");
  else {
    file.append("ccktemp.sh");
    sh = Components.classes["@mozilla.org/file/local;1"]
                   .createInstance(Components.interfaces.nsILocalFile);
    sh.initWithPath("/bin/sh");
  }
  // create an nsIProcess
  var process = Components.classes["@mozilla.org/process/util;1"]
                          .createInstance(Components.interfaces.nsIProcess);
                          
  if (navigator.platform == "Win32")
    process.init(file);
  else
    process.init(sh);

  var args = [file.path];
  
  process.run(true, args, args.length);
  file.remove(false);
}

function CCKWriteXULOverlay(destdir)
{
  var tooltipXUL  = '  <button id="navigator-throbber" tooltiptext="&throbber.tooltip;"/>\n';

  var titlebarXUL = '  <window id="main-window" titlemodifier="&mainWindow.titlemodifier;"/>\n';

  var helpmenu1   = '  <menupopup id="menu_HelpPopup">\n';
  var helpmenu2   = '    <menuseparator insertafter="aboutSeparator"/>\n';
  var helpmenu3   = '    <menuitem label="&cckHelp.label;" insertafter="aboutSeparator"\n';
  var helpmenu4   = '              accesskey="&cckHelp.accesskey;"\n';
  var helpmenu5   = '              oncommand="openUILink(getCCKLink(\'cckhelp.url\'), event, false, true);"\n';
  var helpmenu6   = '              onclick="checkForMiddleClick(this, event);"/>\n';
  var helpmenu7   = '  </menupopup>\n';

  var file = destdir.clone();
  file.append("cck-browser-overlay.xul");
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck-browser-overlay.xul.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  var tooltip = document.getElementById("AnimatedLogoTooltip").value;
  if (tooltip && (tooltip.length > 0))
    str = str.replace(/%button%/g, tooltipXUL);
  else
    str = str.replace(/%button%/g, "");

  var titlebar = document.getElementById("CompanyName").value;
  if (titlebar && (titlebar.length > 0))
      str = str.replace(/%window%/g, titlebarXUL);
  else
    str = str.replace(/%window%/g, "");

  var helpmenu = document.getElementById("HelpMenuCommandName").value;
  if (helpmenu && (helpmenu.length > 0)) {
    var helpmenuXUL = helpmenu1 + helpmenu2 + helpmenu3;
    var helpmenuakey = document.getElementById("HelpMenuCommandAccesskey").value;
    if (helpmenuakey && (helpmenuakey.length > 0)) {
      helpmenuXUL += helpmenu4;
    }
    helpmenuXUL += helpmenu5 + helpmenu6 + helpmenu7;
    str = str.replace(/%menupopup%/g, helpmenuXUL);
  } else {
    str = str.replace(/%menupopup%/g, "");
  }    

  fos.write(str, str.length); 
  fos.close();
}

function CCKWriteCSS(destdir)
{

var animated1 = '#navigator-throbber[busy="true"] {\n';
var animated2 = 'toolbar[iconsize="small"] #navigator-throbber[busy="true"],\n';
var animated3 = 'toolbar[mode="text"] #navigator-throbber[busy="true"] {\n';
var atrest1 = '#navigator-throbber {\n';
var atrest2 = 'toolbar[iconsize="small"] #navigator-throbber,\n';
var atrest3 = 'toolbar[mode="text"] #navigator-throbber {\n';
var liststyleimage = '  list-style-image: url("chrome://cck/content/';
var liststyleimageend = '");\n}\n';

  var file = destdir.clone();
  file.append("cck.css");
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);

  var animatedlogopath = document.getElementById("LargeAnimPath").value;
  if (animatedlogopath && (animatedlogopath.length > 0)) {
    var file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(document.getElementById("LargeAnimPath").value);

    fos.write(animated1, animated1.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);

    fos.write(animated2, animated2.length);
    fos.write(animated3, animated3.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);
  }
  var atrestlogopath = document.getElementById("LargeStillPath").value;
  if (atrestlogopath && (animatedlogopath.length > 0)) {
    var file = Components.classes["@mozilla.org/file/local;1"]
                         .createInstance(Components.interfaces.nsILocalFile);
    file.initWithPath(document.getElementById("LargeStillPath").value);

    fos.write(atrest1, atrest1.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);

    fos.write(atrest2, atrest2.length);
    fos.write(atrest3, atrest3.length);
    fos.write(liststyleimage, liststyleimage.length);
    fos.write(file.leafName, file.leafName.length);
    fos.write(liststyleimageend, liststyleimageend.length);
  }
  fos.close();
}

function CCKWriteDTD(destdir)
{
  var file = destdir.clone();
  file.append("cck.dtd");
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck.dtd.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%throbber.tooltip%/g, document.getElementById("AnimatedLogoTooltip").value);
  str = str.replace(/%mainWindow.titlemodifier%/g, document.getElementById("CompanyName").value);
  str = str.replace(/%cckHelp.label%/g, document.getElementById("HelpMenuCommandName").value);
  str = str.replace(/%cckHelp.accesskey%/g, document.getElementById("HelpMenuCommandAccesskey").value);
  fos.write(str, str.length); 
  fos.close();
}


function CCKWriteProperties(destdir)
{
  var file = destdir.clone();
  file.append("cck.properties");
  
  try {
    file.remove(false);                         
  } catch (ex) {}
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                      .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
                          .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components.classes["@mozilla.org/scriptableinputstream;1"]
                                 .getService(Components.interfaces.nsIScriptableInputStream);
    
  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/cck.properties.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%browser.throbber.url%/g, document.getElementById("AnimatedLogoURL").value);
  str = str.replace(/%cckhelp.url%/g, document.getElementById("HelpMenuCommandURL").value);
  str = str.replace(/%browser.startup.homepage%/g, document.getElementById("HomePageURL").value);
  str = str.replace(/%PopupAllowedSites%/g, document.getElementById("PopupAllowedSites").value);
  str = str.replace(/%InstallAllowedSites%/g, document.getElementById("InstallAllowedSites").value);
  fos.write(str, str.length); 
  
/* Add toolbar/bookmark stuff at end */

  str = document.getElementById('ToolbarFolder1').value;
  if (str && str.length) {
    str = "ToolbarFolder1=" + str + "\n";
    fos.write(str, str.length);
    for (var i=1; i <= 5; i++) {
      str = document.getElementById("ToolbarFolder1.BookmarkTitle" + i).value;
      if (str && str.length) {
        str = "ToolbarFolder1.BookmarkTitle" + i + "=" + str + "\n";
        fos.write(str, str.length);
      }
      str = document.getElementById("ToolbarFolder1.BookmarkURL" + i).value;
      if (str && str.length) {
        str = "ToolbarFolder1.BookmarkURL" + i + "=" + str + "\n";
        fos.write(str, str.length);
      }
    }
  }
  
  for (var i=1; i <= 5; i++) {
    str = document.getElementById("ToolbarBookmarkTitle" + i).value;
    if (str && str.length) {
      str = "ToolbarBookmarkTitle" + i + "=" + str + "\n";
      fos.write(str, str.length);
    }
    str = document.getElementById("ToolbarBookmarkURL" + i).value;
    if (str && str.length) {
      str = "ToolbarBookmarkURL" + i + "=" + str + "\n";
      fos.write(str, str.length);
    }
  }

  str = document.getElementById('BookmarkFolder1').value;
  if (str && str.length) {
    str = "BookmarkFolder1=" + str + "\n";
    fos.write(str, str.length);
    for (var i=1; i <= 5; i++) {
      str = document.getElementById("BookmarkFolder1.BookmarkTitle" + i).value;
      if (str && str.length) {
        str = "BookmarkFolder1.BookmarkTitle" + i + "=" + str + "\n";
        fos.write(str, str.length);
      }
      str = document.getElementById("BookmarkFolder1.BookmarkURL" + i).value;
      if (str && str.length) {
        str = "BookmarkFolder1.BookmarkURL" + i + "=" + str + "\n";
        fos.write(str, str.length);
      }
    }
  }
  
  for (var i=1; i <= 5; i++) {
    str = document.getElementById("BookmarkTitle" + i).value;
    if (str && str.length) {
      str = "BookmarkTitle" + i + "=" + str + "\n";
      fos.write(str, str.length);
    }
    str = document.getElementById("BookmarkURL" + i).value;
    if (str && str.length) {
      str = "BookmarkURL" + i + "=" + str + "\n";
      fos.write(str, str.length);
    }
  }

  

  fos.close();
}

function CCKWriteDefaultJS(destdir)
{
  var throbber1 = 'pref("browser.throbber.url",            "chrome://cck/content/cck.properties");\n';
  var homepage1 = 'pref("browser.startup.homepage",        "chrome://cck/content/cck.properties");\n';
  var homepage2 = 'pref("browser.startup.homepage_reset",  "chrome://cck/content/cck.properties");\n';
  var useragent1begin = 'pref("general.useragent.vendorComment", "CK-';
  var useragent2begin = 'pref("general.useragent.extra.cck", "(CK-';

  var useragent1end = '");\n';
  var useragent2end = ')");\n';

  var file = destdir.clone();
  file.append("firefox-cck.js");
             
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);

  var logobuttonurl = document.getElementById("AnimatedLogoURL").value;
  if (logobuttonurl && (logobuttonurl.length > 0)) {
    fos.write(throbber1, throbber1.length);
  }

  var browserstartuppage = document.getElementById("HomePageURL").value;
  if (browserstartuppage && (browserstartuppage.length > 0)) {
    fos.write(homepage1, homepage1.length);
    fos.write(homepage2, homepage2.length);    
  }
  
  var useragent = document.getElementById("OrganizationName").value;
  if (useragent && (useragent.length > 0)) {
    fos.write(useragent1begin, useragent1begin.length);
    fos.write(useragent, useragent.length);
    fos.write(useragent1end, useragent1end.length);
    fos.write(useragent2begin, useragent2begin.length);
    fos.write(useragent, useragent.length);
    fos.write(useragent2end, useragent2end.length);
  }
  

  fos.close();
}

function CCKWriteInstallRDF(destdir)
{
  var idline =          "<em:id>%id%</em:id>";
  var nameline =        "<em:name>%name%</em:name>";
  var versionline =     "<em:version>%version%</em:version>";
  var descriptionline = "<em:description>%description%</em:description>";
  var creatorline =     "<em:creator>%creator%</em:creator>";
  var homepageURLline = "<em:homepageURL>%homepageURL%</em:homepageURL>";
  var iconURLline =     "<em:iconURL>chrome://cck/content/%iconURL%</em:iconURL>";


  var file = destdir.clone();
  file.append("install.rdf");
  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);

  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/install.rdf.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();
  
  var id = document.getElementById("id").value;
  if (id && (id.length > 0)) {
    str = str.replace(/%idline%/g, idline);
    str = str.replace(/%id%/g, document.getElementById("id").value);
  }

  var name = document.getElementById("name").value;
  if (name && (name.length > 0)) {
    str = str.replace(/%nameline%/g, nameline);
    str = str.replace(/%name%/g, document.getElementById("name").value);
  } else {
    str = str.replace(/%nameline%/g, "");
  }

  var version = document.getElementById("version").value;
  if (version && (version.length > 0)) {
    str = str.replace(/%versionline%/g, versionline);
    str = str.replace(/%version%/g, document.getElementById("version").value);
  } else {
    str = str.replace(/%versionline%/g, "");
  }

  var description = document.getElementById("description").value;
  if (description && (description.length > 0)) {
    str = str.replace(/%descriptionline%/g, descriptionline);
    str = str.replace(/%description%/g, document.getElementById("description").value);
  } else {
    str = str.replace(/%descrptionline%/g, "");
  }

  var creator = document.getElementById("creator").value;
  if (creator && (creator.length > 0)) {
    str = str.replace(/%creatorline%/g, creatorline);
    str = str.replace(/%creator%/g, document.getElementById("creator").value);
  } else {
    str = str.replace(/%creatorline%/g, "");
  }

  var homepageURL = document.getElementById("homepageURL").value;
  if (homepageURL && (homepageURL.length > 0)) {
    str = str.replace(/%homepageURLline%/g, homepageURLline);
    str = str.replace(/%homepageURL%/g, document.getElementById("homepageURL").value);
  } else {
    str = str.replace(/%homepageURLline%/g, "");
  }

  var iconURL = document.getElementById("iconURL").value;
  if (iconURL && (iconURL.length > 0)) {
    var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                               .createInstance(Components.interfaces.nsILocalFile);
    sourcefile.initWithPath(iconURL);
    str = str.replace(/%iconURLline%/g, iconURLline);
    str = str.replace(/%iconURL%/g, sourcefile.leafName);
  } else {
    str = str.replace(/%iconURLline%/g, "");
  }

  fos.write(str, str.length); 
  fos.close();
}

function CCKWriteInstallJS(destdir)
{
  var file = destdir.clone();
  file.append("install.js");
  try {
    file.remove(false);                         
  } catch (ex) {
  }
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);

  fos.init(file, -1, -1, false);
  var ioService=Components.classes["@mozilla.org/network/io-service;1"]
    .getService(Components.interfaces.nsIIOService);
  var scriptableStream=Components
    .classes["@mozilla.org/scriptableinputstream;1"]
    .getService(Components.interfaces.nsIScriptableInputStream);

  var channel=ioService.newChannel("chrome://cckwizard/content/srcfiles/install.js.in",null,null);
  var input=channel.open();
  scriptableStream.init(input);
  var str=scriptableStream.read(input.available());
  scriptableStream.close();
  input.close();

  str = str.replace(/%id%/g, document.getElementById("id").value);
  str = str.replace(/%name%/g, document.getElementById("name").value);

  if (haveplugins)
    str = str.replace(/%plugins%/g, 'addDirectory("", "%version%", "plugins", cckextensiondir, "plugins", true);');
  else
    str = str.replace(/%plugins%/g, '');

  if (havesearchplugins)
    str = str.replace(/%searchplugins%/g, 'addDirectory("", "%version%", "searchplugins", cckextensiondir, "searchplugins", true);');
  else
    str = str.replace(/%searchplugins%/g, '');
    
  str = str.replace(/%version%/g, document.getElementById("version").value);


  fos.write(str, str.length); 
  fos.close();
}


/* This function copies a source file to a destination directory, including */
/* deleting the file at the destination if it exists */

function CCKCopyFile(source, destination)
{
  if (source.length == 0)
    return false;
  
  var sourcefile = Components.classes["@mozilla.org/file/local;1"]
                       .createInstance(Components.interfaces.nsILocalFile);
  sourcefile.initWithPath(source);

  var destfile = destination.clone();
  destfile.append(sourcefile.leafName);

  try {
    destfile.remove(false);
  } catch (ex) {}

  sourcefile.copyTo(destination, "");
  
  return true;
}


function ShowConfigInfo()
{
  window.openDialog("chrome://cckwizard/content/showconfig.xul","showconfig","chrome,modal");
}

function InitConfigInfo()
{
  var file = Components.classes["@mozilla.org/file/local;1"]
                          .createInstance(Components.interfaces.nsILocalFile);
  file.initWithPath(this.opener.currentconfigpath);

  file.append("cck.config");
  
  if (!file.exists())
    return;

  var stream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Components.interfaces.nsIFileInputStream);

  stream.init(file, 0x01, 0644, 0);
  
  var lis = stream.QueryInterface(Components.interfaces.nsILineInputStream);
  var line = {value:null};
  
  var box = document.getElementById("showconfigy");
  
  do {
    var more = lis.readLine(line);
    var str = line.value;
    box.value += str;
    box.value += "\n";
  } while (more);
  
  stream.close();
}



function CCKWriteConfigFile(destdir)
{
  var file = destdir.clone();
  file.append("cck.config");
             
  var fos = Components.classes["@mozilla.org/network/file-output-stream;1"]
                       .createInstance(Components.interfaces.nsIFileOutputStream);
  fos.init(file, -1, -1, false);

  var elements = document.getElementsByAttribute("id", "*")
  for (var i=0; i < elements.length; i++) {
    if (elements[i].nodeName == "textbox") {
      var line = elements[i].getAttribute("id") + "=" + elements[i].value + "\n";
      fos.write(line, line.length);
    }
  
  }

  fos.close();
}

function CCKReadConfigFile(srcdir)
{
  var file = srcdir.clone();
  file.append("cck.config");
  
  if (!file.exists())
    return;

  var stream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                         .createInstance(Components.interfaces.nsIFileInputStream);

  stream.init(file, 0x01, 0644, 0);
  var lis = stream.QueryInterface(Components.interfaces.nsILineInputStream);
  var line = {value:null};
  
  do {
    var more = lis.readLine(line);
    var str = line.value;
    var linearray = str.split("=");
    if (linearray[0].length)
      document.getElementById(linearray[0]).value = linearray[1];
  } while (more);
  
  stream.close();
}

