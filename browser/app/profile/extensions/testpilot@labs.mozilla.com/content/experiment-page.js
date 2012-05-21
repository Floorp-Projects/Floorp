/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const PAGE_TYPE_STATUS = 0;
const PAGE_TYPE_QUIT = 1;
var stringBundle;

  function showRawData(experimentId) {
    window.openDialog(
      "chrome://testpilot/content/raw-data-dialog.xul",
      "TestPilotRawDataDialog", "chrome,centerscreen,resizable,scrollbars",
      experimentId);
  }

  function getUrlParam(name) {
    // from http://www.netlobo.com/url_query_string_javascript.html
    name = name.replace(/[\[]/,"\\\[").replace(/[\]]/,"\\\]");
    var regexS = "[\\?&]"+name+"=([^&#]*)";
    var regex = new RegExp(regexS);
    var results = regex.exec(window.location.href);
    if( results == null )
      return "";
    else
      return results[1];
  }

  function uploadData() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    let eid = getUrlParam("eid");
    let task = TestPilotSetup.getTaskById(eid);

    // If always-submit-checkbox is checked, set the pref
    if (task._recursAutomatically) {
      let checkBox = document.getElementById("always-submit-checkbox");
      if (checkBox && checkBox.checked) {
        task.setRecurPref(TaskConstants.ALWAYS_SUBMIT);
      }
    }

    // Study web content must provide an element with id 'upload-status'.
    // Fill it first with a message about data being uploaded; if there's
    // an error, replace it with the error message.
    let uploadStatus = document.getElementById("upload-status");
    uploadStatus.innerHTML =
      stringBundle.GetStringFromName("testpilot.statusPage.uploadingData");
    task.upload( function(success) {
      if (success) {
        window.location =
	  "chrome://testpilot/content/status.html?eid=" + eid;
      } else {
        // Replace 'now uploading' message
        let errorParagraph = document.createElement("p");
        errorParagraph.innerHTML = stringBundle.GetStringFromName("testpilot.statusPage.uploadErrorMsg");
        let willRetryParagraph = document.createElement("p");
        willRetryParagraph.innerHTML = stringBundle.GetStringFromName("testpilot.statusPage.willRetry");
        uploadStatus.innerHTML = "";
        uploadStatus.appendChild(errorParagraph);
        uploadStatus.appendChild(willRetryParagraph);
      }
    });
  }

  function deleteData() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    Components.utils.import("resource://testpilot/modules/tasks.js");
    let eid = getUrlParam("eid");
    let task = TestPilotSetup.getTaskById(eid);
    task.dataStore.wipeAllData();
    // reload the URL after wiping all data.
    window.location = "chrome://testpilot/content/status.html?eid=" + eid;
  }

  function saveCanvas(canvas) {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    let filePicker = Components.classes["@mozilla.org/filepicker;1"].
      createInstance(nsIFilePicker);
    filePicker.init(window, null, nsIFilePicker.modeSave);
    filePicker.appendFilters(
	nsIFilePicker.filterImages | nsIFilePicker.filterAll);
    filePicker.defaultString = "canvas.png";

    let response = filePicker.show();
    if (response == nsIFilePicker.returnOK ||
	response == nsIFilePicker.returnReplace) {
      const nsIWebBrowserPersist = Components.interfaces.nsIWebBrowserPersist;
      let file = filePicker.file;

      // create a data url from the canvas and then create URIs of the source
      // and targets
      let io = Components.classes["@mozilla.org/network/io-service;1"].
	getService(Components.interfaces.nsIIOService);
      let source = io.newURI(canvas.toDataURL("image/png"), "UTF8", null);
      let target = io.newFileURI(file);

      // prepare to save the canvas data
      let persist = Components.classes[
	"@mozilla.org/embedding/browser/nsWebBrowserPersist;1"].
	  createInstance(nsIWebBrowserPersist);
      persist.persistFlags = nsIWebBrowserPersist.
	PERSIST_FLAGS_REPLACE_EXISTING_FILES;
      persist.persistFlags |= nsIWebBrowserPersist.
        PERSIST_FLAGS_AUTODETECT_APPLY_CONVERSION;

      // displays a download dialog (remove these 3 lines for silent download)
      let xfer = Components.classes["@mozilla.org/transfer;1"].
	createInstance(Components.interfaces.nsITransfer);
      xfer.init(source, target, "", null, null, null, persist);
      persist.progressListener = xfer;

      // save the canvas data to the file
      persist.saveURI(source, null, null, null, null, file);
    }
  }

  function exportData() {
    const nsIFilePicker = Components.interfaces.nsIFilePicker;
    let filePicker = Components.classes["@mozilla.org/filepicker;1"].
      createInstance(nsIFilePicker);
    let eid = getUrlParam("eid");
    let task = TestPilotSetup.getTaskById(eid);

    filePicker.init(window, null, nsIFilePicker.modeSave);
    filePicker.appendFilters(
	nsIFilePicker.filterImages | nsIFilePicker.filterAll);
    filePicker.defaultString = task.title + ".csv";

    let response = filePicker.show();
    if (response == nsIFilePicker.returnOK ||
	response == nsIFilePicker.returnReplace) {
      const nsIWebBrowserPersist = Components.interfaces.nsIWebBrowserPersist;
      let foStream =
        Components.classes["@mozilla.org/network/file-output-stream;1"].
	  createInstance(Components.interfaces.nsIFileOutputStream);
      let converter =
        Components.classes["@mozilla.org/intl/converter-output-stream;1"].
	  createInstance(Components.interfaces.nsIConverterOutputStream);
      let file = filePicker.file;
      let dataStore = task.dataStore;
      let columnNames = dataStore.getHumanReadableColumnNames();
      let propertyNames = dataStore.getPropertyNames();
      let csvString = "";

      // titles
      for (let i = 0; i < columnNames.length; i++) {
	csvString += "\"" + columnNames[i] + "\",";
      }
      if (csvString.length > 0) {
	csvString = csvString.substring(0, (csvString.length - 1));
        csvString += "\n";
      }

      dataStore.getAllDataAsJSON(true, function(rawData) {
        // data
        for (let i = 0; i < rawData.length; i++) {
          for (let j = 0; j < columnNames.length; j++) {
	    csvString += "\"" + rawData[i][propertyNames[j]] + "\",";
          }
	  csvString = csvString.substring(0, (csvString.length - 1));
          csvString += "\n";
        }

        // write, create, truncate
        foStream.init(file, 0x02 | 0x08 | 0x20, 0664, 0);
        converter.init(foStream, "UTF-8", 0, 0);
        converter.writeString(csvString);
        converter.close();
      });
    }
  }

  function openLink(url) {
    // open the link in the chromeless window
    let wm = Components.classes["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Components.interfaces.nsIWindowMediator);
    let recentWindow = wm.getMostRecentWindow("navigator:browser");

    if (recentWindow) {
      recentWindow.TestPilotWindowUtils.openInTab(url);
    } else {
      window.open(url);
    }
  }

  function getTestEndingDate(experimentId) {
    Components.utils.import("resource://testpilot/modules/setup.js");
    var task = TestPilotSetup.getTaskById(experimentId);
    var endDate = new Date(task.endDate);
    var diff = (endDate - Date.now());
    var span = document.getElementById("test-end-time");
    if (!span) {
      return;
    }
    if (diff < 0) {
      span.innerHTML =
        stringBundle.GetStringFromName("testpilot.statusPage.endedAlready");
      return;
    }
    var hours = diff / (60 * 60 * 1000);
    if (hours < 24) {
      span.innerHTML =
        stringBundle.formatStringFromName(
	  "testpilot.statusPage.todayAt", [endDate.toLocaleTimeString()], 1);
    } else {
      span.innerHTML =
        stringBundle.formatStringFromName(
	  "testpilot.statusPage.endOn", [endDate.toLocaleString()], 1);
    }
  }

  function showMetaData() {
    Components.utils.import("resource://testpilot/modules/metadata.js");
    Components.utils.import("resource://gre/modules/PluralForm.jsm");
    MetadataCollector.getMetadata(function(md) {
      var mdLocale = document.getElementById("md-locale");
      if (mdLocale)
        mdLocale.innerHTML = md.location;
      var mdVersion = document.getElementById("md-version");
      if (mdVersion)
        mdVersion.innerHTML = md.version;
      var mdOs = document.getElementById("md-os");
      if (mdOs)
        mdOs.innerHTML = md.operatingSystem;
      var mdNumExt = document.getElementById("md-num-ext");
      if (mdNumExt) {
        // This computes the correctly localized singular or plural string
        // of the number of extensions, e.g. "1 extension", "2 extensions", etc.
        let str = stringBundle.GetStringFromName("testpilot.statusPage.numExtensions");
        var numExt = md.extensions.length;
        mdNumExt.innerHTML = PluralForm.get(numExt, str).replace("#1", numExt);
      }
    });
  }

  function onQuitPageLoad() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    setStrings(PAGE_TYPE_QUIT);
    let eid = getUrlParam("eid");
    let task = TestPilotSetup.getTaskById(eid);
    let header = document.getElementById("about-quit-title");
    header.innerHTML =
      stringBundle.formatStringFromName(
	"testpilot.quitPage.aboutToQuit", [task.title], 1);

    if (task._recursAutomatically) {
      document.getElementById("recur-options").setAttribute("style", "");
      document.getElementById("recur-checkbox-container").
        setAttribute("style", "");
    }
  }

  function quitExperiment() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    Components.utils.import("resource://testpilot/modules/tasks.js");
    let eid = getUrlParam("eid");
    let reason = document.getElementById("reason-for-quit").value;
    let task = TestPilotSetup.getTaskById(eid);
    task.optOut(reason, function(success) {
      // load the you-are-canceleed page.
      window.location = "chrome://testpilot/content/status.html?eid=" + eid;
    });

    // If opt-out-forever checkbox is checked, opt out forever!
    if (task._recursAutomatically) {
      let checkBox = document.getElementById("opt-out-forever");
      if (checkBox.checked) {
        task.setRecurPref(TaskConstants.NEVER_SUBMIT);
      }
      // quit test so rescheduling
      task._reschedule();
    }
  }

  function updateRecurSettings() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    let eid = getUrlParam("eid");
    let experiment = TestPilotSetup.getTaskById(eid);
    let recurSelector = document.getElementById("recur-selector");
    let newValue = recurSelector.options[recurSelector.selectedIndex].value;
    experiment.setRecurPref(parseInt(newValue));
  }

  function showRecurControls(experiment) {
    Components.utils.import("resource://testpilot/modules/tasks.js");
    let recurPrefSpan = document.getElementById("recur-pref");
    if (!recurPrefSpan) {
      return;
    }
    let days = experiment._recurrenceInterval;
    recurPrefSpan.innerHTML =
      stringBundle.formatStringFromName(
	"testpilot.statusPage.recursEveryNumberOfDays", [days], 1);

    let controls = document.getElementById("recur-controls");
    let selector = document.createElement("select");
    controls.appendChild(selector);
    selector.setAttribute("onchange", "updateRecurSettings();");
    selector.setAttribute("id", "recur-selector");

    let option = document.createElement("option");
    option.setAttribute("value", TaskConstants.ASK_EACH_TIME);
    if (experiment.recurPref == TaskConstants.ASK_EACH_TIME) {
      option.setAttribute("selected", "true");
    }
    option.innerHTML =
      stringBundle.GetStringFromName(
	"testpilot.statusPage.askMeBeforeSubmitData");
    selector.appendChild(option);

    option = document.createElement("option");
    option.setAttribute("value", TaskConstants.ALWAYS_SUBMIT);
    if (experiment.recurPref == TaskConstants.ALWAYS_SUBMIT) {
      option.setAttribute("selected", "true");
    }
    option.innerHTML =
      stringBundle.GetStringFromName(
	"testpilot.statusPage.alwaysSubmitData");
    selector.appendChild(option);

    option = document.createElement("option");
    option.setAttribute("value", TaskConstants.NEVER_SUBMIT);
    if (experiment.recurPref == TaskConstants.NEVER_SUBMIT) {
      option.setAttribute("selected", "true");
    }
    option.innerHTML =
      stringBundle.GetStringFromName(
	"testpilot.statusPage.neverSubmitData");
    selector.appendChild(option);
  }

  function loadExperimentPage() {
    Components.utils.import("resource://testpilot/modules/setup.js");
    Components.utils.import("resource://testpilot/modules/tasks.js");
    var contentDiv = document.getElementById("experiment-specific-text");
    var dataPrivacyDiv = document.getElementById("data-privacy-text");
    // Get experimentID from the GET args of page
    var eid = getUrlParam("eid");
    var experiment = TestPilotSetup.getTaskById(eid);
    if (!experiment) {
      // Possible that experiments aren't done loading yet.  Try again in
      // a few seconds.
      contentDiv.innerHTML =
        stringBundle.GetStringFromName("testpilot.statusPage.loading");
      window.setTimeout(function() { loadExperimentPage(); }, 2000);
      return;
    }

    // Let the experiment fill in its web content (asynchronous)
    experiment.getWebContent(function(webContent) {
      contentDiv.innerHTML = webContent;

      // Metadata and start/end date should be filled in for every experiment:
      showMetaData();
      getTestEndingDate(eid);
      if (experiment._recursAutomatically &&
        experiment.status != TaskConstants.STATUS_FINISHED) {
        showRecurControls(experiment);
      }

      // Do whatever the experiment's web content wants done on load
      // (Usually drawing a graph) - must be done after innerHTML is set.
      experiment.webContent.onPageLoad(experiment, document, jQuery);
    });

    experiment.getDataPrivacyContent(function(dataPrivacyContent) {
      if (dataPrivacyContent && dataPrivacyContent.length > 0) {
        dataPrivacyDiv.innerHTML = dataPrivacyContent;
        dataPrivacyDiv.removeAttribute("hidden");
      }
    });
  }

  function onStatusPageLoad() {
    setStrings(PAGE_TYPE_STATUS);
    /* An experiment ID (eid) must be provided in the url params. Show status
     * for that experiment.*/
    loadExperimentPage();
  }

  function setStrings(pageType) {
    stringBundle =
      Components.classes["@mozilla.org/intl/stringbundle;1"].
        getService(Components.interfaces.nsIStringBundleService).
	  createBundle("chrome://testpilot/locale/main.properties");
    let map;
    let mapLength;

    if (pageType == PAGE_TYPE_STATUS) {
      map = [
	{ id: "page-title", stringKey: "testpilot.fullBrandName" },
	{ id: "comments-and-discussions-link",
	  stringKey: "testpilot.page.commentsAndDiscussions" },
	{ id: "propose-test-link",
	  stringKey: "testpilot.page.proposeATest" },
	{ id: "testpilot-twitter-link",
	  stringKey: "testpilot.page.testpilotOnTwitter" }
      ];
    } else if (pageType == PAGE_TYPE_QUIT) {
      map = [
	{ id: "page-title", stringKey: "testpilot.fullBrandName" },
	{ id: "comments-and-discussions-link",
	  stringKey: "testpilot.page.commentsAndDiscussions" },
	{ id: "propose-test-link",
	  stringKey: "testpilot.page.proposeATest" },
	{ id: "testpilot-twitter-link",
	  stringKey: "testpilot.page.testpilotOnTwitter" },
	{ id: "optional-message",
	  stringKey: "testpilot.quitPage.optionalMessage" },
	{ id: "reason-text",
	  stringKey: "testpilot.quitPage.reason" },
	{ id: "recur-options",
	  stringKey: "testpilot.quitPage.recurringStudy" },
	{ id: "quit-forever-text",
	  stringKey: "testpilot.quitPage.quitForever" },
	{ id: "quit-study-link",
	  stringKey: "testpilot.quitPage.quitStudy" }
      ];
    }
    mapLength = map.length;
    for (let i = 0; i < mapLength; i++) {
      let entry = map[i];
      document.getElementById(entry.id).innerHTML =
        stringBundle.GetStringFromName(entry.stringKey);
    }
  }
