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
   * @param path      A forward-slash separated path.
   * @param maxBytes  The max bytes to read from the file at path.
   */
  Promise<Uint8Array> read(DOMString path, optional unsigned long maxBytes);
  /**
   * Atomically write |data| to a file at |path|. This operation attempts to
   * ensure that until the data is entirely written to disk, the destination
   * file is not modified.
   *
   * This is generally accomplished by writing to a temporary file, then
   * performing an overwriting move.
   *
   * @param path    A forward-slash separated path.
   * @param data    Data to write to the file at path.
   */
  Promise<unsigned long long> writeAtomic(DOMString path, Uint8Array data, optional WriteAtomicOptions options = {});
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
   * If specified, write the data to a file at |tmpPath|. Once the write is
   * complete, the destination will be overwritten by a move.
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
