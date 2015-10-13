/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

'use strict';

// settings.js loads this file when the HUD setting is enabled.

const DEVELOPER_HUD_LOG_PREFIX = 'DeveloperHUD';
const CUSTOM_HISTOGRAM_PREFIX = 'DEVTOOLS_HUD_CUSTOM_';

XPCOMUtils.defineLazyGetter(this, 'devtools', function() {
  const {devtools} = Cu.import('resource://devtools/shared/Loader.jsm', {});
  return devtools;
});

XPCOMUtils.defineLazyGetter(this, 'DebuggerClient', function() {
  return devtools.require('devtools/shared/client/main').DebuggerClient;
});

XPCOMUtils.defineLazyGetter(this, 'WebConsoleUtils', function() {
  return devtools.require('devtools/shared/webconsole/utils').Utils;
});

XPCOMUtils.defineLazyGetter(this, 'EventLoopLagFront', function() {
  return devtools.require('devtools/server/actors/eventlooplag').EventLoopLagFront;
});

XPCOMUtils.defineLazyGetter(this, 'PerformanceEntriesFront', function() {
  return devtools.require('devtools/server/actors/performance-entries').PerformanceEntriesFront;
});

XPCOMUtils.defineLazyGetter(this, 'MemoryFront', function() {
  return devtools.require('devtools/server/actors/memory').MemoryFront;
});

Cu.import('resource://gre/modules/Frames.jsm');

var _telemetryDebug = false;

function telemetryDebug(...args) {
  if (_telemetryDebug) {
    args.unshift('[AdvancedTelemetry]');
    console.log(...args);
  }
}

/**
 * The Developer HUD is an on-device developer tool that displays widgets,
 * showing visual debug information about apps. Each widget corresponds to a
 * metric as tracked by a metric watcher (e.g. consoleWatcher).
 */
var developerHUD = {

  _targets: new Map(),
  _histograms: new Set(),
  _customHistograms: new Set(),
  _client: null,
  _conn: null,
  _watchers: [],
  _logging: true,
  _telemetry: false,

  /**
   * This method registers a metric watcher that will watch one or more metrics
   * on app frames that are being tracked. A watcher must implement the
   * `trackTarget(target)` and `untrackTarget(target)` methods, register
   * observed metrics with `target.register(metric)`, and keep them up-to-date
   * with `target.update(metric, message)` when necessary.
   */
  registerWatcher(watcher) {
    this._watchers.unshift(watcher);
  },

  init() {
    if (this._client) {
      return;
    }

    if (!DebuggerServer.initialized) {
      RemoteDebugger.initServer();
    }

    // We instantiate a local debugger connection so that watchers can use our
    // DebuggerClient to send requests to tab actors (e.g. the consoleActor).
    // Note the special usage of the private _serverConnection, which we need
    // to call connectToChild and set up child process actors on a frame we
    // intend to track. These actors will use the connection to communicate with
    // our DebuggerServer in the parent process.
    let transport = DebuggerServer.connectPipe();
    this._conn = transport._serverConnection;
    this._client = new DebuggerClient(transport);

    for (let w of this._watchers) {
      if (w.init) {
        w.init(this._client);
      }
    }

    Frames.addObserver(this);

    let appFrames = Frames.list().filter(frame => frame.getAttribute('mozapp'));
    for (let frame of appFrames) {
      this.trackFrame(frame);
    }

    SettingsListener.observe('hud.logging', this._logging, enabled => {
      this._logging = enabled;
    });

    SettingsListener.observe('metrics.selectedMetrics.level', "", level => {
      this._telemetry = (level === 'Enhanced');
    });
  },

  uninit() {
    if (!this._client) {
      return;
    }

    for (let frame of this._targets.keys()) {
      this.untrackFrame(frame);
    }

    Frames.removeObserver(this);

    this._client.close();
    delete this._client;
  },

  /**
   * This method will ask all registered watchers to track and update metrics
   * on an app frame.
   */
  trackFrame(frame) {
    if (this._targets.has(frame)) {
      return;
    }

    DebuggerServer.connectToChild(this._conn, frame).then(actor => {
      let target = new Target(frame, actor);
      this._targets.set(frame, target);

      for (let w of this._watchers) {
        w.trackTarget(target);
      }
    });
  },

  untrackFrame(frame) {
    let target = this._targets.get(frame);
    if (target) {
      for (let w of this._watchers) {
        w.untrackTarget(target);
      }

      target.destroy();
      this._targets.delete(frame);
    }
  },

  onFrameCreated(frame, isFirstAppFrame) {
    let mozapp = frame.getAttribute('mozapp');
    if (!mozapp) {
      return;
    }
    this.trackFrame(frame);
  },

  onFrameDestroyed(frame, isLastAppFrame) {
    let mozapp = frame.getAttribute('mozapp');
    if (!mozapp) {
      return;
    }
    this.untrackFrame(frame);
  },

  log(message) {
    if (this._logging) {
      dump(DEVELOPER_HUD_LOG_PREFIX + ': ' + message + '\n');
    }
  }

};


