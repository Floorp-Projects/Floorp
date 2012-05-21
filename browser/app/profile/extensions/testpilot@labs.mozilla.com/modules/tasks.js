/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

EXPORTED_SYMBOLS = ["TaskConstants", "TestPilotBuiltinSurvey",
                    "TestPilotExperiment", "TestPilotStudyResults",
                    "TestPilotLegacyStudy", "TestPilotWebSurvey"];

const Cc = Components.classes;
const Ci = Components.interfaces;

Components.utils.import("resource://testpilot/modules/Observers.js");
Components.utils.import("resource://testpilot/modules/metadata.js");
Components.utils.import("resource://testpilot/modules/log4moz.js");
Components.utils.import("resource://testpilot/modules/string_sanitizer.js");

const STATUS_PREF_PREFIX = "extensions.testpilot.taskstatus.";
const START_DATE_PREF_PREFIX = "extensions.testpilot.startDate.";
const RECUR_PREF_PREFIX = "extensions.testpilot.reSubmit.";
const RECUR_TIMES_PREF_PREFIX = "extensions.testpilot.recurCount.";
const SURVEY_ANSWER_PREFIX = "extensions.testpilot.surveyAnswers.";
const EXPIRATION_DATE_FOR_DATA_SUBMISSION_PREFIX =
  "extensions.testpilot.expirationDateForDataSubmission.";
const DATE_FOR_DATA_DELETION_PREFIX =
  "extensions.testpilot.dateForDataDeletion.";
const GUID_PREF_PREFIX = "extensions.testpilot.taskGUID.";
const RETRY_INTERVAL_PREF = "extensions.testpilot.uploadRetryInterval";
const TIME_FOR_DATA_DELETION = 7 * (24 * 60 * 60 * 1000); // 7 days
const DATA_UPLOAD_PREF = "extensions.testpilot.dataUploadURL";
const DEFAULT_THUMBNAIL_URL = "chrome://testpilot/skin/badge-default.png";


const TaskConstants = {
  // TODO status RESULTS and ARCHIVED don't make sense for studies anymore;
  // we need a status MISSED and a status EXPIRED.  (can you be cancelled and
  // expired?)
 STATUS_NEW: 0, // It's new and you haven't seen it yet.
 STATUS_PENDING : 1,  // You've seen it but it hasn't started.
 STATUS_STARTING: 2,  // Data collection started but notification not shown.
 STATUS_IN_PROGRESS : 3, // Started and notification shown.
 STATUS_FINISHED : 4, // Finished and awaiting your choice.
 STATUS_CANCELLED : 5, // You've opted out and not submitted anything.
 STATUS_SUBMITTED : 6, // You've submitted your data.
 STATUS_RESULTS : 7, // Test finished AND final results visible somewhere, deprecated
 STATUS_ARCHIVED: 8, // Results seen.  Deprecated. TODO: or use for expired?
 STATUS_MISSED: 9, // You never ran this study.

 TYPE_EXPERIMENT : 1,
 TYPE_SURVEY : 2,
 TYPE_RESULTS : 3,
 TYPE_LEGACY: 4,

 ALWAYS_SUBMIT: 1,
 NEVER_SUBMIT: -1,
 ASK_EACH_TIME: 0
};
/* Note that experiments use all 9 status codes, but surveys don't have a
 * data collection period so they are never STARTING or IN_PROGRESS or
 * FINISHED, they go straight from PENDING to SUBMITTED or CANCELED. */

let Application = Cc["@mozilla.org/fuel/application;1"]
                  .getService(Ci.fuelIApplication);

