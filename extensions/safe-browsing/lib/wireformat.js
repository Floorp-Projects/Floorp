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
 * The Original Code is Google Safe Browsing.
 *
 * The Initial Developer of the Original Code is Google Inc.
 * Portions created by the Initial Developer are Copyright (C) 2006
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Niels Provos <niels@google.com> (original author)
 *   Fritz Schneider <fritz@google.com>
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


// A class that serializes and deserializes opaque key/value string to
// string maps to/from maps (trtables). It knows how to create
// trtables from the serialized format, so it also understands
// meta-information like the name of the table and the table's
// version. See docs for the protocol description.
// 
// TODO: wireformatreader: if you have multiple updates for one table
//       in a call to deserialize, the later ones will be merged 
//       (all but the last will be ignored). To fix, merge instead
//       of replace when you have an existing table, and only do so once.
// TODO must have blank line between successive types -- problem?
// TODO doesn't tolerate blank lines very well
//
// Maybe: These classes could use a LOT more cleanup, but it's not a
//       priority at the moment. For example, the tablesData/Known
//       maps should be combined into a single object, the parser
//       for a given type should be separate from the version info,
//       and there should be synchronous interfaces for testing.


/**
 * A class that knows how to serialize and deserialize meta-information.
 * This meta information is the table name and version number, and 
 * in its serialized form looks like the first line below:
 * 
 * [name-of-table X.Y update?]                
 * ...key/value pairs to add or delete follow...
 * <blank line ends the table>
 *
 * The X.Y is the version number and the optional "update" token means 
 * that the table is a differential from the curent table the extension
 * has. Its absence means that this is a full, new table.
 */
function PROT_VersionParser(type, opt_major, opt_minor) {
  this.debugZone = "versionparser";
  this.type = type;
  this.major = 0;
  this.minor = 0;
  this.update = false;
  if (opt_major)
    this.major = parseInt(opt_major);
  if (opt_minor)
    this.minor = parseInt(opt_minor);
}

/** 
 * Creates a string like [goog-white-black 1.1] from internal information
 * 
 * @returns version string
 */
PROT_VersionParser.prototype.toString = function() {
  return "[" + this.type + " " + this.major + "." + this.minor + "]";
}

/** 
 * Creates a string like 1:1 from internal information used for
 * fetching updates from the server. Called by the listmanager.
 * 
 * @returns version string
 */
PROT_VersionParser.prototype.toUrl = function() {
  return this.major + ":" + this.minor;
}

/** 
 * Takes a string like [name-of-table 1.1] and figures out the type
 * and corresponding version numbers.
 * 
 * @returns true if the string could be parsed, false otherwise
 */
PROT_VersionParser.prototype.fromString = function(line) {
  if (line[0] != '[' || line.slice(-1) != ']')
    return false;

  var description = line.slice(1, -1);

  // Get the type name and version number of this table
  var tokens = description.split(" ");
  this.type = tokens[0];
  var majorminor = tokens[1].split(".");
  this.major = parseInt(majorminor[0]);
  this.minor = parseInt(majorminor[1]);
  if (tokens.length >= 3) {
    this.update = tokens[2] == "update";
  }

  return true;
}


/**
 * A WireFormatWriter can serialize table data
 *
 * @param threadQueue A thread queue we should run on
 *
 * @constructor
 */
function PROT_WireFormatWriter(threadQueue) {
  this.threadQueue_ = threadQueue;
  this.debugZone = "wireformatwriter";
}

/**
 * Serializes a table to a string.
 *
 * @param tableData Reference to the table data we should serialize
 *
 * @param vParser Reference to the version parser/unparser we should use
 *
 * @param callback Reference to a function we should call when complete.
 *                 The callback will be invoked with a string holding 
 *                 the serialized data as an argument
 *
 * @returns False if the serializer is busy, else true
 *
 */
PROT_WireFormatWriter.prototype.serialize = function(tableData,
                                                     vParser, 
                                                     callback) {

  if (this.callback_) {
    G_Debug(this, "Serializer busy");
    return false;
  }

  this.callback_ = callback;
  this.current_ = 0;
  this.serialized_ = vParser.toString() + "\n";
  this.keyValList_ = tableData.getList();

  this.threadQueue_.addWorker(BindToObject(this.doWorkUnit, this),
                              BindToObject(this.isComplete, this));
  this.threadQueue_.run();

  return true;
}