/**
 * A Target object represents all there is to know about a Firefox OS app frame
 * that is being tracked, e.g. a pointer to the frame, current values of watched
 * metrics, and how to notify the front-end when metrics have changed.
 */
function Target(frame, actor) {
  this.frame = frame;
  this.actor = actor;
  this.metrics = new Map();
  this._appName = null;
}

Target.prototype = {

  get manifest() {
    return this.frame.appManifestURL;
  },

  get appName() {

    if (this._appName) {
      return this._appName;
    }

    let manifest = this.manifest;
    if (!manifest) {
      let msg = DEVELOPER_HUD_LOG_PREFIX + ': Unable to determine app for telemetry metric. src: ' +
                this.frame.src;
      console.error(msg);
      return null;
    }

    // "communications" apps are a special case
    if (manifest.indexOf('communications') === -1) {
      let start = manifest.indexOf('/') + 2;
      let end = manifest.indexOf('.', start);
      this._appName = manifest.substring(start, end).toLowerCase();
    } else {
      let src = this.frame.src;
      if (src) {
        // e.g., `app://communications.gaiamobile.org/contacts/index.html`
        let parts = src.split('/');
        let APP = 3;
        let EXPECTED_PARTS_LENGTH = 5;
        if (parts.length === EXPECTED_PARTS_LENGTH) {
          this._appName = parts[APP];
        }
      }
    }

    return this._appName;
  },

  /**
   * Register a metric that can later be updated. Does not update the front-end.
   */
  register(metric) {
    this.metrics.set(metric, 0);
  },

  /**
   * Modify one of a target's metrics, and send out an event to notify relevant
   * parties (e.g. the developer HUD, automated tests, etc).
   */
  update(metric, message) {
    if (!metric.name) {
      throw new Error('Missing metric.name');
    }

    if (!metric.value) {
      metric.value = 0;
    }

    let metrics = this.metrics;
    if (metrics) {
      metrics.set(metric.name, metric.value);
    }

    let data = {
      metrics: [], // FIXME(Bug 982066) Remove this field.
      manifest: this.manifest,
      metric: metric,
      message: message
    };

    // FIXME(Bug 982066) Remove this loop.
    if (metrics && metrics.size > 0) {
      for (let name of metrics.keys()) {
        data.metrics.push({name: name, value: metrics.get(name)});
      }
    }

    if (message) {
      developerHUD.log('[' + data.manifest + '] ' + data.message);
    }

    this._send(data);
  },

  /**
   * Nicer way to call update() when the metric value is a number that needs
   * to be incremented.
   */
  bump(metric, message) {
    metric.value = (this.metrics.get(metric.name) || 0) + 1;
    this.update(metric, message);
  },

  /**
   * Void a metric value and make sure it isn't displayed on the front-end
   * anymore.
   */
  clear(metric) {
    metric.value = 0;
    this.update(metric);
  },

  /**
   * Tear everything down, including the front-end by sending a message without
   * widgets.
   */
  destroy() {
    delete this.metrics;
    this._send({metric: {skipTelemetry: true}});
  },

  _send(data) {
    let frame = this.frame;

    shell.sendEvent(frame, 'developer-hud-update', Cu.cloneInto(data, frame));
    this._logHistogram(data.metric);
  },

  _getAddonHistogram(item) {
    let APPNAME_IDX = 3;
    let HISTNAME_IDX = 4;

    let array = item.split('_');
    let appName = array[APPNAME_IDX].toUpperCase();
    let histName = array[HISTNAME_IDX].toUpperCase();
    return Services.telemetry.getAddonHistogram(appName,
      CUSTOM_HISTOGRAM_PREFIX + histName);
  },

  _clearTelemetryData() {
    developerHUD._histograms.forEach(function(item) {
      Services.telemetry.getKeyedHistogramById(item).clear();
    });

    developerHUD._customHistograms.forEach(item => {
      this._getAddonHistogram(item).clear();
    });
  },

  _sendTelemetryData() {
    if (!developerHUD._telemetry) {
      return;
    }
    telemetryDebug('calling sendTelemetryData');
    let frame = this.frame;
    let payload = {
      keyedHistograms: {},
      addonHistograms: {}
    };
    // Package the hud histograms.
    developerHUD._histograms.forEach(function(item) {
      payload.keyedHistograms[item] =
        Services.telemetry.getKeyedHistogramById(item).snapshot();
    });
    // Package the registered hud custom histograms
    developerHUD._customHistograms.forEach(item => {
      payload.addonHistograms[item] = this._getAddonHistogram(item).snapshot();
    });

    shell.sendEvent(frame, 'advanced-telemetry-update', Cu.cloneInto(payload, frame));
  },

  _logHistogram(metric) {
    if (!developerHUD._telemetry || metric.skipTelemetry) {
      return;
    }

    metric.appName = this.appName;
    if (!metric.appName) {
      return;
    }

    let metricName = metric.name.toUpperCase();
    let metricAppName = metric.appName.toUpperCase();
    if (!metric.custom) {
      let keyedMetricName = 'DEVTOOLS_HUD_' + metricName;
      try {
        let keyed = Services.telemetry.getKeyedHistogramById(keyedMetricName);
        if (keyed) {
          keyed.add(metric.appName, parseInt(metric.value, 10));
          developerHUD._histograms.add(keyedMetricName);
          telemetryDebug(keyedMetricName, metric.value, metric.appName);
        }
      } catch(err) {
        console.error('Histogram error is metricname added to histograms.json:'
          + keyedMetricName);
      }
    } else {
      let histogramName = CUSTOM_HISTOGRAM_PREFIX + metricAppName + '_'
        + metricName;
      // This is a call to add a value to an existing histogram.
      if (typeof metric.value !== 'undefined') {
        Services.telemetry.getAddonHistogram(metricAppName,
          CUSTOM_HISTOGRAM_PREFIX + metricName).add(parseInt(metric.value, 10));
        telemetryDebug(histogramName, metric.value);
        return;
      }

      // The histogram already exists and are not adding data to it.
      if (developerHUD._customHistograms.has(histogramName)) {
        return;
      }

      // This is a call to create a new histogram.
      try {
        let metricType = parseInt(metric.type, 10);
        if (metricType === Services.telemetry.HISTOGRAM_COUNT) {
          Services.telemetry.registerAddonHistogram(metricAppName,
            CUSTOM_HISTOGRAM_PREFIX + metricName, metricType);
        } else {
          Services.telemetry.registerAddonHistogram(metricAppName,
            CUSTOM_HISTOGRAM_PREFIX + metricName, metricType, metric.min,
            metric.max, metric.buckets);
        }
        developerHUD._customHistograms.add(histogramName);
      } catch (err) {
        console.error('Histogram error: ' + err);
      }
    }
  }
};


