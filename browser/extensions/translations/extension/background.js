/******/ (() => { // webpackBootstrap
/******/ 	var __webpack_modules__ = ({

/***/ 6789:
/*!*******************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/constants.js ***!
  \*******************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Oo": () => (/* binding */ GLEAN_SCHEMA_VERSION),
/* harmony export */   "Q9": () => (/* binding */ GLEAN_VERSION),
/* harmony export */   "ni": () => (/* binding */ PING_INFO_STORAGE),
/* harmony export */   "xW": () => (/* binding */ CLIENT_INFO_STORAGE),
/* harmony export */   "Ei": () => (/* binding */ KNOWN_CLIENT_ID),
/* harmony export */   "ey": () => (/* binding */ DEFAULT_TELEMETRY_ENDPOINT),
/* harmony export */   "Ui": () => (/* binding */ DELETION_REQUEST_PING_NAME),
/* harmony export */   "og": () => (/* binding */ GLEAN_MAX_SOURCE_TAGS)
/* harmony export */ });
const GLEAN_SCHEMA_VERSION = 1;
const GLEAN_VERSION = "0.14.1";
const PING_INFO_STORAGE = "glean_ping_info";
const CLIENT_INFO_STORAGE = "glean_client_info";
const KNOWN_CLIENT_ID = "c0ffeec0-ffee-c0ff-eec0-ffeec0ffeec0";
const DEFAULT_TELEMETRY_ENDPOINT = "https://incoming.telemetry.mozilla.org";
const DELETION_REQUEST_PING_NAME = "deletion-request";
const GLEAN_MAX_SOURCE_TAGS = 5;


/***/ }),

/***/ 4658:
/*!*****************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/context.js + 1 modules ***!
  \*****************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "_": () => (/* binding */ Context)
});

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/dispatcher.js
var DispatcherState;
(function (DispatcherState) {
    DispatcherState[DispatcherState["Uninitialized"] = 0] = "Uninitialized";
    DispatcherState[DispatcherState["Idle"] = 1] = "Idle";
    DispatcherState[DispatcherState["Processing"] = 2] = "Processing";
    DispatcherState[DispatcherState["Stopped"] = 3] = "Stopped";
})(DispatcherState || (DispatcherState = {}));
var Commands;
(function (Commands) {
    Commands[Commands["Task"] = 0] = "Task";
    Commands[Commands["Stop"] = 1] = "Stop";
    Commands[Commands["Clear"] = 2] = "Clear";
    Commands[Commands["TestTask"] = 3] = "TestTask";
})(Commands || (Commands = {}));
class Dispatcher {
    constructor(maxPreInitQueueSize = 100) {
        this.maxPreInitQueueSize = maxPreInitQueueSize;
        this.queue = [];
        this.state = 0;
    }
    getNextCommand() {
        return this.queue.shift();
    }
    async executeTask(task) {
        try {
            await task();
        }
        catch (e) {
            console.error("Error executing task:", e);
        }
    }
    async execute() {
        let nextCommand = this.getNextCommand();
        while (nextCommand) {
            switch (nextCommand.command) {
                case (1):
                    this.state = 3;
                    return;
                case (2):
                    this.queue.forEach(c => {
                        if (c.command === 3) {
                            c.resolver();
                        }
                    });
                    this.queue = [];
                    this.state = 3;
                    return;
                case (3):
                    await this.executeTask(nextCommand.task);
                    nextCommand.resolver();
                    nextCommand = this.getNextCommand();
                    continue;
                case (0):
                    await this.executeTask(nextCommand.task);
                    nextCommand = this.getNextCommand();
            }
        }
    }
    triggerExecution() {
        if (this.state === 1 && this.queue.length > 0) {
            this.state = 2;
            this.currentJob = this.execute();
            this.currentJob
                .then(() => {
                this.currentJob = undefined;
                if (this.state === 2) {
                    this.state = 1;
                }
            })
                .catch(error => {
                console.error("IMPOSSIBLE: Something went wrong while the dispatcher was executing the tasks queue.", error);
            });
        }
    }
    launchInternal(command, priorityTask = false) {
        if (!priorityTask && this.state === 0) {
            if (this.queue.length >= this.maxPreInitQueueSize) {
                console.warn("Unable to enqueue task, pre init queue is full.");
                return false;
            }
        }
        if (priorityTask) {
            this.queue.unshift(command);
        }
        else {
            this.queue.push(command);
        }
        this.triggerExecution();
        return true;
    }
    launch(task) {
        this.launchInternal({
            task,
            command: 0
        });
    }
    flushInit(task) {
        if (this.state !== 0) {
            console.warn("Attempted to initialize the Dispatcher, but it is already initialized. Ignoring.");
            return;
        }
        if (task) {
            this.launchInternal({
                task,
                command: 0
            }, true);
        }
        this.state = 1;
        this.triggerExecution();
    }
    clear() {
        this.launchInternal({ command: 2 }, true);
        this.resume();
    }
    stop() {
        this.launchInternal({ command: 1 }, true);
    }
    resume() {
        if (this.state === 3) {
            this.state = 1;
            this.triggerExecution();
        }
    }
    async testBlockOnQueue() {
        return this.currentJob && await this.currentJob;
    }
    async testUninitialize() {
        if (this.state === 0) {
            return;
        }
        this.clear();
        await this.testBlockOnQueue();
        this.state = 0;
    }
    testLaunch(task) {
        return new Promise((resolver, reject) => {
            this.resume();
            const wasLaunched = this.launchInternal({
                resolver,
                task,
                command: 3
            });
            if (!wasLaunched) {
                reject();
            }
        });
    }
}
/* harmony default export */ const dispatcher = (Dispatcher);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/context.js

class Context {
    constructor() {
        this._initialized = false;
    }
    static get instance() {
        if (!Context._instance) {
            Context._instance = new Context();
        }
        return Context._instance;
    }
    static async testUninitialize() {
        if (Context.instance._dispatcher) {
            await Context.instance._dispatcher.testUninitialize();
        }
        Context.instance._dispatcher = null;
        Context.initialized = false;
    }
    static get dispatcher() {
        if (!Context.instance._dispatcher) {
            Context.instance._dispatcher = new dispatcher();
        }
        return Context.instance._dispatcher;
    }
    static set dispatcher(dispatcher) {
        Context.instance._dispatcher = dispatcher;
    }
    static get uploadEnabled() {
        return Context.instance._uploadEnabled;
    }
    static set uploadEnabled(upload) {
        Context.instance._uploadEnabled = upload;
    }
    static get metricsDatabase() {
        return Context.instance._metricsDatabase;
    }
    static set metricsDatabase(db) {
        Context.instance._metricsDatabase = db;
    }
    static get eventsDatabase() {
        return Context.instance._eventsDatabase;
    }
    static set eventsDatabase(db) {
        Context.instance._eventsDatabase = db;
    }
    static get pingsDatabase() {
        return Context.instance._pingsDatabase;
    }
    static set pingsDatabase(db) {
        Context.instance._pingsDatabase = db;
    }
    static get errorManager() {
        return Context.instance._errorManager;
    }
    static set errorManager(db) {
        Context.instance._errorManager = db;
    }
    static get applicationId() {
        return Context.instance._applicationId;
    }
    static set applicationId(id) {
        Context.instance._applicationId = id;
    }
    static get initialized() {
        return Context.instance._initialized;
    }
    static set initialized(init) {
        Context.instance._initialized = init;
    }
    static get debugOptions() {
        return Context.instance._debugOptions;
    }
    static set debugOptions(options) {
        Context.instance._debugOptions = options;
    }
}


/***/ }),

/***/ 7864:
/*!**************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/error/error_type.js ***!
  \**************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "N": () => (/* binding */ ErrorType)
/* harmony export */ });
var ErrorType;
(function (ErrorType) {
    ErrorType["InvalidValue"] = "invalid_value";
    ErrorType["InvalidLabel"] = "invalid_label";
    ErrorType["InvalidState"] = "invalid_state";
    ErrorType["InvalidOverflow"] = "invalid_overflow";
})(ErrorType || (ErrorType = {}));


/***/ }),

/***/ 4815:
/*!**********************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/events/index.js ***!
  \**********************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Z": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* unused harmony export CoreEvent */
class CoreEvent {
    constructor(name) {
        this.name = name;
    }
    get registeredPluginIdentifier() {
        var _a;
        return (_a = this.plugin) === null || _a === void 0 ? void 0 : _a.name;
    }
    registerPlugin(plugin) {
        if (this.plugin) {
            console.error(`Attempted to register plugin '${plugin.name}', which listens to the event '${plugin.event}'.`, `That event is already watched by plugin '${this.plugin.name}'`, `Plugin '${plugin.name}' will be ignored.`);
            return;
        }
        this.plugin = plugin;
    }
    deregisterPlugin() {
        this.plugin = undefined;
    }
    trigger(...args) {
        if (this.plugin) {
            return this.plugin.action(...args);
        }
    }
}
const CoreEvents = {
    afterPingCollection: new CoreEvent("afterPingCollection")
};
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (CoreEvents);


/***/ }),

/***/ 9150:
/*!*********************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/events_database.js ***!
  \*********************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "K": () => (/* binding */ RecordedEvent),
/* harmony export */   "Z": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../utils.js */ 6222);

class RecordedEvent {
    constructor(category, name, timestamp, extra) {
        this.category = category;
        this.name = name;
        this.timestamp = timestamp;
        this.extra = extra;
    }
    static toJSONObject(e) {
        return {
            "category": e.category,
            "name": e.name,
            "timestamp": e.timestamp,
            "extra": e.extra,
        };
    }
    static fromJSONObject(e) {
        return new RecordedEvent(e["category"], e["name"], e["timestamp"], e["extra"]);
    }
}
class EventsDatabase {
    constructor(storage) {
        this.eventsStore = new storage("events");
    }
    async record(metric, value) {
        if (metric.disabled) {
            return;
        }
        for (const ping of metric.sendInPings) {
            const transformFn = (v) => {
                var _a;
                const existing = (_a = v) !== null && _a !== void 0 ? _a : [];
                existing.push(RecordedEvent.toJSONObject(value));
                return existing;
            };
            await this.eventsStore.update([ping], transformFn);
        }
    }
    async getEvents(ping, metric) {
        const events = await this.getAndValidatePingData(ping);
        if (events.length === 0) {
            return;
        }
        return events
            .filter((e) => {
            return (e.category === metric.category) && (e.name === metric.name);
        });
    }
    async getAndValidatePingData(ping) {
        const data = await this.eventsStore.get([ping]);
        if ((0,_utils_js__WEBPACK_IMPORTED_MODULE_0__/* .isUndefined */ .o8)(data)) {
            return [];
        }
        if (!Array.isArray(data)) {
            console.error(`Unexpected value found for ping ${ping}: ${JSON.stringify(data)}. Clearing.`);
            await this.eventsStore.delete([ping]);
            return [];
        }
        return data.map((e) => RecordedEvent.fromJSONObject(e));
    }
    async getPingEvents(ping, clearPingLifetimeData) {
        const pingData = await this.getAndValidatePingData(ping);
        if (clearPingLifetimeData) {
            await this.eventsStore.delete([ping]);
        }
        if (pingData.length === 0) {
            return;
        }
        const sortedData = pingData.sort((a, b) => {
            return a.timestamp - b.timestamp;
        });
        const firstTimestamp = sortedData[0].timestamp;
        return sortedData.map((e) => {
            const adjusted = RecordedEvent.toJSONObject(e);
            adjusted["timestamp"] = e.timestamp - firstTimestamp;
            return adjusted;
        });
    }
    async clearAll() {
        await this.eventsStore.delete([]);
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (EventsDatabase);


/***/ }),

/***/ 8284:
/*!***********************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/index.js ***!
  \***********************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "t": () => (/* binding */ MetricType)
/* harmony export */ });
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../utils.js */ 6222);
/* harmony import */ var _types_labeled_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./types/labeled.js */ 3119);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../context.js */ 4658);



class MetricType {
    constructor(type, meta) {
        this.type = type;
        this.name = meta.name;
        this.category = meta.category;
        this.sendInPings = meta.sendInPings;
        this.lifetime = meta.lifetime;
        this.disabled = meta.disabled;
        this.dynamicLabel = meta.dynamicLabel;
    }
    baseIdentifier() {
        if (this.category.length > 0) {
            return `${this.category}.${this.name}`;
        }
        else {
            return this.name;
        }
    }
    async identifier(metricsDatabase) {
        const baseIdentifier = this.baseIdentifier();
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_0__/* .isUndefined */ .o8)(this.dynamicLabel)) {
            return await (0,_types_labeled_js__WEBPACK_IMPORTED_MODULE_1__/* .getValidDynamicLabel */ .qd)(metricsDatabase, this);
        }
        else {
            return baseIdentifier;
        }
    }
    shouldRecord(uploadEnabled) {
        return (uploadEnabled && !this.disabled);
    }
    async testGetNumRecordedErrors(errorType, ping = this.sendInPings[0]) {
        return _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.errorManager.testGetNumRecordedErrors */ ._.errorManager.testGetNumRecordedErrors(this, errorType, ping);
    }
}


/***/ }),

/***/ 519:
/*!************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/metric.js ***!
  \************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "j": () => (/* binding */ Metric)
/* harmony export */ });
class Metric {
    constructor(v) {
        if (!this.validate(v)) {
            throw new Error("Unable to create new Metric instance, values is in unexpected format.");
        }
        this._inner = v;
    }
    get() {
        return this._inner;
    }
    set(v) {
        if (!this.validate(v)) {
            console.error(`Unable to set metric to ${JSON.stringify(v)}. Value is in unexpected format. Ignoring.`);
            return;
        }
        this._inner = v;
    }
}


/***/ }),

/***/ 9696:
/*!***************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/time_unit.js ***!
  \***************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Z": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
var TimeUnit;
(function (TimeUnit) {
    TimeUnit["Nanosecond"] = "nanosecond";
    TimeUnit["Microsecond"] = "microsecond";
    TimeUnit["Millisecond"] = "millisecond";
    TimeUnit["Second"] = "second";
    TimeUnit["Minute"] = "minute";
    TimeUnit["Hour"] = "hour";
    TimeUnit["Day"] = "day";
})(TimeUnit || (TimeUnit = {}));
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (TimeUnit);


/***/ }),

/***/ 4429:
/*!*******************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/counter.js ***!
  \*******************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "CounterMetric": () => (/* binding */ CounterMetric),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../../utils.js */ 6222);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _error_error_type_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../../error/error_type.js */ 7864);





class CounterMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_4__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_1__/* .isInteger */ .U)(v)) {
            return false;
        }
        if (v <= 0) {
            return false;
        }
        return true;
    }
    payload() {
        return this._inner;
    }
}
class CounterMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_0__/* .MetricType */ .t {
    constructor(meta) {
        super("counter", meta);
    }
    static async _private_addUndispatched(instance, amount) {
        if (!instance.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
            return;
        }
        if ((0,_utils_js__WEBPACK_IMPORTED_MODULE_1__/* .isUndefined */ .o8)(amount)) {
            amount = 1;
        }
        if (amount <= 0) {
            await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.errorManager.record */ ._.errorManager.record(instance, _error_error_type_js__WEBPACK_IMPORTED_MODULE_3__/* .ErrorType.InvalidValue */ .N.InvalidValue, `Added negative and zero value ${amount}`);
            return;
        }
        const transformFn = ((amount) => {
            return (v) => {
                let metric;
                let result;
                try {
                    metric = new CounterMetric(v);
                    result = metric.get() + amount;
                }
                catch (_a) {
                    metric = new CounterMetric(amount);
                    result = amount;
                }
                if (result > Number.MAX_SAFE_INTEGER) {
                    result = Number.MAX_SAFE_INTEGER;
                }
                metric.set(result);
                return metric;
            };
        })(amount);
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.transform */ ._.metricsDatabase.transform(instance, transformFn);
    }
    add(amount) {
        _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.launch */ ._.dispatcher.launch(async () => CounterMetricType._private_addUndispatched(this, amount));
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let metric;
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            metric = await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.getMetric */ ._.metricsDatabase.getMetric(ping, this);
        });
        return metric;
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (CounterMetricType);


/***/ }),

/***/ 9303:
/*!********************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/datetime.js ***!
  \********************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "ui": () => (/* binding */ DatetimeMetric),
/* harmony export */   "ZP": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* unused harmony export formatTimezoneOffset */
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../../metrics/time_unit.js */ 9696);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../../utils.js */ 6222);





function formatTimezoneOffset(timezone) {
    const offset = (timezone / 60) * -1;
    const sign = offset > 0 ? "+" : "-";
    const hours = Math.abs(offset).toString().padStart(2, "0");
    return `${sign}${hours}:00`;
}
class DatetimeMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_4__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    static fromDate(v, timeUnit) {
        return new DatetimeMetric({
            timeUnit,
            timezone: v.getTimezoneOffset(),
            date: v.toISOString()
        });
    }
    get date() {
        return new Date(this._inner.date);
    }
    get timezone() {
        return this._inner.timezone;
    }
    get timeUnit() {
        return this._inner.timeUnit;
    }
    get dateISOString() {
        return this._inner.date;
    }
    validate(v) {
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_3__/* .isObject */ .Kn)(v) || Object.keys(v).length !== 3) {
            return false;
        }
        const timeUnitVerification = "timeUnit" in v && (0,_utils_js__WEBPACK_IMPORTED_MODULE_3__/* .isString */ .HD)(v.timeUnit) && Object.values(_metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default */ .Z).includes(v.timeUnit);
        const timezoneVerification = "timezone" in v && (0,_utils_js__WEBPACK_IMPORTED_MODULE_3__/* .isNumber */ .hj)(v.timezone);
        const dateVerification = "date" in v && (0,_utils_js__WEBPACK_IMPORTED_MODULE_3__/* .isString */ .HD)(v.date) && v.date.length === 24 && !isNaN(Date.parse(v.date));
        if (!timeUnitVerification || !timezoneVerification || !dateVerification) {
            return false;
        }
        return true;
    }
    payload() {
        const extractedDateInfo = this.dateISOString.match(/\d+/g);
        if (!extractedDateInfo || extractedDateInfo.length < 0) {
            throw new Error("IMPOSSIBLE: Unable to extract date information from DatetimeMetric.");
        }
        const correctedDate = new Date(parseInt(extractedDateInfo[0]), parseInt(extractedDateInfo[1]) - 1, parseInt(extractedDateInfo[2]), parseInt(extractedDateInfo[3]) - (this.timezone / 60), parseInt(extractedDateInfo[4]), parseInt(extractedDateInfo[5]), parseInt(extractedDateInfo[6]));
        const timezone = formatTimezoneOffset(this.timezone);
        const year = correctedDate.getFullYear().toString().padStart(2, "0");
        const month = (correctedDate.getMonth() + 1).toString().padStart(2, "0");
        const day = correctedDate.getDate().toString().padStart(2, "0");
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Day */ .Z.Day) {
            return `${year}-${month}-${day}${timezone}`;
        }
        const hours = correctedDate.getHours().toString().padStart(2, "0");
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Hour */ .Z.Hour) {
            return `${year}-${month}-${day}T${hours}${timezone}`;
        }
        const minutes = correctedDate.getMinutes().toString().padStart(2, "0");
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Minute */ .Z.Minute) {
            return `${year}-${month}-${day}T${hours}:${minutes}${timezone}`;
        }
        const seconds = correctedDate.getSeconds().toString().padStart(2, "0");
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Second */ .Z.Second) {
            return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}${timezone}`;
        }
        const milliseconds = correctedDate.getMilliseconds().toString().padStart(3, "0");
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Millisecond */ .Z.Millisecond) {
            return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}.${milliseconds}${timezone}`;
        }
        if (this.timeUnit === _metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Microsecond */ .Z.Microsecond) {
            return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}.${milliseconds}000${timezone}`;
        }
        return `${year}-${month}-${day}T${hours}:${minutes}:${seconds}.${milliseconds}000000${timezone}`;
    }
}
class DatetimeMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_0__/* .MetricType */ .t {
    constructor(meta, timeUnit) {
        super("datetime", meta);
        this.timeUnit = timeUnit;
    }
    static async _private_setUndispatched(instance, value) {
        if (!instance.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
            return;
        }
        if (!value) {
            value = new Date();
        }
        const truncatedDate = value;
        switch (instance.timeUnit) {
            case (_metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Day */ .Z.Day):
                truncatedDate.setMilliseconds(0);
                truncatedDate.setSeconds(0);
                truncatedDate.setMinutes(0);
                truncatedDate.setMilliseconds(0);
            case (_metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Hour */ .Z.Hour):
                truncatedDate.setMilliseconds(0);
                truncatedDate.setSeconds(0);
                truncatedDate.setMinutes(0);
            case (_metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Minute */ .Z.Minute):
                truncatedDate.setMilliseconds(0);
                truncatedDate.setSeconds(0);
            case (_metrics_time_unit_js__WEBPACK_IMPORTED_MODULE_1__/* .default.Second */ .Z.Second):
                truncatedDate.setMilliseconds(0);
            default:
                break;
        }
        const metric = DatetimeMetric.fromDate(value, instance.timeUnit);
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.record */ ._.metricsDatabase.record(instance, metric);
    }
    set(value) {
        _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.launch */ ._.dispatcher.launch(() => DatetimeMetricType._private_setUndispatched(this, value));
    }
    async testGetValueAsDatetimeMetric(ping) {
        let value;
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            value = await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.getMetric */ ._.metricsDatabase.getMetric(ping, this);
        });
        if (value) {
            return new DatetimeMetric(value);
        }
    }
    async testGetValueAsString(ping = this.sendInPings[0]) {
        const metric = await this.testGetValueAsDatetimeMetric(ping);
        return metric ? metric.payload() : undefined;
    }
    async testGetValue(ping = this.sendInPings[0]) {
        const metric = await this.testGetValueAsDatetimeMetric(ping);
        return metric ? metric.date : undefined;
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (DatetimeMetricType);


/***/ }),

/***/ 4986:
/*!*****************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/event.js ***!
  \*****************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _events_database_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../events_database.js */ 9150);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../utils.js */ 6222);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../../error/error_type.js */ 7864);





const MAX_LENGTH_EXTRA_KEY_VALUE = 100;
class EventMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_0__/* .MetricType */ .t {
    constructor(meta, allowedExtraKeys) {
        super("event", meta);
        this.allowedExtraKeys = allowedExtraKeys;
    }
    record(extra) {
        _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.launch */ ._.dispatcher.launch(async () => {
            if (!this.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
                return;
            }
            const timestamp = (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .getMonotonicNow */ .ts)();
            let truncatedExtra = undefined;
            if (extra && this.allowedExtraKeys) {
                truncatedExtra = {};
                for (const [name, value] of Object.entries(extra)) {
                    if (this.allowedExtraKeys.includes(name)) {
                        truncatedExtra[name] = await (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .truncateStringAtBoundaryWithError */ .hY)(this, value, MAX_LENGTH_EXTRA_KEY_VALUE);
                    }
                    else {
                        await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(this, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidValue */ .N.InvalidValue, `Invalid key index: ${name}`);
                        continue;
                    }
                }
            }
            const event = new _events_database_js__WEBPACK_IMPORTED_MODULE_1__/* .RecordedEvent */ .K(this.category, this.name, timestamp, truncatedExtra);
            await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.eventsDatabase.record */ ._.eventsDatabase.record(this, event);
        });
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let events;
        await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            events = await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.eventsDatabase.getEvents */ ._.eventsDatabase.getEvents(ping, this);
        });
        return events;
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (EventMetricType);


/***/ }),

/***/ 3119:
/*!*******************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/labeled.js ***!
  \*******************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Dc": () => (/* binding */ LabeledMetric),
/* harmony export */   "lb": () => (/* binding */ combineIdentifierAndLabel),
/* harmony export */   "ho": () => (/* binding */ stripLabel),
/* harmony export */   "qd": () => (/* binding */ getValidDynamicLabel)
/* harmony export */ });
/* unused harmony export OTHER_LABEL */
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _error_error_type_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../../error/error_type.js */ 7864);



class LabeledMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_2__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        return true;
    }
    payload() {
        return this._inner;
    }
}
const MAX_LABELS = 16;
const MAX_LABEL_LENGTH = 61;
const OTHER_LABEL = "__other__";
const LABEL_REGEX = /^[a-z_][a-z0-9_-]{0,29}(\.[a-z_][a-z0-9_-]{0,29})*$/;
function combineIdentifierAndLabel(metricName, label) {
    return `${metricName}/${label}`;
}
function stripLabel(identifier) {
    return identifier.split("/")[0];
}
async function getValidDynamicLabel(metricsDatabase, metric) {
    if (metric.dynamicLabel === undefined) {
        throw new Error("This point should never be reached.");
    }
    const key = combineIdentifierAndLabel(metric.baseIdentifier(), metric.dynamicLabel);
    for (const ping of metric.sendInPings) {
        if (await metricsDatabase.hasMetric(metric.lifetime, ping, metric.type, key)) {
            return key;
        }
    }
    let numUsedKeys = 0;
    for (const ping of metric.sendInPings) {
        numUsedKeys += await metricsDatabase.countByBaseIdentifier(metric.lifetime, ping, metric.type, metric.baseIdentifier());
    }
    let hitError = false;
    if (numUsedKeys >= MAX_LABELS) {
        hitError = true;
    }
    else if (metric.dynamicLabel.length > MAX_LABEL_LENGTH) {
        hitError = true;
        await _context_js__WEBPACK_IMPORTED_MODULE_0__/* .Context.errorManager.record */ ._.errorManager.record(metric, _error_error_type_js__WEBPACK_IMPORTED_MODULE_1__/* .ErrorType.InvalidLabel */ .N.InvalidLabel, `Label length ${metric.dynamicLabel.length} exceeds maximum of ${MAX_LABEL_LENGTH}.`);
    }
    else if (!LABEL_REGEX.test(metric.dynamicLabel)) {
        hitError = true;
        await _context_js__WEBPACK_IMPORTED_MODULE_0__/* .Context.errorManager.record */ ._.errorManager.record(metric, _error_error_type_js__WEBPACK_IMPORTED_MODULE_1__/* .ErrorType.InvalidLabel */ .N.InvalidLabel, `Label must be snake_case, got '${metric.dynamicLabel}'.`);
    }
    return (hitError)
        ? combineIdentifierAndLabel(metric.baseIdentifier(), OTHER_LABEL)
        : key;
}
class LabeledMetricType {
    constructor(meta, submetric, labels) {
        return new Proxy(this, {
            get: (_target, label) => {
                if (labels) {
                    return LabeledMetricType.createFromStaticLabel(meta, submetric, labels, label);
                }
                return LabeledMetricType.createFromDynamicLabel(meta, submetric, label);
            }
        });
    }
    static createFromStaticLabel(meta, submetricClass, allowedLabels, label) {
        const adjustedLabel = allowedLabels.includes(label) ? label : OTHER_LABEL;
        const newMeta = {
            ...meta,
            name: combineIdentifierAndLabel(meta.name, adjustedLabel)
        };
        return new submetricClass(newMeta);
    }
    static createFromDynamicLabel(meta, submetricClass, label) {
        const newMeta = {
            ...meta,
            dynamicLabel: label
        };
        return new submetricClass(newMeta);
    }
}
/* unused harmony default export */ var __WEBPACK_DEFAULT_EXPORT__ = ((/* unused pure expression or super */ null && (LabeledMetricType)));


/***/ }),

/***/ 6120:
/*!********************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/quantity.js ***!
  \********************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "QuantityMetric": () => (/* binding */ QuantityMetric),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../../utils.js */ 6222);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _error_error_type_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../../error/error_type.js */ 7864);





class QuantityMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_4__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_1__/* .isInteger */ .U)(v)) {
            return false;
        }
        if (v < 0) {
            return false;
        }
        return true;
    }
    payload() {
        return this._inner;
    }
}
class QuantityMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_0__/* .MetricType */ .t {
    constructor(meta) {
        super("quantity", meta);
    }
    static async _private_setUndispatched(instance, value) {
        if (!instance.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
            return;
        }
        if (value < 0) {
            await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.errorManager.record */ ._.errorManager.record(instance, _error_error_type_js__WEBPACK_IMPORTED_MODULE_3__/* .ErrorType.InvalidValue */ .N.InvalidValue, `Set negative value ${value}`);
            return;
        }
        if (value > Number.MAX_SAFE_INTEGER) {
            value = Number.MAX_SAFE_INTEGER;
        }
        const metric = new QuantityMetric(value);
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.record */ ._.metricsDatabase.record(instance, metric);
    }
    set(value) {
        _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.launch */ ._.dispatcher.launch(() => QuantityMetricType._private_setUndispatched(this, value));
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let metric;
        await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            metric = await _context_js__WEBPACK_IMPORTED_MODULE_2__/* .Context.metricsDatabase.getMetric */ ._.metricsDatabase.getMetric(ping, this);
        });
        return metric;
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (QuantityMetricType);


/***/ }),

/***/ 5799:
/*!******************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/string.js ***!
  \******************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "MAX_LENGTH_VALUE": () => (/* binding */ MAX_LENGTH_VALUE),
/* harmony export */   "StringMetric": () => (/* binding */ StringMetric),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../utils.js */ 6222);




const MAX_LENGTH_VALUE = 100;
class StringMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_3__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isString */ .HD)(v)) {
            return false;
        }
        if (v.length > MAX_LENGTH_VALUE) {
            return false;
        }
        return true;
    }
    payload() {
        return this._inner;
    }
}
class StringMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_0__/* .MetricType */ .t {
    constructor(meta) {
        super("string", meta);
    }
    static async _private_setUndispatched(instance, value) {
        if (!instance.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_1__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
            return;
        }
        const truncatedValue = await (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .truncateStringAtBoundaryWithError */ .hY)(instance, value, MAX_LENGTH_VALUE);
        const metric = new StringMetric(truncatedValue);
        await _context_js__WEBPACK_IMPORTED_MODULE_1__/* .Context.metricsDatabase.record */ ._.metricsDatabase.record(instance, metric);
    }
    set(value) {
        _context_js__WEBPACK_IMPORTED_MODULE_1__/* .Context.dispatcher.launch */ ._.dispatcher.launch(() => StringMetricType._private_setUndispatched(this, value));
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let metric;
        await _context_js__WEBPACK_IMPORTED_MODULE_1__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            metric = await _context_js__WEBPACK_IMPORTED_MODULE_1__/* .Context.metricsDatabase.getMetric */ ._.metricsDatabase.getMetric(ping, this);
        });
        return metric;
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (StringMetricType);


/***/ }),

/***/ 4498:
/*!********************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/timespan.js ***!
  \********************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "TimespanMetric": () => (/* binding */ TimespanMetric),
/* harmony export */   "default": () => (__WEBPACK_DEFAULT_EXPORT__)
/* harmony export */ });
/* harmony import */ var _time_unit_js__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ../time_unit.js */ 9696);
/* harmony import */ var _index_js__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ../index.js */ 8284);
/* harmony import */ var _utils_js__WEBPACK_IMPORTED_MODULE_2__ = __webpack_require__(/*! ../../utils.js */ 6222);
/* harmony import */ var _metric_js__WEBPACK_IMPORTED_MODULE_5__ = __webpack_require__(/*! ../metric.js */ 519);
/* harmony import */ var _context_js__WEBPACK_IMPORTED_MODULE_3__ = __webpack_require__(/*! ../../context.js */ 4658);
/* harmony import */ var _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__ = __webpack_require__(/*! ../../error/error_type.js */ 7864);






class TimespanMetric extends _metric_js__WEBPACK_IMPORTED_MODULE_5__/* .Metric */ .j {
    constructor(v) {
        super(v);
    }
    get timespan() {
        switch (this._inner.timeUnit) {
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Nanosecond */ .Z.Nanosecond:
                return this._inner.timespan * 10 ** 6;
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Microsecond */ .Z.Microsecond:
                return this._inner.timespan * 10 ** 3;
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Millisecond */ .Z.Millisecond:
                return this._inner.timespan;
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Second */ .Z.Second:
                return Math.round(this._inner.timespan / 1000);
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Minute */ .Z.Minute:
                return Math.round(this._inner.timespan / 1000 / 60);
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Hour */ .Z.Hour:
                return Math.round(this._inner.timespan / 1000 / 60 / 60);
            case _time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default.Day */ .Z.Day:
                return Math.round(this._inner.timespan / 1000 / 60 / 60 / 24);
        }
    }
    validate(v) {
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isObject */ .Kn)(v) || Object.keys(v).length !== 2) {
            return false;
        }
        const timeUnitVerification = "timeUnit" in v && (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isString */ .HD)(v.timeUnit) && Object.values(_time_unit_js__WEBPACK_IMPORTED_MODULE_0__/* .default */ .Z).includes(v.timeUnit);
        const timespanVerification = "timespan" in v && (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isNumber */ .hj)(v.timespan) && v.timespan >= 0;
        if (!timeUnitVerification || !timespanVerification) {
            return false;
        }
        return true;
    }
    payload() {
        return {
            time_unit: this._inner.timeUnit,
            value: this.timespan
        };
    }
}
class TimespanMetricType extends _index_js__WEBPACK_IMPORTED_MODULE_1__/* .MetricType */ .t {
    constructor(meta, timeUnit) {
        super("timespan", meta);
        this.timeUnit = timeUnit;
    }
    static async _private_setRawUndispatched(instance, elapsed) {
        if (!instance.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
            return;
        }
        if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isUndefined */ .o8)(instance.startTime)) {
            await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(instance, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidState */ .N.InvalidState, "Timespan already running. Raw value not recorded.");
            return;
        }
        let reportValueExists = false;
        const transformFn = ((elapsed) => {
            return (old) => {
                let metric;
                try {
                    metric = new TimespanMetric(old);
                    reportValueExists = true;
                }
                catch (_a) {
                    metric = new TimespanMetric({
                        timespan: elapsed,
                        timeUnit: instance.timeUnit,
                    });
                }
                return metric;
            };
        })(elapsed);
        await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.metricsDatabase.transform */ ._.metricsDatabase.transform(instance, transformFn);
        if (reportValueExists) {
            await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(instance, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidState */ .N.InvalidState, "Timespan value already recorded. New value discarded.");
        }
    }
    start() {
        const startTime = (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .getMonotonicNow */ .ts)();
        _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.launch */ ._.dispatcher.launch(async () => {
            if (!this.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
                return;
            }
            if (!(0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isUndefined */ .o8)(this.startTime)) {
                await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(this, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidState */ .N.InvalidState, "Timespan already started");
                return;
            }
            this.startTime = startTime;
            return Promise.resolve();
        });
    }
    stop() {
        const stopTime = (0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .getMonotonicNow */ .ts)();
        _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.launch */ ._.dispatcher.launch(async () => {
            if (!this.shouldRecord(_context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.uploadEnabled */ ._.uploadEnabled)) {
                this.startTime = undefined;
                return;
            }
            if ((0,_utils_js__WEBPACK_IMPORTED_MODULE_2__/* .isUndefined */ .o8)(this.startTime)) {
                await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(this, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidState */ .N.InvalidState, "Timespan not running");
                return;
            }
            const elapsed = stopTime - this.startTime;
            this.startTime = undefined;
            if (elapsed < 0) {
                await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.errorManager.record */ ._.errorManager.record(this, _error_error_type_js__WEBPACK_IMPORTED_MODULE_4__/* .ErrorType.InvalidState */ .N.InvalidState, "Timespan was negative.");
                return;
            }
            await TimespanMetricType._private_setRawUndispatched(this, elapsed);
        });
    }
    cancel() {
        _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.launch */ ._.dispatcher.launch(() => {
            this.startTime = undefined;
            return Promise.resolve();
        });
    }
    setRawNanos(elapsed) {
        _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.launch */ ._.dispatcher.launch(async () => {
            const elapsedMillis = elapsed * 10 ** (-6);
            await TimespanMetricType._private_setRawUndispatched(this, elapsedMillis);
        });
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let value;
        await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.dispatcher.testLaunch */ ._.dispatcher.testLaunch(async () => {
            value = await _context_js__WEBPACK_IMPORTED_MODULE_3__/* .Context.metricsDatabase.getMetric */ ._.metricsDatabase.getMetric(ping, this);
        });
        if (value) {
            return (new TimespanMetric(value)).timespan;
        }
    }
}
/* harmony default export */ const __WEBPACK_DEFAULT_EXPORT__ = (TimespanMetricType);


/***/ }),

/***/ 2455:
/*!*************************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/pings/ping_type.js + 1 modules ***!
  \*************************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "default": () => (/* binding */ ping_type)
});

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/constants.js
var constants = __webpack_require__(6789);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/utils.js + 5 modules
var utils = __webpack_require__(6222);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/counter.js
var counter = __webpack_require__(4429);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/datetime.js
var datetime = __webpack_require__(9303);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/time_unit.js
var time_unit = __webpack_require__(9696);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/events/index.js
var events = __webpack_require__(4815);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/context.js + 1 modules
var context = __webpack_require__(4658);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/pings/maker.js






const GLEAN_START_TIME = new Date();
async function getSequenceNumber(metricsDatabase, ping) {
    const seq = new counter.default({
        category: "",
        name: `${ping.name}#sequence`,
        sendInPings: [constants/* PING_INFO_STORAGE */.ni],
        lifetime: "user",
        disabled: false
    });
    const currentSeqData = await metricsDatabase.getMetric(constants/* PING_INFO_STORAGE */.ni, seq);
    await counter.default._private_addUndispatched(seq, 1);
    if (currentSeqData) {
        try {
            const metric = new counter.CounterMetric(currentSeqData);
            return metric.payload();
        }
        catch (e) {
            console.warn(`Unexpected value found for sequence number in ping ${ping.name}. Ignoring.`);
        }
    }
    return 0;
}
async function getStartEndTimes(metricsDatabase, ping) {
    const start = new datetime/* default */.ZP({
        category: "",
        name: `${ping.name}#start`,
        sendInPings: [constants/* PING_INFO_STORAGE */.ni],
        lifetime: "user",
        disabled: false
    }, time_unit/* default.Minute */.Z.Minute);
    const startTimeData = await metricsDatabase.getMetric(constants/* PING_INFO_STORAGE */.ni, start);
    let startTime;
    if (startTimeData) {
        startTime = new datetime/* DatetimeMetric */.ui(startTimeData);
    }
    else {
        startTime = datetime/* DatetimeMetric.fromDate */.ui.fromDate(GLEAN_START_TIME, time_unit/* default.Minute */.Z.Minute);
    }
    const endTimeData = new Date();
    await datetime/* default._private_setUndispatched */.ZP._private_setUndispatched(start, endTimeData);
    const endTime = datetime/* DatetimeMetric.fromDate */.ui.fromDate(endTimeData, time_unit/* default.Minute */.Z.Minute);
    return {
        startTime: startTime.payload(),
        endTime: endTime.payload()
    };
}
async function buildPingInfoSection(metricsDatabase, ping, reason) {
    const seq = await getSequenceNumber(metricsDatabase, ping);
    const { startTime, endTime } = await getStartEndTimes(metricsDatabase, ping);
    const pingInfo = {
        seq,
        start_time: startTime,
        end_time: endTime
    };
    if (reason) {
        pingInfo.reason = reason;
    }
    return pingInfo;
}
async function buildClientInfoSection(metricsDatabase, ping) {
    let clientInfo = await metricsDatabase.getPingMetrics(constants/* CLIENT_INFO_STORAGE */.xW, true);
    if (!clientInfo) {
        console.warn("Empty client info data. Will submit anyways.");
        clientInfo = {};
    }
    let finalClientInfo = {
        "telemetry_sdk_build": constants/* GLEAN_VERSION */.Q9
    };
    for (const metricType in clientInfo) {
        finalClientInfo = { ...finalClientInfo, ...clientInfo[metricType] };
    }
    if (!ping.includeClientId) {
        delete finalClientInfo["client_id"];
    }
    return finalClientInfo;
}
function getPingHeaders(debugOptions) {
    const headers = {};
    if (debugOptions === null || debugOptions === void 0 ? void 0 : debugOptions.debugViewTag) {
        headers["X-Debug-ID"] = debugOptions.debugViewTag;
    }
    if (debugOptions === null || debugOptions === void 0 ? void 0 : debugOptions.sourceTags) {
        headers["X-Source-Tags"] = debugOptions.sourceTags.toString();
    }
    if (Object.keys(headers).length > 0) {
        return headers;
    }
}
async function collectPing(metricsDatabase, eventsDatabase, ping, reason) {
    const metricsData = await metricsDatabase.getPingMetrics(ping.name, true);
    const eventsData = await eventsDatabase.getPingEvents(ping.name, true);
    if (!metricsData && !eventsData) {
        if (!ping.sendIfEmpty) {
            console.info(`Storage for ${ping.name} empty. Bailing out.`);
            return;
        }
        console.info(`Storage for ${ping.name} empty. Ping will still be sent.`);
    }
    const metrics = metricsData ? { metrics: metricsData } : {};
    const events = eventsData ? { events: eventsData } : {};
    const pingInfo = await buildPingInfoSection(metricsDatabase, ping, reason);
    const clientInfo = await buildClientInfoSection(metricsDatabase, ping);
    return {
        ...metrics,
        ...events,
        ping_info: pingInfo,
        client_info: clientInfo,
    };
}
function makePath(applicationId, identifier, ping) {
    return `/submit/${applicationId}/${ping.name}/${constants/* GLEAN_SCHEMA_VERSION */.Oo}/${identifier}`;
}
async function collectAndStorePing(identifier, ping, reason) {
    const collectedPayload = await collectPing(context/* Context.metricsDatabase */._.metricsDatabase, context/* Context.eventsDatabase */._.eventsDatabase, ping, reason);
    if (!collectedPayload) {
        return;
    }
    let modifiedPayload;
    try {
        modifiedPayload = await events/* default.afterPingCollection.trigger */.Z.afterPingCollection.trigger(collectedPayload);
    }
    catch (e) {
        console.error(`Error while attempting to modify ping payload for the "${ping.name}" ping using`, `the ${JSON.stringify(events/* default.afterPingCollection.registeredPluginIdentifier */.Z.afterPingCollection.registeredPluginIdentifier)} plugin.`, "Ping will not be submitted. See more logs below.\n\n", e);
        return;
    }
    if (context/* Context.debugOptions.logPings */._.debugOptions.logPings) {
        console.info(JSON.stringify(collectedPayload, null, 2));
    }
    const finalPayload = modifiedPayload ? modifiedPayload : collectedPayload;
    const headers = getPingHeaders(context/* Context.debugOptions */._.debugOptions);
    return context/* Context.pingsDatabase.recordPing */._.pingsDatabase.recordPing(makePath(context/* Context.applicationId */._.applicationId, identifier, ping), identifier, finalPayload, headers);
}
/* harmony default export */ const maker = (collectAndStorePing);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/pings/ping_type.js




