/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * IOUtils is a simple, efficient interface for performing file I/O from a
 * privileged chrome-only context. All asynchronous I/O tasks are run on
 * a background thread.
 *
 * Pending I/O tasks will block shutdown at the |profileBeforeChange| phase.
 * During this shutdown phase, no additional I/O tasks will be accepted --
 * method calls to this interface will reject once shutdown has entered this
 * phase.
 *
 * IOUtils methods may reject for any number of reasons. Reasonable attempts
 * have been made to map each common operating system error to a |DOMException|.
 * Most often, a caller only needs to check if a given file wasn't found by
 * catching the rejected error and checking if |ex.name === 'NotFoundError'|.
 * In other cases, it is likely sufficient to allow the error to be caught and
 * reported elsewhere.
 */
[ChromeOnly, Exposed=(Window, Worker)]
namespace IOUtils {
 /**
   * Reads up to |maxBytes| of the file at |path| according to |opts|.
   *
   * @param path An absolute file path.
   *
   * @return Resolves with an array of unsigned byte values read from disk,
   *         otherwise rejects with a DOMException.
   */
  Promise<Uint8Array> read(DOMString path, optional ReadOptions opts = {});
  /**
   * Reads the UTF-8 text file located at |path| and returns the decoded
   * contents as a |DOMString|.
   *
   * @param path An absolute file path.
   *
   * @return Resolves with the file contents encoded as a string, otherwise
   *         rejects with a DOMException.
   */
  Promise<UTF8String> readUTF8(DOMString path, optional ReadUTF8Options opts = {});
  /**
   * Read the UTF-8 text file located at |path| and return the contents
   * parsed as JSON into a JS value.
   *
   * @param path An absolute path.
   *
   * @return Resolves with the contents of the file parsed as JSON.
   */
  Promise<any> readJSON(DOMString path, optional ReadUTF8Options opts = {});
  /**
   * Attempts to safely write |data| to a file at |path|.
   *
   * This operation can be made atomic by specifying the |tmpFile| option. If
   * specified, then this method ensures that the destination file is not
   * modified until the data is entirely written to the temporary file, after
   * which point the |tmpFile| is moved to the specified |path|.
   *
   * The target file can also be backed up to a |backupFile| before any writes
   * are performed to prevent data loss in case of corruption.
   *
   * @param path    An absolute file path.
   * @param data    Data to write to the file at path.
   *
   * @return Resolves with the number of bytes successfully written to the file,
   *         otherwise rejects with a DOMException.
   */
  Promise<unsigned long long> write(DOMString path, Uint8Array data, optional WriteOptions options = {});
  /**
   * Attempts to encode |string| to UTF-8, then safely write the result to a
   * file at |path|. Works exactly like |write|.
   *
   * @param path      An absolute file path.
   * @param string    A string to write to the file at path.
   *
   * @return Resolves with the number of bytes successfully written to the file,
   *         otherwise rejects with a DOMException.
   */
  Promise<unsigned long long> writeUTF8(DOMString path, UTF8String string, optional WriteOptions options = {});
  /**
   * Attempts to serialize |value| into a JSON string and encode it as into a
   * UTF-8 string, then safely write the result to a file at |path|. Works
   * exactly like |write|.
   *
   * @param path   An absolute file path
   * @param value  The value to be serialized.
   *
   * @return Resolves with the number of bytes successfully written to the file,
   *         otherwise rejects with a DOMException.
   */
  Promise<unsigned long long> writeJSON(DOMString path, any value, optional WriteOptions options = {});
  /**
   * Moves the file from |sourcePath| to |destPath|, creating necessary parents.
   * If |destPath| is a directory, then the source file will be moved into the
   * destination directory.
   *
   * @param sourcePath An absolute file path identifying the file or directory
   *                   to move.
   * @param destPath   An absolute file path identifying the destination
   *                   directory and/or file name.
   *
   * @return Resolves if the file is moved successfully, otherwise rejects with
   *         a DOMException.
   */
  Promise<void> move(DOMString sourcePath, DOMString destPath, optional MoveOptions options = {});
  /**
   * Removes a file or directory at |path| according to |options|.
   *
   * @param path An absolute file path identifying the file or directory to
   *             remove.
   *
   * @return Resolves if the file is removed successfully, otherwise rejects
   *         with a DOMException.
   */
  Promise<void> remove(DOMString path, optional RemoveOptions options = {});
  /**
   * Creates a new directory at |path| according to |options|.
   *
   * @param path An absolute file path identifying the directory to create.
   *
   * @return Resolves if the directory is created successfully, otherwise
   *         rejects with a DOMException.
   */
  Promise<void> makeDirectory(DOMString path, optional MakeDirectoryOptions options = {});
  /**
   * Obtains information about a file, such as size, modification dates, etc.
   *
   * @param path An absolute file path identifying the file or directory to
   *             inspect.
   *
   * @return Resolves with a |FileInfo| object for the file at path, otherwise
   *         rejects with a DOMException.
   *
   * @see FileInfo
   */
  Promise<FileInfo> stat(DOMString path);
  /**
   * Copies a file or directory from |sourcePath| to |destPath| according to
   * |options|.
   *
   * @param sourcePath An absolute file path identifying the source file to be
   *                   copied.
   * @param destPath   An absolute file path identifying the location for the
   *                   copy.
   *
   * @return Resolves if the file was copied successfully, otherwise rejects
   *         with a DOMException.
   */
  Promise<void> copy(DOMString sourcePath, DOMString destPath, optional CopyOptions options = {});
  /**
   * Updates the |modification| time for the file at |path|.
   *
   * @param path         An absolute file path identifying the file to touch.
   * @param modification An optional modification time for the file expressed in
   *                     milliseconds since the Unix epoch
   *                     (1970-01-01T00:00:00Z). The current system time is used
   *                     if this parameter is not provided.
   *
   * @return Resolves with the updated modification time expressed in
   *         milliseconds since the Unix epoch, otherwise rejects with a
   *         DOMException.
   */
  Promise<long long> touch(DOMString path, optional long long modification);
  /**
   * Retrieves a (possibly empty) list of immediate children of the directory at
   * |path|. If the file at |path| is not a directory, this method resolves with
   * an empty list.
   *
   * @param path An absolute file path.
   *
   * @return Resolves with a sequence of absolute file paths representing the
   *         children of the directory at |path|, otherwise rejects with a
   *         DOMException.
   */
  Promise<sequence<DOMString>> getChildren(DOMString path);
  /**
   * Set the permissions of the file at |path|.
   *
   * Windows does not make a distinction between user, group, and other
   * permissions like UNICES do. If a permission flag is set for any of user,
   * group, or other has a permission, then all users will have that
   * permission. Additionally, Windows does not support setting the
   * "executable" permission.
   *
   * @param path        An absolute file path
   * @param permissions The UNIX file mode representing the permissions.
   * @param honorUmask  If omitted or true, any UNIX file mode value is
   *                    modified by the process umask. If false, the exact value
   *                    of UNIX file mode will be applied. This value has no effect
   *                    on Windows.
   *
   * @return Resolves if the permissions were set successfully, otherwise
   *         rejects with a DOMException.
   */
  Promise<void> setPermissions(DOMString path, unsigned long permissions, optional boolean honorUmask = true);
  /**
   * Return whether or not the file exists at the given path.
   *
   * @param path An absolute file path.
   *
   * @return A promise that resolves to whether or not the given file exists.
   */
  Promise<boolean> exists(DOMString path);
};