// Prototype for both TestPilotSurvey and TestPilotExperiment.
var TestPilotTask = {
  _id: null,
  _title: null,
  _status: null,
  _url: null,

  _taskInit: function TestPilotTask__taskInit(id, title, url, summary, thumb) {
    this._id = id;
    this._title = title;
    this._status = Application.prefs.getValue(STATUS_PREF_PREFIX + this._id,
                                              TaskConstants.STATUS_NEW);
    this._url = url;
    this._summary = summary;
    this._thumbnail = thumb;
    this._logger = Log4Moz.repository.getLogger("TestPilot.Task_"+this._id);
  },

  get title() {
    return this._title;
  },

  get id() {
    return this._id;
  },

  get version() {
    return this._versionNumber;
  },

  get taskType() {
    return null;
  },

  get status() {
    return this._status;
  },

  get webContent() {
    return this._webContent;
  },

  get summary() {
    if (this._summary) {
      return this._summary;
    } else {
      return this._title;
    }
  },

  get thumbnail() {
    if (this._thumbnail) {
      return this._thumbnail;
    } else {
      return DEFAULT_THUMBNAIL_URL;
    }
  },

  // urls:
  get infoPageUrl() {
    return this._url;
  },

  get currentStatusUrl() {
    return this._url;
  },

  get defaultUrl() {
    return this.infoPageUrl;
  },

  get uploadUrl() {
    let url = Application.prefs.getValue(DATA_UPLOAD_PREF, "");
    return url + this._id;
  },

  // event handlers:

  onExperimentStartup: function TestPilotTask_onExperimentStartup() {
    // Called when experiment is to start running (either on Firefox
    // startup, or when study first becomes IN_PROGRESS)
  },

  onExperimentShutdown: function TestPilotTask_onExperimentShutdown() {
    // Called when experiment needs to stop running (either on Firefox
    // shutdown, or on experiment reload, or on finishing or being canceled.)
  },

  doExperimentCleanup: function TestPilotTask_onExperimentCleanup() {
    // Called when experiment has finished or been canceled; do any cleanup
    // of user's profile.
  },

  onAppStartup: function TestPilotTask_onAppStartup() {
    // Called by extension core when Firefox startup is complete.
  },

  onAppShutdown: function TestPilotTask_onAppShutdown() {
    // Called by extension core when Firefox is shutting down.
  },

  onEnterPrivateBrowsing: function TestPilotTask_onEnterPrivate() {
  },

  onExitPrivateBrowsing: function TestPilotTask_onExitPrivate() {
  },

  onNewWindow: function TestPilotTask_onNewWindow(window) {
  },

  onWindowClosed: function TestPilotTask_onWindowClosed(window) {
  },

  onUrlLoad: function TestPilotTask_onUrlLoad(url) {
  },

  onDetailPageOpened: function TestPilotTask_onDetailPageOpened(){
    // TODO fold this into loadPage()?
  },

  checkDate: function TestPilotTask_checkDate() {
  },

  changeStatus: function TPS_changeStatus(newStatus, suppressNotification) {
    // TODO we always suppress notifications except when new status is
    // "finished"; maybe remove that argument and only fire notification
    // when status is "finished".
    let logger = Log4Moz.repository.getLogger("TestPilot.Task");
    logger.info("Changing task " + this._id + " status to " + newStatus);
    this._status = newStatus;
    // Set the pref:
    Application.prefs.setValue(STATUS_PREF_PREFIX + this._id, newStatus);
    // Notify user of status change:
    if (!suppressNotification) {
      Observers.notify("testpilot:task:changed", "", null);
    }
  },

  loadPage: function TestPilotTask_loadPage() {
    // open the link in the chromeless window
    var wm = Cc["@mozilla.org/appshell/window-mediator;1"]
                       .getService(Ci.nsIWindowMediator);
    let window = wm.getMostRecentWindow("navigator:browser");
    window.TestPilotWindowUtils.openChromeless(this.defaultUrl);
    /* Advance the status when the user sees the page, so that we can stop
     * notifying them about stuff they've seen. */
    if (this._status == TaskConstants.STATUS_NEW) {
      this.changeStatus(TaskConstants.STATUS_PENDING);
    } else if (this._status == TaskConstants.STATUS_STARTING) {
      this.changeStatus(TaskConstants.STATUS_IN_PROGRESS);
    } else if (this._status == TaskConstants.STATUS_RESULTS) {
      this.changeStatus(TaskConstants.STATUS_ARCHIVED);
    }

    this.onDetailPageOpened();
  },

  getGuid: function TPS_getGuid(id) {
    // If there is a guid for the task with the given id (not neccessarily this one!)
    // then use it; if there isn't, generate it and store it.
    let guid = Application.prefs.getValue(GUID_PREF_PREFIX + id, "");
    if (guid == "") {
      let uuidGenerator =
        Cc["@mozilla.org/uuid-generator;1"].getService(Ci.nsIUUIDGenerator);
      guid = uuidGenerator.generateUUID().toString();
      // remove the brackets from the generated UUID
      if (guid.indexOf("{") == 0) {
        guid = guid.substring(1, (guid.length - 1));
      }
      Application.prefs.setValue(GUID_PREF_PREFIX + id, guid);
    }
    return guid;
  }
};