class PingType {
    constructor(meta) {
        var _a;
        this.name = meta.name;
        this.includeClientId = meta.includeClientId;
        this.sendIfEmpty = meta.sendIfEmpty;
        this.reasonCodes = (_a = meta.reasonCodes) !== null && _a !== void 0 ? _a : [];
    }
    isDeletionRequest() {
        return this.name === constants/* DELETION_REQUEST_PING_NAME */.Ui;
    }
    submit(reason) {
        if (this.testCallback) {
            this.testCallback(reason)
                .then(() => {
                PingType._private_internalSubmit(this, reason, this.resolveTestPromiseFunction);
            })
                .catch(e => {
                console.error(`There was an error validating "${this.name}" (${reason !== null && reason !== void 0 ? reason : "no reason"}):`, e);
                PingType._private_internalSubmit(this, reason, this.rejectTestPromiseFunction);
            });
        }
        else {
            PingType._private_internalSubmit(this, reason);
        }
    }
    static _private_internalSubmit(instance, reason, testResolver) {
        context/* Context.dispatcher.launch */._.dispatcher.launch(async () => {
            if (!context/* Context.initialized */._.initialized) {
                console.info("Glean must be initialized before submitting pings.");
                return;
            }
            if (!context/* Context.uploadEnabled */._.uploadEnabled && !instance.isDeletionRequest()) {
                console.info("Glean disabled: not submitting pings. Glean may still submit the deletion-request ping.");
                return;
            }
            let correctedReason = reason;
            if (reason && !instance.reasonCodes.includes(reason)) {
                console.error(`Invalid reason code ${reason} from ${this.name}. Ignoring.`);
                correctedReason = undefined;
            }
            const identifier = (0,utils/* generateUUIDv4 */.Ln)();
            await maker(identifier, instance, correctedReason);
            if (testResolver) {
                testResolver();
                instance.resolveTestPromiseFunction = undefined;
                instance.rejectTestPromiseFunction = undefined;
                instance.testCallback = undefined;
            }
        });
    }
    async testBeforeNextSubmit(callbackFn) {
        if (this.testCallback) {
            console.error(`There is an existing test call for ping "${this.name}". Ignoring.`);
            return;
        }
        return new Promise((resolve, reject) => {
            this.resolveTestPromiseFunction = resolve;
            this.rejectTestPromiseFunction = reject;
            this.testCallback = callbackFn;
        });
    }
}
/* harmony default export */ const ping_type = (PingType);


/***/ }),

/***/ 6222:
/*!***************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/core/utils.js + 5 modules ***!
  \***************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "Ln": () => (/* binding */ generateUUIDv4),
  "ts": () => (/* binding */ getMonotonicNow),
  "jn": () => (/* binding */ isBoolean),
  "U": () => (/* binding */ isInteger),
  "qT": () => (/* binding */ isJSONValue),
  "hj": () => (/* binding */ isNumber),
  "Kn": () => (/* binding */ isObject),
  "HD": () => (/* binding */ isString),
  "o8": () => (/* binding */ isUndefined),
  "hL": () => (/* binding */ sanitizeApplicationId),
  "hY": () => (/* binding */ truncateStringAtBoundaryWithError),
  "AK": () => (/* binding */ validateHeader),
  "r4": () => (/* binding */ validateURL)
});

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/node_modules/uuid/dist/esm-browser/rng.js
// Unique ID creation requires a high quality random # generator. In the browser we therefore
// require the crypto API and do not support built-in fallback to lower quality random number
// generators (like Math.random()).
var getRandomValues;
var rnds8 = new Uint8Array(16);
function rng() {
  // lazy load so that environments that need to polyfill have a chance to do so
  if (!getRandomValues) {
    // getRandomValues needs to be invoked in a context where "this" is a Crypto implementation. Also,
    // find the complete implementation of crypto (msCrypto) on IE11.
    getRandomValues = typeof crypto !== 'undefined' && crypto.getRandomValues && crypto.getRandomValues.bind(crypto) || typeof msCrypto !== 'undefined' && typeof msCrypto.getRandomValues === 'function' && msCrypto.getRandomValues.bind(msCrypto);

    if (!getRandomValues) {
      throw new Error('crypto.getRandomValues() not supported. See https://github.com/uuidjs/uuid#getrandomvalues-not-supported');
    }
  }

  return getRandomValues(rnds8);
}
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/node_modules/uuid/dist/esm-browser/regex.js
/* harmony default export */ const regex = (/^(?:[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|00000000-0000-0000-0000-000000000000)$/i);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/node_modules/uuid/dist/esm-browser/validate.js


function validate(uuid) {
  return typeof uuid === 'string' && regex.test(uuid);
}

/* harmony default export */ const esm_browser_validate = (validate);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/node_modules/uuid/dist/esm-browser/stringify.js

/**
 * Convert array of 16 byte values to UUID string format of the form:
 * XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
 */

var byteToHex = [];

for (var i = 0; i < 256; ++i) {
  byteToHex.push((i + 0x100).toString(16).substr(1));
}

function stringify(arr) {
  var offset = arguments.length > 1 && arguments[1] !== undefined ? arguments[1] : 0;
  // Note: Be careful editing this code!  It's been tuned for performance
  // and works in ways you may not expect. See https://github.com/uuidjs/uuid/pull/434
  var uuid = (byteToHex[arr[offset + 0]] + byteToHex[arr[offset + 1]] + byteToHex[arr[offset + 2]] + byteToHex[arr[offset + 3]] + '-' + byteToHex[arr[offset + 4]] + byteToHex[arr[offset + 5]] + '-' + byteToHex[arr[offset + 6]] + byteToHex[arr[offset + 7]] + '-' + byteToHex[arr[offset + 8]] + byteToHex[arr[offset + 9]] + '-' + byteToHex[arr[offset + 10]] + byteToHex[arr[offset + 11]] + byteToHex[arr[offset + 12]] + byteToHex[arr[offset + 13]] + byteToHex[arr[offset + 14]] + byteToHex[arr[offset + 15]]).toLowerCase(); // Consistency check for valid UUID.  If this throws, it's likely due to one
  // of the following:
  // - One or more input array values don't map to a hex octet (leading to
  // "undefined" in the uuid)
  // - Invalid input values for the RFC `version` or `variant` fields

  if (!esm_browser_validate(uuid)) {
    throw TypeError('Stringified UUID is invalid');
  }

  return uuid;
}

/* harmony default export */ const esm_browser_stringify = (stringify);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/node_modules/uuid/dist/esm-browser/v4.js



function v4(options, buf, offset) {
  options = options || {};
  var rnds = options.random || (options.rng || rng)(); // Per 4.4, set bits for version and `clock_seq_hi_and_reserved`

  rnds[6] = rnds[6] & 0x0f | 0x40;
  rnds[8] = rnds[8] & 0x3f | 0x80; // Copy bytes to buffer, if provided

  if (buf) {
    offset = offset || 0;

    for (var i = 0; i < 16; ++i) {
      buf[offset + i] = rnds[i];
    }

    return buf;
  }

  return esm_browser_stringify(rnds);
}

/* harmony default export */ const esm_browser_v4 = (v4);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/context.js + 1 modules
var context = __webpack_require__(4658);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/error/error_type.js
var error_type = __webpack_require__(7864);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/utils.js



function isJSONValue(v) {
    if (isString(v) || isBoolean(v) || isNumber(v)) {
        return true;
    }
    if (isObject(v)) {
        if (Object.keys(v).length === 0) {
            return true;
        }
        for (const key in v) {
            return isJSONValue(v[key]);
        }
    }
    if (Array.isArray(v)) {
        return v.every((e) => isJSONValue(e));
    }
    return false;
}
function isObject(v) {
    return (typeof v === "object" && v !== null && v.constructor === Object);
}
function isUndefined(v) {
    return typeof v === "undefined";
}
function isString(v) {
    return typeof v === "string";
}
function isBoolean(v) {
    return typeof v === "boolean";
}
function isNumber(v) {
    return typeof v === "number" && !isNaN(v);
}
function isInteger(v) {
    return isNumber(v) && Number.isInteger(v);
}
function sanitizeApplicationId(applicationId) {
    return applicationId.replace(/[^a-z0-9]+/gi, "-").toLowerCase();
}
function validateURL(v) {
    const urlPattern = /^(http|https):\/\/[a-zA-Z0-9._-]+(:\d+){0,1}(\/{0,1})$/i;
    return urlPattern.test(v);
}
function validateHeader(v) {
    return /^[a-z0-9-]{1,20}$/i.test(v);
}
function generateUUIDv4() {
    if (typeof crypto !== "undefined") {
        return esm_browser_v4();
    }
    else {
        return "xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx".replace(/[xy]/g, function (c) {
            const r = Math.random() * 16 | 0, v = c == "x" ? r : (r & 0x3 | 0x8);
            return v.toString(16);
        });
    }
}
function getMonotonicNow() {
    return typeof performance === "undefined" ? Date.now() : performance.now();
}
async function truncateStringAtBoundaryWithError(metric, value, length) {
    const truncated = value.substr(0, length);
    if (truncated !== value) {
        await context/* Context.errorManager.record */._.errorManager.record(metric, error_type/* ErrorType.InvalidOverflow */.N.InvalidOverflow, `Value length ${value.length} exceeds maximum of ${length}.`);
    }
    return truncated;
}


/***/ }),

/***/ 6902:
/*!******************************************************************************!*\
  !*** ./node_modules/@mozilla/glean/dist/webext/index/webext.js + 20 modules ***!
  \******************************************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "default": () => (/* binding */ index_webext)
});

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/constants.js
var constants = __webpack_require__(6789);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/utils.js + 5 modules
var utils = __webpack_require__(6222);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/config.js


class Configuration {
    constructor(config) {
        this.appBuild = config === null || config === void 0 ? void 0 : config.appBuild;
        this.appDisplayVersion = config === null || config === void 0 ? void 0 : config.appDisplayVersion;
        this.debug = Configuration.sanitizeDebugOptions(config === null || config === void 0 ? void 0 : config.debug);
        if ((config === null || config === void 0 ? void 0 : config.serverEndpoint) && !(0,utils/* validateURL */.r4)(config.serverEndpoint)) {
            throw new Error(`Unable to initialize Glean, serverEndpoint ${config.serverEndpoint} is an invalid URL.`);
        }
        this.serverEndpoint = (config && config.serverEndpoint)
            ? config.serverEndpoint : constants/* DEFAULT_TELEMETRY_ENDPOINT */.ey;
        this.httpClient = config === null || config === void 0 ? void 0 : config.httpClient;
    }
    static sanitizeDebugOptions(debug) {
        const correctedDebugOptions = debug || {};
        if ((debug === null || debug === void 0 ? void 0 : debug.debugViewTag) !== undefined && !Configuration.validateDebugViewTag(debug === null || debug === void 0 ? void 0 : debug.debugViewTag)) {
            delete correctedDebugOptions["debugViewTag"];
        }
        if ((debug === null || debug === void 0 ? void 0 : debug.sourceTags) !== undefined && !Configuration.validateSourceTags(debug === null || debug === void 0 ? void 0 : debug.sourceTags)) {
            delete correctedDebugOptions["sourceTags"];
        }
        return correctedDebugOptions;
    }
    static validateDebugViewTag(tag) {
        const validation = (0,utils/* validateHeader */.AK)(tag);
        if (!validation) {
            console.error(`"${tag}" is not a valid \`debugViewTag\` value.`, "Please make sure the value passed satisfies the regex `^[a-zA-Z0-9-]{1,20}$`.");
        }
        return validation;
    }
    static validateSourceTags(tags) {
        if (tags.length < 1 || tags.length > constants/* GLEAN_MAX_SOURCE_TAGS */.og) {
            console.error(`A list of tags cannot contain more than ${constants/* GLEAN_MAX_SOURCE_TAGS */.og} elements.`);
            return false;
        }
        for (const tag of tags) {
            if (tag.startsWith("glean")) {
                console.error("Tags starting with `glean` are reserved and must not be used.");
                return false;
            }
            if (!(0,utils/* validateHeader */.AK)(tag)) {
                return false;
            }
        }
        return true;
    }
}

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/labeled.js
var labeled = __webpack_require__(3119);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/index.js
var metrics = __webpack_require__(8284);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/context.js + 1 modules
var context = __webpack_require__(4658);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/metric.js
var metric = __webpack_require__(519);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/boolean.js




class BooleanMetric extends metric/* Metric */.j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        return (0,utils/* isBoolean */.jn)(v);
    }
    payload() {
        return this._inner;
    }
}
class BooleanMetricType extends (/* unused pure expression or super */ null && (MetricType)) {
    constructor(meta) {
        super("boolean", meta);
    }
    set(value) {
        Context.dispatcher.launch(async () => {
            if (!this.shouldRecord(Context.uploadEnabled)) {
                return;
            }
            const metric = new BooleanMetric(value);
            await Context.metricsDatabase.record(this, metric);
        });
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let metric;
        await Context.dispatcher.testLaunch(async () => {
            metric = await Context.metricsDatabase.getMetric(ping, this);
        });
        return metric;
    }
}
/* harmony default export */ const types_boolean = ((/* unused pure expression or super */ null && (BooleanMetricType)));

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/counter.js
var counter = __webpack_require__(4429);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/datetime.js
var datetime = __webpack_require__(9303);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/quantity.js
var quantity = __webpack_require__(6120);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/string.js
var string = __webpack_require__(5799);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/timespan.js
var timespan = __webpack_require__(4498);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/error/error_type.js
var error_type = __webpack_require__(7864);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/types/uuid.js





const UUID_REGEX = /^[0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}$/i;
class UUIDMetric extends metric/* Metric */.j {
    constructor(v) {
        super(v);
    }
    validate(v) {
        if (!(0,utils/* isString */.HD)(v)) {
            return false;
        }
        return UUID_REGEX.test(v);
    }
    payload() {
        return this._inner;
    }
}
class UUIDMetricType extends metrics/* MetricType */.t {
    constructor(meta) {
        super("uuid", meta);
    }
    static async _private_setUndispatched(instance, value) {
        if (!instance.shouldRecord(context/* Context.uploadEnabled */._.uploadEnabled)) {
            return;
        }
        if (!value) {
            value = (0,utils/* generateUUIDv4 */.Ln)();
        }
        let metric;
        try {
            metric = new UUIDMetric(value);
        }
        catch (_a) {
            await context/* Context.errorManager.record */._.errorManager.record(instance, error_type/* ErrorType.InvalidValue */.N.InvalidValue, `"${value}" is not a valid UUID.`);
            return;
        }
        await context/* Context.metricsDatabase.record */._.metricsDatabase.record(instance, metric);
    }
    set(value) {
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => UUIDMetricType._private_setUndispatched(this, value));
    }
    generateAndSet() {
        if (!this.shouldRecord(context/* Context.uploadEnabled */._.uploadEnabled)) {
            return;
        }
        const value = (0,utils/* generateUUIDv4 */.Ln)();
        this.set(value);
        return value;
    }
    async testGetValue(ping = this.sendInPings[0]) {
        let metric;
        await context/* Context.dispatcher.testLaunch */._.dispatcher.testLaunch(async () => {
            metric = await context/* Context.metricsDatabase.getMetric */._.metricsDatabase.getMetric(ping, this);
        });
        return metric;
    }
}
/* harmony default export */ const uuid = (UUIDMetricType);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/utils.js








const METRIC_MAP = Object.freeze({
    "boolean": BooleanMetric,
    "counter": counter.CounterMetric,
    "datetime": datetime/* DatetimeMetric */.ui,
    "labeled_boolean": labeled/* LabeledMetric */.Dc,
    "labeled_counter": labeled/* LabeledMetric */.Dc,
    "labeled_string": labeled/* LabeledMetric */.Dc,
    "quantity": quantity.QuantityMetric,
    "string": string.StringMetric,
    "timespan": timespan.TimespanMetric,
    "uuid": UUIDMetric,
});
function createMetric(type, v) {
    if (!(type in METRIC_MAP)) {
        throw new Error(`Unable to create metric of unknown type ${type}`);
    }
    return new METRIC_MAP[type](v);
}
function validateMetricInternalRepresentation(type, v) {
    try {
        createMetric(type, v);
        return true;
    }
    catch (_a) {
        return false;
    }
}

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/database.js


function isValidInternalMetricsRepresentation(v) {
    if ((0,utils/* isObject */.Kn)(v)) {
        for (const metricType in v) {
            const metrics = v[metricType];
            if ((0,utils/* isObject */.Kn)(metrics)) {
                for (const metricIdentifier in metrics) {
                    if (!validateMetricInternalRepresentation(metricType, metrics[metricIdentifier])) {
                        return false;
                    }
                }
            }
            else {
                return false;
            }
        }
        return true;
    }
    else {
        return false;
    }
}
function createMetricsPayload(v) {
    const result = {};
    for (const metricType in v) {
        const metrics = v[metricType];
        result[metricType] = {};
        for (const metricIdentifier in metrics) {
            const metric = createMetric(metricType, metrics[metricIdentifier]);
            result[metricType][metricIdentifier] = metric.payload();
        }
    }
    return result;
}
class MetricsDatabase {
    constructor(storage) {
        this.userStore = new storage("userLifetimeMetrics");
        this.pingStore = new storage("pingLifetimeMetrics");
        this.appStore = new storage("appLifetimeMetrics");
    }
    _chooseStore(lifetime) {
        switch (lifetime) {
            case "user":
                return this.userStore;
            case "ping":
                return this.pingStore;
            case "application":
                return this.appStore;
        }
    }
    async record(metric, value) {
        await this.transform(metric, () => value);
    }
    async transform(metric, transformFn) {
        if (metric.disabled) {
            return;
        }
        const store = this._chooseStore(metric.lifetime);
        const storageKey = await metric.identifier(this);
        for (const ping of metric.sendInPings) {
            const finalTransformFn = (v) => transformFn(v).get();
            await store.update([ping, metric.type, storageKey], finalTransformFn);
        }
    }
    async hasMetric(lifetime, ping, metricType, metricIdentifier) {
        const store = this._chooseStore(lifetime);
        const value = await store.get([ping, metricType, metricIdentifier]);
        return !(0,utils/* isUndefined */.o8)(value);
    }
    async countByBaseIdentifier(lifetime, ping, metricType, metricIdentifier) {
        const store = this._chooseStore(lifetime);
        const pingStorage = await store.get([ping, metricType]);
        if ((0,utils/* isUndefined */.o8)(pingStorage)) {
            return 0;
        }
        return Object.keys(pingStorage).filter(n => n.startsWith(metricIdentifier)).length;
    }
    async getMetric(ping, metric) {
        const store = this._chooseStore(metric.lifetime);
        const storageKey = await metric.identifier(this);
        const value = await store.get([ping, metric.type, storageKey]);
        if (!(0,utils/* isUndefined */.o8)(value) && !validateMetricInternalRepresentation(metric.type, value)) {
            console.error(`Unexpected value found for metric ${storageKey}: ${JSON.stringify(value)}. Clearing.`);
            await store.delete([ping, metric.type, storageKey]);
            return;
        }
        else {
            return value;
        }
    }
    async getAndValidatePingData(ping, lifetime) {
        const store = this._chooseStore(lifetime);
        const data = await store.get([ping]);
        if ((0,utils/* isUndefined */.o8)(data)) {
            return {};
        }
        if (!isValidInternalMetricsRepresentation(data)) {
            console.error(`Unexpected value found for ping ${ping} in ${lifetime} store: ${JSON.stringify(data)}. Clearing.`);
            await store.delete([ping]);
            return {};
        }
        return data;
    }
    processLabeledMetric(snapshot, metricType, metricId, metricData) {
        const newType = `labeled_${metricType}`;
        const idLabelSplit = metricId.split("/", 2);
        const newId = idLabelSplit[0];
        const label = idLabelSplit[1];
        if (newType in snapshot && newId in snapshot[newType]) {
            const existingData = snapshot[newType][newId];
            snapshot[newType][newId] = {
                ...existingData,
                [label]: metricData
            };
        }
        else {
            snapshot[newType] = {
                ...snapshot[newType],
                [newId]: {
                    [label]: metricData
                }
            };
        }
    }
    async getPingMetrics(ping, clearPingLifetimeData) {
        const userData = await this.getAndValidatePingData(ping, "user");
        const pingData = await this.getAndValidatePingData(ping, "ping");
        const appData = await this.getAndValidatePingData(ping, "application");
        if (clearPingLifetimeData) {
            await this.clear("ping", ping);
        }
        const response = {};
        for (const data of [userData, pingData, appData]) {
            for (const metricType in data) {
                for (const metricId in data[metricType]) {
                    if (metricId.includes("/")) {
                        this.processLabeledMetric(response, metricType, metricId, data[metricType][metricId]);
                    }
                    else {
                        response[metricType] = {
                            ...response[metricType],
                            [metricId]: data[metricType][metricId]
                        };
                    }
                }
            }
        }
        if (Object.keys(response).length === 0) {
            return;
        }
        else {
            return createMetricsPayload(response);
        }
    }
    async clear(lifetime, ping) {
        const store = this._chooseStore(lifetime);
        const storageIndex = ping ? [ping] : [];
        await store.delete(storageIndex);
    }
    async clearAll() {
        await this.userStore.delete([]);
        await this.pingStore.delete([]);
        await this.appStore.delete([]);
    }
}
/* harmony default export */ const database = (MetricsDatabase);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/pings/database.js

function isValidPingInternalRepresentation(v) {
    if ((0,utils/* isObject */.Kn)(v) && (Object.keys(v).length === 2 || Object.keys(v).length === 3)) {
        const hasValidPath = "path" in v && (0,utils/* isString */.HD)(v.path);
        const hasValidPayload = "payload" in v && (0,utils/* isJSONValue */.qT)(v.payload) && (0,utils/* isObject */.Kn)(v.payload);
        const hasValidHeaders = (!("headers" in v)) || ((0,utils/* isJSONValue */.qT)(v.headers) && (0,utils/* isObject */.Kn)(v.headers));
        if (!hasValidPath || !hasValidPayload || !hasValidHeaders) {
            return false;
        }
        return true;
    }
    return false;
}
class PingsDatabase {
    constructor(store) {
        this.store = new store("pings");
    }
    attachObserver(observer) {
        this.observer = observer;
    }
    async recordPing(path, identifier, payload, headers) {
        const ping = {
            path,
            payload
        };
        if (headers) {
            ping.headers = headers;
        }
        await this.store.update([identifier], () => ping);
        this.observer && this.observer.update(identifier, ping);
    }
    async deletePing(identifier) {
        await this.store.delete([identifier]);
    }
    async getAllPings() {
        const allStoredPings = await this.store._getWholeStore();
        const finalPings = {};
        for (const identifier in allStoredPings) {
            const ping = allStoredPings[identifier];
            if (isValidPingInternalRepresentation(ping)) {
                finalPings[identifier] = ping;
            }
            else {
                console.warn("Unexpected data found in pings database. Deleting.");
                await this.store.delete([identifier]);
            }
        }
        return finalPings;
    }
    async scanPendingPings() {
        if (!this.observer) {
            return;
        }
        const pings = await this.getAllPings();
        for (const identifier in pings) {
            this.observer.update(identifier, pings[identifier]);
        }
    }
    async clearAll() {
        await this.store.delete([]);
    }
}
/* harmony default export */ const pings_database = (PingsDatabase);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/upload/index.js

var PingUploaderStatus;
(function (PingUploaderStatus) {
    PingUploaderStatus[PingUploaderStatus["Idle"] = 0] = "Idle";
    PingUploaderStatus[PingUploaderStatus["Uploading"] = 1] = "Uploading";
    PingUploaderStatus[PingUploaderStatus["Cancelling"] = 2] = "Cancelling";
})(PingUploaderStatus || (PingUploaderStatus = {}));
class PingUploader {
    constructor(config, platform, pingsDatabase) {
        this.initialized = false;
        this.queue = [];
        this.status = 0;
        this.uploader = config.httpClient ? config.httpClient : platform.uploader;
        this.platformInfo = platform.info;
        this.serverEndpoint = config.serverEndpoint;
        this.pingsDatabase = pingsDatabase;
    }
    setInitialized(state) {
        this.initialized = state !== null && state !== void 0 ? state : true;
    }
    enqueuePing(ping) {
        let isDuplicate = false;
        for (const queuedPing of this.queue) {
            if (queuedPing.identifier === ping.identifier) {
                isDuplicate = true;
            }
        }
        !isDuplicate && this.queue.push(ping);
    }
    getNextPing() {
        return this.queue.shift();
    }
    async preparePingForUpload(ping) {
        const stringifiedBody = JSON.stringify(ping.payload);
        let headers = ping.headers || {};
        headers = {
            ...ping.headers,
            "Content-Type": "application/json; charset=utf-8",
            "Content-Length": stringifiedBody.length.toString(),
            "Date": (new Date()).toISOString(),
            "X-Client-Type": "Glean.js",
            "X-Client-Version": constants/* GLEAN_VERSION */.Q9,
            "X-Telemetry-Agent": `Glean/${constants/* GLEAN_VERSION */.Q9} (JS on ${await this.platformInfo.os()})`
        };
        return {
            headers,
            payload: stringifiedBody
        };
    }
    async attemptPingUpload(ping) {
        if (!this.initialized) {
            console.warn("Attempted to upload a ping, but Glean is not initialized yet. Ignoring.");
            return { result: 0 };
        }
        const finalPing = await this.preparePingForUpload(ping);
        const result = await this.uploader.post(`${this.serverEndpoint}${ping.path}`, finalPing.payload, finalPing.headers);
        return result;
    }
    async processPingUploadResponse(identifier, response) {
        const { status, result } = response;
        if (status && status >= 200 && status < 300) {
            console.info(`Ping ${identifier} succesfully sent ${status}.`);
            await this.pingsDatabase.deletePing(identifier);
            return false;
        }
        if (result === 1 || (status && status >= 400 && status < 500)) {
            console.warn(`Unrecoverable upload failure while attempting to send ping ${identifier}. Error was ${status !== null && status !== void 0 ? status : "no status"}.`);
            await this.pingsDatabase.deletePing(identifier);
            return false;
        }
        console.warn(`Recoverable upload failure while attempting to send ping ${identifier}, will retry. Error was ${status !== null && status !== void 0 ? status : "no status"}.`);
        return true;
    }
    async triggerUploadInternal() {
        let retries = 0;
        let nextPing = this.getNextPing();
        while (nextPing && this.status !== 2) {
            const status = await this.attemptPingUpload(nextPing);
            const shouldRetry = await this.processPingUploadResponse(nextPing.identifier, status);
            if (shouldRetry) {
                retries++;
                this.enqueuePing(nextPing);
            }
            if (retries >= 3) {
                console.info("Reached maximum recoverable failures for the current uploading window. You are done.");
                return;
            }
            nextPing = this.getNextPing();
        }
    }
    async triggerUpload() {
        if (this.status !== 0) {
            return;
        }
        this.status = 1;
        try {
            this.currentJob = this.triggerUploadInternal();
            await this.currentJob;
        }
        finally {
            this.status = 0;
        }
    }
    async cancelUpload() {
        if (this.status === 1) {
            this.status = 2;
            await this.currentJob;
        }
        return;
    }
    async clearPendingPingsQueue() {
        await this.cancelUpload();
        this.queue = [];
    }
    update(identifier, ping) {
        this.enqueuePing({ identifier, ...ping });
        void this.triggerUpload();
    }
}
/* harmony default export */ const upload = (PingUploader);

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/time_unit.js
var time_unit = __webpack_require__(9696);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/internal_metrics.js







