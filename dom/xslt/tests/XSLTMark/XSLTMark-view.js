/* -*- indent-tabs-mode: nil; js-indent-level: 4 -*- */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

var view = {
  configUrl: null,
  testArray: null,
  mCurrent: null,

  browseForConfig() {
    enablePrivilege("UniversalXPConnect");
    var fp = Cc["@mozilla.org/filepicker;1"].createInstance(nsIFilePicker);
    fp.init(window, "XSLTMark Description File", nsIFilePicker.modeOpen);
    fp.appendFilter("*.conf", "*.conf");
    fp.appendFilters(nsIFilePicker.filterAll);
    var res = fp.show();

    if (res == nsIFilePicker.returnOK) {
      this.configUrl = Cc[STDURLMUT_CTRID].createInstance(nsIURIMutator)
        .setSpec(fp.fileURL.spec)
        .finalize();
      document
        .getElementById("config")
        .setAttribute("value", this.configUrl.spec);
    }
    this.parseConfig();
    return true;
  },

  parseConfig() {
    this.testArray = new Array();
    var test;
    if (!this.configUrl) {
      return;
    }

    var content = loadFile(this.configUrl.spec);

    var lines = content.split("\n");
    var line, res;
    var head = /^\[(.+)\]$/;
    var instruct = /^(.+)=(.+)$/;
    while (lines.length) {
      line = lines.shift();
      if (head.test(line)) {
        test = new Object();
        res = head.exec(line);
        test.title = res[1];
        this.testArray.push(test);
      } else if (line == "") {
        test = undefined;
      } else {
        res = instruct.exec(line);
        test[res[1]] = res[2];
      }
    }
  },

  onLoad() {
    this.mCurrentStatus = document.getElementById("currentStatus");
    this.mCurrentProgress = document.getElementById("currentProgress");
    this.mTotalProgress = document.getElementById("totalProgress");
    this.mOutput = document.getElementById("transformOutput");
    this.mDetailOutput = document.getElementById("transformDetailedOutput");
    this.mDetail = true;
  },

  progress(aTitle, aTime, aProgress) {
    // dump20(aTitle);
    // dump20(aTime);
    // dump20(aProgress);
    this.mCurrentProgress.value = aProgress;
    this.displayDetailTime(aTime);
    this.mTimes.push(aTime);
    // dump("\n");
  },

  done(aTitle) {
    // dump(aTitle + " is finished.\n");
    this.mCurrent++;
    this.mCurrentProgress.value = 0;
    this.displayTotalTime();
    if (this.mCurrent >= this.testArray.length) {
      this.mTotalProgress.value = 0;
      this.mCurrentStatus.value = "done";
      return;
    }
    this.mTotalProgress.value = (this.mCurrent * 100) / this.testArray.length;
    var test = this.testArray[this.mCurrent];
    enablePrivilege("UniversalXPConnect");
    this.displayTest(test.title);
    runTest(
      test.title,
      this.configUrl.resolve(test.input),
      this.configUrl.resolve(test.stylesheet),
      test.iterations,
      this
    );
  },

  onStop() {
    clearTimeout(gTimeout);
    this.mCurrentProgress.value = 0;
    this.mTotalProgress.value = 0;
    this.mCurrentStatus.value = "stopped";
  },

  displayTest(aTitle) {
    this.mTimes = new Array();
    aTitle += "\t";
    this.mCurrentStatus.value = aTitle;
    this.mOutput.value += aTitle;
    if (this.mDetail) {
      this.mDetailOutput.value += aTitle;
    }
  },

  displayDetailTime(aTime) {
    if (this.mDetail) {
      this.mDetailOutput.value += aTime + " ms\t";
    }
  },

  displayTotalTime() {
    var sum = 0;
    for (k = 0; k < this.mTimes.length; k++) {
      sum += this.mTimes[k];
    }
    var mean = sum / this.mTimes.length;
    this.mOutput.value += Number(mean).toFixed(2) + " ms\t" + sum + " ms\t";
    var variance = 0;
    for (k = 0; k < this.mTimes.length; k++) {
      var n = this.mTimes[k] - mean;
      variance += n * n;
    }
    variance = Math.sqrt(variance / this.mTimes.length);
    this.mOutput.value += Number(variance).toFixed(2) + "\n";
    if (this.mDetail) {
      this.mDetailOutput.value += "\n";
    }
  },

  runBenchmark() {
    enablePrivilege("UniversalXPConnect");
    if (!this.testArray) {
      if (!this.configUrl) {
        this.configUrl = Cc[STDURLMUT_CTRID].createInstance(nsIURIMutator)
          .setSpec(document.getElementById("config").value)
          .finalize();
      }
      this.parseConfig();
    }

    this.mCurrent = 0;
    var test = this.testArray[this.mCurrent];
    this.mOutput.value = "";
    if (this.mDetail) {
      this.mDetailOutput.value = "";
    }
    this.displayTest(test.title);
    runTest(
      test.title,
      this.configUrl.resolve(test.input),
      this.configUrl.resolve(test.stylesheet),
      test.iterations,
      this
    );
    return true;
  },
};
