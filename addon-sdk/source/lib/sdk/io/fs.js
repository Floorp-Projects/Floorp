/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
"use strict";

module.metadata = {
  "stability": "experimental"
};

const { Cc, Ci, CC } = require("chrome");

const { setTimeout } = require("../timers");
const { Stream, InputStream, OutputStream } = require("./stream");
const { emit, on } = require("../event/core");
const { Buffer } = require("./buffer");
const { ns } = require("../core/namespace");
const { Class } = require("../core/heritage");


const nsILocalFile = CC("@mozilla.org/file/local;1", "nsILocalFile",
                        "initWithPath");
const FileOutputStream = CC("@mozilla.org/network/file-output-stream;1",
                            "nsIFileOutputStream", "init");
const FileInputStream = CC("@mozilla.org/network/file-input-stream;1",
                           "nsIFileInputStream", "init");
const BinaryInputStream = CC("@mozilla.org/binaryinputstream;1",
                             "nsIBinaryInputStream", "setInputStream");
const BinaryOutputStream = CC("@mozilla.org/binaryoutputstream;1",
                              "nsIBinaryOutputStream", "setOutputStream");
const StreamPump = CC("@mozilla.org/network/input-stream-pump;1",
                      "nsIInputStreamPump", "init");

const { createOutputTransport, createInputTransport } =
  Cc["@mozilla.org/network/stream-transport-service;1"].
  getService(Ci.nsIStreamTransportService);

const { OPEN_UNBUFFERED } = Ci.nsITransport;


const { REOPEN_ON_REWIND, DEFER_OPEN } = Ci.nsIFileInputStream;
const { DIRECTORY_TYPE, NORMAL_FILE_TYPE } = Ci.nsIFile;
const { NS_SEEK_SET, NS_SEEK_CUR, NS_SEEK_END } = Ci.nsISeekableStream;

const FILE_PERMISSION = parseInt("0666", 8);
const PR_UINT32_MAX = 0xfffffff;
// Values taken from:
// http://mxr.mozilla.org/mozilla-central/source/nsprpub/pr/include/prio.h#615
const PR_RDONLY =       0x01;
const PR_WRONLY =       0x02;
const PR_RDWR =         0x04;
const PR_CREATE_FILE =  0x08;
const PR_APPEND =       0x10;
const PR_TRUNCATE =     0x20;
const PR_SYNC =         0x40;
const PR_EXCL =         0x80;

const FLAGS = {
  "r":                  PR_RDONLY,
  "r+":                 PR_RDWR,
  "w":                  PR_CREATE_FILE | PR_TRUNCATE | PR_WRONLY,
  "w+":                 PR_CREATE_FILE | PR_TRUNCATE | PR_RDWR,
  "a":                  PR_APPEND | PR_CREATE_FILE | PR_WRONLY,
  "a+":                 PR_APPEND | PR_CREATE_FILE | PR_RDWR
};

function accessor() {
  let map = new WeakMap();
  return function(fd, value) {
    if (value === null) map.delete(fd);
    if (value !== undefined) map.set(fd, value);
    return map.get(fd);
  }
}

let nsIFile = accessor();
let nsIFileInputStream = accessor();
let nsIFileOutputStream = accessor();
let nsIBinaryInputStream = accessor();
let nsIBinaryOutputStream = accessor();

// Just a contstant object used to signal that all of the file
// needs to be read.
const ALL = new String("Read all of the file");

function isWritable(mode) !!(mode & PR_WRONLY || mode & PR_RDWR)
function isReadable(mode) !!(mode & PR_RDONLY || mode & PR_RDWR)

function isString(value) typeof(value) === "string"
function isFunction(value) typeof(value) === "function"

function toArray(enumerator) {
  let value = [];
  while(enumerator.hasMoreElements())
    value.push(enumerator.getNext())
  return value
}

function getFileName(file) file.QueryInterface(Ci.nsIFile).leafName