function TestPilotExperiment(expInfo, dataStore, handlers, webContent, dateOverrideFunc) {
  // All four of these are objects defined in the remote experiment file
  this._init(expInfo, dataStore, handlers, webContent, dateOverrideFunc);
}
TestPilotExperiment.prototype = {
  _init: function TestPilotExperiment__init(expInfo,
					    dataStore,
					    handlers,
                                            webContent,
                                            dateOverrideFunc) {
    /* expInfo is a dictionary defined in the remote experiment code, which
     * should have the following properties:
     * startDate (string representation of date)
     * duration (number of days)
     * testName (human-readable string)
     * testId (int)
     * testInfoUrl (url)
     * summary (string - couple of sentences explaining study)
     * thumbnail (url of an image representing the study)
     * optInRequired  (boolean)
     * recursAutomatically (boolean)
     * recurrenceInterval (number of days)
     * versionNumber (int) */

    // dateOverrideFunc: For unit testing. Optional. If provided, will be called
    // instead of Date.now() for determining the current time.
    if (dateOverrideFunc) {
      this._now = dateOverrideFunc;
    } else {
      this._now = Date.now;
    }
    this._taskInit(expInfo.testId, expInfo.testName, expInfo.testInfoUrl,
                   expInfo.summary, expInfo.thumbnail);
    this._webContent = webContent;
    this._dataStore = dataStore;
    this._versionNumber = expInfo.versionNumber;
    this._optInRequired = expInfo.optInRequired;
    // TODO implement opt-in interface for tests that require opt-in.
    this._recursAutomatically = expInfo.recursAutomatically;
    this._recurrenceInterval = expInfo.recurrenceInterval;

    let prefName = START_DATE_PREF_PREFIX + this._id;
    let startDateString = Application.prefs.getValue(prefName, false);
    if (startDateString) {
      // If this isn't the first time we're starting, use the start date
      // already stored in prefs.
      this._startDate = Date.parse(startDateString);
    } else {
      // If a start date is provided in expInfo, use that.
      // Otherwise, start immediately!
      if (expInfo.startDate) {
        this._startDate = Date.parse(expInfo.startDate);
        Application.prefs.setValue(prefName, expInfo.startDate);
      } else {
        this._startDate = this._now();
        Application.prefs.setValue(prefName, (new Date(this._startDate)).toString());
      }
    }

    // Duration is specified in days:
    let duration = expInfo.duration || 7; // default 1 week
    this._endDate = this._startDate + duration * (24 * 60 * 60 * 1000);
    this._logger.info("Start date is " + this._startDate.toString());
    this._logger.info("End date is " + this._endDate.toString());

    this._handlers = handlers;
    this._uploadRetryTimer = null;
    this._startedUpHandlers = false;

    // checkDate will see what our status is with regards to the start and
    // end dates, and set status appropriately.
    this.checkDate();

    if (this.experimentIsRunning) {
      this.onExperimentStartup();
    }
  },

  get taskType() {
    return TaskConstants.TYPE_EXPERIMENT;
  },

  get endDate() {
    return this._endDate;
  },

  get startDate() {
    return this._startDate;
  },

  get dataStore() {
    return this._dataStore;
  },

  get currentStatusUrl() {
    let param = "?eid=" + this._id;
    return "chrome://testpilot/content/status.html" + param;
  },

  get defaultUrl() {
    return this.currentStatusUrl;
  },

  get recurPref() {
    let prefName = RECUR_PREF_PREFIX + this._id;
    return Application.prefs.getValue(prefName, TaskConstants.ASK_EACH_TIME);
  },

  getDataStoreAsJSON: function(callback) {
    this._dataStore.getAllDataAsJSON(false, callback);
  },

  getWebContent: function TestPilotExperiment_getWebContent(callback) {
    let content = "";
    let waitForData = false;
    let self = this;

    switch (this._status) {
      case TaskConstants.STATUS_NEW:
      case TaskConstants.STATUS_PENDING:
        content = this.webContent.upcomingHtml;
      break;
      case TaskConstants.STATUS_STARTING:
      case TaskConstants.STATUS_IN_PROGRESS:
        content = this.webContent.inProgressHtml;
      break;
      case TaskConstants.STATUS_FINISHED:
	waitForData = true;
	this._dataStore.haveData(function(withData) {
	  if (withData) {
	    content = self.webContent.completedHtml;
	  } else {
	    // for after deleting data manually by user.
            let stringBundle =
              Components.classes["@mozilla.org/intl/stringbundle;1"].
                getService(Components.interfaces.nsIStringBundleService).
	          createBundle("chrome://testpilot/locale/main.properties");
	    let link =
	      '<a href="' + self.infoPageUrl + '">' + self.title + '</a>';
	    content =
	      '<h2>' + stringBundle.formatStringFromName(
	        "testpilot.finishedTask.finishedStudy", [link], 1) + '</h2>' +
	      '<p>' + stringBundle.GetStringFromName(
	        "testpilot.finishedTask.allRelatedDataDeleted") + '</p>';
	  }
	  callback(content);
	});
      break;
      case TaskConstants.STATUS_CANCELLED:
	if (this._expirationDateForDataSubmission.length == 0) {
          content = this.webContent.canceledHtml;
	} else {
          content = this.webContent.dataExpiredHtml;
	}
      break;
      case TaskConstants.STATUS_SUBMITTED:
	if (this._dateForDataDeletion.length > 0) {
          content = this.webContent.remainDataHtml;
	} else {
          content = this.webContent.deletedRemainDataHtml;
	}
      break;
    }
    // TODO what to do if status is cancelled, submitted, results, or archived?
    if (!waitForData) {
      callback(content);
    }
  },

  getDataPrivacyContent: function(callback) {
    let content = "";
    let waitForData = false;
    let self = this;

    switch (this._status) {
      case TaskConstants.STATUS_STARTING:
      case TaskConstants.STATUS_IN_PROGRESS:
        content = this.webContent.inProgressDataPrivacyHtml;
      break;
      case TaskConstants.STATUS_FINISHED:
	waitForData = true;
	this._dataStore.haveData(function(withData) {
	  if (withData) {
	    content = self.webContent.completedDataPrivacyHtml;
	  }
	  callback(content);
	});
      break;
      case TaskConstants.STATUS_CANCELLED:
	if (this._expirationDateForDataSubmission.length == 0) {
          content = this.webContent.canceledDataPrivacyHtml;
	} else {
          content = this.webContent.dataExpiredDataPrivacyHtml;
	}
      break;
      case TaskConstants.STATUS_SUBMITTED:
	if (this._dateForDataDeletion.length > 0) {
          content = this.webContent.remainDataDataPrivacyHtml;
	} else {
          content = this.webContent.deletedRemainDataDataPrivacyHtml;
	}
      break;
    }
    if (!waitForData) {
      callback(content);
    }
  },

  experimentIsRunning: function TestPilotExperiment_isRunning() {
    // bug 575767
    return (this._status == TaskConstants.STATUS_STARTING ||
            this._status == TaskConstants.STATUS_IN_PROGRESS);
  },

  // Pass events along to handlers:
  onNewWindow: function TestPilotExperiment_onNewWindow(window) {
    this._logger.trace("Experiment.onNewWindow called.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onNewWindow(window);
      } catch(e) {
        this._dataStore.logException("onNewWindow: " + e);
      }
    }
  },

  onWindowClosed: function TestPilotExperiment_onWindowClosed(window) {
    this._logger.trace("Experiment.onWindowClosed called.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onWindowClosed(window);
      } catch(e) {
        this._dataStore.logException("onWindowClosed: " + e);
      }
    }
  },

  onAppStartup: function TestPilotExperiment_onAppStartup() {
    this._logger.trace("Experiment.onAppStartup called.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onAppStartup();
      } catch(e) {
        this._dataStore.logException("onAppStartup: " + e);
      }
    }
  },

  onAppShutdown: function TestPilotExperiment_onAppShutdown() {
    this._logger.trace("Experiment.onAppShutdown called.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onAppShutdown();
      } catch(e) {
        this._dataStore.logException("onAppShutdown: " + e);
      }
    }
  },

  onExperimentStartup: function TestPilotExperiment_onStartup() {
    this._logger.trace("Experiment.onExperimentStartup called.");
    // Make sure not to call this if it's already been called:
    if (this.experimentIsRunning() && !this._startedUpHandlers) {
      this._logger.trace("  ... starting up handlers!");
      try {
        this._handlers.onExperimentStartup(this._dataStore);
      } catch(e) {
        this._dataStore.logException("onExperimentStartup: " + e);
      }
      this._startedUpHandlers = true;
    }
  },

  onExperimentShutdown: function TestPilotExperiment_onShutdown() {
    this._logger.trace("Experiment.onExperimentShutdown called.");
    if (this.experimentIsRunning() && this._startedUpHandlers) {
      try {
        this._handlers.onExperimentShutdown();
      } catch(e) {
        this._dataStore.logException("onExperimentShutdown: " + e);
      }
      this._startedUpHandlers = false;
    }
  },

  doExperimentCleanup: function TestPilotExperiment_doExperimentCleanup() {
    if (this._handlers.doExperimentCleanup) {
      this._logger.trace("Doing experiment cleanup.");
      try {
        this._handlers.doExperimentCleanup();
      } catch(e) {
        this._dataStore.logException("doExperimentCleanup: " + e);
      }
    }
  },

  onEnterPrivateBrowsing: function TestPilotExperiment_onEnterPrivate() {
    this._logger.trace("Task is entering private browsing.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onEnterPrivateBrowsing();
      } catch(e) {
        this._dataStore.logException("onEnterPrivateBrowsing: " + e);
      }
    }
  },

  onExitPrivateBrowsing: function TestPilotExperiment_onExitPrivate() {
    this._logger.trace("Task is exiting private browsing.");
    if (this.experimentIsRunning()) {
      try {
        this._handlers.onExitPrivateBrowsing();
      } catch(e) {
        this._dataStore.logException("onExitPrivateBrowsing: " + e);
      }
    }
  },

  getStudyMetadata: function TestPilotExperiment_getStudyMetadata() {
    try {
      if (this._handlers.getStudyMetadata) {
        let metadata = this._handlers.getStudyMetadata();
        if (metadata.length) {
          // getStudyMetadata must return an array, otherwise it is invalid.
          return metadata;
        }
      }
    } catch(e) {
      this._dataStore.logException("getStudyMetadata: " + e);
    }
    return null;
  },

  _reschedule: function TestPilotExperiment_reschedule() {
    // Schedule next run of test:
    // add recurrence interval to start date and store!
    let ms = this._recurrenceInterval * (24 * 60 * 60 * 1000);
    // recurrenceInterval is in days, convert to milliseconds:
    this._startDate += ms;
    this._endDate += ms;
    let prefName = START_DATE_PREF_PREFIX + this._id;
    Application.prefs.setValue(prefName,
                               (new Date(this._startDate)).toString());
  },

  get _numTimesRun() {
    // For automatically recurring tests, this is the number of times it
    // has recurred - it will be 1 on the first run, 2 on the second run,
    // etc.
    if (this._recursAutomatically) {
      return Application.prefs.getValue(RECUR_TIMES_PREF_PREFIX + this._id,
                                        1);
    } else {
      return 0;
    }
  },

  set _expirationDateForDataSubmission(date) {
    if (date) {
      Application.prefs.setValue(
        EXPIRATION_DATE_FOR_DATA_SUBMISSION_PREFIX + this._id,
        (new Date(date)).toString());
    } else {
      Application.prefs.setValue(
        EXPIRATION_DATE_FOR_DATA_SUBMISSION_PREFIX + this._id, "");
    }
  },

  get _expirationDateForDataSubmission() {
    return Application.prefs.getValue(
      EXPIRATION_DATE_FOR_DATA_SUBMISSION_PREFIX + this._id, "");
  },

  set _dateForDataDeletion(date) {
    if (date) {
      Application.prefs.setValue(
        DATE_FOR_DATA_DELETION_PREFIX + this._id, (new Date(date)).toString());
    } else {
      Application.prefs.setValue(DATE_FOR_DATA_DELETION_PREFIX + this._id, "");
    }
  },

  get _dateForDataDeletion() {
    return Application.prefs.getValue(
      DATE_FOR_DATA_DELETION_PREFIX + this._id, "");
  },

  checkDate: function TestPilotExperiment_checkDate() {
    // This method handles all date-related status changes and should be
    // called periodically.
    let currentDate = this._now();
    let self = this;

    // Reset automatically recurring tests:
    if (this._recursAutomatically &&
        this._status >= TaskConstants.STATUS_FINISHED &&
        currentDate >= this._startDate &&
	currentDate <= this._endDate) {
      // if we've done a permanent opt-out, then don't start over-
      // just keep rescheduling.
      if (this.recurPref == TaskConstants.NEVER_SUBMIT) {
        this._logger.info("recurPref is never submit, so I'm rescheduling.");
        this._reschedule();
      } else {
        // Normal case is reset to new.
        this.changeStatus(TaskConstants.STATUS_NEW, true);

        // increment count of how many times this recurring test has run
        let numTimesRun = this._numTimesRun;
        numTimesRun++;
        this._logger.trace("Test recurring... incrementing " + RECUR_TIMES_PREF_PREFIX + this._id + " to " + numTimesRun);
        Application.prefs.setValue( RECUR_TIMES_PREF_PREFIX + this._id,
                                    numTimesRun );
        this._logger.trace("Incremented it.");
      }
    }

    // If the notify-on-new-study pref is turned off, and the test doesn't
    // require opt-in, then it can jump straight ahead to STARTING.
    if (!this._optInRequired &&
        !Application.prefs.getValue("extensions.testpilot.popup.showOnNewStudy",
                                    false) &&
        (this._status == TaskConstants.STATUS_NEW ||
         this._status == TaskConstants.STATUS_PENDING)) {
      this._logger.info("Skipping pending and going straight to starting.");
      this.changeStatus(TaskConstants.STATUS_STARTING, true);
    }

    // If a study is STARTING, and we're in the right date range,
    // then start it, and move it to IN_PROGRESS.
    if ( this._status == TaskConstants.STATUS_STARTING &&
        currentDate >= this._startDate &&
        currentDate <= this._endDate) {
      this._logger.info("Study now starting.");
      // clear the data before starting.
      this._dataStore.wipeAllData(function() {
        // Experiment is now in progress.
        self.changeStatus(TaskConstants.STATUS_IN_PROGRESS, true);
        self.onExperimentStartup();
      });
    }

    // What happens when a test finishes:
    if (this._status < TaskConstants.STATUS_FINISHED &&
	currentDate > this._endDate) {
      let setDataDeletionDate = true;
      this._logger.info("Passed End Date - Switched Task Status to Finished");
      this.changeStatus(TaskConstants.STATUS_FINISHED);
      this.onExperimentShutdown();
      this.doExperimentCleanup();

      if (this._recursAutomatically) {
        this._reschedule();
        // A recurring experiment may have been set to automatically submit. If
        // so, submit now!
        if (this.recurPref == TaskConstants.ALWAYS_SUBMIT) {
          this._logger.info("Automatically Uploading Data");
          this.upload(function(success) {
            Observers.notify("testpilot:task:dataAutoSubmitted", self, null);
	  });
        } else if (this.recurPref == TaskConstants.NEVER_SUBMIT) {
          this._logger.info("Automatically opting out of uploading data");
          this.changeStatus(TaskConstants.STATUS_CANCELLED, true);
          this._dataStore.wipeAllData();
	  setDataDeletionDate = false;
        } else {
          if (Application.prefs.getValue(
              "extensions.testpilot.alwaysSubmitData", false)) {
            this.upload(function(success) {
	      if (success) {
                Observers.notify(
		  "testpilot:task:dataAutoSubmitted", self, null);
	      }
	    });
          }
	}
      } else {
        if (Application.prefs.getValue(
            "extensions.testpilot.alwaysSubmitData", false)) {
          this.upload(function(success) {
	    if (success) {
              Observers.notify("testpilot:task:dataAutoSubmitted", self, null);
	    }
	  });
        }
      }
      if (setDataDeletionDate) {
	let date = this._endDate + TIME_FOR_DATA_DELETION;
        this._dateForDataDeletion = date;
        this._expirationDateForDataSubmission = date;
      } else {
        this._dateForDataDeletion = null;
        this._expirationDateForDataSubmission = null;
      }
    } else {
      // only do this if the state is already finished and the data is expired.
      if (this._status == TaskConstants.STATUS_FINISHED) {
	if (Application.prefs.getValue(
	    "extensions.testpilot.alwaysSubmitData", false)) {
          this.upload(function(success) {
	    if (success) {
              Observers.notify("testpilot:task:dataAutoSubmitted", self, null);
	    }
	  });
        } else if (this._expirationDateForDataSubmission.length > 0) {
	  let expirationDate = Date.parse(this._expirationDateForDataSubmission);
	  if (currentDate > expirationDate) {
            this.changeStatus(TaskConstants.STATUS_CANCELLED, true);
            this._dataStore.wipeAllData();
	    this._dateForDataDeletion = null;
	    // need to keep the expirationDateForDataSubmission value so
	    // we know that data is expired, not user cancels the task.
	  }
	}
      } else if (this._status == TaskConstants.STATUS_SUBMITTED) {
	if (this._dateForDataDeletion.length > 0) {
	  let deleteDate = Date.parse(this._dateForDataDeletion);
	  if (currentDate > deleteDate) {
            this._dataStore.wipeAllData();
	    this._dateForDataDeletion = null;
	  }
	}
      }
    }
  },

  _prependMetadataToJSON: function TestPilotExperiment__prependToJson(callback) {
    let json = {};
    let self = this;
    MetadataCollector.getMetadata(function(md) {
      json.metadata = md;
      json.metadata.task_guid = self.getGuid(self._id);
      json.metadata.event_headers = self._dataStore.getPropertyNames();
      let moreMd = self.getStudyMetadata();
      if (moreMd) {
        for (let i = 0; i < moreMd.length; i++) {
          if (moreMd[i].name && moreMd[i].value) {
            json.metadata[ moreMd[i].name ] = moreMd[i].value; // TODO sanitize strings?
            // TODO handle case where name or value are something other than strings?
          }
        }
      }
      self._dataStore.getJSONRows(function(rows) {
        json.events = rows;
        self._dataStore.getExceptionsAsJson(function(errs) {
          json.exceptions = errs;
          callback( JSON.stringify(json) );
        });
      });
    });
  },

  // Note: When we have multiple experiments running, the uploads
  // are separate files.
  upload: function TestPilotExperiment_upload(callback, retryCount) {
    // Callback is a function that will be called back with true or false
    // on success or failure.

    /* If we've already uploaded, and the user tries to upload again for
     * some reason (they could navigate back to the status.html page,
     * for instance), then proceed without uploading: */
    if (this._status >= TaskConstants.STATUS_SUBMITTED) {
      callback(true);
      return;
    }

    // note the server will reject any upload over 5MB - shouldn't be a problem
    let self = this;
    let url = self.uploadUrl;
    self._logger.info("Posting data to url " + url + "\n");
    self._prependMetadataToJSON( function(dataString) {
      let req = Cc["@mozilla.org/xmlextras/xmlhttprequest;1"]
                  .createInstance( Ci.nsIXMLHttpRequest );
      req.open('POST', url, true);
      req.setRequestHeader("Content-type", "application/json");
      req.setRequestHeader("Content-length", dataString.length);
      req.setRequestHeader("Connection", "close");
      req.addEventListener("readystatechange", function(aEvt) {
        if (req.readyState == 4) {
          if (req.status == 200 || req.status == 201 || req.status == 202) {
            let location = req.getResponseHeader("Location");
  	    self._logger.info("DATA WAS POSTED SUCCESSFULLY " + location);
            if (self._uploadRetryTimer) {
              self._uploadRetryTimer.cancel(); // Stop retrying - it worked!
            }
            self.changeStatus(TaskConstants.STATUS_SUBMITTED);
            self._dateForDataDeletion = self._now() + TIME_FOR_DATA_DELETION;
            self._expirationDateForDataSubmission = null;
            callback(true);
          } else {
          /* If something went wrong with the upload, give up for now,
           * but start a timer to try again later.  Maybe the network will
           * be better by then.  "later" starts at 1 hour, but increases
           * using a random exponential function.  This serves as a backoff
           * in cases where a lot of users are trying to submit data at
           * the same time and the network or server can't handle it.
           */

            // TODO don't retry if status code is 401, 404, or...
            // any others?
            self._logger.warn("ERROR POSTING DATA: " + req.responseText);
            self._uploadRetryTimer = Cc["@mozilla.org/timer;1"]
              .createInstance(Ci.nsITimer);

            if (!retryCount) {
	      retryCount = 0;
            }
            let interval =
	      Application.prefs.getValue(RETRY_INTERVAL_PREF, 3600000); // 1 hour
            let delay =
              parseInt(Math.random() * Math.pow(2, retryCount) * interval);
            self._uploadRetryTimer.initWithCallback(
              { notify: function(timer) {
		self.upload(callback, retryCount++);
	      } }, (interval + delay), Ci.nsITimer.TYPE_ONE_SHOT);
            callback(false);
          }
        }
      }, false);
      req.send(dataString);
    });
  },

  optOut: function TestPilotExperiment_optOut(reason, callback) {
    // Regardless of study ID, post the opt-out message to a special
    // database table of just opt-out messages; include study ID in metadata.
    let logger = this._logger;

    this.onExperimentShutdown();
    this.changeStatus(TaskConstants.STATUS_CANCELLED);
    this._dataStore.wipeAllData();
    this.doExperimentCleanup();
    this._dateForDataDeletion = null;
    this._expirationDateForDataSubmission = null;
    logger.info("Opting out of test with reason " + reason);
    if (reason) {
      // Send us the reason...
      // (TODO: include metadata?)
      let url = Application.prefs.getValue(DATA_UPLOAD_PREF, "") + "opt-out";
      let answer = {id: this._id,
                    reason: reason};
      let dataString = JSON.stringify(answer);
      var req =
        Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
	  createInstance(Ci.nsIXMLHttpRequest);
      logger.trace("Posting " + dataString + " to " + url);
      req.open('POST', url, true);
      req.setRequestHeader("Content-type", "application/json");
      req.setRequestHeader("Content-length", dataString.length);
      req.setRequestHeader("Connection", "close");
      req.addEventListener("readystatechange", function(aEvt) {
        if (req.readyState == 4) {
          if (req.status == 200 || req.status == 201 || req.status == 202) {
	    logger.info("Quit reason posted successfully " + req.responseText);
            if (callback) {
              callback(true);
            }
	  } else {
	    logger.warn(req.status + " posting error " + req.responseText);
            if (callback) {
              callback(false);
            }
	  }
	}
      }, false);
      logger.trace("Sending quit reason.");
      req.send(dataString);
    } else {
      if (callback) {
        callback(false);
      }
    }
  },

  setRecurPref: function TPE_setRecurPrefs(value) {
    // value is NEVER_SUBMIT, ALWAYS_SUBMIT, or ASK_EACH_TIME
    let prefName = RECUR_PREF_PREFIX + this._id;
    this._logger.info("Setting recur pref to " + value);
    Application.prefs.setValue(prefName, value);
  }
};
TestPilotExperiment.prototype.__proto__ = TestPilotTask;

