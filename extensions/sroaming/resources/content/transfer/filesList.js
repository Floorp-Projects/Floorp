/*-*- Mode: C++; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*-*/
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
 * The Original Code is Mozilla Roaming code.
 *
 * The Initial Developer of the Original Code is 
 *   Ben Bucksch <http://www.bucksch.org> of
 *   Beonex <http://www.beonex.com>
 * Portions created by the Initial Developer are Copyright (C) 2002-2004
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   ManyOne <http://www.manyone.net>
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


/* A few common data types used here:
   FilesStats  Array of FileStats  Like transfer.files
   FileStats  Object with properties
                   filename  String  path to the file, incl. possible dirs,
                                     relative to the local dir.
                   mimetype  String  e.g. "application/octet-stream"
                   lastModified  int, in milliseconds since start of 1970
                                 (Unix time * 1000)
                   size  int, in bytes
   FilesList  Array of Objects with property
                   filename  String  See above
                   (i.e. a FilesStats with possibly fewer properties)
   FilesIndexed  Objects with filenames as properties, containing an
                   Object (e.g. a FileStats) with that filename,
                   i.e. filesIndexed["myfile"] gives a file object with
                   file.filename = "myfile".
*/

const kLocalFilesDefaultMimetype = "application/octet-stream";


/*
  Takes 3 lists of files. For each entry in filesList, it compares the
  corresponding entries in files1 and files2 and returns,
  which entries match and which don't.
  If files1 or files2 is empty, this is considered a mismatch.

  @param filesList  FilesList
  @param files1  FilesStats
  @param files2  FilesStats
  @param newer  bool  false or null: require last modified to match
                      true: consider files in files2 to match, even if they
                      have been modified after those in files1 (but not
                      vice versa).
                      Hack.
  @result  Object with propreties |matches| and |mismatches|, both
                    are FilesStats-Arrays holding subsets of filesList.
                    Any additional properties of the file entries in filesList
                    will be preserved.
*/
function compareFiles(filesList, files1, files2, newer)
{
  //ddump("comparing file lists");
  //ddumpObject(filesList, "  filesList", 1);
  //ddumpObject(files1, "  files1", 1);
  //ddumpObject(files2, "  files2", 1);

  var result = new Object();
  result.matches = new Array();
  result.mismatches = new Array();

  // sanitize input
  if (!filesList)
    filesList = new Array();
  if (!files1)
    files1 = new Array();
  if (!files2)
    files2 = new Array();

  // create indexes, very important for speed with large lists
  var files1Indexed = createFilesIndexed(files1);
  var files2Indexed = createFilesIndexed(files2);

  /* go through filesList and compare each entry with the corresponding ones
     in files1 and files2 */
  for (var i = 0, l = filesList.length; i < l; i++)
  {
    // find corresponding entries
    var f1 = files1Indexed[filesList[i].filename];
    var f2 = files2Indexed[filesList[i].filename];

    // compare
    var matches = (f1 && f2
                   && f1.size == f2.size
                   && ( // last modified time
                        // allow variations of 1 sec because of FAT inaccuracy
                        newer == null //XXX use |!newer|
                          && f2.lastModified - 1000 <= f1.lastModified  // basically ==, with leeway
                          && f1.lastModified <= f2.lastModified + 1000 //   "
                        || newer
                          && f1.lastModified <= f2.lastModified + 1000
                      ));
    if (!f1 && !f2)
      matches = true;

    //ddump(filesList[i].filename + (matches ? " matches" : " doesn't match"));
    if (matches)
      result.matches.push(filesList[i]);
    else
      result.mismatches.push(filesList[i]);
  }
  return result;
}

/*
  For each entry in filesList, it returns the corresponding entry in files1,
  if existant.
  I.e. extractFiles() is the complement to substractFile(),
  i.e. addFiles(extractFiles(a, b), substractFiles(a, b)) matches a.

  @param files1  FilesStats
  @param filesList  FilesList
  @result  FilesStats  Subset of files1
*/
function extractFiles(files1, filesList)
{
  var result = new Array();

  // sanitize input
  if (!filesList)
    filesList = new Array();
  if (!files1)
    files1 = new Array();

  var files1Indexed = createFilesIndexed(files1);

  for (var i = 0, l = filesList.length; i < l; i++)
  {
    var f = files1Indexed[filesList[i].filename];
    if (f)
      result.push(f);
  }
  return result;
}

/*
  Returns files1 - filesList, i.e. returns all entries of files1 which
  do *not* have a corresponding entry in filesList.

  @param files1  FilesStats
  @param filesList  FilesList
  @result  FilesStats  Subset of files1
*/
function substractFiles(files1, filesList)
{
  var result = new Array();

  // sanitize input
  if (!filesList)
    filesList = new Array();
  if (!files1)
    files1 = new Array();

  var filesListIndexed = createFilesIndexed(filesList);

  for (var i = 0, l = files1.length; i < l; i++)
  {
    var f = filesListIndexed[files1[i].filename];
    if (!f)
      result.push(files1[i]);
  }

  return result;
}

