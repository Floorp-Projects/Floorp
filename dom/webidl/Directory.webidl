/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

interface File;

/*
 * All functions on Directory that accept DOMString arguments for file or
 * directory names only allow relative path to current directory itself. The
 * path should be a descendent path like "path/to/file.txt" and not contain a
 * segment of ".." or ".". So the paths aren't allowed to walk up the directory
 * tree. For example, paths like "../foo", "..", "/foo/bar" or "foo/../bar" are
 * not allowed.
 */
[NoInterfaceObject]
interface Directory {
  /*
   * The leaf name of the directory.
   */
  readonly attribute DOMString name;

  /*
   * Creates a new file or replaces an existing file with given data. The file
   * should be a descendent of current directory.
   *
   * @param path The relative path of the new file to current directory.
   * @param options It has two optional properties, 'ifExists' and 'data'.
   * If 'ifExists' is 'fail' and the path already exists, createFile must fail;
   * If 'ifExists' is 'replace', the path already exists, and is a file, create
   * a new file to replace the existing one;
   * If 'ifExists' is 'replace', the path already exists, but is a directory,
   * createFile must fail.
   * Otherwise, if no other error occurs, createFile will create a new file.
   * The 'data' property contains the new file's content.
   * @return If succeeds, the promise is resolved with the new created
   * File object. Otherwise, rejected with a DOM error.
   */
  [NewObject, Throws]
  Promise<File> createFile(DOMString path, optional CreateFileOptions options);

  /*
   * Creates a descendent directory. This method will create any intermediate
   * directories specified by the path segments.
   *
   * @param path The relative path of the new directory to current directory.
   * If path exists, createDirectory must fail.
   * @return If succeeds, the promise is resolved with the new created
   * Directory object. Otherwise, rejected with a DOM error.
   */
  [NewObject, Throws]
  Promise<Directory> createDirectory(DOMString path);

  /*
   * Gets a descendent file or directory with the given path.
   *
   * @param path The descendent entry's relative path to current directory.
   * @return If the path exists and no error occurs, the promise is resolved
   * with a File or Directory object, depending on the entry's type. Otherwise,
   * rejected with a DOM error.
   */
  [NewObject, Throws]
  Promise<(File or Directory)> get(DOMString path);

  /*
   * Deletes a file or an empty directory. The target must be a descendent of
   * current directory.
   * @param path If a DOM string is passed, it is the relative path of the
   * target. Otherwise, the File or Directory object of the target should be
   * passed.
   * @return If the target is a non-empty directory, or if deleting the target
   * fails, the promise is rejected with a DOM error. If the target did not
   * exist, the promise is resolved with boolean false. If the target did exist
   * and was successfully deleted, the promise is resolved with boolean true.
   */
  [NewObject, Throws]
  Promise<boolean> remove((DOMString or File or Directory) path);

  /*
   * Deletes a file or a directory recursively. The target should be a
   * descendent of current directory.
   * @param path If a DOM string is passed, it is the relative path of the
   * target. Otherwise, the File or Directory object of the target should be
   * passed.
   * @return If the target exists, but deleting the target fails, the promise is
   * rejected with a DOM error. If the target did not exist, the promise is
   * resolved with boolean false. If the target did exist and was successfully
   * deleted, the promise is resolved with boolean true.
   */
  [NewObject, Throws]
  Promise<boolean> removeDeep((DOMString or File or Directory) path);
};

enum CreateIfExistsMode { "replace", "fail" };

dictionary CreateFileOptions {
  CreateIfExistsMode ifExists = "fail";
  (DOMString or Blob or ArrayBuffer or ArrayBufferView) data;
};