class CoreMetrics {
    constructor() {
        this.clientId = new uuid({
            name: "client_id",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "user",
            disabled: false,
        });
        this.firstRunDate = new datetime/* default */.ZP({
            name: "first_run_date",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "user",
            disabled: false,
        }, time_unit/* default.Day */.Z.Day);
        this.os = new string.default({
            name: "os",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
        this.osVersion = new string.default({
            name: "os_version",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
        this.architecture = new string.default({
            name: "architecture",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
        this.locale = new string.default({
            name: "locale",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
        this.appBuild = new string.default({
            name: "app_build",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
        this.appDisplayVersion = new string.default({
            name: "app_display_version",
            category: "",
            sendInPings: ["glean_client_info"],
            lifetime: "application",
            disabled: false,
        });
    }
    async initialize(config, platform, metricsDatabase) {
        await this.initializeClientId(metricsDatabase);
        await this.initializeFirstRunDate(metricsDatabase);
        await string.default._private_setUndispatched(this.os, await platform.info.os());
        await string.default._private_setUndispatched(this.osVersion, await platform.info.osVersion());
        await string.default._private_setUndispatched(this.architecture, await platform.info.arch());
        await string.default._private_setUndispatched(this.locale, await platform.info.locale());
        await string.default._private_setUndispatched(this.appBuild, config.appBuild || "Unknown");
        await string.default._private_setUndispatched(this.appDisplayVersion, config.appDisplayVersion || "Unknown");
    }
    async initializeClientId(metricsDatabase) {
        let needNewClientId = false;
        const clientIdData = await metricsDatabase.getMetric(constants/* CLIENT_INFO_STORAGE */.xW, this.clientId);
        if (clientIdData) {
            try {
                const currentClientId = createMetric("uuid", clientIdData);
                if (currentClientId.payload() === constants/* KNOWN_CLIENT_ID */.Ei) {
                    needNewClientId = true;
                }
            }
            catch (_a) {
                console.warn("Unexpected value found for Glean clientId. Ignoring.");
                needNewClientId = true;
            }
        }
        else {
            needNewClientId = true;
        }
        if (needNewClientId) {
            await uuid._private_setUndispatched(this.clientId, (0,utils/* generateUUIDv4 */.Ln)());
        }
    }
    async initializeFirstRunDate(metricsDatabase) {
        const firstRunDate = await metricsDatabase.getMetric(constants/* CLIENT_INFO_STORAGE */.xW, this.firstRunDate);
        if (!firstRunDate) {
            await datetime/* default._private_setUndispatched */.ZP._private_setUndispatched(this.firstRunDate);
        }
    }
}

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/metrics/events_database.js
var events_database = __webpack_require__(9150);
// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/pings/ping_type.js + 1 modules
var ping_type = __webpack_require__(2455);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/internal_pings.js


class CorePings {
    constructor() {
        this.deletionRequest = new ping_type.default({
            name: constants/* DELETION_REQUEST_PING_NAME */.Ui,
            includeClientId: true,
            sendIfEmpty: true,
        });
    }
}
/* harmony default export */ const internal_pings = (CorePings);

// EXTERNAL MODULE: ./node_modules/@mozilla/glean/dist/webext/core/events/index.js
var events = __webpack_require__(4815);
;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/events/utils.js

function registerPluginToEvent(plugin) {
    const eventName = plugin.event;
    if (eventName in events/* default */.Z) {
        const event = events/* default */.Z[eventName];
        event.registerPlugin(plugin);
        return;
    }
    console.error(`Attempted to register plugin '${plugin.name}', which listens to the event '${plugin.event}'.`, "That is not a valid Glean event. Ignoring");
}
function testResetEvents() {
    for (const event in events/* default */.Z) {
        events/* default */.Z[event].deregisterPlugin();
    }
}

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/error/index.js


function getErrorMetricForMetric(metric, error) {
    const identifier = metric.baseIdentifier();
    const name = (0,labeled/* stripLabel */.ho)(identifier);
    return new counter.default({
        name: (0,labeled/* combineIdentifierAndLabel */.lb)(error, name),
        category: "glean.error",
        lifetime: "ping",
        sendInPings: metric.sendInPings,
        disabled: false,
    });
}
class ErrorManager {
    async record(metric, error, message, numErrors = 1) {
        const errorMetric = getErrorMetricForMetric(metric, error);
        console.warn(`${metric.baseIdentifier()}: ${message}`);
        if (numErrors > 0) {
            await counter.default._private_addUndispatched(errorMetric, numErrors);
        }
        else {
        }
    }
    async testGetNumRecordedErrors(metric, error, ping) {
        const errorMetric = getErrorMetricForMetric(metric, error);
        const numErrors = await errorMetric.testGetValue(ping);
        return numErrors || 0;
    }
}

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/storage/utils.js

function getValueFromNestedObject(obj, index) {
    if (index.length === 0) {
        throw Error("The index must contain at least one property to get.");
    }
    let target = obj;
    for (const key of index) {
        if ((0,utils/* isObject */.Kn)(target) && key in target) {
            const temp = target[key];
            if ((0,utils/* isJSONValue */.qT)(temp)) {
                target = temp;
            }
        }
        else {
            return;
        }
    }
    return target;
}
function updateNestedObject(obj, index, transformFn) {
    if (index.length === 0) {
        throw Error("The index must contain at least one property to update.");
    }
    const returnObject = { ...obj };
    let target = returnObject;
    for (const key of index.slice(0, index.length - 1)) {
        if (!(0,utils/* isObject */.Kn)(target[key])) {
            target[key] = {};
        }
        target = target[key];
    }
    const finalKey = index[index.length - 1];
    const current = target[finalKey];
    try {
        const value = transformFn(current);
        target[finalKey] = value;
        return returnObject;
    }
    catch (e) {
        console.error("Error while transforming stored value. Ignoring old value.", e);
        target[finalKey] = transformFn(undefined);
        return returnObject;
    }
}
function deleteKeyFromNestedObject(obj, index) {
    if (index.length === 0) {
        return {};
    }
    const returnObject = { ...obj };
    let target = returnObject;
    for (const key of index.slice(0, index.length - 1)) {
        const value = target[key];
        if (!(0,utils/* isObject */.Kn)(value)) {
            throw Error("Attempted to delete an entry from an invalid index.");
        }
        else {
            target = value;
        }
    }
    const finalKey = index[index.length - 1];
    delete target[finalKey];
    return returnObject;
}

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/test/storage.js

let globalStore = {};
class MockStore {
    constructor(rootKey) {
        this.rootKey = rootKey;
    }
    _getWholeStore() {
        const result = globalStore[this.rootKey] || {};
        return Promise.resolve(result);
    }
    get(index) {
        try {
            const value = getValueFromNestedObject(globalStore, [this.rootKey, ...index]);
            return Promise.resolve(value);
        }
        catch (e) {
            return Promise.reject(e);
        }
    }
    update(index, transformFn) {
        try {
            globalStore = updateNestedObject(globalStore, [this.rootKey, ...index], transformFn);
            return Promise.resolve();
        }
        catch (e) {
            return Promise.reject(e);
        }
    }
    delete(index) {
        try {
            globalStore = deleteKeyFromNestedObject(globalStore, [this.rootKey, ...index]);
        }
        catch (e) {
            console.warn(e.message, "Ignoring.");
        }
        return Promise.resolve();
    }
}
/* harmony default export */ const storage = (MockStore);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/test/index.js

class MockUploader {
    post(_url, _body, _headers) {
        const result = {
            result: 2,
            status: 200
        };
        return Promise.resolve(result);
    }
}
const MockPlatformInfo = {
    os() {
        return Promise.resolve("Unknown");
    },
    osVersion() {
        return Promise.resolve("Unknown");
    },
    arch() {
        return Promise.resolve("Unknown");
    },
    locale() {
        return Promise.resolve("Unknown");
    },
};
const TestPlatform = {
    Storage: storage,
    uploader: new MockUploader(),
    info: MockPlatformInfo,
    name: "test"
};
/* harmony default export */ const test = (TestPlatform);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/glean.js















class Glean {
    constructor() {
        if (!(0,utils/* isUndefined */.o8)(Glean._instance)) {
            throw new Error(`Tried to instantiate Glean through \`new\`.
      Use Glean.instance instead to access the Glean singleton.`);
        }
        this._coreMetrics = new CoreMetrics();
        this._corePings = new internal_pings();
    }
    static get instance() {
        if (!Glean._instance) {
            Glean._instance = new Glean();
        }
        return Glean._instance;
    }
    static get pingUploader() {
        return Glean.instance._pingUploader;
    }
    static get coreMetrics() {
        return Glean.instance._coreMetrics;
    }
    static get corePings() {
        return Glean.instance._corePings;
    }
    static async onUploadEnabled() {
        context/* Context.uploadEnabled */._.uploadEnabled = true;
        await Glean.coreMetrics.initialize(Glean.instance._config, Glean.platform, context/* Context.metricsDatabase */._.metricsDatabase);
    }
    static async onUploadDisabled() {
        context/* Context.uploadEnabled */._.uploadEnabled = false;
        await Glean.clearMetrics();
        Glean.corePings.deletionRequest.submit();
    }
    static async clearMetrics() {
        await Glean.pingUploader.clearPendingPingsQueue();
        let firstRunDate;
        try {
            firstRunDate = new datetime/* DatetimeMetric */.ui(await context/* Context.metricsDatabase.getMetric */._.metricsDatabase.getMetric(constants/* CLIENT_INFO_STORAGE */.xW, Glean.coreMetrics.firstRunDate)).date;
        }
        catch (_a) {
            firstRunDate = new Date();
        }
        await context/* Context.eventsDatabase.clearAll */._.eventsDatabase.clearAll();
        await context/* Context.metricsDatabase.clearAll */._.metricsDatabase.clearAll();
        await context/* Context.pingsDatabase.clearAll */._.pingsDatabase.clearAll();
        context/* Context.uploadEnabled */._.uploadEnabled = true;
        await uuid._private_setUndispatched(Glean.coreMetrics.clientId, constants/* KNOWN_CLIENT_ID */.Ei);
        await datetime/* default._private_setUndispatched */.ZP._private_setUndispatched(Glean.coreMetrics.firstRunDate, firstRunDate);
        context/* Context.uploadEnabled */._.uploadEnabled = false;
    }
    static initialize(applicationId, uploadEnabled, config) {
        if (context/* Context.initialized */._.initialized) {
            console.warn("Attempted to initialize Glean, but it has already been initialized. Ignoring.");
            return;
        }
        if (applicationId.length === 0) {
            console.error("Unable to initialize Glean, applicationId cannot be an empty string.");
            return;
        }
        if (!Glean.instance._platform) {
            console.error("Unable to initialize Glean, environment has not been set.");
            return;
        }
        const correctConfig = new Configuration(config);
        context/* Context.metricsDatabase */._.metricsDatabase = new database(Glean.platform.Storage);
        context/* Context.eventsDatabase */._.eventsDatabase = new events_database/* default */.Z(Glean.platform.Storage);
        context/* Context.pingsDatabase */._.pingsDatabase = new pings_database(Glean.platform.Storage);
        context/* Context.errorManager */._.errorManager = new ErrorManager();
        Glean.instance._pingUploader = new upload(correctConfig, Glean.platform, context/* Context.pingsDatabase */._.pingsDatabase);
        context/* Context.pingsDatabase.attachObserver */._.pingsDatabase.attachObserver(Glean.pingUploader);
        if (config === null || config === void 0 ? void 0 : config.plugins) {
            for (const plugin of config.plugins) {
                registerPluginToEvent(plugin);
            }
        }
        context/* Context.dispatcher.flushInit */._.dispatcher.flushInit(async () => {
            context/* Context.applicationId */._.applicationId = (0,utils/* sanitizeApplicationId */.hL)(applicationId);
            context/* Context.debugOptions */._.debugOptions = correctConfig.debug;
            Glean.instance._config = correctConfig;
            await context/* Context.metricsDatabase.clear */._.metricsDatabase.clear("application");
            context/* Context.initialized */._.initialized = true;
            Glean.pingUploader.setInitialized(true);
            if (uploadEnabled) {
                await Glean.onUploadEnabled();
            }
            else {
                const clientId = await context/* Context.metricsDatabase.getMetric */._.metricsDatabase.getMetric(constants/* CLIENT_INFO_STORAGE */.xW, Glean.coreMetrics.clientId);
                if (clientId) {
                    if (clientId !== constants/* KNOWN_CLIENT_ID */.Ei) {
                        await Glean.onUploadDisabled();
                    }
                }
                else {
                    await Glean.clearMetrics();
                }
            }
            await context/* Context.pingsDatabase.scanPendingPings */._.pingsDatabase.scanPendingPings();
            void Glean.pingUploader.triggerUpload();
        });
    }
    static get serverEndpoint() {
        var _a;
        return (_a = Glean.instance._config) === null || _a === void 0 ? void 0 : _a.serverEndpoint;
    }
    static get logPings() {
        var _a, _b;
        return ((_b = (_a = Glean.instance._config) === null || _a === void 0 ? void 0 : _a.debug) === null || _b === void 0 ? void 0 : _b.logPings) || false;
    }
    static get debugViewTag() {
        var _a;
        return (_a = Glean.instance._config) === null || _a === void 0 ? void 0 : _a.debug.debugViewTag;
    }
    static get sourceTags() {
        var _a, _b;
        return (_b = (_a = Glean.instance._config) === null || _a === void 0 ? void 0 : _a.debug.sourceTags) === null || _b === void 0 ? void 0 : _b.toString();
    }
    static get platform() {
        if (!Glean.instance._platform) {
            throw new Error("IMPOSSIBLE: Attempted to access environment specific APIs before Glean was initialized.");
        }
        return Glean.instance._platform;
    }
    static setUploadEnabled(flag) {
        context/* Context.dispatcher.launch */._.dispatcher.launch(async () => {
            if (!context/* Context.initialized */._.initialized) {
                console.error("Changing upload enabled before Glean is initialized is not supported.\n", "Pass the correct state into `Glean.initialize\n`.", "See documentation at https://mozilla.github.io/glean/book/user/general-api.html#initializing-the-glean-sdk`");
                return;
            }
            if (context/* Context.uploadEnabled */._.uploadEnabled !== flag) {
                if (flag) {
                    await Glean.onUploadEnabled();
                }
                else {
                    await Glean.onUploadDisabled();
                }
            }
        });
    }
    static setLogPings(flag) {
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => {
            Glean.instance._config.debug.logPings = flag;
            return Promise.resolve();
        });
    }
    static setDebugViewTag(value) {
        if (!Configuration.validateDebugViewTag(value)) {
            console.error(`Invalid \`debugViewTag\` ${value}. Ignoring.`);
            return;
        }
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => {
            Glean.instance._config.debug.debugViewTag = value;
            return Promise.resolve();
        });
    }
    static unsetDebugViewTag() {
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => {
            delete Glean.instance._config.debug.debugViewTag;
            return Promise.resolve();
        });
    }
    static setSourceTags(value) {
        if (!Configuration.validateSourceTags(value)) {
            console.error(`Invalid \`sourceTags\` ${value.toString()}. Ignoring.`);
            return;
        }
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => {
            Glean.instance._config.debug.sourceTags = value;
            return Promise.resolve();
        });
    }
    static unsetSourceTags() {
        context/* Context.dispatcher.launch */._.dispatcher.launch(() => {
            delete Glean.instance._config.debug.sourceTags;
            return Promise.resolve();
        });
    }
    static setPlatform(platform) {
        if (context/* Context.initialized */._.initialized) {
            return;
        }
        if (Glean.instance._platform && Glean.instance._platform.name !== platform.name) {
            console.debug(`Changing Glean platform from "${Glean.platform.name}" to "${platform.name}".`);
        }
        Glean.instance._platform = platform;
    }
    static async testInitialize(applicationId, uploadEnabled = true, config) {
        Glean.setPlatform(test);
        Glean.initialize(applicationId, uploadEnabled, config);
        await context/* Context.dispatcher.testBlockOnQueue */._.dispatcher.testBlockOnQueue();
    }
    static async testUninitialize() {
        await context/* Context.testUninitialize */._.testUninitialize();
        testResetEvents();
        if (Glean.pingUploader) {
            await Glean.pingUploader.clearPendingPingsQueue();
        }
    }
    static async testResetGlean(applicationId, uploadEnabled = true, config) {
        await Glean.testUninitialize();
        try {
            await context/* Context.eventsDatabase.clearAll */._.eventsDatabase.clearAll();
            await context/* Context.metricsDatabase.clearAll */._.metricsDatabase.clearAll();
            await context/* Context.pingsDatabase.clearAll */._.pingsDatabase.clearAll();
        }
        catch (_a) {
        }
        await Glean.testInitialize(applicationId, uploadEnabled, config);
    }
}
/* harmony default export */ const glean = (Glean);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/webext/storage.js


function stripNulls(query) {
    for (const key in query) {
        const curr = query[key];
        if (curr === null) {
            delete query[key];
        }
        if ((0,utils/* isObject */.Kn)(curr)) {
            if (Object.keys(curr).length === 0) {
                delete query[key];
            }
            else {
                stripNulls(curr);
            }
        }
    }
}
class WebExtStore {
    constructor(rootKey) {
        if (typeof browser === "undefined") {
            throw Error(`The web extensions store should only be user in a browser extension context.
        If running is a browser different from Firefox, make sure you have installed
        the webextension-polyfill peer dependency. To do so, run \`npm i webextension-polyfill\`.`);
        }
        this.store = browser.storage.local;
        this.rootKey = rootKey;
    }
    async _getWholeStore() {
        const result = await this.store.get({ [this.rootKey]: {} });
        return result[this.rootKey];
    }
    _buildQuery(index) {
        let query = null;
        for (const key of [this.rootKey, ...index].reverse()) {
            query = { [key]: query };
        }
        return query;
    }
    async _buildQueryFromStore(transformFn) {
        const store = await this._getWholeStore();
        return { [this.rootKey]: transformFn(store) };
    }
    async get(index) {
        const query = this._buildQuery(index);
        const response = await this.store.get(query);
        stripNulls(response);
        if (!response) {
            return;
        }
        if ((0,utils/* isJSONValue */.qT)(response)) {
            if ((0,utils/* isObject */.Kn)(response)) {
                return getValueFromNestedObject(response, [this.rootKey, ...index]);
            }
            else {
                return response;
            }
        }
        console.warn(`Unexpected value found in storage for index ${JSON.stringify(index)}. Ignoring.
      ${JSON.stringify(response, null, 2)}`);
    }
    async update(index, transformFn) {
        if (index.length === 0) {
            throw Error("The index must contain at least one property to update.");
        }
        const query = await this._buildQueryFromStore(store => updateNestedObject(store, index, transformFn));
        return this.store.set(query);
    }
    async delete(index) {
        try {
            const query = await this._buildQueryFromStore(store => deleteKeyFromNestedObject(store, index));
            return this.store.set(query);
        }
        catch (e) {
            console.warn("Ignoring key", e);
        }
    }
}
/* harmony default export */ const webext_storage = (WebExtStore);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/core/upload/uploader.js
const DEFAULT_UPLOAD_TIMEOUT_MS = 10000;
var UploadResultStatus;
(function (UploadResultStatus) {
    UploadResultStatus[UploadResultStatus["RecoverableFailure"] = 0] = "RecoverableFailure";
    UploadResultStatus[UploadResultStatus["UnrecoverableFailure"] = 1] = "UnrecoverableFailure";
    UploadResultStatus[UploadResultStatus["Success"] = 2] = "Success";
})(UploadResultStatus || (UploadResultStatus = {}));

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/webext/uploader.js

class BrowserUploader {
    async post(url, body, headers = {}) {
        const controller = new AbortController();
        const timeout = setTimeout(() => controller.abort(), DEFAULT_UPLOAD_TIMEOUT_MS);
        let response;
        try {
            response = await fetch(url.toString(), {
                headers,
                method: "POST",
                body: body,
                keepalive: true,
                credentials: "omit",
                signal: controller.signal,
                redirect: "error",
            });
        }
        catch (e) {
            if (e instanceof DOMException) {
                console.error("Timeout while attempting to upload ping.", e);
            }
            else if (e instanceof TypeError) {
                console.error("Network while attempting to upload ping.", e);
            }
            else {
                console.error("Unknown error while attempting to upload ping.", e);
            }
            clearTimeout(timeout);
            return { result: 0 };
        }
        clearTimeout(timeout);
        return {
            result: 2,
            status: response.status
        };
    }
}
/* harmony default export */ const uploader = (new BrowserUploader());

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/webext/platform_info.js
const WebExtPlatformInfo = {
    async os() {
        const platformInfo = await browser.runtime.getPlatformInfo();
        switch (platformInfo.os) {
            case "mac":
                return "Darwin";
            case "win":
                return "Windows";
            case "android":
                return "Android";
            case "cros":
                return "ChromeOS";
            case "linux":
                return "Linux";
            case "openbsd":
                return "OpenBSD";
            default:
                return "Unknown";
        }
    },
    async osVersion() {
        return Promise.resolve("Unknown");
    },
    async arch() {
        const platformInfo = await browser.runtime.getPlatformInfo();
        return platformInfo.arch;
    },
    async locale() {
        return Promise.resolve(navigator.language || "und");
    }
};
/* harmony default export */ const platform_info = (WebExtPlatformInfo);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/platform/webext/index.js



const WebExtPlatform = {
    Storage: webext_storage,
    uploader: uploader,
    info: platform_info,
    name: "webext"
};
/* harmony default export */ const webext = (WebExtPlatform);

;// CONCATENATED MODULE: ./node_modules/@mozilla/glean/dist/webext/index/webext.js


/* harmony default export */ const index_webext = ({
    initialize(applicationId, uploadEnabled, config) {
        glean.setPlatform(webext);
        glean.initialize(applicationId, uploadEnabled, config);
    },
    setUploadEnabled(flag) {
        glean.setUploadEnabled(flag);
    },
    setLogPings(flag) {
        glean.setLogPings(flag);
    },
    setDebugViewTag(value) {
        glean.setDebugViewTag(value);
    },
    unsetDebugViewTag() {
        glean.unsetDebugViewTag();
    },
    setSourceTags(value) {
        glean.setSourceTags(value);
    },
    unsetSourceTags() {
        glean.unsetSourceTags();
    },
    async testResetGlean(applicationId, uploadEnabled = true, config) {
        return glean.testResetGlean(applicationId, uploadEnabled, config);
    }
});


/***/ }),

/***/ 8313:
/*!****************************************!*\
  !*** ./node_modules/base-64/base64.js ***!
  \****************************************/
/***/ (function(module, exports, __webpack_require__) {

/* module decorator */ module = __webpack_require__.nmd(module);
var __WEBPACK_AMD_DEFINE_RESULT__;/*! http://mths.be/base64 v0.1.0 by @mathias | MIT license */
;(function(root) {

	// Detect free variables `exports`.
	var freeExports =  true && exports;

	// Detect free variable `module`.
	var freeModule =  true && module &&
		module.exports == freeExports && module;

	// Detect free variable `global`, from Node.js or Browserified code, and use
	// it as `root`.
	var freeGlobal = typeof __webpack_require__.g == 'object' && __webpack_require__.g;
	if (freeGlobal.global === freeGlobal || freeGlobal.window === freeGlobal) {
		root = freeGlobal;
	}

	/*--------------------------------------------------------------------------*/

	var InvalidCharacterError = function(message) {
		this.message = message;
	};
	InvalidCharacterError.prototype = new Error;
	InvalidCharacterError.prototype.name = 'InvalidCharacterError';

	var error = function(message) {
		// Note: the error messages used throughout this file match those used by
		// the native `atob`/`btoa` implementation in Chromium.
		throw new InvalidCharacterError(message);
	};

	var TABLE = 'ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/';
	// http://whatwg.org/html/common-microsyntaxes.html#space-character
	var REGEX_SPACE_CHARACTERS = /[\t\n\f\r ]/g;

	// `decode` is designed to be fully compatible with `atob` as described in the
	// HTML Standard. http://whatwg.org/html/webappapis.html#dom-windowbase64-atob
	// The optimized base64-decoding algorithm used is based on @atk’s excellent
	// implementation. https://gist.github.com/atk/1020396
	var decode = function(input) {
		input = String(input)
			.replace(REGEX_SPACE_CHARACTERS, '');
		var length = input.length;
		if (length % 4 == 0) {
			input = input.replace(/==?$/, '');
			length = input.length;
		}
		if (
			length % 4 == 1 ||
			// http://whatwg.org/C#alphanumeric-ascii-characters
			/[^+a-zA-Z0-9/]/.test(input)
		) {
			error(
				'Invalid character: the string to be decoded is not correctly encoded.'
			);
		}
		var bitCounter = 0;
		var bitStorage;
		var buffer;
		var output = '';
		var position = -1;
		while (++position < length) {
			buffer = TABLE.indexOf(input.charAt(position));
			bitStorage = bitCounter % 4 ? bitStorage * 64 + buffer : buffer;
			// Unless this is the first of a group of 4 characters…
			if (bitCounter++ % 4) {
				// …convert the first 8 bits to a single ASCII character.
				output += String.fromCharCode(
					0xFF & bitStorage >> (-2 * bitCounter & 6)
				);
			}
		}
		return output;
	};

	// `encode` is designed to be fully compatible with `btoa` as described in the
	// HTML Standard: http://whatwg.org/html/webappapis.html#dom-windowbase64-btoa
	var encode = function(input) {
		input = String(input);
		if (/[^\0-\xFF]/.test(input)) {
			// Note: no need to special-case astral symbols here, as surrogates are
			// matched, and the input is supposed to only contain ASCII anyway.
			error(
				'The string to be encoded contains characters outside of the ' +
				'Latin1 range.'
			);
		}
		var padding = input.length % 3;
		var output = '';
		var position = -1;
		var a;
		var b;
		var c;
		var d;
		var buffer;
		// Make sure any padding is handled outside of the loop.
		var length = input.length - padding;

		while (++position < length) {
			// Read three bytes, i.e. 24 bits.
			a = input.charCodeAt(position) << 16;
			b = input.charCodeAt(++position) << 8;
			c = input.charCodeAt(++position);
			buffer = a + b + c;
			// Turn the 24 bits into four chunks of 6 bits each, and append the
			// matching character for each of them to the output.
			output += (
				TABLE.charAt(buffer >> 18 & 0x3F) +
				TABLE.charAt(buffer >> 12 & 0x3F) +
				TABLE.charAt(buffer >> 6 & 0x3F) +
				TABLE.charAt(buffer & 0x3F)
			);
		}

		if (padding == 2) {
			a = input.charCodeAt(position) << 8;
			b = input.charCodeAt(++position);
			buffer = a + b;
			output += (
				TABLE.charAt(buffer >> 10) +
				TABLE.charAt((buffer >> 4) & 0x3F) +
				TABLE.charAt((buffer << 2) & 0x3F) +
				'='
			);
		} else if (padding == 1) {
			buffer = input.charCodeAt(position);
			output += (
				TABLE.charAt(buffer >> 2) +
				TABLE.charAt((buffer << 4) & 0x3F) +
				'=='
			);
		}

		return output;
	};

	var base64 = {
		'encode': encode,
		'decode': decode,
		'version': '0.1.0'
	};

	// Some AMD build optimizers, like r.js, check for specific condition patterns
	// like the following:
	if (
		true
	) {
		!(__WEBPACK_AMD_DEFINE_RESULT__ = (function() {
			return base64;
		}).call(exports, __webpack_require__, exports, module),
		__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
	}	else { var key; }

}(this));


/***/ }),

/***/ 3305:
/*!*************************************!*\
  !*** ./node_modules/clone/clone.js ***!
  \*************************************/
/***/ ((module) => {

var clone = (function() {
'use strict';

function _instanceof(obj, type) {
  return type != null && obj instanceof type;
}

var nativeMap;
try {
  nativeMap = Map;
} catch(_) {
  // maybe a reference error because no `Map`. Give it a dummy value that no
  // value will ever be an instanceof.
  nativeMap = function() {};
}

var nativeSet;
try {
  nativeSet = Set;
} catch(_) {
  nativeSet = function() {};
}

var nativePromise;
try {
  nativePromise = Promise;
} catch(_) {
  nativePromise = function() {};
}

/**
 * Clones (copies) an Object using deep copying.
 *
 * This function supports circular references by default, but if you are certain
 * there are no circular references in your object, you can save some CPU time
 * by calling clone(obj, false).
 *
 * Caution: if `circular` is false and `parent` contains circular references,
 * your program may enter an infinite loop and crash.
 *
 * @param `parent` - the object to be cloned
 * @param `circular` - set to true if the object to be cloned may contain
 *    circular references. (optional - true by default)
 * @param `depth` - set to a number if the object is only to be cloned to
 *    a particular depth. (optional - defaults to Infinity)
 * @param `prototype` - sets the prototype to be used when cloning an object.
 *    (optional - defaults to parent prototype).
 * @param `includeNonEnumerable` - set to true if the non-enumerable properties
 *    should be cloned as well. Non-enumerable properties on the prototype
 *    chain will be ignored. (optional - false by default)
*/
function clone(parent, circular, depth, prototype, includeNonEnumerable) {
  if (typeof circular === 'object') {
    depth = circular.depth;
    prototype = circular.prototype;
    includeNonEnumerable = circular.includeNonEnumerable;
    circular = circular.circular;
  }
  // maintain two arrays for circular references, where corresponding parents
  // and children have the same index
  var allParents = [];
  var allChildren = [];

  var useBuffer = typeof Buffer != 'undefined';

  if (typeof circular == 'undefined')
    circular = true;

  if (typeof depth == 'undefined')
    depth = Infinity;

  // recurse this function so we don't reset allParents and allChildren
  function _clone(parent, depth) {
    // cloning null always returns null
    if (parent === null)
      return null;

    if (depth === 0)
      return parent;

    var child;
    var proto;
    if (typeof parent != 'object') {
      return parent;
    }

    if (_instanceof(parent, nativeMap)) {
      child = new nativeMap();
    } else if (_instanceof(parent, nativeSet)) {
      child = new nativeSet();
    } else if (_instanceof(parent, nativePromise)) {
      child = new nativePromise(function (resolve, reject) {
        parent.then(function(value) {
          resolve(_clone(value, depth - 1));
        }, function(err) {
          reject(_clone(err, depth - 1));
        });
      });
    } else if (clone.__isArray(parent)) {
      child = [];
    } else if (clone.__isRegExp(parent)) {
      child = new RegExp(parent.source, __getRegExpFlags(parent));
      if (parent.lastIndex) child.lastIndex = parent.lastIndex;
    } else if (clone.__isDate(parent)) {
      child = new Date(parent.getTime());
    } else if (useBuffer && Buffer.isBuffer(parent)) {
      child = new Buffer(parent.length);
      parent.copy(child);
      return child;
    } else if (_instanceof(parent, Error)) {
      child = Object.create(parent);
    } else {
      if (typeof prototype == 'undefined') {
        proto = Object.getPrototypeOf(parent);
        child = Object.create(proto);
      }
      else {
        child = Object.create(prototype);
        proto = prototype;
      }
    }

    if (circular) {
      var index = allParents.indexOf(parent);

      if (index != -1) {
        return allChildren[index];
      }
      allParents.push(parent);
      allChildren.push(child);
    }

    if (_instanceof(parent, nativeMap)) {
      parent.forEach(function(value, key) {
        var keyChild = _clone(key, depth - 1);
        var valueChild = _clone(value, depth - 1);
        child.set(keyChild, valueChild);
      });
    }
    if (_instanceof(parent, nativeSet)) {
      parent.forEach(function(value) {
        var entryChild = _clone(value, depth - 1);
        child.add(entryChild);
      });
    }

    for (var i in parent) {
      var attrs;
      if (proto) {
        attrs = Object.getOwnPropertyDescriptor(proto, i);
      }

      if (attrs && attrs.set == null) {
        continue;
      }
      child[i] = _clone(parent[i], depth - 1);
    }

    if (Object.getOwnPropertySymbols) {
      var symbols = Object.getOwnPropertySymbols(parent);
      for (var i = 0; i < symbols.length; i++) {
        // Don't need to worry about cloning a symbol because it is a primitive,
        // like a number or string.
        var symbol = symbols[i];
        var descriptor = Object.getOwnPropertyDescriptor(parent, symbol);
        if (descriptor && !descriptor.enumerable && !includeNonEnumerable) {
          continue;
        }
        child[symbol] = _clone(parent[symbol], depth - 1);
        if (!descriptor.enumerable) {
          Object.defineProperty(child, symbol, {
            enumerable: false
          });
        }
      }
    }

    if (includeNonEnumerable) {
      var allPropertyNames = Object.getOwnPropertyNames(parent);
      for (var i = 0; i < allPropertyNames.length; i++) {
        var propertyName = allPropertyNames[i];
        var descriptor = Object.getOwnPropertyDescriptor(parent, propertyName);
        if (descriptor && descriptor.enumerable) {
          continue;
        }
        child[propertyName] = _clone(parent[propertyName], depth - 1);
        Object.defineProperty(child, propertyName, {
          enumerable: false
        });
      }
    }

    return child;
  }

  return _clone(parent, depth);
}

/**
 * Simple flat clone using prototype, accepts only objects, usefull for property
 * override on FLAT configuration object (no nested props).
 *
 * USE WITH CAUTION! This may not behave as you wish if you do not know how this
 * works.
 */
clone.clonePrototype = function clonePrototype(parent) {
  if (parent === null)
    return null;

  var c = function () {};
  c.prototype = parent;
  return new c();
};

// private utility functions

function __objToStr(o) {
  return Object.prototype.toString.call(o);
}
clone.__objToStr = __objToStr;

function __isDate(o) {
  return typeof o === 'object' && __objToStr(o) === '[object Date]';
}
clone.__isDate = __isDate;

function __isArray(o) {
  return typeof o === 'object' && __objToStr(o) === '[object Array]';
}
clone.__isArray = __isArray;

function __isRegExp(o) {
  return typeof o === 'object' && __objToStr(o) === '[object RegExp]';
}
clone.__isRegExp = __isRegExp;

function __getRegExpFlags(re) {
  var flags = '';
  if (re.global) flags += 'g';
  if (re.ignoreCase) flags += 'i';
  if (re.multiline) flags += 'm';
  return flags;
}
clone.__getRegExpFlags = __getRegExpFlags;

return clone;
})();

if ( true && module.exports) {
  module.exports = clone;
}


/***/ }),

/***/ 2150:
/*!*************************************************!*\
  !*** ./node_modules/component-emitter/index.js ***!
  \*************************************************/
/***/ ((module) => {


/**
 * Expose `Emitter`.
 */

if (true) {
  module.exports = Emitter;
}

/**
 * Initialize a new `Emitter`.
 *
 * @api public
 */

function Emitter(obj) {
  if (obj) return mixin(obj);
};

/**
 * Mixin the emitter properties.
 *
 * @param {Object} obj
 * @return {Object}
 * @api private
 */

function mixin(obj) {
  for (var key in Emitter.prototype) {
    obj[key] = Emitter.prototype[key];
  }
  return obj;
}

/**
 * Listen on the given `event` with `fn`.
 *
 * @param {String} event
 * @param {Function} fn
 * @return {Emitter}
 * @api public
 */

Emitter.prototype.on =
Emitter.prototype.addEventListener = function(event, fn){
  this._callbacks = this._callbacks || {};
  (this._callbacks['$' + event] = this._callbacks['$' + event] || [])
    .push(fn);
  return this;
};

/**
 * Adds an `event` listener that will be invoked a single
 * time then automatically removed.
 *
 * @param {String} event
 * @param {Function} fn
 * @return {Emitter}
 * @api public
 */

Emitter.prototype.once = function(event, fn){
  function on() {
    this.off(event, on);
    fn.apply(this, arguments);
  }

  on.fn = fn;
  this.on(event, on);
  return this;
};

/**
 * Remove the given callback for `event` or all
 * registered callbacks.
 *
 * @param {String} event
 * @param {Function} fn
 * @return {Emitter}
 * @api public
 */

Emitter.prototype.off =
Emitter.prototype.removeListener =
Emitter.prototype.removeAllListeners =
Emitter.prototype.removeEventListener = function(event, fn){
  this._callbacks = this._callbacks || {};

  // all
  if (0 == arguments.length) {
    this._callbacks = {};
    return this;
  }

  // specific event
  var callbacks = this._callbacks['$' + event];
  if (!callbacks) return this;

  // remove all handlers
  if (1 == arguments.length) {
    delete this._callbacks['$' + event];
    return this;
  }

  // remove specific handler
  var cb;
  for (var i = 0; i < callbacks.length; i++) {
    cb = callbacks[i];
    if (cb === fn || cb.fn === fn) {
      callbacks.splice(i, 1);
      break;
    }
  }
  return this;
};

/**
 * Emit `event` with the given args.
 *
 * @param {String} event
 * @param {Mixed} ...
 * @return {Emitter}
 */

Emitter.prototype.emit = function(event){
  this._callbacks = this._callbacks || {};
  var args = [].slice.call(arguments, 1)
    , callbacks = this._callbacks['$' + event];

  if (callbacks) {
    callbacks = callbacks.slice(0);
    for (var i = 0, len = callbacks.length; i < len; ++i) {
      callbacks[i].apply(this, args);
    }
  }

  return this;
};

/**
 * Return array of callbacks for `event`.
 *
 * @param {String} event
 * @return {Array}
 * @api public
 */

Emitter.prototype.listeners = function(event){
  this._callbacks = this._callbacks || {};
  return this._callbacks['$' + event] || [];
};

/**
 * Check if this emitter has `event` handlers.
 *
 * @param {String} event
 * @return {Boolean}
 * @api public
 */

Emitter.prototype.hasListeners = function(event){
  return !! this.listeners(event).length;
};


/***/ }),

/***/ 3527:
/*!************************************!*\
  !*** ./node_modules/jsan/index.js ***!
  \************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

module.exports = __webpack_require__(/*! ./lib */ 5661);


/***/ }),

/***/ 1480:
/*!****************************************!*\
  !*** ./node_modules/jsan/lib/cycle.js ***!
  \****************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

var pathGetter = __webpack_require__(/*! ./path-getter */ 6445);
var utils = __webpack_require__(/*! ./utils */ 4326);

var WMap = typeof WeakMap !== 'undefined'?
  WeakMap:
  function() {
    var keys = [];
    var values = [];
    return {
      set: function(key, value) {
        keys.push(key);
        values.push(value);
      },
      get: function(key) {
        for (var i = 0; i < keys.length; i++) {
          if (keys[i] === key) {
            return values[i];
          }
        }
      }
    }
  };

// Based on https://github.com/douglascrockford/JSON-js/blob/master/cycle.js

exports.decycle = function decycle(object, options, replacer) {
  'use strict';

  var map = new WMap()

  var noCircularOption = !Object.prototype.hasOwnProperty.call(options, 'circular');
  var withRefs = options.refs !== false;

  return (function derez(_value, path, key) {

    // The derez recurses through the object, producing the deep copy.

    var i,        // The loop counter
      name,       // Property name
      nu;         // The new object or array

    // typeof null === 'object', so go on if this value is really an object but not
    // one of the weird builtin objects.

    var value = typeof replacer === 'function' ? replacer(key || '', _value) : _value;

    if (options.date && value instanceof Date) {
      return {$jsan: 'd' + value.getTime()};
    }
    if (options.regex && value instanceof RegExp) {
      return {$jsan: 'r' + utils.getRegexFlags(value) + ',' + value.source};
    }
    if (options['function'] && typeof value === 'function') {
      return {$jsan: 'f' + utils.stringifyFunction(value, options['function'])}
    }
    if (options['nan'] && typeof value === 'number' && isNaN(value)) {
      return {$jsan: 'n'}
    }
    if (options['infinity']) {
      if (Number.POSITIVE_INFINITY === value) return {$jsan: 'i'}
      if (Number.NEGATIVE_INFINITY === value) return {$jsan: 'y'}
    }
    if (options['undefined'] && value === undefined) {
      return {$jsan: 'u'}
    }
    if (options['error'] && value instanceof Error) {
      return {$jsan: 'e' + value.message}
    }
    if (options['symbol'] && typeof value === 'symbol') {
      var symbolKey = Symbol.keyFor(value)
      if (symbolKey !== undefined) {
        return {$jsan: 'g' + symbolKey}
      }

      // 'Symbol(foo)'.slice(7, -1) === 'foo'
      return {$jsan: 's' + value.toString().slice(7, -1)}
    }

    if (options['map'] && typeof Map === 'function' && value instanceof Map && typeof Array.from === 'function') {
      return {$jsan: 'm' + JSON.stringify(decycle(Array.from(value), options, replacer))}
    }

    if (options['set'] && typeof Set === 'function' && value instanceof Set && typeof Array.from === 'function') {
      return {$jsan: 'l' + JSON.stringify(decycle(Array.from(value), options, replacer))}
    }

    if (value && typeof value.toJSON === 'function') {
      try {
        value = value.toJSON(key);
      } catch (error) {
        var keyString = (key || '$');
        return "toJSON failed for '" + (map.get(value) || keyString) + "'";
      }
    }

    if (typeof value === 'object' && value !== null &&
      !(value instanceof Boolean) &&
      !(value instanceof Date)    &&
      !(value instanceof Number)  &&
      !(value instanceof RegExp)  &&
      !(value instanceof String)  &&
      !(typeof value === 'symbol')  &&
      !(value instanceof Error)) {

        // If the value is an object or array, look to see if we have already
        // encountered it. If so, return a $ref/path object.

      if (typeof value === 'object') {
        var foundPath = map.get(value);
        if (foundPath) {
          if (noCircularOption && withRefs) {
            return {$jsan: foundPath};
          }
          if (path.indexOf(foundPath) === 0) {
            if (!noCircularOption) {
              return typeof options.circular === 'function'?
              options.circular(value, path, foundPath):
              options.circular;
            }
            return {$jsan: foundPath};
          }
          if (withRefs) return {$jsan: foundPath};
        }
        map.set(value, path);
      }


      // If it is an array, replicate the array.

      if (Object.prototype.toString.apply(value) === '[object Array]') {
          nu = [];
          for (i = 0; i < value.length; i += 1) {
              nu[i] = derez(value[i], path + '[' + i + ']', i);
          }
      } else {

        // If it is an object, replicate the object.

        nu = {};
        for (name in value) {
          if (Object.prototype.hasOwnProperty.call(value, name)) {
            var nextPath = /^\w+$/.test(name) ?
              '.' + name :
              '[' + JSON.stringify(name) + ']';
            nu[name] = name === '$jsan' ? [derez(value[name], path + nextPath)] : derez(value[name], path + nextPath, name);
          }
        }
      }
      return nu;
    }
    return value;
  }(object, '$'));
};


exports.retrocycle = function retrocycle($) {
  'use strict';


  return (function rez(value) {

    // The rez function walks recursively through the object looking for $jsan
    // properties. When it finds one that has a value that is a path, then it
    // replaces the $jsan object with a reference to the value that is found by
    // the path.

    var i, item, name, path;

    if (value && typeof value === 'object') {
      if (Object.prototype.toString.apply(value) === '[object Array]') {
        for (i = 0; i < value.length; i += 1) {
          item = value[i];
          if (item && typeof item === 'object') {
            if (item.$jsan) {
              value[i] = utils.restore(item.$jsan, $);
            } else {
              rez(item);
            }
          }
        }
      } else {
        for (name in value) {
          // base case passed raw object
          if(typeof value[name] === 'string' && name === '$jsan'){
            return utils.restore(value.$jsan, $);
            break;
          }
          else {
            if (name === '$jsan') {
              value[name] = value[name][0];
            }
            if (typeof value[name] === 'object') {
              item = value[name];
              if (item && typeof item === 'object') {
                if (item.$jsan) {
                  value[name] = utils.restore(item.$jsan, $);
                } else {
                  rez(item);
                }
              }
            }
          }
        }
      }
    }
    return value;
  }($));
};


/***/ }),

/***/ 5661:
/*!****************************************!*\
  !*** ./node_modules/jsan/lib/index.js ***!
  \****************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

var cycle = __webpack_require__(/*! ./cycle */ 1480);

exports.stringify = function stringify(value, replacer, space, _options) {

  if (arguments.length < 4) {
    try {
      if (arguments.length === 1) {
        return JSON.stringify(value);
      } else {
        return JSON.stringify.apply(JSON, arguments);
      }
    } catch (e) {}
  }

  var options = _options || false;
  if (typeof options === 'boolean') {
    options = {
      'date': options,
      'function': options,
      'regex': options,
      'undefined': options,
      'error': options,
      'symbol': options,
      'map': options,
      'set': options,
      'nan': options,
      'infinity': options
    }
  }

  var decycled = cycle.decycle(value, options, replacer);
  if (arguments.length === 1) {
    return JSON.stringify(decycled);
  } else {
    // decycle already handles when replacer is a function.
    return JSON.stringify(decycled, Array.isArray(replacer) ? replacer : null, space);
  }
}

exports.parse = function parse(text, reviver) {
  var needsRetrocycle = /"\$jsan"/.test(text);
  var parsed;
  if (arguments.length === 1) {
    parsed = JSON.parse(text);
  } else {
    parsed = JSON.parse(text, reviver);
  }
  if (needsRetrocycle) {
    parsed = cycle.retrocycle(parsed);
  }
  return parsed;
}


/***/ }),

/***/ 6445:
/*!**********************************************!*\
  !*** ./node_modules/jsan/lib/path-getter.js ***!
  \**********************************************/
/***/ ((module) => {

module.exports = pathGetter;

function pathGetter(obj, path) {
  if (path !== '$') {
    var paths = getPaths(path);
    for (var i = 0; i < paths.length; i++) {
      path = paths[i].toString().replace(/\\"/g, '"');
      if (typeof obj[path] === 'undefined' && i !== paths.length - 1) continue;
      obj = obj[path];
    }
  }
  return obj;
}

function getPaths(pathString) {
  var regex = /(?:\.(\w+))|(?:\[(\d+)\])|(?:\["((?:[^\\"]|\\.)*)"\])/g;
  var matches = [];
  var match;
  while (match = regex.exec(pathString)) {
    matches.push( match[1] || match[2] || match[3] );
  }
  return matches;
}


/***/ }),

/***/ 4326:
/*!****************************************!*\
  !*** ./node_modules/jsan/lib/utils.js ***!
  \****************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

var pathGetter = __webpack_require__(/*! ./path-getter */ 6445);
var jsan = __webpack_require__(/*! ./ */ 5661);

exports.getRegexFlags = function getRegexFlags(regex) {
  var flags = '';
  if (regex.ignoreCase) flags += 'i';
  if (regex.global) flags += 'g';
  if (regex.multiline) flags += 'm';
  return flags;
};