/*
  Returns files1 + files2, i.e. returns all entries of files1 and
  all entries of files2. It is *not* garanteed that every entry appears
  only once in the result.

  @param files1  FilesList
  @param files2  FilesList
  @result  FilesStats  Superset of files1 and files2
*/
function addFiles(files1, files2)
{
  return files1.concat(files2);
}

/*
   Returns |files|'s entry which matches |filename|.

   This may be very to use inefficient in loops, up to freezing
   the app for several seconds. Use createFilesIndexed() instead.

   @param files  FilesStats  an array to search in
   @param filename  String  the file to search for
   @return  FileStats (singular)  has matching filename
            nothing  if not found
*/
function findEntry(filename, files)
{
  if (!filename || !files)
    return;

  var f;
  for (var i = 0, l = files.length; i < l; i++)
  {
    if (files[i].filename == filename)
      f = files[i];
  }
  return f;
}

/* Creates an filename-indexed object from |files|, i.e. for each
   object A in |files| it creates a result[A.filename] with A as content,
   e.g. result["myfile"] gives an A with A.filename = "myfile".

   @param files  FilesStats
   @return FilesIndexed
 */
function createFilesIndexed(files)
{
  var result = Object();
  for (var i = 0, l = files.length; i < l; i++)
    result[files[i].filename] = files[i];
  return result;
}


/*
   Reads an (XML) listing file from disk and returns the contained data as
   FilesStats (by invoking readListingFile).

   Non-blocking, it will return before the file is loaded, so you need to
   specify a callback to be able to actually use the returned array.

   @param filename  String  filename rel to local dir.
                       May be null (to be used e.g. when the network transfer 
                       failed), in which case the result array will be empty.
   @param loadedCallback  function()  will be called, when the
                       DOMDocument finished loading and was read,
                       i.e. that's where you continue processing.
   @return Object with property |value| of type FilesStats
                       (i.e. Files Stats passed by reference)
                       The data read from the file.
                       It may be empty, in case loading/reading the file
                       failed (e.g. file doesn't exist).
                       Attention: you cannot use the data after the
                       function returned, only in the loadedCallback.
*/
function loadAndReadListingFile(filename, loadedCallback)
{
  var result = new Object();
  result.value = new Array(); /* passing back just the array seems to pass
                                 by value, while the property value seems
                                 to be passed by reference. We need to pass
                                 by reference, because it will be filled,
                                 as described above. */

  var domdoc = null;
  var loaded = function()
  {
    result.value = readListingFile(domdoc);
    loadedCallback();
  }

  if (filename)
  {
    try
    {
      domdoc = document.implementation.createDocument("", filename, null);
      var url = GetIOService().newFileURI(makeNSIFileFromRelFilename(filename));
      //ddump("trying to load " + url.spec);
      //      domdoc.async = true;
      domdoc.load(url.spec);
      domdoc.addEventListener("load", loaded, false);
    }
    catch(e)
    {
      //ddump("error during load: " + e);
      setTimeout(loaded, 0); // see below
    }
  }
  else
  {
    //ddump("no filename");
    setTimeout(loaded, 0);
      /* using timeout, so that we first return (to deliver |result| to the
         caller) before we invoke loadedCallback, so that loadedCallback
         can use |result|. */
  }
  //ddumpObject(result, "result");
  return result;
}

/*
  Takes a listing file and reads it into the returned array

  @param listingFileDOM  nsIDOMDocument  listing file, already read into DOM
  @result  FilesStats  list of files which have to be transferred.
                       Will be empty in the case of an error.
 */
function readListingFile(listingFileDOM)
{
  //ddump("will interpret listing file");
  var files = new Array();

  if (!listingFileDOM || !listingFileDOM.childNodes)
  {
    dumpError("got no listing file");
    return files;
  }

  try
  {
    printtree(listingFileDOM);
    var root = listingFileDOM.childNodes; // document's children
    var listing; // <listing>'s children

    /* although the file has only one real root node, there are always
       spurious other nodes (#text etc.), which we have to skip/ignore */
    for (var i = 0, l = root.length; i < l; i++)
    {
      var curNode = root.item(i);
      if (curNode.nodeType == Node.ELEMENT_NODE
          && curNode.tagName == "listing")
        listing = curNode.childNodes;
    }
    if (listing == undefined)
    {
      dumpError("malformed listing file");
      return files;
    }

    for (var i = 0, l = listing.length; i < l; i++) // <file>s and <directory>s
    {
      var curNode = listing.item(i);
      if (curNode.nodeType == Node.ELEMENT_NODE && curNode.tagName == "file")
      {
        var f = new Object();
        f.filename = curNode.getAttribute("filename");
        f.mimetype = curNode.getAttribute("mimetype");
        f.size = parseInt(curNode.getAttribute("size"));
        f.lastModified =
               parseInt(curNode.getAttribute("lastModifiedUnixtime")) * 1000;

        files.push(f);
      }
      // <directory>s not yet supported
      // ignore spurious nodes like #text etc.
    }
  }
  catch(e)
  {
    dumpError("Error while reading listing file: " + e);
    files = new Array();
         // should return incomplete list? Would break some callers.
    return files;
  }

  //ddumpObject(files, "files");
  return files;
}

