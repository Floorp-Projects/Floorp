/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const EXPORTED_SYMBOLS = ['Log4Moz'];

const Cc = Components.classes;
const Ci = Components.interfaces;
const Cr = Components.results;
const Cu = Components.utils;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

const MODE_RDONLY   = 0x01;
const MODE_WRONLY   = 0x02;
const MODE_CREATE   = 0x08;
const MODE_APPEND   = 0x10;
const MODE_TRUNCATE = 0x20;

const PERMS_FILE      = 0644;
const PERMS_DIRECTORY = 0755;

const ONE_BYTE = 1;
const ONE_KILOBYTE = 1024 * ONE_BYTE;
const ONE_MEGABYTE = 1024 * ONE_KILOBYTE;

let Log4Moz = {
  Level: {
    Fatal:  70,
    Error:  60,
    Warn:   50,
    Info:   40,
    Config: 30,
    Debug:  20,
    Trace:  10,
    All:    0,
    Desc: {
      70: "FATAL",
      60: "ERROR",
      50: "WARN",
      40: "INFO",
      30: "CONFIG",
      20: "DEBUG",
      10: "TRACE",
      0:  "ALL"
    }
  },

  get repository() {
    delete Log4Moz.repository;
    Log4Moz.repository = new LoggerRepository();
    return Log4Moz.repository;
  },
  set repository(value) {
    delete Log4Moz.repository;
    Log4Moz.repository = value;
  },

  get LogMessage() { return LogMessage; },
  get Logger() { return Logger; },
  get LoggerRepository() { return LoggerRepository; },

  get Formatter() { return Formatter; },
  get BasicFormatter() { return BasicFormatter; },

  get Appender() { return Appender; },
  get DumpAppender() { return DumpAppender; },
  get ConsoleAppender() { return ConsoleAppender; },
  get FileAppender() { return FileAppender; },
  get RotatingFileAppender() { return RotatingFileAppender; },

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateInterfaces(someObject).join(","));
  enumerateInterfaces: function Log4Moz_enumerateInterfaces(aObject) {
    let interfaces = [];

    for (i in Ci) {
      try {
        aObject.QueryInterface(Ci[i]);
        interfaces.push(i);
      }
      catch(ex) {}
    }

    return interfaces;
  },

  // Logging helper:
  // let logger = Log4Moz.repository.getLogger("foo");
  // logger.info(Log4Moz.enumerateProperties(someObject).join(","));
  enumerateProperties: function Log4Moz_enumerateProps(aObject,
                                                       aExcludeComplexTypes) {
    let properties = [];

    for (p in aObject) {
      try {
        if (aExcludeComplexTypes &&
            (typeof aObject[p] == "object" || typeof aObject[p] == "function"))
          continue;
        properties.push(p + " = " + aObject[p]);
      }
      catch(ex) {
        properties.push(p + " = " + ex);
      }
    }

    return properties;
  }
};


/*
 * LogMessage
 * Encapsulates a single log event's data
 */
function LogMessage(loggerName, level, message){
  this.loggerName = loggerName;
  this.message = message;
  this.level = level;
  this.time = Date.now();
}
LogMessage.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  get levelDesc() {
    if (this.level in Log4Moz.Level.Desc)
      return Log4Moz.Level.Desc[this.level];
    return "UNKNOWN";
  },

  toString: function LogMsg_toString(){
    return "LogMessage [" + this.time + " " + this.level + " " +
      this.message + "]";
  }
};

/*
 * Logger
 * Hierarchical version.  Logs to all appenders, assigned or inherited
 */