exports.stringifyFunction = function stringifyFunction(fn, customToString) {
  if (typeof customToString === 'function') {
    return customToString(fn);
  }
  var str = fn.toString();
  var match = str.match(/^[^{]*{|^[^=]*=>/);
  var start = match ? match[0] : '<function> ';
  var end = str[str.length - 1] === '}' ? '}' : '';
  return start.replace(/\r\n|\n/g, ' ').replace(/\s+/g, ' ') + ' /* ... */ ' + end;
};

exports.restore = function restore(obj, root) {
  var type = obj[0];
  var rest = obj.slice(1);
  switch(type) {
    case '$':
      return pathGetter(root, obj);
    case 'r':
      var comma = rest.indexOf(',');
      var flags = rest.slice(0, comma);
      var source = rest.slice(comma + 1);
      return RegExp(source, flags);
    case 'd':
      return new Date(+rest);
    case 'f':
      var fn = function() { throw new Error("can't run jsan parsed function") };
      fn.toString = function() { return rest; };
      return fn;
    case 'u':
      return undefined;
    case 'e':
      var error = new Error(rest);
      error.stack = 'Stack is unavailable for jsan parsed errors';
      return error;
    case 's':
      return Symbol(rest);
    case 'g':
      return Symbol.for(rest);
    case 'm':
      return new Map(jsan.parse(rest));
    case 'l':
      return new Set(jsan.parse(rest));
    case 'n':
      return NaN;
    case 'i':
      return Infinity;
    case 'y':
      return -Infinity;
    default:
      console.warn('unknown type', obj);
      return obj;
  }
}


/***/ }),

/***/ 8758:
/*!*********************************************************!*\
  !*** ./node_modules/linked-list/_source/linked-list.js ***!
  \*********************************************************/
/***/ ((module) => {

"use strict";


/**
 * Constants.
 */

var errorMessage;

errorMessage = 'An argument without append, prepend, ' +
    'or detach methods was given to `List';

/**
 * Creates a new List: A linked list is a bit like an Array, but
 * knows nothing about how many items are in it, and knows only about its
 * first (`head`) and last (`tail`) items. Each item (e.g. `head`, `tail`,
 * &c.) knows which item comes before or after it (its more like the
 * implementation of the DOM in JavaScript).
 * @global
 * @private
 * @constructor
 * @class Represents an instance of List.
 */

function List(/*items...*/) {
    if (arguments.length) {
        return List.from(arguments);
    }
}

var ListPrototype;

ListPrototype = List.prototype;

/**
 * Creates a new list from the arguments (each a list item) passed in.
 * @name List.of
 * @param {...ListItem} [items] - Zero or more items to attach.
 * @returns {list} - A new instance of List.
 */

List.of = function (/*items...*/) {
    return List.from.call(this, arguments);
};

/**
 * Creates a new list from the given array-like object (each a list item)
 * passed in.
 * @name List.from
 * @param {ListItem[]} [items] - The items to append.
 * @returns {list} - A new instance of List.
 */
List.from = function (items) {
    var list = new this(), length, iterator, item;

    if (items && (length = items.length)) {
        iterator = -1;

        while (++iterator < length) {
            item = items[iterator];

            if (item !== null && item !== undefined) {
                list.append(item);
            }
        }
    }

    return list;
};

/**
 * List#head
 * Default to `null`.
 */
ListPrototype.head = null;

/**
 * List#tail
 * Default to `null`.
 */
ListPrototype.tail = null;

/**
 * Returns the list's items as an array. This does *not* detach the items.
 * @name List#toArray
 * @returns {ListItem[]} - An array of (still attached) ListItems.
 */
ListPrototype.toArray = function () {
    var item = this.head,
        result = [];

    while (item) {
        result.push(item);
        item = item.next;
    }

    return result;
};

/**
 * Prepends the given item to the list: Item will be the new first item
 * (`head`).
 * @name List#prepend
 * @param {ListItem} item - The item to prepend.
 * @returns {ListItem} - An instance of ListItem (the given item).
 */
ListPrototype.prepend = function (item) {
    if (!item) {
        return false;
    }

    if (!item.append || !item.prepend || !item.detach) {
        throw new Error(errorMessage + '#prepend`.');
    }

    var self, head;

    // Cache self.
    self = this;

    // If self has a first item, defer prepend to the first items prepend
    // method, and return the result.
    head = self.head;

    if (head) {
        return head.prepend(item);
    }

    // ...otherwise, there is no `head` (or `tail`) item yet.

    // Detach the prependee.
    item.detach();

    // Set the prependees parent list to reference self.
    item.list = self;

    // Set self's first item to the prependee, and return the item.
    self.head = item;

    return item;
};

/**
 * Appends the given item to the list: Item will be the new last item (`tail`)
 * if the list had a first item, and its first item (`head`) otherwise.
 * @name List#append
 * @param {ListItem} item - The item to append.
 * @returns {ListItem} - An instance of ListItem (the given item).
 */

ListPrototype.append = function (item) {
    if (!item) {
        return false;
    }

    if (!item.append || !item.prepend || !item.detach) {
        throw new Error(errorMessage + '#append`.');
    }

    var self, head, tail;

    // Cache self.
    self = this;

    // If self has a last item, defer appending to the last items append
    // method, and return the result.
    tail = self.tail;

    if (tail) {
        return tail.append(item);
    }

    // If self has a first item, defer appending to the first items append
    // method, and return the result.
    head = self.head;

    if (head) {
        return head.append(item);
    }

    // ...otherwise, there is no `tail` or `head` item yet.

    // Detach the appendee.
    item.detach();

    // Set the appendees parent list to reference self.
    item.list = self;

    // Set self's first item to the appendee, and return the item.
    self.head = item;

    return item;
};

/**
 * Creates a new ListItem: A linked list item is a bit like DOM node:
 * It knows only about its "parent" (`list`), the item before it (`prev`),
 * and the item after it (`next`).
 * @global
 * @private
 * @constructor
 * @class Represents an instance of ListItem.
 */

function ListItem() {}

List.Item = ListItem;

var ListItemPrototype = ListItem.prototype;

ListItemPrototype.next = null;

ListItemPrototype.prev = null;

ListItemPrototype.list = null;

/**
 * Detaches the item operated on from its parent list.
 * @name ListItem#detach
 * @returns {ListItem} - The item operated on.
 */
ListItemPrototype.detach = function () {
    // Cache self, the parent list, and the previous and next items.
    var self = this,
        list = self.list,
        prev = self.prev,
        next = self.next;

    // If the item is already detached, return self.
    if (!list) {
        return self;
    }

    // If self is the last item in the parent list, link the lists last item
    // to the previous item.
    if (list.tail === self) {
        list.tail = prev;
    }

    // If self is the first item in the parent list, link the lists first item
    // to the next item.
    if (list.head === self) {
        list.head = next;
    }

    // If both the last and first items in the parent list are the same,
    // remove the link to the last item.
    if (list.tail === list.head) {
        list.tail = null;
    }

    // If a previous item exists, link its next item to selfs next item.
    if (prev) {
        prev.next = next;
    }

    // If a next item exists, link its previous item to selfs previous item.
    if (next) {
        next.prev = prev;
    }

    // Remove links from self to both the next and previous items, and to the
    // parent list.
    self.prev = self.next = self.list = null;

    // Return self.
    return self;
};

/**
 * Prepends the given item *before* the item operated on.
 * @name ListItem#prepend
 * @param {ListItem} item - The item to prepend.
 * @returns {ListItem} - The item operated on, or false when that item is not
 * attached.
 */
ListItemPrototype.prepend = function (item) {
    if (!item || !item.append || !item.prepend || !item.detach) {
        throw new Error(errorMessage + 'Item#prepend`.');
    }

    // Cache self, the parent list, and the previous item.
    var self = this,
        list = self.list,
        prev = self.prev;

    // If self is detached, return false.
    if (!list) {
        return false;
    }

    // Detach the prependee.
    item.detach();

    // If self has a previous item...
    if (prev) {
        // ...link the prependees previous item, to selfs previous item.
        item.prev = prev;

        // ...link the previous items next item, to self.
        prev.next = item;
    }

    // Set the prependees next item to self.
    item.next = self;

    // Set the prependees parent list to selfs parent list.
    item.list = list;

    // Set the previous item of self to the prependee.
    self.prev = item;

    // If self is the first item in the parent list, link the lists first item
    // to the prependee.
    if (self === list.head) {
        list.head = item;
    }

    // If the the parent list has no last item, link the lists last item to
    // self.
    if (!list.tail) {
        list.tail = self;
    }

    // Return the prependee.
    return item;
};

/**
 * Appends the given item *after* the item operated on.
 * @name ListItem#append
 * @param {ListItem} item - The item to append.
 * @returns {ListItem} - The item operated on, or false when that item is not
 * attached.
 */
ListItemPrototype.append = function (item) {
    // If item is falsey, return false.
    if (!item || !item.append || !item.prepend || !item.detach) {
        throw new Error(errorMessage + 'Item#append`.');
    }

    // Cache self, the parent list, and the next item.
    var self = this,
        list = self.list,
        next = self.next;

    // If self is detached, return false.
    if (!list) {
        return false;
    }

    // Detach the appendee.
    item.detach();

    // If self has a next item...
    if (next) {
        // ...link the appendees next item, to selfs next item.
        item.next = next;

        // ...link the next items previous item, to the appendee.
        next.prev = item;
    }

    // Set the appendees previous item to self.
    item.prev = self;

    // Set the appendees parent list to selfs parent list.
    item.list = list;

    // Set the next item of self to the appendee.
    self.next = item;

    // If the the parent list has no last item or if self is the parent lists
    // last item, link the lists last item to the appendee.
    if (self === list.tail || !list.tail) {
        list.tail = item;
    }

    // Return the appendee.
    return item;
};

/**
 * Expose `List`.
 */

module.exports = List;


/***/ }),

/***/ 1441:
/*!*******************************************!*\
  !*** ./node_modules/linked-list/index.js ***!
  \*******************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


module.exports = __webpack_require__(/*! ./_source/linked-list.js */ 8758);


/***/ }),

/***/ 4:
/*!********************************************!*\
  !*** ./node_modules/querystring/decode.js ***!
  \********************************************/
/***/ ((module) => {

"use strict";
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.



// If obj.hasOwnProperty has been overridden, then calling
// obj.hasOwnProperty(prop) will break.
// See: https://github.com/joyent/node/issues/1707
function hasOwnProperty(obj, prop) {
  return Object.prototype.hasOwnProperty.call(obj, prop);
}

module.exports = function(qs, sep, eq, options) {
  sep = sep || '&';
  eq = eq || '=';
  var obj = {};

  if (typeof qs !== 'string' || qs.length === 0) {
    return obj;
  }

  var regexp = /\+/g;
  qs = qs.split(sep);

  var maxKeys = 1000;
  if (options && typeof options.maxKeys === 'number') {
    maxKeys = options.maxKeys;
  }

  var len = qs.length;
  // maxKeys <= 0 means that we should not limit keys count
  if (maxKeys > 0 && len > maxKeys) {
    len = maxKeys;
  }

  for (var i = 0; i < len; ++i) {
    var x = qs[i].replace(regexp, '%20'),
        idx = x.indexOf(eq),
        kstr, vstr, k, v;

    if (idx >= 0) {
      kstr = x.substr(0, idx);
      vstr = x.substr(idx + 1);
    } else {
      kstr = x;
      vstr = '';
    }

    k = decodeURIComponent(kstr);
    v = decodeURIComponent(vstr);

    if (!hasOwnProperty(obj, k)) {
      obj[k] = v;
    } else if (Array.isArray(obj[k])) {
      obj[k].push(v);
    } else {
      obj[k] = [obj[k], v];
    }
  }

  return obj;
};


/***/ }),

/***/ 3132:
/*!********************************************!*\
  !*** ./node_modules/querystring/encode.js ***!
  \********************************************/
/***/ ((module) => {

"use strict";
// Copyright Joyent, Inc. and other Node contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to permit
// persons to whom the Software is furnished to do so, subject to the
// following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
// NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
// DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
// OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
// USE OR OTHER DEALINGS IN THE SOFTWARE.



var stringifyPrimitive = function(v) {
  switch (typeof v) {
    case 'string':
      return v;

    case 'boolean':
      return v ? 'true' : 'false';

    case 'number':
      return isFinite(v) ? v : '';

    default:
      return '';
  }
};

module.exports = function(obj, sep, eq, name) {
  sep = sep || '&';
  eq = eq || '=';
  if (obj === null) {
    obj = undefined;
  }

  if (typeof obj === 'object') {
    return Object.keys(obj).map(function(k) {
      var ks = encodeURIComponent(stringifyPrimitive(k)) + eq;
      if (Array.isArray(obj[k])) {
        return obj[k].map(function(v) {
          return ks + encodeURIComponent(stringifyPrimitive(v));
        }).join(sep);
      } else {
        return ks + encodeURIComponent(stringifyPrimitive(obj[k]));
      }
    }).join(sep);

  }

  if (!name) return '';
  return encodeURIComponent(stringifyPrimitive(name)) + eq +
         encodeURIComponent(stringifyPrimitive(obj));
};


/***/ }),

/***/ 6774:
/*!*******************************************!*\
  !*** ./node_modules/querystring/index.js ***!
  \*******************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


exports.decode = exports.parse = __webpack_require__(/*! ./decode */ 4);
exports.encode = exports.stringify = __webpack_require__(/*! ./encode */ 3132);


/***/ }),

/***/ 3288:
/*!*************************************************!*\
  !*** ./node_modules/remotedev/lib/constants.js ***!
  \*************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";


exports.__esModule = true;
var defaultSocketOptions = exports.defaultSocketOptions = {
  secure: true,
  hostname: 'remotedev.io',
  port: 443,
  autoReconnect: true,
  autoReconnectOptions: {
    randomness: 60000
  }
};

/***/ }),

/***/ 4952:
/*!************************************************!*\
  !*** ./node_modules/remotedev/lib/devTools.js ***!
  \************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";


exports.__esModule = true;
exports.send = undefined;
exports.extractState = extractState;
exports.generateId = generateId;
exports.start = start;
exports.connect = connect;
exports.connectViaExtension = connectViaExtension;

var _jsan = __webpack_require__(/*! jsan */ 3527);

var _socketclusterClient = __webpack_require__(/*! socketcluster-client */ 8401);

var _socketclusterClient2 = _interopRequireDefault(_socketclusterClient);

var _rnHostDetect = __webpack_require__(/*! rn-host-detect */ 5826);

var _rnHostDetect2 = _interopRequireDefault(_rnHostDetect);

var _constants = __webpack_require__(/*! ./constants */ 3288);

function _interopRequireDefault(obj) { return obj && obj.__esModule ? obj : { default: obj }; }

var socket = undefined;
var channel = undefined;
var listeners = {};

function extractState(message) {
  if (!message || !message.state) return undefined;
  if (typeof message.state === 'string') return (0, _jsan.parse)(message.state);
  return message.state;
}

function generateId() {
  return Math.random().toString(36).substr(2);
}

function handleMessages(message) {
  if (!message.payload) message.payload = message.action;
  Object.keys(listeners).forEach(function (id) {
    if (message.instanceId && id !== message.instanceId) return;
    if (typeof listeners[id] === 'function') listeners[id](message);else listeners[id].forEach(function (fn) {
      fn(message);
    });
  });
}

function watch() {
  if (channel) return;
  socket.emit('login', 'master', function (err, channelName) {
    if (err) {
      console.log(err);return;
    }
    channel = socket.subscribe(channelName);
    channel.watch(handleMessages);
    socket.on(channelName, handleMessages);
  });
}

function connectToServer(options) {
  if (socket) return;
  var socketOptions = undefined;
  if (options.port) {
    socketOptions = {
      port: options.port,
      hostname: (0, _rnHostDetect2.default)(options.hostname || 'localhost'),
      secure: !!options.secure
    };
  } else socketOptions = _constants.defaultSocketOptions;
  socket = _socketclusterClient2.default.create(socketOptions);
  watch();
}

function start(options) {
  if (options) {
    if (options.port && !options.hostname) {
      options.hostname = 'localhost';
    }
  }
  connectToServer(options);
}

function transformAction(action, config) {
  if (action.action) return action;
  var liftedAction = { timestamp: Date.now() };
  if (action) {
    if (config.getActionType) liftedAction.action = config.getActionType(action);else {
      if (typeof action === 'string') liftedAction.action = { type: action };else if (!action.type) liftedAction.action = { type: 'update' };else liftedAction.action = action;
    }
  } else {
    liftedAction.action = { type: action };
  }
  return liftedAction;
}

function _send(action, state, options, type, instanceId) {
  start(options);
  setTimeout(function () {
    var message = {
      payload: state ? (0, _jsan.stringify)(state) : '',
      action: type === 'ACTION' ? (0, _jsan.stringify)(transformAction(action, options)) : action,
      type: type || 'ACTION',
      id: socket.id,
      instanceId: instanceId,
      name: options.name
    };
    socket.emit(socket.id ? 'log' : 'log-noid', message);
  }, 0);
}

exports.send = _send;
function connect() {
  var options = arguments.length <= 0 || arguments[0] === undefined ? {} : arguments[0];

  var id = generateId(options.instanceId);
  start(options);
  return {
    init: function init(state, action) {
      _send(action || {}, state, options, 'INIT', id);
    },
    subscribe: function subscribe(listener) {
      if (!listener) return undefined;
      if (!listeners[id]) listeners[id] = [];
      listeners[id].push(listener);

      return function unsubscribe() {
        var index = listeners[id].indexOf(listener);
        listeners[id].splice(index, 1);
      };
    },
    unsubscribe: function unsubscribe() {
      delete listeners[id];
    },
    send: function send(action, payload) {
      if (action) {
        _send(action, payload, options, 'ACTION', id);
      } else {
        _send(undefined, payload, options, 'STATE', id);
      }
    },
    error: function error(payload) {
      socket.emit({ type: 'ERROR', payload: payload, id: socket.id, instanceId: id });
    }
  };
}

function connectViaExtension(options) {
  if (options && options.remote || typeof window === 'undefined' || !window.__REDUX_DEVTOOLS_EXTENSION__) {
    return connect(options);
  }
  return window.__REDUX_DEVTOOLS_EXTENSION__.connect(options);
}

exports.default = { connect: connect, connectViaExtension: connectViaExtension, send: _send, extractState: extractState, generateId: generateId };

/***/ }),

/***/ 9607:
/*!*********************************************!*\
  !*** ./node_modules/remotedev/lib/index.js ***!
  \*********************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

"use strict";


module.exports = __webpack_require__(/*! ./devTools */ 4952);

/***/ }),

/***/ 5826:
/*!**********************************************!*\
  !*** ./node_modules/rn-host-detect/index.js ***!
  \**********************************************/
/***/ ((module) => {

"use strict";


/*
 * It only for Debug Remotely mode for Android
 * When __DEV__ === false, we can't use window.require('NativeModules')
 */
function getByRemoteConfig(hostname) {
  var remoteModuleConfig = typeof window !== 'undefined' &&
    window.__fbBatchedBridgeConfig &&
    window.__fbBatchedBridgeConfig.remoteModuleConfig

  if (
    !Array.isArray(remoteModuleConfig) ||
    hostname !== 'localhost' && hostname !== '127.0.0.1'
  ) return { hostname: hostname, passed: false }

  var result = hostname
  var passed = false
  remoteModuleConfig.some(function (config) {
    if (!config) return false
    
    var name = config[0]
    var content = config[1]
    if (
      (name === 'AndroidConstants' || name === 'PlatformConstants') &&
      content &&
      content.ServerHost
    ) {
      result = content.ServerHost.split(':')[0]
      passed = true
      return true
    }

    if (
      name === 'SourceCode' &&
      content &&
      content.scriptURL
    ) {
      result = content.scriptURL.replace(/https?:\/\//, '').split(':')[0]
      passed = true
      return true
    }
    return false
  })

  return { hostname: result, passed: passed }
}

function getByRNRequirePolyfill(hostname) {
  var originalWarn = console.warn
  var NativeModules
  var Constants
  var SourceCode
  if (
    typeof window === 'undefined' ||
    !window.__DEV__ ||
    typeof window.require !== 'function' ||
    // RN >= 0.56
    // TODO: Get NativeModules for RN >= 0.56
    window.require.name === 'metroRequire'
  ) {
    return hostname
  }
  console.warn = function() {
    if (
      arguments[0] &&
      typeof arguments[0].indexOf == 'function' &&
      arguments[0].indexOf("Requiring module 'NativeModules' by name") > -1
    )
      return
    return originalWarn.apply(console, arguments)
  }
  try {
    NativeModules = window.require('NativeModules')
  } catch (e) {}
  console.warn = originalWarn
  if (!NativeModules) return hostname

  Constants = NativeModules.PlatformConstants || NativeModules.AndroidConstants
  SourceCode = NativeModules.SourceCode

  if (Constants && Constants.ServerHost) {
    return Constants.ServerHost.split(':')[0]
  } else if (SourceCode && SourceCode.scriptURL) {
    return SourceCode.scriptURL.replace(/https?:\/\//, '').split(':')[0]
  }
  return hostname
}

/*
 * Get React Native server IP if hostname is `localhost`
 * On Android emulator, the IP of host is `10.0.2.2` (Genymotion: 10.0.3.2)
 */
module.exports = function (hostname) {
  // Check if it in React Native environment
  if (
    typeof __fbBatchedBridgeConfig !== 'object' ||
    hostname !== 'localhost' && hostname !== '127.0.0.1'
  ) {
    return hostname
  }
  var result = getByRemoteConfig(hostname)

  // Leave if get hostname by remote config successful
  if (result.passed) {
    return result.hostname
  }

  // Otherwise, use RN's require polyfill
  return getByRNRequirePolyfill(hostname)
}


/***/ }),

/***/ 6125:
/*!******************************************!*\
  !*** ./node_modules/sc-channel/index.js ***!
  \******************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var Emitter = __webpack_require__(/*! component-emitter */ 2150);

var SCChannel = function (name, client, options) {
  var self = this;

  Emitter.call(this);

  this.PENDING = 'pending';
  this.SUBSCRIBED = 'subscribed';
  this.UNSUBSCRIBED = 'unsubscribed';

  this.name = name;
  this.state = this.UNSUBSCRIBED;
  this.client = client;

  this.options = options || {};
  this.setOptions(this.options);
};

SCChannel.prototype = Object.create(Emitter.prototype);

SCChannel.prototype.setOptions = function (options) {
  if (!options) {
    options = {};
  }
  this.waitForAuth = options.waitForAuth || false;
  this.batch = options.batch || false;

  if (options.data !== undefined) {
    this.data = options.data;
  }
};

SCChannel.prototype.getState = function () {
  return this.state;
};

SCChannel.prototype.subscribe = function (options) {
  this.client.subscribe(this.name, options);
};

SCChannel.prototype.unsubscribe = function () {
  this.client.unsubscribe(this.name);
};

SCChannel.prototype.isSubscribed = function (includePending) {
  return this.client.isSubscribed(this.name, includePending);
};

SCChannel.prototype.publish = function (data, callback) {
  this.client.publish(this.name, data, callback);
};

SCChannel.prototype.watch = function (handler) {
  this.client.watch(this.name, handler);
};

SCChannel.prototype.unwatch = function (handler) {
  this.client.unwatch(this.name, handler);
};

SCChannel.prototype.watchers = function () {
  return this.client.watchers(this.name);
};

SCChannel.prototype.destroy = function () {
  this.client.destroyChannel(this.name);
};

module.exports.X = SCChannel;


/***/ }),

/***/ 5932:
/*!*******************************************!*\
  !*** ./node_modules/sc-errors/decycle.js ***!
  \*******************************************/
/***/ ((module) => {

// Based on https://github.com/dscape/cycle/blob/master/cycle.js

module.exports = function decycle(object) {
// Make a deep copy of an object or array, assuring that there is at most
// one instance of each object or array in the resulting structure. The
// duplicate references (which might be forming cycles) are replaced with
// an object of the form
//      {$ref: PATH}
// where the PATH is a JSONPath string that locates the first occurance.
// So,
//      var a = [];
//      a[0] = a;
//      return JSON.stringify(JSON.decycle(a));
// produces the string '[{"$ref":"$"}]'.

// JSONPath is used to locate the unique object. $ indicates the top level of
// the object or array. [NUMBER] or [STRING] indicates a child member or
// property.

    var objects = [],   // Keep a reference to each unique object or array
        paths = [];     // Keep the path to each unique object or array

    return (function derez(value, path) {

// The derez recurses through the object, producing the deep copy.

        var i,          // The loop counter
            name,       // Property name
            nu;         // The new object or array

// typeof null === 'object', so go on if this value is really an object but not
// one of the weird builtin objects.

        if (typeof value === 'object' && value !== null &&
                !(value instanceof Boolean) &&
                !(value instanceof Date)    &&
                !(value instanceof Number)  &&
                !(value instanceof RegExp)  &&
                !(value instanceof String)) {

// If the value is an object or array, look to see if we have already
// encountered it. If so, return a $ref/path object. This is a hard way,
// linear search that will get slower as the number of unique objects grows.

            for (i = 0; i < objects.length; i += 1) {
                if (objects[i] === value) {
                    return {$ref: paths[i]};
                }
            }

// Otherwise, accumulate the unique value and its path.

            objects.push(value);
            paths.push(path);

// If it is an array, replicate the array.

            if (Object.prototype.toString.apply(value) === '[object Array]') {
                nu = [];
                for (i = 0; i < value.length; i += 1) {
                    nu[i] = derez(value[i], path + '[' + i + ']');
                }
            } else {

// If it is an object, replicate the object.

                nu = {};
                for (name in value) {
                    if (Object.prototype.hasOwnProperty.call(value, name)) {
                        nu[name] = derez(value[name],
                            path + '[' + JSON.stringify(name) + ']');
                    }
                }
            }
            return nu;
        }
        return value;
    }(object, '$'));
};


/***/ }),

/***/ 5588:
/*!*****************************************!*\
  !*** ./node_modules/sc-errors/index.js ***!
  \*****************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var decycle = __webpack_require__(/*! ./decycle */ 5932);

var isStrict = (function () { return !this; })();