/**
 * The Console Watcher tracks the following metrics in apps: reflows, warnings,
 * and errors, with security errors reported separately.
 */
var consoleWatcher = {

  _client: null,
  _targets: new Map(),
  _watching: {
    reflows: false,
    warnings: false,
    errors: false,
    security: false
  },
  _security: [
    'Mixed Content Blocker',
    'Mixed Content Message',
    'CSP',
    'Invalid HSTS Headers',
    'Invalid HPKP Headers',
    'Insecure Password Field',
    'SSL',
    'CORS'
  ],

  init(client) {
    this._client = client;
    this.consoleListener = this.consoleListener.bind(this);

    let watching = this._watching;

    for (let key in watching) {
      let metric = key;
      SettingsListener.observe('hud.' + metric, watching[metric], watch => {
        // Watch or unwatch the metric.
        if (watching[metric] = watch) {
          return;
        }

        // If unwatched, remove any existing widgets for that metric.
        for (let target of this._targets.values()) {
          target.clear({name: metric});
        }
      });
    }

    client.addListener('logMessage', this.consoleListener);
    client.addListener('pageError', this.consoleListener);
    client.addListener('consoleAPICall', this.consoleListener);
    client.addListener('reflowActivity', this.consoleListener);
  },

  trackTarget(target) {
    target.register('reflows');
    target.register('warnings');
    target.register('errors');
    target.register('security');

    this._client.request({
      to: target.actor.consoleActor,
      type: 'startListeners',
      listeners: ['LogMessage', 'PageError', 'ConsoleAPI', 'ReflowActivity']
    }, (res) => {
      this._targets.set(target.actor.consoleActor, target);
    });
  },

  untrackTarget(target) {
    this._client.request({
      to: target.actor.consoleActor,
      type: 'stopListeners',
      listeners: ['LogMessage', 'PageError', 'ConsoleAPI', 'ReflowActivity']
    }, (res) => { });

    this._targets.delete(target.actor.consoleActor);
  },

  consoleListener(type, packet) {
    let target = this._targets.get(packet.from);
    let metric = {};
    let output = '';

    switch (packet.type) {

      case 'pageError':
        let pageError = packet.pageError;

        if (pageError.warning || pageError.strict) {
          metric.name = 'warnings';
          output += 'Warning (';
        } else {
          metric.name = 'errors';
          output += 'Error (';
        }

        if (this._security.indexOf(pageError.category) > -1) {
          metric.name = 'security';

          // Telemetry sends the security error category not the
          // count of security errors.
          target._logHistogram({
            name: 'security_category',
            value: pageError.category
          });

          // Indicate that the 'hud' security metric (the count of security
          // errors) should not be sent as a telemetry metric since the
          // security error category is being sent instead.
          metric.skipTelemetry = true;
        }

        let {errorMessage, sourceName, category, lineNumber, columnNumber} = pageError;
        output += category + '): "' + (errorMessage.initial || errorMessage) +
          '" in ' + sourceName + ':' + lineNumber + ':' + columnNumber;
        break;

      case 'consoleAPICall':
        switch (packet.message.level) {

          case 'error':
            metric.name = 'errors';
            output += 'Error (console)';
            break;

          case 'warn':
            metric.name = 'warnings';
            output += 'Warning (console)';
            break;

          case 'info':
            this.handleTelemetryMessage(target, packet);

            // Currently, informational log entries are tracked only by
            // telemetry. Nonetheless, for consistency, we continue here
            // and let the function return normally, when it concludes 'info'
            // entries are not being watched.
            metric.name = 'info';
            break;

          default:
            return;
        }
        break;

      case 'reflowActivity':
        metric.name = 'reflows';

        let {start, end, sourceURL, interruptible} = packet;
        metric.interruptible = interruptible;
        let duration = Math.round((end - start) * 100) / 100;
        output += 'Reflow: ' + duration + 'ms';
        if (sourceURL) {
          output += ' ' + this.formatSourceURL(packet);
        }

        // Telemetry also records reflow duration.
        target._logHistogram({
          name: 'reflow_duration',
          value: Math.round(duration)
        });
        break;

      default:
        return;
    }

    if (developerHUD._telemetry) {
      // Always record telemetry for these metrics.
      if (metric.name === 'errors' || metric.name === 'warnings' || metric.name === 'reflows') {
        let value = target.metrics.get(metric.name);
        metric.value = (value || 0) + 1;
        target._logHistogram(metric);

        // Telemetry has already been recorded.
        metric.skipTelemetry = true;

        // If the metric is not being watched, persist the incremented value.
        // If the metric is being watched, `target.bump` will increment the value
        // of the metric and will persist the incremented value.
        if (!this._watching[metric.name]) {
          target.metrics.set(metric.name, metric.value);
        }
      }
    }

    if (!this._watching[metric.name]) {
      return;
    }

    target.bump(metric, output);
  },

  formatSourceURL(packet) {
    // Abbreviate source URL
    let source = WebConsoleUtils.abbreviateSourceURL(packet.sourceURL);

    // Add function name and line number
    let {functionName, sourceLine} = packet;
    source = 'in ' + (functionName || '<anonymousFunction>') +
      ', ' + source + ':' + sourceLine;

    return source;
  },

  handleTelemetryMessage(target, packet) {
    if (!developerHUD._telemetry) {
      return;
    }

    // If this is a 'telemetry' log entry, create a telemetry metric from
    // the log content.
    let separator = '|';
    let logContent = packet.message.arguments.toString();

    if (logContent.indexOf('telemetry') < 0) {
      return;
    }

    let telemetryData = logContent.split(separator);

    // Positions of the components of a telemetry log entry.
    let TELEMETRY_IDENTIFIER_IDX = 0;
    let NAME_IDX = 1;
    let VALUE_IDX = 2;
    let TYPE_IDX = 2;
    let MIN_IDX = 3;
    let MAX_IDX = 4;
    let BUCKETS_IDX = 5;
    let MAX_CUSTOM_ARGS = 6;
    let MIN_CUSTOM_ARGS = 3;

    if (telemetryData[TELEMETRY_IDENTIFIER_IDX] != 'telemetry' ||
        telemetryData.length < MIN_CUSTOM_ARGS ||
        telemetryData.length > MAX_CUSTOM_ARGS) {
      return;
    }

    let metric = {
      name: telemetryData[NAME_IDX]
    };

    if (metric.name === 'MGMT') {
      metric.value = telemetryData[VALUE_IDX];
      if (metric.value === 'TIMETOSHIP') {
        telemetryDebug('Received a Ship event');
        target._sendTelemetryData();
      } else if (metric.value === 'CLEARMETRICS') {
        target._clearTelemetryData();
      }
    } else {
      if (telemetryData.length === MIN_CUSTOM_ARGS) {
        metric.value = telemetryData[VALUE_IDX];
      } else if (telemetryData.length === MAX_CUSTOM_ARGS) {
        metric.type = telemetryData[TYPE_IDX];
        metric.min = telemetryData[MIN_IDX];
        metric.max = telemetryData[MAX_IDX];
        metric.buckets = telemetryData[BUCKETS_IDX];
      }
      metric.custom = true;
      target._logHistogram(metric);
    }
  }
};
developerHUD.registerWatcher(consoleWatcher);