function remove(path, recursive) {
  let fd = new nsILocalFile(path)
  if (fd.exists()) {
    fd.remove(recursive || false);
  }
  else {
    throw FSError("remove", "ENOENT", 34, path);
  }
}

/**
 * Utility function to convert either an octal number or string
 * into an octal number
 * 0777 => 0777
 * "0644" => 0644
 */
function Mode(mode, fallback) {
  return isString(mode) ? parseInt(mode, 8) : mode || fallback;
}
function Flags(flag) {
  return !isString(flag) ? flag :
         FLAGS[flag] || Error("Unknown file open flag: " + flag);
}


function FSError(op, code, errno, path, file, line) {
  let error = Error(code + ", " + op + " " + path, file, line);
  error.code = code;
  error.path = path;
  error.errno = errno;
  return error;
}

const ReadStream = Class({
  extends: InputStream,
  initialize: function initialize(path, options) {
    this.position = -1;
    this.length = -1;
    this.flags = "r";
    this.mode = FILE_PERMISSION;
    this.bufferSize = 64 * 1024;

    options = options || {};

    if ("flags" in options && options.flags)
      this.flags = options.flags;
    if ("bufferSize" in options && options.bufferSize)
      this.bufferSize = options.bufferSize;
    if ("length" in options && options.length)
      this.length = options.length;
    if ("position" in options && options.position !== undefined)
      this.position = options.position;

    let { flags, mode, position, length } = this;
    let fd = isString(path) ? openSync(path, flags, mode) : path;
    this.fd = fd;

    let input = nsIFileInputStream(fd);
    // Setting a stream position, unless it"s `-1` which means current position.
    if (position >= 0)
      input.QueryInterface(Ci.nsISeekableStream).seek(NS_SEEK_SET, position);
    // We use `nsIStreamTransportService` service to transform blocking
    // file input stream into a fully asynchronous stream that can be written
    // without blocking the main thread.
    let transport = createInputTransport(input, position, length, false);
    // Open an input stream on a transport. We don"t pass flags to guarantee
    // non-blocking stream semantics. Also we use defaults for segment size &
    // count.
    InputStream.prototype.initialize.call(this, {
      asyncInputStream: transport.openInputStream(null, 0, 0)
    });

    // Close file descriptor on end and destroy the stream.
    on(this, "end", _ => {
      this.destroy();
      emit(this, "close");
    });

    this.read();
  },
  destroy: function() {
    closeSync(this.fd);
    InputStream.prototype.destroy.call(this);
  }
});
exports.ReadStream = ReadStream;
exports.createReadStream = function createReadStream(path, options) {
  return new ReadStream(path, options);
};

const WriteStream = Class({
  extends: OutputStream,
  initialize: function initialize(path, options) {
    this.drainable = true;
    this.flags = "w";
    this.position = -1;
    this.mode = FILE_PERMISSION;

    options = options || {};

    if ("flags" in options && options.flags)
      this.flags = options.flags;
    if ("mode" in options && options.mode)
      this.mode = options.mode;
    if ("position" in options && options.position !== undefined)
      this.position = options.position;

    let { position, flags, mode } = this;
    // If pass was passed we create a file descriptor out of it. Otherwise
    // we just use given file descriptor.
    let fd = isString(path) ? openSync(path, flags, mode) : path;
    this.fd = fd;

    let output = nsIFileOutputStream(fd);
    // Setting a stream position, unless it"s `-1` which means current position.
    if (position >= 0)
      output.QueryInterface(Ci.nsISeekableStream).seek(NS_SEEK_SET, position);
    // We use `nsIStreamTransportService` service to transform blocking
    // file output stream into a fully asynchronous stream that can be written
    // without blocking the main thread.
    let transport = createOutputTransport(output, position, -1, false);
    // Open an output stream on a transport. We don"t pass flags to guarantee
    // non-blocking stream semantics. Also we use defaults for segment size &
    // count.
    OutputStream.prototype.initialize.call(this, {
      asyncOutputStream: transport.openOutputStream(OPEN_UNBUFFERED, 0, 0),
      output: output
    });

    // For write streams "finish" basically means close.
    on(this, "finish", _ => {
       this.destroy();
       emit(this, "close");
    });
  },
  destroy: function() {
    OutputStream.prototype.destroy.call(this);
    closeSync(this.fd);
  }
});
exports.WriteStream = WriteStream;
exports.createWriteStream = function createWriteStream(path, options) {
  return new WriteStream(path, options);
};