function Logger(name, repository) {
  this._init(name, repository);
}
Logger.prototype = {
  _init: function Logger__init(name, repository) {
    if (!repository)
      repository = Log4Moz.repository;
    this._name = name;
    this._appenders = [];
    this._repository = repository;
  },

  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  parent: null,

  get name() {
    return this._name;
  },

  _level: null,
  get level() {
    if (this._level != null)
      return this._level;
    if (this.parent)
      return this.parent.level;
    dump("log4moz warning: root logger configuration error: no level defined\n");
    return Log4Moz.Level.All;
  },
  set level(level) {
    this._level = level;
  },

  _appenders: null,
  get appenders() {
    if (!this.parent)
      return this._appenders;
    return this._appenders.concat(this.parent.appenders);
  },

  addAppender: function Logger_addAppender(appender) {
    for (let i = 0; i < this._appenders.length; i++) {
      if (this._appenders[i] == appender)
        return;
    }
    this._appenders.push(appender);
  },

  removeAppender: function Logger_removeAppender(appender) {
    let newAppenders = [];
    for (let i = 0; i < this._appenders.length; i++) {
      if (this._appenders[i] != appender)
        newAppenders.push(this._appenders[i]);
    }
    this._appenders = newAppenders;
  },

  log: function Logger_log(message) {
    if (this.level > message.level)
      return;
    let appenders = this.appenders;
    for (let i = 0; i < appenders.length; i++){
      appenders[i].append(message);
    }
  },

  fatal: function Logger_fatal(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Fatal, string));
  },
  error: function Logger_error(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Error, string));
  },
  warn: function Logger_warn(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Warn, string));
  },
  info: function Logger_info(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Info, string));
  },
  config: function Logger_config(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Config, string));
  },
  debug: function Logger_debug(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Debug, string));
  },
  trace: function Logger_trace(string) {
    this.log(new LogMessage(this._name, Log4Moz.Level.Trace, string));
  }
};

/*
 * LoggerRepository
 * Implements a hierarchy of Loggers
 */

function LoggerRepository() {}
LoggerRepository.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  _loggers: {},

  _rootLogger: null,
  get rootLogger() {
    if (!this._rootLogger) {
      this._rootLogger = new Logger("root", this);
      this._rootLogger.level = Log4Moz.Level.All;
    }
    return this._rootLogger;
  },
  // FIXME: need to update all parent values if we do this
  //set rootLogger(logger) {
  //  this._rootLogger = logger;
  //},

  _updateParents: function LogRep__updateParents(name) {
    let pieces = name.split('.');
    let cur, parent;

    // find the closest parent
    // don't test for the logger name itself, as there's a chance it's already
    // there in this._loggers
    for (let i = 0; i < pieces.length - 1; i++) {
      if (cur)
        cur += '.' + pieces[i];
      else
        cur = pieces[i];
      if (cur in this._loggers)
        parent = cur;
    }

    // if we didn't assign a parent above, there is no parent
    if (!parent)
      this._loggers[name].parent = this.rootLogger;
    else
      this._loggers[name].parent = this._loggers[parent];

    // trigger updates for any possible descendants of this logger
    for (let logger in this._loggers) {
      if (logger != name && logger.indexOf(name) == 0)
        this._updateParents(logger);
    }
  },

  getLogger: function LogRep_getLogger(name) {
    if (!name)
      name = this.getLogger.caller.name;
    if (name in this._loggers)
      return this._loggers[name];
    this._loggers[name] = new Logger(name, this);
    this._updateParents(name);
    return this._loggers[name];
  }
};

/*
 * Formatters
 * These massage a LogMessage into whatever output is desired
 * Only the BasicFormatter is currently implemented
 */

// Abstract formatter
function Formatter() {}
Formatter.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),
  format: function Formatter_format(message) {}
};

// FIXME: should allow for formatting the whole string, not just the date
function BasicFormatter(dateFormat) {
  if (dateFormat)
    this.dateFormat = dateFormat;
}
BasicFormatter.prototype = {
  __proto__: Formatter.prototype,

  _dateFormat: null,

  get dateFormat() {
    if (!this._dateFormat)
      this._dateFormat = "%Y-%m-%d %H:%M:%S";
    return this._dateFormat;
  },

  set dateFormat(format) {
    this._dateFormat = format;
  },

  format: function BF_format(message) {
    // Pad a string to a certain length (20) with a character (space)
    let pad = function BF__pad(str, len, chr) str +
      new Array(Math.max((len || 20) - str.length + 1, 0)).join(chr || " ");

    // Generate a date string because toLocaleString doesn't work XXX 514803
    let z = function(n) n < 10 ? "0" + n : n;
    let d = new Date(message.time);
    let dateStr = [d.getFullYear(), "-", z(d.getMonth() + 1), "-",
      z(d.getDate()), " ", z(d.getHours()), ":", z(d.getMinutes()), ":",
      z(d.getSeconds())].join("");

    return dateStr + "\t" + pad(message.loggerName) + " " + message.levelDesc +
      "\t" + message.message + "\n";
  }
};

/*
 * Appenders
 * These can be attached to Loggers to log to different places
 * Simply subclass and override doAppend to implement a new one
 */