function AuthTokenExpiredError(message, expiry) {
  this.name = 'AuthTokenExpiredError';
  this.message = message;
  this.expiry = expiry;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
AuthTokenExpiredError.prototype = Object.create(Error.prototype);


function AuthTokenInvalidError(message) {
  this.name = 'AuthTokenInvalidError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
AuthTokenInvalidError.prototype = Object.create(Error.prototype);


function AuthTokenNotBeforeError(message, date) {
  this.name = 'AuthTokenNotBeforeError';
  this.message = message;
  this.date = date;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
AuthTokenNotBeforeError.prototype = Object.create(Error.prototype);


// For any other auth token error.
function AuthTokenError(message) {
  this.name = 'AuthTokenError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
AuthTokenError.prototype = Object.create(Error.prototype);


function SilentMiddlewareBlockedError(message, type) {
  this.name = 'SilentMiddlewareBlockedError';
  this.message = message;
  this.type = type;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
SilentMiddlewareBlockedError.prototype = Object.create(Error.prototype);


function InvalidActionError(message) {
  this.name = 'InvalidActionError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
InvalidActionError.prototype = Object.create(Error.prototype);

function InvalidArgumentsError(message) {
  this.name = 'InvalidArgumentsError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
InvalidArgumentsError.prototype = Object.create(Error.prototype);

function InvalidOptionsError(message) {
  this.name = 'InvalidOptionsError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
InvalidOptionsError.prototype = Object.create(Error.prototype);


function InvalidMessageError(message) {
  this.name = 'InvalidMessageError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
InvalidMessageError.prototype = Object.create(Error.prototype);


function SocketProtocolError(message, code) {
  this.name = 'SocketProtocolError';
  this.message = message;
  this.code = code;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
SocketProtocolError.prototype = Object.create(Error.prototype);


function ServerProtocolError(message) {
  this.name = 'ServerProtocolError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
ServerProtocolError.prototype = Object.create(Error.prototype);

function HTTPServerError(message) {
  this.name = 'HTTPServerError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
HTTPServerError.prototype = Object.create(Error.prototype);


function ResourceLimitError(message) {
  this.name = 'ResourceLimitError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
ResourceLimitError.prototype = Object.create(Error.prototype);


function TimeoutError(message) {
  this.name = 'TimeoutError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
TimeoutError.prototype = Object.create(Error.prototype);


function BadConnectionError(message, type) {
  this.name = 'BadConnectionError';
  this.message = message;
  this.type = type;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
BadConnectionError.prototype = Object.create(Error.prototype);


function BrokerError(message) {
  this.name = 'BrokerError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
BrokerError.prototype = Object.create(Error.prototype);


function ProcessExitError(message, code) {
  this.name = 'ProcessExitError';
  this.message = message;
  this.code = code;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
ProcessExitError.prototype = Object.create(Error.prototype);


function UnknownError(message) {
  this.name = 'UnknownError';
  this.message = message;
  if (Error.captureStackTrace && !isStrict) {
    Error.captureStackTrace(this, arguments.callee);
  } else {
    this.stack = (new Error()).stack;
  }
}
UnknownError.prototype = Object.create(Error.prototype);


// Expose all error types.

module.exports = {
  AuthTokenExpiredError: AuthTokenExpiredError,
  AuthTokenInvalidError: AuthTokenInvalidError,
  AuthTokenNotBeforeError: AuthTokenNotBeforeError,
  AuthTokenError: AuthTokenError,
  SilentMiddlewareBlockedError: SilentMiddlewareBlockedError,
  InvalidActionError: InvalidActionError,
  InvalidArgumentsError: InvalidArgumentsError,
  InvalidOptionsError: InvalidOptionsError,
  InvalidMessageError: InvalidMessageError,
  SocketProtocolError: SocketProtocolError,
  ServerProtocolError: ServerProtocolError,
  HTTPServerError: HTTPServerError,
  ResourceLimitError: ResourceLimitError,
  TimeoutError: TimeoutError,
  BadConnectionError: BadConnectionError,
  BrokerError: BrokerError,
  ProcessExitError: ProcessExitError,
  UnknownError: UnknownError
};

module.exports.socketProtocolErrorStatuses = {
  1001: 'Socket was disconnected',
  1002: 'A WebSocket protocol error was encountered',
  1003: 'Server terminated socket because it received invalid data',
  1005: 'Socket closed without status code',
  1006: 'Socket hung up',
  1007: 'Message format was incorrect',
  1008: 'Encountered a policy violation',
  1009: 'Message was too big to process',
  1010: 'Client ended the connection because the server did not comply with extension requirements',
  1011: 'Server encountered an unexpected fatal condition',
  4000: 'Server ping timed out',
  4001: 'Client pong timed out',
  4002: 'Server failed to sign auth token',
  4003: 'Failed to complete handshake',
  4004: 'Client failed to save auth token',
  4005: 'Did not receive #handshake from client before timeout',
  4006: 'Failed to bind socket to message broker',
  4007: 'Client connection establishment timed out',
  4008: 'Server rejected handshake from client'
};

module.exports.socketProtocolIgnoreStatuses = {
  1000: 'Socket closed normally',
  1001: 'Socket hung up'
};

// Properties related to error domains cannot be serialized.
var unserializableErrorProperties = {
  domain: 1,
  domainEmitter: 1,
  domainThrown: 1
};

// Convert an error into a JSON-compatible type which can later be hydrated
// back to its *original* form.
module.exports.dehydrateError = function dehydrateError(error, includeStackTrace) {
  var dehydratedError;

  if (error && typeof error === 'object') {
    dehydratedError = {
      message: error.message
    };
    if (includeStackTrace) {
      dehydratedError.stack = error.stack;
    }
    for (var i in error) {
      if (!unserializableErrorProperties[i]) {
        dehydratedError[i] = error[i];
      }
    }
  } else if (typeof error === 'function') {
    dehydratedError = '[function ' + (error.name || 'anonymous') + ']';
  } else {
    dehydratedError = error;
  }

  return decycle(dehydratedError);
};

// Convert a dehydrated error back to its *original* form.
module.exports.hydrateError = function hydrateError(error) {
  var hydratedError = null;
  if (error != null) {
    if (typeof error === 'object') {
      hydratedError = new Error(error.message);
      for (var i in error) {
        if (error.hasOwnProperty(i)) {
          hydratedError[i] = error[i];
        }
      }
    } else {
      hydratedError = error;
    }
  }
  return hydratedError;
};

module.exports.decycle = decycle;


/***/ }),

/***/ 2973:
/*!********************************************!*\
  !*** ./node_modules/sc-formatter/index.js ***!
  \********************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
var validJSONStartRegex = /^[ \n\r\t]*[{\[]/;

var arrayBufferToBase64 = function (arraybuffer) {
  var bytes = new Uint8Array(arraybuffer);
  var len = bytes.length;
  var base64 = '';

  for (var i = 0; i < len; i += 3) {
    base64 += base64Chars[bytes[i] >> 2];
    base64 += base64Chars[((bytes[i] & 3) << 4) | (bytes[i + 1] >> 4)];
    base64 += base64Chars[((bytes[i + 1] & 15) << 2) | (bytes[i + 2] >> 6)];
    base64 += base64Chars[bytes[i + 2] & 63];
  }

  if ((len % 3) === 2) {
    base64 = base64.substring(0, base64.length - 1) + '=';
  } else if (len % 3 === 1) {
    base64 = base64.substring(0, base64.length - 2) + '==';
  }

  return base64;
};

var binaryToBase64Replacer = function (key, value) {
  if (__webpack_require__.g.ArrayBuffer && value instanceof __webpack_require__.g.ArrayBuffer) {
    return {
      base64: true,
      data: arrayBufferToBase64(value)
    };
  } else if (__webpack_require__.g.Buffer) {
    if (value instanceof __webpack_require__.g.Buffer){
      return {
        base64: true,
        data: value.toString('base64')
      };
    }
    // Some versions of Node.js convert Buffers to Objects before they are passed to
    // the replacer function - Because of this, we need to rehydrate Buffers
    // before we can convert them to base64 strings.
    if (value && value.type === 'Buffer' && Array.isArray(value.data)) {
      var rehydratedBuffer;
      if (__webpack_require__.g.Buffer.from) {
        rehydratedBuffer = __webpack_require__.g.Buffer.from(value.data);
      } else {
        rehydratedBuffer = new __webpack_require__.g.Buffer(value.data);
      }
      return {
        base64: true,
        data: rehydratedBuffer.toString('base64')
      };
    }
  }
  return value;
};

// Decode the data which was transmitted over the wire to a JavaScript Object in a format which SC understands.
// See encode function below for more details.
module.exports.decode = function (input) {
  if (input == null) {
   return null;
  }
  // Leave ping or pong message as is
  if (input === '#1' || input === '#2') {
    return input;
  }
  var message = input.toString();

  // Performance optimization to detect invalid JSON packet sooner.
  if (!validJSONStartRegex.test(message)) {
    return message;
  }

  try {
    return JSON.parse(message);
  } catch (err) {}
  return message;
};

// Encode a raw JavaScript object (which is in the SC protocol format) into a format for
// transfering it over the wire. In this case, we just convert it into a simple JSON string.
// If you want to create your own custom codec, you can encode the object into any format
// (e.g. binary ArrayBuffer or string with any kind of compression) so long as your decode
// function is able to rehydrate that object back into its original JavaScript Object format
// (which adheres to the SC protocol).
// See https://github.com/SocketCluster/socketcluster/blob/master/socketcluster-protocol.md
// for details about the SC protocol.
module.exports.encode = function (object) {
  // Leave ping or pong message as is
  if (object === '#1' || object === '#2') {
    return object;
  }
  return JSON.stringify(object, binaryToBase64Replacer);
};


/***/ }),

/***/ 8401:
/*!****************************************************!*\
  !*** ./node_modules/socketcluster-client/index.js ***!
  \****************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var SCClientSocket = __webpack_require__(/*! ./lib/scclientsocket */ 4363);
var factory = __webpack_require__(/*! ./lib/factory */ 9620);

module.exports.factory = factory;
module.exports.SCClientSocket = SCClientSocket;

module.exports.Emitter = __webpack_require__(/*! component-emitter */ 2150);

module.exports.create = function (options) {
  return factory.create(options);
};

module.exports.connect = module.exports.create;

module.exports.destroy = function (socket) {
  return factory.destroy(socket);
};

module.exports.clients = factory.clients;

module.exports.version = '13.0.1';


/***/ }),

/***/ 4539:
/*!*******************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/auth.js ***!
  \*******************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var AuthEngine = function () {
  this._internalStorage = {};
  this.isLocalStorageEnabled = this._checkLocalStorageEnabled();
};

AuthEngine.prototype._checkLocalStorageEnabled = function () {
  var err;
  try {
    // Some browsers will throw an error here if localStorage is disabled.
    __webpack_require__.g.localStorage;

    // Safari, in Private Browsing Mode, looks like it supports localStorage but all calls to setItem
    // throw QuotaExceededError. We're going to detect this and avoid hard to debug edge cases.
    __webpack_require__.g.localStorage.setItem('__scLocalStorageTest', 1);
    __webpack_require__.g.localStorage.removeItem('__scLocalStorageTest');
  } catch (e) {
    err = e;
  }
  return !err;
};

AuthEngine.prototype.saveToken = function (name, token, options, callback) {
  if (this.isLocalStorageEnabled && __webpack_require__.g.localStorage) {
    __webpack_require__.g.localStorage.setItem(name, token);
  } else {
    this._internalStorage[name] = token;
  }
  callback && callback(null, token);
};

AuthEngine.prototype.removeToken = function (name, callback) {
  var token;

  this.loadToken(name, function (err, authToken) {
    token = authToken;
  });

  if (this.isLocalStorageEnabled && __webpack_require__.g.localStorage) {
    __webpack_require__.g.localStorage.removeItem(name);
  } else {
    delete this._internalStorage[name];
  }

  callback && callback(null, token);
};

AuthEngine.prototype.loadToken = function (name, callback) {
  var token;

  if (this.isLocalStorageEnabled && __webpack_require__.g.localStorage) {
    token = __webpack_require__.g.localStorage.getItem(name);
  } else {
    token = this._internalStorage[name] || null;
  }
  callback(null, token);
};

module.exports.K = AuthEngine;


/***/ }),

/***/ 9620:
/*!**********************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/factory.js ***!
  \**********************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var SCClientSocket = __webpack_require__(/*! ./scclientsocket */ 4363);
var scErrors = __webpack_require__(/*! sc-errors */ 5588);
var uuid = __webpack_require__(/*! uuid */ 6205);
var InvalidArgumentsError = scErrors.InvalidArgumentsError;

var _clients = {};

function getMultiplexId(options) {
  var protocolPrefix = options.secure ? 'https://' : 'http://';
  var queryString = '';
  if (options.query) {
    if (typeof options.query == 'string') {
      queryString = options.query;
    } else {
      var queryArray = [];
      var queryMap = options.query;
      for (var key in queryMap) {
        if (queryMap.hasOwnProperty(key)) {
          queryArray.push(key + '=' + queryMap[key]);
        }
      }
      if (queryArray.length) {
        queryString = '?' + queryArray.join('&');
      }
    }
  }
  var host;
  if (options.host) {
    host = options.host;
  } else {
    host = options.hostname + ':' + options.port;
  }
  return protocolPrefix + host + options.path + queryString;
}

function isUrlSecure() {
  return __webpack_require__.g.location && location.protocol == 'https:';
}

function getPort(options, isSecureDefault) {
  var isSecure = options.secure == null ? isSecureDefault : options.secure;
  return options.port || (__webpack_require__.g.location && location.port ? location.port : isSecure ? 443 : 80);
}

function create(options) {
  var self = this;

  options = options || {};

  if (options.host && !options.host.match(/[^:]+:\d{2,5}/)) {
    throw new InvalidArgumentsError('The host option should include both' +
      ' the hostname and the port number in the format "hostname:port"');
  }

  if (options.host && options.hostname) {
    throw new InvalidArgumentsError('The host option should already include' +
      ' the hostname and the port number in the format "hostname:port"' +
      ' - Because of this, you should never use host and hostname options together');
  }

  if (options.host && options.port) {
    throw new InvalidArgumentsError('The host option should already include' +
      ' the hostname and the port number in the format "hostname:port"' +
      ' - Because of this, you should never use host and port options together');
  }

  var isSecureDefault = isUrlSecure();

  var opts = {
    port: getPort(options, isSecureDefault),
    hostname: __webpack_require__.g.location && location.hostname || 'localhost',
    path: '/socketcluster/',
    secure: isSecureDefault,
    autoConnect: true,
    autoReconnect: true,
    autoSubscribeOnConnect: true,
    connectTimeout: 20000,
    ackTimeout: 10000,
    timestampRequests: false,
    timestampParam: 't',
    authEngine: null,
    authTokenName: 'socketCluster.authToken',
    binaryType: 'arraybuffer',
    multiplex: true,
    pubSubBatchDuration: null,
    cloneData: false
  };
  for (var i in options) {
    if (options.hasOwnProperty(i)) {
      opts[i] = options[i];
    }
  }
  opts.clientMap = _clients;

  if (opts.multiplex === false) {
    opts.clientId = uuid.v4();
    var socket = new SCClientSocket(opts);
    _clients[opts.clientId] = socket;
    return socket;
  }
  opts.clientId = getMultiplexId(opts);

  if (_clients[opts.clientId]) {
    if (opts.autoConnect) {
      _clients[opts.clientId].connect();
    }
  } else {
    _clients[opts.clientId] = new SCClientSocket(opts);
  }
  return _clients[opts.clientId];
}

function destroy(socket) {
  socket.destroy();
}

module.exports = {
  create: create,
  destroy: destroy,
  clients: _clients
};


/***/ }),

/***/ 9165:
/*!***********************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/response.js ***!
  \***********************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var scErrors = __webpack_require__(/*! sc-errors */ 5588);
var InvalidActionError = scErrors.InvalidActionError;

var Response = function (socket, id) {
  this.socket = socket;
  this.id = id;
  this.sent = false;
};

Response.prototype._respond = function (responseData) {
  if (this.sent) {
    throw new InvalidActionError('Response ' + this.id + ' has already been sent');
  } else {
    this.sent = true;
    this.socket.send(this.socket.encode(responseData));
  }
};

Response.prototype.end = function (data) {
  if (this.id) {
    var responseData = {
      rid: this.id
    };
    if (data !== undefined) {
      responseData.data = data;
    }
    this._respond(responseData);
  }
};

Response.prototype.error = function (error, data) {
  if (this.id) {
    var err = scErrors.dehydrateError(error);

    var responseData = {
      rid: this.id,
      error: err
    };
    if (data !== undefined) {
      responseData.data = data;
    }

    this._respond(responseData);
  }
};

Response.prototype.callback = function (error, data) {
  if (error) {
    this.error(error, data);
  } else {
    this.end(data);
  }
};

module.exports.H = Response;


/***/ }),

/***/ 4363:
/*!*****************************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/scclientsocket.js ***!
  \*****************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var Emitter = __webpack_require__(/*! component-emitter */ 2150);
var SCChannel = __webpack_require__(/*! sc-channel */ 6125)/* .SCChannel */ .X;
var Response = __webpack_require__(/*! ./response */ 9165)/* .Response */ .H;
var AuthEngine = __webpack_require__(/*! ./auth */ 4539)/* .AuthEngine */ .K;
var formatter = __webpack_require__(/*! sc-formatter */ 2973);
var SCTransport = __webpack_require__(/*! ./sctransport */ 4868)/* .SCTransport */ .U;
var querystring = __webpack_require__(/*! querystring */ 6774);
var LinkedList = __webpack_require__(/*! linked-list */ 1441);
var base64 = __webpack_require__(/*! base-64 */ 8313);
var clone = __webpack_require__(/*! clone */ 3305);

var scErrors = __webpack_require__(/*! sc-errors */ 5588);
var InvalidArgumentsError = scErrors.InvalidArgumentsError;
var InvalidMessageError = scErrors.InvalidMessageError;
var InvalidActionError = scErrors.InvalidActionError;
var SocketProtocolError = scErrors.SocketProtocolError;
var TimeoutError = scErrors.TimeoutError;
var BadConnectionError = scErrors.BadConnectionError;

var isBrowser = typeof window != 'undefined';


var SCClientSocket = function (opts) {
  var self = this;

  Emitter.call(this);

  this.id = null;
  this.state = this.CLOSED;
  this.authState = this.UNAUTHENTICATED;
  this.signedAuthToken = null;
  this.authToken = null;
  this.pendingReconnect = false;
  this.pendingReconnectTimeout = null;
  this.preparingPendingSubscriptions = false;
  this.clientId = opts.clientId;

  this.connectTimeout = opts.connectTimeout;
  this.ackTimeout = opts.ackTimeout;
  this.channelPrefix = opts.channelPrefix || null;
  this.disconnectOnUnload = opts.disconnectOnUnload == null ? true : opts.disconnectOnUnload;
  this.authTokenName = opts.authTokenName;

  // pingTimeout will be ackTimeout at the start, but it will
  // be updated with values provided by the 'connect' event
  this.pingTimeout = this.ackTimeout;
  this.pingTimeoutDisabled = !!opts.pingTimeoutDisabled;
  this.active = true;

  this._clientMap = opts.clientMap || {};

  var maxTimeout = Math.pow(2, 31) - 1;

  var verifyDuration = function (propertyName) {
    if (self[propertyName] > maxTimeout) {
      throw new InvalidArgumentsError('The ' + propertyName +
        ' value provided exceeded the maximum amount allowed');
    }
  };

  verifyDuration('connectTimeout');
  verifyDuration('ackTimeout');

  this._localEvents = {
    'connect': 1,
    'connectAbort': 1,
    'close': 1,
    'disconnect': 1,
    'message': 1,
    'error': 1,
    'raw': 1,
    'kickOut': 1,
    'subscribe': 1,
    'unsubscribe': 1,
    'subscribeStateChange': 1,
    'authStateChange': 1,
    'authenticate': 1,
    'deauthenticate': 1,
    'removeAuthToken': 1,
    'subscribeRequest': 1
  };

  this.connectAttempts = 0;

  this._emitBuffer = new LinkedList();
  this.channels = {};

  this.options = opts;

  this._cid = 1;

  this.options.callIdGenerator = function () {
    return self._cid++;
  };

  if (this.options.autoReconnect) {
    if (this.options.autoReconnectOptions == null) {
      this.options.autoReconnectOptions = {};
    }

    // Add properties to the this.options.autoReconnectOptions object.
    // We assign the reference to a reconnectOptions variable to avoid repetition.
    var reconnectOptions = this.options.autoReconnectOptions;
    if (reconnectOptions.initialDelay == null) {
      reconnectOptions.initialDelay = 10000;
    }
    if (reconnectOptions.randomness == null) {
      reconnectOptions.randomness = 10000;
    }
    if (reconnectOptions.multiplier == null) {
      reconnectOptions.multiplier = 1.5;
    }
    if (reconnectOptions.maxDelay == null) {
      reconnectOptions.maxDelay = 60000;
    }
  }

  if (this.options.subscriptionRetryOptions == null) {
    this.options.subscriptionRetryOptions = {};
  }

  if (this.options.authEngine) {
    this.auth = this.options.authEngine;
  } else {
    this.auth = new AuthEngine();
  }

  if (this.options.codecEngine) {
    this.codec = this.options.codecEngine;
  } else {
    // Default codec engine
    this.codec = formatter;
  }

  this.options.path = this.options.path.replace(/\/$/, '') + '/';

  this.options.query = opts.query || {};
  if (typeof this.options.query == 'string') {
    this.options.query = querystring.parse(this.options.query);
  }

  this._channelEmitter = new Emitter();

  this._unloadHandler = function () {
    self.disconnect();
  };

  if (isBrowser && this.disconnectOnUnload && __webpack_require__.g.addEventListener) {
    __webpack_require__.g.addEventListener('beforeunload', this._unloadHandler, false);
  }
  this._clientMap[this.clientId] = this;

  if (this.options.autoConnect) {
    this.connect();
  }
};

SCClientSocket.prototype = Object.create(Emitter.prototype);

SCClientSocket.CONNECTING = SCClientSocket.prototype.CONNECTING = SCTransport.prototype.CONNECTING;
SCClientSocket.OPEN = SCClientSocket.prototype.OPEN = SCTransport.prototype.OPEN;
SCClientSocket.CLOSED = SCClientSocket.prototype.CLOSED = SCTransport.prototype.CLOSED;

SCClientSocket.AUTHENTICATED = SCClientSocket.prototype.AUTHENTICATED = 'authenticated';
SCClientSocket.UNAUTHENTICATED = SCClientSocket.prototype.UNAUTHENTICATED = 'unauthenticated';

SCClientSocket.PENDING = SCClientSocket.prototype.PENDING = 'pending';

SCClientSocket.ignoreStatuses = scErrors.socketProtocolIgnoreStatuses;
SCClientSocket.errorStatuses = scErrors.socketProtocolErrorStatuses;

SCClientSocket.prototype._privateEventHandlerMap = {
  '#publish': function (data) {
    var undecoratedChannelName = this._undecorateChannelName(data.channel);
    var isSubscribed = this.isSubscribed(undecoratedChannelName, true);

    if (isSubscribed) {
      this._channelEmitter.emit(undecoratedChannelName, data.data);
    }
  },
  '#kickOut': function (data) {
    var undecoratedChannelName = this._undecorateChannelName(data.channel);
    var channel = this.channels[undecoratedChannelName];
    if (channel) {
      Emitter.prototype.emit.call(this, 'kickOut', data.message, undecoratedChannelName);
      channel.emit('kickOut', data.message, undecoratedChannelName);
      this._triggerChannelUnsubscribe(channel);
    }
  },
  '#setAuthToken': function (data, response) {
    var self = this;

    if (data) {
      var triggerAuthenticate = function (err) {
        if (err) {
          // This is a non-fatal error, we don't want to close the connection
          // because of this but we do want to notify the server and throw an error
          // on the client.
          response.error(err);
          self._onSCError(err);
        } else {
          self._changeToAuthenticatedState(data.token);
          response.end();
        }
      };

      this.auth.saveToken(this.authTokenName, data.token, {}, triggerAuthenticate);
    } else {
      response.error(new InvalidMessageError('No token data provided by #setAuthToken event'));
    }
  },
  '#removeAuthToken': function (data, response) {
    var self = this;

    this.auth.removeToken(this.authTokenName, function (err, oldToken) {
      if (err) {
        // Non-fatal error - Do not close the connection
        response.error(err);
        self._onSCError(err);
      } else {
        Emitter.prototype.emit.call(self, 'removeAuthToken', oldToken);
        self._changeToUnauthenticatedStateAndClearTokens();
        response.end();
      }
    });
  },
  '#disconnect': function (data) {
    this.transport.close(data.code, data.data);
  }
};

SCClientSocket.prototype.getState = function () {
  return this.state;
};

SCClientSocket.prototype.getBytesReceived = function () {
  return this.transport.getBytesReceived();
};

SCClientSocket.prototype.deauthenticate = function (callback) {
  var self = this;

  this.auth.removeToken(this.authTokenName, function (err, oldToken) {
    if (err) {
      // Non-fatal error - Do not close the connection
      self._onSCError(err);
    } else {
      Emitter.prototype.emit.call(self, 'removeAuthToken', oldToken);
      if (self.state != self.CLOSED) {
        self.emit('#removeAuthToken');
      }
      self._changeToUnauthenticatedStateAndClearTokens();
    }
    callback && callback(err);
  });
};

SCClientSocket.prototype.connect = SCClientSocket.prototype.open = function () {
  var self = this;

  if (!this.active) {
    var error = new InvalidActionError('Cannot connect a destroyed client');
    this._onSCError(error);
    return;
  }

  if (this.state == this.CLOSED) {
    this.pendingReconnect = false;
    this.pendingReconnectTimeout = null;
    clearTimeout(this._reconnectTimeoutRef);

    this.state = this.CONNECTING;
    Emitter.prototype.emit.call(this, 'connecting');

    if (this.transport) {
      this.transport.off();
    }

    this.transport = new SCTransport(this.auth, this.codec, this.options);

    this.transport.on('open', function (status) {
      self.state = self.OPEN;
      self._onSCOpen(status);
    });

    this.transport.on('error', function (err) {
      self._onSCError(err);
    });

    this.transport.on('close', function (code, data) {
      self.state = self.CLOSED;
      self._onSCClose(code, data);
    });

    this.transport.on('openAbort', function (code, data) {
      self.state = self.CLOSED;
      self._onSCClose(code, data, true);
    });

    this.transport.on('event', function (event, data, res) {
      self._onSCEvent(event, data, res);
    });
  }
};

SCClientSocket.prototype.reconnect = function (code, data) {
  this.disconnect(code, data);
  this.connect();
};

SCClientSocket.prototype.disconnect = function (code, data) {
  code = code || 1000;

  if (typeof code != 'number') {
    throw new InvalidArgumentsError('If specified, the code argument must be a number');
  }

  if (this.state == this.OPEN || this.state == this.CONNECTING) {
    this.transport.close(code, data);
  } else {
    this.pendingReconnect = false;
    this.pendingReconnectTimeout = null;
    clearTimeout(this._reconnectTimeoutRef);
  }
};

SCClientSocket.prototype.destroy = function (code, data) {
  if (isBrowser && __webpack_require__.g.removeEventListener) {
    __webpack_require__.g.removeEventListener('beforeunload', this._unloadHandler, false);
  }
  this.active = false;
  this.disconnect(code, data);
  delete this._clientMap[this.clientId];
};

SCClientSocket.prototype._changeToUnauthenticatedStateAndClearTokens = function () {
  if (this.authState != this.UNAUTHENTICATED) {
    var oldState = this.authState;
    var oldSignedToken = this.signedAuthToken;
    this.authState = this.UNAUTHENTICATED;
    this.signedAuthToken = null;
    this.authToken = null;

    var stateChangeData = {
      oldState: oldState,
      newState: this.authState
    };
    Emitter.prototype.emit.call(this, 'authStateChange', stateChangeData);
    Emitter.prototype.emit.call(this, 'deauthenticate', oldSignedToken);
  }
};

SCClientSocket.prototype._changeToAuthenticatedState = function (signedAuthToken) {
  this.signedAuthToken = signedAuthToken;
  this.authToken = this._extractAuthTokenData(signedAuthToken);

  if (this.authState != this.AUTHENTICATED) {
    var oldState = this.authState;
    this.authState = this.AUTHENTICATED;
    var stateChangeData = {
      oldState: oldState,
      newState: this.authState,
      signedAuthToken: signedAuthToken,
      authToken: this.authToken
    };
    if (!this.preparingPendingSubscriptions) {
      this.processPendingSubscriptions();
    }

    Emitter.prototype.emit.call(this, 'authStateChange', stateChangeData);
  }
  Emitter.prototype.emit.call(this, 'authenticate', signedAuthToken);
};

SCClientSocket.prototype.decodeBase64 = function (encodedString) {
  var decodedString;
  if (typeof Buffer == 'undefined') {
    if (__webpack_require__.g.atob) {
      decodedString = __webpack_require__.g.atob(encodedString);
    } else {
      decodedString = base64.decode(encodedString);
    }
  } else {
    var buffer = new Buffer(encodedString, 'base64');
    decodedString = buffer.toString('utf8');
  }
  return decodedString;
};

SCClientSocket.prototype.encodeBase64 = function (decodedString) {
  var encodedString;
  if (typeof Buffer == 'undefined') {
    if (__webpack_require__.g.btoa) {
      encodedString = __webpack_require__.g.btoa(decodedString);
    } else {
      encodedString = base64.encode(decodedString);
    }
  } else {
    var buffer = new Buffer(decodedString, 'utf8');
    encodedString = buffer.toString('base64');
  }
  return encodedString;
};

SCClientSocket.prototype._extractAuthTokenData = function (signedAuthToken) {
  var tokenParts = (signedAuthToken || '').split('.');
  var encodedTokenData = tokenParts[1];
  if (encodedTokenData != null) {
    var tokenData = encodedTokenData;
    try {
      tokenData = this.decodeBase64(tokenData);
      return JSON.parse(tokenData);
    } catch (e) {
      return tokenData;
    }
  }
  return null;
};

SCClientSocket.prototype.getAuthToken = function () {
  return this.authToken;
};

SCClientSocket.prototype.getSignedAuthToken = function () {
  return this.signedAuthToken;
};

// Perform client-initiated authentication by providing an encrypted token string.
SCClientSocket.prototype.authenticate = function (signedAuthToken, callback) {
  var self = this;

  this.emit('#authenticate', signedAuthToken, function (err, authStatus) {
    if (authStatus && authStatus.isAuthenticated != null) {
      // If authStatus is correctly formatted (has an isAuthenticated property),
      // then we will rehydrate the authError.
      if (authStatus.authError) {
        authStatus.authError = scErrors.hydrateError(authStatus.authError);
      }
    } else {
      // Some errors like BadConnectionError and TimeoutError will not pass a valid
      // authStatus object to the current function, so we need to create it ourselves.
      authStatus = {
        isAuthenticated: self.authState,
        authError: null
      };
    }
    if (err) {
      if (err.name != 'BadConnectionError' && err.name != 'TimeoutError') {
        // In case of a bad/closed connection or a timeout, we maintain the last
        // known auth state since those errors don't mean that the token is invalid.

        self._changeToUnauthenticatedStateAndClearTokens();
      }
      callback && callback(err, authStatus);
    } else {
      self.auth.saveToken(self.authTokenName, signedAuthToken, {}, function (err) {
        if (err) {
          self._onSCError(err);
        }
        if (authStatus.isAuthenticated) {
          self._changeToAuthenticatedState(signedAuthToken);
        } else {
          self._changeToUnauthenticatedStateAndClearTokens();
        }
        callback && callback(err, authStatus);
      });
    }
  });
};

SCClientSocket.prototype._tryReconnect = function (initialDelay) {
  var self = this;

  var exponent = this.connectAttempts++;
  var reconnectOptions = this.options.autoReconnectOptions;
  var timeout;

  if (initialDelay == null || exponent > 0) {
    var initialTimeout = Math.round(reconnectOptions.initialDelay + (reconnectOptions.randomness || 0) * Math.random());

    timeout = Math.round(initialTimeout * Math.pow(reconnectOptions.multiplier, exponent));
  } else {
    timeout = initialDelay;
  }

  if (timeout > reconnectOptions.maxDelay) {
    timeout = reconnectOptions.maxDelay;
  }

  clearTimeout(this._reconnectTimeoutRef);

  this.pendingReconnect = true;
  this.pendingReconnectTimeout = timeout;
  this._reconnectTimeoutRef = setTimeout(function () {
    self.connect();
  }, timeout);
};

SCClientSocket.prototype._onSCOpen = function (status) {
  var self = this;

  this.preparingPendingSubscriptions = true;

  if (status) {
    this.id = status.id;
    this.pingTimeout = status.pingTimeout;
    this.transport.pingTimeout = this.pingTimeout;
    if (status.isAuthenticated) {
      this._changeToAuthenticatedState(status.authToken);
    } else {
      this._changeToUnauthenticatedStateAndClearTokens();
    }
  } else {
    // This can happen if auth.loadToken (in sctransport.js) fails with
    // an error - This means that the signedAuthToken cannot be loaded by
    // the auth engine and therefore, we need to unauthenticate the client.
    this._changeToUnauthenticatedStateAndClearTokens();
  }

  this.connectAttempts = 0;

  if (this.options.autoSubscribeOnConnect) {
    this.processPendingSubscriptions();
  }

  // If the user invokes the callback while in autoSubscribeOnConnect mode, it
  // won't break anything.
  Emitter.prototype.emit.call(this, 'connect', status, function () {
    self.processPendingSubscriptions();
  });

  if (this.state == this.OPEN) {
    this._flushEmitBuffer();
  }
};

SCClientSocket.prototype._onSCError = function (err) {
  var self = this;

  // Throw error in different stack frame so that error handling
  // cannot interfere with a reconnect action.
  setTimeout(function () {
    if (self.listeners('error').length < 1) {
      throw err;
    } else {
      Emitter.prototype.emit.call(self, 'error', err);
    }
  }, 0);
};

SCClientSocket.prototype._suspendSubscriptions = function () {
  var channel, newState;
  for (var channelName in this.channels) {
    if (this.channels.hasOwnProperty(channelName)) {
      channel = this.channels[channelName];
      if (channel.state == channel.SUBSCRIBED ||
        channel.state == channel.PENDING) {

        newState = channel.PENDING;
      } else {
        newState = channel.UNSUBSCRIBED;
      }

      this._triggerChannelUnsubscribe(channel, newState);
    }
  }
};

SCClientSocket.prototype._abortAllPendingEventsDueToBadConnection = function (failureType) {
  var currentNode = this._emitBuffer.head;
  var nextNode;

  while (currentNode) {
    nextNode = currentNode.next;
    var eventObject = currentNode.data;
    clearTimeout(eventObject.timeout);
    delete eventObject.timeout;
    currentNode.detach();
    currentNode = nextNode;

    var callback = eventObject.callback;
    if (callback) {
      delete eventObject.callback;
      var errorMessage = "Event '" + eventObject.event +
        "' was aborted due to a bad connection";
      var error = new BadConnectionError(errorMessage, failureType);
      callback.call(eventObject, error, eventObject);
    }
    // Cleanup any pending response callback in the transport layer too.
    if (eventObject.cid) {
      this.transport.cancelPendingResponse(eventObject.cid);
    }
  }
};

SCClientSocket.prototype._onSCClose = function (code, data, openAbort) {
  var self = this;

  this.id = null;

  if (this.transport) {
    this.transport.off();
  }
  this.pendingReconnect = false;
  this.pendingReconnectTimeout = null;
  clearTimeout(this._reconnectTimeoutRef);

  this._suspendSubscriptions();
  this._abortAllPendingEventsDueToBadConnection(openAbort ? 'connectAbort' : 'disconnect');

  // Try to reconnect
  // on server ping timeout (4000)
  // or on client pong timeout (4001)
  // or on close without status (1005)
  // or on handshake failure (4003)
  // or on handshake rejection (4008)
  // or on socket hung up (1006)
  if (this.options.autoReconnect) {
    if (code == 4000 || code == 4001 || code == 1005) {
      // If there is a ping or pong timeout or socket closes without
      // status, don't wait before trying to reconnect - These could happen
      // if the client wakes up after a period of inactivity and in this case we
      // want to re-establish the connection as soon as possible.
      this._tryReconnect(0);

      // Codes 4500 and above will be treated as permanent disconnects.
      // Socket will not try to auto-reconnect.
    } else if (code != 1000 && code < 4500) {
      this._tryReconnect();
    }
  }

  if (openAbort) {
    Emitter.prototype.emit.call(self, 'connectAbort', code, data);
  } else {
    Emitter.prototype.emit.call(self, 'disconnect', code, data);
  }
  Emitter.prototype.emit.call(self, 'close', code, data);

  if (!SCClientSocket.ignoreStatuses[code]) {
    var closeMessage;
    if (data) {
      closeMessage = 'Socket connection closed with status code ' + code + ' and reason: ' + data;
    } else {
      closeMessage = 'Socket connection closed with status code ' + code;
    }
    var err = new SocketProtocolError(SCClientSocket.errorStatuses[code] || closeMessage, code);
    this._onSCError(err);
  }
};

SCClientSocket.prototype._onSCEvent = function (event, data, res) {
  var handler = this._privateEventHandlerMap[event];
  if (handler) {
    handler.call(this, data, res);
  } else {
    Emitter.prototype.emit.call(this, event, data, function () {
      res && res.callback.apply(res, arguments);
    });
  }
};

SCClientSocket.prototype.decode = function (message) {
  return this.transport.decode(message);
};

SCClientSocket.prototype.encode = function (object) {
  return this.transport.encode(object);
};

SCClientSocket.prototype._flushEmitBuffer = function () {
  var currentNode = this._emitBuffer.head;
  var nextNode;

  while (currentNode) {
    nextNode = currentNode.next;
    var eventObject = currentNode.data;
    currentNode.detach();
    this.transport.emitObject(eventObject);
    currentNode = nextNode;
  }
};

SCClientSocket.prototype._handleEventAckTimeout = function (eventObject, eventNode) {
  if (eventNode) {
    eventNode.detach();
  }
  delete eventObject.timeout;

  var callback = eventObject.callback;
  if (callback) {
    delete eventObject.callback;
    var error = new TimeoutError("Event response for '" + eventObject.event + "' timed out");
    callback.call(eventObject, error, eventObject);
  }
  // Cleanup any pending response callback in the transport layer too.
  if (eventObject.cid) {
    this.transport.cancelPendingResponse(eventObject.cid);
  }
};

SCClientSocket.prototype._emit = function (event, data, callback) {
  var self = this;

  if (this.state == this.CLOSED) {
    this.connect();
  }
  var eventObject = {
    event: event,
    callback: callback
  };

  var eventNode = new LinkedList.Item();

  if (this.options.cloneData) {
    eventObject.data = clone(data);
  } else {
    eventObject.data = data;
  }
  eventNode.data = eventObject;

  eventObject.timeout = setTimeout(function () {
    self._handleEventAckTimeout(eventObject, eventNode);
  }, this.ackTimeout);

  this._emitBuffer.append(eventNode);
  if (this.state == this.OPEN) {
    this._flushEmitBuffer();
  }
};

SCClientSocket.prototype.send = function (data) {
  this.transport.send(data);
};

SCClientSocket.prototype.emit = function (event, data, callback) {
  if (this._localEvents[event] == null) {
    this._emit(event, data, callback);
  } else if (event == 'error') {
    Emitter.prototype.emit.call(this, event, data);
  } else {
    var error = new InvalidActionError('The "' + event + '" event is reserved and cannot be emitted on a client socket');
    this._onSCError(error);
  }
};

SCClientSocket.prototype.publish = function (channelName, data, callback) {
  var pubData = {
    channel: this._decorateChannelName(channelName),
    data: data
  };
  this.emit('#publish', pubData, callback);
};

SCClientSocket.prototype._triggerChannelSubscribe = function (channel, subscriptionOptions) {
  var channelName = channel.name;

  if (channel.state != channel.SUBSCRIBED) {
    var oldState = channel.state;
    channel.state = channel.SUBSCRIBED;

    var stateChangeData = {
      channel: channelName,
      oldState: oldState,
      newState: channel.state,
      subscriptionOptions: subscriptionOptions
    };
    channel.emit('subscribeStateChange', stateChangeData);
    channel.emit('subscribe', channelName, subscriptionOptions);
    Emitter.prototype.emit.call(this, 'subscribeStateChange', stateChangeData);
    Emitter.prototype.emit.call(this, 'subscribe', channelName, subscriptionOptions);
  }
};

SCClientSocket.prototype._triggerChannelSubscribeFail = function (err, channel, subscriptionOptions) {
  var channelName = channel.name;
  var meetsAuthRequirements = !channel.waitForAuth || this.authState == this.AUTHENTICATED;

  if (channel.state != channel.UNSUBSCRIBED && meetsAuthRequirements) {
    channel.state = channel.UNSUBSCRIBED;

    channel.emit('subscribeFail', err, channelName, subscriptionOptions);
    Emitter.prototype.emit.call(this, 'subscribeFail', err, channelName, subscriptionOptions);
  }
};

// Cancel any pending subscribe callback
SCClientSocket.prototype._cancelPendingSubscribeCallback = function (channel) {
  if (channel._pendingSubscriptionCid != null) {
    this.transport.cancelPendingResponse(channel._pendingSubscriptionCid);
    delete channel._pendingSubscriptionCid;
  }
};

SCClientSocket.prototype._decorateChannelName = function (channelName) {
  if (this.channelPrefix) {
    channelName = this.channelPrefix + channelName;
  }
  return channelName;
};

SCClientSocket.prototype._undecorateChannelName = function (decoratedChannelName) {
  if (this.channelPrefix && decoratedChannelName.indexOf(this.channelPrefix) == 0) {
    return decoratedChannelName.replace(this.channelPrefix, '');
  }
  return decoratedChannelName;
};

SCClientSocket.prototype._trySubscribe = function (channel) {
  var self = this;

  var meetsAuthRequirements = !channel.waitForAuth || this.authState == this.AUTHENTICATED;

  // We can only ever have one pending subscribe action at any given time on a channel
  if (this.state == this.OPEN && !this.preparingPendingSubscriptions &&
    channel._pendingSubscriptionCid == null && meetsAuthRequirements) {

    var options = {
      noTimeout: true
    };

    var subscriptionOptions = {
      channel: this._decorateChannelName(channel.name)
    };
    if (channel.waitForAuth) {
      options.waitForAuth = true;
      subscriptionOptions.waitForAuth = options.waitForAuth;
    }
    if (channel.data) {
      subscriptionOptions.data = channel.data;
    }
    if (channel.batch) {
      options.batch = true;
      subscriptionOptions.batch = true;
    }

    channel._pendingSubscriptionCid = this.transport.emit(
      '#subscribe', subscriptionOptions, options,
      function (err) {
        delete channel._pendingSubscriptionCid;
        if (err) {
          self._triggerChannelSubscribeFail(err, channel, subscriptionOptions);
        } else {
          self._triggerChannelSubscribe(channel, subscriptionOptions);
        }
      }
    );
    Emitter.prototype.emit.call(this, 'subscribeRequest', channel.name, subscriptionOptions);
  }
};

SCClientSocket.prototype.subscribe = function (channelName, options) {
  var channel = this.channels[channelName];

  if (!channel) {
    channel = new SCChannel(channelName, this, options);
    this.channels[channelName] = channel;
  } else if (options) {
    channel.setOptions(options);
  }

  if (channel.state == channel.UNSUBSCRIBED) {
    channel.state = channel.PENDING;
    this._trySubscribe(channel);
  }

  return channel;
};

SCClientSocket.prototype._triggerChannelUnsubscribe = function (channel, newState) {
  var channelName = channel.name;
  var oldState = channel.state;

  if (newState) {
    channel.state = newState;
  } else {
    channel.state = channel.UNSUBSCRIBED;
  }
  this._cancelPendingSubscribeCallback(channel);

  if (oldState == channel.SUBSCRIBED) {
    var stateChangeData = {
      channel: channelName,
      oldState: oldState,
      newState: channel.state
    };
    channel.emit('subscribeStateChange', stateChangeData);
    channel.emit('unsubscribe', channelName);
    Emitter.prototype.emit.call(this, 'subscribeStateChange', stateChangeData);
    Emitter.prototype.emit.call(this, 'unsubscribe', channelName);
  }
};

SCClientSocket.prototype._tryUnsubscribe = function (channel) {
  var self = this;

  if (this.state == this.OPEN) {
    var options = {
      noTimeout: true
    };
    if (channel.batch) {
      options.batch = true;
    }
    // If there is a pending subscribe action, cancel the callback
    this._cancelPendingSubscribeCallback(channel);

    // This operation cannot fail because the TCP protocol guarantees delivery
    // so long as the connection remains open. If the connection closes,
    // the server will automatically unsubscribe the client and thus complete
    // the operation on the server side.
    var decoratedChannelName = this._decorateChannelName(channel.name);
    this.transport.emit('#unsubscribe', decoratedChannelName, options);
  }
};

SCClientSocket.prototype.unsubscribe = function (channelName) {
  var channel = this.channels[channelName];

  if (channel) {
    if (channel.state != channel.UNSUBSCRIBED) {

      this._triggerChannelUnsubscribe(channel);
      this._tryUnsubscribe(channel);
    }
  }
};

SCClientSocket.prototype.channel = function (channelName, options) {
  var currentChannel = this.channels[channelName];

  if (!currentChannel) {
    currentChannel = new SCChannel(channelName, this, options);
    this.channels[channelName] = currentChannel;
  }
  return currentChannel;
};

SCClientSocket.prototype.destroyChannel = function (channelName) {
  var channel = this.channels[channelName];

  if (channel) {
    channel.unwatch();
    channel.unsubscribe();
    delete this.channels[channelName];
  }
};

SCClientSocket.prototype.subscriptions = function (includePending) {
  var subs = [];
  var channel, includeChannel;
  for (var channelName in this.channels) {
    if (this.channels.hasOwnProperty(channelName)) {
      channel = this.channels[channelName];

      if (includePending) {
        includeChannel = channel && (channel.state == channel.SUBSCRIBED ||
          channel.state == channel.PENDING);
      } else {
        includeChannel = channel && channel.state == channel.SUBSCRIBED;
      }

      if (includeChannel) {
        subs.push(channelName);
      }
    }
  }
  return subs;
};

SCClientSocket.prototype.isSubscribed = function (channelName, includePending) {
  var channel = this.channels[channelName];
  if (includePending) {
    return !!channel && (channel.state == channel.SUBSCRIBED ||
      channel.state == channel.PENDING);
  }
  return !!channel && channel.state == channel.SUBSCRIBED;
};

SCClientSocket.prototype.processPendingSubscriptions = function () {
  var self = this;

  this.preparingPendingSubscriptions = false;

  var pendingChannels = [];

  for (var i in this.channels) {
    if (this.channels.hasOwnProperty(i)) {
      var channel = this.channels[i];
      if (channel.state == channel.PENDING) {
        pendingChannels.push(channel);
      }
    }
  }

  pendingChannels.sort(function (a, b) {
    var ap = a.priority || 0;
    var bp = b.priority || 0;
    if (ap > bp) {
      return -1;
    }
    if (ap < bp) {
      return 1;
    }
    return 0;
  });

  pendingChannels.forEach(function (channel) {
    self._trySubscribe(channel);
  });
};

SCClientSocket.prototype.watch = function (channelName, handler) {
  if (typeof handler != 'function') {
    throw new InvalidArgumentsError('No handler function was provided');
  }
  this._channelEmitter.on(channelName, handler);
};

SCClientSocket.prototype.unwatch = function (channelName, handler) {
  if (handler) {
    this._channelEmitter.removeListener(channelName, handler);
  } else {
    this._channelEmitter.removeAllListeners(channelName);
  }
};

SCClientSocket.prototype.watchers = function (channelName) {
  return this._channelEmitter.listeners(channelName);
};

module.exports = SCClientSocket;


/***/ }),

/***/ 4868:
/*!**************************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/sctransport.js ***!
  \**************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var Emitter = __webpack_require__(/*! component-emitter */ 2150);
var Response = __webpack_require__(/*! ./response */ 9165)/* .Response */ .H;
var querystring = __webpack_require__(/*! querystring */ 6774);
var WebSocket;
var createWebSocket;

if (__webpack_require__.g.WebSocket) {
  WebSocket = __webpack_require__.g.WebSocket;
  createWebSocket = function (uri, options) {
    return new WebSocket(uri);
  };
} else {
  WebSocket = __webpack_require__(/*! ws */ 5381);
  createWebSocket = function (uri, options) {
    return new WebSocket(uri, null, options);
  };
}

var scErrors = __webpack_require__(/*! sc-errors */ 5588);
var TimeoutError = scErrors.TimeoutError;
var BadConnectionError = scErrors.BadConnectionError;


var SCTransport = function (authEngine, codecEngine, options) {
  var self = this;

  this.state = this.CLOSED;
  this.auth = authEngine;
  this.codec = codecEngine;
  this.options = options;
  this.connectTimeout = options.connectTimeout;
  this.pingTimeout = options.ackTimeout;
  this.pingTimeoutDisabled = !!options.pingTimeoutDisabled;
  this.callIdGenerator = options.callIdGenerator;
  this.authTokenName = options.authTokenName;

  this._pingTimeoutTicker = null;
  this._callbackMap = {};
  this._batchSendList = [];

  // Open the connection.

  this.state = this.CONNECTING;
  var uri = this.uri();

  var wsSocket = createWebSocket(uri, this.options);
  wsSocket.binaryType = this.options.binaryType;

  this.socket = wsSocket;

  wsSocket.onopen = function () {
    self._onOpen();
  };

  wsSocket.onclose = function (event) {
    var code;
    if (event.code == null) {
      // This is to handle an edge case in React Native whereby
      // event.code is undefined when the mobile device is locked.
      // TODO: This is not perfect since this condition could also apply to
      // an abnormal close (no close control frame) which would be a 1006.
      code = 1005;
    } else {
      code = event.code;
    }
    self._onClose(code, event.reason);
  };

  wsSocket.onmessage = function (message, flags) {
    self._onMessage(message.data);
  };

  wsSocket.onerror = function (error) {
    // The onclose event will be called automatically after the onerror event
    // if the socket is connected - Otherwise, if it's in the middle of
    // connecting, we want to close it manually with a 1006 - This is necessary
    // to prevent inconsistent behavior when running the client in Node.js
    // vs in a browser.

    if (self.state === self.CONNECTING) {
      self._onClose(1006);
    }
  };

  this._connectTimeoutRef = setTimeout(function () {
    self._onClose(4007);
    self.socket.close(4007);
  }, this.connectTimeout);
};

SCTransport.prototype = Object.create(Emitter.prototype);

SCTransport.CONNECTING = SCTransport.prototype.CONNECTING = 'connecting';
SCTransport.OPEN = SCTransport.prototype.OPEN = 'open';
SCTransport.CLOSED = SCTransport.prototype.CLOSED = 'closed';

SCTransport.prototype.uri = function () {
  var query = this.options.query || {};
  var schema = this.options.secure ? 'wss' : 'ws';

  if (this.options.timestampRequests) {
    query[this.options.timestampParam] = (new Date()).getTime();
  }

  query = querystring.encode(query);

  if (query.length) {
    query = '?' + query;
  }

  var host;
  if (this.options.host) {
    host = this.options.host;
  } else {
    var port = '';

    if (this.options.port && ((schema == 'wss' && this.options.port != 443)
      || (schema == 'ws' && this.options.port != 80))) {
      port = ':' + this.options.port;
    }
    host = this.options.hostname + port;
  }

  return schema + '://' + host + this.options.path + query;
};