/**
 * Serialize a chunk of data. This is periodically invoked by the
 * threadqueue.
 */
PROT_WireFormatWriter.prototype.doWorkUnit = function() {
  for (var i = 0; i < 10 && this.current_ < this.keyValList_.length; i++) {
    if (this.keyValList_[this.current_]) {
      this.serialized_ += "+" + 
                          this.keyValList_[this.current_].key + "\t" + 
                          this.keyValList_[this.current_].value + "\n";
    }
    this.current_++;
  }

  if (this.isComplete()) 
    this.onComplete();
}

/**
 * Are we done serializing? Called by the threadqueue to tell when
 * to stop running this thread.
 *
 * @returns Boolean indicating if we're done serializing
 */
PROT_WireFormatWriter.prototype.isComplete = function() {
  return this.current_ >= this.keyValList_.length;
}

/**
 * When we're done, call the callback
 */
PROT_WireFormatWriter.prototype.onComplete = function() {
  this.callback_(this.serialized_);
  this.callback_ = null;
}

/**
 * A WireFormatReader can deserialize data.
 *
 * @constructor
 *
 * @param threadQueue A global thread queue that we use to schedule our
 *        work so that we do not take up too much time at one.
 *
 * @param opt_existingTablesData Optional reference to a map of tables
 *         into which we should merge the data being deserialized 
 */
function PROT_WireFormatReader(threadQueue, opt_existingTablesData) {
  this.debugZone = "wireformatreader";
  this.existingTablesData_ = opt_existingTablesData;
  this.threadQueue_ = threadQueue;
  this.callback_ = null;
}

/**
 * Starts a thread to process the input data.
 *
 * @param tableUpdate A string that contains the data for updating the
 *        tables that we know about.
 *
 * @param callback A user specificed callback that is being executed
 *        when data processing completes.  The callback is provided
 *        with two arguments: tablesKnown and tablesData
 *
 * @returns true is new data is being process, or false on failure.
 */
PROT_WireFormatReader.prototype.deserialize = function(tableUpdate, callback) {
  this.tableUpdate_ = tableUpdate;
  this.tablesKnown_ = {}
  this.tablesData_ = {}

  if (this.callback_ != null) {
    G_Debug(this, "previous deserialize is still running");
    return false;
  }

  this.callback_ = callback;
  this.offset_ = 0;
  this.resetTableState();

  // On empty data, we just invoke the callback directly
  if (!this.tableUpdate_ || !this.tableUpdate_.length) {
    G_Debug(this, "No data. Calling back.");
    this.onComplete();
    return true;
  }

  this.threadQueue_.addWorker(BindToObject(this.processLine, this),
                              BindToObject(this.isComplete, this));
  this.threadQueue_.run();

  return true;
}

/**
 * Resets the per table state
 */
PROT_WireFormatReader.prototype.resetTableState = function() {
  this.vParser_ = null;
  this.insert_ = 0;
  this.remove_ = 0;
}

/**
 * Initalizes our state to process a new table.
 * NOTE: For performance reasons, we might have drive the replacement of
 * the table via a thread.
 *
 * @param line The input line that contains the table name and version
 */
PROT_WireFormatReader.prototype.processNewTable = function(line) {
  this.vParser_ = new PROT_VersionParser("something");
  this.vParser_.fromString(line);

  G_Debug(this, this.vParser_.type + ": " +
          this.vParser_.major + ":" + this.vParser_.minor + " " +
          this.vParser_.update);
       
  // Create temporary table
  this.tablesData_[this.vParser_.type] = new PROT_TRTable(this.vParser_.type);
  if (this.vParser_.update && this.existingTablesData_ && 
      this.existingTablesData_[this.vParser_.type]) {
    // If we update an existing table, we need to copy the old table
    // data from the pre-existing tables
    this.tablesData_[this.vParser_.type].replace(
        this.existingTablesData_[this.vParser_.type]);
  }
}