const Stats = Class({
  initialize: function initialize(path) {
    let file = new nsILocalFile(path);
    if (!file.exists()) throw FSError("stat", "ENOENT", 34, path);
    nsIFile(this, file);
  },
  isDirectory: function() nsIFile(this).isDirectory(),
  isFile: function() nsIFile(this).isFile(),
  isSymbolicLink: function() nsIFile(this).isSymlink(),
  get mode() nsIFile(this).permissions,
  get size() nsIFile(this).fileSize,
  get mtime() nsIFile(this).lastModifiedTime,
  isBlockDevice: function() nsIFile(this).isSpecial(),
  isCharacterDevice: function() nsIFile(this).isSpecial(),
  isFIFO: function() nsIFile(this).isSpecial(),
  isSocket: function() nsIFile(this).isSpecial(),
  // non standard
  get exists() nsIFile(this).exists(),
  get hidden() nsIFile(this).isHidden(),
  get writable() nsIFile(this).isWritable(),
  get readable() nsIFile(this).isReadable()
});
exports.Stats = Stats;

const LStats = Class({
  extends: Stats,
  get size() this.isSymbolicLink() ? nsIFile(this).fileSizeOfLink :
                                     nsIFile(this).fileSize,
  get mtime() this.isSymbolicLink() ? nsIFile(this).lastModifiedTimeOfLink :
                                      nsIFile(this).lastModifiedTime,
  // non standard
  get permissions() this.isSymbolicLink() ? nsIFile(this).permissionsOfLink :
                                            nsIFile(this).permissions
});

const FStat = Class({
  extends: Stats,
  initialize: function initialize(fd) {
    nsIFile(this, nsIFile(fd));
  }
});

function noop() {}
function Async(wrapped) {
  return function (path, callback) {
    let args = Array.slice(arguments);
    callback = args.pop();
    // If node is not given a callback argument
    // it just does not calls it.
    if (typeof(callback) !== "function") {
      args.push(callback);
      callback = noop;
    }
    setTimeout(function() {
      try {
        var result = wrapped.apply(this, args);
        if (result === undefined) callback(null);
        else callback(null, result);
      } catch (error) {
        callback(error);
      }
    }, 0);
  }
}


/**
 * Synchronous rename(2)
 */
function renameSync(oldPath, newPath) {
  let source = new nsILocalFile(oldPath);
  let target = new nsILocalFile(newPath);
  if (!source.exists()) throw FSError("rename", "ENOENT", 34, oldPath);
  return source.moveTo(target.parent, target.leafName);
};
exports.renameSync = renameSync;

/**
 * Asynchronous rename(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let rename = Async(renameSync);
exports.rename = rename;

/**
 * Test whether or not the given path exists by checking with the file system.
 */
function existsSync(path) {
  return new nsILocalFile(path).exists();
}
exports.existsSync = existsSync;

let exists = Async(existsSync);
exports.exists = exists;

/**
 * Synchronous ftruncate(2).
 */
function truncateSync(path, length) {
  let fd = openSync(path, "w");
  ftruncateSync(fd, length);
  closeSync(fd);
}
exports.truncateSync = truncateSync;