SCTransport.prototype._onOpen = function () {
  var self = this;

  clearTimeout(this._connectTimeoutRef);
  this._resetPingTimeout();

  this._handshake(function (err, status) {
    if (err) {
      var statusCode;
      if (status && status.code) {
        statusCode = status.code;
      } else {
        statusCode = 4003;
      }
      self._onError(err);
      self._onClose(statusCode, err.toString());
      self.socket.close(statusCode);
    } else {
      self.state = self.OPEN;
      Emitter.prototype.emit.call(self, 'open', status);
      self._resetPingTimeout();
    }
  });
};

SCTransport.prototype._handshake = function (callback) {
  var self = this;
  this.auth.loadToken(this.authTokenName, function (err, token) {
    if (err) {
      callback(err);
    } else {
      // Don't wait for this.state to be 'open'.
      // The underlying WebSocket (this.socket) is already open.
      var options = {
        force: true
      };
      self.emit('#handshake', {
        authToken: token
      }, options, function (err, status) {
        if (status) {
          // Add the token which was used as part of authentication attempt
          // to the status object.
          status.authToken = token;
          if (status.authError) {
            status.authError = scErrors.hydrateError(status.authError);
          }
        }
        callback(err, status);
      });
    }
  });
};

SCTransport.prototype._abortAllPendingEventsDueToBadConnection = function (failureType) {
  for (var i in this._callbackMap) {
    if (this._callbackMap.hasOwnProperty(i)) {
      var eventObject = this._callbackMap[i];
      delete this._callbackMap[i];

      clearTimeout(eventObject.timeout);
      delete eventObject.timeout;

      var errorMessage = "Event '" + eventObject.event +
        "' was aborted due to a bad connection";
      var badConnectionError = new BadConnectionError(errorMessage, failureType);

      var callback = eventObject.callback;
      delete eventObject.callback;
      callback.call(eventObject, badConnectionError, eventObject);
    }
  }
};

SCTransport.prototype._onClose = function (code, data) {
  delete this.socket.onopen;
  delete this.socket.onclose;
  delete this.socket.onmessage;
  delete this.socket.onerror;

  clearTimeout(this._connectTimeoutRef);
  clearTimeout(this._pingTimeoutTicker);
  clearTimeout(this._batchTimeout);

  if (this.state == this.OPEN) {
    this.state = this.CLOSED;
    Emitter.prototype.emit.call(this, 'close', code, data);
    this._abortAllPendingEventsDueToBadConnection('disconnect');

  } else if (this.state == this.CONNECTING) {
    this.state = this.CLOSED;
    Emitter.prototype.emit.call(this, 'openAbort', code, data);
    this._abortAllPendingEventsDueToBadConnection('connectAbort');
  }
};

SCTransport.prototype._handleEventObject = function (obj, message) {
  if (obj && obj.event != null) {
    var response = new Response(this, obj.cid);
    Emitter.prototype.emit.call(this, 'event', obj.event, obj.data, response);
  } else if (obj && obj.rid != null) {
    var eventObject = this._callbackMap[obj.rid];
    if (eventObject) {
      clearTimeout(eventObject.timeout);
      delete eventObject.timeout;
      delete this._callbackMap[obj.rid];

      if (eventObject.callback) {
        var rehydratedError = scErrors.hydrateError(obj.error);
        eventObject.callback(rehydratedError, obj.data);
      }
    }
  } else {
    Emitter.prototype.emit.call(this, 'event', 'raw', message);
  }
};

SCTransport.prototype._onMessage = function (message) {
  Emitter.prototype.emit.call(this, 'event', 'message', message);

  var obj = this.decode(message);

  // If ping
  if (obj == '#1') {
    this._resetPingTimeout();
    if (this.socket.readyState == this.socket.OPEN) {
      this.sendObject('#2');
    }
  } else {
    if (Array.isArray(obj)) {
      var len = obj.length;
      for (var i = 0; i < len; i++) {
        this._handleEventObject(obj[i], message);
      }
    } else {
      this._handleEventObject(obj, message);
    }
  }
};

SCTransport.prototype._onError = function (err) {
  Emitter.prototype.emit.call(this, 'error', err);
};

SCTransport.prototype._resetPingTimeout = function () {
  if (this.pingTimeoutDisabled) {
    return;
  }
  var self = this;

  var now = (new Date()).getTime();
  clearTimeout(this._pingTimeoutTicker);

  this._pingTimeoutTicker = setTimeout(function () {
    self._onClose(4000);
    self.socket.close(4000);
  }, this.pingTimeout);
};

SCTransport.prototype.getBytesReceived = function () {
  return this.socket.bytesReceived;
};

SCTransport.prototype.close = function (code, data) {
  code = code || 1000;

  if (this.state == this.OPEN) {
    var packet = {
      code: code,
      data: data
    };
    this.emit('#disconnect', packet);

    this._onClose(code, data);
    this.socket.close(code);

  } else if (this.state == this.CONNECTING) {
    this._onClose(code, data);
    this.socket.close(code);
  }
};

SCTransport.prototype.emitObject = function (eventObject, options) {
  var simpleEventObject = {
    event: eventObject.event,
    data: eventObject.data
  };

  if (eventObject.callback) {
    simpleEventObject.cid = eventObject.cid = this.callIdGenerator();
    this._callbackMap[eventObject.cid] = eventObject;
  }

  this.sendObject(simpleEventObject, options);

  return eventObject.cid || null;
};

SCTransport.prototype._handleEventAckTimeout = function (eventObject) {
  if (eventObject.cid) {
    delete this._callbackMap[eventObject.cid];
  }
  delete eventObject.timeout;

  var callback = eventObject.callback;
  if (callback) {
    delete eventObject.callback;
    var error = new TimeoutError("Event response for '" + eventObject.event + "' timed out");
    callback.call(eventObject, error, eventObject);
  }
};

// The last two optional arguments (a and b) can be options and/or callback
SCTransport.prototype.emit = function (event, data, a, b) {
  var self = this;

  var callback, options;

  if (b) {
    options = a;
    callback = b;
  } else {
    if (a instanceof Function) {
      options = {};
      callback = a;
    } else {
      options = a;
    }
  }

  var eventObject = {
    event: event,
    data: data,
    callback: callback
  };

  if (callback && !options.noTimeout) {
    eventObject.timeout = setTimeout(function () {
      self._handleEventAckTimeout(eventObject);
    }, this.options.ackTimeout);
  }

  var cid = null;
  if (this.state == this.OPEN || options.force) {
    cid = this.emitObject(eventObject, options);
  }
  return cid;
};

SCTransport.prototype.cancelPendingResponse = function (cid) {
  delete this._callbackMap[cid];
};

SCTransport.prototype.decode = function (message) {
  return this.codec.decode(message);
};

SCTransport.prototype.encode = function (object) {
  return this.codec.encode(object);
};

SCTransport.prototype.send = function (data) {
  if (this.socket.readyState != this.socket.OPEN) {
    this._onClose(1005);
  } else {
    this.socket.send(data);
  }
};

SCTransport.prototype.serializeObject = function (object) {
  var str, formatError;
  try {
    str = this.encode(object);
  } catch (err) {
    formatError = err;
    this._onError(formatError);
  }
  if (!formatError) {
    return str;
  }
  return null;
};

SCTransport.prototype.sendObjectBatch = function (object) {
  var self = this;

  this._batchSendList.push(object);
  if (this._batchTimeout) {
    return;
  }

  this._batchTimeout = setTimeout(function () {
    delete self._batchTimeout;
    if (self._batchSendList.length) {
      var str = self.serializeObject(self._batchSendList);
      if (str != null) {
        self.send(str);
      }
      self._batchSendList = [];
    }
  }, this.options.pubSubBatchDuration || 0);
};

SCTransport.prototype.sendObjectSingle = function (object) {
  var str = this.serializeObject(object);
  if (str != null) {
    this.send(str);
  }
};

SCTransport.prototype.sendObject = function (object, options) {
  if (options && options.batch) {
    this.sendObjectBatch(object);
  } else {
    this.sendObjectSingle(object);
  }
};

module.exports.U = SCTransport;


/***/ }),

/***/ 5381:
/*!*************************************************************!*\
  !*** ./node_modules/socketcluster-client/lib/ws-browser.js ***!
  \*************************************************************/
/***/ ((module) => {

var global;
if (typeof WorkerGlobalScope !== 'undefined') {
  global = self;
} else {
  global = typeof window != 'undefined' && window || (function() { return this; })();
}

var WebSocket = global.WebSocket || global.MozWebSocket;

/**
 * WebSocket constructor.
 *
 * The third `opts` options object gets ignored in web browsers, since it's
 * non-standard, and throws a TypeError if passed to the constructor.
 * See: https://github.com/einaros/ws/issues/227
 *
 * @param {String} uri
 * @param {Array} protocols (optional)
 * @param {Object} opts (optional)
 * @api public
 */

function ws(uri, protocols, opts) {
  var instance;
  if (protocols) {
    instance = new WebSocket(uri, protocols);
  } else {
    instance = new WebSocket(uri);
  }
  return instance;
}

if (WebSocket) ws.prototype = WebSocket.prototype;

module.exports = WebSocket ? ws : null;


/***/ }),

/***/ 6205:
/*!**********************************************************************!*\
  !*** ./node_modules/socketcluster-client/node_modules/uuid/index.js ***!
  \**********************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var v1 = __webpack_require__(/*! ./v1 */ 9390);
var v4 = __webpack_require__(/*! ./v4 */ 7547);

var uuid = v4;
uuid.v1 = v1;
uuid.v4 = v4;

module.exports = uuid;


/***/ }),

/***/ 4599:
/*!********************************************************************************!*\
  !*** ./node_modules/socketcluster-client/node_modules/uuid/lib/bytesToUuid.js ***!
  \********************************************************************************/
/***/ ((module) => {

/**
 * Convert array of 16 byte values to UUID string format of the form:
 * XXXXXXXX-XXXX-XXXX-XXXX-XXXXXXXXXXXX
 */
var byteToHex = [];
for (var i = 0; i < 256; ++i) {
  byteToHex[i] = (i + 0x100).toString(16).substr(1);
}

function bytesToUuid(buf, offset) {
  var i = offset || 0;
  var bth = byteToHex;
  return bth[buf[i++]] + bth[buf[i++]] +
          bth[buf[i++]] + bth[buf[i++]] + '-' +
          bth[buf[i++]] + bth[buf[i++]] + '-' +
          bth[buf[i++]] + bth[buf[i++]] + '-' +
          bth[buf[i++]] + bth[buf[i++]] + '-' +
          bth[buf[i++]] + bth[buf[i++]] +
          bth[buf[i++]] + bth[buf[i++]] +
          bth[buf[i++]] + bth[buf[i++]];
}

module.exports = bytesToUuid;


/***/ }),

/***/ 9341:
/*!********************************************************************************!*\
  !*** ./node_modules/socketcluster-client/node_modules/uuid/lib/rng-browser.js ***!
  \********************************************************************************/
/***/ ((module) => {

// Unique ID creation requires a high quality random # generator.  In the
// browser this is a little complicated due to unknown quality of Math.random()
// and inconsistent support for the `crypto` API.  We do the best we can via
// feature-detection

// getRandomValues needs to be invoked in a context where "this" is a Crypto implementation.
var getRandomValues = (typeof(crypto) != 'undefined' && crypto.getRandomValues.bind(crypto)) ||
                      (typeof(msCrypto) != 'undefined' && msCrypto.getRandomValues.bind(msCrypto));
if (getRandomValues) {
  // WHATWG crypto RNG - http://wiki.whatwg.org/wiki/Crypto
  var rnds8 = new Uint8Array(16); // eslint-disable-line no-undef

  module.exports = function whatwgRNG() {
    getRandomValues(rnds8);
    return rnds8;
  };
} else {
  // Math.random()-based (RNG)
  //
  // If all else fails, use Math.random().  It's fast, but is of unspecified
  // quality.
  var rnds = new Array(16);

  module.exports = function mathRNG() {
    for (var i = 0, r; i < 16; i++) {
      if ((i & 0x03) === 0) r = Math.random() * 0x100000000;
      rnds[i] = r >>> ((i & 0x03) << 3) & 0xff;
    }

    return rnds;
  };
}


/***/ }),

/***/ 9390:
/*!*******************************************************************!*\
  !*** ./node_modules/socketcluster-client/node_modules/uuid/v1.js ***!
  \*******************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var rng = __webpack_require__(/*! ./lib/rng */ 9341);
var bytesToUuid = __webpack_require__(/*! ./lib/bytesToUuid */ 4599);

// **`v1()` - Generate time-based UUID**
//
// Inspired by https://github.com/LiosK/UUID.js
// and http://docs.python.org/library/uuid.html

var _nodeId;
var _clockseq;

// Previous uuid creation time
var _lastMSecs = 0;
var _lastNSecs = 0;

// See https://github.com/broofa/node-uuid for API details
function v1(options, buf, offset) {
  var i = buf && offset || 0;
  var b = buf || [];

  options = options || {};
  var node = options.node || _nodeId;
  var clockseq = options.clockseq !== undefined ? options.clockseq : _clockseq;

  // node and clockseq need to be initialized to random values if they're not
  // specified.  We do this lazily to minimize issues related to insufficient
  // system entropy.  See #189
  if (node == null || clockseq == null) {
    var seedBytes = rng();
    if (node == null) {
      // Per 4.5, create and 48-bit node id, (47 random bits + multicast bit = 1)
      node = _nodeId = [
        seedBytes[0] | 0x01,
        seedBytes[1], seedBytes[2], seedBytes[3], seedBytes[4], seedBytes[5]
      ];
    }
    if (clockseq == null) {
      // Per 4.2.2, randomize (14 bit) clockseq
      clockseq = _clockseq = (seedBytes[6] << 8 | seedBytes[7]) & 0x3fff;
    }
  }

  // UUID timestamps are 100 nano-second units since the Gregorian epoch,
  // (1582-10-15 00:00).  JSNumbers aren't precise enough for this, so
  // time is handled internally as 'msecs' (integer milliseconds) and 'nsecs'
  // (100-nanoseconds offset from msecs) since unix epoch, 1970-01-01 00:00.
  var msecs = options.msecs !== undefined ? options.msecs : new Date().getTime();

  // Per 4.2.1.2, use count of uuid's generated during the current clock
  // cycle to simulate higher resolution clock
  var nsecs = options.nsecs !== undefined ? options.nsecs : _lastNSecs + 1;

  // Time since last uuid creation (in msecs)
  var dt = (msecs - _lastMSecs) + (nsecs - _lastNSecs)/10000;

  // Per 4.2.1.2, Bump clockseq on clock regression
  if (dt < 0 && options.clockseq === undefined) {
    clockseq = clockseq + 1 & 0x3fff;
  }

  // Reset nsecs if clock regresses (new clockseq) or we've moved onto a new
  // time interval
  if ((dt < 0 || msecs > _lastMSecs) && options.nsecs === undefined) {
    nsecs = 0;
  }

  // Per 4.2.1.2 Throw error if too many uuids are requested
  if (nsecs >= 10000) {
    throw new Error('uuid.v1(): Can\'t create more than 10M uuids/sec');
  }

  _lastMSecs = msecs;
  _lastNSecs = nsecs;
  _clockseq = clockseq;

  // Per 4.1.4 - Convert from unix epoch to Gregorian epoch
  msecs += 12219292800000;

  // `time_low`
  var tl = ((msecs & 0xfffffff) * 10000 + nsecs) % 0x100000000;
  b[i++] = tl >>> 24 & 0xff;
  b[i++] = tl >>> 16 & 0xff;
  b[i++] = tl >>> 8 & 0xff;
  b[i++] = tl & 0xff;

  // `time_mid`
  var tmh = (msecs / 0x100000000 * 10000) & 0xfffffff;
  b[i++] = tmh >>> 8 & 0xff;
  b[i++] = tmh & 0xff;

  // `time_high_and_version`
  b[i++] = tmh >>> 24 & 0xf | 0x10; // include version
  b[i++] = tmh >>> 16 & 0xff;

  // `clock_seq_hi_and_reserved` (Per 4.2.2 - include variant)
  b[i++] = clockseq >>> 8 | 0x80;

  // `clock_seq_low`
  b[i++] = clockseq & 0xff;

  // `node`
  for (var n = 0; n < 6; ++n) {
    b[i + n] = node[n];
  }

  return buf ? buf : bytesToUuid(b);
}

module.exports = v1;


/***/ }),

/***/ 7547:
/*!*******************************************************************!*\
  !*** ./node_modules/socketcluster-client/node_modules/uuid/v4.js ***!
  \*******************************************************************/
/***/ ((module, __unused_webpack_exports, __webpack_require__) => {

var rng = __webpack_require__(/*! ./lib/rng */ 9341);
var bytesToUuid = __webpack_require__(/*! ./lib/bytesToUuid */ 4599);

function v4(options, buf, offset) {
  var i = buf && offset || 0;

  if (typeof(options) == 'string') {
    buf = options === 'binary' ? new Array(16) : null;
    options = null;
  }
  options = options || {};

  var rnds = options.random || (options.rng || rng)();

  // Per 4.4, set bits for version and `clock_seq_hi_and_reserved`
  rnds[6] = (rnds[6] & 0x0f) | 0x40;
  rnds[8] = (rnds[8] & 0x3f) | 0x80;

  // Copy bytes to buffer, if provided
  if (buf) {
    for (var ii = 0; ii < 16; ++ii) {
      buf[i + ii] = rnds[ii];
    }
  }

  return buf || bytesToUuid(rnds);
}

module.exports = v4;


/***/ }),

/***/ 410:
/*!****************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/contentScriptBergamotApiClientPortListener.ts ***!
  \****************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.contentScriptBergamotApiClientPortListener = void 0;
const BergamotWasmApiClient_1 = __webpack_require__(/*! ./translation-api-clients/BergamotWasmApiClient */ 4222);
const BergamotRestApiClient_1 = __webpack_require__(/*! ./translation-api-clients/BergamotRestApiClient */ 484);
const config_1 = __webpack_require__(/*! ../../config */ 964);
// Currently it is possible build variants of the extension that uses the REST API - eg for performance testing / research
const bergamotApiClient = config_1.config.useBergamotRestApi
    ? new BergamotRestApiClient_1.BergamotRestApiClient()
    : new BergamotWasmApiClient_1.BergamotWasmApiClient();
const contentScriptBergamotApiClientPortListener = (port) => {
    if (port.name !== "port-from-content-script-bergamot-api-client") {
        return;
    }
    port.onMessage.addListener(function (m) {
        return __awaiter(this, void 0, void 0, function* () {
            // console.debug("Message from content-script-bergamot-api-client:", {m});
            const { texts, from, to, requestId } = m;
            try {
                const results = yield bergamotApiClient.sendTranslationRequest(texts, from, to, (translationRequestProgress) => {
                    const translationRequestUpdate = {
                        translationRequestProgress,
                        requestId,
                    };
                    port.postMessage({
                        translationRequestUpdate,
                    });
                });
                // console.log({ results });
                const translationRequestUpdate = {
                    results,
                    requestId,
                };
                port.postMessage({
                    translationRequestUpdate,
                });
            }
            catch (err) {
                if (err.message === "Attempt to postMessage on disconnected port") {
                    console.warn("Attempt to postMessage on disconnected port, but it is ok", err);
                }
                else {
                    console.info(`Caught exception/error in content script bergamot api client port listener:`, err);
                    // Make possibly unserializable errors serializable by only sending name, message and stack
                    let communicatedError;
                    if (err instanceof Error) {
                        const { name, message, stack } = err;
                        communicatedError = { name, message, stack };
                    }
                    else {
                        communicatedError = err;
                    }
                    const translationRequestUpdate = {
                        error: communicatedError,
                        requestId,
                    };
                    port.postMessage({
                        translationRequestUpdate,
                    });
                }
            }
        });
    });
};
exports.contentScriptBergamotApiClientPortListener = contentScriptBergamotApiClientPortListener;


/***/ }),

/***/ 220:
/*!********************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/contentScriptFrameInfoPortListener.ts ***!
  \********************************************************************************************/
/***/ (function(__unused_webpack_module, exports) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.contentScriptFrameInfoPortListener = void 0;
const contentScriptFrameInfoPortListener = (port) => {
    if (port.name !== "port-from-content-script-frame-info") {
        return;
    }
    port.onMessage.addListener(function (m, senderPort) {
        var _a, _b;
        return __awaiter(this, void 0, void 0, function* () {
            // console.debug("Message from port-from-content-script-frame-info:", {m});
            const { requestId } = m;
            const frameInfo = {
                windowId: (_a = senderPort.sender.tab) === null || _a === void 0 ? void 0 : _a.windowId,
                tabId: (_b = senderPort.sender.tab) === null || _b === void 0 ? void 0 : _b.id,
                frameId: senderPort.sender.frameId,
            };
            try {
                port.postMessage({
                    requestId,
                    frameInfo,
                });
            }
            catch (err) {
                if (err.message === "Attempt to postMessage on disconnected port") {
                    console.warn("Attempt to postMessage on disconnected port, but it is ok", err);
                }
                else {
                    throw err;
                }
            }
        });
    });
};
exports.contentScriptFrameInfoPortListener = contentScriptFrameInfoPortListener;


/***/ }),

/***/ 6047:
/*!********************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/contentScriptLanguageDetectorProxyPortListener.ts ***!
  \********************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.contentScriptLanguageDetectorProxyPortListener = void 0;
const LanguageDetector_1 = __webpack_require__(/*! ./lib/LanguageDetector */ 7991);
const contentScriptLanguageDetectorProxyPortListener = (port) => {
    if (port.name !== "port-from-content-script-language-detector-proxy") {
        return;
    }
    port.onMessage.addListener(function (m) {
        return __awaiter(this, void 0, void 0, function* () {
            // console.debug("Message from content-script-language-detector-proxy:", { m });
            const { str, requestId } = m;
            const results = yield LanguageDetector_1.LanguageDetector.detectLanguage({ text: str });
            // console.debug({ results });
            try {
                port.postMessage({
                    languageDetectorResults: {
                        results,
                        requestId,
                    },
                });
            }
            catch (err) {
                if (err.message === "Attempt to postMessage on disconnected port") {
                    console.warn("Attempt to postMessage on disconnected port, but it is ok", err);
                }
                else {
                    throw err;
                }
            }
        });
    });
};
exports.contentScriptLanguageDetectorProxyPortListener = contentScriptLanguageDetectorProxyPortListener;


/***/ }),

/***/ 7333:
/*!***********************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/lib/BergamotTranslatorAPI.ts ***!
  \***********************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BergamotTranslatorAPI = exports.BergamotTranslatorAPIModelDownloadError = exports.BergamotTranslatorAPIModelLoadError = exports.BergamotTranslatorAPITranslationError = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
const config_1 = __webpack_require__(/*! ../../../config */ 964);
// Since Emscripten can handle heap growth, but not heap shrinkage, we
// need to refresh the worker after we've loaded/processed large models/translations
// in order to prevent unnecessary resident memory growth.
//
// These values define the cut-off estimated heap growth size and the idle
// timeout (in milliseconds) before destroying a worker. Once the heap growth
// is estimated to have exceeded a certain size, the worker is marked for
// destruction, and is terminated as soon as it has been idle for the
// given timeout.
//
// TODO: Update to reflect relevant checks for translation-related heap growth
// const FOO_LIMIT = X * 1024 * 1024;
const IDLE_TIMEOUT = 10 * 1000;
const WORKER_URL = webextension_polyfill_ts_1.browser.runtime.getURL(`translation-worker.js`);
class BergamotTranslatorAPITranslationError extends Error {
    constructor() {
        super(...arguments);
        this.name = "BergamotTranslatorAPITranslationError";
    }
}
exports.BergamotTranslatorAPITranslationError = BergamotTranslatorAPITranslationError;
class BergamotTranslatorAPIModelLoadError extends Error {
    constructor() {
        super(...arguments);
        this.name = "BergamotTranslatorAPIModelLoadError";
    }
}
exports.BergamotTranslatorAPIModelLoadError = BergamotTranslatorAPIModelLoadError;
class BergamotTranslatorAPIModelDownloadError extends Error {
    constructor() {
        super(...arguments);
        this.name = "BergamotTranslatorAPIModelDownloadError";
    }
}
exports.BergamotTranslatorAPIModelDownloadError = BergamotTranslatorAPIModelDownloadError;
/**
 * Class responsible for instantiating and communicating between this script
 * and the translation worker process.
 */
class WorkerManager {
    constructor() {
        this.pendingRequests = new Map();
        // Holds the ID of the current pending idle cleanup setTimeout.
        this._idleTimeout = null;
    }
    // private estimatedHeapGrowth: number;
    loadModel(loadModelRequestWorkerMessage, onModelDownloadProgress) {
        return __awaiter(this, void 0, void 0, function* () {
            const worker = yield this.workerReady;
            const loadModelResults = yield new Promise((resolve, reject) => {
                const { requestId } = loadModelRequestWorkerMessage;
                this.pendingRequests.set(requestId, {
                    resolve,
                    reject,
                    onModelDownloadProgress,
                });
                worker.postMessage(loadModelRequestWorkerMessage);
            });
            // TODO: Update estimatedHeapGrowth
            this.checkEstimatedHeapGrowth();
            return loadModelResults;
        });
    }
    translate(translateRequestWorkerMessage) {
        return __awaiter(this, void 0, void 0, function* () {
            const worker = yield this.workerReady;
            const translationResults = yield new Promise((resolve, reject) => {
                const { requestId } = translateRequestWorkerMessage;
                this.pendingRequests.set(requestId, { resolve, reject });
                worker.postMessage(translateRequestWorkerMessage);
            });
            // TODO: Update estimatedHeapGrowth
            this.checkEstimatedHeapGrowth();
            return translationResults;
        });
    }
    /**
     * Triggers after we have our asynchronous result from the worker.
     */
    checkEstimatedHeapGrowth() {
        /*
        // Determine if our input was large enough to trigger heap growth,
        // or if we're already waiting to destroy the worker when it's
        // idle. If so, schedule termination after the idle timeout.
        if (this.estimatedHeapGrowth >= FOO_LIMIT || this._idleTimeout !== null) {
          this.flushWorker();
        }
        */
    }
    onLoadModelResults(loadModelResultsWorkerMessage) {
        const { requestId, loadModelResults } = loadModelResultsWorkerMessage;
        this.pendingRequests.get(requestId).resolve(loadModelResults);
    }
    onTranslateWorkerResult(translationResultsWorkerMessage) {
        const { requestId, translationResults } = translationResultsWorkerMessage;
        this.pendingRequests.get(requestId).resolve(translationResults);
    }
    onModelDownloadProgress(modelDownloadProgressWorkerMessage) {
        const { requestId, modelDownloadProgress, } = modelDownloadProgressWorkerMessage;
        this.pendingRequests
            .get(requestId)
            .onModelDownloadProgress(modelDownloadProgress);
    }
    onError(errorWorkerMessage) {
        const { requestId, message, errorSource } = errorWorkerMessage;
        const apiErrorMessage = `Error event occurred during ${errorSource} in worker (requestId=${requestId}): ${message}`;
        let error;
        if (errorSource === "loadModel") {
            error = new BergamotTranslatorAPIModelLoadError(apiErrorMessage);
        }
        else if (errorSource === "downloadModel") {
            error = new BergamotTranslatorAPIModelDownloadError(apiErrorMessage);
        }
        else if (errorSource === "translate") {
            error = new BergamotTranslatorAPITranslationError(apiErrorMessage);
        }
        else {
            error = new Error(apiErrorMessage);
        }
        this.pendingRequests.get(requestId).reject(error);
    }
    get workerReady() {
        if (!this._workerReadyPromise) {
            this._workerReadyPromise = new Promise((resolve, reject) => {
                const worker = new Worker(WORKER_URL);
                worker.onerror = err => {
                    console.warn("Worker onerror callback fired", err);
                    reject(err);
                };
                worker.onmessage = (msg) => {
                    // console.debug("Incoming message from worker", { msg });
                    if (msg.data === "ready") {
                        resolve(worker);
                    }
                    else if (msg.data.type === "loadModelResults") {
                        this.onLoadModelResults(msg.data);
                    }
                    else if (msg.data.type === "translationResults") {
                        this.onTranslateWorkerResult(msg.data);
                    }
                    else if (msg.data.type === "modelDownloadProgress") {
                        this.onModelDownloadProgress(msg.data);
                    }
                    else if (msg.data.type === "log") {
                        console.log(`Relayed log message from worker: ${msg.data.message}`);
                    }
                    else if (msg.data.type === "error") {
                        this.onError(msg.data);
                    }
                    else {
                        throw new Error("Unknown worker message payload");
                    }
                };
                this._worker = worker;
            });
        }
        return this._workerReadyPromise;
    }
    // Schedule the current worker to be terminated after the idle timeout.
    flushWorker() {
        if (this._idleTimeout !== null) {
            clearTimeout(this._idleTimeout);
        }
        this._idleTimeout = setTimeout(this._flushWorker.bind(this), IDLE_TIMEOUT);
    }
    // Immediately terminate the worker, as long as there no pending
    // results. Otherwise, reschedule termination until after the next
    // idle timeout.
    _flushWorker() {
        if (this.pendingRequests.size) {
            this.flushWorker();
        }
        else {
            if (this._worker) {
                this._worker.terminate();
            }
            this._worker = null;
            this._workerReadyPromise = null;
            this._idleTimeout = null;
        }
    }
}
const workerManager = new WorkerManager();
const translationPerformanceStats = (texts, translationWallTimeMs) => {
    const seconds = translationWallTimeMs / 1000;
    const textCount = texts.length;
    const wordCount = texts
        .map(text => text
        .trim()
        .split(" ")
        .filter(word => word.trim() !== "").length)
        .reduce((a, b) => a + b, 0);
    const characterCount = texts
        .map(text => text.trim().length)
        .reduce((a, b) => a + b, 0);
    const wordsPerSecond = Math.round(wordCount / seconds);
    const charactersPerSecond = Math.round(characterCount / seconds);
    return {
        seconds,
        textCount,
        wordCount,
        characterCount,
        wordsPerSecond,
        charactersPerSecond,
    };
};
/**
 * Class responsible for sending translations requests to the translation worker process
 * in a compatible and somewhat efficient order.
 * Emits events that can be used to track translation progress at a low level.
 */
class TranslationRequestDispatcher extends EventTarget {
    constructor() {
        super(...arguments);
        this.queuedRequests = [];
        this.queuedRequestsByRequestId = new Map();
    }
    processQueue() {
        return __awaiter(this, void 0, void 0, function* () {
            if (this.processing) {
                return;
            }
            this.processing = true;
            while (this.queuedRequests.length) {
                console.info(`Processing translation request queue of ${this.queuedRequests.length} requests`);
                yield this.processNextItemInQueue();
            }
            this.processing = false;
        });
    }
    processNextItemInQueue() {
        return __awaiter(this, void 0, void 0, function* () {
            // Shift the next request off the queue
            const translateRequestWorkerMessage = this.queuedRequests.shift();
            const { translateParams, requestId } = translateRequestWorkerMessage;
            try {
                const { loadModelParams } = translateParams;
                const { from, to } = loadModelParams;
                const languagePair = `${from}${to}`;
                // First check if we need to load a model
                if (!this.loadedLanguagePair ||
                    this.loadedLanguagePair !== languagePair) {
                    const modelWillLoadEventData = {
                        requestId,
                        loadModelParams,
                    };
                    this.dispatchEvent(new CustomEvent("modelWillLoad", {
                        detail: modelWillLoadEventData,
                    }));
                    const loadModelRequestWorkerMessage = {
                        type: "loadModel",
                        requestId,
                        loadModelParams,
                    };
                    const loadModelResults = yield workerManager.loadModel(loadModelRequestWorkerMessage, (modelDownloadProgress) => {
                        const modelDownloadProgressEventData = {
                            requestId,
                            modelDownloadProgress,
                        };
                        this.dispatchEvent(new CustomEvent("modelDownloadProgress", {
                            detail: modelDownloadProgressEventData,
                        }));
                    });
                    this.loadedLanguagePair = languagePair;
                    const modelLoadedEventData = {
                        requestId,
                        loadModelParams,
                        loadModelResults,
                    };
                    this.dispatchEvent(new CustomEvent("modelLoaded", {
                        detail: modelLoadedEventData,
                    }));
                }
                // Send the translation request
                const start = performance.now();
                const translationResults = yield workerManager.translate(translateRequestWorkerMessage);
                // Summarize performance stats
                const end = performance.now();
                const translationWallTimeMs = end - start;
                const originalTextsTranslationPerformanceStats = translationPerformanceStats(translationResults.originalTexts, translationWallTimeMs);
                const translationFinishedEventData = {
                    requestId,
                    translationWallTimeMs,
                    originalTextsTranslationPerformanceStats,
                };
                this.dispatchEvent(new CustomEvent("translationFinished", {
                    detail: translationFinishedEventData,
                }));
                // Resolve the translation request
                this.queuedRequestsByRequestId.get(requestId).resolve(translationResults);
            }
            catch (error) {
                // Reject the translation request
                this.queuedRequestsByRequestId.get(requestId).reject(error);
            }
        });
    }
    translate(requestId, texts, from, to) {
        const loadModelParams = {
            from,
            to,
            bergamotModelsBaseUrl: config_1.config.bergamotModelsBaseUrl,
        };
        const translateParams = {
            texts,
            loadModelParams,
        };
        const translateRequestWorkerMessage = {
            type: "translate",
            requestId,
            translateParams,
        };
        this.queuedRequests.push(translateRequestWorkerMessage);
        const queueLength = this.queuedRequests.length + (this.processing ? 1 : 0);
        const translationRequestQueuedEventData = {
            requestId,
            queueLength,
        };
        this.dispatchEvent(new CustomEvent("translationRequestQueued", {
            detail: translationRequestQueuedEventData,
        }));
        const requestPromise = new Promise((resolve, reject) => {
            this.queuedRequestsByRequestId.set(requestId, { resolve, reject });
        });
        // Kick off queue processing async
        /* eslint-disable no-unused-vars */
        this.processQueue().then(_r => void 0);
        /* eslint-enable no-unused-vars */
        // Return the promise that resolves when the in-scope translation request resolves
        return requestPromise;
    }
}
const translationRequestDispatcher = new TranslationRequestDispatcher();
/**
 * Provide a simpler public interface (compared to above)
 */
exports.BergamotTranslatorAPI = {
    translate(texts, from, to, onTranslationRequestQueued, onModelWillLoad, onModelDownloadProgress, onModelLoaded, onTranslationFinished) {
        return __awaiter(this, void 0, void 0, function* () {
            const requestId = nanoid_1.nanoid();
            const translationRequestQueuedListener = (e) => {
                // console.debug('Listener received "translationRequestQueued".', e.detail);
                if (e.detail.requestId !== requestId) {
                    return;
                }
                translationRequestDispatcher.removeEventListener("translationRequestQueued", translationRequestQueuedListener);
                if (e.detail.queueLength > 1) {
                    console.info(`BergamotTranslatorAPI[${requestId}]: Queued translation request to be processed after ${e
                        .detail.queueLength - 1} already queued requests`);
                }
                else {
                    console.info(`BergamotTranslatorAPI[${requestId}]: Queued translation request for immediate execution`);
                }
                onTranslationRequestQueued(e.detail);
            };
            const modelWillLoadListener = (e) => {
                // console.debug('Listener received "modelWillLoad".', e.detail);
                if (e.detail.requestId !== requestId) {
                    return;
                }
                translationRequestDispatcher.removeEventListener("modelWillLoad", modelWillLoadListener);
                const languagePair = `${from}${to}`;
                console.info(`BergamotTranslatorAPI[${requestId}]: Model ${languagePair} will load`);
                onModelWillLoad(e.detail);
            };
            const modelDownloadProgressListener = (e) => {
                // console.debug('Listener received "modelDownloadProgress".', e.detail);
                if (e.detail.requestId !== requestId) {
                    return;
                }
                const languagePair = `${from}${to}`;
                const { modelDownloadProgress } = e.detail;
                console.info(`BergamotTranslatorAPI[${requestId}]: Model ${languagePair} download progress: `, `${languagePair}: onDownloadProgressUpdate - ${Math.round((modelDownloadProgress.bytesDownloaded /
                    modelDownloadProgress.bytesToDownload) *
                    100)}% out of ${Math.round((modelDownloadProgress.bytesToDownload / 1024 / 1024) * 10) / 10} mb downloaded`, { modelDownloadProgress });
                onModelDownloadProgress(e.detail);
            };
            const modelLoadedListener = (e) => {
                // console.debug('Listener received "modelLoaded".', e.detail);
                if (e.detail.requestId !== requestId) {
                    return;
                }
                translationRequestDispatcher.removeEventListener("modelLoaded", modelLoadedListener);
                const { loadModelResults } = e.detail;
                const languagePair = `${from}${to}`;
                const { modelLoadWallTimeMs } = loadModelResults;
                console.info(`BergamotTranslatorAPI[${requestId}]: Model ${languagePair} loaded in ${modelLoadWallTimeMs /
                    1000} secs`);
                onModelLoaded(e.detail);
            };
            const translationFinishedListener = (e) => {
                // console.debug('Listener received "translationFinished".', e.detail);
                if (e.detail.requestId !== requestId) {
                    return;
                }
                translationRequestDispatcher.removeEventListener("translationFinished", translationFinishedListener);
                const { originalTextsTranslationPerformanceStats } = e.detail;
                const { wordCount, seconds, wordsPerSecond, } = originalTextsTranslationPerformanceStats;
                console.info(`BergamotTranslatorAPI[${requestId}]: Translation of ${texts.length} texts (wordCount ${wordCount}) took ${seconds} secs (${wordsPerSecond} words per second)`);
                onTranslationFinished(e.detail);
            };
            try {
                // console.debug(`Adding listeners for request id ${requestId}`);
                translationRequestDispatcher.addEventListener("translationRequestQueued", translationRequestQueuedListener);
                translationRequestDispatcher.addEventListener("modelWillLoad", modelWillLoadListener);
                translationRequestDispatcher.addEventListener("modelDownloadProgress", modelDownloadProgressListener);
                translationRequestDispatcher.addEventListener("modelLoaded", modelLoadedListener);
                translationRequestDispatcher.addEventListener("translationFinished", translationFinishedListener);
                const requestPromise = translationRequestDispatcher.translate(requestId, texts, from, to);
                const [translationResults] = yield Promise.all([
                    requestPromise,
                ]);
                return translationResults;
            }
            finally {
                translationRequestDispatcher.removeEventListener("translationRequestQueued", translationRequestQueuedListener);
                translationRequestDispatcher.removeEventListener("modelWillLoad", modelWillLoadListener);
                translationRequestDispatcher.removeEventListener("modelDownloadProgress", modelDownloadProgressListener);
                translationRequestDispatcher.removeEventListener("modelLoaded", modelLoadedListener);
                translationRequestDispatcher.removeEventListener("translationFinished", translationFinishedListener);
            }
        });
    },
};


