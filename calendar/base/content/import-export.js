/* -*- Mode: javascript; tab-width: 20; indent-tabs-mode: nil; c-basic-offset: 4 -*-
 * ***** BEGIN LICENSE BLOCK *****
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
 * The Original Code is Mozilla Calendar code.
 *
 * The Initial Developer of the Original Code is
 * ArentJan Banck <ajbanck@planet.nl>.
 * Portions created by the Initial Developer are Copyright (C) 2002
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s): ArentJan Banck <ajbanck@planet.nl>
 *                 Steve Hampton <mvgrad78@yahoo.com>
 *                 Eric Belhaire <belhaire@ief.u-psud.fr>
 *                 Jussi Kukkonen <jussi.kukkonen@welho.com>
 *                 Michiel van Leeuwen <mvl@exedo.nl>
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
 
// File constants copied from file-utils.js
const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_RDWR     = 0x04;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;
const MODE_SYNC     = 0x40;
const MODE_EXCL     = 0x80;


/**
 * loadEventsFromFile
 * shows a file dialog, reads the selected file(s) and tries to parse events from it.
 */

function loadEventsFromFile()
{
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
  
    var fp = Components.classes["@mozilla.org/filepicker;1"]
                       .createInstance(nsIFilePicker);
    fp.init(window, getStringBundle().GetStringFromName("Open"),
            nsIFilePicker.modeOpen);
    fp.defaultExtension = "ics";

    // Get a list of exporters
    var contractids = new Array();
    var catman = Components.classes["@mozilla.org/categorymanager;1"]
                           .getService(Components.interfaces.nsICategoryManager);
    var catenum = catman.enumerateCategory('cal-importers');
    while (catenum.hasMoreElements()) {
        var entry = catenum.getNext();
        entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
        var contractid = catman.getCategoryEntry('cal-importers', entry);
        var exporter = Components.classes[contractid]
                                 .getService(Components.interfaces.calIImporter);
        var types = exporter.getFileTypes({});
        var type;
        for each (type in types) {
            fp.appendFilter(type.description, "*."+type.extension);
            contractids.push(contractid);
        }
    }

    fp.show();

    var filePath = fp.file.path;

    if (filePath) {
        var importer = Components.classes[contractids[fp.filterIndex]]
                                 .getService(Components.interfaces.calIImporter);

        const nsIFileInputStream = Components.interfaces.nsIFileInputStream;
        const nsIScriptableInputStream = Components.interfaces.nsIScriptableInputStream;

        var inputStream = Components.classes["@mozilla.org/network/file-input-stream;1"]
                                    .createInstance(nsIFileInputStream);
        var items;
        try
        {
           inputStream.init( fp.file, MODE_RDONLY, 0444, {} );

           items = importer.importFromStream(inputStream, {});
           inputStream.close();
        }
        catch(ex)
        {
           alert(getStringBundle().GetStringFromName("unableToRead") + filePath + "\n"+ex );
        }

        // XXX Ask for a calendar to import into
        var destCal = getDefaultCalendar();

        // XXX This might not work in lighting
        startBatchTransaction();
        
        for each (item in items) {
            // XXX prompt when finding a duplicate.
            destCal.addItem(item, null);
        }
        
        // XXX This might not work in lighting
        endBatchTransaction();
    }
    return true;
}

/**
 * saveEventsToFile
 *
 * Save data to a file. Create the file or overwrite an existing file.
 * Input an array of calendar events, or no parameter for selected events.
 */

function saveEventsToFile(calendarEventArray)
{
   if (!calendarEventArray)
       return;

   if (!calendarEventArray.length)
   {
      alert(getStringBundle().GetStringFromName("noEventsToSave"));
      return;
   }

   // Show the 'Save As' dialog and ask for a filename to save to
   const nsIFilePicker = Components.interfaces.nsIFilePicker;

   var fp = Components.classes["@mozilla.org/filepicker;1"]
                      .createInstance(nsIFilePicker);

   fp.init(window,  getStringBundle().GetStringFromName("SaveAs"),
           nsIFilePicker.modeSave);

   if(calendarEventArray.length == 1 && calendarEventArray[0].title)
      fp.defaultString = calendarEventArray[0].title;
   else
      fp.defaultString = getStringBundle().GetStringFromName("defaultFileName");

   fp.defaultExtension = "ics";

   // Get a list of exporters
   var contractids = new Array();
   var catman = Components.classes["@mozilla.org/categorymanager;1"]
                          .getService(Components.interfaces.nsICategoryManager);
   var catenum = catman.enumerateCategory('cal-exporters');
   while (catenum.hasMoreElements()) {
       var entry = catenum.getNext();
       entry = entry.QueryInterface(Components.interfaces.nsISupportsCString);
       var contractid = catman.getCategoryEntry('cal-exporters', entry);
       var exporter = Components.classes[contractid]
                                .getService(Components.interfaces.calIExporter);
       var types = exporter.getFileTypes({});
       var type;
       for each (type in types) {
           fp.appendFilter(type.description, "*."+type.extension);
           contractids.push(contractid);
       }
   }


   fp.show();

   // Now find out as what to save, convert the events and save to file.
   if (fp.file && fp.file.path.length > 0) 
   {
      const UTF8 = "UTF-8";
      var aDataStream;
      var extension;
      var charset;

      var exporter = Components.classes[contractids[fp.filterIndex]]
                               .getService(Components.interfaces.calIExporter);

      var filePath = fp.file.path;
      if(filePath.indexOf(".") == -1 )
          filePath += "."+exporter.getFileTypes({})[0].extension;

      const LOCALFILE_CTRID = "@mozilla.org/file/local;1";
      const FILEOUT_CTRID = "@mozilla.org/network/file-output-stream;1";
      const nsILocalFile = Components.interfaces.nsILocalFile;
      const nsIFileOutputStream = Components.interfaces.nsIFileOutputStream;

      var outputStream;

      var localFileInstance = Components.classes[LOCALFILE_CTRID]
                                        .createInstance(nsILocalFile);
      localFileInstance.initWithPath(filePath);

      outputStream = Components.classes[FILEOUT_CTRID]
                               .createInstance(nsIFileOutputStream);
      try
      {
         outputStream.init(localFileInstance, MODE_WRONLY | MODE_CREATE | MODE_TRUNCATE, 0664, 0);
         // XXX Do the right thing with unicode and stuff. Or, again, should the
         //     exporter handle that?
         exporter.exportToStream(outputStream, calendarEventArray.length, calendarEventArray);
         outputStream.close();
      }
      catch(ex)
      {
         alert(getStringBundle().GetStringFromName("unableToWrite") + filePath );
      }
   }
}

function getStringBundle()
{
    var strBundleService = 
        Components.classes["@mozilla.org/intl/stringbundle;1"]
                  .getService(Components.interfaces.nsIStringBundleService);
    return strBundleService.createBundle("chrome://calendar/locale/calendar.properties");
}
