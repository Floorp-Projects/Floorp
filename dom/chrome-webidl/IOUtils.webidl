/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */
[ChromeOnly, Exposed=(Window, Worker)]
namespace IOUtils {
 /**
   * Reads up to |maxBytes| of the file at |path|. If |maxBytes| is unspecified,
   * the entire file is read.
   *
   * @param path      An absolute file path.
   * @param maxBytes  The max bytes to read from the file at path.
   */
  Promise<Uint8Array> read(DOMString path, optional unsigned long maxBytes);
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
   */
  Promise<unsigned long long> writeAtomic(DOMString path, Uint8Array data, optional WriteAtomicOptions options = {});
  /**
   * Moves the file from |sourcePath| to |destPath|, creating necessary parents.
   * If |destPath| is a directory, then the source file will be moved into the
   * destination directory.
   *
   * @param sourcePath An absolute file path identifying the file or directory
   *                   to move.
   * @param destPath   An absolute file path identifying the destination
   *                   directory and/or file name.
   */
  Promise<void> move(DOMString sourcePath, DOMString destPath, optional MoveOptions options = {});
  /**
   * Removes a file or directory at |path| according to |options|.
   *
   * @param path An absolute file path identifying the file or directory to
   *             remove.
   */
  Promise<void> remove(DOMString path, optional RemoveOptions options = {});
};

/**
 * Options to be passed to the |IOUtils.writeAtomic| method.
 */
dictionary WriteAtomicOptions {
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