/**
 * Asynchronous ftruncate(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
function truncate(path, length, callback) {
  open(path, "w", function(error, fd) {
    if (error) return callback(error);
    ftruncate(fd, length, function(error) {
      if (error) {
        closeSync(fd);
        callback(error);
      }
      else {
        close(fd, callback);
      }
    });
  });
}
exports.truncate = truncate;

function ftruncate(fd, length, callback) {
  write(fd, new Buffer(length), 0, length, 0, function(error) {
    callback(error);
  });
}
exports.ftruncate = ftruncate;

function ftruncateSync(fd, length = 0) {
  writeSync(fd, new Buffer(length), 0, length, 0);
}
exports.ftruncateSync = ftruncateSync;

function chownSync(path, uid, gid) {
  throw Error("Not implemented yet!!");
}
exports.chownSync = chownSync;

let chown = Async(chownSync);
exports.chown = chown;

function lchownSync(path, uid, gid) {
  throw Error("Not implemented yet!!");
}
exports.lchownSync = chownSync;

let lchown = Async(lchown);
exports.lchown = lchown;

/**
 * Synchronous chmod(2).
 */
function chmodSync (path, mode) {
  let file;
  try {
    file = new nsILocalFile(path);
  } catch(e) {
    throw FSError("chmod", "ENOENT", 34, path);
  }

  file.permissions = Mode(mode);
}
exports.chmodSync = chmodSync;
/**
 * Asynchronous chmod(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let chmod = Async(chmodSync);
exports.chmod = chmod;

/**
 * Synchronous chmod(2).
 */
function fchmodSync(fd, mode) {
  throw Error("Not implemented yet!!");
};
exports.fchmodSync = fchmodSync;
/**
 * Asynchronous chmod(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let fchmod = Async(fchmodSync);
exports.fchmod = fchmod;


/**
 * Synchronous stat(2). Returns an instance of `fs.Stats`
 */
function statSync(path) {
  return new Stats(path);
};
exports.statSync = statSync;

/**
 * Asynchronous stat(2). The callback gets two arguments (err, stats) where
 * stats is a `fs.Stats` object. It looks like this:
 */
let stat = Async(statSync);
exports.stat = stat;

/**
 * Synchronous lstat(2). Returns an instance of `fs.Stats`.
 */
function lstatSync(path) {
  return new LStats(path);
};
exports.lstatSync = lstatSync;

/**
 * Asynchronous lstat(2). The callback gets two arguments (err, stats) where
 * stats is a fs.Stats object. lstat() is identical to stat(), except that if
 * path is a symbolic link, then the link itself is stat-ed, not the file that
 * it refers to.
 */
let lstat = Async(lstatSync);
exports.lstat = lstat;

/**
 * Synchronous fstat(2). Returns an instance of `fs.Stats`.
 */
function fstatSync(fd) {
  return new FStat(fd);
};
exports.fstatSync = fstatSync;

/**
 * Asynchronous fstat(2). The callback gets two arguments (err, stats) where
 * stats is a fs.Stats object.
 */
let fstat = Async(fstatSync);
exports.fstat = fstat;

/**
 * Synchronous link(2).
 */
function linkSync(source, target) {
  throw Error("Not implemented yet!!");
};
exports.linkSync = linkSync;

/**
 * Asynchronous link(2). No arguments other than a possible exception are given
 * to the completion callback.
 */
let link = Async(linkSync);
exports.link = link;

/**
 * Synchronous symlink(2).
 */
function symlinkSync(source, target) {
  throw Error("Not implemented yet!!");
};
exports.symlinkSync = symlinkSync;

/**
 * Asynchronous symlink(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let symlink = Async(symlinkSync);
exports.symlink = symlink;

/**
 * Synchronous readlink(2). Returns the resolved path.
 */
function readlinkSync(path) {
  return new nsILocalFile(path).target;
};
exports.readlinkSync = readlinkSync;

/**
 * Asynchronous readlink(2). The callback gets two arguments
 * `(error, resolvedPath)`.
 */
let readlink = Async(readlinkSync);
exports.readlink = readlink;

/**
 * Synchronous realpath(2). Returns the resolved path.
 */
