/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

[NoInterfaceObject]
interface Entry {
    readonly attribute boolean isFile;
    readonly attribute boolean isDirectory;

    [GetterThrows]
    readonly attribute DOMString name;

    [GetterThrows]
    readonly attribute DOMString fullPath;

    [GetterThrows]
    readonly attribute DOMFileSystem filesystem;

/** Not implemented:
 *  void getMetadata(MetadataCallback successCallback, optional ErrorCallback errorCallback);
 *  void moveTo(DirectoryEntry parent, optional DOMString? name, optional EntryCallback successCallback, optional ErrorCallback errorCallback);
 *  void copyTo(DirectoryEntry parent, optional DOMString? name, optional EntryCallback successCallback, optional ErrorCallback errorCallback);
 *  DOMString toURL();
 *  void remove(VoidCallback successCallback, optional ErrorCallback errorCallback);
 *  void getParent(optional EntryCallback successCallback, optional ErrorCallback errorCallback);
 */
};

dictionary FileSystemFlags {
    boolean create = false;
    boolean exclusive = false;
};

callback interface EntryCallback {
    void handleEvent(Entry entry);
};

callback interface VoidCallback {
    void handleEvent();
};

[NoInterfaceObject]
interface DirectoryEntry : Entry {
    DirectoryReader createReader();

    [Throws]
    void getFile(DOMString? path, optional FileSystemFlags options, optional EntryCallback successCallback, optional ErrorCallback errorCallback);

    [Throws]
    void getDirectory(DOMString? path, optional FileSystemFlags options, optional EntryCallback successCallback, optional ErrorCallback errorCallback);

    // This method is not implemented. ErrorCallback will be called with
    // NS_ERROR_DOM_NOT_SUPPORTED_ERR.
    void removeRecursively(VoidCallback successCallback, optional ErrorCallback errorCallback);
};

callback interface EntriesCallback {
    void handleEvent(sequence<Entry> entries);
};

callback interface ErrorCallback {
    // This should be FileError but we are implementing just a subset of this API.
    void handleEvent(DOMError error);
};

[NoInterfaceObject]
interface DirectoryReader {

    // readEntries can be called just once. The second time it returns no data.

    [Throws]
    void readEntries (EntriesCallback successCallback, optional ErrorCallback errorCallback);
};

callback interface BlobCallback {
    void handleEvent(Blob? blob);
};

[NoInterfaceObject]
interface FileEntry : Entry {
    // the successCallback should be a FileWriteCallback but this method is not
    // implemented. ErrorCallback will be called with
    // NS_ERROR_DOM_NOT_SUPPORTED_ERR.
    void createWriter (VoidCallback successCallback, optional ErrorCallback errorCallback);

    [BinaryName="GetFile"]
    void file (BlobCallback successCallback, optional ErrorCallback errorCallback);
};

[NoInterfaceObject]
interface DOMFileSystem {
    [GetterThrows]
    readonly    attribute DOMString      name;

    [GetterThrows]
    readonly    attribute DirectoryEntry root;
};
