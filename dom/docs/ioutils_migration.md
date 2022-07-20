# IOUtils Migration Guide

**Improving performance through a new file API**

---

## What is IOUtils?

`IOUtils` is a privileged JavaScript API for performing file I/O in the Firefox frontend.
It was developed as a replacement for `OS.File`, addressing
[bug 1231711](https://bugzilla.mozilla.org/show_bug.cgi?id=1231711).
It is *not to be confused* with the unprivileged
[DOM File API](https://developer.mozilla.org/en-US/docs/Web/API/File).

`IOUtils` provides a minimal API surface to perform common
I/O tasks via a collection of static methods inspired from `OS.File`.
It is implemented in C++, and exposed to JavaScript via WebIDL bindings.

The most up-to-date API can always be found in
[IOUtils.webidl](https://searchfox.org/mozilla-central/source/dom/chrome-webidl/IOUtils.webidl).

## Differences from `OS.File`

`IOUtils` has a similar API to `OS.File`, but one should keep in mind some key differences.

### No `File` instances (except `SyncReadFile` in workers)

Most of the `IOUtils` methods only operate on absolute path strings, and don't expose a file handle to the caller.
The exception to this rule is the `openFileForSyncReading` API, which is only available in workers.

Furthermore, `OS.File` was exposing platform-specific file descriptors through the
[`fd`](https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/OSFile.jsm/OS.File_for_workers#Attributes)
attribute. `IOUtils` does not expose file descriptors.

### WebIDL has no `Date` type

`IOUtils` is written in C++ and exposed to JavaScript through WebIDL.
Many uses of `OS.File` concern themselves with obtaining or manipulating file metadata,
like the last modified time, however the `Date` type does not exist in WebIDL.
Using `IOUtils`,
these values are returned to the caller as the number of milliseconds since
`1970-01-01T00:00:00Z`.
`Date`s can be safely constructed from these values if needed.

For example, to obtain the last modification time of a file and update it to the current time:

```js
let { lastModified } = await IOUtils.stat(path);

let lastModifiedDate = new Date(lastModified);

let now = new Date();

await IOUtils.touch(path, now.valueOf());
```

### Some methods are not implemented

For various reasons
(complexity, safety, availability of underlying system calls, usefulness, etc.)
the following `OS.File` methods have no analogue in IOUtils.
They also will **not** be implemented.

-   void unixSymlink(in string targetPath, in string createPath)
-   string getCurrentDirectory(void)
-   void setCurrentDirectory(in string path)
-   object open(in string path)
-   object openUnique(in string path)

### Errors are reported as `DOMException`s

When an `OS.File` method runs into an error,
it will throw/reject with a custom
[`OS.File.Error`](https://developer.mozilla.org/en-US/docs/Mozilla/JavaScript_code_modules/OSFile.jsm/OS.File.Error).
These objects have custom attributes that can be checked for common error cases.

`IOUtils` has similar behaviour, however its methods consistently reject with a
[`DOMException`](https://developer.mozilla.org/en-US/docs/Web/API/DOMException)
whose name depends on the failure:

| Exception Name | Reason for exception |
| -------------- | -------------------- |
| `NotFoundError` | A file at the specified path could not be found on disk. |
| `NotAllowedError` | Access to a file at the specified path was denied by the operating system. |
| `NotReadableError` | A file at the specified path could not be read for some reason. It may have been too big to read, or it was corrupt, or some other reason. The exception message should have more details. |
| `ReadOnlyError` | A file at the specified path is read only and could not be modified. |
| `NoModificationAllowedError` | A file already exists at the specified path and could not be overwritten according to the specified options. The exception message should have more details. |
| `OperationError` | Something went wrong during the I/O operation. E.g. failed to allocate a buffer. The exception message should have more details. |
| `UnknownError` | An unknown error occurred in the implementation. An nsresult error code should be included in the exception message to assist with debugging and improving `IOUtils` internal error handling. |

### `IOUtils` is mostly async-only

`OS.File` provided an asynchronous front-end for main-thread consumers,
and a synchronous front-end for workers.
`IOUtils` only provides an asynchronous API for the vast majority of its API surface.
These asynchronous methods can be called from both the main thread and from chrome-privileged worker threads.

The one exception to this rule is `openFileForSyncReading`, which allows synchronous file reading in workers.

## `OS.File` vs `IOUtils`

Some methods and options of `OS.File` keep the same name and underlying behaviour in `IOUtils`,
but others have been renamed.
The following is a detailed comparison with examples of the methods and options in each API.

### Reading a file

`IOUtils` provides the following methods to read data from a file. Like
`OS.File`, they accept an `options` dictionary.

Note: The maximum file size that can be read is `UINT32_MAX` bytes. Attempting
to read a file larger will result in a `NotReadableError`.

```idl
Promise<Uint8Array> read(DOMString path, ...);

Promise<DOMString> readUTF8(DOMString path, ...);

Promise<any> readJSON(DOMString path, ...);

// Workers only:
SyncReadFile openFileForSyncReading(DOMString path);
```

#### Options

| `OS.File` option   | `IOUtils` option             | Description                                                                                                               |
| ------------------ | ---------------------------- | ----------------------------------------------------------------------------------------------------------------------------- |
| bytes: number?     | maxBytes: number?            | If specified, read only up to this number of bytes. Otherwise, read the entire file. Default is null.                         |
| compression: 'lz4' | decompress: boolean          | If true, read the file and return the decompressed LZ4 stream. Otherwise, just read the file byte-for-byte. Default is false. |
| encoding: 'utf-8'  | N/A; use `readUTF8` instead. | Interprets the file as UTF-8 encoded text, and returns a string to the caller.                                                |

#### Examples

##### Read raw (unsigned) byte values

**`OS.File`**
```js
let bytes = await OS.File.read(path); // Uint8Array
```
**`IOUtils`**
```js
let bytes = await IOUtils.read(path); // Uint8Array
```

##### Read UTF-8 encoded text

**`OS.File`**
```js
let utf8 = await OS.File.read(path, { encoding: 'utf-8' }); // string
```
**`IOUtils`**
```js
let utf8 = await IOUtils.readUTF8(path); // string
```

##### Read JSON file

**`IOUtils`**
```js
let obj = await IOUtils.readJSON(path); // object
```

##### Read LZ4 compressed file contents

**`OS.File`**
```js
// Uint8Array
let bytes = await OS.File.read(path, { compression: 'lz4' });
// string
let utf8 = await OS.File.read(path, {
    encoding: 'utf-8',
    compression: 'lz4',
});
```
**`IOUtils`**
```js
let bytes = await IOUtils.read(path, { decompress: true }); // Uint8Array
let utf8 = await IOUtils.readUTF8(path, { decompress: true }); // string
```

##### Synchronously read a fragment of a file into a buffer, from a worker

**`OS.File`**
```js
// Read 64 bytes at offset 128, workers only:
let file = OS.File.open(path, { read: true });
file.setPosition(128);
let bytes = file.read({ bytes: 64 }); // Uint8Array
file.close();
```
**`IOUtils`**
```js
// Read 64 bytes at offset 128, workers only:
let file = IOUtils.openFileForSyncReading(path);
let bytes = new Uint8Array(64);
file.readBytesInto(bytes, 128);
file.close();
```

### Writing to a file

IOUtils provides the following methods to write data to a file. Like
OS.File, they accept an options dictionary.

```idl
Promise<unsigned long long> write(DOMString path, Uint8Array data, ...);

Promise<unsigned long long> writeUTF8(DOMString path, DOMString string, ...);

Promise<unsigned long long> writeJSON(DOMString path, any value, ...);
```

#### Options

| `OS.File` option     | `IOUtils` option              | Description                                                                                                                                                               |
| -------------------- | ----------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| backupTo: string?    | backupFile: string?           | Identifies the path to backup the target file to before performing the write operation. If unspecified, no backup will be performed. Default is null.                     |
| tmpPath: string?     | tmpPath: string?              | Identifies a path to write to first, before performing a move to overwrite the target file. If unspecified, the target file will be written to directly. Default is null. |
| noOverwrite: boolean | noOverwrite: boolean          | If true, fail if the destination already exists. Default is false.                                                                                                        |
| flush: boolean       | flush: boolean                | If true, force the OS to flush its internal buffers to disk. Default is false.                                                                                            |
| encoding: 'utf-8'    | N/A; use `writeUTF8` instead. | Allows the caller to supply a string to be encoded as utf-8 text on disk.                                                                                                 |

#### Examples
##### Write raw (unsigned) byte values

**`OS.File`**
```js
let bytes = new Uint8Array();
await OS.File.writeAtomic(path, bytes);
```

**`IOUtils`**
```js
let bytes = new Uint8Array();
await IOUtils.write(path, bytes);
```

##### Write UTF-8 encoded text

**`OS.File`**
```js
let str = "";
await OS.File.writeAtomic(path, str, { encoding: 'utf-8' });
```

**`IOUtils`**
```js
let str = "";
await IOUtils.writeUTF8(path, str);
```

##### Write A JSON object

**`IOUtils`**
```js
let obj = {};
await IOUtils.writeJSON(path, obj);
```

##### Write with LZ4 compression

**`OS.File`**
```js
let bytes = new Uint8Array();
await OS.File.writeAtomic(path, bytes, { compression: 'lz4' });
let str = "";
await OS.File.writeAtomic(path, str, {
    compression: 'lz4',
});
```

**`IOUtils`**
```js
let bytes = new Uint8Array();
await IOUtils.write(path, bytes, { compress: true });
let str = "";
await IOUtils.writeUTF8(path, str, { compress: true });
```

### Move a file

`IOUtils` provides the following method to move files on disk.
Like `OS.File`, it accepts an options dictionary.

```idl
Promise<void> move(DOMString sourcePath, DOMString destPath, ...);
```

#### Options

| `OS.File` option     | `IOUtils` option                    | Description                                                                                                                                                               |
| -------------------- | ----------------------------------- | ---------------------------------------------------------------------------- |
| noOverwrite: boolean | noOverwrite: boolean                | If true, fail if the destination already exists. Default is false.           |
| noCopy: boolean      | N/A; will not be implemented        | This option is not implemented in `IOUtils`, and will be ignored if provided |

#### Example

**`OS.File`**
```js
await OS.File.move(srcPath, destPath);
```

**`IOUtils`**
```js
await IOUtils.move(srcPath, destPath);
```

### Remove a file

`IOUtils` provides *one* method to remove files from disk.
`OS.File` provides several methods.

```idl
Promise<void> remove(DOMString path, ...);
```

#### Options

| `OS.File` option                                           | `IOUtils` option      | Description                                                                                                      |
| ---------------------------------------------------------- | --------------------- | ---------------------------------------------------------------------------------------------------------------- |
| ignoreAbsent: boolean                                      | ignoreAbsent: boolean | If true, and the destination does not exist, then do not raise an error. Default is true.                        |
| N/A; `OS.File` has dedicated methods for directory removal | recursive: boolean    | If true, and the target is a directory, recursively remove the directory and all its children. Default is false. |

#### Examples

##### Remove a file

**`OS.File`**
```js
await OS.File.remove(path, { ignoreAbsent: true });
```

**`IOUtils`**
```js
await IOUtils.remove(path);
```

##### Remove a directory and all its contents

**`OS.File`**
```js
await OS.File.removeDir(path, { ignoreAbsent: true });
```

**`IOUtils`**
```js
await IOUtils.remove(path, { recursive: true });
```

##### Remove an empty directory

**`OS.File`**
```js
await OS.File.removeEmptyDir(path); // Will throw an exception if `path` is not empty.
```

**`IOUtils`**
```js
await IOUtils.remove(path); // Will throw an exception if `path` is not empty.
```

### Make a directory

`IOUtils` provides the following method to create directories on disk.
Like `OS.File`, it accepts an options dictionary.

```idl
Promise<void> makeDirectory(DOMString path, ...);
```

#### Options

| `OS.File` option        | `IOUtils` option         | Description                                                                                                                                                                                                     |
| ----------------------- | ------------------------ | --------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| ignoreExisting: boolean | ignoreExisting: boolean  | If true, succeed even if the target directory already exists. Default is true.                                                                                                                                  |
| from: string            | createAncestors: boolean | If true, `IOUtils` will create all missing ancestors in a path. Default is true. This option differs from `OS.File`, which requires the caller to specify a root path from which to create missing directories. |
| unixMode                | N/A                      | `IOUtils` does not support setting a custom directory mode on unix.                                                                                                                                             |
| winSecurity             | N/A                      | `IOUtils` does not support setting custom directory security settings on Windows.                                                                                                                               |

#### Example

**`OS.File`**
```js
await OS.File.makeDir(srcPath, destPath);
```
**`IOUtils`**
```js
await IOUtils.makeDirectory(srcPath, destPath);
```

### Update a file's modification time

`IOUtils` provides the following method to update a file's modification time.

```idl
Promise<void> setModificationTime(DOMString path, optional long long modification);
```

#### Example

**`OS.File`**
```js
await OS.File.setDates(path, new Date(), new Date());
```

**`IOUtils`**
```js
await IOUtils.setModificationTime(path, new Date().valueOf());
```

### Get file metadata

`IOUtils` provides the following method to query file metadata.

```idl
Promise<void> stat(DOMString path);
```

#### Example

**`OS.File`**
```js
let fileInfo = await OS.File.stat(path);
```

**`IOUtils`**
```js
let fileInfo = await IOUtils.stat(path);
```

### Copy a file

`IOUtils` provides the following method to copy a file on disk.
Like `OS.File`, it accepts an options dictionary.

```idl
Promise<void> copy(DOMString path, ...);
```

#### Options

| `OS.File` option                                                    | `IOUtils` option     | Description                                                    |
| ------------------------------------------------------------------- | -------------------- | -------------------------------------------------------------------|
| noOverwrite: boolean                                                | noOverwrite: boolean | If true, fail if the destination already exists. Default is false. |
| N/A; `OS.File` does not appear to support recursively copying files | recursive: boolean   | If true, copy the source recursively.                              |

#### Examples

##### Copy a file

**`OS.File`**
```js
await OS.File.copy(srcPath, destPath);
```

**`IOUtils`**
```js
await IOUtils.copy(srcPath, destPath);
```

##### Copy a directory recursively

**`OS.File`**
```js
// Not easy to do.
```

**`IOUtils`**
```js
await IOUtils.copy(srcPath, destPath, { recursive: true });
```

### Iterate a directory

At the moment, `IOUtils` does not have a way to expose an iterator for directories.
This is blocked by
[bug 1577383](https://bugzilla.mozilla.org/show_bug.cgi?id=1577383).
As a stop-gap for this functionality,
one can get all the children of a directory and iterate through the returned path array using the following method.

```idl
Promise<sequence<DOMString>> getChildren(DOMString path);
```

#### Example

**`OS.File`**
```js
for await (const { path } of new OS.FileDirectoryIterator(dirName)) {</p>
  ...
}
```

**`IOUtils`**
```js
for (const path of await IOUtils.getChildren(dirName)) {
  ...
}
```

### Check if a file exists

`IOUtils` provides the following method analogous to the `OS.File` method of the same name.

```idl
Promise<boolean> exists(DOMString path);
```

#### Example

**`OS.File`**
```js
if (await OS.File.exists(path)) {
  ...
}
```

**`IOUtils`**
```js
if (await IOUtils.exists(path)) {
  ...
}
```

### Set the permissions of a file

`IOUtils` provides the following method analogous to the `OS.File` method of the same name.

```idl
Promise<void> setPermissions(DOMString path, unsigned long permissions, optional boolean honorUmask = true);
```

#### Options

| `OS.File` option        | `IOUtils` option           | Description                                                                                                                          |
| ----------------------- | -------------------------- | ------------------------------------------------------------------------------------------------------------------------------------ |
| unixMode: number        | permissions: unsigned long | The UNIX file mode representing the permissions. Required in IOUtils.                                                                |
| unixHonorUmask: boolean | honorUmask: boolean        | If omitted or true, any UNIX file mode is modified by the permissions. Otherwise the exact value of the permissions will be applied. |

#### Example

**`OS.File`**
```js
await OS.File.setPermissions(path, { unixMode: 0o600 });
```

**`IOUtils`**
```js
await IOUtils.setPermissions(path, 0o600);
```

## FAQs

**Why should I use `IOUtils` instead of `OS.File`?**

[Bug 1231711](https://bugzilla.mozilla.org/show_bug.cgi?id=1231711)
provides some good context, but some reasons include:
* reduced cache-contention,
* faster startup, and
* less memory usage.

Additionally, `IOUtils` benefits from a native implementation,
which assists in performance-related work for
[Project Fission](https://hacks.mozilla.org/2021/05/introducing-firefox-new-site-isolation-security-architecture/).

We are actively working to migrate old code usages of `OS.File`
to analogous `IOUtils` calls, so new usages of `OS.File`
should not be introduced at this time.

**Do I need to import anything to use this API?**

Nope! It's available via the `IOUtils` global in JavaScript (`ChromeOnly` context).

**Can I use this API from C++ or Rust?**

Currently usage is geared exclusively towards JavaScript callers,
and all C++ methods are private except for the Web IDL bindings.
However given sufficient interest,
it should be easy to expose ergonomic public methods for C++ and/or Rust.

**Why isn't `IOUtils` written in Rust?**

At the time of writing,
support for Web IDL bindings was more mature for C++ oriented tooling than it was for Rust.

**Is `IOUtils` feature complete? When will it be available?**

`IOUtils` is considered feature complete as of Firefox 83.