function realpathSync(path) {
  return new nsILocalFile(path).path;
};
exports.realpathSync = realpathSync;

/**
 * Asynchronous realpath(2). The callback gets two arguments
 * `(err, resolvedPath)`.
 */
let realpath = Async(realpathSync);
exports.realpath = realpath;

/**
 * Synchronous unlink(2).
 */
let unlinkSync = remove;
exports.unlinkSync = unlinkSync;

/**
 * Asynchronous unlink(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let unlink = Async(remove);
exports.unlink = unlink;

/**
 * Synchronous rmdir(2).
 */
let rmdirSync = remove;
exports.rmdirSync = rmdirSync;

/**
 * Asynchronous rmdir(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let rmdir = Async(rmdirSync);
exports.rmdir = rmdir;

/**
 * Synchronous mkdir(2).
 */
function mkdirSync(path, mode) {
  try {
    return nsILocalFile(path).create(DIRECTORY_TYPE, Mode(mode));
  } catch (error) {
    // Adjust exception thorw to match ones thrown by node.
    if (error.name === "NS_ERROR_FILE_ALREADY_EXISTS") {
      let { fileName, lineNumber } = error;
      error = FSError("mkdir", "EEXIST", 47, path, fileName, lineNumber);
    }
    throw error;
  }
};
exports.mkdirSync = mkdirSync;

/**
 * Asynchronous mkdir(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let mkdir = Async(mkdirSync);
exports.mkdir = mkdir;

/**
 * Synchronous readdir(3). Returns an array of filenames excluding `"."` and
 * `".."`.
 */
function readdirSync(path) {
  try {
    return toArray(new nsILocalFile(path).directoryEntries).map(getFileName);
  }
  catch (error) {
    // Adjust exception thorw to match ones thrown by node.
    if (error.name === "NS_ERROR_FILE_TARGET_DOES_NOT_EXIST" ||
        error.name === "NS_ERROR_FILE_NOT_FOUND")
    {
      let { fileName, lineNumber } = error;
      error = FSError("readdir", "ENOENT", 34, path, fileName, lineNumber);
    }
    throw error;
  }
};
exports.readdirSync = readdirSync;

/**
 * Asynchronous readdir(3). Reads the contents of a directory. The callback
 * gets two arguments `(error, files)` where `files` is an array of the names
 * of the files in the directory excluding `"."` and `".."`.
 */
let readdir = Async(readdirSync);
exports.readdir = readdir;

/**
 * Synchronous close(2).
 */
 function closeSync(fd) {
   let input = nsIFileInputStream(fd);
   let output = nsIFileOutputStream(fd);

   // Closing input stream and removing reference.
   if (input) input.close();
   // Closing output stream and removing reference.
   if (output) output.close();

   nsIFile(fd, null);
   nsIFileInputStream(fd, null);
   nsIFileOutputStream(fd, null);
   nsIBinaryInputStream(fd, null);
   nsIBinaryOutputStream(fd, null);
};
exports.closeSync = closeSync;
/**
 * Asynchronous close(2). No arguments other than a possible exception are
 * given to the completion callback.
 */
let close = Async(closeSync);
exports.close = close;

/**
 * Synchronous open(2).
 */
function openSync(path, flags, mode) {
  let [ fd, flags, mode, file ] =
      [ { path: path }, Flags(flags), Mode(mode), nsILocalFile(path) ];

  nsIFile(fd, file);

  // If trying to open file for just read that does not exists
  // need to throw exception as node does.
  if (!file.exists() && !isWritable(flags))
    throw FSError("open", "ENOENT", 34, path);

  // If we want to open file in read mode we initialize input stream.
  if (isReadable(flags)) {
    let input = FileInputStream(file, flags, mode, DEFER_OPEN);
    nsIFileInputStream(fd, input);
  }

  // If we want to open file in write mode we initialize output stream for it.
  if (isWritable(flags)) {
    let output = FileOutputStream(file, flags, mode, DEFER_OPEN);
    nsIFileOutputStream(fd, output);
  }

  return fd;
}
exports.openSync = openSync;
/**
 * Asynchronous file open. See open(2). Flags can be
 * `"r", "r+", "w", "w+", "a"`, or `"a+"`. mode defaults to `0666`.
 * The callback gets two arguments `(error, fd).
 */