var eventLoopLagWatcher = {
  _client: null,
  _fronts: new Map(),
  _active: false,

  init(client) {
    this._client = client;

    SettingsListener.observe('hud.jank', false, this.settingsListener.bind(this));
  },

  settingsListener(value) {
    if (this._active == value) {
      return;
    }

    this._active = value;

    // Toggle the state of existing fronts.
    let fronts = this._fronts;
    for (let target of fronts.keys()) {
      if (value) {
        fronts.get(target).start();
      } else {
        fronts.get(target).stop();
        target.clear({name: 'jank'});
      }
    }
  },

  trackTarget(target) {
    target.register('jank');

    let front = new EventLoopLagFront(this._client, target.actor);
    this._fronts.set(target, front);

    front.on('event-loop-lag', time => {
      target.update({name: 'jank', value: time}, 'Jank: ' + time + 'ms');
    });

    if (this._active) {
      front.start();
    }
  },

  untrackTarget(target) {
    let fronts = this._fronts;
    if (fronts.has(target)) {
      fronts.get(target).destroy();
      fronts.delete(target);
    }
  }
};
developerHUD.registerWatcher(eventLoopLagWatcher);

/*
 * The performanceEntriesWatcher determines the delta between the epoch
 * of an app's launch time and the app's performance entry marks.
 * When it receives an "appLaunch" performance entry mark it records the
 * name of the app being launched and the epoch of when the launch ocurred.
 * When it receives subsequent performance entry events for the app being
 * launched, it records the delta of the performance entry opoch compared
 * to the app-launch epoch and emits an "app-start-time-<performance mark name>"
 * event containing the delta.
 */