/**
 * Options to be passed to the |IOUtils.readUTF8| method.
 */
dictionary ReadUTF8Options {
  /**
   * If true, this option indicates that the file to be read is compressed with
   * LZ4-encoding, and should be decompressed before the data is returned to
   * the caller.
   */
  boolean decompress = false;
};

/**
 * Options to be passed to the |IOUtils.read| method.
 */
dictionary ReadOptions : ReadUTF8Options {
  /**
   * The max bytes to read from the file at path. If unspecified, the entire
   * file will be read. This option is incompatible with |decompress|.
   */
  unsigned long? maxBytes = null;
};

/**
 * Options to be passed to the |IOUtils.write| and |writeUTF8|
 * methods.
 */
dictionary WriteOptions {
  /**
   * If specified, backup the destination file to this path before writing.
   */
  DOMString backupFile;
  /**
   * If specified, write the data to a file at |tmpPath| instead of directly to
   * the destination. Once the write is complete, the destination will be
   * overwritten by a move. Specifying this option will make the write a little
   * slower, but also safer.
   */
  DOMString tmpPath;
  /**
   * If true, fail if the destination already exists.
   */
  boolean noOverwrite = false;
  /**
   * If true, force the OS to write its internal buffers to the disk.
   * This is considerably slower for the whole system, but safer in case of
   * an improper system shutdown (e.g. due to a kernel panic) or device
   * disconnection before the buffers are flushed.
   */
  boolean flush = false;
  /**
   * If true, compress the data with LZ4-encoding before writing to the file.
   */
  boolean compress = false;
};