let open = Async(openSync);
exports.open = open;

/**
 * Synchronous version of buffer-based fs.write(). Returns the number of bytes
 * written.
 */
function writeSync(fd, buffer, offset, length, position) {
  if (length + offset > buffer.length) {
    throw Error("Length is extends beyond buffer");
  }
  else if (length + offset !== buffer.length) {
    buffer = buffer.slice(offset, offset + length);
  }
  let writeStream = new WriteStream(fd, { position: position,
                                          length: length });

  let output = BinaryOutputStream(nsIFileOutputStream(fd));
  nsIBinaryOutputStream(fd, output);
  // We write content as a byte array as this will avoid any transcoding
  // if content was a buffer.
  output.writeByteArray(buffer.valueOf(), buffer.length);
  output.flush();
};
exports.writeSync = writeSync;

/**
 * Write buffer to the file specified by fd.
 *
 * `offset` and `length` determine the part of the buffer to be written.
 *
 * `position` refers to the offset from the beginning of the file where this
 * data should be written. If `position` is `null`, the data will be written
 * at the current position. See pwrite(2).
 *
 * The callback will be given three arguments `(error, written, buffer)` where
 * written specifies how many bytes were written into buffer.
 *
 * Note that it is unsafe to use `fs.write` multiple times on the same file
 * without waiting for the callback.
 */
function write(fd, buffer, offset, length, position, callback) {
  if (!Buffer.isBuffer(buffer)) {
    // (fd, data, position, encoding, callback)
    let encoding = null;
    [ position, encoding, callback ] = Array.slice(arguments, 1);
    buffer = new Buffer(String(buffer), encoding);
    offset = 0;
  } else if (length + offset > buffer.length) {
    throw Error("Length is extends beyond buffer");
  } else if (length + offset !== buffer.length) {
    buffer = buffer.slice(offset, offset + length);
  }

  let writeStream = new WriteStream(fd, { position: position,
                                          length: length });
  writeStream.on("error", callback);
  writeStream.write(buffer, function onEnd() {
    writeStream.destroy();
    if (callback)
      callback(null, buffer.length, buffer);
  });
};
exports.write = write;

/**
 * Synchronous version of string-based fs.read. Returns the number of
 * bytes read.
 */
function readSync(fd, buffer, offset, length, position) {
  let input = nsIFileInputStream(fd);
  // Setting a stream position, unless it"s `-1` which means current position.
  if (position >= 0)
    input.QueryInterface(Ci.nsISeekableStream).seek(NS_SEEK_SET, position);
  // We use `nsIStreamTransportService` service to transform blocking
  // file input stream into a fully asynchronous stream that can be written
  // without blocking the main thread.
  let binaryInputStream = BinaryInputStream(input);
  let count = length === ALL ? binaryInputStream.available() : length;
  if (offset === 0) binaryInputStream.readArrayBuffer(count, buffer.buffer);
  else {
    let chunk = new Buffer(count);
    binaryInputStream.readArrayBuffer(count, chunk.buffer);
    chunk.copy(buffer, offset);
  }

  return buffer.slice(offset, offset + count);
};
exports.readSync = readSync;

/**
 * Read data from the file specified by `fd`.
 *
 * `buffer` is the buffer that the data will be written to.
 * `offset` is offset within the buffer where writing will start.
 *
 * `length` is an integer specifying the number of bytes to read.
 *
 * `position` is an integer specifying where to begin reading from in the file.
 * If `position` is `null`, data will be read from the current file position.
 *
 * The callback is given the three arguments, `(error, bytesRead, buffer)`.
 */