function printtree(domnode, indent)
{
  return;
  if (!indent)
    indent = 1;
  for (var i = 0; i < indent; i++)
    ddumpCont("  ");
  ddumpCont(domnode.nodeName);
  if (domnode.nodeType == Node.ELEMENT_NODE)
    ddump(" (Tag)");
  else
    ddump("");

  var root = domnode.childNodes;
  for (var i = 0, l = root.length; i < l; i++)
    printtree(root.item(i), indent + 1);
}

/* Creates an XML file on the filesystem from a files array.

   Example content:
<?xml version="1.0"?>
<listing>
  <file
    filename="index.html"
    mimetype="text/html"
    size="1950"
    lastModifiedUnixtime="1045508684"
  />
</listing>

   @param files  FilesStats  data to be written to the file
   @param filename  String  filename of the to-be-created XML file, relative
                            to the local local dir
   @result nothing
   @throws all errors (that can happen during file creation/writing)
 */
function createListingFile(files, filename)
{
  // create content as string
  var content = "<?xml version=\"1.0\"?>\n" //content of the file to be written
              + "<listing xmlns=\"http://www.mozilla.org/keymaster/"
              + "transfer-listing.xml\">\n";
  for (var i = 0, l = files.length; i < l; i++)
  {
    var f = files[i];
    content += "  <file\n"
             + "    filename=\"" + f.filename + "\"\n"
             + "    mimetype=\"" + f.mimetype + "\"\n"
             + "    size=\"" + f.size + "\"\n"
             + "    lastModifiedUnixtime=\"" + (f.lastModified / 1000) + "\"\n"
             + "  />\n";
  }
  content += "</listing>\n";

  // write string to file
  var lf = makeNSIFileFromRelFilename(filename);
  if (!lf)
    return;
  if (!lf.exists())
    lf.create(Components.interfaces.nsIFile.NORMAL_FILE_TYPE, 0600); // needed?
  var fos = Components
            .classes["@mozilla.org/network/file-output-stream;1"]
            .createInstance(Components.interfaces.nsIFileOutputStream);
  var bos = Components
            .classes["@mozilla.org/network/buffered-output-stream;1"]
            .createInstance(Components.interfaces.nsIBufferedOutputStream);
  fos.init(lf, -1, -1, 0); //odd params from NS_NewLocalFileOutputStream
  bos.init(fos, 32768);
  bos.write(content, content.length);
  bos.close(); // throwing?
  fos.close(); // dito
}


/*
  Creates a FilesStats (with stats from the filesystem) from the
  local files.
  If a file doesn't exist, no entry will be returned for that file.

  @param  checkFiles  FilesList  List of files to be checked
  @param  baseDir  String  filenames in |checkFiles| are relative to this dir
                           null: use makeNSIFileFromRelFilename
                           non-null: file: URL to base dir, with trailing slash
  @result  FilesStats  filesystem stats of the files
 */
function localFilesStats(checkFiles, baseDir)
{
  var result = new Array();

  for (var i = 0, l = checkFiles.length; i < l; i++)
  {
    var filename = checkFiles[i].filename;

    // create file entry for result array
    var f = new Object();
    f.filename = filename;
    f.mimetype = kLocalFilesDefaultMimetype;
    // dummy values for error case
    f.lastModified = 0;
    f.size = 0;

    try
    {
      var lf = baseDir == null ? makeNSIFileFromRelFilename(filename)
                               : makeNSIFileFromAbsFileURL(baseDir + filename);
//XXX change needed?
      if (lf.exists() && lf.isFile() && lf.isReadable() && lf.isWritable())
      {
        f.lastModified = lf.lastModifiedTime;
        f.size = lf.fileSize;
      }
      else
      {
        continue;
        // use error field?
      }
    }
    catch(e)
    {
      continue;
      // XXX use error field? (see also above)
    }
    result.push(f);
  }
  return result;
}


/* Create nsIFile from filename relative to local dir.
   "Local dir" is implementation-specific.

   @param file  String  filename/path relative to the local dir
   @result nsIFile
function makeNSIFileFromRelFilename(filename)

must be implemented elsewhere, because the "root" of the relational filename
depends on the application
*/

/* Create nsIFile from (absolute) file: URL

   @param fileURL  String  absolute file: URL
   @result nsIFile
*/
function makeNSIFileFromAbsFileURL(fileURL)
{
  return GetIOService().newURI(fileURL, null, null)
                       .QueryInterface(Components.interfaces.nsIFileURL)
                       .file.clone();
}

/* Moves a file in the local dir to another name there.
 */
function move(from, to)
{
  //ddump("move " + from + " to " + to);
  var lf = makeNSIFileFromRelFilename(from);
  if (lf && lf.exists())
    lf.moveTo(null, to);
}


/* Deletes a file in the local dir
 */
function remove(filename)
{
  //ddump("remove " + filename);
  var lf = makeNSIFileFromRelFilename(filename);
  if (lf && lf.exists())
    lf.remove(false);
}