/**
 * Updates the contents of our temporary table. Exceptions should not 
 * be masked here -- they're caught at a higher level.
 *
 * @param line The input line that contains the new data
 */
PROT_WireFormatReader.prototype.processUpdateTable = function(line) {
  // Regular update to the current version
  var tokens = line.split("\t");
  var operation = tokens[0][0];
  var key = tokens[0].slice(1);
  var value = tokens[1];

  if (operation == "+") {
    this.tablesData_[this.vParser_.type].insert(key, value);
    this.insert_++;
  } else {
    this.tablesData_[this.vParser_.type].erase(key);
    this.remove_++;
  }
}

/**
 * Processes a chunk of data.
 */
PROT_WireFormatReader.prototype.processLine = function() {
  for (var count = 0;
       count < 5 && this.offset_ < this.tableUpdate_.length; count++) {
    var newOffset = this.tableUpdate_.indexOf("\n", this.offset_);
    var line = "";
    if (newOffset == -1) {
      this.offset_ = this.tableUpdate_.length;
    } else {
      line = this.tableUpdate_.slice(this.offset_, newOffset);
      this.offset_ = newOffset + 1;
    }

    // Ignore empty lines if we currently do not have a table
    if (!this.vParser_ && (!line || line[0] != '['))
      continue;

    if (!line) {
      // End of one table - pop the results
      G_Debug(this,
              "Finished: " + this.vParser_.type + " +" + 
              this.insert_ + " -" + this.remove_);
      
      this.tablesKnown_[this.vParser_.type] = this.vParser_;

      this.resetTableState();
    } else if (line[0] == '[' && line.slice(-1) == ']') {
      // TODO: Should probably handle malformed table headers
      this.processNewTable(line);
    } else {
      if (!this.vParser_) {
        G_Debug(this, "Ignoring: " + line);
        continue;
      }

      // Now we try to read a data line. However the table could've
      // been corrupted on disk (e.g., the browser suddenly quit while
      // we were in the middle of writing a line). If so,
      // porcessUpdateTable() will throw. In this case we want to
      // resynch the whole table, so we skip this line and then
      // explicitly set the table's minor version to -1 (the lowest
      // possible -- not 0, which means the table is local), causing a
      // full update the next time we ask for it.
      //
      // We ignore the case where we wrote an incomplete but malformed
      // table -- it fixes itself over time as the missing keys become
      // less relevant.
      
      try {
        this.processUpdateTable(line);
      } catch(e) {
        G_Debug(this, "MALFORMED TABLE LINE: [" + line + "]\n" +
                "Skipping this line, and resetting table " + 
                this.vParser_.type + " to version -1.\n" +
                "(This as a result of exception: " + e + ")");
        this.vParser_.minor = "-1";
      }
    }
  }

  // If the table we're reading is the last, then the for loop will
  // fail, causing the table finish logic to be skipped. So here 
  // ensure that we finish up whatever table we're working on.

  if (this.vParser_ && this.offset_ >= this.tableUpdate_.length) {
    G_Debug(this,
            "Finished (final table): " + this.vParser_.type + " +" + 
            this.insert_ + " -" + this.remove_);
    
    this.tablesKnown_[this.vParser_.type] = this.vParser_;
    this.resetTableState();
  }

  if (this.isComplete()) this.onComplete();
}

/**
 * Returns true if all input data has been processed
 */
PROT_WireFormatReader.prototype.isComplete = function() {
  return this.offset_ >= this.tableUpdate_.length;
}

/**
 * Notifies our caller of completion.
 */
PROT_WireFormatReader.prototype.onComplete = function() {
  G_Debug(this, "Processing complete. Executing callback.");
  this.callback_(this.tablesKnown_, this.tablesData_);
  this.callback_ = null;
}