function read(fd, buffer, offset, length, position, callback) {
  let bytesRead = 0;
  let readStream = new ReadStream(fd, { position: position, length: length });
  readStream.on("data", function onData(data) {
      data.copy(buffer, offset + bytesRead);
      bytesRead += data.length;
  });
  readStream.on("end", function onEnd() {
    callback(null, bytesRead, buffer);
    readStream.destroy();
  });
};
exports.read = read;

/**
 * Asynchronously reads the entire contents of a file.
 * The callback is passed two arguments `(error, data)`, where data is the
 * contents of the file.
 */
function readFile(path, encoding, callback) {
  if (isFunction(encoding)) {
    callback = encoding
    encoding = null
  }

  let buffer = null;
  try {
    let readStream = new ReadStream(path);
    readStream.on("data", function(data) {
      if (!buffer) buffer = data;
      else buffer = Buffer.concat([buffer, data], 2);
    });
    readStream.on("error", function onError(error) {
      callback(error);
    });
    readStream.on("end", function onEnd() {
      // Note: Need to destroy before invoking a callback
      // so that file descriptor is released.
      readStream.destroy();
      callback(null, buffer);
    });
  } catch (error) {
    setTimeout(callback, 0, error);
  }
};
exports.readFile = readFile;

/**
 * Synchronous version of `fs.readFile`. Returns the contents of the path.
 * If encoding is specified then this function returns a string.
 * Otherwise it returns a buffer.
 */
function readFileSync(path, encoding) {
  let fd = openSync(path, "r");
  let size = fstatSync(fd).size;
  let buffer = new Buffer(size);
  try {
    readSync(fd, buffer, 0, ALL, 0);
  }
  finally {
    closeSync(fd);
  }
  return buffer;
};
exports.readFileSync = readFileSync;

/**
 * Asynchronously writes data to a file, replacing the file if it already
 * exists. data can be a string or a buffer.
 */
function writeFile(path, content, encoding, callback) {
  if (!isString(path))
    throw new TypeError('path must be a string');

  try {
    if (isFunction(encoding)) {
      callback = encoding
      encoding = null
    }
    if (isString(content))
      content = new Buffer(content, encoding);

    let writeStream = new WriteStream(path);
    let error = null;

    writeStream.end(content, function() {
      writeStream.destroy();
      callback(error);
    });

    writeStream.on("error", function onError(reason) {
      error = reason;
      writeStream.destroy();
    });
  } catch (error) {
    callback(error);
  }
};
exports.writeFile = writeFile;

/**
 * The synchronous version of `fs.writeFile`.
 */
function writeFileSync(filename, data, encoding) {
  throw Error("Not implemented");
};
exports.writeFileSync = writeFileSync;


function utimesSync(path, atime, mtime) {
  throw Error("Not implemented");
}
exports.utimesSync = utimesSync;

let utimes = Async(utimesSync);
exports.utimes = utimes;

function futimesSync(fd, atime, mtime, callback) {
  throw Error("Not implemented");
}
exports.futimesSync = futimesSync;

let futimes = Async(futimesSync);
exports.futimes = futimes;

function fsyncSync(fd, atime, mtime, callback) {
  throw Error("Not implemented");
}
exports.fsyncSync = fsyncSync;

let fsync = Async(fsyncSync);
exports.fsync = fsync;


/**
 * Watch for changes on filename. The callback listener will be called each
 * time the file is accessed.
 *
 * The second argument is optional. The options if provided should be an object
 * containing two members a boolean, persistent, and interval, a polling value
 * in milliseconds. The default is { persistent: true, interval: 0 }.
 */
function watchFile(path, options, listener) {
  throw Error("Not implemented");
};
exports.watchFile = watchFile;


function unwatchFile(path, listener) {
  throw Error("Not implemented");
}
exports.unwatchFile = unwatchFile;

function watch(path, options, listener) {
  throw Error("Not implemented");
}
exports.watch = watch;