/***/ }),

/***/ 7991:
/*!******************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/lib/LanguageDetector.ts ***!
  \******************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.LanguageDetector = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
// Since Emscripten can handle heap growth, but not heap shrinkage, we
// need to refresh the worker after we've processed a particularly large
// string in order to prevent unnecessary resident memory growth.
//
// These values define the cut-off string length and the idle timeout
// (in milliseconds) before destroying a worker. Once a string of the
// maximum size has been processed, the worker is marked for
// destruction, and is terminated as soon as it has been idle for the
// given timeout.
//
// 1.5MB. This is the approximate string length that forces heap growth
// for a 2MB heap.
const LARGE_STRING = 1.5 * 1024 * 1024;
const IDLE_TIMEOUT = 10 * 1000;
const WORKER_URL = webextension_polyfill_ts_1.browser.runtime.getURL(`wasm/cld-worker.js`);
const workerManager = {
    // TODO: Make into a map instead to avoid the implicit assumption that the order of requests and results are the same
    detectionQueue: [],
    detectLanguage(params) {
        return __awaiter(this, void 0, void 0, function* () {
            const worker = yield this.workerReady;
            const result = yield new Promise(resolve => {
                this.detectionQueue.push({ resolve });
                worker.postMessage(params);
            });
            // We have our asynchronous result from the worker.
            //
            // Determine if our input was large enough to trigger heap growth,
            // or if we're already waiting to destroy the worker when it's
            // idle. If so, schedule termination after the idle timeout.
            if (params.text.length >= LARGE_STRING || this._idleTimeout !== null) {
                this.flushWorker();
            }
            return result;
        });
    },
    onDetectLanguageWorkerResult(detectedLanguageResults) {
        this.detectionQueue.shift().resolve(detectedLanguageResults);
    },
    _worker: null,
    _workerReadyPromise: null,
    get workerReady() {
        if (!this._workerReadyPromise) {
            this._workerReadyPromise = new Promise(resolve => {
                const worker = new Worker(WORKER_URL);
                worker.onmessage = msg => {
                    if (msg.data === "ready") {
                        resolve(worker);
                    }
                    else {
                        this.onDetectLanguageWorkerResult(msg.data);
                    }
                };
                this._worker = worker;
            });
        }
        return this._workerReadyPromise;
    },
    // Holds the ID of the current pending idle cleanup setTimeout.
    _idleTimeout: null,
    // Schedule the current worker to be terminated after the idle timeout.
    flushWorker() {
        if (this._idleTimeout !== null) {
            clearTimeout(this._idleTimeout);
        }
        this._idleTimeout = setTimeout(this._flushWorker.bind(this), IDLE_TIMEOUT);
    },
    // Immediately terminate the worker, as long as there no pending
    // results. Otherwise, reschedule termination until after the next
    // idle timeout.
    _flushWorker() {
        if (this.detectionQueue.length) {
            this.flushWorker();
        }
        else {
            if (this._worker) {
                this._worker.terminate();
            }
            this._worker = null;
            this._workerReadyPromise = null;
            this._idleTimeout = null;
        }
    },
};
exports.LanguageDetector = {
    /**
     * Detect the language of a given string.
     *
     * The argument may be either a string containing the text to analyze,
     * or an object with the following properties:
     *
     *  - 'text' The text to analyze.
     *
     *  - 'isHTML' (optional) A boolean, indicating whether the text
     *      should be analyzed as HTML rather than plain text.
     *
     *  - 'language' (optional) A string indicating the expected language.
     *      For text extracted from HTTP documents, this is expected to
     *      come from the Content-Language header.
     *
     *  - 'tld' (optional) A string indicating the top-level domain of the
     *      document the text was extracted from.
     *
     *  - 'encoding' (optional) A string describing the encoding of the
     *      document the string was extracted from. Note that, regardless
     *      of the value of this property, the 'text' property must be a
     *      UTF-16 JavaScript string.
     *
     * @returns {Promise<DetectedLanguageResults>}
     * @resolves When detection is finished, with a object containing
     * these fields:
     *  - 'language' (string with a language code)
     *  - 'confident' (boolean) Whether the detector is confident of the
     *      result.
     *  - 'languages' (array) An array of up to three elements, containing
     *      the most prevalent languages detected. It contains a
     *      'languageCode' property, containing the ISO language code of
     *      the language, and a 'percent' property, describing the
     *      approximate percentage of the input which is in that language.
     *      For text of an unknown language, the result may contain an
     *      entry with the language code 'un', indicating the percent of
     *      the text which is unknown.
     */
    detectLanguage(params) {
        return __awaiter(this, void 0, void 0, function* () {
            if (typeof params === "string") {
                params = { text: params };
            }
            return workerManager.detectLanguage(params);
        });
    },
};


/***/ }),

/***/ 5692:
/*!*************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/lib/translateAllFramesInTab.ts ***!
  \*************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.translateAllFramesInTab = void 0;
const mobx_1 = __webpack_require__(/*! mobx */ 9637);
const BaseTranslationState_1 = __webpack_require__(/*! ../../../shared-resources/models/BaseTranslationState */ 4779);
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const Telemetry_1 = __webpack_require__(/*! ../telemetry/Telemetry */ 647);
const translateAllFramesInTab = (tabId, from, to, extensionState) => __awaiter(void 0, void 0, void 0, function* () {
    // Start timing
    const start = performance.now();
    // Request translation of all frames in a specific tab
    extensionState.requestTranslationOfAllFramesInTab(tabId, from, to);
    // Wait for translation in all frames in tab to complete
    yield mobx_1.when(() => {
        const { tabTranslationStates } = extensionState;
        const currentTabTranslationState = tabTranslationStates.get(tabId);
        return (currentTabTranslationState &&
            [BaseTranslationState_1.TranslationStatus.TRANSLATED, BaseTranslationState_1.TranslationStatus.ERROR].includes(currentTabTranslationState.translationStatus));
    });
    // End timing
    const end = performance.now();
    const timeToFullPageTranslatedMs = end - start;
    const { tabTranslationStates } = extensionState;
    const currentTabTranslationState = mobx_keystone_1.getSnapshot(tabTranslationStates.get(tabId));
    const { totalModelLoadWallTimeMs, totalTranslationEngineRequestCount, totalTranslationWallTimeMs, wordCount, translationStatus, modelDownloadProgress, } = currentTabTranslationState;
    if (translationStatus === BaseTranslationState_1.TranslationStatus.TRANSLATED) {
        // Record "translation attempt concluded" telemetry
        const timeToFullPageTranslatedSeconds = timeToFullPageTranslatedMs / 1000;
        const timeToFullPageTranslatedWordsPerSecond = Math.round(wordCount / timeToFullPageTranslatedSeconds);
        const translationEngineTimeMs = totalTranslationWallTimeMs;
        const translationEngineWordsPerSecond = Math.round(wordCount / (translationEngineTimeMs / 1000));
        const modelDownloadTimeMs = modelDownloadProgress.durationMs || 0;
        const modelLoadTimeMs = totalModelLoadWallTimeMs;
        const unaccountedTranslationTimeMs = timeToFullPageTranslatedMs -
            modelDownloadTimeMs -
            modelLoadTimeMs -
            translationEngineTimeMs;
        console.info(`Translation of the full page in tab with id ${tabId} (${wordCount} words) took ${timeToFullPageTranslatedSeconds} secs (perceived as ${timeToFullPageTranslatedWordsPerSecond} words per second) across ${totalTranslationEngineRequestCount} translation engine requests (which took ${totalTranslationWallTimeMs /
            1000} seconds, operating at ${translationEngineWordsPerSecond} words per second). Model loading took ${modelLoadTimeMs /
            1000} seconds, after spending ${modelDownloadTimeMs / 1000} seconds ${modelDownloadProgress.bytesToDownload === 0
            ? "hydrating"
            : "downloading and persisting"} model files. The remaining ${unaccountedTranslationTimeMs /
            1000} seconds where spent elsewhere.`);
        Telemetry_1.telemetry.onTranslationFinished(tabId, from, to, timeToFullPageTranslatedMs, timeToFullPageTranslatedWordsPerSecond, modelDownloadTimeMs, modelLoadTimeMs, translationEngineTimeMs, translationEngineWordsPerSecond);
    }
    else {
        // TODO: Record error telemetry
    }
});
exports.translateAllFramesInTab = translateAllFramesInTab;


/***/ }),

/***/ 805:
/*!************************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/state-management/MobxKeystoneBackgroundContextHost.ts ***!
  \************************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.MobxKeystoneBackgroundContextHost = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const ErrorReporting_1 = __webpack_require__(/*! ../../../shared-resources/ErrorReporting */ 3345);
const DocumentTranslationState_1 = __webpack_require__(/*! ../../../shared-resources/models/DocumentTranslationState */ 5482);
const TranslateOwnTextTranslationState_1 = __webpack_require__(/*! ../../../shared-resources/models/TranslateOwnTextTranslationState */ 8238);
// If we don't import and use all relevant models here, we can't reference models in this build
// Ref: https://github.com/xaviergonz/mobx-keystone/issues/183
DocumentTranslationState_1.DocumentTranslationState;
TranslateOwnTextTranslationState_1.TranslateOwnTextTranslationState;
// disable runtime data checking (we rely on TypeScript at compile time so that our model definitions can be cleaner)
mobx_keystone_1.setGlobalConfig({
    modelAutoTypeChecking: mobx_keystone_1.ModelAutoTypeCheckingMode.AlwaysOff,
});
class MobxKeystoneBackgroundContextHost {
    constructor() {
        this.connectedPorts = [];
    }
    init(backgroundContextRootStore) {
        let actionIsBeingAppliedForPropagationToContentScripts = false;
        // Set up a connection / listener for the mobx-keystone-proxy
        // allowing state changes to be sent from content scripts
        this.mobxKeystoneProxyPortListener = port => {
            if (port.name !== "port-from-mobx-keystone-proxy") {
                return;
            }
            this.connectedPorts.push(port);
            port.onMessage.addListener((m) => __awaiter(this, void 0, void 0, function* () {
                // console.debug("Message from mobx-keystone-proxy:", { m });
                const { requestInitialState, actionCall, requestId } = m;
                if (requestInitialState) {
                    const initialState = mobx_keystone_1.getSnapshot(backgroundContextRootStore);
                    // console.debug({ initialState });
                    port.postMessage({
                        initialState,
                        requestId,
                    });
                    return;
                }
                if (actionCall) {
                    const _ = actionIsBeingAppliedForPropagationToContentScripts;
                    actionIsBeingAppliedForPropagationToContentScripts = true;
                    this.handleLocallyCancelledActionCall(actionCall, backgroundContextRootStore);
                    actionIsBeingAppliedForPropagationToContentScripts = _;
                    return;
                }
                ErrorReporting_1.captureExceptionWithExtras(new Error("Unexpected message"), { m });
                console.error("Unexpected message", { m });
            }));
            port.onDisconnect.addListener(($port) => {
                const existingPortIndex = this.connectedPorts.findIndex(p => p === $port);
                this.connectedPorts.splice(existingPortIndex, 1);
            });
        };
        webextension_polyfill_ts_1.browser.runtime.onConnect.addListener(this.mobxKeystoneProxyPortListener);
        // Set up a listener for local (background context) actions to be replicated to content scripts
        // in the same way that actions stemming from content scripts do
        mobx_keystone_1.onActionMiddleware(backgroundContextRootStore, {
            onStart: (actionCall, ctx) => {
                if (!actionIsBeingAppliedForPropagationToContentScripts) {
                    // if the action comes from the background context cancel it silently
                    // and send resubmit it in the same way that content script actions are treated
                    // it will then be replicated by the server (background context) and properly executed everywhere
                    const serializedActionCall = mobx_keystone_1.serializeActionCall(actionCall, backgroundContextRootStore);
                    this.handleLocallyCancelledActionCall(serializedActionCall, backgroundContextRootStore);
                    ctx.data.cancelled = true; // just for logging purposes
                    // "cancel" the action by returning undefined
                    return {
                        result: mobx_keystone_1.ActionTrackingResult.Return,
                        value: undefined,
                    };
                }
                // run actions that are being applied for propagation to content scripts unmodified
                /* eslint-disable consistent-return */
                return undefined;
                /* eslint-enable consistent-return */
            },
        });
    }
    handleLocallyCancelledActionCall(serializedActionCall, backgroundContextRootStore) {
        // apply the action over the server root store
        // sometimes applying actions might fail (for example on invalid operations
        // such as when one client asks to delete a model from an array and other asks to mutate it)
        // so we try / catch it
        let serializedActionCallToReplicate;
        try {
            // apply the action on the background context side and keep track of new model IDs being
            // generated, so the clients will have the chance to keep those in sync
            const applyActionResult = mobx_keystone_1.applySerializedActionAndTrackNewModelIds(backgroundContextRootStore, serializedActionCall);
            serializedActionCallToReplicate = applyActionResult.serializedActionCall;
        }
        catch (err) {
            console.error("Error applying action to server:", err);
        }
        if (serializedActionCallToReplicate) {
            this.propagateActionToContentScripts(serializedActionCallToReplicate);
        }
    }
    propagateActionToContentScripts(serializedActionCallToReplicate) {
        // and distribute message, which includes new model IDs to keep them in sync
        this.connectedPorts.forEach($port => {
            try {
                $port.postMessage({
                    serializedActionCallToReplicate,
                });
            }
            catch (err) {
                if (err.message === "Attempt to postMessage on disconnected port") {
                    console.warn("Attempt to postMessage on disconnected port, but it is ok", err);
                }
                else {
                    throw err;
                }
            }
        });
    }
}
exports.MobxKeystoneBackgroundContextHost = MobxKeystoneBackgroundContextHost;


/***/ }),

/***/ 4517:
/*!********************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/state-management/Store.ts ***!
  \********************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.Store = void 0;
const nanoid_1 = __webpack_require__(/*! nanoid */ 350);
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
class Store {
    constructor(localStorageWrapper) {
        this.get = (_keys) => __awaiter(this, void 0, void 0, function* () { return ({}); });
        this.set = (_items) => __awaiter(this, void 0, void 0, function* () { });
        this.initialExtensionPreferences = () => __awaiter(this, void 0, void 0, function* () {
            return {
                enableErrorReporting: false,
                hidePrivacySummaryBanner: false,
                extensionInstallationErrorReportingId: "",
                extensionInstallationId: "",
                extensionVersion: "",
            };
        });
        /**
         * Returns a persistent unique identifier of the extension installation
         * sent with each report. Not related to the Firefox client id
         */
        this.extensionInstallationId = () => __awaiter(this, void 0, void 0, function* () {
            const { extensionInstallationId } = yield this.get("extensionInstallationId");
            if (extensionInstallationId) {
                return extensionInstallationId;
            }
            const generatedId = nanoid_1.nanoid();
            yield this.set({ extensionInstallationId: generatedId });
            return generatedId;
        });
        /**
         * Returns a persistent unique identifier of the extension installation
         * sent with each error report. Not related to the Firefox client id
         * nor the extension installation id that identifies shared data.
         */
        this.extensionInstallationErrorReportingId = () => __awaiter(this, void 0, void 0, function* () {
            const { extensionInstallationErrorReportingId } = yield this.get("extensionInstallationErrorReportingId");
            if (extensionInstallationErrorReportingId) {
                return extensionInstallationErrorReportingId;
            }
            const generatedId = nanoid_1.nanoid();
            yield this.set({ extensionInstallationErrorReportingId: generatedId });
            return generatedId;
        });
        this.getExtensionPreferences = () => __awaiter(this, void 0, void 0, function* () {
            const { extensionPreferences } = yield this.get("extensionPreferences");
            return Object.assign(Object.assign(Object.assign({}, (yield this.initialExtensionPreferences())), extensionPreferences), {
                // The following are not editable extension preferences, but attributes
                // that we want to display on the extension preferences dialog and/or
                // add as context in error reports
                extensionInstallationErrorReportingId: yield this.extensionInstallationErrorReportingId(),
                extensionInstallationId: yield this.extensionInstallationId(),
                extensionVersion: webextension_polyfill_ts_1.browser.runtime.getManifest().version,
            });
        });
        this.setExtensionPreferences = (extensionPreferences) => __awaiter(this, void 0, void 0, function* () {
            yield this.set({ extensionPreferences });
        });
        this.get = localStorageWrapper.get;
        this.set = localStorageWrapper.set;
    }
}
exports.Store = Store;


/***/ }),

/***/ 6432:
/*!*****************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/state-management/connectRootStoreToDevTools.ts ***!
  \*****************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __createBinding = (this && this.__createBinding) || (Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    Object.defineProperty(o, k2, { enumerable: true, get: function() { return m[k]; } });
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}));
var __setModuleDefault = (this && this.__setModuleDefault) || (Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
});
var __importStar = (this && this.__importStar) || function (mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
};
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.connectRootStoreToDevTools = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
function connectRootStoreToDevTools(rootStore) {
    return __awaiter(this, void 0, void 0, function* () {
        // connect the store to the redux dev tools
        // (different ports for different build variants developed simultaneously)
        const { default: remotedev } = yield Promise.resolve().then(() => __importStar(__webpack_require__(/*! remotedev */ 9607)));
        const port = process.env.REMOTE_DEV_SERVER_PORT;
        console.info(`Connecting the background store to the Redux dev tools on port ${port}`);
        const connection = remotedev.connectViaExtension({
            name: `Background Context (Port ${port})`,
            realtime: true,
            port,
        });
        mobx_keystone_1.connectReduxDevTools(remotedev, connection, rootStore);
    });
}
exports.connectRootStoreToDevTools = connectRootStoreToDevTools;


/***/ }),

/***/ 8911:
/*!***********************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/state-management/createBackgroundContextRootStore.ts ***!
  \***********************************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.createBackgroundContextRootStore = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const ExtensionState_1 = __webpack_require__(/*! ../../../shared-resources/models/ExtensionState */ 65);
// enable runtime data checking even in production mode
mobx_keystone_1.setGlobalConfig({
    modelAutoTypeChecking: mobx_keystone_1.ModelAutoTypeCheckingMode.AlwaysOn,
});
function createBackgroundContextRootStore() {
    // the parameter is the initial data for the model
    const rootStore = new ExtensionState_1.ExtensionState({});
    // recommended by mobx-keystone (allows the model hook `onAttachedToRootStore` to work and other goodies)
    mobx_keystone_1.registerRootStore(rootStore);
    return rootStore;
}
exports.createBackgroundContextRootStore = createBackgroundContextRootStore;


/***/ }),

/***/ 5813:
/*!**********************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/state-management/localStorageWrapper.ts ***!
  \**********************************************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.localStorageWrapper = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
exports.localStorageWrapper = {
    get: webextension_polyfill_ts_1.browser.storage.local.get,
    set: webextension_polyfill_ts_1.browser.storage.local.set,
};


/***/ }),

/***/ 647:
/*!*****************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/Telemetry.ts ***!
  \*****************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.telemetry = exports.Telemetry = void 0;
const webext_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext */ 6902));
const pings_1 = __webpack_require__(/*! ./generated/pings */ 3177);
const config_1 = __webpack_require__(/*! ../../../config */ 964);
const performance_1 = __webpack_require__(/*! ./generated/performance */ 1714);
const metadata_1 = __webpack_require__(/*! ./generated/metadata */ 9085);
const infobar_1 = __webpack_require__(/*! ./generated/infobar */ 2457);
const service_1 = __webpack_require__(/*! ./generated/service */ 7659);
const errors_1 = __webpack_require__(/*! ./generated/errors */ 5352);
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const bergamot_translator_version_1 = __webpack_require__(/*! ../../../web-worker-scripts/translation-worker.js/bergamot-translator-version */ 8471);
/**
 * This class contains general telemetry initialization and helper code and synchronous telemetry-recording functions.
 *
 * Synchronous methods here is important, since it is the only way to guarantee that multiple Glean API calls are
 * executed sequentially and not interleaved with other asynchronous Telemetry recording.
 * For more information, see: https://github.com/mozilla-extensions/bergamot-browser-extension/pull/76#discussion_r602128568
 *
 * Glean.js guarantees zero exceptions, but our glue code or specific way of invoking Glean.js may result in exceptions.
 * For this reason we surround all code invoking Glean.js in try/catch blocks.
 *
 * Pings are grouped by tab id and submitted on specific triggers (see below), or after 1 minute of inactivity.
 *
 * The "1 minute" period can be overridden to facilitate testing by setting
 * the telemetryInactivityThresholdInSecondsOverride string argument at initialization.
 *
 * Submit triggers:
 * 1. Regular translation: infobar displayed -> translated pressed -> translation finished or error -> submit
 * 2. Switch language: infobar displayed -> switch source or target lang -> submit (further actions will be submitted in translation scenario)
 * 3. Reject: infobar displayed -> press never, not now or close -> submit
 * 4. No action: infobar displayed -> no action -> submit on timer after a period of inactivity.
 * 5. Language pair unsupported -> submit
 */