var performanceEntriesWatcher = {
  _client: null,
  _fronts: new Map(),
  _appLaunchName: null,
  _appLaunchStartTime: null,
  _supported: [
    'contentInteractive',
    'navigationInteractive',
    'navigationLoaded',
    'visuallyLoaded',
    'fullyLoaded',
    'mediaEnumerated',
    'scanEnd'
  ],

  init(client) {
    this._client = client;
    let setting = 'devtools.telemetry.supported_performance_marks';
    let defaultValue = this._supported.join(',');

    SettingsListener.observe(setting, defaultValue, supported => {
      this._supported = supported.split(',');
    });
  },

  trackTarget(target) {
    // The performanceEntries watcher doesn't register a metric because
    // currently the metrics generated are not displayed in
    // in the front-end.

    let front = new PerformanceEntriesFront(this._client, target.actor);
    this._fronts.set(target, front);

    // User timings are always gathered; there is no setting to enable/
    // disable.
    front.start();

    front.on('entry', detail => {

      // Only process performance marks.
      if (detail.type !== 'mark') {
        return;
      }

      let name = detail.name;
      let epoch = detail.epoch;

      // FIXME There is a potential race condition that can result
      // in some performance entries being disregarded. See bug 1189942.
      //
      // If this is an "app launch" mark, record the app that was
      // launched and the epoch of when it was launched.
      if (name.indexOf('appLaunch') !== -1) {
        let CHARS_UNTIL_APP_NAME = 7; // '@app://'
        let startPos = name.indexOf('@app') + CHARS_UNTIL_APP_NAME;
        let endPos = name.indexOf('.');
        this._appLaunchName = name.slice(startPos, endPos);
        this._appLaunchStartTime = epoch;
        return;
      }

      // Only process supported performance marks
      if (this._supported.indexOf(name) === -1) {
        return;
      }

      let origin = detail.origin;
      origin = origin.slice(0, origin.indexOf('.'));

      // Continue if the performance mark corresponds to the app
      // for which we have recorded app launch information.
      if (this._appLaunchName !== origin) {
        return;
      }

      let time = epoch - this._appLaunchStartTime;
      let eventName = 'app_startup_time_' + name;

      // Events based on performance marks are for telemetry only, they are
      // not displayed in the HUD front end.
      target._logHistogram({name: eventName, value: time});

      memoryWatcher.front(target).residentUnique().then(value => {
        eventName = 'app_memory_' + name;
        target._logHistogram({name: eventName, value: value});
      }, err => {
        console.error(err);
      });
    });
  },

  untrackTarget(target) {
    let fronts = this._fronts;
    if (fronts.has(target)) {
      fronts.get(target).destroy();
      fronts.delete(target);
    }
  }
};
developerHUD.registerWatcher(performanceEntriesWatcher);