function TestPilotBuiltinSurvey(surveyInfo) {
  this._init(surveyInfo);
}
TestPilotBuiltinSurvey.prototype = {
  _init: function TestPilotBuiltinSurvey__init(surveyInfo) {
    this._taskInit(surveyInfo.surveyId,
                   surveyInfo.surveyName,
                   surveyInfo.surveyUrl,
                   surveyInfo.summary,
                   surveyInfo.thumbnail);
    this._studyId = surveyInfo.uploadWithExperiment; // what study do we belong to
    this._versionNumber = surveyInfo.versionNumber;
    this._questions = surveyInfo.surveyQuestions;
    this._explanation = surveyInfo.surveyExplanation;
    this._onPageLoad = surveyInfo.onPageLoad;
  },

  get taskType() {
    return TaskConstants.TYPE_SURVEY;
  },

  get surveyExplanation() {
    return this._explanation;
  },

  get surveyQuestions() {
    return this._questions;
  },

  get currentStatusUrl() {
    let param = "?eid=" + this._id;
    return "chrome://testpilot/content/take-survey.html" + param;
  },

  get defaultUrl() {
    return this.currentStatusUrl;
  },

  get relatedStudyId() {
    return this._studyId;
  },

  onPageLoad: function(task, document) {
    if (this._onPageLoad) {
      this._onPageLoad(task, document);
    }
  },

  onDetailPageOpened: function TPS_onDetailPageOpened() {
    if (this._status < TaskConstants.STATUS_IN_PROGRESS) {
      this.changeStatus( TaskConstants.STATUS_IN_PROGRESS, true );
    }
  },

  get oldAnswers() {
    let surveyResults =
      Application.prefs.getValue(SURVEY_ANSWER_PREFIX + this._id, null);
    if (!surveyResults) {
      return null;
    } else {
      this._logger.info("Trying to json.parse this: " + surveyResults);
      return sanitizeJSONStrings( JSON.parse(surveyResults) );
    }
  },

  store: function TestPilotSurvey_store(surveyResults, callback) {
    /* Store answers in preferences store; then, upload the survey data
     * if this is a survey with an associated study (the matching GUIDs
     * will be used to associate the two datasets server-side).
     * If it's not one with an associated survey, just store and set status
     * to submitted.  Either way, call the callback with true/false for
     * success/failure.*/
    surveyResults = sanitizeJSONStrings(surveyResults);
    let prefName = SURVEY_ANSWER_PREFIX + this._id;
    // Also store survey version number
    if (this._versionNumber) {
      surveyResults["version_number"] = this._versionNumber;
    }
    Application.prefs.setValue(prefName, JSON.stringify(surveyResults));
    if (this._studyId) {
      this._upload(callback, 0);
    } else {
      this.changeStatus(TaskConstants.STATUS_SUBMITTED);
      callback(true);
    }
  },

  _prependMetadataToJSON: function TestPilotSurvey__prependToJson(callback) {
    let json = {};
    let self = this;
    MetadataCollector.getMetadata(function(md) {
      json.metadata = md;
      if (self._studyId) {
        // Include guid of the study that this survey is related to, so we
        // can match them up server-side.
        json.metadata.task_guid = self.getGuid(self._studyId);
      }
      let pref = SURVEY_ANSWER_PREFIX + self._id;
      let surveyAnswers = JSON.parse(Application.prefs.getValue(pref, "{}"));
      json.survey_data = sanitizeJSONStrings(surveyAnswers);
      callback(JSON.stringify(json));
    });
  },

  // Upload function for survey -- TODO this duplicates a lot of code
  // from study._upload().
  _upload: function TestPilotSurvey__upload(callback, retryCount) {
    let self = this;
    this._prependMetadataToJSON(function(params) {
      let req =
        Cc["@mozilla.org/xmlextras/xmlhttprequest;1"].
          createInstance(Ci.nsIXMLHttpRequest);
      let url = self.uploadUrl;
      req.open("POST", url, true);
      req.setRequestHeader("Content-type", "application/json");
      req.setRequestHeader("Content-length", params.length);
      req.setRequestHeader("Connection", "close");
      req.addEventListener("readystatechange", function(aEvt) {
        if (req.readyState == 4) {
          if (req.status == 200 || req.status == 201 ||
             req.status == 202) {
            self._logger.info(
	    "DATA WAS POSTED SUCCESSFULLY " + req.responseText);
            if (self._uploadRetryTimer) {
              self._uploadRetryTimer.cancel(); // Stop retrying - it worked!
	    }
            self.changeStatus(TaskConstants.STATUS_SUBMITTED);
	    callback(true);
	  } else {
	    self._logger.warn(req.status + " ERROR POSTING DATA: " + req.responseText);
	    self._uploadRetryTimer =
	      Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);

	    if (!retryCount) {
              retryCount = 0;
	    }
	    let interval =
              Application.prefs.getValue(RETRY_INTERVAL_PREF, 3600000); // 1 hour
	    let delay =
	      parseInt(Math.random() * Math.pow(2, retryCount) * interval);
	    self._uploadRetryTimer.initWithCallback(
              { notify: function(timer) {
	          self._upload(callback, retryCount++);
	        } }, (interval + delay), Ci.nsITimer.TYPE_ONE_SHOT);
	    callback(false);
	  }
        }
      }, false);
      req.send(params);
    });
  }
};
TestPilotBuiltinSurvey.prototype.__proto__ = TestPilotTask;