class Telemetry {
    constructor() {
        this.queuedRecordingsByTabId = {};
        this.inactivityDispatchTimersByTabId = {};
        /**
         * Submits all collected metrics in a custom ping.
         */
        this.queueRecording = (telemetryRecordingFunction, tabId) => {
            if (!this.initialized) {
                console.warn("Telemetry: ignoring ping that was submitted before Telemetry was initialized");
                return;
            }
            const tabIdString = String(tabId);
            if (!this.queuedRecordingsByTabId[tabIdString]) {
                this.queuedRecordingsByTabId[tabIdString] = [];
            }
            this.queuedRecordingsByTabId[tabIdString].push(telemetryRecordingFunction);
            console.info(`Telemetry: Queued a recording in tab ${tabId}`);
        };
        this.updateInactivityTimerForTab = (tabId) => {
            const tabIdString = String(tabId);
            this.clearInactivityTimerForTab(tabId);
            // Submit queued recordings after a period of inactivity
            this.inactivityDispatchTimersByTabId[tabIdString] = setTimeout(() => {
                this.submitQueuedRecordings(tabIdString);
            }, this.telemetryInactivityThresholdInSeconds * 1000);
            // console.debug(`Telemetry: Inactivity timer ${this.inactivityDispatchTimersByTabId[tabIdString]} for tab ${tabId} set to fire in ${this.telemetryInactivityThresholdInSeconds} seconds.`, new Error())
        };
        this.updateInactivityTimerForAllTabs = () => {
            Object.keys(this.queuedRecordingsByTabId).forEach((tabId) => {
                this.updateInactivityTimerForTab(tabId);
            });
        };
        this.clearInactivityTimerForTab = (tabId) => {
            const tabIdString = String(tabId);
            if (this.inactivityDispatchTimersByTabId[tabIdString]) {
                // console.debug(`Telemetry: Inactivity timer ${this.inactivityDispatchTimersByTabId[tabIdString]} for tab ${tabId} cleared.`)
                clearTimeout(this.inactivityDispatchTimersByTabId[tabIdString]);
                delete this.inactivityDispatchTimersByTabId[tabIdString];
            }
        };
    }
    initialize(uploadEnabled, $firefoxClientId, telemetryInactivityThresholdInSecondsOverride) {
        const appId = config_1.config.telemetryAppId;
        this.setFirefoxClientId($firefoxClientId);
        const manifest = webextension_polyfill_ts_1.browser.runtime.getManifest();
        this.extensionVersion = manifest.version;
        try {
            webext_1.default.initialize(appId, uploadEnabled, {
                debug: { logPings: config_1.config.telemetryDebugMode },
            });
            this.telemetryInactivityThresholdInSeconds = telemetryInactivityThresholdInSecondsOverride
                ? telemetryInactivityThresholdInSecondsOverride
                : 60;
            console.info(`Telemetry: initialization completed with application ID ${appId}. Inactivity threshold is set to ${this.telemetryInactivityThresholdInSeconds} seconds.`);
            this.initialized = true;
        }
        catch (err) {
            console.error(`Telemetry initialization error`, err);
        }
    }
    uploadEnabledPreferenceUpdated(uploadEnabled) {
        console.log("Telemetry: communicating updated uploadEnabled preference to Glean.js", { uploadEnabled });
        webext_1.default.setUploadEnabled(uploadEnabled);
    }
    setFirefoxClientId($firefoxClientId) {
        this.firefoxClientId = $firefoxClientId;
    }
    setTranslationRelevantFxTelemetryMetrics(translationRelevantFxTelemetryMetrics) {
        this.translationRelevantFxTelemetryMetrics = translationRelevantFxTelemetryMetrics;
    }
    recordCommonMetadata(from, to) {
        metadata_1.fromLang.set(from);
        metadata_1.toLang.set(to);
        metadata_1.firefoxClientId.set(this.firefoxClientId);
        metadata_1.extensionVersion.set(this.extensionVersion);
        metadata_1.extensionBuildId.set(config_1.config.extensionBuildId.substring(0, 100));
        metadata_1.bergamotTranslatorVersion.set(bergamot_translator_version_1.BERGAMOT_VERSION_FULL);
        if (this.translationRelevantFxTelemetryMetrics) {
            const { systemMemoryMb, systemCpuCount, systemCpuCores, systemCpuVendor, systemCpuFamily, systemCpuModel, systemCpuStepping, systemCpuL2cacheKB, systemCpuL3cacheKB, systemCpuSpeedMhz, systemCpuExtensions, } = this.translationRelevantFxTelemetryMetrics;
            metadata_1.systemMemory.set(systemMemoryMb);
            metadata_1.cpuCount.set(systemCpuCount);
            metadata_1.cpuCoresCount.set(systemCpuCores);
            metadata_1.cpuVendor.set(systemCpuVendor);
            metadata_1.cpuFamily.set(systemCpuFamily);
            metadata_1.cpuModel.set(systemCpuModel);
            metadata_1.cpuStepping.set(systemCpuStepping);
            metadata_1.cpuL2Cache.set(systemCpuL2cacheKB);
            metadata_1.cpuL3Cache.set(systemCpuL3cacheKB);
            metadata_1.cpuSpeed.set(systemCpuSpeedMhz);
            metadata_1.cpuExtensions.set(systemCpuExtensions.join(","));
        }
    }
    onInfoBarDisplayed(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.displayed.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onSelectTranslateFrom(tabId, newFrom, to) {
        this.queueRecording(() => {
            infobar_1.changeLang.record();
            this.recordCommonMetadata(newFrom, to);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onSelectTranslateTo(tabId, from, newTo) {
        this.queueRecording(() => {
            infobar_1.changeLang.record();
            this.recordCommonMetadata(from, newTo);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onInfoBarClosed(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.closed.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onNeverTranslateSelectedLanguage(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.neverTranslateLang.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onNeverTranslateThisSite(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.neverTranslateSite.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onShowOriginalButtonPressed(tabId, _from, _to) {
        this.updateInactivityTimerForAllTabs();
        // TODO?
    }
    onShowTranslatedButtonPressed(tabId, _from, _to) {
        this.updateInactivityTimerForAllTabs();
        // TODO?
    }
    onTranslateButtonPressed(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.translate.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onNotNowButtonPressed(tabId, from, to) {
        this.queueRecording(() => {
            infobar_1.notNow.record();
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    /**
     * A translation attempt starts when a translation is requested in a
     * specific tab and ends when all translations in that tab has completed
     */
    onTranslationFinished(tabId, from, to, timeToFullPageTranslatedMs, timeToFullPageTranslatedWordsPerSecond, modelDownloadTimeMs, modelLoadTimeMs, translationEngineTimeMs, translationEngineWordsPerSecond) {
        this.queueRecording(() => {
            performance_1.fullPageTranslatedTime.setRawNanos(timeToFullPageTranslatedMs * 1000000);
            performance_1.fullPageTranslatedWps.set(timeToFullPageTranslatedWordsPerSecond);
            performance_1.modelDownloadTimeNum.setRawNanos(modelDownloadTimeMs * 1000000);
            performance_1.modelLoadTimeNum.setRawNanos(modelLoadTimeMs * 1000000);
            performance_1.translationEngineTime.setRawNanos(translationEngineTimeMs * 1000000);
            performance_1.translationEngineWps.set(translationEngineWordsPerSecond);
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onTranslationStatusOffer(tabId, from, to) {
        this.queueRecording(() => {
            service_1.langMismatch.add(1);
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onTranslationStatusTranslationUnsupported(tabId, from, to) {
        this.queueRecording(() => {
            service_1.notSupported.add(1);
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.updateInactivityTimerForAllTabs();
    }
    onModelLoadErrorOccurred(tabId, from, to) {
        this.submitQueuedRecordings(tabId);
        // TODO?
    }
    onModelDownloadErrorOccurred(tabId, from, to) {
        this.queueRecording(() => {
            errors_1.modelDownload.add(1);
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onTranslationErrorOccurred(tabId, from, to) {
        this.queueRecording(() => {
            errors_1.translation.add(1);
            this.recordCommonMetadata(from, to);
        }, tabId);
        this.submitQueuedRecordings(tabId);
    }
    onOtherErrorOccurred(tabId, from, to) {
        this.submitQueuedRecordings(tabId);
        // TODO?
    }
    submitQueuedRecordings(tabId) {
        const tabIdString = String(tabId);
        const recordings = this
            .queuedRecordingsByTabId[tabIdString];
        if (recordings.length === 0) {
            // console.debug(`Telemetry: Submit of 0 recordings from tab ${tabId} requested. Ignoring.`)
            return;
        }
        delete this.queuedRecordingsByTabId[tabIdString];
        this.queuedRecordingsByTabId[tabIdString] = [];
        this.clearInactivityTimerForTab(tabId);
        try {
            recordings.forEach(telemetryRecordingFunction => {
                telemetryRecordingFunction();
            });
            pings_1.custom.submit();
            console.info(`Telemetry: A ping based on ${recordings.length} recordings for tab ${tabId} have been dispatched to Glean.js`);
        }
        catch (err) {
            console.error(`Telemetry dispatch error`, err);
        }
    }
    cleanup() {
        return __awaiter(this, void 0, void 0, function* () {
            // Cancel ongoing timers
            Object.keys(this.inactivityDispatchTimersByTabId).forEach((tabId) => {
                this.clearInactivityTimerForTab(tabId);
            });
            // Make sure to send buffered telemetry events
            Object.keys(this.queuedRecordingsByTabId).forEach((tabId) => {
                this.submitQueuedRecordings(tabId);
            });
        });
    }
}
exports.Telemetry = Telemetry;
// Expose singleton instances
exports.telemetry = new Telemetry();


/***/ }),

/***/ 5352:
/*!************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/errors.ts ***!
  \************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.modelDownload = exports.memory = exports.marian = exports.translation = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const counter_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/counter */ 4429));
/**
 * The translation procedure has failed.
 *
 * Generated from `errors.translation`.
 */
exports.translation = new counter_1.default({
    category: "errors",
    name: "translation",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Marian code related error.
 *
 * Generated from `errors.marian`.
 */
exports.marian = new counter_1.default({
    category: "errors",
    name: "marian",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Memory allocation error.
 *
 * Generated from `errors.memory`.
 */
exports.memory = new counter_1.default({
    category: "errors",
    name: "memory",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Failed to download a model for a supported language pair.
 *
 * Generated from `errors.model_download`.
 */
exports.modelDownload = new counter_1.default({
    category: "errors",
    name: "model_download",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});


/***/ }),

/***/ 2457:
/*!*************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/infobar.ts ***!
  \*************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.changeLang = exports.notNow = exports.neverTranslateSite = exports.neverTranslateLang = exports.translate = exports.closed = exports.displayed = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const event_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/event */ 4986));
/**
 * The translation infobar was automatically displayed in a browser.
 *
 * Generated from `infobar.displayed`.
 */
exports.displayed = new event_1.default({
    category: "infobar",
    name: "displayed",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * The translation infobar was closed.
 *
 * Generated from `infobar.closed`.
 */
exports.closed = new event_1.default({
    category: "infobar",
    name: "closed",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * The "Translate" button was pressed on translation infobar.
 *
 * Generated from `infobar.translate`.
 */
exports.translate = new event_1.default({
    category: "infobar",
    name: "translate",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * "Never translate language" button in the infobar options was pressed.
 *
 * Generated from `infobar.never_translate_lang`.
 */
exports.neverTranslateLang = new event_1.default({
    category: "infobar",
    name: "never_translate_lang",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * "Never translate site" button in the infobar options was pressed.
 *
 * Generated from `infobar.never_translate_site`.
 */
exports.neverTranslateSite = new event_1.default({
    category: "infobar",
    name: "never_translate_site",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * "Not now" button on the infobar was pressed.
 *
 * Generated from `infobar.not_now`.
 */
exports.notNow = new event_1.default({
    category: "infobar",
    name: "not_now",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);
/**
 * "This page is in" language was changed manually.
 *
 * Generated from `infobar.change_lang`.
 */
exports.changeLang = new event_1.default({
    category: "infobar",
    name: "change_lang",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, []);


/***/ }),

/***/ 9085:
/*!**************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/metadata.ts ***!
  \**************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.cpuExtensions = exports.cpuSpeed = exports.cpuL3Cache = exports.cpuL2Cache = exports.cpuStepping = exports.cpuModel = exports.cpuFamily = exports.cpuVendor = exports.cpuCoresCount = exports.cpuCount = exports.systemMemory = exports.bergamotTranslatorVersion = exports.extensionBuildId = exports.extensionVersion = exports.firefoxClientId = exports.toLang = exports.fromLang = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const quantity_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/quantity */ 6120));
const string_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/string */ 5799));
/**
 * Translation source language.
 *
 * Generated from `metadata.from_lang`.
 */
exports.fromLang = new string_1.default({
    category: "metadata",
    name: "from_lang",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Translation target language.
 *
 * Generated from `metadata.to_lang`.
 */
exports.toLang = new string_1.default({
    category: "metadata",
    name: "to_lang",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Firefox Telemetry client id.
 *
 * Generated from `metadata.firefox_client_id`.
 */
exports.firefoxClientId = new string_1.default({
    category: "metadata",
    name: "firefox_client_id",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Extension version
 *
 * Generated from `metadata.extension_version`.
 */
exports.extensionVersion = new string_1.default({
    category: "metadata",
    name: "extension_version",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Extension build id, indicating which git revision
 * and build config was used to produce this build
 *
 * Generated from `metadata.extension_build_id`.
 */
exports.extensionBuildId = new string_1.default({
    category: "metadata",
    name: "extension_build_id",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Bergamot translator engine version
 *
 * Generated from `metadata.bergamot_translator_version`.
 */
exports.bergamotTranslatorVersion = new string_1.default({
    category: "metadata",
    name: "bergamot_translator_version",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.memoryMB` metric
 *
 * Generated from `metadata.system_memory`.
 */
exports.systemMemory = new quantity_1.default({
    category: "metadata",
    name: "system_memory",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.count` metric
 *
 * Generated from `metadata.cpu_count`.
 */
exports.cpuCount = new quantity_1.default({
    category: "metadata",
    name: "cpu_count",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.cores` metric
 *
 * Generated from `metadata.cpu_cores_count`.
 */
exports.cpuCoresCount = new quantity_1.default({
    category: "metadata",
    name: "cpu_cores_count",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.vendor` metric
 *
 * Generated from `metadata.cpu_vendor`.
 */
exports.cpuVendor = new string_1.default({
    category: "metadata",
    name: "cpu_vendor",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.family` metric
 *
 * Generated from `metadata.cpu_family`.
 */
exports.cpuFamily = new string_1.default({
    category: "metadata",
    name: "cpu_family",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.model` metric
 *
 * Generated from `metadata.cpu_model`.
 */
exports.cpuModel = new string_1.default({
    category: "metadata",
    name: "cpu_model",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.stepping` metric
 *
 * Generated from `metadata.cpu_stepping`.
 */
exports.cpuStepping = new string_1.default({
    category: "metadata",
    name: "cpu_stepping",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.l2cacheKB` metric
 *
 * Generated from `metadata.cpu_l2_cache`.
 */
exports.cpuL2Cache = new quantity_1.default({
    category: "metadata",
    name: "cpu_l2_cache",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.l3cacheKB` metric
 *
 * Generated from `metadata.cpu_l3_cache`.
 */
exports.cpuL3Cache = new quantity_1.default({
    category: "metadata",
    name: "cpu_l3_cache",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.speedMHz` metric
 *
 * Generated from `metadata.cpu_speed`.
 */
exports.cpuSpeed = new quantity_1.default({
    category: "metadata",
    name: "cpu_speed",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Telemetry Environment `system.cpu.extensions` metric
 *
 * Generated from `metadata.cpu_extensions`.
 */
exports.cpuExtensions = new string_1.default({
    category: "metadata",
    name: "cpu_extensions",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});


/***/ }),

/***/ 1714:
/*!*****************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/performance.ts ***!
  \*****************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.translationQuality = exports.translationEngineWps = exports.translationEngineTime = exports.modelLoadTimeNum = exports.modelDownloadTimeNum = exports.fullPageTranslatedWps = exports.fullPageTranslatedTime = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const timespan_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/timespan */ 4498));
const quantity_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/quantity */ 6120));
const string_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/string */ 5799));
/**
 * Timing from "translation button pressed"
 * to "full page is translated".
 *
 * Generated from `performance.full_page_translated_time`.
 */
exports.fullPageTranslatedTime = new timespan_1.default({
    category: "performance",
    name: "full_page_translated_time",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, "millisecond");
/**
 * Speed of the translation from "translation button
 * pressed" to "full page is translated".
 *
 * Generated from `performance.full_page_translated_wps`.
 */
exports.fullPageTranslatedWps = new quantity_1.default({
    category: "performance",
    name: "full_page_translated_wps",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Time spent on downloading a translation model for a language pair.
 * (Renamed from model_download_time to model_download_time_num as part of
 * changing type from string to quantity)
 *
 * Generated from `performance.model_download_time_num`.
 */
exports.modelDownloadTimeNum = new timespan_1.default({
    category: "performance",
    name: "model_download_time_num",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, "millisecond");
/**
 * Time spent on loading a model into memory to start translation.
 * (Renamed from model_load_time to model_load_time_num as part of
 * changing type from string to quantity)
 *
 * Generated from `performance.model_load_time_num`.
 */
exports.modelLoadTimeNum = new timespan_1.default({
    category: "performance",
    name: "model_load_time_num",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, "millisecond");
/**
 * Time spent on translation by the translation engine.
 *
 * Generated from `performance.translation_engine_time`.
 */
exports.translationEngineTime = new timespan_1.default({
    category: "performance",
    name: "translation_engine_time",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
}, "millisecond");
/**
 * Speed of translation as measured by the translation engine.
 *
 * Generated from `performance.translation_engine_wps`.
 */
exports.translationEngineWps = new quantity_1.default({
    category: "performance",
    name: "translation_engine_wps",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * Quality estimation of translation.
 *
 * Generated from `performance.translation_quality`.
 */
exports.translationQuality = new string_1.default({
    category: "performance",
    name: "translation_quality",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});


/***/ }),

/***/ 3177:
/*!***********************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/pings.ts ***!
  \***********************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.custom = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const ping_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/ping */ 2455));
/**
 * A custom ping, sending time is fully controlled by the application.
 *
 * Generated from `custom`.
 */
exports.custom = new ping_1.default({
    includeClientId: true,
    sendIfEmpty: false,
    name: "custom",
    reasonCodes: [],
});


/***/ }),

/***/ 7659:
/*!*************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/telemetry/generated/service.ts ***!
  \*************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* eslint-disable */
var __importDefault = (this && this.__importDefault) || function (mod) {
    return (mod && mod.__esModule) ? mod : { "default": mod };
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.langMismatch = exports.notSupported = void 0;
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
// AUTOGENERATED BY glean_parser. DO NOT EDIT. DO NOT COMMIT.
const counter_1 = __importDefault(__webpack_require__(/*! @mozilla/glean/webext/private/metrics/counter */ 4429));
/**
 * Language pair of user and website languages is not supported.
 *
 * Generated from `service.not_supported`.
 */
exports.notSupported = new counter_1.default({
    category: "service",
    name: "not_supported",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});
/**
 * The user and website languages do not match.
 *
 * Generated from `service.lang_mismatch`.
 */
exports.langMismatch = new counter_1.default({
    category: "service",
    name: "lang_mismatch",
    sendInPings: ["custom"],
    lifetime: "ping",
    disabled: false,
});


/***/ }),

/***/ 484:
/*!*******************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/translation-api-clients/BergamotRestApiClient.ts ***!
  \*******************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BergamotRestApiClient = void 0;
const config_1 = __webpack_require__(/*! ../../../config */ 964);
const MS_IN_A_MINUTE = 60 * 1000;
// https://stackoverflow.com/a/57888548/682317
const fetchWithTimeout = (url, ms, options = {}) => {
    const controller = new AbortController();
    const promise = fetch(url, Object.assign({ signal: controller.signal }, options));
    const timeout = setTimeout(() => controller.abort(), ms);
    return promise.finally(() => clearTimeout(timeout));
};
class BergamotRestApiClient {
    constructor(requestTimeoutMs = null) {
        /**
         * Timeout after which we consider a ping submission failed.
         */
        this.requestTimeoutMs = 1.5 * MS_IN_A_MINUTE;
        /**
         * See https://github.com/browsermt/mts/wiki/BergamotAPI
         */
        this.composeSubmitRequestPath = () => {
            return `/api/bergamot/v1`;
        };
        this.composeUrl = () => {
            return `${config_1.config.bergamotRestApiUrl}${this.composeSubmitRequestPath()}`;
        };
        this.sendTranslationRequest = (texts, _from, _to, _translationRequestProgressCallback) => __awaiter(this, void 0, void 0, function* () {
            const payload = {
                text: texts,
                options: {
                    // "inputFormat": "wrappedText",
                    // "returnWordAlignment": true,
                    returnSentenceScore: true,
                    // "returnSoftAlignment": true,
                    // "returnQualityEstimate": true,
                    // "returnWordScores": true,
                    // "returnTokenization": true,
                    // "returnOriginal": true,
                },
            };
            const dataResponse = yield fetchWithTimeout(this.composeUrl(), this.requestTimeoutMs, {
                method: "POST",
                headers: {
                    Accept: "application/json",
                    "Content-Type": "application/json; charset=UTF-8",
                },
                body: JSON.stringify(payload),
            }).catch((error) => __awaiter(this, void 0, void 0, function* () {
                return Promise.reject(error);
            }));
            if (!dataResponse.ok) {
                throw new Error("Data response failed");
            }
            const parsedResponse = yield dataResponse.json();
            // console.log({ parsedResponse });
            const originalTexts = texts;
            const translatedTexts = [];
            const qeAnnotatedTranslatedTexts = [];
            parsedResponse.text.map((paragraph) => {
                const translationObjects = getBestTranslationObjectsOfEachSentenceInBergamotRestApiParagraph(paragraph);
                // TODO: Currently the rest server doesn't retain the leading/trailing
                // whitespace information of sentences. It is a bug on rest server side.
                // Once it is fixed there, we need to stop appending whitespaces.
                const separator = " ";
                // Join sentence translations
                const translatedPlainTextString = translationObjects
                    .map(({ translation }) => translation)
                    .join(separator);
                translatedTexts.push(translatedPlainTextString);
                // Generate QE Annotated HTML for each sentence
                const qeAnnotatedSentenceHTMLs = translationObjects.map(({ translation, sentenceScore }) => generateQEAnnotatedHTML(translation, sentenceScore));
                const qeAnnotatedTranslatedMarkup = qeAnnotatedSentenceHTMLs.join(separator);
                qeAnnotatedTranslatedTexts.push(qeAnnotatedTranslatedMarkup);
            });
            return {
                originalTexts,
                translatedTexts,
                qeAnnotatedTranslatedTexts,
            };
        });
        if (requestTimeoutMs) {
            this.requestTimeoutMs = requestTimeoutMs;
        }
    }
}
exports.BergamotRestApiClient = BergamotRestApiClient;
/**
 * This function parses 'Paragraph' entity of the response for the
 * the best translations and returns them
 */
function getBestTranslationObjectsOfEachSentenceInBergamotRestApiParagraph(paragraph) {
    const bestTranslations = [];
    paragraph[0].forEach(sentenceTranslationList => {
        // Depending on the request, there might be multiple 'best translations'.
        // We are fetching the best one (present in 'translation' field).
        const bestTranslation = sentenceTranslationList.nBest[0];
        bestTranslations.push(bestTranslation);
    });
    return bestTranslations;
}
/**
 * This function generates the Quality Estimation annotated HTML of a string
 * based on its score.
 *
 * @param   translation    input string
 * @param   score          score of the input string
 * @returns string         QE annotated HTML of input string
 */
function generateQEAnnotatedHTML(translation, score) {
    // Color choices and thresholds below are chosen based on intuitiveness.
    // They will be changed according to the UI design of Translator once it
    // is fixed.
    let color;
    if (score >= -0.2) {
        color = "green";
    }
    else if (score >= -0.5 && score < -0.2) {
        color = "black";
    }
    else if (score >= -0.8 && score < -0.5) {
        color = "mediumvioletred";
    }
    else {
        color = "red";
    }
    return `<span data-translation-qe-score="${score}" style="color:${color}"> ${translation}</span>`;
}


/***/ }),

/***/ 4222:
/*!*******************************************************************************************************!*\
  !*** ./src/core/ts/background-scripts/background.js/translation-api-clients/BergamotWasmApiClient.ts ***!
  \*******************************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BergamotWasmApiClient = void 0;
const BergamotTranslatorAPI_1 = __webpack_require__(/*! ../lib/BergamotTranslatorAPI */ 7333);
class BergamotWasmApiClient {
    constructor() {
        this.sendTranslationRequest = (texts, from, to, translationRequestProgressCallback) => __awaiter(this, void 0, void 0, function* () {
            if (typeof texts === "string") {
                texts = [texts];
            }
            const translationRequestProgress = {
                requestId: undefined,
                initiationTimestamp: Date.now(),
                queued: false,
                modelLoadNecessary: undefined,
                modelDownloadNecessary: undefined,
                modelDownloading: false,
                modelDownloadProgress: undefined,
                modelLoading: false,
                modelLoaded: undefined,
                modelLoadWallTimeMs: undefined,
                translationFinished: false,
                translationWallTimeMs: undefined,
                errorOccurred: false,
            };
            try {
                const translationResults = yield BergamotTranslatorAPI_1.BergamotTranslatorAPI.translate(texts, from, to, (translationRequestQueuedEventData) => {
                    translationRequestProgress.requestId =
                        translationRequestQueuedEventData.requestId;
                    translationRequestProgress.queued = true;
                    translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                }, (_modelWillLoadEventData) => {
                    translationRequestProgress.modelLoadNecessary = true;
                    translationRequestProgress.modelLoading = true;
                    translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                }, (modelDownloadProgressEventData) => {
                    translationRequestProgress.modelDownloadNecessary = true;
                    translationRequestProgress.modelDownloading =
                        modelDownloadProgressEventData.modelDownloadProgress
                            .bytesDownloaded <
                            modelDownloadProgressEventData.modelDownloadProgress
                                .bytesToDownload;
                    translationRequestProgress.modelDownloadProgress =
                        modelDownloadProgressEventData.modelDownloadProgress;
                    translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                }, (modelLoadedEventData) => {
                    translationRequestProgress.modelLoading = false;
                    translationRequestProgress.modelDownloading = false;
                    translationRequestProgress.modelLoaded = true;
                    translationRequestProgress.modelLoadWallTimeMs =
                        modelLoadedEventData.loadModelResults.modelLoadWallTimeMs;
                    translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                }, (translationFinishedEventData) => {
                    translationRequestProgress.queued = false;
                    translationRequestProgress.translationFinished = true;
                    translationRequestProgress.translationWallTimeMs =
                        translationFinishedEventData.translationWallTimeMs;
                    translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                });
                console.log({ translationResults });
                return translationResults;
            }
            catch (error) {
                translationRequestProgress.queued = false;
                translationRequestProgress.translationFinished = false;
                translationRequestProgress.errorOccurred = true;
                translationRequestProgressCallback(Object.assign({}, translationRequestProgress));
                throw error;
            }
        });
    }
}
exports.BergamotWasmApiClient = BergamotWasmApiClient;


/***/ }),

/***/ 8471:
/*!*********************************************************************************************!*\
  !*** ./src/core/ts/web-worker-scripts/translation-worker.js/bergamot-translator-version.ts ***!
  \*********************************************************************************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";

Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BERGAMOT_VERSION_FULL = void 0;
exports.BERGAMOT_VERSION_FULL = "v0.3.1+d264450";


/***/ }),

/***/ 7983:
/*!***********************************************************************************************!*\
  !*** ./src/firefox-infobar-ui/ts/background-scripts/background.js/NativeTranslateUiBroker.ts ***!
  \***********************************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.NativeTranslateUiBroker = void 0;
const LanguageSupport_1 = __webpack_require__(/*! ../../../../core/ts/shared-resources/LanguageSupport */ 5602);
const BaseTranslationState_1 = __webpack_require__(/*! ../../../../core/ts/shared-resources/models/BaseTranslationState */ 4779);
const Telemetry_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/telemetry/Telemetry */ 647);
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const mobx_1 = __webpack_require__(/*! mobx */ 9637);
const translateAllFramesInTab_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/lib/translateAllFramesInTab */ 5692);
/* eslint-disable no-unused-vars, no-shadow */
// TODO: update typescript-eslint when support for this kind of declaration is supported
var NativeTranslateUiStateInfobarState;
(function (NativeTranslateUiStateInfobarState) {
    NativeTranslateUiStateInfobarState[NativeTranslateUiStateInfobarState["STATE_OFFER"] = 0] = "STATE_OFFER";
    NativeTranslateUiStateInfobarState[NativeTranslateUiStateInfobarState["STATE_TRANSLATING"] = 1] = "STATE_TRANSLATING";
    NativeTranslateUiStateInfobarState[NativeTranslateUiStateInfobarState["STATE_TRANSLATED"] = 2] = "STATE_TRANSLATED";
    NativeTranslateUiStateInfobarState[NativeTranslateUiStateInfobarState["STATE_ERROR"] = 3] = "STATE_ERROR";
    NativeTranslateUiStateInfobarState[NativeTranslateUiStateInfobarState["STATE_UNAVAILABLE"] = 4] = "STATE_UNAVAILABLE";
})(NativeTranslateUiStateInfobarState || (NativeTranslateUiStateInfobarState = {}));
const browserWithExperimentAPIs = browser;
class NativeTranslateUiBroker {
    constructor(extensionState) {
        this.extensionState = extensionState;
        this.telemetryPreferencesEventsToObserve = [
            "onUploadEnabledPrefChange",
            "onCachedClientIDPrefChange",
        ];
        this.translateUiEventsToObserve = [
            "onInfoBarDisplayed",
            "onSelectTranslateTo",
            "onSelectTranslateFrom",
            "onInfoBarClosed",
            "onNeverTranslateSelectedLanguage",
            "onNeverTranslateThisSite",
            "onShowOriginalButtonPressed",
            "onShowTranslatedButtonPressed",
            "onTranslateButtonPressed",
            "onNotNowButtonPressed",
        ];
    }
    start() {
        return __awaiter(this, void 0, void 0, function* () {
            // Current value of Telemetry preferences
            const uploadEnabled = yield browserWithExperimentAPIs.experiments.telemetryPreferences.getUploadEnabledPref();
            const cachedClientID = yield browserWithExperimentAPIs.experiments.telemetryPreferences.getCachedClientIDPref();
            // Initialize telemetry
            const telemetryInactivityThresholdInSecondsOverride = yield browserWithExperimentAPIs.experiments.extensionPreferences.getTelemetryInactivityThresholdInSecondsOverridePref();
            Telemetry_1.telemetry.initialize(uploadEnabled, cachedClientID, telemetryInactivityThresholdInSecondsOverride);
            // The translationRelevantFxTelemetryMetrics gets available first once the telemetry environment has initialized
            browserWithExperimentAPIs.experiments.telemetryEnvironment
                .getTranslationRelevantFxTelemetryMetrics()
                .then((translationRelevantFxTelemetryMetrics) => {
                Telemetry_1.telemetry.setTranslationRelevantFxTelemetryMetrics(translationRelevantFxTelemetryMetrics);
            });
            // Hook up experiment API events with listeners in this class
            this.telemetryPreferencesEventsToObserve.map((eventRef) => {
                browserWithExperimentAPIs.experiments.telemetryPreferences[eventRef].addListener(this[eventRef].bind(this));
            });
            this.translateUiEventsToObserve.map((eventRef) => {
                browserWithExperimentAPIs.experiments.translateUi[eventRef].addListener(this[eventRef].bind(this));
            });
            yield browserWithExperimentAPIs.experiments.translateUi.start();
            const { summarizeLanguageSupport } = new LanguageSupport_1.LanguageSupport();
            // Boils down extension state to the subset relevant for the native translate ui
            const nativeTranslateUiStateFromTabTranslationState = (tts) => __awaiter(this, void 0, void 0, function* () {
                const infobarState = nativeTranslateUiStateInfobarStateFromTranslationStatus(tts.translationStatus);
                const detectedLanguageResults = tts.detectedLanguageResults;
                const { acceptedTargetLanguages, 
                // defaultSourceLanguage,
                defaultTargetLanguage, supportedSourceLanguages, 
                // supportedTargetLanguagesGivenDefaultSourceLanguage,
                allPossiblySupportedTargetLanguages, } = yield summarizeLanguageSupport(detectedLanguageResults);
                const translationDurationMs = Date.now() - tts.translationInitiationTimestamp;
                // Localized translation progress text
                const { modelLoading, queuedTranslationEngineRequestCount, modelDownloading, modelDownloadProgress, } = tts;
                let localizedTranslationProgressText;
                if (modelDownloading) {
                    const showDetailedProgress = modelDownloadProgress && modelDownloadProgress.bytesDownloaded > 0;
                    const percentDownloaded = Math.round((modelDownloadProgress.bytesDownloaded /
                        modelDownloadProgress.bytesToDownload) *
                        100);
                    const mbToDownload = Math.round((modelDownloadProgress.bytesToDownload / 1024 / 1024) * 10) / 10;
                    const localizedDetailedDownloadProgressText = browser.i18n.getMessage("detailedDownloadProgress", [percentDownloaded, mbToDownload]);
                    localizedTranslationProgressText = `(${browser.i18n.getMessage("currentlyDownloadingLanguageModel")}...${showDetailedProgress
                        ? ` ${localizedDetailedDownloadProgressText}`
                        : ``})`;
                }
                else if (modelLoading) {
                    localizedTranslationProgressText = `(${browser.i18n.getMessage("currentlyLoadingLanguageModel")}...)`;
                }
                else if (queuedTranslationEngineRequestCount > 0) {
                    // Using neutral plural form since there is no support for plural form in browser.i18n
                    localizedTranslationProgressText = `(${browser.i18n.getMessage("loadedLanguageModel")}. ${browser.i18n.getMessage("partsLeftToTranslate", queuedTranslationEngineRequestCount)})`;
                }
                else {
                    localizedTranslationProgressText = "";
                }
                return {
                    acceptedTargetLanguages,
                    detectedLanguageResults,
                    // defaultSourceLanguage,
                    defaultTargetLanguage,
                    infobarState,
                    translatedFrom: tts.translateFrom,
                    translatedTo: tts.translateTo,
                    originalShown: tts.showOriginal,
                    // Additionally, since supported source and target languages are only supported in specific pairs, keep these dynamic:
                    supportedSourceLanguages,
                    supportedTargetLanguages: allPossiblySupportedTargetLanguages,
                    // Translation progress
                    modelDownloading,
                    translationDurationMs,
                    localizedTranslationProgressText,
                };
            });
            const nativeTranslateUiStateInfobarStateFromTranslationStatus = (translationStatus) => {
                switch (translationStatus) {
                    case BaseTranslationState_1.TranslationStatus.UNKNOWN:
                        return NativeTranslateUiStateInfobarState.STATE_UNAVAILABLE;
                    case BaseTranslationState_1.TranslationStatus.UNAVAILABLE:
                        return NativeTranslateUiStateInfobarState.STATE_UNAVAILABLE;
                    case BaseTranslationState_1.TranslationStatus.DETECTING_LANGUAGE:
                        return NativeTranslateUiStateInfobarState.STATE_UNAVAILABLE;
                    case BaseTranslationState_1.TranslationStatus.LANGUAGE_NOT_DETECTED:
                        return NativeTranslateUiStateInfobarState.STATE_OFFER;
                    case BaseTranslationState_1.TranslationStatus.SOURCE_LANGUAGE_UNDERSTOOD:
                        return NativeTranslateUiStateInfobarState.STATE_UNAVAILABLE;
                    case BaseTranslationState_1.TranslationStatus.TRANSLATION_UNSUPPORTED:
                        return NativeTranslateUiStateInfobarState.STATE_UNAVAILABLE;
                    case BaseTranslationState_1.TranslationStatus.OFFER:
                        return NativeTranslateUiStateInfobarState.STATE_OFFER;
                    case BaseTranslationState_1.TranslationStatus.DOWNLOADING_TRANSLATION_MODEL:
                    case BaseTranslationState_1.TranslationStatus.TRANSLATING:
                        return NativeTranslateUiStateInfobarState.STATE_TRANSLATING;
                    case BaseTranslationState_1.TranslationStatus.TRANSLATED:
                        return NativeTranslateUiStateInfobarState.STATE_TRANSLATED;
                    case BaseTranslationState_1.TranslationStatus.ERROR:
                        return NativeTranslateUiStateInfobarState.STATE_ERROR;
                }
                throw Error(`No corresponding NativeTranslateUiStateInfobarState available for translationStatus "${translationStatus}"`);
            };
            // React to tab translation state changes
            mobx_1.reaction(() => this.extensionState.tabTranslationStates, (tabTranslationStates, _previousTabTranslationStates) => __awaiter(this, void 0, void 0, function* () {
                tabTranslationStates.forEach((tts, tabId) => __awaiter(this, void 0, void 0, function* () {
                    const uiState = yield nativeTranslateUiStateFromTabTranslationState(mobx_keystone_1.getSnapshot(tts));
                    browserWithExperimentAPIs.experiments.translateUi.setUiState(tabId, uiState);
                    // Send telemetry on some translation status changes
                    const hasChanged = property => {
                        const previousTabTranslationState = _previousTabTranslationStates.get(tabId);
                        return (!previousTabTranslationState ||
                            tts[property] !== previousTabTranslationState[property]);
                    };
                    if (hasChanged("translationStatus")) {
                        if (tts.translationStatus === BaseTranslationState_1.TranslationStatus.OFFER) {
                            Telemetry_1.telemetry.onTranslationStatusOffer(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                        if (tts.translationStatus ===
                            BaseTranslationState_1.TranslationStatus.TRANSLATION_UNSUPPORTED) {
                            Telemetry_1.telemetry.onTranslationStatusTranslationUnsupported(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                    }
                    if (hasChanged("modelLoadErrorOccurred")) {
                        if (tts.modelLoadErrorOccurred) {
                            Telemetry_1.telemetry.onModelLoadErrorOccurred(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                    }
                    if (hasChanged("modelDownloadErrorOccurred")) {
                        if (tts.modelDownloadErrorOccurred) {
                            Telemetry_1.telemetry.onModelDownloadErrorOccurred(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                    }
                    if (hasChanged("translationErrorOccurred")) {
                        if (tts.translationErrorOccurred) {
                            Telemetry_1.telemetry.onTranslationErrorOccurred(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                    }
                    if (hasChanged("otherErrorOccurred")) {
                        if (tts.otherErrorOccurred) {
                            Telemetry_1.telemetry.onOtherErrorOccurred(tabId, tts.effectiveTranslateFrom, tts.effectiveTranslateTo);
                        }
                    }
                    if (hasChanged("modelDownloadProgress")) {
                        if (tts.modelDownloadProgress) {
                            Telemetry_1.telemetry.updateInactivityTimerForAllTabs();
                        }
                    }
                }));
                // TODO: check _previousTabTranslationStates for those that had something and now should be inactive
            }));
        });
    }
    onUploadEnabledPrefChange() {
        return __awaiter(this, void 0, void 0, function* () {
            const uploadEnabled = yield browserWithExperimentAPIs.experiments.telemetryPreferences.getUploadEnabledPref();
            // console.debug("onUploadEnabledPrefChange", { uploadEnabled });
            Telemetry_1.telemetry.uploadEnabledPreferenceUpdated(uploadEnabled);
        });
    }
    onCachedClientIDPrefChange() {
        return __awaiter(this, void 0, void 0, function* () {
            const cachedClientID = yield browserWithExperimentAPIs.experiments.telemetryPreferences.getCachedClientIDPref();
            // console.debug("onCachedClientIDPrefChange", { cachedClientID });
            Telemetry_1.telemetry.setFirefoxClientId(cachedClientID);
        });
    }
    onInfoBarDisplayed(tabId, from, to) {
        console.debug("onInfoBarDisplayed", { tabId, from, to });
        Telemetry_1.telemetry.onInfoBarDisplayed(tabId, from, to);
    }
    onSelectTranslateFrom(tabId, newFrom, to) {
        console.debug("onSelectTranslateFrom", { tabId, newFrom, to });
        Telemetry_1.telemetry.onSelectTranslateFrom(tabId, newFrom, to);
    }
    onSelectTranslateTo(tabId, from, newTo) {
        console.debug("onSelectTranslateTo", { tabId, from, newTo });
        Telemetry_1.telemetry.onSelectTranslateFrom(tabId, from, newTo);
    }
    onInfoBarClosed(tabId, from, to) {
        console.debug("onInfoBarClosed", { tabId, from, to });
        Telemetry_1.telemetry.onInfoBarClosed(tabId, from, to);
    }
    onNeverTranslateSelectedLanguage(tabId, from, to) {
        console.debug("onNeverTranslateSelectedLanguage", { tabId, from, to });
        Telemetry_1.telemetry.onNeverTranslateSelectedLanguage(tabId, from, to);
    }
    onNeverTranslateThisSite(tabId, from, to) {
        console.debug("onNeverTranslateThisSite", { tabId, from, to });
        Telemetry_1.telemetry.onNeverTranslateThisSite(tabId, from, to);
    }
    onShowOriginalButtonPressed(tabId, from, to) {
        console.debug("onShowOriginalButtonPressed", { tabId, from, to });
        Telemetry_1.telemetry.onShowOriginalButtonPressed(tabId, from, to);
        this.extensionState.showOriginalInTab(tabId);
    }
    onShowTranslatedButtonPressed(tabId, from, to) {
        console.debug("onShowTranslatedButtonPressed", { tabId, from, to });
        Telemetry_1.telemetry.onShowTranslatedButtonPressed(tabId, from, to);
        this.extensionState.hideOriginalInTab(tabId);
    }
    onTranslateButtonPressed(tabId, from, to) {
        console.debug("onTranslateButtonPressed", { tabId, from, to });
        Telemetry_1.telemetry.onTranslateButtonPressed(tabId, from, to);
        translateAllFramesInTab_1.translateAllFramesInTab(tabId, from, to, this.extensionState);
    }
    onNotNowButtonPressed(tabId, from, to) {
        console.debug("onNotNowButtonPressed", { tabId, from, to });
        Telemetry_1.telemetry.onNotNowButtonPressed(tabId, from, to);
    }
    stop() {
        return __awaiter(this, void 0, void 0, function* () {
            yield browserWithExperimentAPIs.experiments.translateUi.stop();
            this.telemetryPreferencesEventsToObserve.map(eventRef => {
                browserWithExperimentAPIs.experiments.telemetryPreferences[eventRef].removeListener(this[eventRef]);
            });
            this.translateUiEventsToObserve.map(eventRef => {
                browserWithExperimentAPIs.experiments.translateUi[eventRef].removeListener(this[eventRef]);
            });
            yield Telemetry_1.telemetry.cleanup();
        });
    }
}
exports.NativeTranslateUiBroker = NativeTranslateUiBroker;


/***/ }),

/***/ 9755:
/*!*****************************************************************************!*\
  !*** ./src/firefox-infobar-ui/ts/background-scripts/background.js/index.ts ***!
  \*****************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __awaiter = (this && this.__awaiter) || function (thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
/* eslint no-unused-vars: ["error", { "varsIgnorePattern": "(extensionGlue)" }]*/
const ErrorReporting_1 = __webpack_require__(/*! ../../../../core/ts/shared-resources/ErrorReporting */ 3345);
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const localStorageWrapper_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/state-management/localStorageWrapper */ 5813);
const contentScriptFrameInfoPortListener_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/contentScriptFrameInfoPortListener */ 220);
const createBackgroundContextRootStore_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/state-management/createBackgroundContextRootStore */ 8911);
const contentScriptLanguageDetectorProxyPortListener_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/contentScriptLanguageDetectorProxyPortListener */ 6047);
const Store_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/state-management/Store */ 4517);
const connectRootStoreToDevTools_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/state-management/connectRootStoreToDevTools */ 6432);
const MobxKeystoneBackgroundContextHost_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/state-management/MobxKeystoneBackgroundContextHost */ 805);
const NativeTranslateUiBroker_1 = __webpack_require__(/*! ./NativeTranslateUiBroker */ 7983);
const contentScriptBergamotApiClientPortListener_1 = __webpack_require__(/*! ../../../../core/ts/background-scripts/background.js/contentScriptBergamotApiClientPortListener */ 410);
const bergamot_translator_version_1 = __webpack_require__(/*! ../../../../core/ts/web-worker-scripts/translation-worker.js/bergamot-translator-version */ 8471);
const config_1 = __webpack_require__(/*! ../../../../core/ts/config */ 964);
const store = new Store_1.Store(localStorageWrapper_1.localStorageWrapper);
/* eslint-enable no-unused-vars */
/**
 * Ties together overall execution logic and allows content scripts
 * to access persistent storage and background-context API:s via cross-process messaging
 */
class ExtensionGlue {
    constructor() {
        this.extensionState = createBackgroundContextRootStore_1.createBackgroundContextRootStore();
    }
    init() {
        return __awaiter(this, void 0, void 0, function* () {
            // Add version to extension log output to facilitate general troubleshooting
            const manifest = webextension_polyfill_ts_1.browser.runtime.getManifest();
            console.info(`Extension ${manifest.name} version ${manifest.version} [build id "${config_1.config.extensionBuildId}"] (with bergamot-translator ${bergamot_translator_version_1.BERGAMOT_VERSION_FULL}) initializing.`);
            // Initiate the root extension state store
            this.extensionState = createBackgroundContextRootStore_1.createBackgroundContextRootStore();
            // Allow for easy introspection of extension state in development mode
            if (false) {}
            // Make the root extension state store available to content script contexts
            const mobxKeystoneBackgroundContextHost = new MobxKeystoneBackgroundContextHost_1.MobxKeystoneBackgroundContextHost();
            mobxKeystoneBackgroundContextHost.init(this.extensionState);
            // Enable error reporting if not opted out
            this.extensionPreferencesPortListener = yield ErrorReporting_1.initErrorReportingInBackgroundScript(store, [
                "port-from-options-ui:index",
                "port-from-options-ui:form",
                "port-from-main-interface:index",
                "port-from-get-started:index",
                "port-from-get-started:component",
                "port-from-dom-translation-content-script:index",
            ]);
        });
    }
    start() {
        return __awaiter(this, void 0, void 0, function* () {
            // Set up native translate ui
            this.nativeTranslateUiBroker = new NativeTranslateUiBroker_1.NativeTranslateUiBroker(this.extensionState);
            yield this.nativeTranslateUiBroker.start();
            // Set up content script port listeners
            this.contentScriptLanguageDetectorProxyPortListener = contentScriptLanguageDetectorProxyPortListener_1.contentScriptLanguageDetectorProxyPortListener;
            this.contentScriptBergamotApiClientPortListener = contentScriptBergamotApiClientPortListener_1.contentScriptBergamotApiClientPortListener;
            this.contentScriptFrameInfoPortListener = contentScriptFrameInfoPortListener_1.contentScriptFrameInfoPortListener;
            [
                "contentScriptLanguageDetectorProxyPortListener",
                "contentScriptBergamotApiClientPortListener",
                "contentScriptFrameInfoPortListener",
            ].forEach(listenerName => {
                webextension_polyfill_ts_1.browser.runtime.onConnect.addListener(this[listenerName]);
            });
        });
    }
    // TODO: Run this cleanup-method when relevant
    cleanup() {
        return __awaiter(this, void 0, void 0, function* () {
            yield this.nativeTranslateUiBroker.stop();
            // Tear down content script port listeners
            [
                this.extensionPreferencesPortListener,
                this.contentScriptLanguageDetectorProxyPortListener,
                this.contentScriptBergamotApiClientPortListener,
                this.contentScriptFrameInfoPortListener,
            ].forEach((listener) => {
                try {
                    webextension_polyfill_ts_1.browser.runtime.onConnect.removeListener(listener);
                }
                catch (err) {
                    console.warn(`Port listener removal error`, err);
                }
            });
        });
    }
}
// make an instance of the ExtensionGlue class available to the extension background context
const extensionGlue = (window.extensionGlue = new ExtensionGlue());
// migrations
const runMigrations = () => __awaiter(void 0, void 0, void 0, function* () {
    console.info("Running relevant migrations");
});
// init the extension glue on every extension load
function onEveryExtensionLoad() {
    return __awaiter(this, void 0, void 0, function* () {
        yield extensionGlue.init();
        yield runMigrations();
        yield extensionGlue.start();
    });
}
onEveryExtensionLoad().then();
// Open and keep the test-runner open after each extension reload when in development mode
if (false) {}


/***/ })

/******/ 	});
/************************************************************************/
/******/ 	// The module cache
/******/ 	var __webpack_module_cache__ = {};
/******/ 	
/******/ 	// The require function
/******/ 	function __webpack_require__(moduleId) {
/******/ 		// Check if module is in cache
/******/ 		if(__webpack_module_cache__[moduleId]) {
/******/ 			return __webpack_module_cache__[moduleId].exports;
/******/ 		}
/******/ 		// Create a new module (and put it into the cache)
/******/ 		var module = __webpack_module_cache__[moduleId] = {
/******/ 			id: moduleId,
/******/ 			loaded: false,
/******/ 			exports: {}
/******/ 		};
/******/ 	
/******/ 		// Execute the module function
/******/ 		__webpack_modules__[moduleId].call(module.exports, module, module.exports, __webpack_require__);
/******/ 	
/******/ 		// Flag the module as loaded
/******/ 		module.loaded = true;
/******/ 	
/******/ 		// Return the exports of the module
/******/ 		return module.exports;
/******/ 	}
/******/ 	
/******/ 	// expose the modules object (__webpack_modules__)
/******/ 	__webpack_require__.m = __webpack_modules__;
/******/ 	
/******/ 	// the startup function
/******/ 	// It's empty as some runtime module handles the default behavior
/******/ 	__webpack_require__.x = x => {};
/************************************************************************/
/******/ 	/* webpack/runtime/compat get default export */
/******/ 	(() => {
/******/ 		// getDefaultExport function for compatibility with non-harmony modules
/******/ 		__webpack_require__.n = (module) => {
/******/ 			var getter = module && module.__esModule ?
/******/ 				() => (module['default']) :
/******/ 				() => (module);
/******/ 			__webpack_require__.d(getter, { a: getter });
/******/ 			return getter;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/define property getters */
/******/ 	(() => {
/******/ 		// define getter functions for harmony exports
/******/ 		__webpack_require__.d = (exports, definition) => {
/******/ 			for(var key in definition) {
/******/ 				if(__webpack_require__.o(definition, key) && !__webpack_require__.o(exports, key)) {
/******/ 					Object.defineProperty(exports, key, { enumerable: true, get: definition[key] });
/******/ 				}
/******/ 			}
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/global */
/******/ 	(() => {
/******/ 		__webpack_require__.g = (function() {
/******/ 			if (typeof globalThis === 'object') return globalThis;
/******/ 			try {
/******/ 				return this || new Function('return this')();
/******/ 			} catch (e) {
/******/ 				if (typeof window === 'object') return window;
/******/ 			}
/******/ 		})();
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/harmony module decorator */
/******/ 	(() => {
/******/ 		__webpack_require__.hmd = (module) => {
/******/ 			module = Object.create(module);
/******/ 			if (!module.children) module.children = [];
/******/ 			Object.defineProperty(module, 'exports', {
/******/ 				enumerable: true,
/******/ 				set: () => {
/******/ 					throw new Error('ES Modules may not assign module.exports or exports.*, Use ESM export syntax, instead: ' + module.id);
/******/ 				}
/******/ 			});
/******/ 			return module;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/hasOwnProperty shorthand */
/******/ 	(() => {
/******/ 		__webpack_require__.o = (obj, prop) => (Object.prototype.hasOwnProperty.call(obj, prop))
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/make namespace object */
/******/ 	(() => {
/******/ 		// define __esModule on exports
/******/ 		__webpack_require__.r = (exports) => {
/******/ 			if(typeof Symbol !== 'undefined' && Symbol.toStringTag) {
/******/ 				Object.defineProperty(exports, Symbol.toStringTag, { value: 'Module' });
/******/ 			}
/******/ 			Object.defineProperty(exports, '__esModule', { value: true });
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/node module decorator */
/******/ 	(() => {
/******/ 		__webpack_require__.nmd = (module) => {
/******/ 			module.paths = [];
/******/ 			if (!module.children) module.children = [];
/******/ 			return module;
/******/ 		};
/******/ 	})();
/******/ 	
/******/ 	/* webpack/runtime/jsonp chunk loading */
/******/ 	(() => {
/******/ 		// no baseURI
/******/ 		
/******/ 		// object to store loaded and loading chunks
/******/ 		// undefined = chunk not loaded, null = chunk preloaded/prefetched
/******/ 		// Promise = chunk loading, 0 = chunk loaded
/******/ 		var installedChunks = {
/******/ 			352: 0
/******/ 		};
/******/ 		
/******/ 		var deferredModules = [
/******/ 			[9755,351]
/******/ 		];
/******/ 		// no chunk on demand loading
/******/ 		
/******/ 		// no prefetching
/******/ 		
/******/ 		// no preloaded
/******/ 		
/******/ 		// no HMR
/******/ 		
/******/ 		// no HMR manifest
/******/ 		
/******/ 		var checkDeferredModules = x => {};
/******/ 		
/******/ 		// install a JSONP callback for chunk loading
/******/ 		var webpackJsonpCallback = (parentChunkLoadingFunction, data) => {
/******/ 			var [chunkIds, moreModules, runtime, executeModules] = data;
/******/ 			// add "moreModules" to the modules object,
/******/ 			// then flag all "chunkIds" as loaded and fire callback
/******/ 			var moduleId, chunkId, i = 0, resolves = [];
/******/ 			for(;i < chunkIds.length; i++) {
/******/ 				chunkId = chunkIds[i];
/******/ 				if(__webpack_require__.o(installedChunks, chunkId) && installedChunks[chunkId]) {
/******/ 					resolves.push(installedChunks[chunkId][0]);
/******/ 				}
/******/ 				installedChunks[chunkId] = 0;
/******/ 			}
/******/ 			for(moduleId in moreModules) {
/******/ 				if(__webpack_require__.o(moreModules, moduleId)) {
/******/ 					__webpack_require__.m[moduleId] = moreModules[moduleId];
/******/ 				}
/******/ 			}
/******/ 			if(runtime) runtime(__webpack_require__);
/******/ 			if(parentChunkLoadingFunction) parentChunkLoadingFunction(data);
/******/ 			while(resolves.length) {
/******/ 				resolves.shift()();
/******/ 			}
/******/ 		
/******/ 			// add entry modules from loaded chunk to deferred list
/******/ 			if(executeModules) deferredModules.push.apply(deferredModules, executeModules);
/******/ 		
/******/ 			// run deferred modules when all chunks ready
/******/ 			return checkDeferredModules();
/******/ 		}
/******/ 		
/******/ 		var chunkLoadingGlobal = self["webpackChunkbergamot_browser_extension"] = self["webpackChunkbergamot_browser_extension"] || [];
/******/ 		chunkLoadingGlobal.forEach(webpackJsonpCallback.bind(null, 0));
/******/ 		chunkLoadingGlobal.push = webpackJsonpCallback.bind(null, chunkLoadingGlobal.push.bind(chunkLoadingGlobal));
/******/ 		
/******/ 		function checkDeferredModulesImpl() {
/******/ 			var result;
/******/ 			for(var i = 0; i < deferredModules.length; i++) {
/******/ 				var deferredModule = deferredModules[i];
/******/ 				var fulfilled = true;
/******/ 				for(var j = 1; j < deferredModule.length; j++) {
/******/ 					var depId = deferredModule[j];
/******/ 					if(installedChunks[depId] !== 0) fulfilled = false;
/******/ 				}
/******/ 				if(fulfilled) {
/******/ 					deferredModules.splice(i--, 1);
/******/ 					result = __webpack_require__(__webpack_require__.s = deferredModule[0]);
/******/ 				}
/******/ 			}
/******/ 			if(deferredModules.length === 0) {
/******/ 				__webpack_require__.x();
/******/ 				__webpack_require__.x = x => {};
/******/ 			}
/******/ 			return result;
/******/ 		}
/******/ 		var startup = __webpack_require__.x;
/******/ 		__webpack_require__.x = () => {
/******/ 			// reset startup function so it can be called again when more startup code is added
/******/ 			__webpack_require__.x = startup || (x => {});
/******/ 			return (checkDeferredModules = checkDeferredModulesImpl)();
/******/ 		};
/******/ 	})();
/******/ 	
/************************************************************************/
/******/ 	
/******/ 	// run startup
/******/ 	var __webpack_exports__ = __webpack_require__.x();
/******/ 	
/******/ })()
;
//# sourceMappingURL=background.js.map