/**
 * The Memory Watcher uses devtools actors to track memory usage.
 */
var memoryWatcher = {

  _client: null,
  _fronts: new Map(),
  _timers: new Map(),
  _watching: {
    uss: false,
    appmemory: false,
    jsobjects: false,
    jsstrings: false,
    jsother: false,
    dom: false,
    style: false,
    other: false
  },
  _active: false,

  init(client) {
    this._client = client;
    let watching = this._watching;

    for (let key in watching) {
      let category = key;
      SettingsListener.observe('hud.' + category, false, watch => {
        watching[category] = watch;
        this.update();
      });
    }
  },

  update() {
    let watching = this._watching;
    let active = watching.appmemory || watching.uss;

    if (this._active) {
      for (let target of this._fronts.keys()) {
        if (!watching.appmemory) target.clear({name: 'memory'});
        if (!watching.uss) target.clear({name: 'uss'});
        if (!active) clearTimeout(this._timers.get(target));
      }
    } else if (active) {
      for (let target of this._fronts.keys()) {
        this.measure(target);
      }
    }
    this._active = active;
  },

  measure(target) {
    let watch = this._watching;
    let format = this.formatMemory;

    if (watch.uss) {
      this.front(target).residentUnique().then(value => {
        target.update({name: 'uss', value: value}, 'USS: ' + format(value));
      }, err => {
        console.error(err);
      });
    }

    if (watch.appmemory) {
      front.measure().then(data => {
        let total = 0;
        let details = [];

        function item(name, condition, value) {
          if (!condition) {
            return;
          }

          let v = parseInt(value);
          total += v;
          details.push(name + ': ' + format(v));
        }

        item('JS objects', watch.jsobjects, data.jsObjectsSize);
        item('JS strings', watch.jsstrings, data.jsStringsSize);
        item('JS other', watch.jsother, data.jsOtherSize);
        item('DOM', watch.dom, data.domSize);
        item('Style', watch.style, data.styleSize);
        item('Other', watch.other, data.otherSize);
        // TODO Also count images size (bug #976007).

        target.update({name: 'memory', value: total},
          'App Memory: ' + format(total) + ' (' + details.join(', ') + ')');
      }, err => {
        console.error(err);
      });
    }

    let timer = setTimeout(() => this.measure(target), 2000);
    this._timers.set(target, timer);
  },

  formatMemory(bytes) {
    var prefix = ['','K','M','G','T','P','E','Z','Y'];
    var i = 0;
    for (; bytes > 1024 && i < prefix.length; ++i) {
      bytes /= 1024;
    }
    return (Math.round(bytes * 100) / 100) + ' ' + prefix[i] + 'B';
  },

  trackTarget(target) {
    target.register('uss');
    target.register('memory');
    this._fronts.set(target, MemoryFront(this._client, target.actor));
    if (this._active) {
      this.measure(target);
    }
  },

  untrackTarget(target) {
    let front = this._fronts.get(target);
    if (front) {
      front.destroy();
      clearTimeout(this._timers.get(target));
      this._fronts.delete(target);
      this._timers.delete(target);
    }
  },

  front(target) {
    return this._fronts.get(target);
  }
};
developerHUD.registerWatcher(memoryWatcher);