function TestPilotWebSurvey(surveyInfo) {
  this._init(surveyInfo);
}
TestPilotWebSurvey.prototype = {
  _init: function TestPilotWebSurvey__init(surveyInfo) {
    this._taskInit(surveyInfo.surveyId,
                   surveyInfo.surveyName,
                   surveyInfo.surveyUrl,
                   surveyInfo.summary,
                   surveyInfo.thumbnail);
    this._logger.info("Initing survey.  This._status is " + this._status);
  },

  get taskType() {
    return TaskConstants.TYPE_SURVEY;
  },

  get defaultUrl() {
    return this.infoPageUrl;
  },

  onDetailPageOpened: function TPWS_onDetailPageOpened() {
    /* Once you view the URL of the survey, we'll assume you've taken it.
     * There's no reliable way to tell whether you have or not, so let's
     * default to not bugging the user about it again.
     */
    if (this._status < TaskConstants.STATUS_SUBMITTED) {
      this.changeStatus( TaskConstants.STATUS_SUBMITTED, true );
    }
  }
};
TestPilotWebSurvey.prototype.__proto__ = TestPilotTask;


function TestPilotStudyResults(resultsInfo) {
  this._init(resultsInfo);
};
TestPilotStudyResults.prototype = {
  _init: function TestPilotStudyResults__init(resultsInfo) {
    this._taskInit( resultsInfo.id,
                    resultsInfo.title,
                    resultsInfo.url,
                    resultsInfo.summary,
                    resultsInfo.thumbnail);
    this._studyId = resultsInfo.studyId; // what study do we belong to
    this._pubDate = Date.parse(resultsInfo.date);
  },

  get taskType() {
    return TaskConstants.TYPE_RESULTS;
  },

  get publishDate() {
    return this._pubDate;
  },

  get relatedStudyId() {
    return this._studyId;
  }
};
TestPilotStudyResults.prototype.__proto__ = TestPilotTask;

