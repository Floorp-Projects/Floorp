/* -*- Mode: IDL; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/.
 */

/**
 * PathUtils is a set of utilities for operating on absolute paths.
 */
[ChromeOnly, Exposed=(Window, Worker)]
namespace PathUtils {
  /**
   * Return the last path component.
   *
   * @param path An absolute path.
   *
   * @returns The last path component.
   */
  [Throws]
  DOMString filename(DOMString path);

  /**
   * Return an ancestor directory of the given path.
   *
   * @param path An absolute path.
   * @param depth The number of ancestors to remove, defaulting to 1 (i.e., the
   *              parent).
   *
   * @return The ancestor directory.
   *
   *         If the path provided is a root path (e.g., `C:` on Windows or `/`
   *         on *NIX), then null is returned.
   */
  [Throws]
  DOMString? parent(DOMString path, optional long depth = 1);

  /**
   * Join the given components into a full path.
   *
   * @param components The path components. The first component must be an
   *                   absolute path.
   */
  [Throws]
  DOMString join(DOMString... components);

  /**
   * Join the given relative path to the base path.
   *
   * @param base The base path. This must be an absolute path.
   * @param relativePath A relative path to join to the base path.
   */
  [Throws]
  DOMString joinRelative(DOMString base, DOMString relativePath);

  /**
   * Creates an adjusted path using a path whose length is already close
   * to MAX_PATH. For windows only.
   *
   * @param path An absolute path.
   */
  [Throws]
  DOMString toExtendedWindowsPath(DOMString path);

  /**
   * Normalize a path by removing multiple separators and `..` and `.`
   * directories.
   *
   * On UNIX platforms, the path must exist as symbolic links will be resolved.
   *
   * @param path The absolute path to normalize.
   *
   */
  [Throws]
  DOMString normalize(DOMString path);

  /**
   * Split a path into its components.
   *
   * @param path An absolute path.
   */
  [Throws]
  sequence<DOMString> split(DOMString path);

  /**
   * Transform a file path into a file: URI
   *
   * @param path An absolute path.
   *
   * @return The file: URI as a string.
   */
  [Throws]
  UTF8String toFileURI(DOMString path);

  /**
   * Determine if the given path is an absolute or relative path.
   *
   * @param path A file path that is either relative or absolute.
   *
   * @return Whether or not the path is absolute.
   */
  boolean isAbsolute(DOMString path);
};

[Exposed=Window]
partial namespace PathUtils {
  /**
   * The profile directory.
   */
  [Throws, BinaryName="ProfileDirSync"]
  readonly attribute DOMString profileDir;

  /**
   * The local-specific profile directory.
   */
  [Throws, BinaryName="LocalProfileDirSync"]
  readonly attribute DOMString localProfileDir;

  /**
   * The temporary directory for the process.
   */
  [Throws, BinaryName="TempDirSync"]
  readonly attribute DOMString tempDir;

  /**
   * The OS temporary directory.
   */
  [Throws, BinaryName="OSTempDirSync"]
  readonly attribute DOMString osTempDir;

  /**
   * The libxul path.
   */
  [Throws, BinaryName="XulLibraryPathSync"]
  readonly attribute DOMString xulLibraryPath;
};

[Exposed=Worker]
partial namespace PathUtils {
  /**
   * The profile directory.
   */
  [NewObject, BinaryName="GetProfileDirAsync"]
  Promise<DOMString> getProfileDir();

  /**
   * The local-specific profile directory.
   */
  [NewObject, BinaryName="GetLocalProfileDirAsync"]
  Promise<DOMString> getLocalProfileDir();

  /**
   * The temporary directory for the process.
   */
  [NewObject, BinaryName="GetTempDirAsync"]
  Promise<DOMString> getTempDir();

  /**
   * The OS temporary directory.
   */
  [NewObject, BinaryName="GetOSTempDirAsync"]
  Promise<DOMString> getOSTempDir();

  /**
   * The libxul path.
   */
  [NewObject, BinaryName="GetXulLibraryPathAsync"]
  Promise<DOMString> getXulLibraryPath();
};