function Appender(formatter) {
  this._name = "Appender";
  this._formatter = formatter? formatter : new BasicFormatter();
}
Appender.prototype = {
  QueryInterface: XPCOMUtils.generateQI([Ci.nsISupports]),

  _level: Log4Moz.Level.All,
  get level() { return this._level; },
  set level(level) { this._level = level; },

  append: function App_append(message) {
    if(this._level <= message.level)
      this.doAppend(this._formatter.format(message));
  },
  toString: function App_toString() {
    return this._name + " [level=" + this._level +
      ", formatter=" + this._formatter + "]";
  },
  doAppend: function App_doAppend(message) {}
};

/*
 * DumpAppender
 * Logs to standard out
 */

function DumpAppender(formatter) {
  this._name = "DumpAppender";
  this._formatter = formatter? formatter : new BasicFormatter();
}
DumpAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function DApp_doAppend(message) {
    dump(message);
  }
};

/*
 * ConsoleAppender
 * Logs to the javascript console
 */

function ConsoleAppender(formatter) {
  this._name = "ConsoleAppender";
  this._formatter = formatter;
}
ConsoleAppender.prototype = {
  __proto__: Appender.prototype,

  doAppend: function CApp_doAppend(message) {
    if (message.level > Log4Moz.Level.Warn) {
      Cu.reportError(message);
      return;
    }
    Cc["@mozilla.org/consoleservice;1"].
      getService(Ci.nsIConsoleService).logStringMessage(message);
  }
};

/*
 * FileAppender
 * Logs to a file
 */

function FileAppender(file, formatter) {
  this._name = "FileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter? formatter : new BasicFormatter();
}
FileAppender.prototype = {
  __proto__: Appender.prototype,
  __fos: null,
  get _fos() {
    if (!this.__fos)
      this.openStream();
    return this.__fos;
  },

  openStream: function FApp_openStream() {
    try {
      let __fos = Cc["@mozilla.org/network/file-output-stream;1"].
        createInstance(Ci.nsIFileOutputStream);
      let flags = MODE_WRONLY | MODE_CREATE | MODE_APPEND;
      __fos.init(this._file, flags, PERMS_FILE, 0);

      this.__fos = Cc["@mozilla.org/intl/converter-output-stream;1"]
            .createInstance(Ci.nsIConverterOutputStream);
      this.__fos.init(__fos, "UTF-8", 4096,
            Ci.nsIConverterInputStream.DEFAULT_REPLACEMENT_CHARACTER);
    } catch(e) {
      dump("Error opening stream:\n" + e);
    }
  },

  closeStream: function FApp_closeStream() {
    if (!this.__fos)
      return;
    try {
      this.__fos.close();
      this.__fos = null;
    } catch(e) {
      dump("Failed to close file output stream\n" + e);
    }
  },

  doAppend: function FApp_doAppend(message) {
    if (message === null || message.length <= 0)
      return;
    try {
      this._fos.writeString(message);
    } catch(e) {
      dump("Error writing file:\n" + e);
    }
  },

  clear: function FApp_clear() {
    this.closeStream();
    try {
      this._file.remove(false);
    } catch (e) {
      // XXX do something?
    }
  }
};

/*
 * RotatingFileAppender
 * Similar to FileAppender, but rotates logs when they become too large
 */

function RotatingFileAppender(file, formatter, maxSize, maxBackups) {
  if (maxSize === undefined)
    maxSize = ONE_MEGABYTE * 2;

  if (maxBackups === undefined)
    maxBackups = 0;

  this._name = "RotatingFileAppender";
  this._file = file; // nsIFile
  this._formatter = formatter? formatter : new BasicFormatter();
  this._maxSize = maxSize;
  this._maxBackups = maxBackups;
}
RotatingFileAppender.prototype = {
  __proto__: FileAppender.prototype,

  doAppend: function RFApp_doAppend(message) {
    if (message === null || message.length <= 0)
      return;
    try {
      this.rotateLogs();
      FileAppender.prototype.doAppend.call(this, message);
    } catch(e) {
      dump("Error writing file:" + e + "\n");
    }
  },

  rotateLogs: function RFApp_rotateLogs() {
    if(this._file.exists() &&
       this._file.fileSize < this._maxSize)
      return;

    this.closeStream();

    for (let i = this.maxBackups - 1; i > 0; i--){
      let backup = this._file.parent.clone();
      backup.append(this._file.leafName + "." + i);
      if (backup.exists())
        backup.moveTo(this._file.parent, this._file.leafName + "." + (i + 1));
    }

    let cur = this._file.clone();
    if (cur.exists())
      cur.moveTo(cur.parent, cur.leafName + ".1");

    // Note: this._file still points to the same file
  }
};