function TestPilotLegacyStudy(studyInfo) {
  this._init(studyInfo);
};
TestPilotLegacyStudy.prototype = {
  _init: function TestPilotLegacyStudy__init(studyInfo) {
    let stat = Application.prefs.getValue(STATUS_PREF_PREFIX + studyInfo.id,
                                          null);
    this._taskInit( studyInfo.id,
                    studyInfo.name,
                    studyInfo.url,
                    studyInfo.summary,
                    studyInfo.thumbnail );

    /* Only three statuses are valid for legacy studies: It must be either
     * canceled, archived (meaning you submitted it), or missed (you never ran it).
     * Set status to one of these.
     */
    switch (stat) {
    case TaskConstants.STATUS_CANCELLED:
    case TaskConstants.STATUS_ARCHIVED:
    case TaskConstants.STATUS_MISSED:
      // Keep that status, so do nothing
      break;
    case TaskConstants.STATUS_SUBMITTED:
      // Change submitted to archived
      this.changeStatus(TaskConstants.STATUS_ARCHIVED, true);
      break;
    default:
      // Anything else means you missed it
      this.changeStatus(TaskConstants.STATUS_MISSED, true);
    }

    if (studyInfo.duration) {
      let prefName = START_DATE_PREF_PREFIX + this._id;
      let startDateString = Application.prefs.getValue(prefName, null);
      if (startDateString) {
        this._startDate = Date.parse(startDateString);
        this._endDate = this._startDate + duration * (24 * 60 * 60 * 1000);
      }
    }
  },

  get taskType() {
    return TaskConstants.TYPE_LEGACY;
  }
  // TODO test that they don't say "thanks for contributing" if the
  // user didn't actually complete them...
};
TestPilotLegacyStudy.prototype.__proto__ = TestPilotTask;