function TEST_PROT_WireFormat() {

  if (G_GDEBUG) {

    // Sorry, this is incredibly ugly. What we need is continuations -- each
    // unittest that passes invokes the next.

    var z = "wireformat UNITTEST";
    G_debugService.enableZone(z);
    G_Debug(z, "Starting");

    function testMalformedTables() {
      G_Debug(z, "Testing malformed tables...");
        
      var wfr = new PROT_WireFormatReader(new TH_ThreadQueue());
      
      // Damn these unittests are ugly. Ugh. Now test handling corrupt tables
      var data = 
        "[test1-black-url 1.1]\n" +
        "+foo1\tbar\n" +
        "+foo2\n" +           // Malformed
        "+foo3\tbar\n" +
        "+foo4\tbar\n" +
        "\n" +
        "[test2-black-url 1.2]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n" +
        "\n" +
        "[test3-black-url 1.3]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\n" +          // Malformed
        "\n" +
        "[test4-black-url 1.4]\n" +
        "+foo1\tbar\n" +
        "+foo2\tbar\n" +
        "+foo3\tbar\n" +
        "+foo4\tbar\n";

      function malformedcb(tablesKnown, tablesData) {
        
        // Table has malformed data
        G_Assert(z, tablesKnown["test1-black-url"].minor == "-1", 
                 "test table 1 didn't get reset");
        G_Assert(z, !!tablesData["test1-black-url"].find("foo1"), 
                 "test table 1 didn't set keys before the error");
        G_Assert(z, !!tablesData["test1-black-url"].find("foo3"),
                 "test table 1 didn't set keys after the error");

        // Table should be good
        G_Assert(z, tablesKnown["test2-black-url"].minor == "2", 
                 "test table 1 didnt get correct version number");
        G_Assert(z, !!tablesData["test2-black-url"].find("foo4"), 
                 "test table 2 didnt parse properly");
        
        // Table is malformed
        G_Assert(z, tablesKnown["test3-black-url"].minor == "-1", 
                 "test table 3 didn't get reset");
        G_Assert(z, !tablesData["test3-black-url"].find("foo4"),
                 "test table 3 somehow has its malformed line?");
        
        // Table should be good
        G_Assert(z, tablesKnown["test4-black-url"].minor == "4", 
                 "test table 4 didn't get correct version number");
        G_Assert(z, !!tablesData["test2-black-url"].find("foo4"), 
                 "test table 4 didnt parse properly");

        G_Debug(z, "PASSED");
      };
      
      wfr.deserialize(data, malformedcb);
    };

    var testTablesData = {};
    
    var tableName = "test-black-url";
    var data1 = "[" + tableName + " 1.5]\n";
    for (var i = 0; i < 100; i++)
      data1 += "+http://exists" + i + "\t1\n";
    data1 += "-http://exists50\t1\n";
    data1 += "-http://exists666\t1\n";

    function data1cb(tablesKnown, tablesData) {
      G_Assert(z, 
               tablesData[tableName] != null,
               "Didn't get our table back");

      for (var i = 0; i < 100; i++)
        if (i != 50)
          G_Assert(z, 
                   tablesData[tableName].find("http://exists" + i) == "1", 
                   "Item addition broken");

      G_Assert(z, 
               !tablesData[tableName].find("http://exists50"), 
               "Item removal broken");
      G_Assert(z, 
               !tablesData[tableName].find("http://exists666"), 
               "Non-existent item");

      G_Assert(z, tablesKnown[tableName].major == "1", "Major parsing broke");
      G_Assert(z, tablesKnown[tableName].minor == "5", "Major parsing broke");

      var data2 = "[" + tableName + " 1.7 update]\n";
      for (var i = 0; i < 100; i++)
        data2 += "-http://exists" + i + "\t1\n";
      var wfr2 = new PROT_WireFormatReader(new TH_ThreadQueue(), tablesData);
      wfr2.deserialize(data2, data2cb);
    };

    function data2cb(tablesKnown, tablesData) {
      for (var i = 0; i < 100; i++)
        G_Assert(z, 
                 !tablesData[tableName].find("http://exists" + i),
                 "Tables merge broken");

      G_Assert(z, tablesKnown[tableName].major == "1", "Major parsing broke");
      G_Assert(z, tablesKnown[tableName].minor == "7", "Major parsing broke");

      testMalformedTables();
    };

    var wfr = new PROT_WireFormatReader(new TH_ThreadQueue());
    wfr.deserialize(data1, data1cb);
  }
}