/**
 * Options to be passed to the |IOUtils.move| method.
 */
dictionary MoveOptions {
  /**
   * If true, fail if the destination already exists.
   */
  boolean noOverwrite = false;
};

/**
 * Options to be passed to the |IOUtils.remove| method.
 */
dictionary RemoveOptions {
  /**
   * If true, no error will be reported if the target file is missing.
   */
  boolean ignoreAbsent = true;
  /**
   * If true, and the target is a directory, recursively remove files.
   */
  boolean recursive = false;
};

/**
 * Options to be passed to the |IOUtils.makeDirectory| method.
 */
dictionary MakeDirectoryOptions {
  /**
   * If true, create the directory and all necessary ancestors if they do not
   * already exist. If false and any ancestor directories do not exist,
   * |makeDirectory| will reject with an error.
   */
  boolean createAncestors = true;
  /**
   * If true, succeed even if the directory already exists (default behavior).
   * Otherwise, fail if the directory already exists.
   */
  boolean ignoreExisting = true;
  /**
   * The file mode to create the directory with.
   *
   * This is ignored on Windows.
   */
  unsigned long permissions = 0755;

};

/**
 * Options to be passed to the |IOUtils.copy| method.
 */
dictionary CopyOptions {
  /**
   * If true, fail if the destination already exists.
   */
  boolean noOverwrite = false;
  /**
   * If true, copy the source recursively.
   */
  boolean recursive = false;
};

/**
 * Types of files that are recognized by the |IOUtils.stat| method.
 */
enum FileType { "regular", "directory", "other" };

/**
 * Basic metadata about a file.
 */
dictionary FileInfo {
  /**
   * The absolute path to the file on disk, as known when this file info was
   * obtained.
   */
  DOMString path;
  /**
   * Identifies if the file at |path| is a regular file, directory, or something
   * something else.
   */
  FileType type;
  /**
   * If this represents a regular file, the size of the file in bytes.
   * Otherwise, -1.
   */
  long long size;
  /**
   * The timestamp of the last file modification, represented in milliseconds
   * since Epoch (1970-01-01T00:00:00.000Z).
   */
  long long lastModified;

  /**
   * The timestamp of file creation, represented in milliseconds since Epoch
   * (1970-01-01T00:00:00.000Z).
   *
   * This is only available on MacOS and Windows.
   */
  long long creationTime;
  /**
   * The permissions of the file, expressed as a UNIX file mode.
   *
   * NB: Windows does not make a distinction between user, group, and other
   * permissions like UNICES do. The user, group, and other parts will always
   * be identical on Windows.
   */
  unsigned long permissions;
};
