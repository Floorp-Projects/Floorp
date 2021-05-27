(self["webpackChunkbergamot_browser_extension"] = self["webpackChunkbergamot_browser_extension"] || []).push([[351],{

/***/ 6614:
/*!****************************************************************!*\
  !*** ./node_modules/@sentry/browser/esm/index.js + 51 modules ***!
  \****************************************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "BrowserClient": () => (/* reexport */ BrowserClient),
  "Hub": () => (/* reexport */ Hub),
  "Integrations": () => (/* binding */ INTEGRATIONS),
  "SDK_NAME": () => (/* reexport */ SDK_NAME),
  "SDK_VERSION": () => (/* reexport */ SDK_VERSION),
  "Scope": () => (/* reexport */ Scope),
  "Severity": () => (/* reexport */ Severity),
  "Status": () => (/* reexport */ Status),
  "Transports": () => (/* reexport */ transports_namespaceObject),
  "addBreadcrumb": () => (/* reexport */ addBreadcrumb),
  "addGlobalEventProcessor": () => (/* reexport */ addGlobalEventProcessor),
  "captureEvent": () => (/* reexport */ captureEvent),
  "captureException": () => (/* reexport */ captureException),
  "captureMessage": () => (/* reexport */ captureMessage),
  "close": () => (/* reexport */ sdk_close),
  "configureScope": () => (/* reexport */ configureScope),
  "defaultIntegrations": () => (/* reexport */ defaultIntegrations),
  "eventFromException": () => (/* reexport */ eventFromException),
  "eventFromMessage": () => (/* reexport */ eventFromMessage),
  "flush": () => (/* reexport */ flush),
  "forceLoad": () => (/* reexport */ forceLoad),
  "getCurrentHub": () => (/* reexport */ getCurrentHub),
  "getHubFromCarrier": () => (/* reexport */ getHubFromCarrier),
  "init": () => (/* reexport */ init),
  "injectReportDialog": () => (/* reexport */ injectReportDialog),
  "lastEventId": () => (/* reexport */ lastEventId),
  "makeMain": () => (/* reexport */ makeMain),
  "onLoad": () => (/* reexport */ onLoad),
  "setContext": () => (/* reexport */ setContext),
  "setExtra": () => (/* reexport */ setExtra),
  "setExtras": () => (/* reexport */ setExtras),
  "setTag": () => (/* reexport */ setTag),
  "setTags": () => (/* reexport */ setTags),
  "setUser": () => (/* reexport */ setUser),
  "showReportDialog": () => (/* reexport */ showReportDialog),
  "startTransaction": () => (/* reexport */ startTransaction),
  "withScope": () => (/* reexport */ withScope),
  "wrap": () => (/* reexport */ sdk_wrap)
});

// NAMESPACE OBJECT: ./node_modules/@sentry/core/esm/integrations/index.js
var integrations_namespaceObject = {};
__webpack_require__.r(integrations_namespaceObject);
__webpack_require__.d(integrations_namespaceObject, {
  "FunctionToString": () => (FunctionToString),
  "InboundFilters": () => (InboundFilters)
});

// NAMESPACE OBJECT: ./node_modules/@sentry/browser/esm/integrations/index.js
var esm_integrations_namespaceObject = {};
__webpack_require__.r(esm_integrations_namespaceObject);
__webpack_require__.d(esm_integrations_namespaceObject, {
  "Breadcrumbs": () => (Breadcrumbs),
  "GlobalHandlers": () => (GlobalHandlers),
  "LinkedErrors": () => (LinkedErrors),
  "TryCatch": () => (TryCatch),
  "UserAgent": () => (UserAgent)
});

// NAMESPACE OBJECT: ./node_modules/@sentry/browser/esm/transports/index.js
var transports_namespaceObject = {};
__webpack_require__.r(transports_namespaceObject);
__webpack_require__.d(transports_namespaceObject, {
  "BaseTransport": () => (BaseTransport),
  "FetchTransport": () => (FetchTransport),
  "XHRTransport": () => (XHRTransport)
});

;// CONCATENATED MODULE: ./node_modules/tslib/tslib.es6.js
/*! *****************************************************************************
Copyright (c) Microsoft Corporation.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
***************************************************************************** */
/* global Reflect, Promise */

var extendStatics = function(d, b) {
    extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (b.hasOwnProperty(p)) d[p] = b[p]; };
    return extendStatics(d, b);
};

function __extends(d, b) {
    extendStatics(d, b);
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
}

var __assign = function() {
    __assign = Object.assign || function __assign(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p)) t[p] = s[p];
        }
        return t;
    }
    return __assign.apply(this, arguments);
}

function __rest(s, e) {
    var t = {};
    for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p) && e.indexOf(p) < 0)
        t[p] = s[p];
    if (s != null && typeof Object.getOwnPropertySymbols === "function")
        for (var i = 0, p = Object.getOwnPropertySymbols(s); i < p.length; i++) {
            if (e.indexOf(p[i]) < 0 && Object.prototype.propertyIsEnumerable.call(s, p[i]))
                t[p[i]] = s[p[i]];
        }
    return t;
}

function __decorate(decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
}

function __param(paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
}

function __metadata(metadataKey, metadataValue) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(metadataKey, metadataValue);
}

function __awaiter(thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
}

function __generator(thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
}

function __createBinding(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
}

function __exportStar(m, exports) {
    for (var p in m) if (p !== "default" && !exports.hasOwnProperty(p)) exports[p] = m[p];
}

function __values(o) {
    var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
    if (m) return m.call(o);
    if (o && typeof o.length === "number") return {
        next: function () {
            if (o && i >= o.length) o = void 0;
            return { value: o && o[i++], done: !o };
        }
    };
    throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
}

function __read(o, n) {
    var m = typeof Symbol === "function" && o[Symbol.iterator];
    if (!m) return o;
    var i = m.call(o), r, ar = [], e;
    try {
        while ((n === void 0 || n-- > 0) && !(r = i.next()).done) ar.push(r.value);
    }
    catch (error) { e = { error: error }; }
    finally {
        try {
            if (r && !r.done && (m = i["return"])) m.call(i);
        }
        finally { if (e) throw e.error; }
    }
    return ar;
}

function tslib_es6_spread() {
    for (var ar = [], i = 0; i < arguments.length; i++)
        ar = ar.concat(__read(arguments[i]));
    return ar;
}

function __spreadArrays() {
    for (var s = 0, i = 0, il = arguments.length; i < il; i++) s += arguments[i].length;
    for (var r = Array(s), k = 0, i = 0; i < il; i++)
        for (var a = arguments[i], j = 0, jl = a.length; j < jl; j++, k++)
            r[k] = a[j];
    return r;
};

function __await(v) {
    return this instanceof __await ? (this.v = v, this) : new __await(v);
}

function __asyncGenerator(thisArg, _arguments, generator) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var g = generator.apply(thisArg, _arguments || []), i, q = [];
    return i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i;
    function verb(n) { if (g[n]) i[n] = function (v) { return new Promise(function (a, b) { q.push([n, v, a, b]) > 1 || resume(n, v); }); }; }
    function resume(n, v) { try { step(g[n](v)); } catch (e) { settle(q[0][3], e); } }
    function step(r) { r.value instanceof __await ? Promise.resolve(r.value.v).then(fulfill, reject) : settle(q[0][2], r); }
    function fulfill(value) { resume("next", value); }
    function reject(value) { resume("throw", value); }
    function settle(f, v) { if (f(v), q.shift(), q.length) resume(q[0][0], q[0][1]); }
}

function __asyncDelegator(o) {
    var i, p;
    return i = {}, verb("next"), verb("throw", function (e) { throw e; }), verb("return"), i[Symbol.iterator] = function () { return this; }, i;
    function verb(n, f) { i[n] = o[n] ? function (v) { return (p = !p) ? { value: __await(o[n](v)), done: n === "return" } : f ? f(v) : v; } : f; }
}

function __asyncValues(o) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var m = o[Symbol.asyncIterator], i;
    return m ? m.call(o) : (o = typeof __values === "function" ? __values(o) : o[Symbol.iterator](), i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i);
    function verb(n) { i[n] = o[n] && function (v) { return new Promise(function (resolve, reject) { v = o[n](v), settle(resolve, reject, v.done, v.value); }); }; }
    function settle(resolve, reject, d, v) { Promise.resolve(v).then(function(v) { resolve({ value: v, done: d }); }, reject); }
}

function __makeTemplateObject(cooked, raw) {
    if (Object.defineProperty) { Object.defineProperty(cooked, "raw", { value: raw }); } else { cooked.raw = raw; }
    return cooked;
};

function __importStar(mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (Object.hasOwnProperty.call(mod, k)) result[k] = mod[k];
    result.default = mod;
    return result;
}

function __importDefault(mod) {
    return (mod && mod.__esModule) ? mod : { default: mod };
}

function __classPrivateFieldGet(receiver, privateMap) {
    if (!privateMap.has(receiver)) {
        throw new TypeError("attempted to get private field on non-instance");
    }
    return privateMap.get(receiver);
}

function __classPrivateFieldSet(receiver, privateMap, value) {
    if (!privateMap.has(receiver)) {
        throw new TypeError("attempted to set private field on non-instance");
    }
    privateMap.set(receiver, value);
    return value;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/types/esm/severity.js
/** JSDoc */
// eslint-disable-next-line import/export
var Severity;
(function (Severity) {
    /** JSDoc */
    Severity["Fatal"] = "fatal";
    /** JSDoc */
    Severity["Error"] = "error";
    /** JSDoc */
    Severity["Warning"] = "warning";
    /** JSDoc */
    Severity["Log"] = "log";
    /** JSDoc */
    Severity["Info"] = "info";
    /** JSDoc */
    Severity["Debug"] = "debug";
    /** JSDoc */
    Severity["Critical"] = "critical";
})(Severity || (Severity = {}));
// eslint-disable-next-line @typescript-eslint/no-namespace, import/export
(function (Severity) {
    /**
     * Converts a string-based level into a {@link Severity}.
     *
     * @param level string representation of Severity
     * @returns Severity
     */
    function fromString(level) {
        switch (level) {
            case 'debug':
                return Severity.Debug;
            case 'info':
                return Severity.Info;
            case 'warn':
            case 'warning':
                return Severity.Warning;
            case 'error':
                return Severity.Error;
            case 'fatal':
                return Severity.Fatal;
            case 'critical':
                return Severity.Critical;
            case 'log':
            default:
                return Severity.Log;
        }
    }
    Severity.fromString = fromString;
})(Severity || (Severity = {}));

;// CONCATENATED MODULE: ./node_modules/@sentry/types/esm/status.js
/** The status of an event. */
// eslint-disable-next-line import/export
var Status;
(function (Status) {
    /** The status could not be determined. */
    Status["Unknown"] = "unknown";
    /** The event was skipped due to configuration or callbacks. */
    Status["Skipped"] = "skipped";
    /** The event was sent to Sentry successfully. */
    Status["Success"] = "success";
    /** The client is currently rate limited and will try again later. */
    Status["RateLimit"] = "rate_limit";
    /** The event could not be processed. */
    Status["Invalid"] = "invalid";
    /** A server-side error ocurred during submission. */
    Status["Failed"] = "failed";
})(Status || (Status = {}));
// eslint-disable-next-line @typescript-eslint/no-namespace, import/export
(function (Status) {
    /**
     * Converts a HTTP status code into a {@link Status}.
     *
     * @param code The HTTP response status code.
     * @returns The send status or {@link Status.Unknown}.
     */
    function fromHttpCode(code) {
        if (code >= 200 && code < 300) {
            return Status.Success;
        }
        if (code === 429) {
            return Status.RateLimit;
        }
        if (code >= 400 && code < 500) {
            return Status.Invalid;
        }
        if (code >= 500) {
            return Status.Failed;
        }
        return Status.Unknown;
    }
    Status.fromHttpCode = fromHttpCode;
})(Status || (Status = {}));

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/is.js
/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable @typescript-eslint/explicit-module-boundary-types */
/**
 * Checks whether given value's type is one of a few Error or Error-like
 * {@link isError}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isError(wat) {
    switch (Object.prototype.toString.call(wat)) {
        case '[object Error]':
            return true;
        case '[object Exception]':
            return true;
        case '[object DOMException]':
            return true;
        default:
            return isInstanceOf(wat, Error);
    }
}
/**
 * Checks whether given value's type is ErrorEvent
 * {@link isErrorEvent}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isErrorEvent(wat) {
    return Object.prototype.toString.call(wat) === '[object ErrorEvent]';
}
/**
 * Checks whether given value's type is DOMError
 * {@link isDOMError}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isDOMError(wat) {
    return Object.prototype.toString.call(wat) === '[object DOMError]';
}
/**
 * Checks whether given value's type is DOMException
 * {@link isDOMException}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isDOMException(wat) {
    return Object.prototype.toString.call(wat) === '[object DOMException]';
}
/**
 * Checks whether given value's type is a string
 * {@link isString}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isString(wat) {
    return Object.prototype.toString.call(wat) === '[object String]';
}
/**
 * Checks whether given value's is a primitive (undefined, null, number, boolean, string)
 * {@link isPrimitive}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isPrimitive(wat) {
    return wat === null || (typeof wat !== 'object' && typeof wat !== 'function');
}
/**
 * Checks whether given value's type is an object literal
 * {@link isPlainObject}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isPlainObject(wat) {
    return Object.prototype.toString.call(wat) === '[object Object]';
}
/**
 * Checks whether given value's type is an Event instance
 * {@link isEvent}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isEvent(wat) {
    return typeof Event !== 'undefined' && isInstanceOf(wat, Event);
}
/**
 * Checks whether given value's type is an Element instance
 * {@link isElement}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isElement(wat) {
    return typeof Element !== 'undefined' && isInstanceOf(wat, Element);
}
/**
 * Checks whether given value's type is an regexp
 * {@link isRegExp}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isRegExp(wat) {
    return Object.prototype.toString.call(wat) === '[object RegExp]';
}
/**
 * Checks whether given value has a then function.
 * @param wat A value to be checked.
 */
function isThenable(wat) {
    // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
    return Boolean(wat && wat.then && typeof wat.then === 'function');
}
/**
 * Checks whether given value's type is a SyntheticEvent
 * {@link isSyntheticEvent}.
 *
 * @param wat A value to be checked.
 * @returns A boolean representing the result.
 */
function isSyntheticEvent(wat) {
    return isPlainObject(wat) && 'nativeEvent' in wat && 'preventDefault' in wat && 'stopPropagation' in wat;
}
/**
 * Checks whether given value's type is an instance of provided constructor.
 * {@link isInstanceOf}.
 *
 * @param wat A value to be checked.
 * @param base A constructor to be used in a check.
 * @returns A boolean representing the result.
 */
function isInstanceOf(wat, base) {
    try {
        return wat instanceof base;
    }
    catch (_e) {
        return false;
    }
}

// EXTERNAL MODULE: ./node_modules/@sentry/utils/esm/time.js
var time = __webpack_require__(7685);
;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/syncpromise.js
/* eslint-disable @typescript-eslint/explicit-function-return-type */
/* eslint-disable @typescript-eslint/typedef */
/* eslint-disable @typescript-eslint/explicit-module-boundary-types */
/* eslint-disable @typescript-eslint/no-explicit-any */

/** SyncPromise internal states */
var States;
(function (States) {
    /** Pending */
    States["PENDING"] = "PENDING";
    /** Resolved / OK */
    States["RESOLVED"] = "RESOLVED";
    /** Rejected / Error */
    States["REJECTED"] = "REJECTED";
})(States || (States = {}));
/**
 * Thenable class that behaves like a Promise and follows it's interface
 * but is not async internally
 */
var SyncPromise = /** @class */ (function () {
    function SyncPromise(executor) {
        var _this = this;
        this._state = States.PENDING;
        this._handlers = [];
        /** JSDoc */
        this._resolve = function (value) {
            _this._setResult(States.RESOLVED, value);
        };
        /** JSDoc */
        this._reject = function (reason) {
            _this._setResult(States.REJECTED, reason);
        };
        /** JSDoc */
        this._setResult = function (state, value) {
            if (_this._state !== States.PENDING) {
                return;
            }
            if (isThenable(value)) {
                value.then(_this._resolve, _this._reject);
                return;
            }
            _this._state = state;
            _this._value = value;
            _this._executeHandlers();
        };
        // TODO: FIXME
        /** JSDoc */
        this._attachHandler = function (handler) {
            _this._handlers = _this._handlers.concat(handler);
            _this._executeHandlers();
        };
        /** JSDoc */
        this._executeHandlers = function () {
            if (_this._state === States.PENDING) {
                return;
            }
            var cachedHandlers = _this._handlers.slice();
            _this._handlers = [];
            cachedHandlers.forEach(function (handler) {
                if (handler.done) {
                    return;
                }
                if (_this._state === States.RESOLVED) {
                    if (handler.onfulfilled) {
                        // eslint-disable-next-line @typescript-eslint/no-floating-promises
                        handler.onfulfilled(_this._value);
                    }
                }
                if (_this._state === States.REJECTED) {
                    if (handler.onrejected) {
                        handler.onrejected(_this._value);
                    }
                }
                handler.done = true;
            });
        };
        try {
            executor(this._resolve, this._reject);
        }
        catch (e) {
            this._reject(e);
        }
    }
    /** JSDoc */
    SyncPromise.resolve = function (value) {
        return new SyncPromise(function (resolve) {
            resolve(value);
        });
    };
    /** JSDoc */
    SyncPromise.reject = function (reason) {
        return new SyncPromise(function (_, reject) {
            reject(reason);
        });
    };
    /** JSDoc */
    SyncPromise.all = function (collection) {
        return new SyncPromise(function (resolve, reject) {
            if (!Array.isArray(collection)) {
                reject(new TypeError("Promise.all requires an array as input."));
                return;
            }
            if (collection.length === 0) {
                resolve([]);
                return;
            }
            var counter = collection.length;
            var resolvedCollection = [];
            collection.forEach(function (item, index) {
                SyncPromise.resolve(item)
                    .then(function (value) {
                    resolvedCollection[index] = value;
                    counter -= 1;
                    if (counter !== 0) {
                        return;
                    }
                    resolve(resolvedCollection);
                })
                    .then(null, reject);
            });
        });
    };
    /** JSDoc */
    SyncPromise.prototype.then = function (onfulfilled, onrejected) {
        var _this = this;
        return new SyncPromise(function (resolve, reject) {
            _this._attachHandler({
                done: false,
                onfulfilled: function (result) {
                    if (!onfulfilled) {
                        // TODO: ¯\_(ツ)_/¯
                        // TODO: FIXME
                        resolve(result);
                        return;
                    }
                    try {
                        resolve(onfulfilled(result));
                        return;
                    }
                    catch (e) {
                        reject(e);
                        return;
                    }
                },
                onrejected: function (reason) {
                    if (!onrejected) {
                        reject(reason);
                        return;
                    }
                    try {
                        resolve(onrejected(reason));
                        return;
                    }
                    catch (e) {
                        reject(e);
                        return;
                    }
                },
            });
        });
    };
    /** JSDoc */
    SyncPromise.prototype.catch = function (onrejected) {
        return this.then(function (val) { return val; }, onrejected);
    };
    /** JSDoc */
    SyncPromise.prototype.finally = function (onfinally) {
        var _this = this;
        return new SyncPromise(function (resolve, reject) {
            var val;
            var isRejected;
            return _this.then(function (value) {
                isRejected = false;
                val = value;
                if (onfinally) {
                    onfinally();
                }
            }, function (reason) {
                isRejected = true;
                val = reason;
                if (onfinally) {
                    onfinally();
                }
            }).then(function () {
                if (isRejected) {
                    reject(val);
                    return;
                }
                resolve(val);
            });
        });
    };
    /** JSDoc */
    SyncPromise.prototype.toString = function () {
        return '[object SyncPromise]';
    };
    return SyncPromise;
}());


// EXTERNAL MODULE: ./node_modules/@sentry/utils/esm/misc.js
var misc = __webpack_require__(2681);
;// CONCATENATED MODULE: ./node_modules/@sentry/hub/esm/scope.js


/**
 * Holds additional event information. {@link Scope.applyToEvent} will be
 * called by the client before an event will be sent.
 */
var Scope = /** @class */ (function () {
    function Scope() {
        /** Flag if notifiying is happening. */
        this._notifyingListeners = false;
        /** Callback for client to receive scope changes. */
        this._scopeListeners = [];
        /** Callback list that will be called after {@link applyToEvent}. */
        this._eventProcessors = [];
        /** Array of breadcrumbs. */
        this._breadcrumbs = [];
        /** User */
        this._user = {};
        /** Tags */
        this._tags = {};
        /** Extra */
        this._extra = {};
        /** Contexts */
        this._contexts = {};
    }
    /**
     * Inherit values from the parent scope.
     * @param scope to clone.
     */
    Scope.clone = function (scope) {
        var newScope = new Scope();
        if (scope) {
            newScope._breadcrumbs = tslib_es6_spread(scope._breadcrumbs);
            newScope._tags = __assign({}, scope._tags);
            newScope._extra = __assign({}, scope._extra);
            newScope._contexts = __assign({}, scope._contexts);
            newScope._user = scope._user;
            newScope._level = scope._level;
            newScope._span = scope._span;
            newScope._session = scope._session;
            newScope._transactionName = scope._transactionName;
            newScope._fingerprint = scope._fingerprint;
            newScope._eventProcessors = tslib_es6_spread(scope._eventProcessors);
        }
        return newScope;
    };
    /**
     * Add internal on change listener. Used for sub SDKs that need to store the scope.
     * @hidden
     */
    Scope.prototype.addScopeListener = function (callback) {
        this._scopeListeners.push(callback);
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.addEventProcessor = function (callback) {
        this._eventProcessors.push(callback);
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setUser = function (user) {
        this._user = user || {};
        if (this._session) {
            this._session.update({ user: user });
        }
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.getUser = function () {
        return this._user;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setTags = function (tags) {
        this._tags = __assign(__assign({}, this._tags), tags);
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setTag = function (key, value) {
        var _a;
        this._tags = __assign(__assign({}, this._tags), (_a = {}, _a[key] = value, _a));
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setExtras = function (extras) {
        this._extra = __assign(__assign({}, this._extra), extras);
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setExtra = function (key, extra) {
        var _a;
        this._extra = __assign(__assign({}, this._extra), (_a = {}, _a[key] = extra, _a));
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setFingerprint = function (fingerprint) {
        this._fingerprint = fingerprint;
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setLevel = function (level) {
        this._level = level;
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setTransactionName = function (name) {
        this._transactionName = name;
        this._notifyScopeListeners();
        return this;
    };
    /**
     * Can be removed in major version.
     * @deprecated in favor of {@link this.setTransactionName}
     */
    Scope.prototype.setTransaction = function (name) {
        return this.setTransactionName(name);
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setContext = function (key, context) {
        var _a;
        if (context === null) {
            // eslint-disable-next-line @typescript-eslint/no-dynamic-delete
            delete this._contexts[key];
        }
        else {
            this._contexts = __assign(__assign({}, this._contexts), (_a = {}, _a[key] = context, _a));
        }
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setSpan = function (span) {
        this._span = span;
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.getSpan = function () {
        return this._span;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.getTransaction = function () {
        var _a, _b, _c, _d;
        // often, this span will be a transaction, but it's not guaranteed to be
        var span = this.getSpan();
        // try it the new way first
        if ((_a = span) === null || _a === void 0 ? void 0 : _a.transaction) {
            return (_b = span) === null || _b === void 0 ? void 0 : _b.transaction;
        }
        // fallback to the old way (known bug: this only finds transactions with sampled = true)
        if ((_d = (_c = span) === null || _c === void 0 ? void 0 : _c.spanRecorder) === null || _d === void 0 ? void 0 : _d.spans[0]) {
            return span.spanRecorder.spans[0];
        }
        // neither way found a transaction
        return undefined;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.setSession = function (session) {
        if (!session) {
            delete this._session;
        }
        else {
            this._session = session;
        }
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.getSession = function () {
        return this._session;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.update = function (captureContext) {
        if (!captureContext) {
            return this;
        }
        if (typeof captureContext === 'function') {
            var updatedScope = captureContext(this);
            return updatedScope instanceof Scope ? updatedScope : this;
        }
        if (captureContext instanceof Scope) {
            this._tags = __assign(__assign({}, this._tags), captureContext._tags);
            this._extra = __assign(__assign({}, this._extra), captureContext._extra);
            this._contexts = __assign(__assign({}, this._contexts), captureContext._contexts);
            if (captureContext._user && Object.keys(captureContext._user).length) {
                this._user = captureContext._user;
            }
            if (captureContext._level) {
                this._level = captureContext._level;
            }
            if (captureContext._fingerprint) {
                this._fingerprint = captureContext._fingerprint;
            }
        }
        else if (isPlainObject(captureContext)) {
            // eslint-disable-next-line no-param-reassign
            captureContext = captureContext;
            this._tags = __assign(__assign({}, this._tags), captureContext.tags);
            this._extra = __assign(__assign({}, this._extra), captureContext.extra);
            this._contexts = __assign(__assign({}, this._contexts), captureContext.contexts);
            if (captureContext.user) {
                this._user = captureContext.user;
            }
            if (captureContext.level) {
                this._level = captureContext.level;
            }
            if (captureContext.fingerprint) {
                this._fingerprint = captureContext.fingerprint;
            }
        }
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.clear = function () {
        this._breadcrumbs = [];
        this._tags = {};
        this._extra = {};
        this._user = {};
        this._contexts = {};
        this._level = undefined;
        this._transactionName = undefined;
        this._fingerprint = undefined;
        this._span = undefined;
        this._session = undefined;
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.addBreadcrumb = function (breadcrumb, maxBreadcrumbs) {
        var mergedBreadcrumb = __assign({ timestamp: (0,time/* dateTimestampInSeconds */.yW)() }, breadcrumb);
        this._breadcrumbs =
            maxBreadcrumbs !== undefined && maxBreadcrumbs >= 0
                ? tslib_es6_spread(this._breadcrumbs, [mergedBreadcrumb]).slice(-maxBreadcrumbs)
                : tslib_es6_spread(this._breadcrumbs, [mergedBreadcrumb]);
        this._notifyScopeListeners();
        return this;
    };
    /**
     * @inheritDoc
     */
    Scope.prototype.clearBreadcrumbs = function () {
        this._breadcrumbs = [];
        this._notifyScopeListeners();
        return this;
    };
    /**
     * Applies the current context and fingerprint to the event.
     * Note that breadcrumbs will be added by the client.
     * Also if the event has already breadcrumbs on it, we do not merge them.
     * @param event Event
     * @param hint May contain additional informartion about the original exception.
     * @hidden
     */
    Scope.prototype.applyToEvent = function (event, hint) {
        if (this._extra && Object.keys(this._extra).length) {
            event.extra = __assign(__assign({}, this._extra), event.extra);
        }
        if (this._tags && Object.keys(this._tags).length) {
            event.tags = __assign(__assign({}, this._tags), event.tags);
        }
        if (this._user && Object.keys(this._user).length) {
            event.user = __assign(__assign({}, this._user), event.user);
        }
        if (this._contexts && Object.keys(this._contexts).length) {
            event.contexts = __assign(__assign({}, this._contexts), event.contexts);
        }
        if (this._level) {
            event.level = this._level;
        }
        if (this._transactionName) {
            event.transaction = this._transactionName;
        }
        // We want to set the trace context for normal events only if there isn't already
        // a trace context on the event. There is a product feature in place where we link
        // errors with transaction and it relys on that.
        if (this._span) {
            event.contexts = __assign({ trace: this._span.getTraceContext() }, event.contexts);
        }
        this._applyFingerprint(event);
        event.breadcrumbs = tslib_es6_spread((event.breadcrumbs || []), this._breadcrumbs);
        event.breadcrumbs = event.breadcrumbs.length > 0 ? event.breadcrumbs : undefined;
        return this._notifyEventProcessors(tslib_es6_spread(getGlobalEventProcessors(), this._eventProcessors), event, hint);
    };
    /**
     * This will be called after {@link applyToEvent} is finished.
     */
    Scope.prototype._notifyEventProcessors = function (processors, event, hint, index) {
        var _this = this;
        if (index === void 0) { index = 0; }
        return new SyncPromise(function (resolve, reject) {
            var processor = processors[index];
            if (event === null || typeof processor !== 'function') {
                resolve(event);
            }
            else {
                var result = processor(__assign({}, event), hint);
                if (isThenable(result)) {
                    result
                        .then(function (final) { return _this._notifyEventProcessors(processors, final, hint, index + 1).then(resolve); })
                        .then(null, reject);
                }
                else {
                    _this._notifyEventProcessors(processors, result, hint, index + 1)
                        .then(resolve)
                        .then(null, reject);
                }
            }
        });
    };
    /**
     * This will be called on every set call.
     */
    Scope.prototype._notifyScopeListeners = function () {
        var _this = this;
        if (!this._notifyingListeners) {
            this._notifyingListeners = true;
            setTimeout(function () {
                _this._scopeListeners.forEach(function (callback) {
                    callback(_this);
                });
                _this._notifyingListeners = false;
            });
        }
    };
    /**
     * Applies fingerprint from the scope to the event if there's one,
     * uses message if there's one instead or get rid of empty fingerprint
     */
    Scope.prototype._applyFingerprint = function (event) {
        // Make sure it's an array first and we actually have something in place
        event.fingerprint = event.fingerprint
            ? Array.isArray(event.fingerprint)
                ? event.fingerprint
                : [event.fingerprint]
            : [];
        // If we have something on the scope, then merge it with event
        if (this._fingerprint) {
            event.fingerprint = event.fingerprint.concat(this._fingerprint);
        }
        // If we have no data at all, remove empty array default
        if (event.fingerprint && !event.fingerprint.length) {
            delete event.fingerprint;
        }
    };
    return Scope;
}());

/**
 * Retruns the global event processors.
 */
function getGlobalEventProcessors() {
    var global = (0,misc/* getGlobalObject */.Rf)();
    global.__SENTRY__ = global.__SENTRY__ || {};
    global.__SENTRY__.globalEventProcessors = global.__SENTRY__.globalEventProcessors || [];
    return global.__SENTRY__.globalEventProcessors;
}
/**
 * Add a EventProcessor to be kept globally.
 * @param callback EventProcessor to add
 */
function addGlobalEventProcessor(callback) {
    getGlobalEventProcessors().push(callback);
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/logger.js
/* eslint-disable @typescript-eslint/no-explicit-any */

// TODO: Implement different loggers for different environments
var global = (0,misc/* getGlobalObject */.Rf)();
/** Prefix for logging strings */
var PREFIX = 'Sentry Logger ';
/** JSDoc */
var Logger = /** @class */ (function () {
    /** JSDoc */
    function Logger() {
        this._enabled = false;
    }
    /** JSDoc */
    Logger.prototype.disable = function () {
        this._enabled = false;
    };
    /** JSDoc */
    Logger.prototype.enable = function () {
        this._enabled = true;
    };
    /** JSDoc */
    Logger.prototype.log = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        if (!this._enabled) {
            return;
        }
        (0,misc/* consoleSandbox */.Cf)(function () {
            global.console.log(PREFIX + "[Log]: " + args.join(' '));
        });
    };
    /** JSDoc */
    Logger.prototype.warn = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        if (!this._enabled) {
            return;
        }
        (0,misc/* consoleSandbox */.Cf)(function () {
            global.console.warn(PREFIX + "[Warn]: " + args.join(' '));
        });
    };
    /** JSDoc */
    Logger.prototype.error = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        if (!this._enabled) {
            return;
        }
        (0,misc/* consoleSandbox */.Cf)(function () {
            global.console.error(PREFIX + "[Error]: " + args.join(' '));
        });
    };
    return Logger;
}());
// Ensure we only have a single logger instance, even if multiple versions of @sentry/utils are being used
global.__SENTRY__ = global.__SENTRY__ || {};
var logger = global.__SENTRY__.logger || (global.__SENTRY__.logger = new Logger());


// EXTERNAL MODULE: ./node_modules/@sentry/utils/esm/node.js
var node = __webpack_require__(8612);
;// CONCATENATED MODULE: ./node_modules/@sentry/types/esm/session.js
/**
 * Session Status
 */
var SessionStatus;
(function (SessionStatus) {
    /** JSDoc */
    SessionStatus["Ok"] = "ok";
    /** JSDoc */
    SessionStatus["Exited"] = "exited";
    /** JSDoc */
    SessionStatus["Crashed"] = "crashed";
    /** JSDoc */
    SessionStatus["Abnormal"] = "abnormal";
})(SessionStatus || (SessionStatus = {}));

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/browser.js

/**
 * Given a child DOM element, returns a query-selector statement describing that
 * and its ancestors
 * e.g. [HTMLElement] => body > div > input#foo.btn[name=baz]
 * @returns generated DOM path
 */
function htmlTreeAsString(elem) {
    // try/catch both:
    // - accessing event.target (see getsentry/raven-js#838, #768)
    // - `htmlTreeAsString` because it's complex, and just accessing the DOM incorrectly
    // - can throw an exception in some circumstances.
    try {
        var currentElem = elem;
        var MAX_TRAVERSE_HEIGHT = 5;
        var MAX_OUTPUT_LEN = 80;
        var out = [];
        var height = 0;
        var len = 0;
        var separator = ' > ';
        var sepLength = separator.length;
        var nextStr = void 0;
        // eslint-disable-next-line no-plusplus
        while (currentElem && height++ < MAX_TRAVERSE_HEIGHT) {
            nextStr = _htmlElementAsString(currentElem);
            // bail out if
            // - nextStr is the 'html' element
            // - the length of the string that would be created exceeds MAX_OUTPUT_LEN
            //   (ignore this limit if we are on the first iteration)
            if (nextStr === 'html' || (height > 1 && len + out.length * sepLength + nextStr.length >= MAX_OUTPUT_LEN)) {
                break;
            }
            out.push(nextStr);
            len += nextStr.length;
            currentElem = currentElem.parentNode;
        }
        return out.reverse().join(separator);
    }
    catch (_oO) {
        return '<unknown>';
    }
}
/**
 * Returns a simple, query-selector representation of a DOM element
 * e.g. [HTMLElement] => input#foo.btn[name=baz]
 * @returns generated DOM path
 */
function _htmlElementAsString(el) {
    var elem = el;
    var out = [];
    var className;
    var classes;
    var key;
    var attr;
    var i;
    if (!elem || !elem.tagName) {
        return '';
    }
    out.push(elem.tagName.toLowerCase());
    if (elem.id) {
        out.push("#" + elem.id);
    }
    // eslint-disable-next-line prefer-const
    className = elem.className;
    if (className && isString(className)) {
        classes = className.split(/\s+/);
        for (i = 0; i < classes.length; i++) {
            out.push("." + classes[i]);
        }
    }
    var allowedAttrs = ['type', 'name', 'title', 'alt'];
    for (i = 0; i < allowedAttrs.length; i++) {
        key = allowedAttrs[i];
        attr = elem.getAttribute(key);
        if (attr) {
            out.push("[" + key + "=\"" + attr + "\"]");
        }
    }
    return out.join('');
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/memo.js
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable @typescript-eslint/no-explicit-any */
/* eslint-disable @typescript-eslint/explicit-module-boundary-types */
/**
 * Memo class used for decycle json objects. Uses WeakSet if available otherwise array.
 */
var Memo = /** @class */ (function () {
    function Memo() {
        this._hasWeakSet = typeof WeakSet === 'function';
        this._inner = this._hasWeakSet ? new WeakSet() : [];
    }
    /**
     * Sets obj to remember.
     * @param obj Object to remember
     */
    Memo.prototype.memoize = function (obj) {
        if (this._hasWeakSet) {
            if (this._inner.has(obj)) {
                return true;
            }
            this._inner.add(obj);
            return false;
        }
        // eslint-disable-next-line @typescript-eslint/prefer-for-of
        for (var i = 0; i < this._inner.length; i++) {
            var value = this._inner[i];
            if (value === obj) {
                return true;
            }
        }
        this._inner.push(obj);
        return false;
    };
    /**
     * Removes object from internal storage.
     * @param obj Object to forget
     */
    Memo.prototype.unmemoize = function (obj) {
        if (this._hasWeakSet) {
            this._inner.delete(obj);
        }
        else {
            for (var i = 0; i < this._inner.length; i++) {
                if (this._inner[i] === obj) {
                    this._inner.splice(i, 1);
                    break;
                }
            }
        }
    };
    return Memo;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/stacktrace.js
var defaultFunctionName = '<anonymous>';
/**
 * Safely extract function name from itself
 */
function getFunctionName(fn) {
    try {
        if (!fn || typeof fn !== 'function') {
            return defaultFunctionName;
        }
        return fn.name || defaultFunctionName;
    }
    catch (e) {
        // Just accessing custom props in some Selenium environments
        // can cause a "Permission denied" exception (see raven-js#495).
        return defaultFunctionName;
    }
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/string.js

/**
 * Truncates given string to the maximum characters count
 *
 * @param str An object that contains serializable values
 * @param max Maximum number of characters in truncated string
 * @returns string Encoded
 */
function truncate(str, max) {
    if (max === void 0) { max = 0; }
    if (typeof str !== 'string' || max === 0) {
        return str;
    }
    return str.length <= max ? str : str.substr(0, max) + "...";
}
/**
 * This is basically just `trim_line` from
 * https://github.com/getsentry/sentry/blob/master/src/sentry/lang/javascript/processor.py#L67
 *
 * @param str An object that contains serializable values
 * @param max Maximum number of characters in truncated string
 * @returns string Encoded
 */
function snipLine(line, colno) {
    var newLine = line;
    var ll = newLine.length;
    if (ll <= 150) {
        return newLine;
    }
    if (colno > ll) {
        // eslint-disable-next-line no-param-reassign
        colno = ll;
    }
    var start = Math.max(colno - 60, 0);
    if (start < 5) {
        start = 0;
    }
    var end = Math.min(start + 140, ll);
    if (end > ll - 5) {
        end = ll;
    }
    if (end === ll) {
        start = Math.max(end - 140, 0);
    }
    newLine = newLine.slice(start, end);
    if (start > 0) {
        newLine = "'{snip} " + newLine;
    }
    if (end < ll) {
        newLine += ' {snip}';
    }
    return newLine;
}
/**
 * Join values in array
 * @param input array of values to be joined together
 * @param delimiter string to be placed in-between values
 * @returns Joined values
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function safeJoin(input, delimiter) {
    if (!Array.isArray(input)) {
        return '';
    }
    var output = [];
    // eslint-disable-next-line @typescript-eslint/prefer-for-of
    for (var i = 0; i < input.length; i++) {
        var value = input[i];
        try {
            output.push(String(value));
        }
        catch (e) {
            output.push('[value cannot be serialized]');
        }
    }
    return output.join(delimiter);
}
/**
 * Checks if the value matches a regex or includes the string
 * @param value The string value to be checked against
 * @param pattern Either a regex or a string that must be contained in value
 */
function isMatchingPattern(value, pattern) {
    if (!isString(value)) {
        return false;
    }
    if (isRegExp(pattern)) {
        return pattern.test(value);
    }
    if (typeof pattern === 'string') {
        return value.indexOf(pattern) !== -1;
    }
    return false;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/object.js






/**
 * Wrap a given object method with a higher-order function
 *
 * @param source An object that contains a method to be wrapped.
 * @param name A name of method to be wrapped.
 * @param replacement A function that should be used to wrap a given method.
 * @returns void
 */
function fill(source, name, replacement) {
    if (!(name in source)) {
        return;
    }
    var original = source[name];
    var wrapped = replacement(original);
    // Make sure it's a function first, as we need to attach an empty prototype for `defineProperties` to work
    // otherwise it'll throw "TypeError: Object.defineProperties called on non-object"
    if (typeof wrapped === 'function') {
        try {
            wrapped.prototype = wrapped.prototype || {};
            Object.defineProperties(wrapped, {
                __sentry_original__: {
                    enumerable: false,
                    value: original,
                },
            });
        }
        catch (_Oo) {
            // This can throw if multiple fill happens on a global object like XMLHttpRequest
            // Fixes https://github.com/getsentry/sentry-javascript/issues/2043
        }
    }
    source[name] = wrapped;
}
/**
 * Encodes given object into url-friendly format
 *
 * @param object An object that contains serializable values
 * @returns string Encoded
 */
function urlEncode(object) {
    return Object.keys(object)
        .map(function (key) { return encodeURIComponent(key) + "=" + encodeURIComponent(object[key]); })
        .join('&');
}
/**
 * Transforms any object into an object literal with all it's attributes
 * attached to it.
 *
 * @param value Initial source that we have to transform in order to be usable by the serializer
 */
function getWalkSource(value) {
    if (isError(value)) {
        var error = value;
        var err = {
            message: error.message,
            name: error.name,
            stack: error.stack,
        };
        for (var i in error) {
            if (Object.prototype.hasOwnProperty.call(error, i)) {
                err[i] = error[i];
            }
        }
        return err;
    }
    if (isEvent(value)) {
        var event_1 = value;
        var source = {};
        source.type = event_1.type;
        // Accessing event.target can throw (see getsentry/raven-js#838, #768)
        try {
            source.target = isElement(event_1.target)
                ? htmlTreeAsString(event_1.target)
                : Object.prototype.toString.call(event_1.target);
        }
        catch (_oO) {
            source.target = '<unknown>';
        }
        try {
            source.currentTarget = isElement(event_1.currentTarget)
                ? htmlTreeAsString(event_1.currentTarget)
                : Object.prototype.toString.call(event_1.currentTarget);
        }
        catch (_oO) {
            source.currentTarget = '<unknown>';
        }
        if (typeof CustomEvent !== 'undefined' && isInstanceOf(value, CustomEvent)) {
            source.detail = event_1.detail;
        }
        for (var i in event_1) {
            if (Object.prototype.hasOwnProperty.call(event_1, i)) {
                source[i] = event_1;
            }
        }
        return source;
    }
    return value;
}
/** Calculates bytes size of input string */
function utf8Length(value) {
    // eslint-disable-next-line no-bitwise
    return ~-encodeURI(value).split(/%..|./).length;
}
/** Calculates bytes size of input object */
function jsonSize(value) {
    return utf8Length(JSON.stringify(value));
}
/** JSDoc */
function normalizeToSize(object,
// Default Node.js REPL depth
depth,
// 100kB, as 200kB is max payload size, so half sounds reasonable
maxSize) {
    if (depth === void 0) { depth = 3; }
    if (maxSize === void 0) { maxSize = 100 * 1024; }
    var serialized = normalize(object, depth);
    if (jsonSize(serialized) > maxSize) {
        return normalizeToSize(object, depth - 1, maxSize);
    }
    return serialized;
}
/** Transforms any input value into a string form, either primitive value or a type of the input */
function serializeValue(value) {
    var type = Object.prototype.toString.call(value);
    // Node.js REPL notation
    if (typeof value === 'string') {
        return value;
    }
    if (type === '[object Object]') {
        return '[Object]';
    }
    if (type === '[object Array]') {
        return '[Array]';
    }
    var normalized = normalizeValue(value);
    return isPrimitive(normalized) ? normalized : type;
}
/**
 * normalizeValue()
 *
 * Takes unserializable input and make it serializable friendly
 *
 * - translates undefined/NaN values to "[undefined]"/"[NaN]" respectively,
 * - serializes Error objects
 * - filter global objects
 */
function normalizeValue(value, key) {
    if (key === 'domain' && value && typeof value === 'object' && value._events) {
        return '[Domain]';
    }
    if (key === 'domainEmitter') {
        return '[DomainEmitter]';
    }
    if (typeof __webpack_require__.g !== 'undefined' && value === __webpack_require__.g) {
        return '[Global]';
    }
    if (typeof window !== 'undefined' && value === window) {
        return '[Window]';
    }
    if (typeof document !== 'undefined' && value === document) {
        return '[Document]';
    }
    // React's SyntheticEvent thingy
    if (isSyntheticEvent(value)) {
        return '[SyntheticEvent]';
    }
    if (typeof value === 'number' && value !== value) {
        return '[NaN]';
    }
    if (value === void 0) {
        return '[undefined]';
    }
    if (typeof value === 'function') {
        return "[Function: " + getFunctionName(value) + "]";
    }
    return value;
}
/**
 * Walks an object to perform a normalization on it
 *
 * @param key of object that's walked in current iteration
 * @param value object to be walked
 * @param depth Optional number indicating how deep should walking be performed
 * @param memo Optional Memo class handling decycling
 */
// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
function walk(key, value, depth, memo) {
    if (depth === void 0) { depth = +Infinity; }
    if (memo === void 0) { memo = new Memo(); }
    // If we reach the maximum depth, serialize whatever has left
    if (depth === 0) {
        return serializeValue(value);
    }
    /* eslint-disable @typescript-eslint/no-unsafe-member-access */
    // If value implements `toJSON` method, call it and return early
    if (value !== null && value !== undefined && typeof value.toJSON === 'function') {
        return value.toJSON();
    }
    /* eslint-enable @typescript-eslint/no-unsafe-member-access */
    // If normalized value is a primitive, there are no branches left to walk, so we can just bail out, as theres no point in going down that branch any further
    var normalized = normalizeValue(value, key);
    if (isPrimitive(normalized)) {
        return normalized;
    }
    // Create source that we will use for next itterations, either objectified error object (Error type with extracted keys:value pairs) or the input itself
    var source = getWalkSource(value);
    // Create an accumulator that will act as a parent for all future itterations of that branch
    var acc = Array.isArray(value) ? [] : {};
    // If we already walked that branch, bail out, as it's circular reference
    if (memo.memoize(value)) {
        return '[Circular ~]';
    }
    // Walk all keys of the source
    for (var innerKey in source) {
        // Avoid iterating over fields in the prototype if they've somehow been exposed to enumeration.
        if (!Object.prototype.hasOwnProperty.call(source, innerKey)) {
            continue;
        }
        // Recursively walk through all the child nodes
        acc[innerKey] = walk(innerKey, source[innerKey], depth - 1, memo);
    }
    // Once walked through all the branches, remove the parent from memo storage
    memo.unmemoize(value);
    // Return accumulated values
    return acc;
}
/**
 * normalize()
 *
 * - Creates a copy to prevent original input mutation
 * - Skip non-enumerablers
 * - Calls `toJSON` if implemented
 * - Removes circular references
 * - Translates non-serializeable values (undefined/NaN/Functions) to serializable format
 * - Translates known global objects/Classes to a string representations
 * - Takes care of Error objects serialization
 * - Optionally limit depth of final output
 */
// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
function normalize(input, depth) {
    try {
        return JSON.parse(JSON.stringify(input, function (key, value) { return walk(key, value, depth); }));
    }
    catch (_oO) {
        return '**non-serializable**';
    }
}
/**
 * Given any captured exception, extract its keys and create a sorted
 * and truncated list that will be used inside the event message.
 * eg. `Non-error exception captured with keys: foo, bar, baz`
 */
// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
function extractExceptionKeysForMessage(exception, maxLength) {
    if (maxLength === void 0) { maxLength = 40; }
    var keys = Object.keys(getWalkSource(exception));
    keys.sort();
    if (!keys.length) {
        return '[object has no keys]';
    }
    if (keys[0].length >= maxLength) {
        return truncate(keys[0], maxLength);
    }
    for (var includedKeys = keys.length; includedKeys > 0; includedKeys--) {
        var serialized = keys.slice(0, includedKeys).join(', ');
        if (serialized.length > maxLength) {
            continue;
        }
        if (includedKeys === keys.length) {
            return serialized;
        }
        return truncate(serialized, maxLength);
    }
    return '';
}
/**
 * Given any object, return the new object with removed keys that value was `undefined`.
 * Works recursively on objects and arrays.
 */
function dropUndefinedKeys(val) {
    var e_1, _a;
    if (isPlainObject(val)) {
        var obj = val;
        var rv = {};
        try {
            for (var _b = __values(Object.keys(obj)), _c = _b.next(); !_c.done; _c = _b.next()) {
                var key = _c.value;
                if (typeof obj[key] !== 'undefined') {
                    rv[key] = dropUndefinedKeys(obj[key]);
                }
            }
        }
        catch (e_1_1) { e_1 = { error: e_1_1 }; }
        finally {
            try {
                if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
            }
            finally { if (e_1) throw e_1.error; }
        }
        return rv;
    }
    if (Array.isArray(val)) {
        return val.map(dropUndefinedKeys);
    }
    return val;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/hub/esm/session.js


/**
 * @inheritdoc
 */
var Session = /** @class */ (function () {
    function Session(context) {
        this.errors = 0;
        this.sid = (0,misc/* uuid4 */.DM)();
        this.timestamp = Date.now();
        this.started = Date.now();
        this.duration = 0;
        this.status = SessionStatus.Ok;
        if (context) {
            this.update(context);
        }
    }
    /** JSDoc */
    // eslint-disable-next-line complexity
    Session.prototype.update = function (context) {
        if (context === void 0) { context = {}; }
        if (context.user) {
            if (context.user.ip_address) {
                this.ipAddress = context.user.ip_address;
            }
            if (!context.did) {
                this.did = context.user.id || context.user.email || context.user.username;
            }
        }
        this.timestamp = context.timestamp || Date.now();
        if (context.sid) {
            // Good enough uuid validation. — Kamil
            this.sid = context.sid.length === 32 ? context.sid : (0,misc/* uuid4 */.DM)();
        }
        if (context.did) {
            this.did = "" + context.did;
        }
        if (typeof context.started === 'number') {
            this.started = context.started;
        }
        if (typeof context.duration === 'number') {
            this.duration = context.duration;
        }
        else {
            this.duration = this.timestamp - this.started;
        }
        if (context.release) {
            this.release = context.release;
        }
        if (context.environment) {
            this.environment = context.environment;
        }
        if (context.ipAddress) {
            this.ipAddress = context.ipAddress;
        }
        if (context.userAgent) {
            this.userAgent = context.userAgent;
        }
        if (typeof context.errors === 'number') {
            this.errors = context.errors;
        }
        if (context.status) {
            this.status = context.status;
        }
    };
    /** JSDoc */
    Session.prototype.close = function (status) {
        if (status) {
            this.update({ status: status });
        }
        else if (this.status === SessionStatus.Ok) {
            this.update({ status: SessionStatus.Exited });
        }
        else {
            this.update();
        }
    };
    /** JSDoc */
    Session.prototype.toJSON = function () {
        return dropUndefinedKeys({
            sid: "" + this.sid,
            init: true,
            started: new Date(this.started).toISOString(),
            timestamp: new Date(this.timestamp).toISOString(),
            status: this.status,
            errors: this.errors,
            did: typeof this.did === 'number' || typeof this.did === 'string' ? "" + this.did : undefined,
            duration: this.duration,
            attrs: dropUndefinedKeys({
                release: this.release,
                environment: this.environment,
                ip_address: this.ipAddress,
                user_agent: this.userAgent,
            }),
        });
    };
    return Session;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/hub/esm/hub.js




/**
 * API compatibility version of this hub.
 *
 * WARNING: This number should only be increased when the global interface
 * changes and new methods are introduced.
 *
 * @hidden
 */
var API_VERSION = 3;
/**
 * Default maximum number of breadcrumbs added to an event. Can be overwritten
 * with {@link Options.maxBreadcrumbs}.
 */
var DEFAULT_BREADCRUMBS = 100;
/**
 * Absolute maximum number of breadcrumbs added to an event. The
 * `maxBreadcrumbs` option cannot be higher than this value.
 */
var MAX_BREADCRUMBS = 100;
/**
 * @inheritDoc
 */
var Hub = /** @class */ (function () {
    /**
     * Creates a new instance of the hub, will push one {@link Layer} into the
     * internal stack on creation.
     *
     * @param client bound to the hub.
     * @param scope bound to the hub.
     * @param version number, higher number means higher priority.
     */
    function Hub(client, scope, _version) {
        if (scope === void 0) { scope = new Scope(); }
        if (_version === void 0) { _version = API_VERSION; }
        this._version = _version;
        /** Is a {@link Layer}[] containing the client and scope */
        this._stack = [{}];
        this.getStackTop().scope = scope;
        this.bindClient(client);
    }
    /**
     * @inheritDoc
     */
    Hub.prototype.isOlderThan = function (version) {
        return this._version < version;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.bindClient = function (client) {
        var top = this.getStackTop();
        top.client = client;
        if (client && client.setupIntegrations) {
            client.setupIntegrations();
        }
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.pushScope = function () {
        // We want to clone the content of prev scope
        var scope = Scope.clone(this.getScope());
        this.getStack().push({
            client: this.getClient(),
            scope: scope,
        });
        return scope;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.popScope = function () {
        if (this.getStack().length <= 1)
            return false;
        return !!this.getStack().pop();
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.withScope = function (callback) {
        var scope = this.pushScope();
        try {
            callback(scope);
        }
        finally {
            this.popScope();
        }
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.getClient = function () {
        return this.getStackTop().client;
    };
    /** Returns the scope of the top stack. */
    Hub.prototype.getScope = function () {
        return this.getStackTop().scope;
    };
    /** Returns the scope stack for domains or the process. */
    Hub.prototype.getStack = function () {
        return this._stack;
    };
    /** Returns the topmost scope layer in the order domain > local > process. */
    Hub.prototype.getStackTop = function () {
        return this._stack[this._stack.length - 1];
    };
    /**
     * @inheritDoc
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    Hub.prototype.captureException = function (exception, hint) {
        var eventId = (this._lastEventId = (0,misc/* uuid4 */.DM)());
        var finalHint = hint;
        // If there's no explicit hint provided, mimick the same thing that would happen
        // in the minimal itself to create a consistent behavior.
        // We don't do this in the client, as it's the lowest level API, and doing this,
        // would prevent user from having full control over direct calls.
        if (!hint) {
            var syntheticException = void 0;
            try {
                throw new Error('Sentry syntheticException');
            }
            catch (exception) {
                syntheticException = exception;
            }
            finalHint = {
                originalException: exception,
                syntheticException: syntheticException,
            };
        }
        this._invokeClient('captureException', exception, __assign(__assign({}, finalHint), { event_id: eventId }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.captureMessage = function (message, level, hint) {
        var eventId = (this._lastEventId = (0,misc/* uuid4 */.DM)());
        var finalHint = hint;
        // If there's no explicit hint provided, mimick the same thing that would happen
        // in the minimal itself to create a consistent behavior.
        // We don't do this in the client, as it's the lowest level API, and doing this,
        // would prevent user from having full control over direct calls.
        if (!hint) {
            var syntheticException = void 0;
            try {
                throw new Error(message);
            }
            catch (exception) {
                syntheticException = exception;
            }
            finalHint = {
                originalException: message,
                syntheticException: syntheticException,
            };
        }
        this._invokeClient('captureMessage', message, level, __assign(__assign({}, finalHint), { event_id: eventId }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.captureEvent = function (event, hint) {
        var eventId = (this._lastEventId = (0,misc/* uuid4 */.DM)());
        this._invokeClient('captureEvent', event, __assign(__assign({}, hint), { event_id: eventId }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.lastEventId = function () {
        return this._lastEventId;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.addBreadcrumb = function (breadcrumb, hint) {
        var _a = this.getStackTop(), scope = _a.scope, client = _a.client;
        if (!scope || !client)
            return;
        // eslint-disable-next-line @typescript-eslint/unbound-method
        var _b = (client.getOptions && client.getOptions()) || {}, _c = _b.beforeBreadcrumb, beforeBreadcrumb = _c === void 0 ? null : _c, _d = _b.maxBreadcrumbs, maxBreadcrumbs = _d === void 0 ? DEFAULT_BREADCRUMBS : _d;
        if (maxBreadcrumbs <= 0)
            return;
        var timestamp = (0,time/* dateTimestampInSeconds */.yW)();
        var mergedBreadcrumb = __assign({ timestamp: timestamp }, breadcrumb);
        var finalBreadcrumb = beforeBreadcrumb
            ? (0,misc/* consoleSandbox */.Cf)(function () { return beforeBreadcrumb(mergedBreadcrumb, hint); })
            : mergedBreadcrumb;
        if (finalBreadcrumb === null)
            return;
        scope.addBreadcrumb(finalBreadcrumb, Math.min(maxBreadcrumbs, MAX_BREADCRUMBS));
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.setUser = function (user) {
        var scope = this.getScope();
        if (scope)
            scope.setUser(user);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.setTags = function (tags) {
        var scope = this.getScope();
        if (scope)
            scope.setTags(tags);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.setExtras = function (extras) {
        var scope = this.getScope();
        if (scope)
            scope.setExtras(extras);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.setTag = function (key, value) {
        var scope = this.getScope();
        if (scope)
            scope.setTag(key, value);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.setExtra = function (key, extra) {
        var scope = this.getScope();
        if (scope)
            scope.setExtra(key, extra);
    };
    /**
     * @inheritDoc
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Hub.prototype.setContext = function (name, context) {
        var scope = this.getScope();
        if (scope)
            scope.setContext(name, context);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.configureScope = function (callback) {
        var _a = this.getStackTop(), scope = _a.scope, client = _a.client;
        if (scope && client) {
            callback(scope);
        }
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.run = function (callback) {
        var oldHub = makeMain(this);
        try {
            callback(this);
        }
        finally {
            makeMain(oldHub);
        }
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.getIntegration = function (integration) {
        var client = this.getClient();
        if (!client)
            return null;
        try {
            return client.getIntegration(integration);
        }
        catch (_oO) {
            logger.warn("Cannot retrieve integration " + integration.id + " from the current Hub");
            return null;
        }
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.startSpan = function (context) {
        return this._callExtensionMethod('startSpan', context);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.startTransaction = function (context, customSamplingContext) {
        return this._callExtensionMethod('startTransaction', context, customSamplingContext);
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.traceHeaders = function () {
        return this._callExtensionMethod('traceHeaders');
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.startSession = function (context) {
        // End existing session if there's one
        this.endSession();
        var _a = this.getStackTop(), scope = _a.scope, client = _a.client;
        var _b = (client && client.getOptions()) || {}, release = _b.release, environment = _b.environment;
        var session = new Session(__assign(__assign({ release: release,
            environment: environment }, (scope && { user: scope.getUser() })), context));
        if (scope) {
            scope.setSession(session);
        }
        return session;
    };
    /**
     * @inheritDoc
     */
    Hub.prototype.endSession = function () {
        var _a = this.getStackTop(), scope = _a.scope, client = _a.client;
        if (!scope)
            return;
        var session = scope.getSession();
        if (session) {
            session.close();
            if (client && client.captureSession) {
                client.captureSession(session);
            }
            scope.setSession();
        }
    };
    /**
     * Internal helper function to call a method on the top client if it exists.
     *
     * @param method The method to call on the client.
     * @param args Arguments to pass to the client function.
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Hub.prototype._invokeClient = function (method) {
        var _a;
        var args = [];
        for (var _i = 1; _i < arguments.length; _i++) {
            args[_i - 1] = arguments[_i];
        }
        var _b = this.getStackTop(), scope = _b.scope, client = _b.client;
        if (client && client[method]) {
            // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access, @typescript-eslint/no-explicit-any
            (_a = client)[method].apply(_a, tslib_es6_spread(args, [scope]));
        }
    };
    /**
     * Calls global extension method and binding current instance to the function call
     */
    // @ts-ignore Function lacks ending return statement and return type does not include 'undefined'. ts(2366)
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Hub.prototype._callExtensionMethod = function (method) {
        var args = [];
        for (var _i = 1; _i < arguments.length; _i++) {
            args[_i - 1] = arguments[_i];
        }
        var carrier = getMainCarrier();
        var sentry = carrier.__SENTRY__;
        if (sentry && sentry.extensions && typeof sentry.extensions[method] === 'function') {
            return sentry.extensions[method].apply(this, args);
        }
        logger.warn("Extension method " + method + " couldn't be found, doing nothing.");
    };
    return Hub;
}());

/** Returns the global shim registry. */
function getMainCarrier() {
    var carrier = (0,misc/* getGlobalObject */.Rf)();
    carrier.__SENTRY__ = carrier.__SENTRY__ || {
        extensions: {},
        hub: undefined,
    };
    return carrier;
}
/**
 * Replaces the current main hub with the passed one on the global object
 *
 * @returns The old replaced hub
 */
function makeMain(hub) {
    var registry = getMainCarrier();
    var oldHub = getHubFromCarrier(registry);
    setHubOnCarrier(registry, hub);
    return oldHub;
}
/**
 * Returns the default hub instance.
 *
 * If a hub is already registered in the global carrier but this module
 * contains a more recent version, it replaces the registered version.
 * Otherwise, the currently registered hub will be returned.
 */
function getCurrentHub() {
    // Get main carrier (global for every environment)
    var registry = getMainCarrier();
    // If there's no hub, or its an old API, assign a new one
    if (!hasHubOnCarrier(registry) || getHubFromCarrier(registry).isOlderThan(API_VERSION)) {
        setHubOnCarrier(registry, new Hub());
    }
    // Prefer domains over global if they are there (applicable only to Node environment)
    if ((0,node/* isNodeEnv */.KV)()) {
        return getHubFromActiveDomain(registry);
    }
    // Return hub that lives on a global object
    return getHubFromCarrier(registry);
}
/**
 * Returns the active domain, if one exists
 *
 * @returns The domain, or undefined if there is no active domain
 */
function getActiveDomain() {
    var sentry = getMainCarrier().__SENTRY__;
    return sentry && sentry.extensions && sentry.extensions.domain && sentry.extensions.domain.active;
}
/**
 * Try to read the hub from an active domain, and fallback to the registry if one doesn't exist
 * @returns discovered hub
 */
function getHubFromActiveDomain(registry) {
    try {
        var activeDomain = getActiveDomain();
        // If there's no active domain, just return global hub
        if (!activeDomain) {
            return getHubFromCarrier(registry);
        }
        // If there's no hub on current domain, or it's an old API, assign a new one
        if (!hasHubOnCarrier(activeDomain) || getHubFromCarrier(activeDomain).isOlderThan(API_VERSION)) {
            var registryHubTopStack = getHubFromCarrier(registry).getStackTop();
            setHubOnCarrier(activeDomain, new Hub(registryHubTopStack.client, Scope.clone(registryHubTopStack.scope)));
        }
        // Return hub that lives on a domain
        return getHubFromCarrier(activeDomain);
    }
    catch (_Oo) {
        // Return hub that lives on a global object
        return getHubFromCarrier(registry);
    }
}
/**
 * This will tell whether a carrier has a hub on it or not
 * @param carrier object
 */
function hasHubOnCarrier(carrier) {
    return !!(carrier && carrier.__SENTRY__ && carrier.__SENTRY__.hub);
}
/**
 * This will create a new {@link Hub} and add to the passed object on
 * __SENTRY__.hub.
 * @param carrier object
 * @hidden
 */
function getHubFromCarrier(carrier) {
    if (carrier && carrier.__SENTRY__ && carrier.__SENTRY__.hub)
        return carrier.__SENTRY__.hub;
    carrier.__SENTRY__ = carrier.__SENTRY__ || {};
    carrier.__SENTRY__.hub = new Hub();
    return carrier.__SENTRY__.hub;
}
/**
 * This will set passed {@link Hub} on the passed object's __SENTRY__.hub attribute
 * @param carrier object
 * @param hub Hub
 */
function setHubOnCarrier(carrier, hub) {
    if (!carrier)
        return false;
    carrier.__SENTRY__ = carrier.__SENTRY__ || {};
    carrier.__SENTRY__.hub = hub;
    return true;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/minimal/esm/index.js


/**
 * This calls a function on the current hub.
 * @param method function to call on hub.
 * @param args to pass to function.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function callOnHub(method) {
    var args = [];
    for (var _i = 1; _i < arguments.length; _i++) {
        args[_i - 1] = arguments[_i];
    }
    var hub = getCurrentHub();
    if (hub && hub[method]) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return hub[method].apply(hub, tslib_es6_spread(args));
    }
    throw new Error("No hub defined or " + method + " was not found on the hub, please open a bug report.");
}
/**
 * Captures an exception event and sends it to Sentry.
 *
 * @param exception An exception-like object.
 * @returns The generated eventId.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
function captureException(exception, captureContext) {
    var syntheticException;
    try {
        throw new Error('Sentry syntheticException');
    }
    catch (exception) {
        syntheticException = exception;
    }
    return callOnHub('captureException', exception, {
        captureContext: captureContext,
        originalException: exception,
        syntheticException: syntheticException,
    });
}
/**
 * Captures a message event and sends it to Sentry.
 *
 * @param message The message to send to Sentry.
 * @param level Define the level of the message.
 * @returns The generated eventId.
 */
function captureMessage(message, captureContext) {
    var syntheticException;
    try {
        throw new Error(message);
    }
    catch (exception) {
        syntheticException = exception;
    }
    // This is necessary to provide explicit scopes upgrade, without changing the original
    // arity of the `captureMessage(message, level)` method.
    var level = typeof captureContext === 'string' ? captureContext : undefined;
    var context = typeof captureContext !== 'string' ? { captureContext: captureContext } : undefined;
    return callOnHub('captureMessage', message, level, __assign({ originalException: message, syntheticException: syntheticException }, context));
}
/**
 * Captures a manually created event and sends it to Sentry.
 *
 * @param event The event to send to Sentry.
 * @returns The generated eventId.
 */
function captureEvent(event) {
    return callOnHub('captureEvent', event);
}
/**
 * Callback to set context information onto the scope.
 * @param callback Callback function that receives Scope.
 */
function configureScope(callback) {
    callOnHub('configureScope', callback);
}
/**
 * Records a new breadcrumb which will be attached to future events.
 *
 * Breadcrumbs will be added to subsequent events to provide more context on
 * user's actions prior to an error or crash.
 *
 * @param breadcrumb The breadcrumb to record.
 */
function addBreadcrumb(breadcrumb) {
    callOnHub('addBreadcrumb', breadcrumb);
}
/**
 * Sets context data with the given name.
 * @param name of the context
 * @param context Any kind of data. This data will be normalized.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function setContext(name, context) {
    callOnHub('setContext', name, context);
}
/**
 * Set an object that will be merged sent as extra data with the event.
 * @param extras Extras object to merge into current context.
 */
function setExtras(extras) {
    callOnHub('setExtras', extras);
}
/**
 * Set an object that will be merged sent as tags data with the event.
 * @param tags Tags context object to merge into current context.
 */
function setTags(tags) {
    callOnHub('setTags', tags);
}
/**
 * Set key:value that will be sent as extra data with the event.
 * @param key String of extra
 * @param extra Any kind of data. This data will be normalized.
 */
function setExtra(key, extra) {
    callOnHub('setExtra', key, extra);
}
/**
 * Set key:value that will be sent as tags data with the event.
 * @param key String key of tag
 * @param value String value of tag
 */
function setTag(key, value) {
    callOnHub('setTag', key, value);
}
/**
 * Updates user context information for future events.
 *
 * @param user User context object to be set in the current context. Pass `null` to unset the user.
 */
function setUser(user) {
    callOnHub('setUser', user);
}
/**
 * Creates a new scope with and executes the given operation within.
 * The scope is automatically removed once the operation
 * finishes or throws.
 *
 * This is essentially a convenience function for:
 *
 *     pushScope();
 *     callback();
 *     popScope();
 *
 * @param callback that will be enclosed into push/popScope.
 */
function withScope(callback) {
    callOnHub('withScope', callback);
}
/**
 * Calls a function on the latest client. Use this with caution, it's meant as
 * in "internal" helper so we don't need to expose every possible function in
 * the shim. It is not guaranteed that the client actually implements the
 * function.
 *
 * @param method The method to call on the client/client.
 * @param args Arguments to pass to the client/fontend.
 * @hidden
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function _callOnClient(method) {
    var args = [];
    for (var _i = 1; _i < arguments.length; _i++) {
        args[_i - 1] = arguments[_i];
    }
    callOnHub.apply(void 0, __spread(['_invokeClient', method], args));
}
/**
 * Starts a new `Transaction` and returns it. This is the entry point to manual tracing instrumentation.
 *
 * A tree structure can be built by adding child spans to the transaction, and child spans to other spans. To start a
 * new child span within the transaction or any span, call the respective `.startChild()` method.
 *
 * Every child span must be finished before the transaction is finished, otherwise the unfinished spans are discarded.
 *
 * The transaction must be finished with a call to its `.finish()` method, at which point the transaction with all its
 * finished child spans will be sent to Sentry.
 *
 * @param context Properties of the new `Transaction`.
 * @param customSamplingContext Information given to the transaction sampling function (along with context-dependent
 * default values). See {@link Options.tracesSampler}.
 *
 * @returns The transaction which was just started
 */
function startTransaction(context, customSamplingContext) {
    return callOnHub('startTransaction', __assign({}, context), customSamplingContext);
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/polyfill.js
var setPrototypeOf = Object.setPrototypeOf || ({ __proto__: [] } instanceof Array ? setProtoOf : mixinProperties);
/**
 * setPrototypeOf polyfill using __proto__
 */
// eslint-disable-next-line @typescript-eslint/ban-types
function setProtoOf(obj, proto) {
    // @ts-ignore __proto__ does not exist on obj
    obj.__proto__ = proto;
    return obj;
}
/**
 * setPrototypeOf polyfill using mixin
 */
// eslint-disable-next-line @typescript-eslint/ban-types
function mixinProperties(obj, proto) {
    for (var prop in proto) {
        // eslint-disable-next-line no-prototype-builtins
        if (!obj.hasOwnProperty(prop)) {
            // @ts-ignore typescript complains about indexing so we remove
            obj[prop] = proto[prop];
        }
    }
    return obj;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/error.js


/** An error emitted by Sentry SDKs and related utilities. */
var SentryError = /** @class */ (function (_super) {
    __extends(SentryError, _super);
    function SentryError(message) {
        var _newTarget = this.constructor;
        var _this = _super.call(this, message) || this;
        _this.message = message;
        _this.name = _newTarget.prototype.constructor.name;
        setPrototypeOf(_this, _newTarget.prototype);
        return _this;
    }
    return SentryError;
}(Error));


;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/dsn.js


/** Regular expression used to parse a Dsn. */
var DSN_REGEX = /^(?:(\w+):)\/\/(?:(\w+)(?::(\w+))?@)([\w.-]+)(?::(\d+))?\/(.+)/;
/** Error message */
var ERROR_MESSAGE = 'Invalid Dsn';
/** The Sentry Dsn, identifying a Sentry instance and project. */
var Dsn = /** @class */ (function () {
    /** Creates a new Dsn component */
    function Dsn(from) {
        if (typeof from === 'string') {
            this._fromString(from);
        }
        else {
            this._fromComponents(from);
        }
        this._validate();
    }
    /**
     * Renders the string representation of this Dsn.
     *
     * By default, this will render the public representation without the password
     * component. To get the deprecated private representation, set `withPassword`
     * to true.
     *
     * @param withPassword When set to true, the password will be included.
     */
    Dsn.prototype.toString = function (withPassword) {
        if (withPassword === void 0) { withPassword = false; }
        var _a = this, host = _a.host, path = _a.path, pass = _a.pass, port = _a.port, projectId = _a.projectId, protocol = _a.protocol, user = _a.user;
        return (protocol + "://" + user + (withPassword && pass ? ":" + pass : '') +
            ("@" + host + (port ? ":" + port : '') + "/" + (path ? path + "/" : path) + projectId));
    };
    /** Parses a string into this Dsn. */
    Dsn.prototype._fromString = function (str) {
        var match = DSN_REGEX.exec(str);
        if (!match) {
            throw new SentryError(ERROR_MESSAGE);
        }
        var _a = __read(match.slice(1), 6), protocol = _a[0], user = _a[1], _b = _a[2], pass = _b === void 0 ? '' : _b, host = _a[3], _c = _a[4], port = _c === void 0 ? '' : _c, lastPath = _a[5];
        var path = '';
        var projectId = lastPath;
        var split = projectId.split('/');
        if (split.length > 1) {
            path = split.slice(0, -1).join('/');
            projectId = split.pop();
        }
        if (projectId) {
            var projectMatch = projectId.match(/^\d+/);
            if (projectMatch) {
                projectId = projectMatch[0];
            }
        }
        this._fromComponents({ host: host, pass: pass, path: path, projectId: projectId, port: port, protocol: protocol, user: user });
    };
    /** Maps Dsn components into this instance. */
    Dsn.prototype._fromComponents = function (components) {
        this.protocol = components.protocol;
        this.user = components.user;
        this.pass = components.pass || '';
        this.host = components.host;
        this.port = components.port || '';
        this.path = components.path || '';
        this.projectId = components.projectId;
    };
    /** Validates this Dsn and throws on error. */
    Dsn.prototype._validate = function () {
        var _this = this;
        ['protocol', 'user', 'host', 'projectId'].forEach(function (component) {
            if (!_this[component]) {
                throw new SentryError(ERROR_MESSAGE + ": " + component + " missing");
            }
        });
        if (!this.projectId.match(/^\d+$/)) {
            throw new SentryError(ERROR_MESSAGE + ": Invalid projectId " + this.projectId);
        }
        if (this.protocol !== 'http' && this.protocol !== 'https') {
            throw new SentryError(ERROR_MESSAGE + ": Invalid protocol " + this.protocol);
        }
        if (this.port && isNaN(parseInt(this.port, 10))) {
            throw new SentryError(ERROR_MESSAGE + ": Invalid port " + this.port);
        }
    };
    return Dsn;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/integration.js



var installedIntegrations = [];
/** Gets integration to install */
function getIntegrationsToSetup(options) {
    var defaultIntegrations = (options.defaultIntegrations && tslib_es6_spread(options.defaultIntegrations)) || [];
    var userIntegrations = options.integrations;
    var integrations = [];
    if (Array.isArray(userIntegrations)) {
        var userIntegrationsNames_1 = userIntegrations.map(function (i) { return i.name; });
        var pickedIntegrationsNames_1 = [];
        // Leave only unique default integrations, that were not overridden with provided user integrations
        defaultIntegrations.forEach(function (defaultIntegration) {
            if (userIntegrationsNames_1.indexOf(defaultIntegration.name) === -1 &&
                pickedIntegrationsNames_1.indexOf(defaultIntegration.name) === -1) {
                integrations.push(defaultIntegration);
                pickedIntegrationsNames_1.push(defaultIntegration.name);
            }
        });
        // Don't add same user integration twice
        userIntegrations.forEach(function (userIntegration) {
            if (pickedIntegrationsNames_1.indexOf(userIntegration.name) === -1) {
                integrations.push(userIntegration);
                pickedIntegrationsNames_1.push(userIntegration.name);
            }
        });
    }
    else if (typeof userIntegrations === 'function') {
        integrations = userIntegrations(defaultIntegrations);
        integrations = Array.isArray(integrations) ? integrations : [integrations];
    }
    else {
        integrations = tslib_es6_spread(defaultIntegrations);
    }
    // Make sure that if present, `Debug` integration will always run last
    var integrationsNames = integrations.map(function (i) { return i.name; });
    var alwaysLastToRun = 'Debug';
    if (integrationsNames.indexOf(alwaysLastToRun) !== -1) {
        integrations.push.apply(integrations, tslib_es6_spread(integrations.splice(integrationsNames.indexOf(alwaysLastToRun), 1)));
    }
    return integrations;
}
/** Setup given integration */
function setupIntegration(integration) {
    if (installedIntegrations.indexOf(integration.name) !== -1) {
        return;
    }
    integration.setupOnce(addGlobalEventProcessor, getCurrentHub);
    installedIntegrations.push(integration.name);
    logger.log("Integration installed: " + integration.name);
}
/**
 * Given a list of integration instances this installs them all. When `withDefaults` is set to `true` then all default
 * integrations are added unless they were already provided before.
 * @param integrations array of integration instances
 * @param withDefault should enable default integrations
 */
function setupIntegrations(options) {
    var integrations = {};
    getIntegrationsToSetup(options).forEach(function (integration) {
        integrations[integration.name] = integration;
        setupIntegration(integration);
    });
    return integrations;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/baseclient.js

/* eslint-disable max-lines */




/**
 * Base implementation for all JavaScript SDK clients.
 *
 * Call the constructor with the corresponding backend constructor and options
 * specific to the client subclass. To access these options later, use
 * {@link Client.getOptions}. Also, the Backend instance is available via
 * {@link Client.getBackend}.
 *
 * If a Dsn is specified in the options, it will be parsed and stored. Use
 * {@link Client.getDsn} to retrieve the Dsn at any moment. In case the Dsn is
 * invalid, the constructor will throw a {@link SentryException}. Note that
 * without a valid Dsn, the SDK will not send any events to Sentry.
 *
 * Before sending an event via the backend, it is passed through
 * {@link BaseClient.prepareEvent} to add SDK information and scope data
 * (breadcrumbs and context). To add more custom information, override this
 * method and extend the resulting prepared event.
 *
 * To issue automatically created events (e.g. via instrumentation), use
 * {@link Client.captureEvent}. It will prepare the event and pass it through
 * the callback lifecycle. To issue auto-breadcrumbs, use
 * {@link Client.addBreadcrumb}.
 *
 * @example
 * class NodeClient extends BaseClient<NodeBackend, NodeOptions> {
 *   public constructor(options: NodeOptions) {
 *     super(NodeBackend, options);
 *   }
 *
 *   // ...
 * }
 */
var BaseClient = /** @class */ (function () {
    /**
     * Initializes this client instance.
     *
     * @param backendClass A constructor function to create the backend.
     * @param options Options for the client.
     */
    function BaseClient(backendClass, options) {
        /** Array of used integrations. */
        this._integrations = {};
        /** Number of call being processed */
        this._processing = 0;
        this._backend = new backendClass(options);
        this._options = options;
        if (options.dsn) {
            this._dsn = new Dsn(options.dsn);
        }
    }
    /**
     * @inheritDoc
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    BaseClient.prototype.captureException = function (exception, hint, scope) {
        var _this = this;
        var eventId = hint && hint.event_id;
        this._process(this._getBackend()
            .eventFromException(exception, hint)
            .then(function (event) { return _this._captureEvent(event, hint, scope); })
            .then(function (result) {
            eventId = result;
        }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.captureMessage = function (message, level, hint, scope) {
        var _this = this;
        var eventId = hint && hint.event_id;
        var promisedEvent = isPrimitive(message)
            ? this._getBackend().eventFromMessage("" + message, level, hint)
            : this._getBackend().eventFromException(message, hint);
        this._process(promisedEvent
            .then(function (event) { return _this._captureEvent(event, hint, scope); })
            .then(function (result) {
            eventId = result;
        }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.captureEvent = function (event, hint, scope) {
        var eventId = hint && hint.event_id;
        this._process(this._captureEvent(event, hint, scope).then(function (result) {
            eventId = result;
        }));
        return eventId;
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.captureSession = function (session) {
        if (!session.release) {
            logger.warn('Discarded session because of missing release');
        }
        else {
            this._sendSession(session);
        }
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.getDsn = function () {
        return this._dsn;
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.getOptions = function () {
        return this._options;
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.flush = function (timeout) {
        var _this = this;
        return this._isClientProcessing(timeout).then(function (ready) {
            return _this._getBackend()
                .getTransport()
                .close(timeout)
                .then(function (transportFlushed) { return ready && transportFlushed; });
        });
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.close = function (timeout) {
        var _this = this;
        return this.flush(timeout).then(function (result) {
            _this.getOptions().enabled = false;
            return result;
        });
    };
    /**
     * Sets up the integrations
     */
    BaseClient.prototype.setupIntegrations = function () {
        if (this._isEnabled()) {
            this._integrations = setupIntegrations(this._options);
        }
    };
    /**
     * @inheritDoc
     */
    BaseClient.prototype.getIntegration = function (integration) {
        try {
            return this._integrations[integration.id] || null;
        }
        catch (_oO) {
            logger.warn("Cannot retrieve integration " + integration.id + " from the current Client");
            return null;
        }
    };
    /** Updates existing session based on the provided event */
    BaseClient.prototype._updateSessionFromEvent = function (session, event) {
        var e_1, _a;
        var crashed = false;
        var errored = false;
        var userAgent;
        var exceptions = event.exception && event.exception.values;
        if (exceptions) {
            errored = true;
            try {
                for (var exceptions_1 = __values(exceptions), exceptions_1_1 = exceptions_1.next(); !exceptions_1_1.done; exceptions_1_1 = exceptions_1.next()) {
                    var ex = exceptions_1_1.value;
                    var mechanism = ex.mechanism;
                    if (mechanism && mechanism.handled === false) {
                        crashed = true;
                        break;
                    }
                }
            }
            catch (e_1_1) { e_1 = { error: e_1_1 }; }
            finally {
                try {
                    if (exceptions_1_1 && !exceptions_1_1.done && (_a = exceptions_1.return)) _a.call(exceptions_1);
                }
                finally { if (e_1) throw e_1.error; }
            }
        }
        var user = event.user;
        if (!session.userAgent) {
            var headers = event.request ? event.request.headers : {};
            for (var key in headers) {
                if (key.toLowerCase() === 'user-agent') {
                    userAgent = headers[key];
                    break;
                }
            }
        }
        session.update(__assign(__assign({}, (crashed && { status: SessionStatus.Crashed })), { user: user,
            userAgent: userAgent, errors: session.errors + Number(errored || crashed) }));
    };
    /** Deliver captured session to Sentry */
    BaseClient.prototype._sendSession = function (session) {
        this._getBackend().sendSession(session);
    };
    /** Waits for the client to be done with processing. */
    BaseClient.prototype._isClientProcessing = function (timeout) {
        var _this = this;
        return new SyncPromise(function (resolve) {
            var ticked = 0;
            var tick = 1;
            var interval = setInterval(function () {
                if (_this._processing == 0) {
                    clearInterval(interval);
                    resolve(true);
                }
                else {
                    ticked += tick;
                    if (timeout && ticked >= timeout) {
                        clearInterval(interval);
                        resolve(false);
                    }
                }
            }, tick);
        });
    };
    /** Returns the current backend. */
    BaseClient.prototype._getBackend = function () {
        return this._backend;
    };
    /** Determines whether this SDK is enabled and a valid Dsn is present. */
    BaseClient.prototype._isEnabled = function () {
        return this.getOptions().enabled !== false && this._dsn !== undefined;
    };
    /**
     * Adds common information to events.
     *
     * The information includes release and environment from `options`,
     * breadcrumbs and context (extra, tags and user) from the scope.
     *
     * Information that is already present in the event is never overwritten. For
     * nested objects, such as the context, keys are merged.
     *
     * @param event The original event.
     * @param hint May contain additional information about the original exception.
     * @param scope A scope containing event metadata.
     * @returns A new event with more information.
     */
    BaseClient.prototype._prepareEvent = function (event, scope, hint) {
        var _this = this;
        var _a = this.getOptions().normalizeDepth, normalizeDepth = _a === void 0 ? 3 : _a;
        var prepared = __assign(__assign({}, event), { event_id: event.event_id || (hint && hint.event_id ? hint.event_id : (0,misc/* uuid4 */.DM)()), timestamp: event.timestamp || (0,time/* dateTimestampInSeconds */.yW)() });
        this._applyClientOptions(prepared);
        this._applyIntegrationsMetadata(prepared);
        // If we have scope given to us, use it as the base for further modifications.
        // This allows us to prevent unnecessary copying of data if `captureContext` is not provided.
        var finalScope = scope;
        if (hint && hint.captureContext) {
            finalScope = Scope.clone(finalScope).update(hint.captureContext);
        }
        // We prepare the result here with a resolved Event.
        var result = SyncPromise.resolve(prepared);
        // This should be the last thing called, since we want that
        // {@link Hub.addEventProcessor} gets the finished prepared event.
        if (finalScope) {
            // In case we have a hub we reassign it.
            result = finalScope.applyToEvent(prepared, hint);
        }
        return result.then(function (evt) {
            if (typeof normalizeDepth === 'number' && normalizeDepth > 0) {
                return _this._normalizeEvent(evt, normalizeDepth);
            }
            return evt;
        });
    };
    /**
     * Applies `normalize` function on necessary `Event` attributes to make them safe for serialization.
     * Normalized keys:
     * - `breadcrumbs.data`
     * - `user`
     * - `contexts`
     * - `extra`
     * @param event Event
     * @returns Normalized event
     */
    BaseClient.prototype._normalizeEvent = function (event, depth) {
        if (!event) {
            return null;
        }
        var normalized = __assign(__assign(__assign(__assign(__assign({}, event), (event.breadcrumbs && {
            breadcrumbs: event.breadcrumbs.map(function (b) { return (__assign(__assign({}, b), (b.data && {
                data: normalize(b.data, depth),
            }))); }),
        })), (event.user && {
            user: normalize(event.user, depth),
        })), (event.contexts && {
            contexts: normalize(event.contexts, depth),
        })), (event.extra && {
            extra: normalize(event.extra, depth),
        }));
        // event.contexts.trace stores information about a Transaction. Similarly,
        // event.spans[] stores information about child Spans. Given that a
        // Transaction is conceptually a Span, normalization should apply to both
        // Transactions and Spans consistently.
        // For now the decision is to skip normalization of Transactions and Spans,
        // so this block overwrites the normalized event to add back the original
        // Transaction information prior to normalization.
        if (event.contexts && event.contexts.trace) {
            // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
            normalized.contexts.trace = event.contexts.trace;
        }
        return normalized;
    };
    /**
     *  Enhances event using the client configuration.
     *  It takes care of all "static" values like environment, release and `dist`,
     *  as well as truncating overly long values.
     * @param event event instance to be enhanced
     */
    BaseClient.prototype._applyClientOptions = function (event) {
        var options = this.getOptions();
        var environment = options.environment, release = options.release, dist = options.dist, _a = options.maxValueLength, maxValueLength = _a === void 0 ? 250 : _a;
        if (!('environment' in event)) {
            event.environment = 'environment' in options ? environment : 'production';
        }
        if (event.release === undefined && release !== undefined) {
            event.release = release;
        }
        if (event.dist === undefined && dist !== undefined) {
            event.dist = dist;
        }
        if (event.message) {
            event.message = truncate(event.message, maxValueLength);
        }
        var exception = event.exception && event.exception.values && event.exception.values[0];
        if (exception && exception.value) {
            exception.value = truncate(exception.value, maxValueLength);
        }
        var request = event.request;
        if (request && request.url) {
            request.url = truncate(request.url, maxValueLength);
        }
    };
    /**
     * This function adds all used integrations to the SDK info in the event.
     * @param sdkInfo The sdkInfo of the event that will be filled with all integrations.
     */
    BaseClient.prototype._applyIntegrationsMetadata = function (event) {
        var sdkInfo = event.sdk;
        var integrationsArray = Object.keys(this._integrations);
        if (sdkInfo && integrationsArray.length > 0) {
            sdkInfo.integrations = integrationsArray;
        }
    };
    /**
     * Tells the backend to send this event
     * @param event The Sentry event to send
     */
    BaseClient.prototype._sendEvent = function (event) {
        this._getBackend().sendEvent(event);
    };
    /**
     * Processes the event and logs an error in case of rejection
     * @param event
     * @param hint
     * @param scope
     */
    BaseClient.prototype._captureEvent = function (event, hint, scope) {
        return this._processEvent(event, hint, scope).then(function (finalEvent) {
            return finalEvent.event_id;
        }, function (reason) {
            logger.error(reason);
            return undefined;
        });
    };
    /**
     * Processes an event (either error or message) and sends it to Sentry.
     *
     * This also adds breadcrumbs and context information to the event. However,
     * platform specific meta data (such as the User's IP address) must be added
     * by the SDK implementor.
     *
     *
     * @param event The event to send to Sentry.
     * @param hint May contain additional information about the original exception.
     * @param scope A scope containing event metadata.
     * @returns A SyncPromise that resolves with the event or rejects in case event was/will not be send.
     */
    BaseClient.prototype._processEvent = function (event, hint, scope) {
        var _this = this;
        // eslint-disable-next-line @typescript-eslint/unbound-method
        var _a = this.getOptions(), beforeSend = _a.beforeSend, sampleRate = _a.sampleRate;
        if (!this._isEnabled()) {
            return SyncPromise.reject(new SentryError('SDK not enabled, will not send event.'));
        }
        var isTransaction = event.type === 'transaction';
        // 1.0 === 100% events are sent
        // 0.0 === 0% events are sent
        // Sampling for transaction happens somewhere else
        if (!isTransaction && typeof sampleRate === 'number' && Math.random() > sampleRate) {
            return SyncPromise.reject(new SentryError('This event has been sampled, will not send event.'));
        }
        return this._prepareEvent(event, scope, hint)
            .then(function (prepared) {
            if (prepared === null) {
                throw new SentryError('An event processor returned null, will not send event.');
            }
            var isInternalException = hint && hint.data && hint.data.__sentry__ === true;
            if (isInternalException || isTransaction || !beforeSend) {
                return prepared;
            }
            var beforeSendResult = beforeSend(prepared, hint);
            if (typeof beforeSendResult === 'undefined') {
                throw new SentryError('`beforeSend` method has to return `null` or a valid event.');
            }
            else if (isThenable(beforeSendResult)) {
                return beforeSendResult.then(function (event) { return event; }, function (e) {
                    throw new SentryError("beforeSend rejected with " + e);
                });
            }
            return beforeSendResult;
        })
            .then(function (processedEvent) {
            if (processedEvent === null) {
                throw new SentryError('`beforeSend` returned `null`, will not send event.');
            }
            var session = scope && scope.getSession();
            if (!isTransaction && session) {
                _this._updateSessionFromEvent(session, processedEvent);
            }
            _this._sendEvent(processedEvent);
            return processedEvent;
        })
            .then(null, function (reason) {
            if (reason instanceof SentryError) {
                throw reason;
            }
            _this.captureException(reason, {
                data: {
                    __sentry__: true,
                },
                originalException: reason,
            });
            throw new SentryError("Event processing pipeline threw an error, original event will not be sent. Details have been sent as a new event.\nReason: " + reason);
        });
    };
    /**
     * Occupies the client with processing and event
     */
    BaseClient.prototype._process = function (promise) {
        var _this = this;
        this._processing += 1;
        promise.then(function (value) {
            _this._processing -= 1;
            return value;
        }, function (reason) {
            _this._processing -= 1;
            return reason;
        });
    };
    return BaseClient;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/transports/noop.js


/** Noop transport */
var NoopTransport = /** @class */ (function () {
    function NoopTransport() {
    }
    /**
     * @inheritDoc
     */
    NoopTransport.prototype.sendEvent = function (_) {
        return SyncPromise.resolve({
            reason: "NoopTransport: Event has been skipped because no Dsn is configured.",
            status: Status.Skipped,
        });
    };
    /**
     * @inheritDoc
     */
    NoopTransport.prototype.close = function (_) {
        return SyncPromise.resolve(true);
    };
    return NoopTransport;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/basebackend.js


/**
 * This is the base implemention of a Backend.
 * @hidden
 */
var BaseBackend = /** @class */ (function () {
    /** Creates a new backend instance. */
    function BaseBackend(options) {
        this._options = options;
        if (!this._options.dsn) {
            logger.warn('No DSN provided, backend will not do anything.');
        }
        this._transport = this._setupTransport();
    }
    /**
     * @inheritDoc
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
    BaseBackend.prototype.eventFromException = function (_exception, _hint) {
        throw new SentryError('Backend has to implement `eventFromException` method');
    };
    /**
     * @inheritDoc
     */
    BaseBackend.prototype.eventFromMessage = function (_message, _level, _hint) {
        throw new SentryError('Backend has to implement `eventFromMessage` method');
    };
    /**
     * @inheritDoc
     */
    BaseBackend.prototype.sendEvent = function (event) {
        this._transport.sendEvent(event).then(null, function (reason) {
            logger.error("Error while sending event: " + reason);
        });
    };
    /**
     * @inheritDoc
     */
    BaseBackend.prototype.sendSession = function (session) {
        if (!this._transport.sendSession) {
            logger.warn("Dropping session because custom transport doesn't implement sendSession");
            return;
        }
        this._transport.sendSession(session).then(null, function (reason) {
            logger.error("Error while sending session: " + reason);
        });
    };
    /**
     * @inheritDoc
     */
    BaseBackend.prototype.getTransport = function () {
        return this._transport;
    };
    /**
     * Sets up the transport so it can be used later to send requests.
     */
    BaseBackend.prototype._setupTransport = function () {
        return new NoopTransport();
    };
    return BaseBackend;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/supports.js


/**
 * Tells whether current environment supports ErrorEvent objects
 * {@link supportsErrorEvent}.
 *
 * @returns Answer to the given question.
 */
function supportsErrorEvent() {
    try {
        new ErrorEvent('');
        return true;
    }
    catch (e) {
        return false;
    }
}
/**
 * Tells whether current environment supports DOMError objects
 * {@link supportsDOMError}.
 *
 * @returns Answer to the given question.
 */
function supportsDOMError() {
    try {
        // Chrome: VM89:1 Uncaught TypeError: Failed to construct 'DOMError':
        // 1 argument required, but only 0 present.
        // @ts-ignore It really needs 1 argument, not 0.
        new DOMError('');
        return true;
    }
    catch (e) {
        return false;
    }
}
/**
 * Tells whether current environment supports DOMException objects
 * {@link supportsDOMException}.
 *
 * @returns Answer to the given question.
 */
function supportsDOMException() {
    try {
        new DOMException('');
        return true;
    }
    catch (e) {
        return false;
    }
}
/**
 * Tells whether current environment supports Fetch API
 * {@link supportsFetch}.
 *
 * @returns Answer to the given question.
 */
function supportsFetch() {
    if (!('fetch' in (0,misc/* getGlobalObject */.Rf)())) {
        return false;
    }
    try {
        new Headers();
        new Request('');
        new Response();
        return true;
    }
    catch (e) {
        return false;
    }
}
/**
 * isNativeFetch checks if the given function is a native implementation of fetch()
 */
// eslint-disable-next-line @typescript-eslint/ban-types
function isNativeFetch(func) {
    return func && /^function fetch\(\)\s+\{\s+\[native code\]\s+\}$/.test(func.toString());
}
/**
 * Tells whether current environment supports Fetch API natively
 * {@link supportsNativeFetch}.
 *
 * @returns true if `window.fetch` is natively implemented, false otherwise
 */
function supportsNativeFetch() {
    if (!supportsFetch()) {
        return false;
    }
    var global = (0,misc/* getGlobalObject */.Rf)();
    // Fast path to avoid DOM I/O
    // eslint-disable-next-line @typescript-eslint/unbound-method
    if (isNativeFetch(global.fetch)) {
        return true;
    }
    // window.fetch is implemented, but is polyfilled or already wrapped (e.g: by a chrome extension)
    // so create a "pure" iframe to see if that has native fetch
    var result = false;
    var doc = global.document;
    // eslint-disable-next-line deprecation/deprecation
    if (doc && typeof doc.createElement === "function") {
        try {
            var sandbox = doc.createElement('iframe');
            sandbox.hidden = true;
            doc.head.appendChild(sandbox);
            if (sandbox.contentWindow && sandbox.contentWindow.fetch) {
                // eslint-disable-next-line @typescript-eslint/unbound-method
                result = isNativeFetch(sandbox.contentWindow.fetch);
            }
            doc.head.removeChild(sandbox);
        }
        catch (err) {
            logger.warn('Could not create sandbox iframe for pure fetch check, bailing to window.fetch: ', err);
        }
    }
    return result;
}
/**
 * Tells whether current environment supports ReportingObserver API
 * {@link supportsReportingObserver}.
 *
 * @returns Answer to the given question.
 */
function supportsReportingObserver() {
    return 'ReportingObserver' in getGlobalObject();
}
/**
 * Tells whether current environment supports Referrer Policy API
 * {@link supportsReferrerPolicy}.
 *
 * @returns Answer to the given question.
 */
function supportsReferrerPolicy() {
    // Despite all stars in the sky saying that Edge supports old draft syntax, aka 'never', 'always', 'origin' and 'default
    // https://caniuse.com/#feat=referrer-policy
    // It doesn't. And it throw exception instead of ignoring this parameter...
    // REF: https://github.com/getsentry/raven-js/issues/1233
    if (!supportsFetch()) {
        return false;
    }
    try {
        new Request('_', {
            referrerPolicy: 'origin',
        });
        return true;
    }
    catch (e) {
        return false;
    }
}
/**
 * Tells whether current environment supports History API
 * {@link supportsHistory}.
 *
 * @returns Answer to the given question.
 */
function supportsHistory() {
    // NOTE: in Chrome App environment, touching history.pushState, *even inside
    //       a try/catch block*, will cause Chrome to output an error to console.error
    // borrowed from: https://github.com/angular/angular.js/pull/13945/files
    var global = (0,misc/* getGlobalObject */.Rf)();
    /* eslint-disable @typescript-eslint/no-unsafe-member-access */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    var chrome = global.chrome;
    var isChromePackagedApp = chrome && chrome.app && chrome.app.runtime;
    /* eslint-enable @typescript-eslint/no-unsafe-member-access */
    var hasHistoryApi = 'history' in global && !!global.history.pushState && !!global.history.replaceState;
    return !isChromePackagedApp && hasHistoryApi;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/tracekit.js
/**
 * This was originally forked from https://github.com/occ/TraceKit, but has since been
 * largely modified and is now maintained as part of Sentry JS SDK.
 */

// global reference to slice
var UNKNOWN_FUNCTION = '?';
// Chromium based browsers: Chrome, Brave, new Opera, new Edge
var chrome = /^\s*at (?:(.*?) ?\()?((?:file|https?|blob|chrome-extension|address|native|eval|webpack|<anonymous>|[-a-z]+:|.*bundle|\/).*?)(?::(\d+))?(?::(\d+))?\)?\s*$/i;
// gecko regex: `(?:bundle|\d+\.js)`: `bundle` is for react native, `\d+\.js` also but specifically for ram bundles because it
// generates filenames without a prefix like `file://` the filenames in the stacktrace are just 42.js
// We need this specific case for now because we want no other regex to match.
var gecko = /^\s*(.*?)(?:\((.*?)\))?(?:^|@)?((?:file|https?|blob|chrome|webpack|resource|moz-extension|capacitor).*?:\/.*?|\[native code\]|[^@]*(?:bundle|\d+\.js))(?::(\d+))?(?::(\d+))?\s*$/i;
var winjs = /^\s*at (?:((?:\[object object\])?.+) )?\(?((?:file|ms-appx|https?|webpack|blob):.*?):(\d+)(?::(\d+))?\)?\s*$/i;
var geckoEval = /(\S+) line (\d+)(?: > eval line \d+)* > eval/i;
var chromeEval = /\((\S*)(?::(\d+))(?::(\d+))\)/;
// Based on our own mapping pattern - https://github.com/getsentry/sentry/blob/9f08305e09866c8bd6d0c24f5b0aabdd7dd6c59c/src/sentry/lang/javascript/errormapping.py#L83-L108
var reactMinifiedRegexp = /Minified React error #\d+;/i;
/** JSDoc */
// eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/explicit-module-boundary-types
function computeStackTrace(ex) {
    var stack = null;
    var popSize = 0;
    if (ex) {
        if (typeof ex.framesToPop === 'number') {
            popSize = ex.framesToPop;
        }
        else if (reactMinifiedRegexp.test(ex.message)) {
            popSize = 1;
        }
    }
    try {
        // This must be tried first because Opera 10 *destroys*
        // its stacktrace property if you try to access the stack
        // property first!!
        stack = computeStackTraceFromStacktraceProp(ex);
        if (stack) {
            return popFrames(stack, popSize);
        }
    }
    catch (e) {
        // no-empty
    }
    try {
        stack = computeStackTraceFromStackProp(ex);
        if (stack) {
            return popFrames(stack, popSize);
        }
    }
    catch (e) {
        // no-empty
    }
    return {
        message: extractMessage(ex),
        name: ex && ex.name,
        stack: [],
        failed: true,
    };
}
/** JSDoc */
// eslint-disable-next-line @typescript-eslint/no-explicit-any, complexity
function computeStackTraceFromStackProp(ex) {
    if (!ex || !ex.stack) {
        return null;
    }
    var stack = [];
    var lines = ex.stack.split('\n');
    var isEval;
    var submatch;
    var parts;
    var element;
    for (var i = 0; i < lines.length; ++i) {
        if ((parts = chrome.exec(lines[i]))) {
            var isNative = parts[2] && parts[2].indexOf('native') === 0; // start of line
            isEval = parts[2] && parts[2].indexOf('eval') === 0; // start of line
            if (isEval && (submatch = chromeEval.exec(parts[2]))) {
                // throw out eval line/column and use top-most line/column number
                parts[2] = submatch[1]; // url
                parts[3] = submatch[2]; // line
                parts[4] = submatch[3]; // column
            }
            element = {
                // working with the regexp above is super painful. it is quite a hack, but just stripping the `address at `
                // prefix here seems like the quickest solution for now.
                url: parts[2] && parts[2].indexOf('address at ') === 0 ? parts[2].substr('address at '.length) : parts[2],
                func: parts[1] || UNKNOWN_FUNCTION,
                args: isNative ? [parts[2]] : [],
                line: parts[3] ? +parts[3] : null,
                column: parts[4] ? +parts[4] : null,
            };
        }
        else if ((parts = winjs.exec(lines[i]))) {
            element = {
                url: parts[2],
                func: parts[1] || UNKNOWN_FUNCTION,
                args: [],
                line: +parts[3],
                column: parts[4] ? +parts[4] : null,
            };
        }
        else if ((parts = gecko.exec(lines[i]))) {
            isEval = parts[3] && parts[3].indexOf(' > eval') > -1;
            if (isEval && (submatch = geckoEval.exec(parts[3]))) {
                // throw out eval line/column and use top-most line number
                parts[1] = parts[1] || "eval";
                parts[3] = submatch[1];
                parts[4] = submatch[2];
                parts[5] = ''; // no column when eval
            }
            else if (i === 0 && !parts[5] && ex.columnNumber !== void 0) {
                // FireFox uses this awesome columnNumber property for its top frame
                // Also note, Firefox's column number is 0-based and everything else expects 1-based,
                // so adding 1
                // NOTE: this hack doesn't work if top-most frame is eval
                stack[0].column = ex.columnNumber + 1;
            }
            element = {
                url: parts[3],
                func: parts[1] || UNKNOWN_FUNCTION,
                args: parts[2] ? parts[2].split(',') : [],
                line: parts[4] ? +parts[4] : null,
                column: parts[5] ? +parts[5] : null,
            };
        }
        else {
            continue;
        }
        if (!element.func && element.line) {
            element.func = UNKNOWN_FUNCTION;
        }
        stack.push(element);
    }
    if (!stack.length) {
        return null;
    }
    return {
        message: extractMessage(ex),
        name: ex.name,
        stack: stack,
    };
}
/** JSDoc */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function computeStackTraceFromStacktraceProp(ex) {
    if (!ex || !ex.stacktrace) {
        return null;
    }
    // Access and store the stacktrace property before doing ANYTHING
    // else to it because Opera is not very good at providing it
    // reliably in other circumstances.
    var stacktrace = ex.stacktrace;
    var opera10Regex = / line (\d+).*script (?:in )?(\S+)(?:: in function (\S+))?$/i;
    var opera11Regex = / line (\d+), column (\d+)\s*(?:in (?:<anonymous function: ([^>]+)>|([^)]+))\((.*)\))? in (.*):\s*$/i;
    var lines = stacktrace.split('\n');
    var stack = [];
    var parts;
    for (var line = 0; line < lines.length; line += 2) {
        var element = null;
        if ((parts = opera10Regex.exec(lines[line]))) {
            element = {
                url: parts[2],
                func: parts[3],
                args: [],
                line: +parts[1],
                column: null,
            };
        }
        else if ((parts = opera11Regex.exec(lines[line]))) {
            element = {
                url: parts[6],
                func: parts[3] || parts[4],
                args: parts[5] ? parts[5].split(',') : [],
                line: +parts[1],
                column: +parts[2],
            };
        }
        if (element) {
            if (!element.func && element.line) {
                element.func = UNKNOWN_FUNCTION;
            }
            stack.push(element);
        }
    }
    if (!stack.length) {
        return null;
    }
    return {
        message: extractMessage(ex),
        name: ex.name,
        stack: stack,
    };
}
/** Remove N number of frames from the stack */
function popFrames(stacktrace, popSize) {
    try {
        return __assign(__assign({}, stacktrace), { stack: stacktrace.stack.slice(popSize) });
    }
    catch (e) {
        return stacktrace;
    }
}
/**
 * There are cases where stacktrace.message is an Event object
 * https://github.com/getsentry/sentry-javascript/issues/1949
 * In this specific case we try to extract stacktrace.message.error.message
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function extractMessage(ex) {
    var message = ex && ex.message;
    if (!message) {
        return 'No error message';
    }
    if (message.error && typeof message.error.message === 'string') {
        return message.error.message;
    }
    return message;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/parsers.js


var STACKTRACE_LIMIT = 50;
/**
 * This function creates an exception from an TraceKitStackTrace
 * @param stacktrace TraceKitStackTrace that will be converted to an exception
 * @hidden
 */
function exceptionFromStacktrace(stacktrace) {
    var frames = prepareFramesForEvent(stacktrace.stack);
    var exception = {
        type: stacktrace.name,
        value: stacktrace.message,
    };
    if (frames && frames.length) {
        exception.stacktrace = { frames: frames };
    }
    if (exception.type === undefined && exception.value === '') {
        exception.value = 'Unrecoverable error caught';
    }
    return exception;
}
/**
 * @hidden
 */
function eventFromPlainObject(exception, syntheticException, rejection) {
    var event = {
        exception: {
            values: [
                {
                    type: isEvent(exception) ? exception.constructor.name : rejection ? 'UnhandledRejection' : 'Error',
                    value: "Non-Error " + (rejection ? 'promise rejection' : 'exception') + " captured with keys: " + extractExceptionKeysForMessage(exception),
                },
            ],
        },
        extra: {
            __serialized__: normalizeToSize(exception),
        },
    };
    if (syntheticException) {
        var stacktrace = computeStackTrace(syntheticException);
        var frames_1 = prepareFramesForEvent(stacktrace.stack);
        event.stacktrace = {
            frames: frames_1,
        };
    }
    return event;
}
/**
 * @hidden
 */
function eventFromStacktrace(stacktrace) {
    var exception = exceptionFromStacktrace(stacktrace);
    return {
        exception: {
            values: [exception],
        },
    };
}
/**
 * @hidden
 */
function prepareFramesForEvent(stack) {
    if (!stack || !stack.length) {
        return [];
    }
    var localStack = stack;
    var firstFrameFunction = localStack[0].func || '';
    var lastFrameFunction = localStack[localStack.length - 1].func || '';
    // If stack starts with one of our API calls, remove it (starts, meaning it's the top of the stack - aka last call)
    if (firstFrameFunction.indexOf('captureMessage') !== -1 || firstFrameFunction.indexOf('captureException') !== -1) {
        localStack = localStack.slice(1);
    }
    // If stack ends with one of our internal API calls, remove it (ends, meaning it's the bottom of the stack - aka top-most call)
    if (lastFrameFunction.indexOf('sentryWrapped') !== -1) {
        localStack = localStack.slice(0, -1);
    }
    // The frame where the crash happened, should be the last entry in the array
    return localStack
        .slice(0, STACKTRACE_LIMIT)
        .map(function (frame) { return ({
        colno: frame.column === null ? undefined : frame.column,
        filename: frame.url || localStack[0].url,
        function: frame.func || '?',
        in_app: true,
        lineno: frame.line === null ? undefined : frame.line,
    }); })
        .reverse();
}

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/eventbuilder.js




/**
 * Builds and Event from a Exception
 * @hidden
 */
function eventFromException(options, exception, hint) {
    var syntheticException = (hint && hint.syntheticException) || undefined;
    var event = eventFromUnknownInput(exception, syntheticException, {
        attachStacktrace: options.attachStacktrace,
    });
    (0,misc/* addExceptionMechanism */.EG)(event, {
        handled: true,
        type: 'generic',
    });
    event.level = Severity.Error;
    if (hint && hint.event_id) {
        event.event_id = hint.event_id;
    }
    return SyncPromise.resolve(event);
}
/**
 * Builds and Event from a Message
 * @hidden
 */
function eventFromMessage(options, message, level, hint) {
    if (level === void 0) { level = Severity.Info; }
    var syntheticException = (hint && hint.syntheticException) || undefined;
    var event = eventFromString(message, syntheticException, {
        attachStacktrace: options.attachStacktrace,
    });
    event.level = level;
    if (hint && hint.event_id) {
        event.event_id = hint.event_id;
    }
    return SyncPromise.resolve(event);
}
/**
 * @hidden
 */
function eventFromUnknownInput(exception, syntheticException, options) {
    if (options === void 0) { options = {}; }
    var event;
    if (isErrorEvent(exception) && exception.error) {
        // If it is an ErrorEvent with `error` property, extract it to get actual Error
        var errorEvent = exception;
        // eslint-disable-next-line no-param-reassign
        exception = errorEvent.error;
        event = eventFromStacktrace(computeStackTrace(exception));
        return event;
    }
    if (isDOMError(exception) || isDOMException(exception)) {
        // If it is a DOMError or DOMException (which are legacy APIs, but still supported in some browsers)
        // then we just extract the name and message, as they don't provide anything else
        // https://developer.mozilla.org/en-US/docs/Web/API/DOMError
        // https://developer.mozilla.org/en-US/docs/Web/API/DOMException
        var domException = exception;
        var name_1 = domException.name || (isDOMError(domException) ? 'DOMError' : 'DOMException');
        var message = domException.message ? name_1 + ": " + domException.message : name_1;
        event = eventFromString(message, syntheticException, options);
        (0,misc/* addExceptionTypeValue */.Db)(event, message);
        return event;
    }
    if (isError(exception)) {
        // we have a real Error object, do nothing
        event = eventFromStacktrace(computeStackTrace(exception));
        return event;
    }
    if (isPlainObject(exception) || isEvent(exception)) {
        // If it is plain Object or Event, serialize it manually and extract options
        // This will allow us to group events based on top-level keys
        // which is much better than creating new group when any key/value change
        var objectException = exception;
        event = eventFromPlainObject(objectException, syntheticException, options.rejection);
        (0,misc/* addExceptionMechanism */.EG)(event, {
            synthetic: true,
        });
        return event;
    }
    // If none of previous checks were valid, then it means that it's not:
    // - an instance of DOMError
    // - an instance of DOMException
    // - an instance of Event
    // - an instance of Error
    // - a valid ErrorEvent (one with an error property)
    // - a plain Object
    //
    // So bail out and capture it as a simple message:
    event = eventFromString(exception, syntheticException, options);
    (0,misc/* addExceptionTypeValue */.Db)(event, "" + exception, undefined);
    (0,misc/* addExceptionMechanism */.EG)(event, {
        synthetic: true,
    });
    return event;
}
/**
 * @hidden
 */
function eventFromString(input, syntheticException, options) {
    if (options === void 0) { options = {}; }
    var event = {
        message: input,
    };
    if (options.attachStacktrace && syntheticException) {
        var stacktrace = computeStackTrace(syntheticException);
        var frames_1 = prepareFramesForEvent(stacktrace.stack);
        event.stacktrace = {
            frames: frames_1,
        };
    }
    return event;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/request.js
/** Creates a SentryRequest from an event. */
function sessionToSentryRequest(session, api) {
    var envelopeHeaders = JSON.stringify({
        sent_at: new Date().toISOString(),
    });
    var itemHeaders = JSON.stringify({
        type: 'session',
    });
    return {
        body: envelopeHeaders + "\n" + itemHeaders + "\n" + JSON.stringify(session),
        type: 'session',
        url: api.getEnvelopeEndpointWithUrlEncodedAuth(),
    };
}
/** Creates a SentryRequest from an event. */
function eventToSentryRequest(event, api) {
    var useEnvelope = event.type === 'transaction';
    var req = {
        body: JSON.stringify(event),
        type: event.type || 'event',
        url: useEnvelope ? api.getEnvelopeEndpointWithUrlEncodedAuth() : api.getStoreEndpointWithUrlEncodedAuth(),
    };
    // https://develop.sentry.dev/sdk/envelopes/
    // Since we don't need to manipulate envelopes nor store them, there is no
    // exported concept of an Envelope with operations including serialization and
    // deserialization. Instead, we only implement a minimal subset of the spec to
    // serialize events inline here.
    if (useEnvelope) {
        var envelopeHeaders = JSON.stringify({
            event_id: event.event_id,
            // We need to add * 1000 since we divide it by 1000 by default but JS works with ms precision
            // The reason we use timestampWithMs here is that all clocks across the SDK use the same clock
            sent_at: new Date().toISOString(),
        });
        var itemHeaders = JSON.stringify({
            type: event.type,
        });
        // The trailing newline is optional. We intentionally don't send it to avoid
        // sending unnecessary bytes.
        //
        // const envelope = `${envelopeHeaders}\n${itemHeaders}\n${req.body}\n`;
        var envelope = envelopeHeaders + "\n" + itemHeaders + "\n" + req.body;
        req.body = envelope;
    }
    return req;
}

;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/api.js

var SENTRY_API_VERSION = '7';
/** Helper class to provide urls to different Sentry endpoints. */
var API = /** @class */ (function () {
    /** Create a new instance of API */
    function API(dsn) {
        this.dsn = dsn;
        this._dsnObject = new Dsn(dsn);
    }
    /** Returns the Dsn object. */
    API.prototype.getDsn = function () {
        return this._dsnObject;
    };
    /** Returns the prefix to construct Sentry ingestion API endpoints. */
    API.prototype.getBaseApiEndpoint = function () {
        var dsn = this._dsnObject;
        var protocol = dsn.protocol ? dsn.protocol + ":" : '';
        var port = dsn.port ? ":" + dsn.port : '';
        return protocol + "//" + dsn.host + port + (dsn.path ? "/" + dsn.path : '') + "/api/";
    };
    /** Returns the store endpoint URL. */
    API.prototype.getStoreEndpoint = function () {
        return this._getIngestEndpoint('store');
    };
    /**
     * Returns the store endpoint URL with auth in the query string.
     *
     * Sending auth as part of the query string and not as custom HTTP headers avoids CORS preflight requests.
     */
    API.prototype.getStoreEndpointWithUrlEncodedAuth = function () {
        return this.getStoreEndpoint() + "?" + this._encodedAuth();
    };
    /**
     * Returns the envelope endpoint URL with auth in the query string.
     *
     * Sending auth as part of the query string and not as custom HTTP headers avoids CORS preflight requests.
     */
    API.prototype.getEnvelopeEndpointWithUrlEncodedAuth = function () {
        return this._getEnvelopeEndpoint() + "?" + this._encodedAuth();
    };
    /** Returns only the path component for the store endpoint. */
    API.prototype.getStoreEndpointPath = function () {
        var dsn = this._dsnObject;
        return (dsn.path ? "/" + dsn.path : '') + "/api/" + dsn.projectId + "/store/";
    };
    /**
     * Returns an object that can be used in request headers.
     * This is needed for node and the old /store endpoint in sentry
     */
    API.prototype.getRequestHeaders = function (clientName, clientVersion) {
        var dsn = this._dsnObject;
        var header = ["Sentry sentry_version=" + SENTRY_API_VERSION];
        header.push("sentry_client=" + clientName + "/" + clientVersion);
        header.push("sentry_key=" + dsn.user);
        if (dsn.pass) {
            header.push("sentry_secret=" + dsn.pass);
        }
        return {
            'Content-Type': 'application/json',
            'X-Sentry-Auth': header.join(', '),
        };
    };
    /** Returns the url to the report dialog endpoint. */
    API.prototype.getReportDialogEndpoint = function (dialogOptions) {
        if (dialogOptions === void 0) { dialogOptions = {}; }
        var dsn = this._dsnObject;
        var endpoint = this.getBaseApiEndpoint() + "embed/error-page/";
        var encodedOptions = [];
        encodedOptions.push("dsn=" + dsn.toString());
        for (var key in dialogOptions) {
            if (key === 'user') {
                if (!dialogOptions.user) {
                    continue;
                }
                if (dialogOptions.user.name) {
                    encodedOptions.push("name=" + encodeURIComponent(dialogOptions.user.name));
                }
                if (dialogOptions.user.email) {
                    encodedOptions.push("email=" + encodeURIComponent(dialogOptions.user.email));
                }
            }
            else {
                encodedOptions.push(encodeURIComponent(key) + "=" + encodeURIComponent(dialogOptions[key]));
            }
        }
        if (encodedOptions.length) {
            return endpoint + "?" + encodedOptions.join('&');
        }
        return endpoint;
    };
    /** Returns the envelope endpoint URL. */
    API.prototype._getEnvelopeEndpoint = function () {
        return this._getIngestEndpoint('envelope');
    };
    /** Returns the ingest API endpoint for target. */
    API.prototype._getIngestEndpoint = function (target) {
        var base = this.getBaseApiEndpoint();
        var dsn = this._dsnObject;
        return "" + base + dsn.projectId + "/" + target + "/";
    };
    /** Returns a URL-encoded string with auth config suitable for a query string. */
    API.prototype._encodedAuth = function () {
        var dsn = this._dsnObject;
        var auth = {
            // We send only the minimum set of required information. See
            // https://github.com/getsentry/sentry-javascript/issues/2572.
            sentry_key: dsn.user,
            sentry_version: SENTRY_API_VERSION,
        };
        return urlEncode(auth);
    };
    return API;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/promisebuffer.js


/** A simple queue that holds promises. */
var PromiseBuffer = /** @class */ (function () {
    function PromiseBuffer(_limit) {
        this._limit = _limit;
        /** Internal set of queued Promises */
        this._buffer = [];
    }
    /**
     * Says if the buffer is ready to take more requests
     */
    PromiseBuffer.prototype.isReady = function () {
        return this._limit === undefined || this.length() < this._limit;
    };
    /**
     * Add a promise to the queue.
     *
     * @param task Can be any PromiseLike<T>
     * @returns The original promise.
     */
    PromiseBuffer.prototype.add = function (task) {
        var _this = this;
        if (!this.isReady()) {
            return SyncPromise.reject(new SentryError('Not adding Promise due to buffer limit reached.'));
        }
        if (this._buffer.indexOf(task) === -1) {
            this._buffer.push(task);
        }
        task
            .then(function () { return _this.remove(task); })
            .then(null, function () {
            return _this.remove(task).then(null, function () {
                // We have to add this catch here otherwise we have an unhandledPromiseRejection
                // because it's a new Promise chain.
            });
        });
        return task;
    };
    /**
     * Remove a promise to the queue.
     *
     * @param task Can be any PromiseLike<T>
     * @returns Removed promise.
     */
    PromiseBuffer.prototype.remove = function (task) {
        var removedTask = this._buffer.splice(this._buffer.indexOf(task), 1)[0];
        return removedTask;
    };
    /**
     * This function returns the number of unresolved promises in the queue.
     */
    PromiseBuffer.prototype.length = function () {
        return this._buffer.length;
    };
    /**
     * This will drain the whole queue, returns true if queue is empty or drained.
     * If timeout is provided and the queue takes longer to drain, the promise still resolves but with false.
     *
     * @param timeout Number in ms to wait until it resolves with false.
     */
    PromiseBuffer.prototype.drain = function (timeout) {
        var _this = this;
        return new SyncPromise(function (resolve) {
            var capturedSetTimeout = setTimeout(function () {
                if (timeout && timeout > 0) {
                    resolve(false);
                }
            }, timeout);
            SyncPromise.all(_this._buffer)
                .then(function () {
                clearTimeout(capturedSetTimeout);
                resolve(true);
            })
                .then(null, function () {
                resolve(true);
            });
        });
    };
    return PromiseBuffer;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/transports/base.js




/** Base Transport class implementation */
var BaseTransport = /** @class */ (function () {
    function BaseTransport(options) {
        this.options = options;
        /** A simple buffer holding all requests. */
        this._buffer = new PromiseBuffer(30);
        /** Locks transport after receiving rate limits in a response */
        this._rateLimits = {};
        this._api = new API(this.options.dsn);
        // eslint-disable-next-line deprecation/deprecation
        this.url = this._api.getStoreEndpointWithUrlEncodedAuth();
    }
    /**
     * @inheritDoc
     */
    BaseTransport.prototype.sendEvent = function (_) {
        throw new SentryError('Transport Class has to implement `sendEvent` method');
    };
    /**
     * @inheritDoc
     */
    BaseTransport.prototype.close = function (timeout) {
        return this._buffer.drain(timeout);
    };
    /**
     * Handle Sentry repsonse for promise-based transports.
     */
    BaseTransport.prototype._handleResponse = function (_a) {
        var requestType = _a.requestType, response = _a.response, headers = _a.headers, resolve = _a.resolve, reject = _a.reject;
        var status = Status.fromHttpCode(response.status);
        /**
         * "The name is case-insensitive."
         * https://developer.mozilla.org/en-US/docs/Web/API/Headers/get
         */
        var limited = this._handleRateLimit(headers);
        if (limited)
            logger.warn("Too many requests, backing off till: " + this._disabledUntil(requestType));
        if (status === Status.Success) {
            resolve({ status: status });
            return;
        }
        reject(response);
    };
    /**
     * Gets the time that given category is disabled until for rate limiting
     */
    BaseTransport.prototype._disabledUntil = function (category) {
        return this._rateLimits[category] || this._rateLimits.all;
    };
    /**
     * Checks if a category is rate limited
     */
    BaseTransport.prototype._isRateLimited = function (category) {
        return this._disabledUntil(category) > new Date(Date.now());
    };
    /**
     * Sets internal _rateLimits from incoming headers. Returns true if headers contains a non-empty rate limiting header.
     */
    BaseTransport.prototype._handleRateLimit = function (headers) {
        var e_1, _a, e_2, _b;
        var now = Date.now();
        var rlHeader = headers['x-sentry-rate-limits'];
        var raHeader = headers['retry-after'];
        if (rlHeader) {
            try {
                for (var _c = __values(rlHeader.trim().split(',')), _d = _c.next(); !_d.done; _d = _c.next()) {
                    var limit = _d.value;
                    var parameters = limit.split(':', 2);
                    var headerDelay = parseInt(parameters[0], 10);
                    var delay = (!isNaN(headerDelay) ? headerDelay : 60) * 1000; // 60sec default
                    try {
                        for (var _e = (e_2 = void 0, __values(parameters[1].split(';'))), _f = _e.next(); !_f.done; _f = _e.next()) {
                            var category = _f.value;
                            this._rateLimits[category || 'all'] = new Date(now + delay);
                        }
                    }
                    catch (e_2_1) { e_2 = { error: e_2_1 }; }
                    finally {
                        try {
                            if (_f && !_f.done && (_b = _e.return)) _b.call(_e);
                        }
                        finally { if (e_2) throw e_2.error; }
                    }
                }
            }
            catch (e_1_1) { e_1 = { error: e_1_1 }; }
            finally {
                try {
                    if (_d && !_d.done && (_a = _c.return)) _a.call(_c);
                }
                finally { if (e_1) throw e_1.error; }
            }
            return true;
        }
        else if (raHeader) {
            this._rateLimits.all = new Date(now + (0,misc/* parseRetryAfterHeader */.JY)(now, raHeader));
            return true;
        }
        return false;
    };
    return BaseTransport;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/transports/fetch.js




var fetch_global = (0,misc/* getGlobalObject */.Rf)();
/** `fetch` based transport */
var FetchTransport = /** @class */ (function (_super) {
    __extends(FetchTransport, _super);
    function FetchTransport() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    /**
     * @inheritDoc
     */
    FetchTransport.prototype.sendEvent = function (event) {
        return this._sendRequest(eventToSentryRequest(event, this._api), event);
    };
    /**
     * @inheritDoc
     */
    FetchTransport.prototype.sendSession = function (session) {
        return this._sendRequest(sessionToSentryRequest(session, this._api), session);
    };
    /**
     * @param sentryRequest Prepared SentryRequest to be delivered
     * @param originalPayload Original payload used to create SentryRequest
     */
    FetchTransport.prototype._sendRequest = function (sentryRequest, originalPayload) {
        var _this = this;
        if (this._isRateLimited(sentryRequest.type)) {
            return Promise.reject({
                event: originalPayload,
                type: sentryRequest.type,
                reason: "Transport locked till " + this._disabledUntil(sentryRequest.type) + " due to too many requests.",
                status: 429,
            });
        }
        var options = {
            body: sentryRequest.body,
            method: 'POST',
            // Despite all stars in the sky saying that Edge supports old draft syntax, aka 'never', 'always', 'origin' and 'default
            // https://caniuse.com/#feat=referrer-policy
            // It doesn't. And it throw exception instead of ignoring this parameter...
            // REF: https://github.com/getsentry/raven-js/issues/1233
            referrerPolicy: (supportsReferrerPolicy() ? 'origin' : ''),
        };
        if (this.options.fetchParameters !== undefined) {
            Object.assign(options, this.options.fetchParameters);
        }
        if (this.options.headers !== undefined) {
            options.headers = this.options.headers;
        }
        return this._buffer.add(new SyncPromise(function (resolve, reject) {
            fetch_global
                .fetch(sentryRequest.url, options)
                .then(function (response) {
                var headers = {
                    'x-sentry-rate-limits': response.headers.get('X-Sentry-Rate-Limits'),
                    'retry-after': response.headers.get('Retry-After'),
                };
                _this._handleResponse({ requestType: sentryRequest.type, response: response, headers: headers, resolve: resolve, reject: reject });
            })
                .catch(reject);
        }));
    };
    return FetchTransport;
}(BaseTransport));


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/transports/xhr.js




/** `XHR` based transport */
var XHRTransport = /** @class */ (function (_super) {
    __extends(XHRTransport, _super);
    function XHRTransport() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    /**
     * @inheritDoc
     */
    XHRTransport.prototype.sendEvent = function (event) {
        return this._sendRequest(eventToSentryRequest(event, this._api), event);
    };
    /**
     * @inheritDoc
     */
    XHRTransport.prototype.sendSession = function (session) {
        return this._sendRequest(sessionToSentryRequest(session, this._api), session);
    };
    /**
     * @param sentryRequest Prepared SentryRequest to be delivered
     * @param originalPayload Original payload used to create SentryRequest
     */
    XHRTransport.prototype._sendRequest = function (sentryRequest, originalPayload) {
        var _this = this;
        if (this._isRateLimited(sentryRequest.type)) {
            return Promise.reject({
                event: originalPayload,
                type: sentryRequest.type,
                reason: "Transport locked till " + this._disabledUntil(sentryRequest.type) + " due to too many requests.",
                status: 429,
            });
        }
        return this._buffer.add(new SyncPromise(function (resolve, reject) {
            var request = new XMLHttpRequest();
            request.onreadystatechange = function () {
                if (request.readyState === 4) {
                    var headers = {
                        'x-sentry-rate-limits': request.getResponseHeader('X-Sentry-Rate-Limits'),
                        'retry-after': request.getResponseHeader('Retry-After'),
                    };
                    _this._handleResponse({ requestType: sentryRequest.type, response: request, headers: headers, resolve: resolve, reject: reject });
                }
            };
            request.open('POST', sentryRequest.url);
            for (var header in _this.options.headers) {
                if (_this.options.headers.hasOwnProperty(header)) {
                    request.setRequestHeader(header, _this.options.headers[header]);
                }
            }
            request.send(sentryRequest.body);
        }));
    };
    return XHRTransport;
}(BaseTransport));


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/backend.js






/**
 * The Sentry Browser SDK Backend.
 * @hidden
 */
var BrowserBackend = /** @class */ (function (_super) {
    __extends(BrowserBackend, _super);
    function BrowserBackend() {
        return _super !== null && _super.apply(this, arguments) || this;
    }
    /**
     * @inheritDoc
     */
    BrowserBackend.prototype.eventFromException = function (exception, hint) {
        return eventFromException(this._options, exception, hint);
    };
    /**
     * @inheritDoc
     */
    BrowserBackend.prototype.eventFromMessage = function (message, level, hint) {
        if (level === void 0) { level = Severity.Info; }
        return eventFromMessage(this._options, message, level, hint);
    };
    /**
     * @inheritDoc
     */
    BrowserBackend.prototype._setupTransport = function () {
        if (!this._options.dsn) {
            // We return the noop transport here in case there is no Dsn.
            return _super.prototype._setupTransport.call(this);
        }
        var transportOptions = __assign(__assign({}, this._options.transportOptions), { dsn: this._options.dsn });
        if (this._options.transport) {
            return new this._options.transport(transportOptions);
        }
        if (supportsFetch()) {
            return new FetchTransport(transportOptions);
        }
        return new XHRTransport(transportOptions);
    };
    return BrowserBackend;
}(BaseBackend));


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/helpers.js



var ignoreOnError = 0;
/**
 * @hidden
 */
function shouldIgnoreOnError() {
    return ignoreOnError > 0;
}
/**
 * @hidden
 */
function ignoreNextOnError() {
    // onerror should trigger before setTimeout
    ignoreOnError += 1;
    setTimeout(function () {
        ignoreOnError -= 1;
    });
}
/**
 * Instruments the given function and sends an event to Sentry every time the
 * function throws an exception.
 *
 * @param fn A function to wrap.
 * @returns The wrapped function.
 * @hidden
 */
function wrap(fn, options, before) {
    if (options === void 0) { options = {}; }
    if (typeof fn !== 'function') {
        return fn;
    }
    try {
        // We don't wanna wrap it twice
        if (fn.__sentry__) {
            return fn;
        }
        // If this has already been wrapped in the past, return that wrapped function
        if (fn.__sentry_wrapped__) {
            return fn.__sentry_wrapped__;
        }
    }
    catch (e) {
        // Just accessing custom props in some Selenium environments
        // can cause a "Permission denied" exception (see raven-js#495).
        // Bail on wrapping and return the function as-is (defers to window.onerror).
        return fn;
    }
    /* eslint-disable prefer-rest-params */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    var sentryWrapped = function () {
        var args = Array.prototype.slice.call(arguments);
        try {
            if (before && typeof before === 'function') {
                before.apply(this, arguments);
            }
            // eslint-disable-next-line @typescript-eslint/no-explicit-any, @typescript-eslint/no-unsafe-member-access
            var wrappedArguments = args.map(function (arg) { return wrap(arg, options); });
            if (fn.handleEvent) {
                // Attempt to invoke user-land function
                // NOTE: If you are a Sentry user, and you are seeing this stack frame, it
                //       means the sentry.javascript SDK caught an error invoking your application code. This
                //       is expected behavior and NOT indicative of a bug with sentry.javascript.
                // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
                return fn.handleEvent.apply(this, wrappedArguments);
            }
            // Attempt to invoke user-land function
            // NOTE: If you are a Sentry user, and you are seeing this stack frame, it
            //       means the sentry.javascript SDK caught an error invoking your application code. This
            //       is expected behavior and NOT indicative of a bug with sentry.javascript.
            return fn.apply(this, wrappedArguments);
        }
        catch (ex) {
            ignoreNextOnError();
            withScope(function (scope) {
                scope.addEventProcessor(function (event) {
                    var processedEvent = __assign({}, event);
                    if (options.mechanism) {
                        (0,misc/* addExceptionTypeValue */.Db)(processedEvent, undefined, undefined);
                        (0,misc/* addExceptionMechanism */.EG)(processedEvent, options.mechanism);
                    }
                    processedEvent.extra = __assign(__assign({}, processedEvent.extra), { arguments: args });
                    return processedEvent;
                });
                captureException(ex);
            });
            throw ex;
        }
    };
    /* eslint-enable prefer-rest-params */
    // Accessing some objects may throw
    // ref: https://github.com/getsentry/sentry-javascript/issues/1168
    try {
        for (var property in fn) {
            if (Object.prototype.hasOwnProperty.call(fn, property)) {
                sentryWrapped[property] = fn[property];
            }
        }
    }
    catch (_oO) { } // eslint-disable-line no-empty
    fn.prototype = fn.prototype || {};
    sentryWrapped.prototype = fn.prototype;
    Object.defineProperty(fn, '__sentry_wrapped__', {
        enumerable: false,
        value: sentryWrapped,
    });
    // Signal that this function has been wrapped/filled already
    // for both debugging and to prevent it to being wrapped/filled twice
    Object.defineProperties(sentryWrapped, {
        __sentry__: {
            enumerable: false,
            value: true,
        },
        __sentry_original__: {
            enumerable: false,
            value: fn,
        },
    });
    // Restore original function name (not all browsers allow that)
    try {
        var descriptor = Object.getOwnPropertyDescriptor(sentryWrapped, 'name');
        if (descriptor.configurable) {
            Object.defineProperty(sentryWrapped, 'name', {
                get: function () {
                    return fn.name;
                },
            });
        }
        // eslint-disable-next-line no-empty
    }
    catch (_oO) { }
    return sentryWrapped;
}
/**
 * Injects the Report Dialog script
 * @hidden
 */
function injectReportDialog(options) {
    if (options === void 0) { options = {}; }
    if (!options.eventId) {
        logger.error("Missing eventId option in showReportDialog call");
        return;
    }
    if (!options.dsn) {
        logger.error("Missing dsn option in showReportDialog call");
        return;
    }
    var script = document.createElement('script');
    script.async = true;
    script.src = new API(options.dsn).getReportDialogEndpoint(options);
    if (options.onLoad) {
        // eslint-disable-next-line @typescript-eslint/unbound-method
        script.onload = options.onLoad;
    }
    (document.head || document.body).appendChild(script);
}

;// CONCATENATED MODULE: ./node_modules/@sentry/utils/esm/instrument.js







var instrument_global = (0,misc/* getGlobalObject */.Rf)();
/**
 * Instrument native APIs to call handlers that can be used to create breadcrumbs, APM spans etc.
 *  - Console API
 *  - Fetch API
 *  - XHR API
 *  - History API
 *  - DOM API (click/typing)
 *  - Error API
 *  - UnhandledRejection API
 */
var handlers = {};
var instrumented = {};
/** Instruments given API */
function instrument(type) {
    if (instrumented[type]) {
        return;
    }
    instrumented[type] = true;
    switch (type) {
        case 'console':
            instrumentConsole();
            break;
        case 'dom':
            instrumentDOM();
            break;
        case 'xhr':
            instrumentXHR();
            break;
        case 'fetch':
            instrumentFetch();
            break;
        case 'history':
            instrumentHistory();
            break;
        case 'error':
            instrumentError();
            break;
        case 'unhandledrejection':
            instrumentUnhandledRejection();
            break;
        default:
            logger.warn('unknown instrumentation type:', type);
    }
}
/**
 * Add handler that will be called when given type of instrumentation triggers.
 * Use at your own risk, this might break without changelog notice, only used internally.
 * @hidden
 */
function addInstrumentationHandler(handler) {
    if (!handler || typeof handler.type !== 'string' || typeof handler.callback !== 'function') {
        return;
    }
    handlers[handler.type] = handlers[handler.type] || [];
    handlers[handler.type].push(handler.callback);
    instrument(handler.type);
}
/** JSDoc */
function triggerHandlers(type, data) {
    var e_1, _a;
    if (!type || !handlers[type]) {
        return;
    }
    try {
        for (var _b = __values(handlers[type] || []), _c = _b.next(); !_c.done; _c = _b.next()) {
            var handler = _c.value;
            try {
                handler(data);
            }
            catch (e) {
                logger.error("Error while triggering instrumentation handler.\nType: " + type + "\nName: " + getFunctionName(handler) + "\nError: " + e);
            }
        }
    }
    catch (e_1_1) { e_1 = { error: e_1_1 }; }
    finally {
        try {
            if (_c && !_c.done && (_a = _b.return)) _a.call(_b);
        }
        finally { if (e_1) throw e_1.error; }
    }
}
/** JSDoc */
function instrumentConsole() {
    if (!('console' in instrument_global)) {
        return;
    }
    ['debug', 'info', 'warn', 'error', 'log', 'assert'].forEach(function (level) {
        if (!(level in instrument_global.console)) {
            return;
        }
        fill(instrument_global.console, level, function (originalConsoleLevel) {
            return function () {
                var args = [];
                for (var _i = 0; _i < arguments.length; _i++) {
                    args[_i] = arguments[_i];
                }
                triggerHandlers('console', { args: args, level: level });
                // this fails for some browsers. :(
                if (originalConsoleLevel) {
                    Function.prototype.apply.call(originalConsoleLevel, instrument_global.console, args);
                }
            };
        });
    });
}
/** JSDoc */
function instrumentFetch() {
    if (!supportsNativeFetch()) {
        return;
    }
    fill(instrument_global, 'fetch', function (originalFetch) {
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            var handlerData = {
                args: args,
                fetchData: {
                    method: getFetchMethod(args),
                    url: getFetchUrl(args),
                },
                startTimestamp: Date.now(),
            };
            triggerHandlers('fetch', __assign({}, handlerData));
            // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
            return originalFetch.apply(instrument_global, args).then(function (response) {
                triggerHandlers('fetch', __assign(__assign({}, handlerData), { endTimestamp: Date.now(), response: response }));
                return response;
            }, function (error) {
                triggerHandlers('fetch', __assign(__assign({}, handlerData), { endTimestamp: Date.now(), error: error }));
                // NOTE: If you are a Sentry user, and you are seeing this stack frame,
                //       it means the sentry.javascript SDK caught an error invoking your application code.
                //       This is expected behavior and NOT indicative of a bug with sentry.javascript.
                throw error;
            });
        };
    });
}
/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/** Extract `method` from fetch call arguments */
function getFetchMethod(fetchArgs) {
    if (fetchArgs === void 0) { fetchArgs = []; }
    if ('Request' in instrument_global && isInstanceOf(fetchArgs[0], Request) && fetchArgs[0].method) {
        return String(fetchArgs[0].method).toUpperCase();
    }
    if (fetchArgs[1] && fetchArgs[1].method) {
        return String(fetchArgs[1].method).toUpperCase();
    }
    return 'GET';
}
/** Extract `url` from fetch call arguments */
function getFetchUrl(fetchArgs) {
    if (fetchArgs === void 0) { fetchArgs = []; }
    if (typeof fetchArgs[0] === 'string') {
        return fetchArgs[0];
    }
    if ('Request' in instrument_global && isInstanceOf(fetchArgs[0], Request)) {
        return fetchArgs[0].url;
    }
    return String(fetchArgs[0]);
}
/* eslint-enable @typescript-eslint/no-unsafe-member-access */
/** JSDoc */
function instrumentXHR() {
    if (!('XMLHttpRequest' in instrument_global)) {
        return;
    }
    // Poor man's implementation of ES6 `Map`, tracking and keeping in sync key and value separately.
    var requestKeys = [];
    var requestValues = [];
    var xhrproto = XMLHttpRequest.prototype;
    fill(xhrproto, 'open', function (originalOpen) {
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            // eslint-disable-next-line @typescript-eslint/no-this-alias
            var xhr = this;
            var url = args[1];
            xhr.__sentry_xhr__ = {
                // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
                method: isString(args[0]) ? args[0].toUpperCase() : args[0],
                url: args[1],
            };
            // if Sentry key appears in URL, don't capture it as a request
            // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
            if (isString(url) && xhr.__sentry_xhr__.method === 'POST' && url.match(/sentry_key/)) {
                xhr.__sentry_own_request__ = true;
            }
            var onreadystatechangeHandler = function () {
                if (xhr.readyState === 4) {
                    try {
                        // touching statusCode in some platforms throws
                        // an exception
                        if (xhr.__sentry_xhr__) {
                            xhr.__sentry_xhr__.status_code = xhr.status;
                        }
                    }
                    catch (e) {
                        /* do nothing */
                    }
                    try {
                        var requestPos = requestKeys.indexOf(xhr);
                        if (requestPos !== -1) {
                            // Make sure to pop both key and value to keep it in sync.
                            requestKeys.splice(requestPos);
                            var args_1 = requestValues.splice(requestPos)[0];
                            if (xhr.__sentry_xhr__ && args_1[0] !== undefined) {
                                xhr.__sentry_xhr__.body = args_1[0];
                            }
                        }
                    }
                    catch (e) {
                        /* do nothing */
                    }
                    triggerHandlers('xhr', {
                        args: args,
                        endTimestamp: Date.now(),
                        startTimestamp: Date.now(),
                        xhr: xhr,
                    });
                }
            };
            if ('onreadystatechange' in xhr && typeof xhr.onreadystatechange === 'function') {
                fill(xhr, 'onreadystatechange', function (original) {
                    return function () {
                        var readyStateArgs = [];
                        for (var _i = 0; _i < arguments.length; _i++) {
                            readyStateArgs[_i] = arguments[_i];
                        }
                        onreadystatechangeHandler();
                        return original.apply(xhr, readyStateArgs);
                    };
                });
            }
            else {
                xhr.addEventListener('readystatechange', onreadystatechangeHandler);
            }
            return originalOpen.apply(xhr, args);
        };
    });
    fill(xhrproto, 'send', function (originalSend) {
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            requestKeys.push(this);
            requestValues.push(args);
            triggerHandlers('xhr', {
                args: args,
                startTimestamp: Date.now(),
                xhr: this,
            });
            return originalSend.apply(this, args);
        };
    });
}
var lastHref;
/** JSDoc */
function instrumentHistory() {
    if (!supportsHistory()) {
        return;
    }
    var oldOnPopState = instrument_global.onpopstate;
    instrument_global.onpopstate = function () {
        var args = [];
        for (var _i = 0; _i < arguments.length; _i++) {
            args[_i] = arguments[_i];
        }
        var to = instrument_global.location.href;
        // keep track of the current URL state, as we always receive only the updated state
        var from = lastHref;
        lastHref = to;
        triggerHandlers('history', {
            from: from,
            to: to,
        });
        if (oldOnPopState) {
            return oldOnPopState.apply(this, args);
        }
    };
    /** @hidden */
    function historyReplacementFunction(originalHistoryFunction) {
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            var url = args.length > 2 ? args[2] : undefined;
            if (url) {
                // coerce to string (this is what pushState does)
                var from = lastHref;
                var to = String(url);
                // keep track of the current URL state, as we always receive only the updated state
                lastHref = to;
                triggerHandlers('history', {
                    from: from,
                    to: to,
                });
            }
            return originalHistoryFunction.apply(this, args);
        };
    }
    fill(instrument_global.history, 'pushState', historyReplacementFunction);
    fill(instrument_global.history, 'replaceState', historyReplacementFunction);
}
/** JSDoc */
function instrumentDOM() {
    if (!('document' in instrument_global)) {
        return;
    }
    // Capture breadcrumbs from any click that is unhandled / bubbled up all the way
    // to the document. Do this before we instrument addEventListener.
    instrument_global.document.addEventListener('click', domEventHandler('click', triggerHandlers.bind(null, 'dom')), false);
    instrument_global.document.addEventListener('keypress', keypressEventHandler(triggerHandlers.bind(null, 'dom')), false);
    // After hooking into document bubbled up click and keypresses events, we also hook into user handled click & keypresses.
    ['EventTarget', 'Node'].forEach(function (target) {
        /* eslint-disable @typescript-eslint/no-unsafe-member-access */
        var proto = instrument_global[target] && instrument_global[target].prototype;
        // eslint-disable-next-line no-prototype-builtins
        if (!proto || !proto.hasOwnProperty || !proto.hasOwnProperty('addEventListener')) {
            return;
        }
        /* eslint-enable @typescript-eslint/no-unsafe-member-access */
        fill(proto, 'addEventListener', function (original) {
            return function (eventName, fn, options) {
                if (fn && fn.handleEvent) {
                    if (eventName === 'click') {
                        fill(fn, 'handleEvent', function (innerOriginal) {
                            return function (event) {
                                domEventHandler('click', triggerHandlers.bind(null, 'dom'))(event);
                                return innerOriginal.call(this, event);
                            };
                        });
                    }
                    if (eventName === 'keypress') {
                        fill(fn, 'handleEvent', function (innerOriginal) {
                            return function (event) {
                                keypressEventHandler(triggerHandlers.bind(null, 'dom'))(event);
                                return innerOriginal.call(this, event);
                            };
                        });
                    }
                }
                else {
                    if (eventName === 'click') {
                        domEventHandler('click', triggerHandlers.bind(null, 'dom'), true)(this);
                    }
                    if (eventName === 'keypress') {
                        keypressEventHandler(triggerHandlers.bind(null, 'dom'))(this);
                    }
                }
                return original.call(this, eventName, fn, options);
            };
        });
        fill(proto, 'removeEventListener', function (original) {
            return function (eventName, fn, options) {
                try {
                    original.call(this, eventName, fn.__sentry_wrapped__, options);
                }
                catch (e) {
                    // ignore, accessing __sentry_wrapped__ will throw in some Selenium environments
                }
                return original.call(this, eventName, fn, options);
            };
        });
    });
}
var debounceDuration = 1000;
var debounceTimer = 0;
var keypressTimeout;
var lastCapturedEvent;
/**
 * Wraps addEventListener to capture UI breadcrumbs
 * @param name the event name (e.g. "click")
 * @param handler function that will be triggered
 * @param debounce decides whether it should wait till another event loop
 * @returns wrapped breadcrumb events handler
 * @hidden
 */
function domEventHandler(name, handler, debounce) {
    if (debounce === void 0) { debounce = false; }
    return function (event) {
        // reset keypress timeout; e.g. triggering a 'click' after
        // a 'keypress' will reset the keypress debounce so that a new
        // set of keypresses can be recorded
        keypressTimeout = undefined;
        // It's possible this handler might trigger multiple times for the same
        // event (e.g. event propagation through node ancestors). Ignore if we've
        // already captured the event.
        if (!event || lastCapturedEvent === event) {
            return;
        }
        lastCapturedEvent = event;
        if (debounceTimer) {
            clearTimeout(debounceTimer);
        }
        if (debounce) {
            debounceTimer = setTimeout(function () {
                handler({ event: event, name: name });
            });
        }
        else {
            handler({ event: event, name: name });
        }
    };
}
/**
 * Wraps addEventListener to capture keypress UI events
 * @param handler function that will be triggered
 * @returns wrapped keypress events handler
 * @hidden
 */
function keypressEventHandler(handler) {
    // TODO: if somehow user switches keypress target before
    //       debounce timeout is triggered, we will only capture
    //       a single breadcrumb from the FIRST target (acceptable?)
    return function (event) {
        var target;
        try {
            target = event.target;
        }
        catch (e) {
            // just accessing event properties can throw an exception in some rare circumstances
            // see: https://github.com/getsentry/raven-js/issues/838
            return;
        }
        var tagName = target && target.tagName;
        // only consider keypress events on actual input elements
        // this will disregard keypresses targeting body (e.g. tabbing
        // through elements, hotkeys, etc)
        if (!tagName || (tagName !== 'INPUT' && tagName !== 'TEXTAREA' && !target.isContentEditable)) {
            return;
        }
        // record first keypress in a series, but ignore subsequent
        // keypresses until debounce clears
        if (!keypressTimeout) {
            domEventHandler('input', handler)(event);
        }
        clearTimeout(keypressTimeout);
        keypressTimeout = setTimeout(function () {
            keypressTimeout = undefined;
        }, debounceDuration);
    };
}
var _oldOnErrorHandler = null;
/** JSDoc */
function instrumentError() {
    _oldOnErrorHandler = instrument_global.onerror;
    instrument_global.onerror = function (msg, url, line, column, error) {
        triggerHandlers('error', {
            column: column,
            error: error,
            line: line,
            msg: msg,
            url: url,
        });
        if (_oldOnErrorHandler) {
            // eslint-disable-next-line prefer-rest-params
            return _oldOnErrorHandler.apply(this, arguments);
        }
        return false;
    };
}
var _oldOnUnhandledRejectionHandler = null;
/** JSDoc */
function instrumentUnhandledRejection() {
    _oldOnUnhandledRejectionHandler = instrument_global.onunhandledrejection;
    instrument_global.onunhandledrejection = function (e) {
        triggerHandlers('unhandledrejection', e);
        if (_oldOnUnhandledRejectionHandler) {
            // eslint-disable-next-line prefer-rest-params
            return _oldOnUnhandledRejectionHandler.apply(this, arguments);
        }
        return true;
    };
}

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/breadcrumbs.js

/* eslint-disable @typescript-eslint/no-unsafe-member-access */
/* eslint-disable max-lines */



/**
 * Default Breadcrumbs instrumentations
 * TODO: Deprecated - with v6, this will be renamed to `Instrument`
 */
var Breadcrumbs = /** @class */ (function () {
    /**
     * @inheritDoc
     */
    function Breadcrumbs(options) {
        /**
         * @inheritDoc
         */
        this.name = Breadcrumbs.id;
        this._options = __assign({ console: true, dom: true, fetch: true, history: true, sentry: true, xhr: true }, options);
    }
    /**
     * Create a breadcrumb of `sentry` from the events themselves
     */
    Breadcrumbs.prototype.addSentryBreadcrumb = function (event) {
        if (!this._options.sentry) {
            return;
        }
        getCurrentHub().addBreadcrumb({
            category: "sentry." + (event.type === 'transaction' ? 'transaction' : 'event'),
            event_id: event.event_id,
            level: event.level,
            message: (0,misc/* getEventDescription */.jH)(event),
        }, {
            event: event,
        });
    };
    /**
     * Instrument browser built-ins w/ breadcrumb capturing
     *  - Console API
     *  - DOM API (click/typing)
     *  - XMLHttpRequest API
     *  - Fetch API
     *  - History API
     */
    Breadcrumbs.prototype.setupOnce = function () {
        var _this = this;
        if (this._options.console) {
            addInstrumentationHandler({
                callback: function () {
                    var args = [];
                    for (var _i = 0; _i < arguments.length; _i++) {
                        args[_i] = arguments[_i];
                    }
                    _this._consoleBreadcrumb.apply(_this, tslib_es6_spread(args));
                },
                type: 'console',
            });
        }
        if (this._options.dom) {
            addInstrumentationHandler({
                callback: function () {
                    var args = [];
                    for (var _i = 0; _i < arguments.length; _i++) {
                        args[_i] = arguments[_i];
                    }
                    _this._domBreadcrumb.apply(_this, tslib_es6_spread(args));
                },
                type: 'dom',
            });
        }
        if (this._options.xhr) {
            addInstrumentationHandler({
                callback: function () {
                    var args = [];
                    for (var _i = 0; _i < arguments.length; _i++) {
                        args[_i] = arguments[_i];
                    }
                    _this._xhrBreadcrumb.apply(_this, tslib_es6_spread(args));
                },
                type: 'xhr',
            });
        }
        if (this._options.fetch) {
            addInstrumentationHandler({
                callback: function () {
                    var args = [];
                    for (var _i = 0; _i < arguments.length; _i++) {
                        args[_i] = arguments[_i];
                    }
                    _this._fetchBreadcrumb.apply(_this, tslib_es6_spread(args));
                },
                type: 'fetch',
            });
        }
        if (this._options.history) {
            addInstrumentationHandler({
                callback: function () {
                    var args = [];
                    for (var _i = 0; _i < arguments.length; _i++) {
                        args[_i] = arguments[_i];
                    }
                    _this._historyBreadcrumb.apply(_this, tslib_es6_spread(args));
                },
                type: 'history',
            });
        }
    };
    /**
     * Creates breadcrumbs from console API calls
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Breadcrumbs.prototype._consoleBreadcrumb = function (handlerData) {
        var breadcrumb = {
            category: 'console',
            data: {
                arguments: handlerData.args,
                logger: 'console',
            },
            level: Severity.fromString(handlerData.level),
            message: safeJoin(handlerData.args, ' '),
        };
        if (handlerData.level === 'assert') {
            if (handlerData.args[0] === false) {
                breadcrumb.message = "Assertion failed: " + (safeJoin(handlerData.args.slice(1), ' ') || 'console.assert');
                breadcrumb.data.arguments = handlerData.args.slice(1);
            }
            else {
                // Don't capture a breadcrumb for passed assertions
                return;
            }
        }
        getCurrentHub().addBreadcrumb(breadcrumb, {
            input: handlerData.args,
            level: handlerData.level,
        });
    };
    /**
     * Creates breadcrumbs from DOM API calls
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Breadcrumbs.prototype._domBreadcrumb = function (handlerData) {
        var target;
        // Accessing event.target can throw (see getsentry/raven-js#838, #768)
        try {
            target = handlerData.event.target
                ? htmlTreeAsString(handlerData.event.target)
                : htmlTreeAsString(handlerData.event);
        }
        catch (e) {
            target = '<unknown>';
        }
        if (target.length === 0) {
            return;
        }
        getCurrentHub().addBreadcrumb({
            category: "ui." + handlerData.name,
            message: target,
        }, {
            event: handlerData.event,
            name: handlerData.name,
        });
    };
    /**
     * Creates breadcrumbs from XHR API calls
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Breadcrumbs.prototype._xhrBreadcrumb = function (handlerData) {
        if (handlerData.endTimestamp) {
            // We only capture complete, non-sentry requests
            if (handlerData.xhr.__sentry_own_request__) {
                return;
            }
            var _a = handlerData.xhr.__sentry_xhr__ || {}, method = _a.method, url = _a.url, status_code = _a.status_code, body = _a.body;
            getCurrentHub().addBreadcrumb({
                category: 'xhr',
                data: {
                    method: method,
                    url: url,
                    status_code: status_code,
                },
                type: 'http',
            }, {
                xhr: handlerData.xhr,
                input: body,
            });
            return;
        }
    };
    /**
     * Creates breadcrumbs from fetch API calls
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Breadcrumbs.prototype._fetchBreadcrumb = function (handlerData) {
        // We only capture complete fetch requests
        if (!handlerData.endTimestamp) {
            return;
        }
        if (handlerData.fetchData.url.match(/sentry_key/) && handlerData.fetchData.method === 'POST') {
            // We will not create breadcrumbs for fetch requests that contain `sentry_key` (internal sentry requests)
            return;
        }
        if (handlerData.error) {
            getCurrentHub().addBreadcrumb({
                category: 'fetch',
                data: handlerData.fetchData,
                level: Severity.Error,
                type: 'http',
            }, {
                data: handlerData.error,
                input: handlerData.args,
            });
        }
        else {
            getCurrentHub().addBreadcrumb({
                category: 'fetch',
                data: __assign(__assign({}, handlerData.fetchData), { status_code: handlerData.response.status }),
                type: 'http',
            }, {
                input: handlerData.args,
                response: handlerData.response,
            });
        }
    };
    /**
     * Creates breadcrumbs from history API calls
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    Breadcrumbs.prototype._historyBreadcrumb = function (handlerData) {
        var global = (0,misc/* getGlobalObject */.Rf)();
        var from = handlerData.from;
        var to = handlerData.to;
        var parsedLoc = (0,misc/* parseUrl */.en)(global.location.href);
        var parsedFrom = (0,misc/* parseUrl */.en)(from);
        var parsedTo = (0,misc/* parseUrl */.en)(to);
        // Initial pushState doesn't provide `from` information
        if (!parsedFrom.path) {
            parsedFrom = parsedLoc;
        }
        // Use only the path component of the URL if the URL matches the current
        // document (almost all the time when using pushState)
        if (parsedLoc.protocol === parsedTo.protocol && parsedLoc.host === parsedTo.host) {
            to = parsedTo.relative;
        }
        if (parsedLoc.protocol === parsedFrom.protocol && parsedLoc.host === parsedFrom.host) {
            from = parsedFrom.relative;
        }
        getCurrentHub().addBreadcrumb({
            category: 'navigation',
            data: {
                from: from,
                to: to,
            },
        });
    };
    /**
     * @inheritDoc
     */
    Breadcrumbs.id = 'Breadcrumbs';
    return Breadcrumbs;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/version.js
var SDK_NAME = 'sentry.javascript.browser';
var SDK_VERSION = '5.27.2';

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/client.js







/**
 * The Sentry Browser SDK Client.
 *
 * @see BrowserOptions for documentation on configuration options.
 * @see SentryClient for usage documentation.
 */
var BrowserClient = /** @class */ (function (_super) {
    __extends(BrowserClient, _super);
    /**
     * Creates a new Browser SDK instance.
     *
     * @param options Configuration options for this SDK.
     */
    function BrowserClient(options) {
        if (options === void 0) { options = {}; }
        return _super.call(this, BrowserBackend, options) || this;
    }
    /**
     * Show a report dialog to the user to send feedback to a specific event.
     *
     * @param options Set individual options for the dialog
     */
    BrowserClient.prototype.showReportDialog = function (options) {
        if (options === void 0) { options = {}; }
        // doesn't work without a document (React Native)
        var document = (0,misc/* getGlobalObject */.Rf)().document;
        if (!document) {
            return;
        }
        if (!this._isEnabled()) {
            logger.error('Trying to call showReportDialog with Sentry Client disabled');
            return;
        }
        injectReportDialog(__assign(__assign({}, options), { dsn: options.dsn || this.getDsn() }));
    };
    /**
     * @inheritDoc
     */
    BrowserClient.prototype._prepareEvent = function (event, scope, hint) {
        event.platform = event.platform || 'javascript';
        event.sdk = __assign(__assign({}, event.sdk), { name: SDK_NAME, packages: tslib_es6_spread(((event.sdk && event.sdk.packages) || []), [
                {
                    name: 'npm:@sentry/browser',
                    version: SDK_VERSION,
                },
            ]), version: SDK_VERSION });
        return _super.prototype._prepareEvent.call(this, event, scope, hint);
    };
    /**
     * @inheritDoc
     */
    BrowserClient.prototype._sendEvent = function (event) {
        var integration = this.getIntegration(Breadcrumbs);
        if (integration) {
            integration.addSentryBreadcrumb(event);
        }
        _super.prototype._sendEvent.call(this, event);
    };
    return BrowserClient;
}(BaseClient));


;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/sdk.js


/**
 * Internal function to create a new SDK client instance. The client is
 * installed and then bound to the current scope.
 *
 * @param clientClass The client class to instantiate.
 * @param options Options to pass to the client.
 */
function initAndBind(clientClass, options) {
    if (options.debug === true) {
        logger.enable();
    }
    var hub = getCurrentHub();
    var client = new clientClass(options);
    hub.bindClient(client);
}

;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/integrations/inboundfilters.js



// "Script error." is hard coded into browsers for errors that it can't read.
// this is the result of a script being pulled in from an external domain and CORS.
var DEFAULT_IGNORE_ERRORS = [/^Script error\.?$/, /^Javascript error: Script error\.? on line 0$/];
/** Inbound filters configurable by the user */
var InboundFilters = /** @class */ (function () {
    function InboundFilters(_options) {
        if (_options === void 0) { _options = {}; }
        this._options = _options;
        /**
         * @inheritDoc
         */
        this.name = InboundFilters.id;
    }
    /**
     * @inheritDoc
     */
    InboundFilters.prototype.setupOnce = function () {
        addGlobalEventProcessor(function (event) {
            var hub = getCurrentHub();
            if (!hub) {
                return event;
            }
            var self = hub.getIntegration(InboundFilters);
            if (self) {
                var client = hub.getClient();
                var clientOptions = client ? client.getOptions() : {};
                var options = self._mergeOptions(clientOptions);
                if (self._shouldDropEvent(event, options)) {
                    return null;
                }
            }
            return event;
        });
    };
    /** JSDoc */
    InboundFilters.prototype._shouldDropEvent = function (event, options) {
        if (this._isSentryError(event, options)) {
            logger.warn("Event dropped due to being internal Sentry Error.\nEvent: " + (0,misc/* getEventDescription */.jH)(event));
            return true;
        }
        if (this._isIgnoredError(event, options)) {
            logger.warn("Event dropped due to being matched by `ignoreErrors` option.\nEvent: " + (0,misc/* getEventDescription */.jH)(event));
            return true;
        }
        if (this._isDeniedUrl(event, options)) {
            logger.warn("Event dropped due to being matched by `denyUrls` option.\nEvent: " + (0,misc/* getEventDescription */.jH)(event) + ".\nUrl: " + this._getEventFilterUrl(event));
            return true;
        }
        if (!this._isAllowedUrl(event, options)) {
            logger.warn("Event dropped due to not being matched by `allowUrls` option.\nEvent: " + (0,misc/* getEventDescription */.jH)(event) + ".\nUrl: " + this._getEventFilterUrl(event));
            return true;
        }
        return false;
    };
    /** JSDoc */
    InboundFilters.prototype._isSentryError = function (event, options) {
        if (!options.ignoreInternal) {
            return false;
        }
        try {
            return ((event &&
                event.exception &&
                event.exception.values &&
                event.exception.values[0] &&
                event.exception.values[0].type === 'SentryError') ||
                false);
        }
        catch (_oO) {
            return false;
        }
    };
    /** JSDoc */
    InboundFilters.prototype._isIgnoredError = function (event, options) {
        if (!options.ignoreErrors || !options.ignoreErrors.length) {
            return false;
        }
        return this._getPossibleEventMessages(event).some(function (message) {
            // Not sure why TypeScript complains here...
            return options.ignoreErrors.some(function (pattern) { return isMatchingPattern(message, pattern); });
        });
    };
    /** JSDoc */
    InboundFilters.prototype._isDeniedUrl = function (event, options) {
        // TODO: Use Glob instead?
        if (!options.denyUrls || !options.denyUrls.length) {
            return false;
        }
        var url = this._getEventFilterUrl(event);
        return !url ? false : options.denyUrls.some(function (pattern) { return isMatchingPattern(url, pattern); });
    };
    /** JSDoc */
    InboundFilters.prototype._isAllowedUrl = function (event, options) {
        // TODO: Use Glob instead?
        if (!options.allowUrls || !options.allowUrls.length) {
            return true;
        }
        var url = this._getEventFilterUrl(event);
        return !url ? true : options.allowUrls.some(function (pattern) { return isMatchingPattern(url, pattern); });
    };
    /** JSDoc */
    InboundFilters.prototype._mergeOptions = function (clientOptions) {
        if (clientOptions === void 0) { clientOptions = {}; }
        return {
            allowUrls: tslib_es6_spread(([]), (this._options.allowUrls || []), ([]), (clientOptions.allowUrls || [])),
            denyUrls: tslib_es6_spread(([]), (this._options.denyUrls || []), ([]), (clientOptions.denyUrls || [])),
            ignoreErrors: tslib_es6_spread(([]), (clientOptions.ignoreErrors || []), DEFAULT_IGNORE_ERRORS),
            ignoreInternal: typeof this._options.ignoreInternal !== 'undefined' ? this._options.ignoreInternal : true,
        };
    };
    /** JSDoc */
    InboundFilters.prototype._getPossibleEventMessages = function (event) {
        if (event.message) {
            return [event.message];
        }
        if (event.exception) {
            try {
                var _a = (event.exception.values && event.exception.values[0]) || {}, _b = _a.type, type = _b === void 0 ? '' : _b, _c = _a.value, value = _c === void 0 ? '' : _c;
                return ["" + value, type + ": " + value];
            }
            catch (oO) {
                logger.error("Cannot extract message for event " + (0,misc/* getEventDescription */.jH)(event));
                return [];
            }
        }
        return [];
    };
    /** JSDoc */
    InboundFilters.prototype._getEventFilterUrl = function (event) {
        try {
            if (event.stacktrace) {
                var frames_1 = event.stacktrace.frames;
                return (frames_1 && frames_1[frames_1.length - 1].filename) || null;
            }
            if (event.exception) {
                var frames_2 = event.exception.values && event.exception.values[0].stacktrace && event.exception.values[0].stacktrace.frames;
                return (frames_2 && frames_2[frames_2.length - 1].filename) || null;
            }
            return null;
        }
        catch (oO) {
            logger.error("Cannot extract url for event " + (0,misc/* getEventDescription */.jH)(event));
            return null;
        }
    };
    /**
     * @inheritDoc
     */
    InboundFilters.id = 'InboundFilters';
    return InboundFilters;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/integrations/functiontostring.js
var originalFunctionToString;
/** Patch toString calls to return proper name for wrapped functions */
var FunctionToString = /** @class */ (function () {
    function FunctionToString() {
        /**
         * @inheritDoc
         */
        this.name = FunctionToString.id;
    }
    /**
     * @inheritDoc
     */
    FunctionToString.prototype.setupOnce = function () {
        // eslint-disable-next-line @typescript-eslint/unbound-method
        originalFunctionToString = Function.prototype.toString;
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        Function.prototype.toString = function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            var context = this.__sentry_original__ || this;
            return originalFunctionToString.apply(context, args);
        };
    };
    /**
     * @inheritDoc
     */
    FunctionToString.id = 'FunctionToString';
    return FunctionToString;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/trycatch.js



var DEFAULT_EVENT_TARGET = [
    'EventTarget',
    'Window',
    'Node',
    'ApplicationCache',
    'AudioTrackList',
    'ChannelMergerNode',
    'CryptoOperation',
    'EventSource',
    'FileReader',
    'HTMLUnknownElement',
    'IDBDatabase',
    'IDBRequest',
    'IDBTransaction',
    'KeyOperation',
    'MediaController',
    'MessagePort',
    'ModalWindow',
    'Notification',
    'SVGElementInstance',
    'Screen',
    'TextTrack',
    'TextTrackCue',
    'TextTrackList',
    'WebSocket',
    'WebSocketWorker',
    'Worker',
    'XMLHttpRequest',
    'XMLHttpRequestEventTarget',
    'XMLHttpRequestUpload',
];
/** Wrap timer functions and event targets to catch errors and provide better meta data */
var TryCatch = /** @class */ (function () {
    /**
     * @inheritDoc
     */
    function TryCatch(options) {
        /**
         * @inheritDoc
         */
        this.name = TryCatch.id;
        this._options = __assign({ XMLHttpRequest: true, eventTarget: true, requestAnimationFrame: true, setInterval: true, setTimeout: true }, options);
    }
    /**
     * Wrap timer functions and event targets to catch errors
     * and provide better metadata.
     */
    TryCatch.prototype.setupOnce = function () {
        var global = (0,misc/* getGlobalObject */.Rf)();
        if (this._options.setTimeout) {
            fill(global, 'setTimeout', this._wrapTimeFunction.bind(this));
        }
        if (this._options.setInterval) {
            fill(global, 'setInterval', this._wrapTimeFunction.bind(this));
        }
        if (this._options.requestAnimationFrame) {
            fill(global, 'requestAnimationFrame', this._wrapRAF.bind(this));
        }
        if (this._options.XMLHttpRequest && 'XMLHttpRequest' in global) {
            fill(XMLHttpRequest.prototype, 'send', this._wrapXHR.bind(this));
        }
        if (this._options.eventTarget) {
            var eventTarget = Array.isArray(this._options.eventTarget) ? this._options.eventTarget : DEFAULT_EVENT_TARGET;
            eventTarget.forEach(this._wrapEventTarget.bind(this));
        }
    };
    /** JSDoc */
    TryCatch.prototype._wrapTimeFunction = function (original) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            var originalCallback = args[0];
            args[0] = wrap(originalCallback, {
                mechanism: {
                    data: { function: getFunctionName(original) },
                    handled: true,
                    type: 'instrument',
                },
            });
            return original.apply(this, args);
        };
    };
    /** JSDoc */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    TryCatch.prototype._wrapRAF = function (original) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return function (callback) {
            // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
            return original.call(this, wrap(callback, {
                mechanism: {
                    data: {
                        function: 'requestAnimationFrame',
                        handler: getFunctionName(original),
                    },
                    handled: true,
                    type: 'instrument',
                },
            }));
        };
    };
    /** JSDoc */
    TryCatch.prototype._wrapEventTarget = function (target) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        var global = (0,misc/* getGlobalObject */.Rf)();
        // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
        var proto = global[target] && global[target].prototype;
        // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
        if (!proto || !proto.hasOwnProperty || !proto.hasOwnProperty('addEventListener')) {
            return;
        }
        fill(proto, 'addEventListener', function (original) {
            return function (eventName, fn, options) {
                try {
                    if (typeof fn.handleEvent === 'function') {
                        fn.handleEvent = wrap(fn.handleEvent.bind(fn), {
                            mechanism: {
                                data: {
                                    function: 'handleEvent',
                                    handler: getFunctionName(fn),
                                    target: target,
                                },
                                handled: true,
                                type: 'instrument',
                            },
                        });
                    }
                }
                catch (err) {
                    // can sometimes get 'Permission denied to access property "handle Event'
                }
                return original.call(this, eventName,
                // eslint-disable-next-line @typescript-eslint/no-explicit-any
                wrap(fn, {
                    mechanism: {
                        data: {
                            function: 'addEventListener',
                            handler: getFunctionName(fn),
                            target: target,
                        },
                        handled: true,
                        type: 'instrument',
                    },
                }), options);
            };
        });
        fill(proto, 'removeEventListener', function (original) {
            return function (eventName, fn, options) {
                /**
                 * There are 2 possible scenarios here:
                 *
                 * 1. Someone passes a callback, which was attached prior to Sentry initialization, or by using unmodified
                 * method, eg. `document.addEventListener.call(el, name, handler). In this case, we treat this function
                 * as a pass-through, and call original `removeEventListener` with it.
                 *
                 * 2. Someone passes a callback, which was attached after Sentry was initialized, which means that it was using
                 * our wrapped version of `addEventListener`, which internally calls `wrap` helper.
                 * This helper "wraps" whole callback inside a try/catch statement, and attached appropriate metadata to it,
                 * in order for us to make a distinction between wrapped/non-wrapped functions possible.
                 * If a function was wrapped, it has additional property of `__sentry_wrapped__`, holding the handler.
                 *
                 * When someone adds a handler prior to initialization, and then do it again, but after,
                 * then we have to detach both of them. Otherwise, if we'd detach only wrapped one, it'd be impossible
                 * to get rid of the initial handler and it'd stick there forever.
                 */
                try {
                    original.call(this, eventName, fn.__sentry_wrapped__, options);
                }
                catch (e) {
                    // ignore, accessing __sentry_wrapped__ will throw in some Selenium environments
                }
                return original.call(this, eventName, fn, options);
            };
        });
    };
    /** JSDoc */
    TryCatch.prototype._wrapXHR = function (originalSend) {
        // eslint-disable-next-line @typescript-eslint/no-explicit-any
        return function () {
            var args = [];
            for (var _i = 0; _i < arguments.length; _i++) {
                args[_i] = arguments[_i];
            }
            // eslint-disable-next-line @typescript-eslint/no-this-alias
            var xhr = this;
            var xmlHttpRequestProps = ['onload', 'onerror', 'onprogress', 'onreadystatechange'];
            xmlHttpRequestProps.forEach(function (prop) {
                if (prop in xhr && typeof xhr[prop] === 'function') {
                    // eslint-disable-next-line @typescript-eslint/no-explicit-any
                    fill(xhr, prop, function (original) {
                        var wrapOptions = {
                            mechanism: {
                                data: {
                                    function: prop,
                                    handler: getFunctionName(original),
                                },
                                handled: true,
                                type: 'instrument',
                            },
                        };
                        // If Instrument integration has been called before TryCatch, get the name of original function
                        if (original.__sentry_original__) {
                            wrapOptions.mechanism.data.handler = getFunctionName(original.__sentry_original__);
                        }
                        // Otherwise wrap directly
                        return wrap(original, wrapOptions);
                    });
                }
            });
            return originalSend.apply(this, args);
        };
    };
    /**
     * @inheritDoc
     */
    TryCatch.id = 'TryCatch';
    return TryCatch;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/globalhandlers.js

/* eslint-disable @typescript-eslint/no-unsafe-member-access */





/** Global handlers */
var GlobalHandlers = /** @class */ (function () {
    /** JSDoc */
    function GlobalHandlers(options) {
        /**
         * @inheritDoc
         */
        this.name = GlobalHandlers.id;
        /** JSDoc */
        this._onErrorHandlerInstalled = false;
        /** JSDoc */
        this._onUnhandledRejectionHandlerInstalled = false;
        this._options = __assign({ onerror: true, onunhandledrejection: true }, options);
    }
    /**
     * @inheritDoc
     */
    GlobalHandlers.prototype.setupOnce = function () {
        Error.stackTraceLimit = 50;
        if (this._options.onerror) {
            logger.log('Global Handler attached: onerror');
            this._installGlobalOnErrorHandler();
        }
        if (this._options.onunhandledrejection) {
            logger.log('Global Handler attached: onunhandledrejection');
            this._installGlobalOnUnhandledRejectionHandler();
        }
    };
    /** JSDoc */
    GlobalHandlers.prototype._installGlobalOnErrorHandler = function () {
        var _this = this;
        if (this._onErrorHandlerInstalled) {
            return;
        }
        addInstrumentationHandler({
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            callback: function (data) {
                var error = data.error;
                var currentHub = getCurrentHub();
                var hasIntegration = currentHub.getIntegration(GlobalHandlers);
                var isFailedOwnDelivery = error && error.__sentry_own_request__ === true;
                if (!hasIntegration || shouldIgnoreOnError() || isFailedOwnDelivery) {
                    return;
                }
                var client = currentHub.getClient();
                var event = isPrimitive(error)
                    ? _this._eventFromIncompleteOnError(data.msg, data.url, data.line, data.column)
                    : _this._enhanceEventWithInitialFrame(eventFromUnknownInput(error, undefined, {
                        attachStacktrace: client && client.getOptions().attachStacktrace,
                        rejection: false,
                    }), data.url, data.line, data.column);
                (0,misc/* addExceptionMechanism */.EG)(event, {
                    handled: false,
                    type: 'onerror',
                });
                currentHub.captureEvent(event, {
                    originalException: error,
                });
            },
            type: 'error',
        });
        this._onErrorHandlerInstalled = true;
    };
    /** JSDoc */
    GlobalHandlers.prototype._installGlobalOnUnhandledRejectionHandler = function () {
        var _this = this;
        if (this._onUnhandledRejectionHandlerInstalled) {
            return;
        }
        addInstrumentationHandler({
            // eslint-disable-next-line @typescript-eslint/no-explicit-any
            callback: function (e) {
                var error = e;
                // dig the object of the rejection out of known event types
                try {
                    // PromiseRejectionEvents store the object of the rejection under 'reason'
                    // see https://developer.mozilla.org/en-US/docs/Web/API/PromiseRejectionEvent
                    if ('reason' in e) {
                        error = e.reason;
                    }
                    // something, somewhere, (likely a browser extension) effectively casts PromiseRejectionEvents
                    // to CustomEvents, moving the `promise` and `reason` attributes of the PRE into
                    // the CustomEvent's `detail` attribute, since they're not part of CustomEvent's spec
                    // see https://developer.mozilla.org/en-US/docs/Web/API/CustomEvent and
                    // https://github.com/getsentry/sentry-javascript/issues/2380
                    else if ('detail' in e && 'reason' in e.detail) {
                        error = e.detail.reason;
                    }
                }
                catch (_oO) {
                    // no-empty
                }
                var currentHub = getCurrentHub();
                var hasIntegration = currentHub.getIntegration(GlobalHandlers);
                var isFailedOwnDelivery = error && error.__sentry_own_request__ === true;
                if (!hasIntegration || shouldIgnoreOnError() || isFailedOwnDelivery) {
                    return true;
                }
                var client = currentHub.getClient();
                var event = isPrimitive(error)
                    ? _this._eventFromIncompleteRejection(error)
                    : eventFromUnknownInput(error, undefined, {
                        attachStacktrace: client && client.getOptions().attachStacktrace,
                        rejection: true,
                    });
                event.level = Severity.Error;
                (0,misc/* addExceptionMechanism */.EG)(event, {
                    handled: false,
                    type: 'onunhandledrejection',
                });
                currentHub.captureEvent(event, {
                    originalException: error,
                });
                return;
            },
            type: 'unhandledrejection',
        });
        this._onUnhandledRejectionHandlerInstalled = true;
    };
    /**
     * This function creates a stack from an old, error-less onerror handler.
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    GlobalHandlers.prototype._eventFromIncompleteOnError = function (msg, url, line, column) {
        var ERROR_TYPES_RE = /^(?:[Uu]ncaught (?:exception: )?)?(?:((?:Eval|Internal|Range|Reference|Syntax|Type|URI|)Error): )?(.*)$/i;
        // If 'message' is ErrorEvent, get real message from inside
        var message = isErrorEvent(msg) ? msg.message : msg;
        var name;
        if (isString(message)) {
            var groups = message.match(ERROR_TYPES_RE);
            if (groups) {
                name = groups[1];
                message = groups[2];
            }
        }
        var event = {
            exception: {
                values: [
                    {
                        type: name || 'Error',
                        value: message,
                    },
                ],
            },
        };
        return this._enhanceEventWithInitialFrame(event, url, line, column);
    };
    /**
     * This function creates an Event from an TraceKitStackTrace that has part of it missing.
     */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    GlobalHandlers.prototype._eventFromIncompleteRejection = function (error) {
        return {
            exception: {
                values: [
                    {
                        type: 'UnhandledRejection',
                        value: "Non-Error promise rejection captured with value: " + error,
                    },
                ],
            },
        };
    };
    /** JSDoc */
    // eslint-disable-next-line @typescript-eslint/no-explicit-any
    GlobalHandlers.prototype._enhanceEventWithInitialFrame = function (event, url, line, column) {
        event.exception = event.exception || {};
        event.exception.values = event.exception.values || [];
        event.exception.values[0] = event.exception.values[0] || {};
        event.exception.values[0].stacktrace = event.exception.values[0].stacktrace || {};
        event.exception.values[0].stacktrace.frames = event.exception.values[0].stacktrace.frames || [];
        var colno = isNaN(parseInt(column, 10)) ? undefined : column;
        var lineno = isNaN(parseInt(line, 10)) ? undefined : line;
        var filename = isString(url) && url.length > 0 ? url : (0,misc/* getLocationHref */.l4)();
        if (event.exception.values[0].stacktrace.frames.length === 0) {
            event.exception.values[0].stacktrace.frames.push({
                colno: colno,
                filename: filename,
                function: '?',
                in_app: true,
                lineno: lineno,
            });
        }
        return event;
    };
    /**
     * @inheritDoc
     */
    GlobalHandlers.id = 'GlobalHandlers';
    return GlobalHandlers;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/linkederrors.js





var DEFAULT_KEY = 'cause';
var DEFAULT_LIMIT = 5;
/** Adds SDK info to an event. */
var LinkedErrors = /** @class */ (function () {
    /**
     * @inheritDoc
     */
    function LinkedErrors(options) {
        if (options === void 0) { options = {}; }
        /**
         * @inheritDoc
         */
        this.name = LinkedErrors.id;
        this._key = options.key || DEFAULT_KEY;
        this._limit = options.limit || DEFAULT_LIMIT;
    }
    /**
     * @inheritDoc
     */
    LinkedErrors.prototype.setupOnce = function () {
        addGlobalEventProcessor(function (event, hint) {
            var self = getCurrentHub().getIntegration(LinkedErrors);
            if (self) {
                return self._handler(event, hint);
            }
            return event;
        });
    };
    /**
     * @inheritDoc
     */
    LinkedErrors.prototype._handler = function (event, hint) {
        if (!event.exception || !event.exception.values || !hint || !isInstanceOf(hint.originalException, Error)) {
            return event;
        }
        var linkedErrors = this._walkErrorTree(hint.originalException, this._key);
        event.exception.values = tslib_es6_spread(linkedErrors, event.exception.values);
        return event;
    };
    /**
     * @inheritDoc
     */
    LinkedErrors.prototype._walkErrorTree = function (error, key, stack) {
        if (stack === void 0) { stack = []; }
        if (!isInstanceOf(error[key], Error) || stack.length + 1 >= this._limit) {
            return stack;
        }
        var stacktrace = computeStackTrace(error[key]);
        var exception = exceptionFromStacktrace(stacktrace);
        return this._walkErrorTree(error[key], key, tslib_es6_spread([exception], stack));
    };
    /**
     * @inheritDoc
     */
    LinkedErrors.id = 'LinkedErrors';
    return LinkedErrors;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/useragent.js



var useragent_global = (0,misc/* getGlobalObject */.Rf)();
/** UserAgent */
var UserAgent = /** @class */ (function () {
    function UserAgent() {
        /**
         * @inheritDoc
         */
        this.name = UserAgent.id;
    }
    /**
     * @inheritDoc
     */
    UserAgent.prototype.setupOnce = function () {
        addGlobalEventProcessor(function (event) {
            var _a, _b, _c;
            if (getCurrentHub().getIntegration(UserAgent)) {
                // if none of the information we want exists, don't bother
                if (!useragent_global.navigator && !useragent_global.location && !useragent_global.document) {
                    return event;
                }
                // grab as much info as exists and add it to the event
                var url = ((_a = event.request) === null || _a === void 0 ? void 0 : _a.url) || ((_b = useragent_global.location) === null || _b === void 0 ? void 0 : _b.href);
                var referrer = (useragent_global.document || {}).referrer;
                var userAgent = (useragent_global.navigator || {}).userAgent;
                var headers = __assign(__assign(__assign({}, (_c = event.request) === null || _c === void 0 ? void 0 : _c.headers), (referrer && { Referer: referrer })), (userAgent && { 'User-Agent': userAgent }));
                var request = __assign(__assign({}, (url && { url: url })), { headers: headers });
                return __assign(__assign({}, event), { request: request });
            }
            return event;
        });
    };
    /**
     * @inheritDoc
     */
    UserAgent.id = 'UserAgent';
    return UserAgent;
}());


;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/sdk.js





var defaultIntegrations = [
    new InboundFilters(),
    new FunctionToString(),
    new TryCatch(),
    new Breadcrumbs(),
    new GlobalHandlers(),
    new LinkedErrors(),
    new UserAgent(),
];
/**
 * The Sentry Browser SDK Client.
 *
 * To use this SDK, call the {@link init} function as early as possible when
 * loading the web page. To set context information or send manual events, use
 * the provided methods.
 *
 * @example
 *
 * ```
 *
 * import { init } from '@sentry/browser';
 *
 * init({
 *   dsn: '__DSN__',
 *   // ...
 * });
 * ```
 *
 * @example
 * ```
 *
 * import { configureScope } from '@sentry/browser';
 * configureScope((scope: Scope) => {
 *   scope.setExtra({ battery: 0.7 });
 *   scope.setTag({ user_mode: 'admin' });
 *   scope.setUser({ id: '4711' });
 * });
 * ```
 *
 * @example
 * ```
 *
 * import { addBreadcrumb } from '@sentry/browser';
 * addBreadcrumb({
 *   message: 'My Breadcrumb',
 *   // ...
 * });
 * ```
 *
 * @example
 *
 * ```
 *
 * import * as Sentry from '@sentry/browser';
 * Sentry.captureMessage('Hello, world!');
 * Sentry.captureException(new Error('Good bye'));
 * Sentry.captureEvent({
 *   message: 'Manual',
 *   stacktrace: [
 *     // ...
 *   ],
 * });
 * ```
 *
 * @see {@link BrowserOptions} for documentation on configuration options.
 */
function init(options) {
    if (options === void 0) { options = {}; }
    if (options.defaultIntegrations === undefined) {
        options.defaultIntegrations = defaultIntegrations;
    }
    if (options.release === undefined) {
        var window_1 = (0,misc/* getGlobalObject */.Rf)();
        // This supports the variable that sentry-webpack-plugin injects
        if (window_1.SENTRY_RELEASE && window_1.SENTRY_RELEASE.id) {
            options.release = window_1.SENTRY_RELEASE.id;
        }
    }
    if (options.autoSessionTracking === undefined) {
        options.autoSessionTracking = false;
    }
    initAndBind(BrowserClient, options);
    if (options.autoSessionTracking) {
        startSessionTracking();
    }
}
/**
 * Present the user with a report dialog.
 *
 * @param options Everything is optional, we try to fetch all info need from the global scope.
 */
function showReportDialog(options) {
    if (options === void 0) { options = {}; }
    if (!options.eventId) {
        options.eventId = getCurrentHub().lastEventId();
    }
    var client = getCurrentHub().getClient();
    if (client) {
        client.showReportDialog(options);
    }
}
/**
 * This is the getter for lastEventId.
 *
 * @returns The last event id of a captured event.
 */
function lastEventId() {
    return getCurrentHub().lastEventId();
}
/**
 * This function is here to be API compatible with the loader.
 * @hidden
 */
function forceLoad() {
    // Noop
}
/**
 * This function is here to be API compatible with the loader.
 * @hidden
 */
function onLoad(callback) {
    callback();
}
/**
 * A promise that resolves when all current events have been sent.
 * If you provide a timeout and the queue takes longer to drain the promise returns false.
 *
 * @param timeout Maximum time in ms the client should wait.
 */
function flush(timeout) {
    var client = getCurrentHub().getClient();
    if (client) {
        return client.flush(timeout);
    }
    return SyncPromise.reject(false);
}
/**
 * A promise that resolves when all current events have been sent.
 * If you provide a timeout and the queue takes longer to drain the promise returns false.
 *
 * @param timeout Maximum time in ms the client should wait.
 */
function sdk_close(timeout) {
    var client = getCurrentHub().getClient();
    if (client) {
        return client.close(timeout);
    }
    return SyncPromise.reject(false);
}
/**
 * Wrap code within a try/catch block so the SDK is able to capture errors.
 *
 * @param fn A function to wrap.
 *
 * @returns The result of wrapped function call.
 */
// eslint-disable-next-line @typescript-eslint/no-explicit-any
function sdk_wrap(fn) {
    return wrap(fn)();
}
/**
 * Enable automatic Session Tracking for the initial page load.
 */
function startSessionTracking() {
    var window = (0,misc/* getGlobalObject */.Rf)();
    var hub = getCurrentHub();
    /**
     * We should be using `Promise.all([windowLoaded, firstContentfulPaint])` here,
     * but, as always, it's not available in the IE10-11. Thanks IE.
     */
    var loadResolved = document.readyState === 'complete';
    var fcpResolved = false;
    var possiblyEndSession = function () {
        if (fcpResolved && loadResolved) {
            hub.endSession();
        }
    };
    var resolveWindowLoaded = function () {
        loadResolved = true;
        possiblyEndSession();
        window.removeEventListener('load', resolveWindowLoaded);
    };
    hub.startSession();
    if (!loadResolved) {
        // IE doesn't support `{ once: true }` for event listeners, so we have to manually
        // attach and then detach it once completed.
        window.addEventListener('load', resolveWindowLoaded);
    }
    try {
        var po = new PerformanceObserver(function (entryList, po) {
            entryList.getEntries().forEach(function (entry) {
                if (entry.name === 'first-contentful-paint' && entry.startTime < firstHiddenTime_1) {
                    po.disconnect();
                    fcpResolved = true;
                    possiblyEndSession();
                }
            });
        });
        // There's no need to even attach this listener if `PerformanceObserver` constructor will fail,
        // so we do it below here.
        var firstHiddenTime_1 = document.visibilityState === 'hidden' ? 0 : Infinity;
        document.addEventListener('visibilitychange', function (event) {
            firstHiddenTime_1 = Math.min(firstHiddenTime_1, event.timeStamp);
        }, { once: true });
        po.observe({
            type: 'paint',
            buffered: true,
        });
    }
    catch (e) {
        fcpResolved = true;
        possiblyEndSession();
    }
}

;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/exports.js








;// CONCATENATED MODULE: ./node_modules/@sentry/core/esm/integrations/index.js



;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/integrations/index.js






;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/transports/index.js




;// CONCATENATED MODULE: ./node_modules/@sentry/browser/esm/index.js






var windowIntegrations = {};
// This block is needed to add compatibility with the integrations packages when used with a CDN
var _window = (0,misc/* getGlobalObject */.Rf)();
if (_window.Sentry && _window.Sentry.Integrations) {
    windowIntegrations = _window.Sentry.Integrations;
}
var INTEGRATIONS = __assign(__assign(__assign({}, windowIntegrations), integrations_namespaceObject), esm_integrations_namespaceObject);



/***/ }),

/***/ 2681:
/*!************************************************!*\
  !*** ./node_modules/@sentry/utils/esm/misc.js ***!
  \************************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "Rf": () => (/* binding */ getGlobalObject),
/* harmony export */   "DM": () => (/* binding */ uuid4),
/* harmony export */   "en": () => (/* binding */ parseUrl),
/* harmony export */   "jH": () => (/* binding */ getEventDescription),
/* harmony export */   "Cf": () => (/* binding */ consoleSandbox),
/* harmony export */   "Db": () => (/* binding */ addExceptionTypeValue),
/* harmony export */   "EG": () => (/* binding */ addExceptionMechanism),
/* harmony export */   "l4": () => (/* binding */ getLocationHref),
/* harmony export */   "JY": () => (/* binding */ parseRetryAfterHeader)
/* harmony export */ });
/* unused harmony exports parseSemver, addContextToFrame, stripUrlQueryAndFragment */
/* harmony import */ var _node__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./node */ 8612);


var fallbackGlobalObject = {};
/**
 * Safely get global scope object
 *
 * @returns Global scope object
 */
function getGlobalObject() {
    return ((0,_node__WEBPACK_IMPORTED_MODULE_0__/* .isNodeEnv */ .KV)()
        ? __webpack_require__.g
        : typeof window !== 'undefined'
            ? window
            : typeof self !== 'undefined'
                ? self
                : fallbackGlobalObject);
}
/**
 * UUID4 generator
 *
 * @returns string Generated UUID4.
 */
function uuid4() {
    var global = getGlobalObject();
    var crypto = global.crypto || global.msCrypto;
    if (!(crypto === void 0) && crypto.getRandomValues) {
        // Use window.crypto API if available
        var arr = new Uint16Array(8);
        crypto.getRandomValues(arr);
        // set 4 in byte 7
        // eslint-disable-next-line no-bitwise
        arr[3] = (arr[3] & 0xfff) | 0x4000;
        // set 2 most significant bits of byte 9 to '10'
        // eslint-disable-next-line no-bitwise
        arr[4] = (arr[4] & 0x3fff) | 0x8000;
        var pad = function (num) {
            var v = num.toString(16);
            while (v.length < 4) {
                v = "0" + v;
            }
            return v;
        };
        return (pad(arr[0]) + pad(arr[1]) + pad(arr[2]) + pad(arr[3]) + pad(arr[4]) + pad(arr[5]) + pad(arr[6]) + pad(arr[7]));
    }
    // http://stackoverflow.com/questions/105034/how-to-create-a-guid-uuid-in-javascript/2117523#2117523
    return 'xxxxxxxxxxxx4xxxyxxxxxxxxxxxxxxx'.replace(/[xy]/g, function (c) {
        // eslint-disable-next-line no-bitwise
        var r = (Math.random() * 16) | 0;
        // eslint-disable-next-line no-bitwise
        var v = c === 'x' ? r : (r & 0x3) | 0x8;
        return v.toString(16);
    });
}
/**
 * Parses string form of URL into an object
 * // borrowed from https://tools.ietf.org/html/rfc3986#appendix-B
 * // intentionally using regex and not <a/> href parsing trick because React Native and other
 * // environments where DOM might not be available
 * @returns parsed URL object
 */
function parseUrl(url) {
    if (!url) {
        return {};
    }
    var match = url.match(/^(([^:/?#]+):)?(\/\/([^/?#]*))?([^?#]*)(\?([^#]*))?(#(.*))?$/);
    if (!match) {
        return {};
    }
    // coerce to undefined values to empty string so we don't get 'undefined'
    var query = match[6] || '';
    var fragment = match[8] || '';
    return {
        host: match[4],
        path: match[5],
        protocol: match[2],
        relative: match[5] + query + fragment,
    };
}
/**
 * Extracts either message or type+value from an event that can be used for user-facing logs
 * @returns event's description
 */
function getEventDescription(event) {
    if (event.message) {
        return event.message;
    }
    if (event.exception && event.exception.values && event.exception.values[0]) {
        var exception = event.exception.values[0];
        if (exception.type && exception.value) {
            return exception.type + ": " + exception.value;
        }
        return exception.type || exception.value || event.event_id || '<unknown>';
    }
    return event.event_id || '<unknown>';
}
/** JSDoc */
function consoleSandbox(callback) {
    var global = getGlobalObject();
    var levels = ['debug', 'info', 'warn', 'error', 'log', 'assert'];
    if (!('console' in global)) {
        return callback();
    }
    // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
    var originalConsole = global.console;
    var wrappedLevels = {};
    // Restore all wrapped console methods
    levels.forEach(function (level) {
        // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
        if (level in global.console && originalConsole[level].__sentry_original__) {
            wrappedLevels[level] = originalConsole[level];
            originalConsole[level] = originalConsole[level].__sentry_original__;
        }
    });
    // Perform callback manipulations
    var result = callback();
    // Revert restoration to wrapped state
    Object.keys(wrappedLevels).forEach(function (level) {
        originalConsole[level] = wrappedLevels[level];
    });
    return result;
}
/**
 * Adds exception values, type and value to an synthetic Exception.
 * @param event The event to modify.
 * @param value Value of the exception.
 * @param type Type of the exception.
 * @hidden
 */
function addExceptionTypeValue(event, value, type) {
    event.exception = event.exception || {};
    event.exception.values = event.exception.values || [];
    event.exception.values[0] = event.exception.values[0] || {};
    event.exception.values[0].value = event.exception.values[0].value || value || '';
    event.exception.values[0].type = event.exception.values[0].type || type || 'Error';
}
/**
 * Adds exception mechanism to a given event.
 * @param event The event to modify.
 * @param mechanism Mechanism of the mechanism.
 * @hidden
 */
function addExceptionMechanism(event, mechanism) {
    if (mechanism === void 0) { mechanism = {}; }
    // TODO: Use real type with `keyof Mechanism` thingy and maybe make it better?
    try {
        // @ts-ignore Type 'Mechanism | {}' is not assignable to type 'Mechanism | undefined'
        // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
        event.exception.values[0].mechanism = event.exception.values[0].mechanism || {};
        Object.keys(mechanism).forEach(function (key) {
            // @ts-ignore Mechanism has no index signature
            // eslint-disable-next-line @typescript-eslint/no-non-null-assertion
            event.exception.values[0].mechanism[key] = mechanism[key];
        });
    }
    catch (_oO) {
        // no-empty
    }
}
/**
 * A safe form of location.href
 */
function getLocationHref() {
    try {
        return document.location.href;
    }
    catch (oO) {
        return '';
    }
}
// https://semver.org/#is-there-a-suggested-regular-expression-regex-to-check-a-semver-string
var SEMVER_REGEXP = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)(?:-((?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*)(?:\.(?:0|[1-9]\d*|\d*[a-zA-Z-][0-9a-zA-Z-]*))*))?(?:\+([0-9a-zA-Z-]+(?:\.[0-9a-zA-Z-]+)*))?$/;
/**
 * Parses input into a SemVer interface
 * @param input string representation of a semver version
 */
function parseSemver(input) {
    var match = input.match(SEMVER_REGEXP) || [];
    var major = parseInt(match[1], 10);
    var minor = parseInt(match[2], 10);
    var patch = parseInt(match[3], 10);
    return {
        buildmetadata: match[5],
        major: isNaN(major) ? undefined : major,
        minor: isNaN(minor) ? undefined : minor,
        patch: isNaN(patch) ? undefined : patch,
        prerelease: match[4],
    };
}
var defaultRetryAfter = 60 * 1000; // 60 seconds
/**
 * Extracts Retry-After value from the request header or returns default value
 * @param now current unix timestamp
 * @param header string representation of 'Retry-After' header
 */
function parseRetryAfterHeader(now, header) {
    if (!header) {
        return defaultRetryAfter;
    }
    var headerDelay = parseInt("" + header, 10);
    if (!isNaN(headerDelay)) {
        return headerDelay * 1000;
    }
    var headerDate = Date.parse("" + header);
    if (!isNaN(headerDate)) {
        return headerDate - now;
    }
    return defaultRetryAfter;
}
/**
 * This function adds context (pre/post/line) lines to the provided frame
 *
 * @param lines string[] containing all lines
 * @param frame StackFrame that will be mutated
 * @param linesOfContext number of context lines we want to add pre/post
 */
function addContextToFrame(lines, frame, linesOfContext) {
    if (linesOfContext === void 0) { linesOfContext = 5; }
    var lineno = frame.lineno || 0;
    var maxLines = lines.length;
    var sourceLine = Math.max(Math.min(maxLines, lineno - 1), 0);
    frame.pre_context = lines
        .slice(Math.max(0, sourceLine - linesOfContext), sourceLine)
        .map(function (line) { return snipLine(line, 0); });
    frame.context_line = snipLine(lines[Math.min(maxLines - 1, sourceLine)], frame.colno || 0);
    frame.post_context = lines
        .slice(Math.min(sourceLine + 1, maxLines), sourceLine + 1 + linesOfContext)
        .map(function (line) { return snipLine(line, 0); });
}
/**
 * Strip the query string and fragment off of a given URL or path (if present)
 *
 * @param urlPath Full URL or path, including possible query string and/or fragment
 * @returns URL or path without query string or fragment
 */
function stripUrlQueryAndFragment(urlPath) {
    // eslint-disable-next-line no-useless-escape
    return urlPath.split(/[\?#]/, 1)[0];
}


/***/ }),

/***/ 8612:
/*!************************************************!*\
  !*** ./node_modules/@sentry/utils/esm/node.js ***!
  \************************************************/
/***/ ((module, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "KV": () => (/* binding */ isNodeEnv),
/* harmony export */   "l$": () => (/* binding */ dynamicRequire)
/* harmony export */ });
/* unused harmony export extractNodeRequestData */
/* module decorator */ module = __webpack_require__.hmd(module);


/**
 * Checks whether we're in the Node.js or Browser environment
 *
 * @returns Answer to given question
 */
function isNodeEnv() {
    return Object.prototype.toString.call(typeof process !== 'undefined' ? process : 0) === '[object process]';
}
/**
 * Requires a module which is protected against bundler minification.
 *
 * @param request The module path to resolve
 */
// eslint-disable-next-line @typescript-eslint/explicit-module-boundary-types
function dynamicRequire(mod, request) {
    // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
    return mod.require(request);
}
/** Default request keys that'll be used to extract data from the request */
var DEFAULT_REQUEST_KEYS = (/* unused pure expression or super */ null && (['cookies', 'data', 'headers', 'method', 'query_string', 'url']));
/**
 * Normalizes data from the request object, accounting for framework differences.
 *
 * @param req The request object from which to extract data
 * @param keys An optional array of keys to include in the normalized data. Defaults to DEFAULT_REQUEST_KEYS if not
 * provided.
 * @returns An object containing normalized request data
 */
function extractNodeRequestData(req, keys) {
    if (keys === void 0) { keys = DEFAULT_REQUEST_KEYS; }
    // make sure we can safely use dynamicRequire below
    if (!isNodeEnv()) {
        throw new Error("Can't get node request data outside of a node environment");
    }
    var requestData = {};
    // headers:
    //   node, express: req.headers
    //   koa: req.header
    var headers = (req.headers || req.header || {});
    // method:
    //   node, express, koa: req.method
    var method = req.method;
    // host:
    //   express: req.hostname in > 4 and req.host in < 4
    //   koa: req.host
    //   node: req.headers.host
    var host = req.hostname || req.host || headers.host || '<no host>';
    // protocol:
    //   node: <n/a>
    //   express, koa: req.protocol
    var protocol = req.protocol === 'https' || req.secure || (req.socket || {}).encrypted
        ? 'https'
        : 'http';
    // url (including path and query string):
    //   node, express: req.originalUrl
    //   koa: req.url
    var originalUrl = (req.originalUrl || req.url || '');
    // absolute url
    var absoluteUrl = protocol + "://" + host + originalUrl;
    keys.forEach(function (key) {
        switch (key) {
            case 'headers':
                requestData.headers = headers;
                break;
            case 'method':
                requestData.method = method;
                break;
            case 'url':
                requestData.url = absoluteUrl;
                break;
            case 'cookies':
                // cookies:
                //   node, express, koa: req.headers.cookie
                //   vercel, sails.js, express (w/ cookie middleware): req.cookies
                // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
                requestData.cookies = req.cookies || dynamicRequire(module, 'cookie').parse(headers.cookie || '');
                break;
            case 'query_string':
                // query string:
                //   node: req.url (raw)
                //   express, koa: req.query
                // eslint-disable-next-line @typescript-eslint/no-unsafe-member-access
                requestData.query_string = dynamicRequire(module, 'url').parse(originalUrl || '', false).query;
                break;
            case 'data':
                if (method === 'GET' || method === 'HEAD') {
                    break;
                }
                // body data:
                //   node, express, koa: req.body
                if (req.body !== undefined) {
                    requestData.data = isString(req.body) ? req.body : JSON.stringify(normalize(req.body));
                }
                break;
            default:
                if ({}.hasOwnProperty.call(req, key)) {
                    requestData[key] = req[key];
                }
        }
    });
    return requestData;
}


/***/ }),

/***/ 7685:
/*!************************************************!*\
  !*** ./node_modules/@sentry/utils/esm/time.js ***!
  \************************************************/
/***/ ((module, __webpack_exports__, __webpack_require__) => {

"use strict";
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "yW": () => (/* binding */ dateTimestampInSeconds)
/* harmony export */ });
/* unused harmony exports timestampInSeconds, timestampWithMs, usingPerformanceAPI, browserPerformanceTimeOrigin */
/* harmony import */ var _misc__WEBPACK_IMPORTED_MODULE_0__ = __webpack_require__(/*! ./misc */ 2681);
/* harmony import */ var _node__WEBPACK_IMPORTED_MODULE_1__ = __webpack_require__(/*! ./node */ 8612);
/* module decorator */ module = __webpack_require__.hmd(module);


/**
 * A TimestampSource implementation for environments that do not support the Performance Web API natively.
 *
 * Note that this TimestampSource does not use a monotonic clock. A call to `nowSeconds` may return a timestamp earlier
 * than a previously returned value. We do not try to emulate a monotonic behavior in order to facilitate debugging. It
 * is more obvious to explain "why does my span have negative duration" than "why my spans have zero duration".
 */
var dateTimestampSource = {
    nowSeconds: function () { return Date.now() / 1000; },
};
/**
 * Returns a wrapper around the native Performance API browser implementation, or undefined for browsers that do not
 * support the API.
 *
 * Wrapping the native API works around differences in behavior from different browsers.
 */
function getBrowserPerformance() {
    var performance = (0,_misc__WEBPACK_IMPORTED_MODULE_0__/* .getGlobalObject */ .Rf)().performance;
    if (!performance || !performance.now) {
        return undefined;
    }
    // Replace performance.timeOrigin with our own timeOrigin based on Date.now().
    //
    // This is a partial workaround for browsers reporting performance.timeOrigin such that performance.timeOrigin +
    // performance.now() gives a date arbitrarily in the past.
    //
    // Additionally, computing timeOrigin in this way fills the gap for browsers where performance.timeOrigin is
    // undefined.
    //
    // The assumption that performance.timeOrigin + performance.now() ~= Date.now() is flawed, but we depend on it to
    // interact with data coming out of performance entries.
    //
    // Note that despite recommendations against it in the spec, browsers implement the Performance API with a clock that
    // might stop when the computer is asleep (and perhaps under other circumstances). Such behavior causes
    // performance.timeOrigin + performance.now() to have an arbitrary skew over Date.now(). In laptop computers, we have
    // observed skews that can be as long as days, weeks or months.
    //
    // See https://github.com/getsentry/sentry-javascript/issues/2590.
    //
    // BUG: despite our best intentions, this workaround has its limitations. It mostly addresses timings of pageload
    // transactions, but ignores the skew built up over time that can aversely affect timestamps of navigation
    // transactions of long-lived web pages.
    var timeOrigin = Date.now() - performance.now();
    return {
        now: function () { return performance.now(); },
        timeOrigin: timeOrigin,
    };
}
/**
 * Returns the native Performance API implementation from Node.js. Returns undefined in old Node.js versions that don't
 * implement the API.
 */
function getNodePerformance() {
    try {
        var perfHooks = (0,_node__WEBPACK_IMPORTED_MODULE_1__/* .dynamicRequire */ .l$)(module, 'perf_hooks');
        return perfHooks.performance;
    }
    catch (_) {
        return undefined;
    }
}
/**
 * The Performance API implementation for the current platform, if available.
 */
var platformPerformance = (0,_node__WEBPACK_IMPORTED_MODULE_1__/* .isNodeEnv */ .KV)() ? getNodePerformance() : getBrowserPerformance();
var timestampSource = platformPerformance === undefined
    ? dateTimestampSource
    : {
        nowSeconds: function () { return (platformPerformance.timeOrigin + platformPerformance.now()) / 1000; },
    };
/**
 * Returns a timestamp in seconds since the UNIX epoch using the Date API.
 */
var dateTimestampInSeconds = dateTimestampSource.nowSeconds.bind(dateTimestampSource);
/**
 * Returns a timestamp in seconds since the UNIX epoch using either the Performance or Date APIs, depending on the
 * availability of the Performance API.
 *
 * See `usingPerformanceAPI` to test whether the Performance API is used.
 *
 * BUG: Note that because of how browsers implement the Performance API, the clock might stop when the computer is
 * asleep. This creates a skew between `dateTimestampInSeconds` and `timestampInSeconds`. The
 * skew can grow to arbitrary amounts like days, weeks or months.
 * See https://github.com/getsentry/sentry-javascript/issues/2590.
 */
var timestampInSeconds = timestampSource.nowSeconds.bind(timestampSource);
// Re-exported with an old name for backwards-compatibility.
var timestampWithMs = (/* unused pure expression or super */ null && (timestampInSeconds));
/**
 * A boolean that is true when timestampInSeconds uses the Performance API to produce monotonic timestamps.
 */
var usingPerformanceAPI = platformPerformance !== undefined;
/**
 * The number of milliseconds since the UNIX epoch. This value is only usable in a browser, and only when the
 * performance API is available.
 */
var browserPerformanceTimeOrigin = (function () {
    var performance = (0,_misc__WEBPACK_IMPORTED_MODULE_0__/* .getGlobalObject */ .Rf)().performance;
    if (!performance) {
        return undefined;
    }
    if (performance.timeOrigin) {
        return performance.timeOrigin;
    }
    // While performance.timing.navigationStart is deprecated in favor of performance.timeOrigin, performance.timeOrigin
    // is not as widely supported. Namely, performance.timeOrigin is undefined in Safari as of writing.
    // Also as of writing, performance.timing is not available in Web Workers in mainstream browsers, so it is not always
    // a valid fallback. In the absence of an initial time provided by the browser, fallback to the current time from the
    // Date API.
    // eslint-disable-next-line deprecation/deprecation
    return (performance.timing && performance.timing.navigationStart) || Date.now();
})();


/***/ }),

/***/ 4050:
/*!***************************************************!*\
  !*** ./node_modules/fast-deep-equal/es6/index.js ***!
  \***************************************************/
/***/ ((module) => {

"use strict";


// do not edit .js files directly - edit src/index.jst


  var envHasBigInt64Array = typeof BigInt64Array !== 'undefined';


module.exports = function equal(a, b) {
  if (a === b) return true;

  if (a && b && typeof a == 'object' && typeof b == 'object') {
    if (a.constructor !== b.constructor) return false;

    var length, i, keys;
    if (Array.isArray(a)) {
      length = a.length;
      if (length != b.length) return false;
      for (i = length; i-- !== 0;)
        if (!equal(a[i], b[i])) return false;
      return true;
    }


    if ((a instanceof Map) && (b instanceof Map)) {
      if (a.size !== b.size) return false;
      for (i of a.entries())
        if (!b.has(i[0])) return false;
      for (i of a.entries())
        if (!equal(i[1], b.get(i[0]))) return false;
      return true;
    }

    if ((a instanceof Set) && (b instanceof Set)) {
      if (a.size !== b.size) return false;
      for (i of a.entries())
        if (!b.has(i[0])) return false;
      return true;
    }

    if (ArrayBuffer.isView(a) && ArrayBuffer.isView(b)) {
      length = a.length;
      if (length != b.length) return false;
      for (i = length; i-- !== 0;)
        if (a[i] !== b[i]) return false;
      return true;
    }


    if (a.constructor === RegExp) return a.source === b.source && a.flags === b.flags;
    if (a.valueOf !== Object.prototype.valueOf) return a.valueOf() === b.valueOf();
    if (a.toString !== Object.prototype.toString) return a.toString() === b.toString();

    keys = Object.keys(a);
    length = keys.length;
    if (length !== Object.keys(b).length) return false;

    for (i = length; i-- !== 0;)
      if (!Object.prototype.hasOwnProperty.call(b, keys[i])) return false;

    for (i = length; i-- !== 0;) {
      var key = keys[i];

      if (!equal(a[key], b[key])) return false;
    }

    return true;
  }

  // true if both NaN, false otherwise
  return a!==a && b!==b;
};


/***/ }),

/***/ 9602:
/*!************************************!*\
  !*** ./node_modules/flat/index.js ***!
  \************************************/
/***/ ((module) => {

module.exports = flatten
flatten.flatten = flatten
flatten.unflatten = unflatten

function isBuffer (obj) {
  return obj &&
    obj.constructor &&
    (typeof obj.constructor.isBuffer === 'function') &&
    obj.constructor.isBuffer(obj)
}

function keyIdentity (key) {
  return key
}

function flatten (target, opts) {
  opts = opts || {}

  const delimiter = opts.delimiter || '.'
  const maxDepth = opts.maxDepth
  const transformKey = opts.transformKey || keyIdentity
  const output = {}

  function step (object, prev, currentDepth) {
    currentDepth = currentDepth || 1
    Object.keys(object).forEach(function (key) {
      const value = object[key]
      const isarray = opts.safe && Array.isArray(value)
      const type = Object.prototype.toString.call(value)
      const isbuffer = isBuffer(value)
      const isobject = (
        type === '[object Object]' ||
        type === '[object Array]'
      )

      const newKey = prev
        ? prev + delimiter + transformKey(key)
        : transformKey(key)

      if (!isarray && !isbuffer && isobject && Object.keys(value).length &&
        (!opts.maxDepth || currentDepth < maxDepth)) {
        return step(value, newKey, currentDepth + 1)
      }

      output[newKey] = value
    })
  }

  step(target)

  return output
}

function unflatten (target, opts) {
  opts = opts || {}

  const delimiter = opts.delimiter || '.'
  const overwrite = opts.overwrite || false
  const transformKey = opts.transformKey || keyIdentity
  const result = {}

  const isbuffer = isBuffer(target)
  if (isbuffer || Object.prototype.toString.call(target) !== '[object Object]') {
    return target
  }

  // safely ensure that the key is
  // an integer.
  function getkey (key) {
    const parsedKey = Number(key)

    return (
      isNaN(parsedKey) ||
      key.indexOf('.') !== -1 ||
      opts.object
    ) ? key
      : parsedKey
  }

  function addKeys (keyPrefix, recipient, target) {
    return Object.keys(target).reduce(function (result, key) {
      result[keyPrefix + delimiter + key] = target[key]

      return result
    }, recipient)
  }

  function isEmpty (val) {
    const type = Object.prototype.toString.call(val)
    const isArray = type === '[object Array]'
    const isObject = type === '[object Object]'

    if (!val) {
      return true
    } else if (isArray) {
      return !val.length
    } else if (isObject) {
      return !Object.keys(val).length
    }
  }

  target = Object.keys(target).reduce(function (result, key) {
    const type = Object.prototype.toString.call(target[key])
    const isObject = (type === '[object Object]' || type === '[object Array]')
    if (!isObject || isEmpty(target[key])) {
      result[key] = target[key]
      return result
    } else {
      return addKeys(
        key,
        result,
        flatten(target[key], opts)
      )
    }
  }, {})

  Object.keys(target).forEach(function (key) {
    const split = key.split(delimiter).map(transformKey)
    let key1 = getkey(split.shift())
    let key2 = getkey(split[0])
    let recipient = result

    while (key2 !== undefined) {
      if (key1 === '__proto__') {
        return
      }

      const type = Object.prototype.toString.call(recipient[key1])
      const isobject = (
        type === '[object Object]' ||
        type === '[object Array]'
      )

      // do not write over falsey, non-undefined values if overwrite is false
      if (!overwrite && !isobject && typeof recipient[key1] !== 'undefined') {
        return
      }

      if ((overwrite && !isobject) || (!overwrite && recipient[key1] == null)) {
        recipient[key1] = (
          typeof key2 === 'number' &&
          !opts.object ? [] : {}
        )
      }

      recipient = recipient[key1]
      if (split.length > 0) {
        key1 = getkey(split.shift())
        key2 = getkey(split[0])
      }
    }

    // unflatten again for 'messy objects'
    recipient[key1] = unflatten(target[key], opts)
  })

  return result
}


/***/ }),

/***/ 7680:
/*!*************************************************************************!*\
  !*** ./node_modules/mobx-keystone/dist/mobxkeystone.esm.js + 6 modules ***!
  \*************************************************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "ActionContextActionType": () => (/* binding */ ActionContextActionType),
  "ActionContextAsyncStepType": () => (/* binding */ ActionContextAsyncStepType),
  "ActionTrackingResult": () => (/* binding */ ActionTrackingResult),
  "ArraySet": () => (/* binding */ ArraySet),
  "ArraySetTypeInfo": () => (/* binding */ ArraySetTypeInfo),
  "ArrayTypeInfo": () => (/* binding */ ArrayTypeInfo),
  "BaseModel": () => (/* binding */ BaseModel),
  "BooleanTypeInfo": () => (/* binding */ BooleanTypeInfo),
  "BuiltInAction": () => (/* binding */ BuiltInAction),
  "Draft": () => (/* binding */ Draft),
  "ExtendedModel": () => (/* binding */ ExtendedModel),
  "Frozen": () => (/* binding */ Frozen),
  "FrozenCheckMode": () => (/* binding */ FrozenCheckMode),
  "FrozenTypeInfo": () => (/* binding */ FrozenTypeInfo),
  "HookAction": () => (/* binding */ HookAction),
  "InternalPatchRecorder": () => (/* binding */ InternalPatchRecorder),
  "LiteralTypeInfo": () => (/* binding */ LiteralTypeInfo),
  "MobxKeystoneError": () => (/* binding */ MobxKeystoneError),
  "Model": () => (/* binding */ Model),
  "ModelAutoTypeCheckingMode": () => (/* binding */ ModelAutoTypeCheckingMode),
  "ModelTypeInfo": () => (/* binding */ ModelTypeInfo),
  "NumberTypeInfo": () => (/* binding */ NumberTypeInfo),
  "ObjectMap": () => (/* binding */ ObjectMap),
  "ObjectMapTypeInfo": () => (/* binding */ ObjectMapTypeInfo),
  "ObjectTypeInfo": () => (/* binding */ ObjectTypeInfo),
  "OrTypeInfo": () => (/* binding */ OrTypeInfo),
  "RecordTypeInfo": () => (/* binding */ RecordTypeInfo),
  "Ref": () => (/* binding */ Ref),
  "RefTypeInfo": () => (/* binding */ RefTypeInfo),
  "RefinementTypeInfo": () => (/* binding */ RefinementTypeInfo),
  "SandboxManager": () => (/* binding */ SandboxManager),
  "StringTypeInfo": () => (/* binding */ StringTypeInfo),
  "TupleTypeInfo": () => (/* binding */ TupleTypeInfo),
  "TypeCheckError": () => (/* binding */ TypeCheckError),
  "TypeInfo": () => (/* binding */ TypeInfo),
  "UncheckedTypeInfo": () => (/* binding */ UncheckedTypeInfo),
  "UndoManager": () => (/* binding */ UndoManager),
  "UndoStore": () => (/* binding */ UndoStore),
  "WalkTreeMode": () => (/* binding */ WalkTreeMode),
  "_async": () => (/* binding */ _async),
  "_await": () => (/* binding */ _await),
  "abstractModelClass": () => (/* binding */ abstractModelClass),
  "actionCallToReduxAction": () => (/* binding */ actionCallToReduxAction),
  "actionTrackingMiddleware": () => (/* binding */ actionTrackingMiddleware),
  "addActionMiddleware": () => (/* binding */ addActionMiddleware),
  "addActionToFnModel": () => (/* binding */ addActionToFnModel),
  "addHiddenProp": () => (/* binding */ addHiddenProp),
  "addLateInitializationFunction": () => (/* binding */ addLateInitializationFunction),
  "applyAction": () => (/* binding */ applyAction),
  "applyDelete": () => (/* binding */ applyDelete),
  "applyMethodCall": () => (/* binding */ applyMethodCall),
  "applyPatches": () => (/* binding */ applyPatches),
  "applySerializedActionAndSyncNewModelIds": () => (/* binding */ applySerializedActionAndSyncNewModelIds),
  "applySerializedActionAndTrackNewModelIds": () => (/* binding */ applySerializedActionAndTrackNewModelIds),
  "applySet": () => (/* binding */ applySet),
  "applySnapshot": () => (/* binding */ applySnapshot),
  "arraySet": () => (/* binding */ arraySet),
  "asMap": () => (/* binding */ asMap),
  "asReduxStore": () => (/* binding */ asReduxStore),
  "asSet": () => (/* binding */ asSet),
  "assertCanWrite": () => (/* binding */ assertCanWrite),
  "assertFnModelKeyNotInUse": () => (/* binding */ assertFnModelKeyNotInUse),
  "assertIsFunction": () => (/* binding */ assertIsFunction),
  "assertIsMap": () => (/* binding */ assertIsMap),
  "assertIsModel": () => (/* binding */ assertIsModel),
  "assertIsModelClass": () => (/* binding */ assertIsModelClass),
  "assertIsObject": () => (/* binding */ assertIsObject),
  "assertIsObservableArray": () => (/* binding */ assertIsObservableArray),
  "assertIsObservableObject": () => (/* binding */ assertIsObservableObject),
  "assertIsPlainObject": () => (/* binding */ assertIsPlainObject),
  "assertIsPrimitive": () => (/* binding */ assertIsPrimitive),
  "assertIsSet": () => (/* binding */ assertIsSet),
  "assertIsString": () => (/* binding */ assertIsString),
  "assertIsTreeNode": () => (/* binding */ assertIsTreeNode),
  "assertTweakedObject": () => (/* binding */ assertTweakedObject),
  "baseModelPropNames": () => (/* binding */ baseModelPropNames),
  "canWrite": () => (/* binding */ canWrite),
  "cannotSerialize": () => (/* binding */ cannotSerialize),
  "checkModelDecoratorArgs": () => (/* binding */ checkModelDecoratorArgs),
  "clone": () => (/* binding */ clone),
  "computedWalkTreeAggregate": () => (/* binding */ computedWalkTreeAggregate),
  "connectReduxDevTools": () => (/* binding */ connectReduxDevTools),
  "createContext": () => (/* binding */ createContext),
  "customRef": () => (/* binding */ customRef),
  "debugFreeze": () => (/* binding */ debugFreeze),
  "decorateWrapMethodOrField": () => (/* binding */ decorateWrapMethodOrField),
  "decoratedModel": () => (/* binding */ decoratedModel),
  "deepEquals": () => (/* binding */ deepEquals),
  "deleteFromArray": () => (/* binding */ deleteFromArray),
  "deserializeActionCall": () => (/* binding */ deserializeActionCall),
  "deserializeActionCallArgument": () => (/* binding */ deserializeActionCallArgument),
  "detach": () => (/* binding */ detach),
  "draft": () => (/* binding */ draft),
  "extendFnModelActions": () => (/* binding */ extendFnModelActions),
  "extendFnModelSetterActions": () => (/* binding */ extendFnModelSetterActions),
  "extendFnModelViews": () => (/* binding */ extendFnModelViews),
  "failure": () => (/* binding */ failure),
  "fastGetParent": () => (/* binding */ fastGetParent),
  "fastGetParentIncludingDataObjects": () => (/* binding */ fastGetParentIncludingDataObjects),
  "fastGetParentPath": () => (/* binding */ fastGetParentPath),
  "fastGetParentPathIncludingDataObjects": () => (/* binding */ fastGetParentPathIncludingDataObjects),
  "fastGetRoot": () => (/* binding */ fastGetRoot),
  "fastGetRootPath": () => (/* binding */ fastGetRootPath),
  "fastGetRootStore": () => (/* binding */ fastGetRootStore),
  "fastIsModelDataObject": () => (/* binding */ fastIsModelDataObject),
  "fastIsRootStore": () => (/* binding */ fastIsRootStore),
  "findParent": () => (/* binding */ findParent),
  "findParentPath": () => (/* binding */ findParentPath),
  "flow": () => (/* binding */ flow),
  "fnArray": () => (/* binding */ fnArray),
  "fnModel": () => (/* binding */ fnModel),
  "fnObject": () => (/* binding */ fnObject),
  "fromSnapshot": () => (/* binding */ fromSnapshot),
  "frozen": () => (/* binding */ frozen),
  "frozenKey": () => (/* binding */ frozenKey),
  "getActionMiddlewares": () => (/* binding */ getActionMiddlewares),
  "getActionProtection": () => (/* binding */ getActionProtection),
  "getChildrenObjects": () => (/* binding */ getChildrenObjects),
  "getCurrentActionContext": () => (/* binding */ getCurrentActionContext),
  "getFnModelAction": () => (/* binding */ getFnModelAction),
  "getGlobalConfig": () => (/* binding */ getGlobalConfig),
  "getMobxVersion": () => (/* binding */ getMobxVersion),
  "getModelDataType": () => (/* binding */ getModelDataType),
  "getModelRefId": () => (/* binding */ getModelRefId),
  "getNodeSandboxManager": () => (/* binding */ getNodeSandboxManager),
  "getParent": () => (/* binding */ getParent),
  "getParentPath": () => (/* binding */ getParentPath),
  "getParentToChildPath": () => (/* binding */ getParentToChildPath),
  "getRefsResolvingTo": () => (/* binding */ getRefsResolvingTo),
  "getRoot": () => (/* binding */ getRoot),
  "getRootPath": () => (/* binding */ getRootPath),
  "getRootStore": () => (/* binding */ getRootStore),
  "getSnapshot": () => (/* binding */ getSnapshot),
  "getTypeInfo": () => (/* binding */ getTypeInfo),
  "inDevMode": () => (/* binding */ inDevMode),
  "instanceCreationDataTypeSymbol": () => (/* binding */ instanceCreationDataTypeSymbol),
  "instanceDataTypeSymbol": () => (/* binding */ instanceDataTypeSymbol),
  "internalApplyDelete": () => (/* binding */ internalApplyDelete),
  "internalApplyMethodCall": () => (/* binding */ internalApplyMethodCall),
  "internalApplyPatches": () => (/* binding */ internalApplyPatches),
  "internalCustomRef": () => (/* binding */ internalCustomRef),
  "internalPatchRecorder": () => (/* binding */ internalPatchRecorder),
  "isArray": () => (/* binding */ isArray),
  "isBuiltInAction": () => (/* binding */ isBuiltInAction),
  "isChildOfParent": () => (/* binding */ isChildOfParent),
  "isFrozenSnapshot": () => (/* binding */ isFrozenSnapshot),
  "isHookAction": () => (/* binding */ isHookAction),
  "isMap": () => (/* binding */ isMap),
  "isModel": () => (/* binding */ isModel),
  "isModelAction": () => (/* binding */ isModelAction),
  "isModelAutoTypeCheckingEnabled": () => (/* binding */ isModelAutoTypeCheckingEnabled),
  "isModelClass": () => (/* binding */ isModelClass),
  "isModelDataObject": () => (/* binding */ isModelDataObject),
  "isModelFlow": () => (/* binding */ isModelFlow),
  "isModelSnapshot": () => (/* binding */ isModelSnapshot),
  "isObject": () => (/* binding */ isObject),
  "isParentOfChild": () => (/* binding */ isParentOfChild),
  "isPlainObject": () => (/* binding */ isPlainObject),
  "isPrimitive": () => (/* binding */ isPrimitive),
  "isRefOfType": () => (/* binding */ isRefOfType),
  "isReservedModelKey": () => (/* binding */ isReservedModelKey),
  "isRoot": () => (/* binding */ isRoot),
  "isRootStore": () => (/* binding */ isRootStore),
  "isSandboxedNode": () => (/* binding */ isSandboxedNode),
  "isSet": () => (/* binding */ isSet),
  "isTreeNode": () => (/* binding */ isTreeNode),
  "isTweakedObject": () => (/* binding */ isTweakedObject),
  "jsonPatchToPatch": () => (/* binding */ jsonPatchToPatch),
  "jsonPointerToPath": () => (/* binding */ jsonPointerToPath),
  "lateVal": () => (/* binding */ lateVal),
  "lazy": () => (/* binding */ lazy),
  "logWarning": () => (/* binding */ logWarning),
  "makePropReadonly": () => (/* binding */ makePropReadonly),
  "mapToArray": () => (/* binding */ mapToArray),
  "mapToObject": () => (/* binding */ mapToObject),
  "model": () => (/* binding */ model),
  "modelAction": () => (/* binding */ modelAction),
  "modelClass": () => (/* binding */ modelClass),
  "modelFlow": () => (/* binding */ modelFlow),
  "modelIdKey": () => (/* binding */ modelIdKey),
  "modelInitializedSymbol": () => (/* binding */ modelInitializedSymbol),
  "modelSnapshotInWithMetadata": () => (/* binding */ modelSnapshotInWithMetadata),
  "modelSnapshotOutWithMetadata": () => (/* binding */ modelSnapshotOutWithMetadata),
  "modelTypeKey": () => (/* binding */ modelTypeKey),
  "noDefaultValue": () => (/* binding */ noDefaultValue),
  "objectMap": () => (/* binding */ objectMap),
  "onActionMiddleware": () => (/* binding */ onActionMiddleware),
  "onChildAttachedTo": () => (/* binding */ onChildAttachedTo),
  "onGlobalPatches": () => (/* binding */ onGlobalPatches),
  "onPatches": () => (/* binding */ onPatches),
  "onSnapshot": () => (/* binding */ onSnapshot),
  "patchRecorder": () => (/* binding */ patchRecorder),
  "patchToJsonPatch": () => (/* binding */ patchToJsonPatch),
  "pathToJsonPointer": () => (/* binding */ pathToJsonPointer),
  "prop": () => (/* binding */ prop),
  "propTransform": () => (/* binding */ propTransform),
  "prop_dateString": () => (/* binding */ prop_dateString),
  "prop_dateTimestamp": () => (/* binding */ prop_dateTimestamp),
  "prop_mapArray": () => (/* binding */ prop_mapArray),
  "prop_mapObject": () => (/* binding */ prop_mapObject),
  "prop_setArray": () => (/* binding */ prop_setArray),
  "propsCreationDataTypeSymbol": () => (/* binding */ propsCreationDataTypeSymbol),
  "propsDataTypeSymbol": () => (/* binding */ propsDataTypeSymbol),
  "readonlyMiddleware": () => (/* binding */ readonlyMiddleware),
  "reduxActionType": () => (/* binding */ reduxActionType),
  "registerActionCallArgumentSerializer": () => (/* binding */ registerActionCallArgumentSerializer),
  "registerRootStore": () => (/* binding */ registerRootStore),
  "resolvePath": () => (/* binding */ resolvePath),
  "resolvePathCheckingIds": () => (/* binding */ resolvePathCheckingIds),
  "rootRef": () => (/* binding */ rootRef),
  "runLateInitializationFunctions": () => (/* binding */ runLateInitializationFunctions),
  "runUnprotected": () => (/* binding */ runUnprotected),
  "runWithoutSnapshotOrPatches": () => (/* binding */ runWithoutSnapshotOrPatches),
  "runningWithoutSnapshotOrPatches": () => (/* binding */ runningWithoutSnapshotOrPatches),
  "sandbox": () => (/* binding */ sandbox),
  "serializeActionCall": () => (/* binding */ serializeActionCall),
  "serializeActionCallArgument": () => (/* binding */ serializeActionCallArgument),
  "setCurrentActionContext": () => (/* binding */ setCurrentActionContext),
  "setGlobalConfig": () => (/* binding */ setGlobalConfig),
  "setToArray": () => (/* binding */ setToArray),
  "simplifyActionContext": () => (/* binding */ simplifyActionContext),
  "skipIdChecking": () => (/* binding */ skipIdChecking),
  "stringAsDate": () => (/* binding */ stringAsDate),
  "tProp": () => (/* binding */ tProp),
  "tProp_dateString": () => (/* binding */ tProp_dateString),
  "tProp_dateTimestamp": () => (/* binding */ tProp_dateTimestamp),
  "tProp_mapArray": () => (/* binding */ tProp_mapArray),
  "tProp_mapObject": () => (/* binding */ tProp_mapObject),
  "tProp_setArray": () => (/* binding */ tProp_setArray),
  "tag": () => (/* binding */ tag),
  "timestampAsDate": () => (/* binding */ timestampAsDate),
  "toTreeNode": () => (/* binding */ toTreeNode),
  "transaction": () => (/* binding */ transaction),
  "transactionMiddleware": () => (/* binding */ transactionMiddleware),
  "tryUntweak": () => (/* binding */ tryUntweak),
  "tweak": () => (/* binding */ tweak),
  "tweakedObjects": () => (/* binding */ tweakedObjects),
  "typeCheck": () => (/* binding */ typeCheck),
  "types": () => (/* binding */ types),
  "undoMiddleware": () => (/* binding */ undoMiddleware),
  "unregisterRootStore": () => (/* binding */ unregisterRootStore),
  "walkTree": () => (/* binding */ walkTree),
  "withoutUndo": () => (/* binding */ withoutUndo)
});

// EXTERNAL MODULE: ./node_modules/mobx/dist/mobx.esm.js
var mobx_esm = __webpack_require__(9637);
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/uuid/dist/esm-browser/rng.js
// Unique ID creation requires a high quality random # generator. In the browser we therefore
// require the crypto API and do not support built-in fallback to lower quality random number
// generators (like Math.random()).
// getRandomValues needs to be invoked in a context where "this" is a Crypto implementation. Also,
// find the complete implementation of crypto (msCrypto) on IE11.
var getRandomValues = typeof crypto !== 'undefined' && crypto.getRandomValues && crypto.getRandomValues.bind(crypto) || typeof msCrypto !== 'undefined' && typeof msCrypto.getRandomValues === 'function' && msCrypto.getRandomValues.bind(msCrypto);
var rnds8 = new Uint8Array(16);
function rng() {
  if (!getRandomValues) {
    throw new Error('crypto.getRandomValues() not supported. See https://github.com/uuidjs/uuid#getrandomvalues-not-supported');
  }

  return getRandomValues(rnds8);
}
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/uuid/dist/esm-browser/regex.js
/* harmony default export */ const regex = (/^(?:[0-9a-f]{8}-[0-9a-f]{4}-[1-5][0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}|00000000-0000-0000-0000-000000000000)$/i);
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/uuid/dist/esm-browser/validate.js


function validate(uuid) {
  return typeof uuid === 'string' && regex.test(uuid);
}

/* harmony default export */ const esm_browser_validate = (validate);
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/uuid/dist/esm-browser/stringify.js

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
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/uuid/dist/esm-browser/v4.js



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
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/node_modules/tslib/tslib.es6.js
/*! *****************************************************************************
Copyright (c) Microsoft Corporation.

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
PERFORMANCE OF THIS SOFTWARE.
***************************************************************************** */
/* global Reflect, Promise */

var extendStatics = function(d, b) {
    extendStatics = Object.setPrototypeOf ||
        ({ __proto__: [] } instanceof Array && function (d, b) { d.__proto__ = b; }) ||
        function (d, b) { for (var p in b) if (Object.prototype.hasOwnProperty.call(b, p)) d[p] = b[p]; };
    return extendStatics(d, b);
};

function __extends(d, b) {
    extendStatics(d, b);
    function __() { this.constructor = d; }
    d.prototype = b === null ? Object.create(b) : (__.prototype = b.prototype, new __());
}

var __assign = function() {
    __assign = Object.assign || function __assign(t) {
        for (var s, i = 1, n = arguments.length; i < n; i++) {
            s = arguments[i];
            for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p)) t[p] = s[p];
        }
        return t;
    }
    return __assign.apply(this, arguments);
}

function __rest(s, e) {
    var t = {};
    for (var p in s) if (Object.prototype.hasOwnProperty.call(s, p) && e.indexOf(p) < 0)
        t[p] = s[p];
    if (s != null && typeof Object.getOwnPropertySymbols === "function")
        for (var i = 0, p = Object.getOwnPropertySymbols(s); i < p.length; i++) {
            if (e.indexOf(p[i]) < 0 && Object.prototype.propertyIsEnumerable.call(s, p[i]))
                t[p[i]] = s[p[i]];
        }
    return t;
}

function __decorate(decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
}

function __param(paramIndex, decorator) {
    return function (target, key) { decorator(target, key, paramIndex); }
}

function __metadata(metadataKey, metadataValue) {
    if (typeof Reflect === "object" && typeof Reflect.metadata === "function") return Reflect.metadata(metadataKey, metadataValue);
}

function __awaiter(thisArg, _arguments, P, generator) {
    function adopt(value) { return value instanceof P ? value : new P(function (resolve) { resolve(value); }); }
    return new (P || (P = Promise))(function (resolve, reject) {
        function fulfilled(value) { try { step(generator.next(value)); } catch (e) { reject(e); } }
        function rejected(value) { try { step(generator["throw"](value)); } catch (e) { reject(e); } }
        function step(result) { result.done ? resolve(result.value) : adopt(result.value).then(fulfilled, rejected); }
        step((generator = generator.apply(thisArg, _arguments || [])).next());
    });
}

function __generator(thisArg, body) {
    var _ = { label: 0, sent: function() { if (t[0] & 1) throw t[1]; return t[1]; }, trys: [], ops: [] }, f, y, t, g;
    return g = { next: verb(0), "throw": verb(1), "return": verb(2) }, typeof Symbol === "function" && (g[Symbol.iterator] = function() { return this; }), g;
    function verb(n) { return function (v) { return step([n, v]); }; }
    function step(op) {
        if (f) throw new TypeError("Generator is already executing.");
        while (_) try {
            if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
            if (y = 0, t) op = [op[0] & 2, t.value];
            switch (op[0]) {
                case 0: case 1: t = op; break;
                case 4: _.label++; return { value: op[1], done: false };
                case 5: _.label++; y = op[1]; op = [0]; continue;
                case 7: op = _.ops.pop(); _.trys.pop(); continue;
                default:
                    if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) { _ = 0; continue; }
                    if (op[0] === 3 && (!t || (op[1] > t[0] && op[1] < t[3]))) { _.label = op[1]; break; }
                    if (op[0] === 6 && _.label < t[1]) { _.label = t[1]; t = op; break; }
                    if (t && _.label < t[2]) { _.label = t[2]; _.ops.push(op); break; }
                    if (t[2]) _.ops.pop();
                    _.trys.pop(); continue;
            }
            op = body.call(thisArg, _);
        } catch (e) { op = [6, e]; y = 0; } finally { f = t = 0; }
        if (op[0] & 5) throw op[1]; return { value: op[0] ? op[1] : void 0, done: true };
    }
}

var __createBinding = Object.create ? (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    Object.defineProperty(o, k2, { enumerable: true, get: function() { return m[k]; } });
}) : (function(o, m, k, k2) {
    if (k2 === undefined) k2 = k;
    o[k2] = m[k];
});

function __exportStar(m, o) {
    for (var p in m) if (p !== "default" && !Object.prototype.hasOwnProperty.call(o, p)) __createBinding(o, m, p);
}

function __values(o) {
    var s = typeof Symbol === "function" && Symbol.iterator, m = s && o[s], i = 0;
    if (m) return m.call(o);
    if (o && typeof o.length === "number") return {
        next: function () {
            if (o && i >= o.length) o = void 0;
            return { value: o && o[i++], done: !o };
        }
    };
    throw new TypeError(s ? "Object is not iterable." : "Symbol.iterator is not defined.");
}

function __read(o, n) {
    var m = typeof Symbol === "function" && o[Symbol.iterator];
    if (!m) return o;
    var i = m.call(o), r, ar = [], e;
    try {
        while ((n === void 0 || n-- > 0) && !(r = i.next()).done) ar.push(r.value);
    }
    catch (error) { e = { error: error }; }
    finally {
        try {
            if (r && !r.done && (m = i["return"])) m.call(i);
        }
        finally { if (e) throw e.error; }
    }
    return ar;
}

function __spread() {
    for (var ar = [], i = 0; i < arguments.length; i++)
        ar = ar.concat(__read(arguments[i]));
    return ar;
}

function __spreadArrays() {
    for (var s = 0, i = 0, il = arguments.length; i < il; i++) s += arguments[i].length;
    for (var r = Array(s), k = 0, i = 0; i < il; i++)
        for (var a = arguments[i], j = 0, jl = a.length; j < jl; j++, k++)
            r[k] = a[j];
    return r;
};

function __await(v) {
    return this instanceof __await ? (this.v = v, this) : new __await(v);
}

function __asyncGenerator(thisArg, _arguments, generator) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var g = generator.apply(thisArg, _arguments || []), i, q = [];
    return i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i;
    function verb(n) { if (g[n]) i[n] = function (v) { return new Promise(function (a, b) { q.push([n, v, a, b]) > 1 || resume(n, v); }); }; }
    function resume(n, v) { try { step(g[n](v)); } catch (e) { settle(q[0][3], e); } }
    function step(r) { r.value instanceof __await ? Promise.resolve(r.value.v).then(fulfill, reject) : settle(q[0][2], r); }
    function fulfill(value) { resume("next", value); }
    function reject(value) { resume("throw", value); }
    function settle(f, v) { if (f(v), q.shift(), q.length) resume(q[0][0], q[0][1]); }
}

function __asyncDelegator(o) {
    var i, p;
    return i = {}, verb("next"), verb("throw", function (e) { throw e; }), verb("return"), i[Symbol.iterator] = function () { return this; }, i;
    function verb(n, f) { i[n] = o[n] ? function (v) { return (p = !p) ? { value: __await(o[n](v)), done: n === "return" } : f ? f(v) : v; } : f; }
}

function __asyncValues(o) {
    if (!Symbol.asyncIterator) throw new TypeError("Symbol.asyncIterator is not defined.");
    var m = o[Symbol.asyncIterator], i;
    return m ? m.call(o) : (o = typeof __values === "function" ? __values(o) : o[Symbol.iterator](), i = {}, verb("next"), verb("throw"), verb("return"), i[Symbol.asyncIterator] = function () { return this; }, i);
    function verb(n) { i[n] = o[n] && function (v) { return new Promise(function (resolve, reject) { v = o[n](v), settle(resolve, reject, v.done, v.value); }); }; }
    function settle(resolve, reject, d, v) { Promise.resolve(v).then(function(v) { resolve({ value: v, done: d }); }, reject); }
}

function __makeTemplateObject(cooked, raw) {
    if (Object.defineProperty) { Object.defineProperty(cooked, "raw", { value: raw }); } else { cooked.raw = raw; }
    return cooked;
};

var __setModuleDefault = Object.create ? (function(o, v) {
    Object.defineProperty(o, "default", { enumerable: true, value: v });
}) : function(o, v) {
    o["default"] = v;
};

function __importStar(mod) {
    if (mod && mod.__esModule) return mod;
    var result = {};
    if (mod != null) for (var k in mod) if (k !== "default" && Object.prototype.hasOwnProperty.call(mod, k)) __createBinding(result, mod, k);
    __setModuleDefault(result, mod);
    return result;
}

function __importDefault(mod) {
    return (mod && mod.__esModule) ? mod : { default: mod };
}

function __classPrivateFieldGet(receiver, privateMap) {
    if (!privateMap.has(receiver)) {
        throw new TypeError("attempted to get private field on non-instance");
    }
    return privateMap.get(receiver);
}

function __classPrivateFieldSet(receiver, privateMap, value) {
    if (!privateMap.has(receiver)) {
        throw new TypeError("attempted to set private field on non-instance");
    }
    privateMap.set(receiver, value);
    return value;
}

// EXTERNAL MODULE: ./node_modules/fast-deep-equal/es6/index.js
var es6 = __webpack_require__(4050);
var es6_default = /*#__PURE__*/__webpack_require__.n(es6);
;// CONCATENATED MODULE: ./node_modules/mobx-keystone/dist/mobxkeystone.esm.js





/**
 * Action type, sync or async.
 */
var ActionContextActionType;

(function (ActionContextActionType) {
  ActionContextActionType["Sync"] = "sync";
  ActionContextActionType["Async"] = "async";
})(ActionContextActionType || (ActionContextActionType = {}));
/**
 * An async step type.
 */


var ActionContextAsyncStepType;

(function (ActionContextAsyncStepType) {
  /**
   * The flow is about to start.
   */
  ActionContextAsyncStepType["Spawn"] = "spawn";
  /**
   * The flow is about to return (finish).
   */

  ActionContextAsyncStepType["Return"] = "return";
  /**
   * The flow is about to continue.
   */

  ActionContextAsyncStepType["Resume"] = "resume";
  /**
   * The flow yield just threw, which might be recovered (caught) or not.
   */

  ActionContextAsyncStepType["ResumeError"] = "resumeError";
  /**
   * The flow is about to throw an error to the flow caller.
   */

  ActionContextAsyncStepType["Throw"] = "throw";
})(ActionContextAsyncStepType || (ActionContextAsyncStepType = {}));

var currentActionContext;
/**
 * Gets the currently running action context, or undefined if none.
 *
 * @returns
 */

function getCurrentActionContext() {
  return currentActionContext;
}
/**
 * @ignore
 * @internal
 *
 * Sets the current action context.
 *
 * @param ctx Current action context.
 */

function setCurrentActionContext(ctx) {
  currentActionContext = ctx;
}

function _defineProperties(target, props) {
  for (var i = 0; i < props.length; i++) {
    var descriptor = props[i];
    descriptor.enumerable = descriptor.enumerable || false;
    descriptor.configurable = true;
    if ("value" in descriptor) descriptor.writable = true;
    Object.defineProperty(target, descriptor.key, descriptor);
  }
}

function _createClass(Constructor, protoProps, staticProps) {
  if (protoProps) _defineProperties(Constructor.prototype, protoProps);
  if (staticProps) _defineProperties(Constructor, staticProps);
  return Constructor;
}

function _extends() {
  _extends = Object.assign || function (target) {
    for (var i = 1; i < arguments.length; i++) {
      var source = arguments[i];

      for (var key in source) {
        if (Object.prototype.hasOwnProperty.call(source, key)) {
          target[key] = source[key];
        }
      }
    }

    return target;
  };

  return _extends.apply(this, arguments);
}

function _inheritsLoose(subClass, superClass) {
  subClass.prototype = Object.create(superClass.prototype);
  subClass.prototype.constructor = subClass;
  subClass.__proto__ = superClass;
}

function _getPrototypeOf(o) {
  _getPrototypeOf = Object.setPrototypeOf ? Object.getPrototypeOf : function _getPrototypeOf(o) {
    return o.__proto__ || Object.getPrototypeOf(o);
  };
  return _getPrototypeOf(o);
}

function _setPrototypeOf(o, p) {
  _setPrototypeOf = Object.setPrototypeOf || function _setPrototypeOf(o, p) {
    o.__proto__ = p;
    return o;
  };

  return _setPrototypeOf(o, p);
}

function _isNativeReflectConstruct() {
  if (typeof Reflect === "undefined" || !Reflect.construct) return false;
  if (Reflect.construct.sham) return false;
  if (typeof Proxy === "function") return true;

  try {
    Date.prototype.toString.call(Reflect.construct(Date, [], function () {}));
    return true;
  } catch (e) {
    return false;
  }
}

function _construct(Parent, args, Class) {
  if (_isNativeReflectConstruct()) {
    _construct = Reflect.construct;
  } else {
    _construct = function _construct(Parent, args, Class) {
      var a = [null];
      a.push.apply(a, args);
      var Constructor = Function.bind.apply(Parent, a);
      var instance = new Constructor();
      if (Class) _setPrototypeOf(instance, Class.prototype);
      return instance;
    };
  }

  return _construct.apply(null, arguments);
}

function _isNativeFunction(fn) {
  return Function.toString.call(fn).indexOf("[native code]") !== -1;
}

function _wrapNativeSuper(Class) {
  var _cache = typeof Map === "function" ? new Map() : undefined;

  _wrapNativeSuper = function _wrapNativeSuper(Class) {
    if (Class === null || !_isNativeFunction(Class)) return Class;

    if (typeof Class !== "function") {
      throw new TypeError("Super expression must either be null or a function");
    }

    if (typeof _cache !== "undefined") {
      if (_cache.has(Class)) return _cache.get(Class);

      _cache.set(Class, Wrapper);
    }

    function Wrapper() {
      return _construct(Class, arguments, _getPrototypeOf(this).constructor);
    }

    Wrapper.prototype = Object.create(Class.prototype, {
      constructor: {
        value: Wrapper,
        enumerable: false,
        writable: true,
        configurable: true
      }
    });
    return _setPrototypeOf(Wrapper, Class);
  };

  return _wrapNativeSuper(Class);
}

function _assertThisInitialized(self) {
  if (self === void 0) {
    throw new ReferenceError("this hasn't been initialised - super() hasn't been called");
  }

  return self;
}

function _unsupportedIterableToArray(o, minLen) {
  if (!o) return;
  if (typeof o === "string") return _arrayLikeToArray(o, minLen);
  var n = Object.prototype.toString.call(o).slice(8, -1);
  if (n === "Object" && o.constructor) n = o.constructor.name;
  if (n === "Map" || n === "Set") return Array.from(o);
  if (n === "Arguments" || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(n)) return _arrayLikeToArray(o, minLen);
}

function _arrayLikeToArray(arr, len) {
  if (len == null || len > arr.length) len = arr.length;

  for (var i = 0, arr2 = new Array(len); i < len; i++) arr2[i] = arr[i];

  return arr2;
}

function _createForOfIteratorHelperLoose(o, allowArrayLike) {
  var it;

  if (typeof Symbol === "undefined" || o[Symbol.iterator] == null) {
    if (Array.isArray(o) || (it = _unsupportedIterableToArray(o)) || allowArrayLike && o && typeof o.length === "number") {
      if (it) o = it;
      var i = 0;
      return function () {
        if (i >= o.length) return {
          done: true
        };
        return {
          done: false,
          value: o[i++]
        };
      };
    }

    throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
  }

  it = o[Symbol.iterator]();
  return it.next.bind(it);
}

/**
 * A mobx-keystone error.
 */

var MobxKeystoneError = /*#__PURE__*/function (_Error) {
  _inheritsLoose(MobxKeystoneError, _Error);

  function MobxKeystoneError(msg) {
    var _this;

    _this = _Error.call(this, msg) || this; // Set the prototype explicitly.

    Object.setPrototypeOf(_assertThisInitialized(_this), MobxKeystoneError.prototype);
    return _this;
  }

  return MobxKeystoneError;
}( /*#__PURE__*/_wrapNativeSuper(Error));
/**
 * @ignore
 * @internal
 */

function failure(msg) {
  return new MobxKeystoneError(msg);
}
var writableHiddenPropDescriptor = {
  enumerable: false,
  writable: true,
  configurable: false,
  value: undefined
};
/**
 * @ignore
 * @internal
 */

function addHiddenProp(object, propName, value, writable) {
  if (writable === void 0) {
    writable = true;
  }

  if (writable) {
    Object.defineProperty(object, propName, writableHiddenPropDescriptor);
    object[propName] = value;
  } else {
    Object.defineProperty(object, propName, {
      enumerable: false,
      writable: writable,
      configurable: true,
      value: value
    });
  }
}
/**
 * @ignore
 * @internal
 */

function makePropReadonly(object, propName, enumerable) {
  var propDesc = Object.getOwnPropertyDescriptor(object, propName);

  if (propDesc) {
    propDesc.enumerable = enumerable;

    if (propDesc.get) {
      delete propDesc.set;
    } else {
      propDesc.writable = false;
    }

    Object.defineProperty(object, propName, propDesc);
  }
}
/**
 * @ignore
 * @internal
 */

function isPlainObject(value) {
  if (!isObject(value)) return false;
  var proto = Object.getPrototypeOf(value);
  return proto === Object.prototype || proto === null;
}
/**
 * @ignore
 * @internal
 */

function isObject(value) {
  return value !== null && typeof value === "object";
}
/**
 * @ignore
 * @internal
 */

function isPrimitive(value) {
  switch (typeof value) {
    case "number":
    case "string":
    case "boolean":
    case "undefined":
    case "bigint":
      return true;
  }

  return value === null;
}
/**
 * @ignore
 * @internal
 */

function debugFreeze(value) {
  if (inDevMode()) {
    Object.freeze(value);
  }
}
/**
 * @ignore
 * @internal
 */

function deleteFromArray(array, value) {
  var index = array.indexOf(value);

  if (index >= 0) {
    array.splice(index, 1);
    return true;
  }

  return false;
}
/**
 * @ignore
 * @internal
 */

function isMap(val) {
  return val instanceof Map || (0,mobx_esm.isObservableMap)(val);
}
/**
 * @ignore
 * @internal
 */

function isSet(val) {
  return val instanceof Set || (0,mobx_esm.isObservableSet)(val);
}
/**
 * @ignore
 * @internal
 */

function isArray(val) {
  return Array.isArray(val) || (0,mobx_esm.isObservableArray)(val);
}
/**
 * @ignore
 * @internal
 */

function inDevMode() {
  return "production" !== "production";
}
/**
 * @ignore
 * @internal
 */

function assertIsObject(value, argName) {
  if (!isObject(value)) {
    throw failure(argName + " must be an object");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsPlainObject(value, argName) {
  if (!isPlainObject(value)) {
    throw failure(argName + " must be a plain object");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsObservableObject(value, argName) {
  if (!(0,mobx_esm.isObservableObject)(value)) {
    throw failure(argName + " must be an observable object");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsObservableArray(value, argName) {
  if (!(0,mobx_esm.isObservableArray)(value)) {
    throw failure(argName + " must be an observable array");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsMap(value, argName) {
  if (!isMap(value)) {
    throw failure(argName + " must be a map");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsSet(value, argName) {
  if (!isSet(value)) {
    throw failure(argName + " must be a set");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsFunction(value, argName) {
  if (typeof value !== "function") {
    throw failure(argName + " must be a function");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsPrimitive(value, argName) {
  if (!isPrimitive(value)) {
    throw failure(argName + " must be a primitive");
  }
}
/**
 * @ignore
 * @internal
 */

function assertIsString(value, argName) {
  if (typeof value !== "string") {
    throw failure(argName + " must be a string");
  }
}
var decoratorsSymbol = /*#__PURE__*/Symbol("decorators");
/**
 * @ignore
 * @internal
 */

function addLateInitializationFunction(target, fn) {
  var decoratorsArray = target[decoratorsSymbol];

  if (!decoratorsArray) {
    decoratorsArray = [];
    addHiddenProp(target, decoratorsSymbol, decoratorsArray);
  }

  decoratorsArray.push(fn);
}
/**
 * @ignore
 * @internal
 */

function decorateWrapMethodOrField(decoratorName, data, wrap) {
  var target = data.target,
      propertyKey = data.propertyKey,
      baseDescriptor = data.baseDescriptor;

  var addFieldDecorator = function addFieldDecorator() {
    addLateInitializationFunction(target, function (instance) {
      instance[propertyKey] = wrap(data, instance[propertyKey]);
    });
  };

  if (baseDescriptor) {
    if (baseDescriptor.get !== undefined) {
      throw failure("@" + decoratorName + " cannot be used with getters");
    }

    if (baseDescriptor.value) {
      // babel / typescript - method decorator
      // @action method() { }
      return {
        enumerable: false,
        writable: true,
        configurable: true,
        value: wrap(data, baseDescriptor.value)
      };
    } else {
      // babel - field decorator: @action method = () => {}
      addFieldDecorator();
    }
  } else {
    // typescript - field decorator
    addFieldDecorator();
  }
}
/**
 * @ignore
 * @internal
 */

function runLateInitializationFunctions(instance) {
  var fns = instance[decoratorsSymbol];

  if (fns) {
    for (var _iterator = _createForOfIteratorHelperLoose(fns), _step; !(_step = _iterator()).done;) {
      var fn = _step.value;
      fn(instance);
    }
  }
}
var warningsAlreadyDisplayed = /*#__PURE__*/new Set();
/**
 * @ignore
 * @internal
 */

function logWarning(type, msg, uniqueKey) {
  if (uniqueKey) {
    if (warningsAlreadyDisplayed.has(uniqueKey)) {
      return;
    }

    warningsAlreadyDisplayed.add(uniqueKey);
  }

  msg = "[mobx-keystone] " + msg;

  switch (type) {
    case "warn":
      console.warn(msg);
      break;

    case "error":
      console.error(msg);
      break;

    default:
      throw failure("unknown log type - " + type);
  }
}
var notMemoized = /*#__PURE__*/Symbol("notMemoized");
/**
 * @ignore
 * @internal
 */

function lateVal(getter) {
  var memoized = notMemoized;

  var fn = function fn() {
    if (memoized === notMemoized) {
      memoized = getter.apply(void 0, arguments);
    }

    return memoized;
  };

  return fn;
}
/**
 * @ignore
 * @internal
 */

function lazy(valueGen) {
  var inited = false;
  var val;
  return function () {
    if (!inited) {
      val = valueGen();
      inited = true;
    }

    return val;
  };
}
/**
 * @ignore
 * @internal
 */

function getMobxVersion() {
  if (mobx_esm.makeObservable) {
    return 6;
  } else {
    return 5;
  }
}

var chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
function toBase64(input) {
  if (typeof __webpack_require__.g === "object" && typeof __webpack_require__.g.Buffer === "function") {
    // node
    return Buffer.from(input).toString("base64");
  }

  if (typeof btoa === "function") {
    // browser
    return btoa(input);
  }

  var str = String(input);
  var output = "";

  for ( // initialize result and counter
  var block = 0, charCode, idx = 0, map = chars; // if the next str index does not exist:
  //   change the mapping table to "="
  //   check if d has no fractional digits
  str.charAt(idx | 0) || (map = "=", idx % 1); // "8 - idx % 1 * 8" generates the sequence 2, 4, 6, 8
  output += map.charAt(63 & block >> 8 - idx % 1 * 8)) {
    charCode = str.charCodeAt(idx += 3 / 4);

    if (charCode > 0xff) {
      throw new Error("the string to be encoded contains characters outside of the Latin1 range.");
    }

    block = block << 8 | charCode;
  }

  return output;
}

/**
 * Model auto type-checking mode.
 */

var ModelAutoTypeCheckingMode;

(function (ModelAutoTypeCheckingMode) {
  /**
   * Auto type check models only in dev mode
   */
  ModelAutoTypeCheckingMode["DevModeOnly"] = "devModeOnly";
  /**
   * Auto type check models no matter the current environment
   */

  ModelAutoTypeCheckingMode["AlwaysOn"] = "alwaysOn";
  /**
   * Do not auto type check models no matter the current environment
   */

  ModelAutoTypeCheckingMode["AlwaysOff"] = "alwaysOff";
})(ModelAutoTypeCheckingMode || (ModelAutoTypeCheckingMode = {}));

var localId = 0;
var localBaseId = /*#__PURE__*/shortenUuid( /*#__PURE__*/esm_browser_v4());

function defaultModelIdGenerator() {
  // we use base 36 for local id since it is short and fast
  var id = localId.toString(36) + "-" + localBaseId;
  localId++;
  return id;
} // defaults


var globalConfig = {
  modelAutoTypeChecking: ModelAutoTypeCheckingMode.DevModeOnly,
  modelIdGenerator: defaultModelIdGenerator,
  allowUndefinedArrayElements: false
};
/**
 * Partially sets the current global config.
 *
 * @param config Partial object with the new configurations. Options not included in the object won't be changed.
 */

function setGlobalConfig(config) {
  globalConfig = Object.freeze(_extends({}, globalConfig, config));
}
/**
 * Returns the current global config object.
 *
 * @returns
 */

function getGlobalConfig() {
  return globalConfig;
}
/**
 * @ignore
 * @internal
 *
 * Returns if the auto type checking for models is enabled.
 *
 * @returns
 */

function isModelAutoTypeCheckingEnabled() {
  switch (getGlobalConfig().modelAutoTypeChecking) {
    case ModelAutoTypeCheckingMode.DevModeOnly:
      return inDevMode();

    case ModelAutoTypeCheckingMode.AlwaysOff:
      return false;

    case ModelAutoTypeCheckingMode.AlwaysOn:
      return true;

    default:
      throw failure("invalid 'modelAutoTypeChecking' config value - " + globalConfig.modelAutoTypeChecking);
  }
}

function shortenUuid(uuid) {
  // remove non-hex chars
  var hex = uuid.split("-").join(""); // convert to base64

  var hexMatch = hex.match(/\w{2}/g);
  var str = String.fromCharCode.apply(null, hexMatch.map(function (a) {
    return parseInt(a, 16);
  }));
  return toBase64(str);
}

/**
 * @ignore
 */

var noDefaultValue = /*#__PURE__*/Symbol("noDefaultValue");
function prop(arg1, arg2) {
  var def;
  var opts = {};
  var hasDefaultValue = false;

  if (arguments.length >= 2) {
    // default, options
    def = arg1;
    hasDefaultValue = true;
    opts = _extends({}, arg2);
  } else if (arguments.length === 1) {
    // default | options
    if (isObject(arg1)) {
      // options
      opts = _extends({}, arg1);
    } else {
      // default
      def = arg1;
      hasDefaultValue = true;
    }
  }

  var isDefFn = typeof def === "function";
  return {
    $propValueType: null,
    $propCreationValueType: null,
    $isOptional: null,
    $instanceValueType: null,
    $instanceCreationValueType: null,
    defaultFn: hasDefaultValue && isDefFn ? def : noDefaultValue,
    defaultValue: hasDefaultValue && !isDefFn ? def : noDefaultValue,
    typeChecker: undefined,
    transform: undefined,
    options: opts
  };
}

/**
 * Creates a prop transform, useful to transform property data into another kind of data.
 *
 * For example, to transform from a number timestamp into a date:
 * ```ts
 * const asDate = propTransform({
 *   propToData(prop: number) {
 *     return new Date(prop)
 *   },
 *   dataToProp(data: Date) {
 *     return +data
 *   }
 * })
 *
 * @model("myApp/MyModel")
 * class MyModel extends Model({
 *   timestamp: prop<number>()
 * }) {
 *   @asDate("timestamp")
 *   date!: Date
 * }
 *
 * const date: Date = myModel.date
 * // inside some model action
 * myModel.date = new Date()
 * ```
 *
 * @typename TProp Property value type.
 * @typename TData Data value type.
 * @param transform Transform to apply.
 * @returns A decorator that can be used to decorate model class fields.
 */

function propTransform(transform) {
  var parametrizedDecorator = function parametrizedDecorator(boundPropName) {
    var decorator = function decorator(target, propertyKey) {
      addLateInitializationFunction(target, function (instance) {
        // make the field a getter setter
        Object.defineProperty(instance, propertyKey, {
          get: function get() {
            var memoTransform = memoTransformCache.getOrCreateMemoTransform(this, propertyKey, transform);
            return memoTransform.propToData(this.$[boundPropName]);
          },
          set: function set(value) {
            var memoTransform = memoTransformCache.getOrCreateMemoTransform(this, propertyKey, transform);
            this.$[boundPropName] = memoTransform.dataToProp(value);
            return true;
          }
        });
      });
    };

    return decorator;
  };

  parametrizedDecorator.propToData = transform.propToData.bind(transform);
  parametrizedDecorator.dataToProp = transform.dataToProp.bind(transform);
  return parametrizedDecorator;
}

var MemoTransformCache = /*#__PURE__*/function () {
  function MemoTransformCache() {
    this.cache = new WeakMap();
  }

  var _proto = MemoTransformCache.prototype;

  _proto.getOrCreateMemoTransform = function getOrCreateMemoTransform(target, propName, baseTransform) {
    var transformsPerProperty = this.cache.get(target);

    if (!transformsPerProperty) {
      transformsPerProperty = new Map();
      this.cache.set(target, transformsPerProperty);
    }

    var memoTransform = transformsPerProperty.get(propName);

    if (!memoTransform) {
      memoTransform = toMemoPropTransform(baseTransform, function (newPropValue) {
        target.$[propName] = newPropValue;
      });
      transformsPerProperty.set(propName, memoTransform);
    }

    return memoTransform;
  };

  return MemoTransformCache;
}();
/**
 * @ignore
 * @internal
 */


var memoTransformCache = /*#__PURE__*/new MemoTransformCache();
var valueNotMemoized = /*#__PURE__*/Symbol("valueNotMemoized");

function toMemoPropTransform(transform, propSetter) {
  var lastPropValue = valueNotMemoized;
  var lastDataValue = valueNotMemoized;
  return {
    isMemoPropTransform: true,
    propToData: function propToData(propValue) {
      if (lastPropValue !== propValue) {
        lastDataValue = transform.propToData(propValue, propSetter);
        lastPropValue = propValue;
      }

      return lastDataValue;
    },
    dataToProp: function dataToProp(newDataValue) {
      // clear caches in this case
      lastPropValue = valueNotMemoized;
      lastDataValue = valueNotMemoized;
      return transform.dataToProp(newDataValue);
    }
  };
}
/**
 * @ignore
 * @internal
 */


function transformedProp(prop, transform, transformDefault) {
  if (prop.transform) {
    throw failure("a property cannot have more than one transform");
  }

  var p = _extends({}, prop, {
    transform: transform
  }); // transform defaults if needed


  if (transformDefault) {
    if (p.defaultValue !== noDefaultValue) {
      var originalDefaultValue = p.defaultValue;
      p.defaultValue = transform.dataToProp(originalDefaultValue);
    }

    if (p.defaultFn !== noDefaultValue) {
      var originalDefaultFn = p.defaultFn;

      p.defaultFn = function () {
        return transform.dataToProp(originalDefaultFn());
      };
    }
  }

  return p;
}

/**
 * @ignore
 */

var objectParents = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 */

var objectParentsAtoms = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 */

function parentPathEquals(parentPath1, parentPath2, comparePath) {
  if (comparePath === void 0) {
    comparePath = true;
  }

  if (!parentPath1 && !parentPath2) return true;
  if (!parentPath1 || !parentPath2) return false;
  var parentEquals = parentPath1.parent === parentPath2.parent;
  if (!parentEquals) return false;
  return comparePath ? parentPath1.path === parentPath2.path : true;
}

function createParentPathAtom(obj) {
  var atom = objectParentsAtoms.get(obj);

  if (!atom) {
    atom = (0,mobx_esm.createAtom)("parentAtom");
    objectParentsAtoms.set(obj, atom);
  }

  return atom;
}
/**
 * @ignore
 */


function reportParentPathObserved(node) {
  createParentPathAtom(node).reportObserved();
}
/**
 * @ignore
 */

function reportParentPathChanged(node) {
  createParentPathAtom(node).reportChanged();
}
/**
 * @ignore
 */

var dataObjectParent = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 */

function dataToModelNode(node) {
  var modelNode = dataObjectParent.get(node);
  return modelNode != null ? modelNode : node;
}
/**
 * @ignore
 */

function modelToDataNode(node) {
  return isModel(node) ? node.$ : node;
}

/**
 * @ignore
 * @internal
 */

var tweakedObjects = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 */

function isTweakedObject(value, canBeDataObject) {
  if (!canBeDataObject && dataObjectParent.has(value)) {
    return false;
  }

  return tweakedObjects.has(value);
}
/**
 * Checks if a given object is now a tree node.
 *
 * @param value Value to check.
 * @returns true if it is a tree node, false otherwise.
 */

function isTreeNode(value) {
  return !isPrimitive(value) && isTweakedObject(value, false);
}
/**
 * @ignore
 * @internal
 */

function assertTweakedObject(treeNode, argName, canBeDataObject) {
  if (canBeDataObject === void 0) {
    canBeDataObject = false;
  }

  if (!canBeDataObject && dataObjectParent.has(treeNode)) {
    throw failure(argName + " must be the model object instance instead of the '$' sub-object");
  }

  if (isPrimitive(treeNode) || !isTweakedObject(treeNode, true)) {
    throw failure(argName + " must be a tree node (usually a model or a shallow / deep child part of a model 'data' object)");
  }
}
/**
 * Asserts a given object is now a tree node, or throws otherwise.
 *
 * @param value Value to check.
 * @param argName Argument name, part of the thrown error description.
 */

function assertIsTreeNode(value, argName) {
  if (argName === void 0) {
    argName = "argument";
  }

  assertTweakedObject(value, argName, false);
}
/**
 * @ignore
 * @internal
 */

var runningWithoutSnapshotOrPatches = false;
/**
 * @ignore
 * @internal
 */

function runWithoutSnapshotOrPatches(fn) {
  var old = runningWithoutSnapshotOrPatches;
  runningWithoutSnapshotOrPatches = true;

  try {
    (0,mobx_esm.runInAction)(function () {
      fn();
    });
  } finally {
    runningWithoutSnapshotOrPatches = old;
  }
}

/**
 * Key where model snapshots will store model type metadata.
 */
var modelTypeKey = "$modelType";
/**
 * Key where model snapshots will store model internal IDs metadata.
 */

var modelIdKey = "$modelId";
/**
 * @internal
 * Returns if a given key is a reserved key in model snapshots.
 *
 * @param key
 * @returns
 */

function isReservedModelKey(key) {
  return key === modelTypeKey || key === modelIdKey;
}

var defaultObservableSetOptions = {
  deep: false
};
var objectChildren = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 */

function initializeObjectChildren(node) {
  if (objectChildren.has(node)) {
    return;
  }

  objectChildren.set(node, {
    shallow: mobx_esm.observable.set(undefined, defaultObservableSetOptions),
    deep: new Set(),
    deepByModelTypeAndId: new Map(),
    deepDirty: true,
    deepAtom: (0,mobx_esm.createAtom)("deepChildrenAtom")
  });
}
/**
 * @ignore
 * @internal
 */

function getObjectChildren(node) {
  return objectChildren.get(node).shallow;
}
/**
 * @ignore
 * @internal
 */

function getDeepObjectChildren(node) {
  var obj = objectChildren.get(node);

  if (obj.deepDirty) {
    updateDeepObjectChildren(node);
  }

  obj.deepAtom.reportObserved();
  return {
    deep: obj.deep,
    deepByModelTypeAndId: obj.deepByModelTypeAndId
  };
}

function addNodeToDeepLists(node, deep, deepByModelTypeAndId) {
  deep.add(node);

  if (isModel(node)) {
    deepByModelTypeAndId.set(byModelTypeAndIdKey(node[modelTypeKey], node[modelIdKey]), node);
  }
}

var updateDeepObjectChildren = /*#__PURE__*/(0,mobx_esm.action)(function (node) {
  var obj = objectChildren.get(node);

  if (!obj.deepDirty) {
    return {
      deep: obj.deep,
      deepByModelTypeAndId: obj.deepByModelTypeAndId
    };
  }

  var deep = new Set();
  var deepByModelTypeAndId = new Map();
  var childrenIter = getObjectChildren(node).values();
  var ch = childrenIter.next();

  while (!ch.done) {
    addNodeToDeepLists(ch.value, deep, deepByModelTypeAndId);
    var ret = updateDeepObjectChildren(ch.value).deep;
    var retIter = ret.values();
    var retCur = retIter.next();

    while (!retCur.done) {
      addNodeToDeepLists(retCur.value, deep, deepByModelTypeAndId);
      retCur = retIter.next();
    }

    ch = childrenIter.next();
  }

  obj.deep = deep;
  obj.deepByModelTypeAndId = deepByModelTypeAndId;
  obj.deepDirty = false;
  obj.deepAtom.reportChanged();
  return {
    deep: deep,
    deepByModelTypeAndId: deepByModelTypeAndId
  };
});
/**
 * @ignore
 * @internal
 */

var addObjectChild = /*#__PURE__*/(0,mobx_esm.action)(function (node, child) {
  var obj = objectChildren.get(node);
  obj.shallow.add(child);
  invalidateDeepChildren(node);
});
/**
 * @ignore
 * @internal
 */

var removeObjectChild = /*#__PURE__*/(0,mobx_esm.action)(function (node, child) {
  var obj = objectChildren.get(node);
  obj.shallow["delete"](child);
  invalidateDeepChildren(node);
});

function invalidateDeepChildren(node) {
  var obj = objectChildren.get(node);

  if (!obj.deepDirty) {
    obj.deepDirty = true;
    obj.deepAtom.reportChanged();
  }

  var parent = fastGetParent(node);

  if (parent) {
    invalidateDeepChildren(parent);
  }
}
/**
 * @ignore
 * @internal
 */


function byModelTypeAndIdKey(modelType, modelId) {
  return modelType + " " + modelId;
}

/**
 * Returns the parent of the target plus the path from the parent to the target, or undefined if it has no parent.
 *
 * @typeparam T Parent object type.
 * @param value Target object.
 * @returns
 */

function getParentPath(value) {
  assertTweakedObject(value, "value");
  return fastGetParentPath(value);
}
/**
 * @ignore
 * @internal
 */

function fastGetParentPath(value) {
  reportParentPathObserved(value);
  return objectParents.get(value);
}
/**
 * @ignore
 * @internal
 */

function fastGetParentPathIncludingDataObjects(value) {
  var parentModel = dataObjectParent.get(value);

  if (parentModel) {
    return {
      parent: parentModel,
      path: "$"
    };
  }

  var parentPath = fastGetParentPath(value);

  if (parentPath && isModel(parentPath.parent)) {
    return {
      parent: parentPath.parent.$,
      path: parentPath.path
    };
  }

  return parentPath;
}
/**
 * Returns the parent object of the target object, or undefined if there's no parent.
 *
 * @typeparam T Parent object type.
 * @param value Target object.
 * @returns
 */

function getParent(value) {
  assertTweakedObject(value, "value");
  return fastGetParent(value);
}
/**
 * @ignore
 * @internal
 */

function fastGetParent(value) {
  var parentPath = fastGetParentPath(value);
  return parentPath ? parentPath.parent : undefined;
}
/**
 * @ignore
 * @internal
 */

function fastGetParentIncludingDataObjects(value) {
  var parentPath = fastGetParentPathIncludingDataObjects(value);
  return parentPath ? parentPath.parent : undefined;
}
/**
 * Returns if a given object is a model interim data object (`$`).
 *
 * @param value Object to check.
 * @returns true if it is, false otherwise.
 */

function isModelDataObject(value) {
  assertTweakedObject(value, "value", true);
  return fastIsModelDataObject(value);
}
/**
 * @ignore
 * @internal
 */

function fastIsModelDataObject(value) {
  return dataObjectParent.has(value);
}
/**
 * Returns the root of the target plus the path from the root to get to the target.
 *
 * @typeparam T Root object type.
 * @param value Target object.
 * @returns
 */

function getRootPath(value) {
  assertTweakedObject(value, "value");
  return fastGetRootPath(value);
} // we use computeds so they are cached whenever possible

var computedsGetRootPath = /*#__PURE__*/new WeakMap();

function internalGetRootPath(value) {
  var rootPath = {
    root: value,
    path: [],
    pathObjects: [value]
  };
  var parentPath;

  while (parentPath = fastGetParentPath(rootPath.root)) {
    rootPath.root = parentPath.parent;
    rootPath.path.unshift(parentPath.path);
    rootPath.pathObjects.unshift(parentPath.parent);
  }

  return rootPath;
}
/**
 * @ignore
 * @internal
 */


function fastGetRootPath(value) {
  var computedGetRootPathForNode = computedsGetRootPath.get(value);

  if (!computedGetRootPathForNode) {
    computedGetRootPathForNode = (0,mobx_esm.computed)(function () {
      return internalGetRootPath(value);
    });
    computedsGetRootPath.set(value, computedGetRootPathForNode);
  }

  return computedGetRootPathForNode.get();
}
/**
 * Returns the root of the target object, or itself if the target is a root.
 *
 * @typeparam T Root object type.
 * @param value Target object.
 * @returns
 */

function getRoot(value) {
  assertTweakedObject(value, "value");
  return fastGetRoot(value);
}
/**
 * @ignore
 * @internal
 */

function fastGetRoot(value) {
  return fastGetRootPath(value).root;
}
/**
 * Returns if a given object is a root object.
 *
 * @param value Target object.
 * @returns
 */

function isRoot(value) {
  assertTweakedObject(value, "value");
  return !fastGetParent(value);
}
/**
 * Returns if the target is a "child" of the tree of the given "parent" object.
 *
 * @param child Target object.
 * @param parent Parent object.
 * @returns
 */

function isChildOfParent(child, parent) {
  assertTweakedObject(child, "child");
  assertTweakedObject(parent, "parent");
  return getDeepObjectChildren(parent).deep.has(child);
}
/**
 * Returns if the target is a "parent" that has in its tree the given "child" object.
 *
 * @param parent Target object.
 * @param child Child object.
 * @returns
 */

function isParentOfChild(parent, child) {
  return isChildOfParent(child, parent);
}
var unresolved = {
  resolved: false
};
/**
 * Tries to resolve a path from an object.
 *
 * @typeparam T Returned value type.
 * @param pathRootObject Object that serves as path root.
 * @param path Path as an string or number array.
 * @returns An object with `{ resolved: true, value: T }` or `{ resolved: false }`.
 */

function resolvePath(pathRootObject, path) {
  // unit tests rely on this to work with any object
  // assertTweakedObject(pathRootObject, "pathRootObject")
  var current = modelToDataNode(pathRootObject);
  var len = path.length;

  for (var i = 0; i < len; i++) {
    if (!isObject(current)) {
      return unresolved;
    }

    var p = path[i]; // check just to avoid mobx warnings about trying to access out of bounds index

    if (isArray(current) && +p >= current.length) {
      return unresolved;
    }

    current = modelToDataNode(current[p]);
  }

  return {
    resolved: true,
    value: dataToModelNode(current)
  };
}
/**
 * @ignore
 */

var skipIdChecking = /*#__PURE__*/Symbol("skipIdChecking");
/**
 * @ignore
 *
 * Tries to resolve a path from an object while checking ids.
 *
 * @typeparam T Returned value type.
 * @param pathRootObject Object that serves as path root.
 * @param path Path as an string or number array.
 * @param pathIds An array of ids of the models that must be checked, null if not a model or `skipIdChecking` to skip it.
 * @returns An object with `{ resolved: true, value: T }` or `{ resolved: false }`.
 */

function resolvePathCheckingIds(pathRootObject, path, pathIds) {
  // unit tests rely on this to work with any object
  // assertTweakedObject(pathRootObject, "pathRootObject")
  var current = modelToDataNode(pathRootObject); // root id is never checked

  var len = path.length;

  for (var i = 0; i < len; i++) {
    if (!isObject(current)) {
      return {
        resolved: false
      };
    }

    var p = path[i]; // check just to avoid mobx warnings about trying to access out of bounds index

    if (isArray(current) && +p >= current.length) {
      return {
        resolved: false
      };
    }

    var currentMaybeModel = current[p];
    current = modelToDataNode(currentMaybeModel);
    var expectedId = pathIds[i];

    if (expectedId !== skipIdChecking) {
      var currentId = isModel(currentMaybeModel) ? currentMaybeModel[modelIdKey] : null;

      if (expectedId !== currentId) {
        return {
          resolved: false
        };
      }
    }
  }

  return {
    resolved: true,
    value: dataToModelNode(current)
  };
}
/**
 * Gets the path to get from a parent to a given child.
 * Returns an empty array if the child is actually the given parent or undefined if the child is not a child of the parent.
 *
 * @param fromParent
 * @param toChild
 * @returns
 */

function getParentToChildPath(fromParent, toChild) {
  assertTweakedObject(fromParent, "fromParent");
  assertTweakedObject(toChild, "toChild");

  if (fromParent === toChild) {
    return [];
  }

  var path = [];
  var current = toChild;
  var parentPath;

  while (parentPath = fastGetParentPath(current)) {
    path.unshift(parentPath.path);
    current = parentPath.parent;

    if (current === fromParent) {
      return path;
    }
  }

  return undefined;
}

var snapshots = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 */

function getInternalSnapshot(value) {
  return snapshots.get(value);
}

function getInternalSnapshotParent(sn, parentPath) {
  return (0,mobx_esm.untracked)(function () {
    if (!parentPath) {
      return undefined;
    }

    var parentSn = getInternalSnapshot(parentPath.parent);

    if (!parentSn) {
      return undefined;
    }

    return sn ? {
      parentSnapshot: parentSn,
      parentPath: parentPath
    } : undefined;
  });
}
/**
 * @ignore
 * @internal
 */


var unsetInternalSnapshot = /*#__PURE__*/(0,mobx_esm.action)("unsetInternalSnapshot", function (value) {
  var oldSn = getInternalSnapshot(value);

  if (oldSn) {
    snapshots["delete"](value);
    oldSn.atom.reportChanged();
  }
});
/**
 * @ignore
 * @internal
 */

var setInternalSnapshot = /*#__PURE__*/(0,mobx_esm.action)("setInternalSnapshot", function (value, standard) {
  var oldSn = getInternalSnapshot(value); // do not actually update if not needed

  if (oldSn && oldSn.standard === standard) {
    return;
  }

  debugFreeze(standard);
  var sn;

  if (oldSn) {
    sn = oldSn;
    sn.standard = standard;
  } else {
    sn = {
      standard: standard,
      atom: (0,mobx_esm.createAtom)("snapshot")
    };
    snapshots.set(value, sn);
  }

  sn.atom.reportChanged(); // also update parent(s) snapshot(s) if needed

  var parent = getInternalSnapshotParent(oldSn, fastGetParentPath(value));

  if (parent) {
    var parentSnapshot = parent.parentSnapshot,
        parentPath = parent.parentPath; // might be false in the cases where the parent has not yet been created

    if (parentSnapshot) {
      var path = parentPath.path; // patches for parent changes should not be emitted

      var parentStandardSn = parentSnapshot.standard;

      if (parentStandardSn[path] !== sn.standard) {
        if (Array.isArray(parentStandardSn)) {
          parentStandardSn = parentStandardSn.slice();
        } else {
          parentStandardSn = Object.assign({}, parentStandardSn);
        }

        parentStandardSn[path] = sn.standard;
        setInternalSnapshot(parentPath.parent, parentStandardSn);
      }
    }
  }
});
/**
 * @ignore
 */

function reportInternalSnapshotObserved(sn) {
  sn.atom.reportObserved();
}

/**
 * Retrieves an immutable snapshot for a data structure.
 * Since returned snapshots are immutable they will respect shallow equality, this is,
 * if no changes are made then the snapshot will be kept the same.
 *
 * @typeparam T Object type.
 * @param nodeOrPrimitive Data structure, including primtives.
 * @returns The snapshot.
 */

function getSnapshot(nodeOrPrimitive) {
  if (isPrimitive(nodeOrPrimitive)) {
    return nodeOrPrimitive;
  }

  assertTweakedObject(nodeOrPrimitive, "nodeOrPrimitive");
  var snapshot = getInternalSnapshot(nodeOrPrimitive);

  if (!snapshot) {
    throw failure("getSnapshot is not supported for this kind of object");
  }

  reportInternalSnapshotObserved(snapshot);
  return snapshot.standard;
}

var modelDataTypeCheckerSymbol = /*#__PURE__*/Symbol("modelDataTypeChecker");
var modelUnwrappedClassSymbol = /*#__PURE__*/Symbol("modelUnwrappedClass");

/**
 * Returns the associated data type for run-time checking (if any) to a model instance or class.
 *
 * @param modelClassOrInstance Model class or instance.
 * @returns The associated data type, or undefined if none.
 */

function getModelDataType(modelClassOrInstance) {
  if (isModel(modelClassOrInstance)) {
    return modelClassOrInstance.constructor[modelDataTypeCheckerSymbol];
  } else if (isModelClass(modelClassOrInstance)) {
    return modelClassOrInstance[modelDataTypeCheckerSymbol];
  } else {
    throw failure("modelClassOrInstance must be a model class or instance");
  }
}

/**
 * @ignore
 */

var modelInfoByName = {};
/**
 * @ignore
 */

var modelInfoByClass = /*#__PURE__*/new Map();
/**
 * @ignore
 */

function getModelInfoForName(name) {
  return modelInfoByName[name];
}

var modelPropertiesSymbol = /*#__PURE__*/Symbol("modelProperties");
/**
 * @ignore
 * @internal
 *
 * Gets the info related to a model class properties.
 *
 * @param modelClass
 */

function getInternalModelClassPropsInfo(modelClass) {
  return modelClass[modelPropertiesSymbol];
}
/**
 * @ignore
 * @internal
 *
 * Sets the info related to a model class properties.
 *
 * @param modelClass
 */

function setInternalModelClassPropsInfo(modelClass, props) {
  modelClass[modelPropertiesSymbol] = props;
}

/**
 * A type checking error.
 */

var TypeCheckError = /*#__PURE__*/function () {
  /**
   * Creates an instance of TypeError.
   * @param path Sub-path where the error occured.
   * @param expectedTypeName Name of the expected type.
   * @param actualValue Actual value.
   */
  function TypeCheckError(path, expectedTypeName, actualValue) {
    this.path = void 0;
    this.expectedTypeName = void 0;
    this.actualValue = void 0;
    this.path = path;
    this.expectedTypeName = expectedTypeName;
    this.actualValue = actualValue;
  }
  /**
   * Throws the type check error as an actual error.
   *
   * @param typeCheckedValue Usually the value where the type check was invoked.
   */


  var _proto = TypeCheckError.prototype;

  _proto["throw"] = function _throw(typeCheckedValue) {
    var msg = "TypeCheckError: ";
    var rootPath = [];

    if (isTweakedObject(typeCheckedValue, true)) {
      rootPath = getRootPath(typeCheckedValue).path;
    }

    msg += "[/" + [].concat(rootPath, this.path).join("/") + "] ";
    msg += "Expected: " + this.expectedTypeName;
    throw failure(msg);
  };

  return TypeCheckError;
}();

var emptyPath = [];
var typeCheckersWithCachedResultsOfObject = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 */

function invalidateCachedTypeCheckerResult(obj) {
  // we need to invalidate it for the object and all its parents
  var current = obj;

  while (current) {
    var set = typeCheckersWithCachedResultsOfObject.get(current);

    if (set) {
      for (var _iterator = _createForOfIteratorHelperLoose(set), _step; !(_step = _iterator()).done;) {
        var typeChecker = _step.value;
        typeChecker.invalidateCachedResult(current);
      }

      typeCheckersWithCachedResultsOfObject["delete"](current);
    }

    current = fastGetParentIncludingDataObjects(current);
  }
}
/**
 * @ignore
 */

var TypeChecker = /*#__PURE__*/function () {
  var _proto = TypeChecker.prototype;

  _proto.createCacheIfNeeded = function createCacheIfNeeded() {
    if (!this.checkResultCache) {
      this.checkResultCache = new WeakMap();
    }

    return this.checkResultCache;
  };

  _proto.setCachedResult = function setCachedResult(obj, newCacheValue) {
    this.createCacheIfNeeded().set(obj, newCacheValue); // register this type checker as listener of that object changes

    var typeCheckerSet = typeCheckersWithCachedResultsOfObject.get(obj);

    if (!typeCheckerSet) {
      typeCheckerSet = new Set();
      typeCheckersWithCachedResultsOfObject.set(obj, typeCheckerSet);
    }

    typeCheckerSet.add(this);
  };

  _proto.invalidateCachedResult = function invalidateCachedResult(obj) {
    if (this.checkResultCache) {
      this.checkResultCache["delete"](obj);
    }
  };

  _proto.getCachedResult = function getCachedResult(obj) {
    return this.checkResultCache ? this.checkResultCache.get(obj) : undefined;
  };

  _proto.check = function check(value, path) {
    if (this.unchecked) {
      return null;
    }

    if (!isTweakedObject(value, true)) {
      return this._check(value, path);
    } // optimized checking with cached values


    var cachedResult = this.getCachedResult(value);

    if (cachedResult === undefined) {
      // we set the path empty since the resoult could be used for another paths other than this base
      cachedResult = this._check(value, emptyPath);
      this.setCachedResult(value, cachedResult);
    }

    if (cachedResult) {
      return new TypeCheckError([].concat(path, cachedResult.path), cachedResult.expectedTypeName, cachedResult.actualValue);
    } else {
      return null;
    }
  };

  _createClass(TypeChecker, [{
    key: "typeInfo",
    get: function get() {
      return this._cachedTypeInfoGen(this);
    }
  }]);

  function TypeChecker(_check, getTypeName, typeInfoGen) {
    this._check = void 0;
    this.getTypeName = void 0;
    this.checkResultCache = void 0;
    this.unchecked = void 0;
    this._cachedTypeInfoGen = void 0;
    this._check = _check;
    this.getTypeName = getTypeName;
    this.unchecked = !_check;
    this._cachedTypeInfoGen = lateVal(typeInfoGen);
  }

  return TypeChecker;
}();
var lateTypeCheckerSymbol = /*#__PURE__*/Symbol("lateTypeCheker");
/**
 * @ignore
 */

function lateTypeChecker(fn, typeInfoGen) {
  var cached;

  var ltc = function ltc() {
    if (cached) {
      return cached;
    }

    cached = fn();
    return cached;
  };

  ltc[lateTypeCheckerSymbol] = true;
  var cachedTypeInfoGen = lateVal(typeInfoGen);
  Object.defineProperty(ltc, "typeInfo", {
    enumerable: true,
    configurable: true,
    get: function get() {
      return cachedTypeInfoGen(ltc);
    }
  });
  return ltc;
}
/**
 * @ignore
 */

function isLateTypeChecker(ltc) {
  return typeof ltc === "function" && ltc[lateTypeCheckerSymbol];
}
/**
 * Type info base class.
 */

var TypeInfo = function TypeInfo(thisType) {
  this.thisType = void 0;
  this.thisType = thisType;
};
/**
 * Gets the type info of a given type.
 *
 * @param type Type to get the info from.
 * @returns The type info.
 */

function getTypeInfo(type) {
  var stdType = resolveStandardType(type);
  var typeInfo = stdType.typeInfo;

  if (!typeInfo) {
    throw failure("type info not found for " + type);
  }

  return typeInfo;
}

/**
 * A refinement over a given type. This allows you to do extra checks
 * over models, ensure numbers are integers, etc.
 *
 * Example:
 * ```ts
 * const integerType = types.refinement(types.number, (n) => {
 *   return Number.isInteger(n)
 * }, "integer")
 *
 * const sumModelType = types.refinement(types.model<Sum>(Sum), (sum) => {
 *   // imagine that for some reason sum includes a number 'a', a number 'b'
 *   // and the result
 *
 *   const rightResult = sum.a + sum.b === sum.result
 *
 *   // simple mode that will just return that the whole model is incorrect
 *   return rightResult
 *
 *   // this will return that the result field is wrong
 *   return rightResult ? null : new TypeCheckError(["result"], "a+b", sum.result)
 * })
 * ```
 *
 * @template T Base type.
 * @param baseType Base type.
 * @param checkFn Function that will receive the data (if it passes the base type
 * check) and return null or false if there were no errors or either a TypeCheckError instance or
 * true if there were.
 * @returns
 */

function typesRefinement(baseType, checkFn, typeName) {
  var typeInfoGen = function typeInfoGen(t) {
    return new RefinementTypeInfo(t, resolveStandardType(baseType), checkFn, typeName);
  };

  return lateTypeChecker(function () {
    var baseChecker = resolveTypeChecker(baseType);

    var getTypeName = function getTypeName() {
      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      var baseTypeName = baseChecker.getTypeName.apply(baseChecker, recursiveTypeCheckers.concat([baseChecker]));
      var refinementName = typeName || "refinementOf";
      return refinementName + "<" + baseTypeName + ">";
    };

    var thisTc = new TypeChecker(function (data, path) {
      var baseErr = baseChecker.check(data, path);

      if (baseErr) {
        return baseErr;
      }

      var refinementErr = checkFn(data);

      if (refinementErr === true) {
        return null;
      } else if (refinementErr === false) {
        return new TypeCheckError([], getTypeName(thisTc), data);
      } else {
        return refinementErr != null ? refinementErr : null;
      }
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.refinement` type info.
 */

var RefinementTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(RefinementTypeInfo, _TypeInfo);

  _createClass(RefinementTypeInfo, [{
    key: "baseTypeInfo",
    get: function get() {
      return getTypeInfo(this.baseType);
    }
  }]);

  function RefinementTypeInfo(thisType, baseType, checkFunction, typeName) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.baseType = void 0;
    _this.checkFunction = void 0;
    _this.typeName = void 0;
    _this.baseType = baseType;
    _this.checkFunction = checkFunction;
    _this.typeName = typeName;
    return _this;
  }

  return RefinementTypeInfo;
}(TypeInfo);

/**
 * A type that represents a certain value of a primitive (for example an *exact* number or string).
 *
 * Example
 * ```ts
 * const hiType = types.literal("hi") // the string with value "hi"
 * const number5Type = types.literal(5) // the number with value 5
 * ```
 *
 * @typeparam T Literal value type.
 * @param literal Literal value.
 * @returns
 */

function typesLiteral(literal) {
  assertIsPrimitive(literal, "literal");
  var typeName;

  switch (literal) {
    case undefined:
      typeName = "undefined";
      break;

    case null:
      typeName = "null";
      break;

    default:
      typeName = JSON.stringify(literal);
      break;
  }

  var typeInfoGen = function typeInfoGen(t) {
    return new LiteralTypeInfo(t, literal);
  };

  return new TypeChecker(function (value, path) {
    return value === literal ? null : new TypeCheckError(path, typeName, value);
  }, function () {
    return typeName;
  }, typeInfoGen);
}
/**
 * `types.literal` type info.
 */

var LiteralTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(LiteralTypeInfo, _TypeInfo);

  function LiteralTypeInfo(thisType, literal) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.literal = void 0;
    _this.literal = literal;
    return _this;
  }

  return LiteralTypeInfo;
}(TypeInfo);
/**
 * A type that represents the value undefined.
 * Syntactic sugar for `types.literal(undefined)`.
 *
 * ```ts
 * types.undefined
 * ```
 */

var typesUndefined = /*#__PURE__*/typesLiteral(undefined);
/**
 * A type that represents the value null.
 * Syntactic sugar for `types.literal(null)`.
 *
 * ```ts
 * types.null
 * ```
 */

var typesNull = /*#__PURE__*/typesLiteral(null);
/**
 * A type that represents any boolean value.
 *
 * ```ts
 * types.boolean
 * ```
 */

var typesBoolean = /*#__PURE__*/new TypeChecker(function (value, path) {
  return typeof value === "boolean" ? null : new TypeCheckError(path, "boolean", value);
}, function () {
  return "boolean";
}, function (t) {
  return new BooleanTypeInfo(t);
});
/**
 * `types.boolean` type info.
 */

var BooleanTypeInfo = /*#__PURE__*/function (_TypeInfo2) {
  _inheritsLoose(BooleanTypeInfo, _TypeInfo2);

  function BooleanTypeInfo() {
    return _TypeInfo2.apply(this, arguments) || this;
  }

  return BooleanTypeInfo;
}(TypeInfo);
/**
 * A type that represents any number value.
 *
 * ```ts
 * types.number
 * ```
 */

var typesNumber = /*#__PURE__*/new TypeChecker(function (value, path) {
  return typeof value === "number" ? null : new TypeCheckError(path, "number", value);
}, function () {
  return "number";
}, function (t) {
  return new NumberTypeInfo(t);
});
/**
 * `types.number` type info.
 */

var NumberTypeInfo = /*#__PURE__*/function (_TypeInfo3) {
  _inheritsLoose(NumberTypeInfo, _TypeInfo3);

  function NumberTypeInfo() {
    return _TypeInfo3.apply(this, arguments) || this;
  }

  return NumberTypeInfo;
}(TypeInfo);
/**
 * A type that represents any string value.
 *
 * ```ts
 * types.string
 * ```
 */

var typesString = /*#__PURE__*/new TypeChecker(function (value, path) {
  return typeof value === "string" ? null : new TypeCheckError(path, "string", value);
}, function () {
  return "string";
}, function (t) {
  return new StringTypeInfo(t);
});
/**
 * `types.string` type info.
 */

var StringTypeInfo = /*#__PURE__*/function (_TypeInfo4) {
  _inheritsLoose(StringTypeInfo, _TypeInfo4);

  function StringTypeInfo() {
    return _TypeInfo4.apply(this, arguments) || this;
  }

  return StringTypeInfo;
}(TypeInfo);
/**
 * A type that represents any integer number value.
 * Syntactic sugar for `types.refinement(types.number, n => Number.isInteger(n), "integer")`
 *
 * ```ts
 * types.integer
 * ```
 */

var typesInteger = /*#__PURE__*/typesRefinement(typesNumber, function (n) {
  return Number.isInteger(n);
}, "integer");
/**
 * A type that represents any string value other than "".
 * Syntactic sugar for `types.refinement(types.string, s => s !== "", "nonEmpty")`
 *
 * ```ts
 * types.nonEmptyString
 * ```
 */

var typesNonEmptyString = /*#__PURE__*/typesRefinement(typesString, function (s) {
  return s !== "";
}, "nonEmpty");

/**
 * @ignore
 */

function resolveTypeChecker(v) {
  var next = v;

  while (true) {
    if (next instanceof TypeChecker) {
      return next;
    } else if (v === String) {
      return typesString;
    } else if (v === Number) {
      return typesNumber;
    } else if (v === Boolean) {
      return typesBoolean;
    } else if (v === null) {
      return typesNull;
    } else if (v === undefined) {
      return typesUndefined;
    } else if (isLateTypeChecker(next)) {
      next = next();
    } else {
      throw failure("type checker could not be resolved");
    }
  }
}
/**
 * @ignore
 */

function resolveStandardType(v) {
  if (v instanceof TypeChecker || isLateTypeChecker(v)) {
    return v;
  } else if (v === String) {
    return typesString;
  } else if (v === Number) {
    return typesNumber;
  } else if (v === Boolean) {
    return typesBoolean;
  } else if (v === null) {
    return typesNull;
  } else if (v === undefined) {
    return typesUndefined;
  } else {
    throw failure("standard type could not be resolved");
  }
}

var cachedModelTypeChecker = /*#__PURE__*/new WeakMap();
/**
 * A type that represents a model. The type referenced in the model decorator will be used for type checking.
 *
 * Example:
 * ```ts
 * const someModelType = types.model<SomeModel>(SomeModel)
 * // or for recursive models
 * const someModelType = types.model<SomeModel>(() => SomeModel)
 * ```
 *
 * @typeparam M Model type.
 * @param modelClass Model class.
 * @returns
 */

function typesModel(modelClass) {
  // if we type it any stronger then recursive defs and so on stop working
  if (!isModelClass(modelClass) && typeof modelClass === "function") {
    // resolve later
    var typeInfoGen = function typeInfoGen(t) {
      return new ModelTypeInfo(t, modelClass());
    };

    return lateTypeChecker(function () {
      return typesModel(modelClass());
    }, typeInfoGen);
  } else {
    var modelClazz = modelClass;
    assertIsModelClass(modelClazz, "modelClass");
    var cachedTypeChecker = cachedModelTypeChecker.get(modelClazz);

    if (cachedTypeChecker) {
      return cachedTypeChecker;
    }

    var _typeInfoGen = function _typeInfoGen(t) {
      return new ModelTypeInfo(t, modelClazz);
    };

    var tc = lateTypeChecker(function () {
      var modelInfo = modelInfoByClass.get(modelClazz);
      var typeName = "Model(" + modelInfo.name + ")";
      return new TypeChecker(function (value, path) {
        if (!(value instanceof modelClazz)) {
          return new TypeCheckError(path, typeName, value);
        }

        var dataTypeChecker = getModelDataType(value);

        if (!dataTypeChecker) {
          throw failure("type checking cannot be performed over model of type '" + modelInfo.name + "' at path " + path.join("/") + " since that model type has no data type declared, consider adding a data type or using types.unchecked() instead");
        }

        var resolvedTc = resolveTypeChecker(dataTypeChecker);

        if (!resolvedTc.unchecked) {
          return resolvedTc.check(value.$, path);
        }

        return null;
      }, function () {
        return typeName;
      }, _typeInfoGen);
    }, _typeInfoGen);
    cachedModelTypeChecker.set(modelClazz, tc);
    return tc;
  }
}
/**
 * `types.model` type info.
 */

var ModelTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(ModelTypeInfo, _TypeInfo);

  _createClass(ModelTypeInfo, [{
    key: "props",
    get: function get() {
      return this._props();
    }
  }, {
    key: "modelType",
    get: function get() {
      var modelInfo = modelInfoByClass.get(this.modelClass);
      return modelInfo.name;
    }
  }]);

  function ModelTypeInfo(thisType, modelClass) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.modelClass = void 0;
    _this._props = lateVal(function () {
      var objSchema = getInternalModelClassPropsInfo(_this.modelClass);
      var propTypes = {};
      Object.keys(objSchema).forEach(function (propName) {
        var propData = objSchema[propName];
        var type = propData.typeChecker;
        var typeInfo;

        if (type) {
          typeInfo = getTypeInfo(type);
        }

        var hasDefault = false;
        var defaultValue;

        if (propData.defaultFn !== noDefaultValue) {
          defaultValue = propData.defaultFn;
          hasDefault = true;
        } else if (propData.defaultValue !== noDefaultValue) {
          defaultValue = propData.defaultValue;
          hasDefault = true;
        }

        propTypes[propName] = {
          type: type,
          typeInfo: typeInfo,
          hasDefault: hasDefault,
          "default": defaultValue
        };
      });
      return propTypes;
    });
    _this.modelClass = modelClass;
    return _this;
  }

  return ModelTypeInfo;
}(TypeInfo);

/**
 * Checks if a value conforms to a given type.
 *
 * @typename S Type.
 * @param type Type to check for.
 * @param value Value to check.
 * @returns A TypeError if the check fails or null if no error.
 */

function typeCheck(type, value) {
  var typeChecker = resolveTypeChecker(type);

  if (typeChecker.unchecked) {
    return null;
  } else {
    return typeChecker.check(value, []);
  }
}

/**
 * @ignore
 * @internal
 */

function canWrite() {
  return !getActionProtection() || !!getCurrentActionContext();
}
/**
 * @ignore
 * @internal
 */

function assertCanWrite() {
  if (!canWrite()) {
    throw failure("data changes must be performed inside model actions");
  }
}
var actionProtection = true;
/**
 * @ignore
 * @internal
 *
 * Gets if the action protection is currently enabled or not.
 *
 * @returns
 */

function getActionProtection() {
  return actionProtection;
} // base case

function runUnprotected(arg1, arg2) {
  var name = typeof arg1 === "string" ? arg1 : undefined;
  var fn = typeof arg1 === "string" ? arg2 : arg1;

  var innerAction = function innerAction() {
    var oldActionProtection = actionProtection;
    actionProtection = false;

    try {
      return fn();
    } finally {
      actionProtection = oldActionProtection;
      tryRunPendingActions();
    }
  };

  if (name) {
    return (0,mobx_esm.action)(name, innerAction)();
  } else {
    return (0,mobx_esm.action)(innerAction)();
  }
}

var pendingActions = [];

function isActionRunning() {
  return !getActionProtection() || getCurrentActionContext();
}
/**
 * @ignore
 */


function enqueuePendingAction(action) {
  // delay action until all current actions are finished
  if (isActionRunning()) {
    pendingActions.push(action);
  } else {
    action();
  }
}
/**
 * @ignore
 */

function tryRunPendingActions() {
  if (isActionRunning()) {
    return false;
  }

  while (pendingActions.length > 0) {
    var nextAction = pendingActions.shift();
    nextAction();
  }

  return true;
}

/**
 * A hook action.
 */
var HookAction;

(function (HookAction) {
  /**
   * onInit hook
   */
  HookAction["OnInit"] = "$$onInit";
  /**
   * onAttachedToRootStore hook
   */

  HookAction["OnAttachedToRootStore"] = "$$onAttachedToRootStore";
  /**
   * disposer for onAttachedToRootStore hook
   */

  HookAction["OnAttachedToRootStoreDisposer"] = "$$onAttachedToRootStoreDisposer";
})(HookAction || (HookAction = {}));

var hookActionValues = /*#__PURE__*/new Set( /*#__PURE__*/Object.values(HookAction));
/**
 * Returns if a given action name corresponds to a hook, this is, one of:
 * - onInit() hook
 * - onAttachedToRootStore() hook
 * - disposer returned by a onAttachedToRootStore() hook
 *
 * @param actionName Action name to check.
 * @returns true if it is a hook, false otherwise.
 */

function isHookAction(actionName) {
  return hookActionValues.has(actionName);
}

var perObjectActionMiddlewares = /*#__PURE__*/new WeakMap();
var perObjectActionMiddlewaresIterator = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 *
 * Gets the current action middlewares to be run over a given object as an iterable object.
 *
 * @returns
 */

function getActionMiddlewares(obj) {
  // when we call a middleware we will call the middlewares of that object plus all parent objects
  // the parent object middlewares are run last
  // since an array like [a, b, c] will be called like c(b(a())) this means that we need to put
  // the parent object ones at the end of the array
  var iterable = perObjectActionMiddlewaresIterator.get(obj);

  if (!iterable) {
    var _iterable;

    iterable = (_iterable = {}, _iterable[Symbol.iterator] = function () {
      var current = obj;

      function getCurrentIterator() {
        var objMwares = current ? perObjectActionMiddlewares.get(current) : undefined;

        if (!objMwares || objMwares.length <= 0) {
          return undefined;
        }

        return objMwares[Symbol.iterator]();
      }

      function findNextIterator() {
        var nextIter;

        while (current && !nextIter) {
          current = fastGetParent(current);
          nextIter = getCurrentIterator();
        }

        return nextIter;
      }

      var iter = getCurrentIterator();

      if (!iter) {
        iter = findNextIterator();
      }

      var iterator = {
        next: function next() {
          if (!iter) {
            return {
              value: undefined,
              done: true
            };
          }

          var result = iter.next();

          if (!result.done) {
            return result;
          }

          iter = findNextIterator();
          return this.next();
        }
      };
      return iterator;
    }, _iterable);
    perObjectActionMiddlewaresIterator.set(obj, iterable);
  }

  return iterable;
}
/**
 * Adds a global action middleware to be run when an action is performed.
 * It is usually preferable to use `onActionMiddleware` instead to limit it to a given tree and only to topmost level actions
 * or `actionTrackingMiddleware` for a simplified middleware.
 *
 * @param mware Action middleware to be run.
 * @returns A disposer to cancel the middleware. Note that if you don't plan to do an early disposal of the middleware
 * calling this function becomes optional.
 */

function addActionMiddleware(mware) {
  assertIsObject(mware, "middleware");
  var middleware = mware.middleware,
      filter = mware.filter,
      subtreeRoot = mware.subtreeRoot;
  assertTweakedObject(subtreeRoot, "middleware.subtreeRoot");
  assertIsFunction(middleware, "middleware.middleware");

  if (filter && typeof filter !== "function") {
    throw failure("middleware.filter must be a function or undefined");
  }

  if (!(0,mobx_esm.isAction)(middleware)) {
    middleware = (0,mobx_esm.action)(middleware.name || "actionMiddleware", middleware);
  }

  if (subtreeRoot) {
    var targetFilter = function targetFilter(ctx) {
      return ctx.target === subtreeRoot || isChildOfParent(ctx.target, subtreeRoot);
    };

    if (!filter) {
      filter = targetFilter;
    } else {
      var customFilter = filter;

      filter = function filter(ctx) {
        return targetFilter(ctx) && customFilter(ctx);
      };
    }
  }

  var actualMware = {
    middleware: middleware,
    filter: filter
  };
  var objMwares = perObjectActionMiddlewares.get(subtreeRoot);

  if (!objMwares) {
    objMwares = [actualMware];
    perObjectActionMiddlewares.set(subtreeRoot, objMwares);
  } else {
    objMwares.push(actualMware);
  }

  return function () {
    deleteFromArray(objMwares, actualMware);
  };
}

/**
 * @ignore
 */

var modelActionSymbol = /*#__PURE__*/Symbol("modelAction");
/**
 * @ignore
 */

function wrapInAction(_ref) {
  var name = _ref.name,
      fn = _ref.fn,
      actionType = _ref.actionType,
      overrideContext = _ref.overrideContext,
      _ref$isFlowFinisher = _ref.isFlowFinisher,
      isFlowFinisher = _ref$isFlowFinisher === void 0 ? false : _ref$isFlowFinisher;
  var wrappedAction = (0,mobx_esm.action)(name, function () {
    var target = this;

    if (inDevMode()) {
      assertTweakedObject(target, "wrappedAction");
    }

    var parentContext = getCurrentActionContext();
    var context = {
      actionName: name,
      type: actionType,
      target: target,
      args: Array.from(arguments),
      parentContext: parentContext,
      data: {},
      rootContext: undefined
    };

    if (overrideContext) {
      overrideContext(context);
    }

    if (!context.rootContext) {
      if (context.previousAsyncStepContext) {
        context.rootContext = context.previousAsyncStepContext.rootContext;
      } else if (context.parentContext) {
        context.rootContext = context.parentContext.rootContext;
      } else {
        context.rootContext = context;
      }
    }

    setCurrentActionContext(context);
    var mwareFn = fn.bind.apply(fn, [target].concat(Array.prototype.slice.call(arguments)));
    var mwareIter = getActionMiddlewares(target)[Symbol.iterator]();
    var mwareCur = mwareIter.next();

    while (!mwareCur.done) {
      var mware = mwareCur.value;
      var filterPassed = mware.filter ? mware.filter(context) : true;

      if (filterPassed) {
        mwareFn = mware.middleware.bind(undefined, context, mwareFn);
      }

      mwareCur = mwareIter.next();
    }

    try {
      var ret = mwareFn();

      if (isFlowFinisher) {
        var flowFinisher = ret;
        var value = flowFinisher.value;

        if (flowFinisher.resolution === "accept") {
          flowFinisher.accepter(value);
        } else {
          flowFinisher.rejecter(value);
        }

        return value; // not sure if this is even needed
      } else {
        return ret;
      }
    } finally {
      setCurrentActionContext(context.parentContext);
      tryRunPendingActions();
    }
  });
  wrappedAction[modelActionSymbol] = true;
  return wrappedAction;
}
/**
 * @ignore
 */

function wrapModelMethodInActionIfNeeded(model, propertyKey, name) {
  var fn = model[propertyKey];

  if (isModelAction(fn)) {
    return;
  }

  var wrappedFn = wrapInAction({
    name: name,
    fn: fn,
    actionType: ActionContextActionType.Sync
  });
  var proto = Object.getPrototypeOf(model);
  var protoFn = proto[propertyKey];

  if (protoFn === fn) {
    proto[propertyKey] = wrappedFn;
  } else {
    model[propertyKey] = wrappedFn;
  }
}

/**
 * Mode for the `walkTree` method.
 */

var WalkTreeMode;

(function (WalkTreeMode) {
  /**
   * The walk will be done parent (roots) first, then children.
   */
  WalkTreeMode["ParentFirst"] = "parentFirst";
  /**
   * The walk will be done children (leafs) first, then parents.
   */

  WalkTreeMode["ChildrenFirst"] = "childrenFirst";
})(WalkTreeMode || (WalkTreeMode = {}));
/**
 * Walks a tree, running the predicate function for each node.
 * If the predicate function returns something other than undefined,
 * then the walk will be stopped and the function will return the returned value.
 *
 * @typeparam T Returned object type, defaults to void.
 * @param target Subtree root object.
 * @param predicate Function that will be run for each node of the tree.
 * @param mode Mode to walk the tree, as defined in `WalkTreeMode`.
 * @returns
 */


function walkTree(target, predicate, mode) {
  assertTweakedObject(target, "target");

  if (mode === WalkTreeMode.ParentFirst) {
    var recurse = function recurse(child) {
      return walkTreeParentFirst(child, predicate, recurse);
    };

    return walkTreeParentFirst(target, predicate, recurse);
  } else {
    var _recurse = function _recurse(child) {
      return walkTreeChildrenFirst(child, predicate, _recurse);
    };

    return walkTreeChildrenFirst(target, predicate, _recurse);
  }
}

function walkTreeParentFirst(target, rootPredicate, recurse) {
  var ret = rootPredicate(target);

  if (ret !== undefined) {
    return ret;
  }

  var childrenIter = getObjectChildren(target).values();
  var ch = childrenIter.next();

  while (!ch.done) {
    var _ret = recurse(ch.value);

    if (_ret !== undefined) {
      return _ret;
    }

    ch = childrenIter.next();
  }

  return undefined;
}

function walkTreeChildrenFirst(target, rootPredicate, recurse) {
  var childrenIter = getObjectChildren(target).values();
  var ch = childrenIter.next();

  while (!ch.done) {
    var _ret2 = recurse(ch.value);

    if (_ret2 !== undefined) {
      return _ret2;
    }

    ch = childrenIter.next();
  }

  var ret = rootPredicate(target);

  if (ret !== undefined) {
    return ret;
  }

  return undefined;
}
/**
 * @ignore
 * @internal
 */


function computedWalkTreeAggregate(predicate) {
  var computedFns = new WeakMap();

  var getComputedTreeResult = function getComputedTreeResult(tree) {
    var cmpted = computedFns.get(tree);

    if (!cmpted) {
      cmpted = (0,mobx_esm.computed)(function () {
        return walkTreeAggregate(tree, predicate, recurse);
      });
      computedFns.set(tree, cmpted);
    }

    return cmpted.get();
  };

  var recurse = function recurse(ch) {
    return getComputedTreeResult(ch);
  };

  return {
    walk: function walk(target) {
      return getComputedTreeResult(target);
    }
  };
}

function walkTreeAggregate(target, rootPredicate, recurse) {
  var map;
  var rootVal = rootPredicate(target);
  var childrenMap = getObjectChildren(target);
  var childrenIter = childrenMap.values();
  var ch = childrenIter.next(); // small optimization, if there is only one child and this
  // object provides no value we can just reuse the child ones

  if (rootVal === undefined && childrenMap.size === 1) {
    return recurse(ch.value);
  }

  while (!ch.done) {
    var childMap = recurse(ch.value);

    if (childMap) {
      if (!map) {
        map = new Map();
      } // add child map keys/values to own map


      var mapIter = childMap.keys();
      var mapCur = mapIter.next();

      while (!mapCur.done) {
        var key = mapCur.value;
        var val = childMap.get(key);
        map.set(key, val);
        mapCur = mapIter.next();
      }
    }

    ch = childrenIter.next();
  } // add it at the end so parent resolutions have higher
  // priority than child ones


  if (rootVal !== undefined) {
    if (!map) {
      map = new Map();
    }

    map.set(rootVal, target);
  }

  return map;
}

var onAttachedDisposers = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 */

var attachToRootStore = /*#__PURE__*/(0,mobx_esm.action)("attachToRootStore", function (rootStore, child) {
  // we use an array to ensure they will get called even if the actual hook modifies the tree
  var childrenToCall = [];
  walkTree(child, function (ch) {
    if (ch instanceof BaseModel && ch.onAttachedToRootStore) {
      wrapModelMethodInActionIfNeeded(ch, "onAttachedToRootStore", HookAction.OnAttachedToRootStore);
      childrenToCall.push(ch);
    }
  }, WalkTreeMode.ParentFirst);
  var childrenToCallLen = childrenToCall.length;

  for (var i = 0; i < childrenToCallLen; i++) {
    var ch = childrenToCall[i];
    var disposer = ch.onAttachedToRootStore(rootStore);

    if (disposer) {
      onAttachedDisposers.set(ch, disposer);
    }
  }
});
/**
 * @ignore
 * @internal
 */

var detachFromRootStore = /*#__PURE__*/(0,mobx_esm.action)("detachFromRootStore", function (child) {
  // we use an array to ensure they will get called even if the actual hook modifies the tree
  var disposersToCall = [];
  walkTree(child, function (ch) {
    var disposer = onAttachedDisposers.get(ch);

    if (disposer) {
      // wrap disposer in action
      var disposerAction = wrapInAction({
        name: HookAction.OnAttachedToRootStoreDisposer,
        fn: disposer,
        actionType: ActionContextActionType.Sync
      }).bind(ch);
      onAttachedDisposers["delete"](ch);
      disposersToCall.push(disposerAction);
    }
  }, WalkTreeMode.ChildrenFirst);
  var disposersToCallLen = disposersToCall.length;

  for (var i = 0; i < disposersToCallLen; i++) {
    disposersToCall[i]();
  }
});

var rootStores = /*#__PURE__*/new WeakSet();
var rootStoreAtoms = /*#__PURE__*/new WeakMap();
/**
 * Registers a model / tree node object as a root store tree.
 * Marking a model object as a root store tree serves several purposes:
 * - It allows the `onAttachedToRootStore` hook (plus disposer) to be invoked on models once they become part of this tree.
 *   These hooks can be used for example to attach effects and serve as some sort of initialization.
 * - It allows auto detachable references to work properly.
 *
 * @typeparam T Object type.
 * @param node Node object to register as root store.
 * @returns The same model object that was passed.
 */

var registerRootStore = /*#__PURE__*/(0,mobx_esm.action)("registerRootStore", function (node) {
  assertTweakedObject(node, "node");

  if (rootStores.has(node)) {
    throw failure("object already registered as root store");
  }

  if (!isRoot(node)) {
    throw failure("a root store must not have a parent");
  }

  rootStores.add(node);
  attachToRootStore(node, node);
  getOrCreateRootStoreAtom(node).reportChanged();
  return node;
});
/**
 * Unregisters an object to mark it as no longer a root store.
 *
 * @param node Node object to unregister as root store.
 */

var unregisterRootStore = /*#__PURE__*/(0,mobx_esm.action)("unregisterRootStore", function (node) {
  if (!isRootStore(node)) {
    throw failure("not a root store");
  }

  rootStores["delete"](node);
  detachFromRootStore(node);
  getOrCreateRootStoreAtom(node).reportChanged();
});
/**
 * Checks if a given object is marked as a root store.
 *
 * @param node Object.
 * @returns
 */

function isRootStore(node) {
  assertTweakedObject(node, "node");
  return fastIsRootStore(node);
}
/**
 * @ignore
 * @internal
 */

function fastIsRootStore(node) {
  getOrCreateRootStoreAtom(node).reportObserved();
  return rootStores.has(node);
}
/**
 * Gets the root store of a given tree child, or undefined if none.
 *
 * @typeparam T Root store type.
 * @param node Target to find the root store for.
 * @returns
 */

function getRootStore(node) {
  assertTweakedObject(node, "node");
  return fastGetRootStore(node);
}
/**
 * @ignore
 * @internal
 */

function fastGetRootStore(node) {
  var root = fastGetRoot(node);
  return fastIsRootStore(root) ? root : undefined;
}

function getOrCreateRootStoreAtom(node) {
  var atom = rootStoreAtoms.get(node);

  if (!atom) {
    atom = (0,mobx_esm.createAtom)("rootStore");
    rootStoreAtoms.set(node, atom);
  }

  return atom;
}

/**
 * @ignore
 * @internal
 */

var setParent = /*#__PURE__*/(0,mobx_esm.action)("setParent", function (value, parentPath, indexChangeAllowed, isDataObject) {
  if (isPrimitive(value)) {
    return;
  }

  if (inDevMode()) {
    if (typeof value === "function" || typeof value === "symbol") {
      throw failure("assertion failed: value cannot be a function or a symbol");
    }

    if (!isTweakedObject(value, true)) {
      throw failure("assertion failed: value is not ready to take a parent");
    }

    if (parentPath) {
      if (!isTweakedObject(parentPath.parent, true)) {
        throw failure("assertion failed: parent is not ready to take children");
      }
    }
  }

  if (isDataObject) {
    dataObjectParent.set(value, parentPath.parent); // data object will proxy to use the actual parent model for child/parent stuff

    return;
  }

  initializeObjectChildren(value); // make sure the new parent actually points to models when we give model data objs

  if (parentPath) {
    var actualParent = dataToModelNode(parentPath.parent);

    if (parentPath.parent !== actualParent) {
      parentPath = {
        parent: actualParent,
        path: parentPath.path
      };
    }
  }

  var oldParentPath = fastGetParentPath(value);

  if (parentPathEquals(oldParentPath, parentPath)) {
    return;
  }

  if (isRootStore(value)) {
    throw failure("root stores cannot be attached to any parents");
  }

  if (oldParentPath && parentPath) {
    if (oldParentPath.parent === parentPath.parent && indexChangeAllowed) {
      // just changing the index
      objectParents.set(value, parentPath);
      reportParentPathChanged(value);
      return;
    } else {
      throw failure("an object cannot be assigned a new parent when it already has one");
    }
  }

  var removeFromOldParent = function removeFromOldParent() {
    if (oldParentPath == null ? void 0 : oldParentPath.parent) {
      removeObjectChild(oldParentPath.parent, value);
    }
  };

  var attachToNewParent = function attachToNewParent() {
    var _parentPath;

    objectParents.set(value, parentPath);

    if ((_parentPath = parentPath) == null ? void 0 : _parentPath.parent) {
      addObjectChild(parentPath.parent, value);
    }

    reportParentPathChanged(value);
  };

  if (value instanceof BaseModel) {
    var oldRoot = fastGetRoot(value);
    var oldRootStore = isRootStore(oldRoot) ? oldRoot : undefined;
    removeFromOldParent();
    attachToNewParent();
    var newRoot = fastGetRoot(value);
    var newRootStore = isRootStore(newRoot) ? newRoot : undefined; // invoke model root store events

    if (oldRootStore !== newRootStore && (oldRootStore || newRootStore)) {
      enqueuePendingAction(function () {
        if (oldRootStore) {
          detachFromRootStore(value);
        }

        if (newRootStore) {
          attachToRootStore(newRootStore, value);
        }
      });
    }
  } else {
    removeFromOldParent();
    attachToNewParent();
  }
});

/**
 * @ignore
 */

function tweakModel(value, parentPath) {
  tweakedObjects.set(value, undefined);
  setParent(value, parentPath, false, false); // nothing to do for models, data is already proxified and its parent is set
  // for snapshots we will use its "$" object snapshot directly

  return value;
}

/**
 * @ignore
 * @internal
 */

var InternalPatchRecorder = /*#__PURE__*/function () {
  function InternalPatchRecorder() {
    this.patches = void 0;
    this.invPatches = void 0;
  }

  var _proto = InternalPatchRecorder.prototype;

  _proto.record = function record(patches, invPatches) {
    this.patches = patches;
    this.invPatches = invPatches;
  };

  _proto.emit = function emit(obj) {
    emitPatch(obj, this.patches, this.invPatches, true);
  };

  return InternalPatchRecorder;
}();
var patchListeners = /*#__PURE__*/new WeakMap();
var globalPatchListeners = [];
/**
 * Adds a listener that will be called every time a patch is generated for the tree of the given target object.
 *
 * @param subtreeRoot Subtree root object of the patch listener.
 * @param listener The listener function that will be called everytime a patch is generated for the object or its children.
 * @returns A disposer to stop listening to patches.
 */

function onPatches(subtreeRoot, listener) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  assertIsFunction(listener, "listener");

  if (!(0,mobx_esm.isAction)(listener)) {
    listener = (0,mobx_esm.action)(listener.name || "onPatchesListener", listener);
  }

  var listenersForObject = patchListeners.get(subtreeRoot);

  if (!listenersForObject) {
    listenersForObject = [];
    patchListeners.set(subtreeRoot, listenersForObject);
  }

  listenersForObject.push(listener);
  return function () {
    deleteFromArray(listenersForObject, listener);
  };
}
/**
 * Adds a listener that will be called every time a patch is generated anywhere.
 * Usually prefer using `onPatches`.
 *
 * @param listener The listener function that will be called everytime a patch is generated anywhere.
 * @returns A disposer to stop listening to patches.
 */

function onGlobalPatches(listener) {
  assertIsFunction(listener, "listener");

  if (!(0,mobx_esm.isAction)(listener)) {
    listener = (0,mobx_esm.action)(listener.name || "onGlobalPatchesListener", listener);
  }

  globalPatchListeners.push(listener);
  return function () {
    deleteFromArray(globalPatchListeners, listener);
  };
}

function emitPatch(obj, patches, inversePatches, emitGlobally) {
  if (patches.length <= 0 && inversePatches.length <= 0) {
    return;
  } // first emit global listeners


  if (emitGlobally) {
    for (var i = 0; i < globalPatchListeners.length; i++) {
      var listener = globalPatchListeners[i];
      listener(obj, patches, inversePatches);
    }
  } // then per subtree listeners


  var listenersForObject = patchListeners.get(obj);

  if (listenersForObject) {
    for (var _i = 0; _i < listenersForObject.length; _i++) {
      var _listener = listenersForObject[_i];

      _listener(patches, inversePatches);
    }
  } // and also emit subtree listeners all the way to the root


  var parentPath = fastGetParentPath(obj);

  if (parentPath) {
    // tweak patches so they include the child path
    var childPath = parentPath.path;
    var newPatches = patches.map(function (p) {
      return addPathToPatch(p, childPath);
    });
    var newInversePatches = inversePatches.map(function (p) {
      return addPathToPatch(p, childPath);
    }); // false to avoid emitting global patches again for the same change

    emitPatch(parentPath.parent, newPatches, newInversePatches, false);
  }
}

function addPathToPatch(patch, path) {
  return _extends({}, patch, {
    path: [path].concat(patch.path)
  });
}

/**
 * Should freeze and plain json checks be done when creating the frozen object?
 */

var FrozenCheckMode;

(function (FrozenCheckMode) {
  /** Only when in dev mode */
  FrozenCheckMode["DevModeOnly"] = "devModeOnly";
  /** Always */

  FrozenCheckMode["On"] = "on";
  /** Never */

  FrozenCheckMode["Off"] = "off";
})(FrozenCheckMode || (FrozenCheckMode = {}));
/**
 * @ignore
 */


var frozenKey = "$frozen";
/**
 * A class that contains frozen data.
 * Use `frozen` to create an instance of this class.
 *
 * @typeparam T Data type.
 */

var Frozen =
/**
 * Frozen data, deeply immutable.
 */

/**
 * Creates an instance of Frozen.
 * Do not use directly, use `frozen` instead.
 *
 * @param dataToFreeze
 * @param checkMode
 */
function Frozen(dataToFreeze, checkMode) {
  if (checkMode === void 0) {
    checkMode = FrozenCheckMode.DevModeOnly;
  }

  this.data = void 0;
  var check = checkMode === FrozenCheckMode.On || checkMode === FrozenCheckMode.DevModeOnly && inDevMode();

  if (check) {
    checkDataIsSerializableAndFreeze(dataToFreeze);
  }

  this.data = dataToFreeze;

  if (check) {
    Object.freeze(this.data);
  }

  tweak(this, undefined);
};
/**
 * Marks some data as frozen. Frozen data becomes immutable (at least in dev mode), and is not enhanced
 * with capabilities such as getting the parent of the objects (except for the root object), it is not
 * made deeply observable (though the root object is observable by reference), etc.
 * On the other hand, this means it will be much faster to create/access. Use this for big data pieces
 * that are unlikely to change unless all of them change (for example lists of points for a polygon, etc).
 *
 * Note that data passed to frozen must be serializable to JSON, this is:
 * - primitive, plain object, or array
 * - without cycles
 *
 * @param data
 * @param checkMode
 */

function frozen(data, checkMode) {
  if (checkMode === void 0) {
    checkMode = FrozenCheckMode.DevModeOnly;
  }

  return new Frozen(data, checkMode);
}

function checkDataIsSerializableAndFreeze(data) {
  // TODO: detect cycles and throw if present?
  // primitives are ok
  if (isPrimitive(data)) {
    return;
  }

  if (Array.isArray(data)) {
    var arrLen = data.length;

    for (var i = 0; i < arrLen; i++) {
      var v = data[i];

      if (v === undefined && !getGlobalConfig().allowUndefinedArrayElements) {
        throw failure("undefined is not supported inside arrays since it is not serializable in JSON, consider using null instead");
      }

      checkDataIsSerializableAndFreeze(v);
    }

    Object.freeze(data);
    return;
  }

  if (isPlainObject(data)) {
    var dataKeys = Object.keys(data);
    var dataKeysLen = dataKeys.length;

    for (var _i = 0; _i < dataKeysLen; _i++) {
      var k = dataKeys[_i];
      var _v = data[k];
      checkDataIsSerializableAndFreeze(k);
      checkDataIsSerializableAndFreeze(_v);
    }

    Object.freeze(data);
    return;
  }

  throw failure("frozen data must be plainly serializable to JSON, but " + data + " is not");
}
/**
 * @ignore
 * @internal
 *
 * Checks if an snapshot is an snapshot for a frozen data.
 *
 * @param snapshot
 * @returns
 */


function isFrozenSnapshot(snapshot) {
  return isPlainObject(snapshot) && !!snapshot[frozenKey];
}

/**
 * Iterates through all the parents (from the nearest until the root)
 * until one of them matches the given predicate.
 * If the predicate is matched it will return the found node.
 * If none is found it will return undefined.
 *
 * @typeparam T Parent object type.
 * @param child Target object.
 * @param predicate Function that will be run for every parent of the target object, from immediate parent to the root.
 * @param maxDepth Max depth, or 0 for infinite.
 * @returns
 */

function findParent(child, predicate, maxDepth) {
  if (maxDepth === void 0) {
    maxDepth = 0;
  }

  var foundParentPath = findParentPath(child, predicate, maxDepth);
  return foundParentPath ? foundParentPath.parent : undefined;
}
/**
 * Iterates through all the parents (from the nearest until the root)
 * until one of them matches the given predicate.
 * If the predicate is matched it will return the found node plus the
 * path to get from the parent to the child.
 * If none is found it will return undefined.
 *
 * @typeparam T Parent object type.
 * @param child Target object.
 * @param predicate Function that will be run for every parent of the target object, from immediate parent to the root.
 * @param maxDepth Max depth, or 0 for infinite.
 * @returns
 */

function findParentPath(child, predicate, maxDepth) {
  if (maxDepth === void 0) {
    maxDepth = 0;
  }

  assertTweakedObject(child, "child");
  var path = [];
  var current = child;
  var depth = 0;
  var parentPath;

  while (parentPath = fastGetParentPath(current)) {
    path.unshift(parentPath.path);
    current = parentPath.parent;

    if (predicate(current)) {
      return {
        parent: current,
        path: path
      };
    }

    depth++;

    if (maxDepth > 0 && depth === maxDepth) {
      break;
    }
  }

  return undefined;
}

/**
 * A built-in action.
 */
var BuiltInAction;

(function (BuiltInAction) {
  /**
   * applyPatches
   */
  BuiltInAction["ApplyPatches"] = "$$applyPatches";
  /**
   * applySnapshot
   */

  BuiltInAction["ApplySnapshot"] = "$$applySnapshot";
  /**
   * detach
   */

  BuiltInAction["Detach"] = "$$detach";
  /**
   * applySet
   */

  BuiltInAction["ApplySet"] = "$$applySet";
  /**
   * applyDelete
   */

  BuiltInAction["ApplyDelete"] = "$$applyDelete";
  /**
   * applyMethodCall
   */

  BuiltInAction["ApplyMethodCall"] = "$$applyMethodCall";
})(BuiltInAction || (BuiltInAction = {}));

var builtInActionValues = /*#__PURE__*/new Set( /*#__PURE__*/Object.values(BuiltInAction));
/**
 * Returns if a given action name is a built-in action, this is, one of:
 * - applyPatches()
 * - applySnapshot()
 * - detach()
 *
 * @param actionName Action name to check.
 * @returns true if it is a built-in action, false otherwise.
 */

function isBuiltInAction(actionName) {
  return builtInActionValues.has(actionName);
}

/**
 * Detaches a given object from a tree.
 * If the parent is an object / model, detaching will delete the property.
 * If the parent is an array detaching will remove the node by splicing it.
 * If there's no parent it will throw.
 *
 * @param node Object to be detached.
 */

function detach(node) {
  assertTweakedObject(node, "node");
  wrappedInternalDetach().call(node);
}
var wrappedInternalDetach = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.Detach,
    fn: internalDetach,
    actionType: ActionContextActionType.Sync
  });
});

function internalDetach() {
  var node = this;
  var parentPath = fastGetParentPathIncludingDataObjects(node);
  if (!parentPath) return;
  var parent = parentPath.parent,
      path = parentPath.path;

  if ((0,mobx_esm.isObservableArray)(parent)) {
    parent.splice(+path, 1);
  } else if ((0,mobx_esm.isObservableObject)(parent)) {
    (0,mobx_esm.remove)(parent, "" + path);
  } else {
    throw failure("parent must be an observable object or an observable array");
  }
}

/**
 * Returns all the children objects (this is, excluding primitives) of an object.
 *
 * @param node Object to get the list of children from.
 * @param [options] An optional object with the `deep` option (defaults to false) to true to get
 * the children deeply or false to get them shallowly.
 * @returns A readonly observable set with the children.
 */

function getChildrenObjects(node, options) {
  assertTweakedObject(node, "node");

  if (!options || !options.deep) {
    return getObjectChildren(node);
  } else {
    return getDeepObjectChildren(node).deep;
  }
}

/**
 * Runs a callback everytime a new object is attached to a given node.
 * The callback can optionally return a disposer which will be run when the child is detached.
 *
 * The optional options parameter accepts and object with the following options:
 * - `deep: boolean` (default: `false`) - true if the callback should be run for all children deeply
 * or false if it it should only run for shallow children.
 * - `fireForCurrentChildren: boolean` (default: `true`) - true if the callback should be immediately
 * called for currently attached children, false if only for future attachments.
 *
 * Returns a disposer, which has a boolean parameter which should be true if pending detachment
 * callbacks should be run or false otherwise.
 *
 * @param target Function that returns the object whose children should be tracked.
 * @param fn Callback called when a child is attached to the target object.
 * @param [options]
 * @returns
 */

function onChildAttachedTo(target, fn, options) {
  assertIsFunction(target, "target");
  assertIsFunction(fn, "fn");

  var opts = _extends({
    deep: false,
    runForCurrentChildren: true
  }, options);

  var detachDisposers = new WeakMap();

  var runDetachDisposer = function runDetachDisposer(n) {
    var detachDisposer = detachDisposers.get(n);

    if (detachDisposer) {
      detachDisposers["delete"](n);
      detachDisposer();
    }
  };

  var addDetachDisposer = function addDetachDisposer(n, disposer) {
    if (disposer) {
      detachDisposers.set(n, disposer);
    }
  };

  var getChildrenObjectOpts = {
    deep: opts.deep
  };

  var getCurrentChildren = function getCurrentChildren() {
    var t = target();
    assertTweakedObject(t, "target()");
    var children = getChildrenObjects(t, getChildrenObjectOpts);
    var set = new Set();
    var iter = children.values();
    var cur = iter.next();

    while (!cur.done) {
      set.add(cur.value);
      cur = iter.next();
    }

    return set;
  };

  var currentChildren = opts.runForCurrentChildren ? new Set() : getCurrentChildren();
  var disposer = (0,mobx_esm.reaction)(function () {
    return getCurrentChildren();
  }, function (newChildren) {
    var disposersToRun = []; // find dead

    var currentChildrenIter = currentChildren.values();
    var currentChildrenCur = currentChildrenIter.next();

    while (!currentChildrenCur.done) {
      var n = currentChildrenCur.value;

      if (!newChildren.has(n)) {
        currentChildren["delete"](n); // we should run it in inverse order

        disposersToRun.push(n);
      }

      currentChildrenCur = currentChildrenIter.next();
    }

    if (disposersToRun.length > 0) {
      for (var i = disposersToRun.length - 1; i >= 0; i--) {
        runDetachDisposer(disposersToRun[i]);
      }
    } // find new


    var newChildrenIter = newChildren.values();
    var newChildrenCur = newChildrenIter.next();

    while (!newChildrenCur.done) {
      var _n = newChildrenCur.value;

      if (!currentChildren.has(_n)) {
        currentChildren.add(_n);
        addDetachDisposer(_n, fn(_n));
      }

      newChildrenCur = newChildrenIter.next();
    }
  }, {
    fireImmediately: true
  });
  return function (runDetachDisposers) {
    disposer();

    if (runDetachDisposers) {
      var currentChildrenIter = currentChildren.values();
      var currentChildrenCur = currentChildrenIter.next();

      while (!currentChildrenCur.done) {
        var n = currentChildrenCur.value;
        runDetachDisposer(n);
        currentChildrenCur = currentChildrenIter.next();
      }
    }

    currentChildren.clear();
  };
}

/**
 * Deserializers a data structure from its snapshot form.
 *
 * @typeparam T Object type.
 * @param snapshot Snapshot, even if a primitive.
 * @param [options] Options.
 * @returns The deserialized object.
 */

var fromSnapshot = function fromSnapshot(snapshot, options) {
  var opts = _extends({
    generateNewIds: false,
    overrideRootModelId: undefined
  }, options);

  var ctx = {
    options: opts
  };
  ctx.snapshotToInitialData = snapshotToInitialData.bind(undefined, ctx);
  return internalFromSnapshot(snapshot, ctx);
};
fromSnapshot = /*#__PURE__*/(0,mobx_esm.action)("fromSnapshot", fromSnapshot);

function internalFromSnapshot(sn, ctx) {
  if (isPrimitive(sn)) {
    return sn;
  }

  if (isMap(sn)) {
    throw failure("a snapshot must not contain maps");
  }

  if (isSet(sn)) {
    throw failure("a snapshot must not contain sets");
  }

  if (isArray(sn)) {
    return fromArraySnapshot(sn, ctx);
  }

  if (isFrozenSnapshot(sn)) {
    return frozen(sn.data);
  }

  if (isModelSnapshot(sn)) {
    return fromModelSnapshot(sn, ctx);
  }

  if (isPlainObject(sn)) {
    return fromPlainObjectSnapshot(sn, ctx);
  }

  throw failure("unsupported snapshot - " + sn);
}

function fromArraySnapshot(sn, ctx) {
  var arr = mobx_esm.observable.array([], observableOptions);
  var ln = sn.length;

  for (var i = 0; i < ln; i++) {
    arr.push(internalFromSnapshot(sn[i], ctx));
  }

  return tweakArray(arr, undefined, true);
}

function fromModelSnapshot(sn, ctx) {
  var type = sn[modelTypeKey];

  if (!type) {
    throw failure("a model snapshot must contain a type key (" + modelTypeKey + "), but none was found");
  }

  var modelInfo = getModelInfoForName(type);

  if (!modelInfo) {
    throw failure("model with name \"" + type + "\" not found in the registry");
  }

  if (!sn[modelIdKey]) {
    throw failure("a model snapshot must contain an id key (" + modelIdKey + "), but none was found");
  }

  return new modelInfo["class"](undefined, {
    snapshotInitialData: {
      unprocessedSnapshot: sn,
      snapshotToInitialData: ctx.snapshotToInitialData
    },
    generateNewIds: ctx.options.generateNewIds
  });
}

function snapshotToInitialData(ctx, processedSn) {
  var initialData = mobx_esm.observable.object({}, undefined, observableOptions);
  var processedSnKeys = Object.keys(processedSn);
  var processedSnKeysLen = processedSnKeys.length;

  for (var i = 0; i < processedSnKeysLen; i++) {
    var k = processedSnKeys[i];

    if (!isReservedModelKey(k)) {
      var v = processedSn[k];
      (0,mobx_esm.set)(initialData, k, internalFromSnapshot(v, ctx));
    }
  }

  return initialData;
}

function fromPlainObjectSnapshot(sn, ctx) {
  var plainObj = mobx_esm.observable.object({}, undefined, observableOptions);
  var snKeys = Object.keys(sn);
  var snKeysLen = snKeys.length;

  for (var i = 0; i < snKeysLen; i++) {
    var k = snKeys[i];
    var v = sn[k];
    (0,mobx_esm.set)(plainObj, k, internalFromSnapshot(v, ctx));
  }

  return tweakPlainObject(plainObj, undefined, undefined, true, false);
}

var observableOptions = {
  deep: false
};

/**
 * @ignore
 */

function reconcileSnapshot(value, sn, modelPool) {
  if (isPrimitive(sn)) {
    return sn;
  }

  if (isArray(sn)) {
    return reconcileArraySnapshot(value, sn, modelPool);
  }

  if (isFrozenSnapshot(sn)) {
    return reconcileFrozenSnapshot(value, sn);
  }

  if (isModelSnapshot(sn)) {
    return reconcileModelSnapshot(value, sn, modelPool);
  }

  if (isPlainObject(sn)) {
    return reconcilePlainObjectSnapshot(value, sn, modelPool);
  }

  if (isMap(sn)) {
    throw failure("a snapshot must not contain maps");
  }

  if (isSet(sn)) {
    throw failure("a snapshot must not contain sets");
  }

  throw failure("unsupported snapshot - " + sn);
}

function reconcileArraySnapshot(value, sn, modelPool) {
  if (!isArray(value)) {
    // no reconciliation possible
    return fromSnapshot(sn);
  } // remove excess items


  if (value.length > sn.length) {
    value.splice(sn.length, value.length - sn.length);
  } // reconcile present items


  for (var i = 0; i < value.length; i++) {
    var oldValue = value[i];
    var newValue = reconcileSnapshot(oldValue, sn[i], modelPool);
    detachIfNeeded(newValue, oldValue, modelPool);
    (0,mobx_esm.set)(value, i, newValue);
  } // add excess items


  for (var _i = value.length; _i < sn.length; _i++) {
    value.push(reconcileSnapshot(undefined, sn[_i], modelPool));
  }

  return value;
}

function reconcileFrozenSnapshot(value, sn) {
  // reconciliation is only possible if the target is a Frozen instance with the same data (by ref)
  // in theory we could compare the JSON representation of both datas or do a deep comparison, but that'd be too slow
  if (value instanceof Frozen && value.data === sn.data) {
    return value;
  }

  return frozen(sn.data);
}

function reconcileModelSnapshot(value, sn, modelPool) {
  var type = sn[modelTypeKey];
  var modelInfo = getModelInfoForName(type);

  if (!modelInfo) {
    throw failure("model with name \"" + type + "\" not found in the registry");
  }

  var id = sn[modelIdKey]; // try to use model from pool if possible

  var modelInPool = modelPool.findModelForSnapshot(sn);

  if (modelInPool) {
    value = modelInPool;
  }

  if (!(value instanceof modelInfo["class"]) || value[modelTypeKey] !== type || value[modelIdKey] !== id) {
    // different kind of model / model instance, no reconciliation possible
    return fromSnapshot(sn);
  }

  var modelObj = value;
  var processedSn = sn;

  if (modelObj.fromSnapshot) {
    processedSn = modelObj.fromSnapshot(sn);
  }

  var data = modelObj.$; // remove excess props

  var dataKeys = Object.keys(data);
  var dataKeysLen = dataKeys.length;

  for (var i = 0; i < dataKeysLen; i++) {
    var k = dataKeys[i];

    if (!(k in processedSn)) {
      (0,mobx_esm.remove)(data, k);
    }
  } // reconcile the rest


  var processedSnKeys = Object.keys(processedSn);
  var processedSnKeysLen = processedSnKeys.length;

  for (var _i2 = 0; _i2 < processedSnKeysLen; _i2++) {
    var _k = processedSnKeys[_i2];

    if (!isReservedModelKey(_k)) {
      var v = processedSn[_k];
      var oldValue = data[_k];
      var newValue = reconcileSnapshot(oldValue, v, modelPool);
      detachIfNeeded(newValue, oldValue, modelPool);
      (0,mobx_esm.set)(data, _k, newValue);
    }
  }

  return modelObj;
}

function reconcilePlainObjectSnapshot(value, sn, modelPool) {
  // plain obj
  if (!isPlainObject(value) && !(0,mobx_esm.isObservableObject)(value)) {
    // no reconciliation possible
    return fromSnapshot(sn);
  }

  var plainObj = value; // remove excess props

  var plainObjKeys = Object.keys(plainObj);
  var plainObjKeysLen = plainObjKeys.length;

  for (var i = 0; i < plainObjKeysLen; i++) {
    var k = plainObjKeys[i];

    if (!(k in sn)) {
      (0,mobx_esm.remove)(plainObj, k);
    }
  } // reconcile the rest


  var snKeys = Object.keys(sn);
  var snKeysLen = snKeys.length;

  for (var _i3 = 0; _i3 < snKeysLen; _i3++) {
    var _k2 = snKeys[_i3];
    var v = sn[_k2];
    var oldValue = plainObj[_k2];
    var newValue = reconcileSnapshot(oldValue, v, modelPool);
    detachIfNeeded(newValue, oldValue, modelPool);
    (0,mobx_esm.set)(plainObj, _k2, newValue);
  }

  return plainObj;
}

function detachIfNeeded(newValue, oldValue, modelPool) {
  // edge case for when we are swapping models around the tree
  if (newValue === oldValue) {
    // already where it should be
    return;
  }

  if (isModel(newValue) && modelPool.findModelByTypeAndId(newValue[modelTypeKey], newValue[modelIdKey])) {
    var parentPath = fastGetParentPathIncludingDataObjects(newValue);

    if (parentPath) {
      (0,mobx_esm.set)(parentPath.parent, parentPath.path, null);
    }
  }
}

var ModelPool = /*#__PURE__*/function () {
  function ModelPool(root) {
    var _dataObjectParent$get;

    this.pool = void 0;
    // make sure we don't use the sub-data $ object
    root = (_dataObjectParent$get = dataObjectParent.get(root)) != null ? _dataObjectParent$get : root;
    this.pool = getDeepObjectChildren(root).deepByModelTypeAndId;
  }

  var _proto = ModelPool.prototype;

  _proto.findModelByTypeAndId = function findModelByTypeAndId(modelType, modelId) {
    return this.pool.get(byModelTypeAndIdKey(modelType, modelId));
  };

  _proto.findModelForSnapshot = function findModelForSnapshot(sn) {
    if (!isModelSnapshot(sn)) {
      return undefined;
    }

    return this.findModelByTypeAndId(sn[modelTypeKey], sn[modelIdKey]);
  };

  return ModelPool;
}();

/**
 * Applies the given patches to the given target object.
 *
 * @param node Target object.
 * @param patches List of patches to apply.
 * @param reverse Whether patches are applied in reverse order.
 */

function applyPatches(node, patches, reverse) {
  if (reverse === void 0) {
    reverse = false;
  }

  assertTweakedObject(node, "node");

  if (patches.length <= 0) {
    return;
  }

  wrappedInternalApplyPatches().call(node, patches, reverse);
}
/**
 * @ignore
 * @internal
 */

function internalApplyPatches(patches, reverse) {
  if (reverse === void 0) {
    reverse = false;
  }

  var obj = this;
  var modelPool = new ModelPool(obj);

  if (reverse) {
    var i = patches.length;

    while (i--) {
      var p = patches[i];

      if (!isArray(p)) {
        applySinglePatch(obj, p, modelPool);
      } else {
        var j = p.length;

        while (j--) {
          applySinglePatch(obj, p[j], modelPool);
        }
      }
    }
  } else {
    var len = patches.length;

    for (var _i = 0; _i < len; _i++) {
      var _p = patches[_i];

      if (!isArray(_p)) {
        applySinglePatch(obj, _p, modelPool);
      } else {
        var len2 = _p.length;

        for (var _j = 0; _j < len2; _j++) {
          applySinglePatch(obj, _p[_j], modelPool);
        }
      }
    }
  }
}
var wrappedInternalApplyPatches = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.ApplyPatches,
    fn: internalApplyPatches,
    actionType: ActionContextActionType.Sync
  });
});

function applySinglePatch(obj, patch, modelPool) {
  var _pathArrayToObjectAnd = pathArrayToObjectAndProp(obj, patch.path),
      target = _pathArrayToObjectAnd.target,
      prop = _pathArrayToObjectAnd.prop;

  if (isArray(target)) {
    switch (patch.op) {
      case "add":
        {
          var index = +prop; // reconcile from the pool if possible

          var newValue = reconcileSnapshot(undefined, patch.value, modelPool);
          target.splice(index, 0, newValue);
          break;
        }

      case "remove":
        {
          var _index = +prop; // no reconciliation, removing


          target.splice(_index, 1);
          break;
        }

      case "replace":
        {
          if (prop === "length") {
            target.length = patch.value;
          } else {
            var _index2 = +prop; // try to reconcile


            var _newValue = reconcileSnapshot(target[_index2], patch.value, modelPool);

            (0,mobx_esm.set)(target, _index2, _newValue);
          }

          break;
        }

      default:
        throw failure("unsupported patch operation: " + patch.op);
    }
  } else {
    switch (patch.op) {
      case "add":
        {
          // reconcile from the pool if possible
          var _newValue2 = reconcileSnapshot(undefined, patch.value, modelPool);

          (0,mobx_esm.set)(target, prop, _newValue2);
          break;
        }

      case "remove":
        {
          // no reconciliation, removing
          (0,mobx_esm.remove)(target, prop);
          break;
        }

      case "replace":
        {
          // try to reconcile
          // we don't need to tweak the pool since reconcileSnapshot will do that for us
          var _newValue3 = reconcileSnapshot(target[prop], patch.value, modelPool);

          (0,mobx_esm.set)(target, prop, _newValue3);
          break;
        }

      default:
        throw failure("unsupported patch operation: " + patch.op);
    }
  }
}

function pathArrayToObjectAndProp(obj, path) {
  if (inDevMode()) {
    if (!isArray(path)) {
      throw failure("invalid path: " + path);
    }
  }

  var target = modelToDataNode(obj);

  if (path.length === 0) {
    return {
      target: target
    };
  }

  for (var i = 0; i <= path.length - 2; i++) {
    target = modelToDataNode(target[path[i]]);
  }

  return {
    target: target,
    prop: path[path.length - 1]
  };
}

/**
 * @ignore
 * @internal
 */

function runTypeCheckingAfterChange(obj, patchRecorder) {
  // invalidate type check cached result
  invalidateCachedTypeCheckerResult(obj);

  if (isModelAutoTypeCheckingEnabled()) {
    var parentModelWithTypeChecker = findNearestParentModelWithTypeChecker(obj);

    if (parentModelWithTypeChecker) {
      var err = parentModelWithTypeChecker.typeCheck();

      if (err) {
        // quietly apply inverse patches (do not generate patches, snapshots, actions, etc)
        runWithoutSnapshotOrPatches(function () {
          internalApplyPatches.call(obj, patchRecorder.invPatches, true);
        }); // at the end of apply patches it will be type checked again and its result cached once more

        err["throw"](parentModelWithTypeChecker);
      }
    }
  }
}
/**
 * @ignore
 *
 * Finds the closest parent model that has a type checker defined.
 *
 * @param child
 * @returns
 */

function findNearestParentModelWithTypeChecker(child) {
  // child might be .$, so we need to check the parent model in that case
  var actualChild = dataToModelNode(child);

  if (child !== actualChild) {
    child = actualChild;

    if (isModel(child) && !!getModelDataType(child)) {
      return child;
    }
  }

  return findParent(child, function (parent) {
    return isModel(parent) && !!getModelDataType(parent);
  });
}

/**
 * @ignore
 */

function tweakArray(value, parentPath, doNotTweakChildren) {
  var originalArr = value;
  var arrLn = originalArr.length;
  var tweakedArr = (0,mobx_esm.isObservableArray)(originalArr) ? originalArr : mobx_esm.observable.array([], observableOptions$1);

  if (tweakedArr !== originalArr) {
    tweakedArr.length = originalArr.length;
  }

  var interceptDisposer;
  var observeDisposer;

  var untweak = function untweak() {
    interceptDisposer();
    observeDisposer();
  };

  tweakedObjects.set(tweakedArr, untweak);
  setParent(tweakedArr, parentPath, false, false);
  var standardSn = [];
  standardSn.length = arrLn; // substitute initial values by proxied values

  for (var i = 0; i < arrLn; i++) {
    var v = originalArr[i];

    if (isPrimitive(v)) {
      if (!doNotTweakChildren) {
        (0,mobx_esm.set)(tweakedArr, i, v);
      }

      standardSn[i] = v;
    } else {
      var path = {
        parent: tweakedArr,
        path: i
      };
      var tweakedValue = void 0;

      if (doNotTweakChildren) {
        tweakedValue = v;
        setParent(tweakedValue, path, false, false);
      } else {
        tweakedValue = tweak(v, path);
        (0,mobx_esm.set)(tweakedArr, i, tweakedValue);
      }

      var valueSn = getInternalSnapshot(tweakedValue);
      standardSn[i] = valueSn.standard;
    }
  }

  setInternalSnapshot(tweakedArr, standardSn);
  interceptDisposer = (0,mobx_esm.intercept)(tweakedArr, interceptArrayMutation.bind(undefined, tweakedArr));
  observeDisposer = (0,mobx_esm.observe)(tweakedArr, arrayDidChange);
  return tweakedArr;
}

function arrayDidChange(change
/*IArrayDidChange*/
) {
  var arr = change.object;

  var _getInternalSnapshot = getInternalSnapshot(arr),
      oldSnapshot = _getInternalSnapshot.standard;

  var patchRecorder = new InternalPatchRecorder();
  var newSnapshot = oldSnapshot.slice();

  switch (change.type) {
    case "splice":
      {
        var index = change.index;
        var addedCount = change.addedCount;
        var removedCount = change.removedCount;
        var addedItems = [];
        addedItems.length = addedCount;

        for (var i = 0; i < addedCount; i++) {
          var v = change.added[i];

          if (isPrimitive(v)) {
            addedItems[i] = v;
          } else {
            addedItems[i] = getInternalSnapshot(v).standard;
          }
        }

        var oldLen = oldSnapshot.length;
        newSnapshot.splice.apply(newSnapshot, [index, removedCount].concat(addedItems));
        var patches = [];
        var invPatches = []; // optimization: if we add as many as we remove then replace instead

        if (addedCount === removedCount) {
          for (var _i = 0; _i < addedCount; _i++) {
            var realIndex = index + _i;
            var newVal = newSnapshot[realIndex];
            var oldVal = oldSnapshot[realIndex];

            if (newVal !== oldVal) {
              var path = [realIndex]; // replace 0, 1, 2...

              patches.push({
                op: "replace",
                path: path,
                value: newVal
              }); // replace ...2, 1, 0 since inverse patches are applied in reverse

              invPatches.push({
                op: "replace",
                path: path,
                value: oldVal
              });
            }
          }
        } else {
          var interimLen = oldLen - removedCount; // first remove items

          if (removedCount > 0) {
            // optimization, when removing from the end set the length instead
            var removeUsingSetLength = index >= interimLen;

            if (removeUsingSetLength) {
              patches.push({
                op: "replace",
                path: ["length"],
                value: interimLen
              });
            }

            for (var _i2 = removedCount - 1; _i2 >= 0; _i2--) {
              var _realIndex = index + _i2;

              var _path = [_realIndex];

              if (!removeUsingSetLength) {
                // remove ...2, 1, 0
                patches.push({
                  op: "remove",
                  path: _path
                });
              } // add 0, 1, 2... since inverse patches are applied in reverse


              invPatches.push({
                op: "add",
                path: _path,
                value: oldSnapshot[_realIndex]
              });
            }
          } // then add items


          if (addedCount > 0) {
            // optimization, for inverse patches, when adding from the end set the length to restore instead
            var restoreUsingSetLength = index >= interimLen;

            if (restoreUsingSetLength) {
              invPatches.push({
                op: "replace",
                path: ["length"],
                value: interimLen
              });
            }

            for (var _i3 = 0; _i3 < addedCount; _i3++) {
              var _realIndex2 = index + _i3;

              var _path2 = [_realIndex2]; // add 0, 1, 2...

              patches.push({
                op: "add",
                path: _path2,
                value: newSnapshot[_realIndex2]
              }); // remove ...2, 1, 0 since inverse patches are applied in reverse

              if (!restoreUsingSetLength) {
                invPatches.push({
                  op: "remove",
                  path: _path2
                });
              }
            }
          }
        }

        patchRecorder.record(patches, invPatches);
      }
      break;

    case "update":
      {
        var k = change.index;
        var val = change.newValue;
        var _oldVal = newSnapshot[k];

        if (isPrimitive(val)) {
          newSnapshot[k] = val;
        } else {
          var valueSn = getInternalSnapshot(val);
          newSnapshot[k] = valueSn.standard;
        }

        var _path3 = [k];
        patchRecorder.record([{
          op: "replace",
          path: _path3,
          value: newSnapshot[k]
        }], [{
          op: "replace",
          path: _path3,
          value: _oldVal
        }]);
      }
      break;
  }

  runTypeCheckingAfterChange(arr, patchRecorder);

  if (!runningWithoutSnapshotOrPatches) {
    setInternalSnapshot(arr, newSnapshot);
    patchRecorder.emit(arr);
  }
}

var undefinedInsideArrayErrorMsg = "undefined is not supported inside arrays since it is not serializable in JSON, consider using null instead"; // TODO: remove array parameter and just use change.object once mobx update event is fixed

function interceptArrayMutation(array, change) {
  assertCanWrite();

  switch (change.type) {
    case "splice":
      {
        if (inDevMode() && !getGlobalConfig().allowUndefinedArrayElements) {
          var len = change.added.length;

          for (var i = 0; i < len; i++) {
            var v = change.added[i];

            if (v === undefined) {
              throw failure(undefinedInsideArrayErrorMsg);
            }
          }
        }

        for (var _i4 = 0; _i4 < change.removedCount; _i4++) {
          var removedValue = change.object[change.index + _i4];
          tweak(removedValue, undefined);
          tryUntweak(removedValue);
        }

        for (var _i5 = 0; _i5 < change.added.length; _i5++) {
          change.added[_i5] = tweak(change.added[_i5], {
            parent: change.object,
            path: change.index + _i5
          });
        } // we might also need to update the parent of the next indexes


        var oldNextIndex = change.index + change.removedCount;
        var newNextIndex = change.index + change.added.length;

        if (oldNextIndex !== newNextIndex) {
          for (var _i6 = oldNextIndex, j = newNextIndex; _i6 < change.object.length; _i6++, j++) {
            setParent(change.object[_i6], {
              parent: change.object,
              path: j
            }, true, false);
          }
        }
      }
      break;

    case "update":
      if (inDevMode() && !getGlobalConfig().allowUndefinedArrayElements && change.newValue === undefined) {
        throw failure(undefinedInsideArrayErrorMsg);
      } // TODO: should be change.object, but mobx is bugged and doesn't send the proxy


      var oldVal = array[change.index];
      tweak(oldVal, undefined); // set old prop obj parent to undefined

      tryUntweak(oldVal);
      change.newValue = tweak(change.newValue, {
        parent: array,
        path: change.index
      });
      break;
  }

  return change;
}

var observableOptions$1 = {
  deep: false
};

/**
 * @ingore
 */

function tweakFrozen(frozenObj, parentPath) {
  var _setInternalSnapshot;

  tweakedObjects.set(frozenObj, undefined);
  setParent(frozenObj, parentPath, false, false); // we DON'T want data proxified, but the snapshot is the data itself

  setInternalSnapshot(frozenObj, (_setInternalSnapshot = {}, _setInternalSnapshot[frozenKey] = true, _setInternalSnapshot.data = frozenObj.data, _setInternalSnapshot));
  return frozenObj;
}

/**
 * Turns an object (array, plain object) into a tree node,
 * which then can accept calls to `getParent`, `getSnapshot`, etc.
 * If a tree node is passed it will return the passed argument directly.
 *
 * @param value Object to turn into a tree node.
 * @returns The object as a tree node.
 */

function toTreeNode(value) {
  if (!isObject(value)) {
    throw failure("only objects can be turned into tree nodes");
  }

  if (!isTweakedObject(value, true)) {
    return tweak(value, undefined);
  }

  return value;
}
/**
 * @ignore
 */

function internalTweak(value, parentPath) {
  if (isPrimitive(value)) {
    return value;
  }

  if (isTweakedObject(value, true)) {
    setParent(value, parentPath, false, false);
    return value;
  }

  if (isModel(value)) {
    return tweakModel(value, parentPath);
  }

  if (isArray(value)) {
    return tweakArray(value, parentPath, false);
  } // plain object


  if ((0,mobx_esm.isObservableObject)(value) || isPlainObject(value)) {
    return tweakPlainObject(value, parentPath, undefined, false, false);
  }

  if (value instanceof Frozen) {
    return tweakFrozen(value, parentPath);
  } // unsupported


  if (isMap(value)) {
    throw failure("maps are not directly supported. consider applying 'transformObjectAsMap' over a '{[k: string]: V}' property, or 'transformArrayAsMap' over a '[string, V][]' property instead.");
  } // unsupported


  if (isSet(value)) {
    throw failure("sets are not directly supported. consider applying 'transformArrayAsSet' over a 'V[]' property instead.");
  }

  throw failure("tweak can only work over models, observable objects/arrays, or primitives, but got " + value + " instead");
}
/**
 * @ignore
 * @internal
 */


var tweak = /*#__PURE__*/(0,mobx_esm.action)("tweak", internalTweak);
/**
 * @ignore
 * @internal
 */

function tryUntweak(value) {
  if (isPrimitive(value)) {
    return true;
  }

  if (inDevMode()) {
    if (fastGetParent(value)) {
      throw failure("assertion error: object cannot be untweaked while it has a parent");
    }
  }

  var untweaker = tweakedObjects.get(value);

  if (!untweaker) {
    return false;
  } // we have to make a copy since it will be changed
  // we have to iterate ourselves since it seems like babel does not do downlevel iteration


  var children = [];
  var childrenIter = getObjectChildren(value).values();
  var childrenCur = childrenIter.next();

  while (!childrenCur.done) {
    children.push(childrenCur.value);
    childrenCur = childrenIter.next();
  }

  for (var i = 0; i < children.length; i++) {
    var v = children[i];
    setParent(v, undefined, false, false);
  }

  untweaker();
  tweakedObjects["delete"](value);
  unsetInternalSnapshot(value);
  return true;
}

/**
 * @ignore
 */

function tweakPlainObject(value, parentPath, snapshotModelType, doNotTweakChildren, isDataObject) {
  var originalObj = value;
  var tweakedObj = (0,mobx_esm.isObservableObject)(originalObj) ? originalObj : mobx_esm.observable.object({}, undefined, observableOptions$2);
  var interceptDisposer;
  var observeDisposer;

  var untweak = function untweak() {
    interceptDisposer();
    observeDisposer();
  };

  tweakedObjects.set(tweakedObj, untweak);
  setParent(tweakedObj, parentPath, false, isDataObject);
  var standardSn = {}; // substitute initial values by tweaked values

  var originalObjKeys = Object.keys(originalObj);
  var originalObjKeysLen = originalObjKeys.length;

  for (var i = 0; i < originalObjKeysLen; i++) {
    var k = originalObjKeys[i];
    var v = originalObj[k];

    if (isPrimitive(v)) {
      if (!doNotTweakChildren) {
        (0,mobx_esm.set)(tweakedObj, k, v);
      }

      standardSn[k] = v;
    } else {
      var path = {
        parent: tweakedObj,
        path: k
      };
      var tweakedValue = void 0;

      if (doNotTweakChildren) {
        tweakedValue = v;
        setParent(tweakedValue, path, false, false);
      } else {
        tweakedValue = tweak(v, path);
        (0,mobx_esm.set)(tweakedObj, k, tweakedValue);
      }

      var valueSn = getInternalSnapshot(tweakedValue);
      standardSn[k] = valueSn.standard;
    }
  }

  if (snapshotModelType) {
    standardSn[modelTypeKey] = snapshotModelType;
  }

  setInternalSnapshot(isDataObject ? dataToModelNode(tweakedObj) : tweakedObj, standardSn);
  interceptDisposer = (0,mobx_esm.intercept)(tweakedObj, interceptObjectMutation);
  observeDisposer = (0,mobx_esm.observe)(tweakedObj, objectDidChange);
  return tweakedObj;
}
var observableOptions$2 = {
  deep: false
};

function objectDidChange(change) {
  var obj = change.object;
  var actualNode = dataToModelNode(obj);

  var _getInternalSnapshot = getInternalSnapshot(actualNode),
      standardSn = _getInternalSnapshot.standard;

  var patchRecorder = new InternalPatchRecorder();
  standardSn = Object.assign({}, standardSn);

  switch (change.type) {
    case "add":
    case "update":
      {
        var k = change.name;
        var val = change.newValue;
        var oldVal = standardSn[k];

        if (isPrimitive(val)) {
          standardSn[k] = val;
        } else {
          var valueSn = getInternalSnapshot(val);
          standardSn[k] = valueSn.standard;
        }

        var path = [k];

        if (change.type === "add") {
          patchRecorder.record([{
            op: "add",
            path: path,
            value: standardSn[k]
          }], [{
            op: "remove",
            path: path
          }]);
        } else {
          patchRecorder.record([{
            op: "replace",
            path: path,
            value: standardSn[k]
          }], [{
            op: "replace",
            path: path,
            value: oldVal
          }]);
        }
      }
      break;

    case "remove":
      {
        var _k = change.name;
        var _oldVal = standardSn[_k];
        delete standardSn[_k];
        var _path = [_k];
        patchRecorder.record([{
          op: "remove",
          path: _path
        }], [{
          op: "add",
          path: _path,
          value: _oldVal
        }]);
      }
      break;
  }

  runTypeCheckingAfterChange(obj, patchRecorder);

  if (!runningWithoutSnapshotOrPatches) {
    setInternalSnapshot(actualNode, standardSn);
    patchRecorder.emit(actualNode);
  }
}

function interceptObjectMutation(change) {
  assertCanWrite();

  if (typeof change.name === "symbol") {
    throw failure("symbol properties are not supported");
  }

  switch (change.type) {
    case "add":
      change.newValue = tweak(change.newValue, {
        parent: change.object,
        path: "" + change.name
      });
      break;

    case "remove":
      {
        var oldVal = change.object[change.name];
        tweak(oldVal, undefined);
        tryUntweak(oldVal);
        break;
      }

    case "update":
      {
        var _oldVal2 = change.object[change.name];
        tweak(_oldVal2, undefined);
        tryUntweak(_oldVal2);
        change.newValue = tweak(change.newValue, {
          parent: change.object,
          path: "" + change.name
        });
        break;
      }
  }

  return change;
}

/**
 * @internal
 * @ignore
 */
var modelInitializersSymbol = /*#__PURE__*/Symbol("modelInitializers");
/**
 * @internal
 * @ignore
 */

function addModelClassInitializer(modelClass, init) {
  var initializers = modelClass[modelInitializersSymbol];

  if (!initializers) {
    initializers = [];
    modelClass[modelInitializersSymbol] = initializers;
  }

  initializers.push(init);
}
/**
 * @internal
 * @ignore
 */

function getModelClassInitializers(modelClass) {
  return modelClass[modelInitializersSymbol];
}

/**
 * @ignore
 * @internal
 */

var internalNewModel = /*#__PURE__*/(0,mobx_esm.action)("newModel", function (origModelObj, initialData, options) {
  var _modelClass = options.modelClass,
      snapshotInitialData = options.snapshotInitialData,
      generateNewIds = options.generateNewIds;
  var modelClass = _modelClass;

  if (inDevMode()) {
    assertIsModelClass(modelClass, "modelClass");
  }

  var modelObj = origModelObj;
  var modelInfo = modelInfoByClass.get(modelClass);

  if (!modelInfo) {
    throw failure("no model info for class " + modelClass.name + " could be found - did you forget to add the @model decorator?");
  }

  var id;

  if (snapshotInitialData) {
    var sn = snapshotInitialData.unprocessedSnapshot;

    if (generateNewIds) {
      id = getGlobalConfig().modelIdGenerator();
    } else {
      id = sn[modelIdKey];
    }

    if (modelObj.fromSnapshot) {
      sn = modelObj.fromSnapshot(sn);
    }

    initialData = snapshotInitialData.snapshotToInitialData(sn);
  } else {
    // use symbol if provided
    if (initialData[modelIdKey]) {
      id = initialData[modelIdKey];
    } else {
      id = getGlobalConfig().modelIdGenerator();
    }
  }

  modelObj[modelTypeKey] = modelInfo.name; // fill in defaults in initial data

  var modelProps = getInternalModelClassPropsInfo(modelClass);
  var modelPropsKeys = Object.keys(modelProps);

  for (var i = 0; i < modelPropsKeys.length; i++) {
    var k = modelPropsKeys[i];
    var v = initialData[k];

    if (v === undefined || v === null) {
      var newValue = v;
      var propData = modelProps[k];

      if (propData.defaultFn !== noDefaultValue) {
        newValue = propData.defaultFn();
      } else if (propData.defaultValue !== noDefaultValue) {
        newValue = propData.defaultValue;
      }

      (0,mobx_esm.set)(initialData, k, newValue);
    }
  }

  (0,mobx_esm.set)(initialData, modelIdKey, id);
  tweakModel(modelObj, undefined); // create observable data object with initial data

  var obsData = tweakPlainObject(initialData, {
    parent: modelObj,
    path: "$"
  }, modelObj[modelTypeKey], false, true); // hide $.$modelId

  Object.defineProperty(obsData, modelIdKey, _extends({}, Object.getOwnPropertyDescriptor(obsData, modelIdKey), {
    enumerable: false
  })); // link it, and make it readonly

  modelObj.$ = obsData;

  if (inDevMode()) {
    makePropReadonly(modelObj, "$", true);
  } // type check it if needed


  if (isModelAutoTypeCheckingEnabled() && getModelDataType(modelClass)) {
    var err = modelObj.typeCheck();

    if (err) {
      err["throw"](modelObj);
    }
  } // run any extra initializers for the class as needed


  var initializers = getModelClassInitializers(modelClass);

  if (initializers) {
    var len = initializers.length;

    for (var _i = 0; _i < len; _i++) {
      var init = initializers[_i];
      init(modelObj);
    }
  }

  return modelObj;
});

/**
 * @ignore
 */

var propsDataTypeSymbol = /*#__PURE__*/Symbol();
/**
 * @ignore
 */

var propsCreationDataTypeSymbol = /*#__PURE__*/Symbol();
/**
 * @ignore
 */

var instanceDataTypeSymbol = /*#__PURE__*/Symbol();
/**
 * @ignore
 */

var instanceCreationDataTypeSymbol = /*#__PURE__*/Symbol();
/**
 * @ignore
 * @internal
 */

var modelInitializedSymbol = /*#__PURE__*/Symbol("modelInitialized");
/**
 * Base abstract class for models. Use `Model` instead when extending.
 *
 * Never override the constructor, use `onInit` or `onAttachedToRootStore` instead.
 *
 * @typeparam Data Data type.
 * @typeparam CreationData Creation data type.
 */

var BaseModel = /*#__PURE__*/function () {
  var _proto = BaseModel.prototype;

  // just to make typing work properly

  /**
   * Model type name.
   */

  /**
   * Model internal id. Can be modified inside a model action.
   */

  /**
   * Can be overriden to offer a reference id to be used in reference resolution.
   * By default it will use `$modelId`.
   */
  _proto.getRefId = function getRefId() {
    return this[modelIdKey];
  }
  /**
   * Data part of the model, which is observable and will be serialized in snapshots.
   * Use it if one of the data properties matches one of the model properties/functions.
   * This also allows access to the backed values of transformed properties.
   */
  ;

  /**
   * Performs a type check over the model instance.
   * For this to work a data type has to be declared in the model decorator.
   *
   * @returns A `TypeCheckError` or `null` if there is no error.
   */
  _proto.typeCheck = function typeCheck$1() {
    var type = typesModel(this.constructor);
    return typeCheck(type, this);
  }
  /**
   * Creates an instance of Model.
   */
  ;

  function BaseModel(data) {
    this[propsDataTypeSymbol] = void 0;
    this[propsCreationDataTypeSymbol] = void 0;
    this[instanceDataTypeSymbol] = void 0;
    this[instanceCreationDataTypeSymbol] = void 0;
    this[modelTypeKey] = void 0;
    this[modelIdKey] = void 0;
    this.$ = void 0;
    var initialData = data;
    var _arguments$ = arguments[1],
        snapshotInitialData = _arguments$.snapshotInitialData,
        modelClass = _arguments$.modelClass,
        propsWithTransforms = _arguments$.propsWithTransforms,
        generateNewIds = _arguments$.generateNewIds;
    Object.setPrototypeOf(this, modelClass.prototype);
    var self = this; // let's always use the one from the prototype

    delete self[modelIdKey]; // delete unnecessary props

    delete self[propsDataTypeSymbol];
    delete self[propsCreationDataTypeSymbol];
    delete self[instanceDataTypeSymbol];
    delete self[instanceCreationDataTypeSymbol];

    if (!snapshotInitialData) {
      // plain new
      assertIsObject(initialData, "initialData"); // apply transforms to initial data if needed

      var propsWithTransformsLen = propsWithTransforms.length;

      if (propsWithTransformsLen > 0) {
        initialData = Object.assign(initialData);

        for (var i = 0; i < propsWithTransformsLen; i++) {
          var propWithTransform = propsWithTransforms[i];
          var propName = propWithTransform[0];
          var propTransform = propWithTransform[1];
          var memoTransform = memoTransformCache.getOrCreateMemoTransform(this, propName, propTransform);
          initialData[propName] = memoTransform.dataToProp(initialData[propName]);
        }
      }

      internalNewModel(this, mobx_esm.observable.object(initialData, undefined, {
        deep: false
      }), {
        modelClass: modelClass,
        generateNewIds: true
      });
    } else {
      // from snapshot
      internalNewModel(this, undefined, {
        modelClass: modelClass,
        snapshotInitialData: snapshotInitialData,
        generateNewIds: generateNewIds
      });
    }
  }

  _proto.toString = function toString(options) {
    var finalOptions = _extends({
      withData: true
    }, options);

    var firstPart = this.constructor.name + "#" + this[modelTypeKey];
    return finalOptions.withData ? "[" + firstPart + " " + JSON.stringify(getSnapshot(this)) + "]" : "[" + firstPart + "]";
  };

  return BaseModel;
}(); // these props will never be hoisted to this (except for model id)

/**
 * @internal
 */

var baseModelPropNames = /*#__PURE__*/new Set([modelTypeKey, modelIdKey, "onInit", "$", "getRefId", "onAttachedToRootStore", "fromSnapshot", "typeCheck"]);
/**
 * @deprecated Should not be needed anymore.
 *
 * Tricks Typescript into accepting abstract classes as a parameter for `ExtendedModel`.
 * Does nothing in runtime.
 *
 * @typeparam T Abstract model class type.
 * @param type Abstract model class.
 * @returns
 */

function abstractModelClass(type) {
  return type;
}
/**
 * Tricks Typescript into accepting a particular kind of generic class as a parameter for `ExtendedModel`.
 * Does nothing in runtime.
 *
 * @typeparam T Generic model class type.
 * @param type Generic model class.
 * @returns
 */

function modelClass(type) {
  return type;
}
/**
 * Add missing model metadata to a model creation snapshot to generate a proper model snapshot.
 * Usually used alongside `fromSnapshot`.
 *
 * @typeparam M Model type.
 * @param modelClass Model class.
 * @param snapshot Model creation snapshot without metadata.
 * @param [internalId] Model internal ID, or `undefined` to generate a new one.
 * @returns The model snapshot (including metadata).
 */

function modelSnapshotInWithMetadata(modelClass, snapshot, internalId) {
  var _extends2;

  if (internalId === void 0) {
    internalId = getGlobalConfig().modelIdGenerator();
  }

  assertIsModelClass(modelClass, "modelClass");
  assertIsObject(snapshot, "initialData");
  var modelInfo = modelInfoByClass.get(modelClass);
  return _extends({}, snapshot, (_extends2 = {}, _extends2[modelTypeKey] = modelInfo.name, _extends2[modelIdKey] = internalId, _extends2));
}
/**
 * Add missing model metadata to a model output snapshot to generate a proper model snapshot.
 * Usually used alongside `applySnapshot`.
 *
 * @typeparam M Model type.
 * @param modelClass Model class.
 * @param snapshot Model output snapshot without metadata.
 * @param [internalId] Model internal ID, or `undefined` to generate a new one.
 * @returns The model snapshot (including metadata).
 */

function modelSnapshotOutWithMetadata(modelClass, snapshot, internalId) {
  var _extends3;

  if (internalId === void 0) {
    internalId = getGlobalConfig().modelIdGenerator();
  }

  assertIsModelClass(modelClass, "modelClass");
  assertIsObject(snapshot, "initialData");
  var modelInfo = modelInfoByClass.get(modelClass);
  return _extends({}, snapshot, (_extends3 = {}, _extends3[modelTypeKey] = modelInfo.name, _extends3[modelIdKey] = internalId, _extends3));
}

/**
 * Checks if an object is a model instance.
 *
 * @param model
 * @returns
 */

function isModel(model) {
  return model instanceof BaseModel;
}
/**
 * @ignore
 * @internal
 *
 * Asserts something is actually a model.
 *
 * @param model
 * @param argName
 */

function assertIsModel(model, argName, customErrMsg) {
  if (customErrMsg === void 0) {
    customErrMsg = "must be a model instance";
  }

  if (!isModel(model)) {
    throw failure(argName + " " + customErrMsg);
  }
}
/**
 * @ignore
 * @internal
 */

function isModelClass(modelClass) {
  if (typeof modelClass !== "function") {
    return false;
  }

  if (modelClass !== BaseModel && !(modelClass.prototype instanceof BaseModel)) {
    return false;
  }

  return true;
}
/**
 * @ignore
 * @internal
 */

function assertIsModelClass(modelClass, argName) {
  if (typeof modelClass !== "function") {
    throw failure(argName + " must be a class");
  }

  if (modelClass !== BaseModel && !(modelClass.prototype instanceof BaseModel)) {
    throw failure(argName + " must extend Model");
  }
}
/**
 * @ignore
 * @internal
 */

function isModelSnapshot(sn) {
  return isPlainObject(sn) && !!sn[modelTypeKey];
}
/**
 * @ignore
 * @internal
 */

function checkModelDecoratorArgs(fnName, target, propertyKey) {
  if (typeof propertyKey !== "string") {
    throw failure(fnName + " cannot be used over symbol properties");
  }

  var errMessage = fnName + " must be used over model classes or instances";

  if (!target) {
    throw failure(errMessage);
  } // check target is a model object or extended class


  if (!(target instanceof BaseModel) && target !== BaseModel && !(target.prototype instanceof BaseModel)) {
    throw failure(errMessage);
  }
}

/**
 * Returns if the given function is a model action or not.
 *
 * @param fn Function to check.
 * @returns
 */

function isModelAction(fn) {
  return typeof fn === "function" && !!fn[modelActionSymbol];
}

function checkModelActionArgs(target, propertyKey, value) {
  if (typeof value !== "function") {
    throw failure("modelAction has to be used over functions");
  }

  checkModelDecoratorArgs("modelAction", target, propertyKey);
}
/**
 * Decorator that turns a function into a model action.
 *
 * @param target
 * @param propertyKey
 * @param [baseDescriptor]
 * @returns
 */


function modelAction(target, propertyKey, baseDescriptor) {
  return decorateWrapMethodOrField("modelAction", {
    target: target,
    propertyKey: propertyKey,
    baseDescriptor: baseDescriptor
  }, function (data, fn) {
    if (isModelAction(fn)) {
      return fn;
    } else {
      checkModelActionArgs(data.target, data.propertyKey, fn);
      return wrapInAction({
        name: data.propertyKey,
        fn: fn,
        actionType: ActionContextActionType.Sync
      });
    }
  });
}

var modelFlowSymbol = /*#__PURE__*/Symbol("modelFlow");
/**
 * @ignore
 * @internal
 */

function flow(name, generator) {
  // Implementation based on https://github.com/tj/co/blob/master/index.js
  var flowFn = function flowFn() {
    for (var _len = arguments.length, args = new Array(_len), _key = 0; _key < _len; _key++) {
      args[_key] = arguments[_key];
    }

    var target = this;

    if (inDevMode()) {
      assertTweakedObject(target, "flow");
    }

    var previousAsyncStepContext;

    var ctxOverride = function ctxOverride(stepType) {
      return function (ctx) {
        ctx.previousAsyncStepContext = previousAsyncStepContext;
        ctx.spawnAsyncStepContext = previousAsyncStepContext ? previousAsyncStepContext.spawnAsyncStepContext : ctx;
        ctx.asyncStepType = stepType;
        ctx.args = args;
        previousAsyncStepContext = ctx;
      };
    };

    var generatorRun = false;
    var gen = wrapInAction({
      name: name,
      fn: function fn() {
        generatorRun = true;
        return generator.apply(target, args);
      },
      actionType: ActionContextActionType.Async,
      overrideContext: ctxOverride(ActionContextAsyncStepType.Spawn)
    }).apply(target);

    if (!generatorRun) {
      // maybe it got overridden into a sync action
      return gen instanceof Promise ? gen : Promise.resolve(gen);
    } // use bound functions to fix es6 compilation


    var genNext = gen.next.bind(gen);
    var genThrow = gen["throw"].bind(gen);
    var promise = new Promise(function (resolve, reject) {
      function onFulfilled(res) {
        var ret;

        try {
          ret = wrapInAction({
            name: name,
            fn: genNext,
            actionType: ActionContextActionType.Async,
            overrideContext: ctxOverride(ActionContextAsyncStepType.Resume)
          }).call(target, res);
        } catch (e) {
          wrapInAction({
            name: name,
            fn: function fn(err) {
              // we use a flow finisher to allow middlewares to tweak the return value before resolution
              return {
                value: err,
                resolution: "reject",
                accepter: resolve,
                rejecter: reject
              };
            },
            actionType: ActionContextActionType.Async,
            overrideContext: ctxOverride(ActionContextAsyncStepType.Throw),
            isFlowFinisher: true
          }).call(target, e);
          return;
        }

        next(ret);
      }

      function onRejected(err) {
        var ret;

        try {
          ret = wrapInAction({
            name: name,
            fn: genThrow,
            actionType: ActionContextActionType.Async,
            overrideContext: ctxOverride(ActionContextAsyncStepType.ResumeError)
          }).call(target, err);
        } catch (e) {
          wrapInAction({
            name: name,
            fn: function fn(err) {
              // we use a flow finisher to allow middlewares to tweak the return value before resolution
              return {
                value: err,
                resolution: "reject",
                accepter: resolve,
                rejecter: reject
              };
            },
            actionType: ActionContextActionType.Async,
            overrideContext: ctxOverride(ActionContextAsyncStepType.Throw),
            isFlowFinisher: true
          }).call(target, e);
          return;
        }

        next(ret);
      }

      function next(ret) {
        if (ret && typeof ret.then === "function") {
          // an async iterator
          ret.then(next, reject);
        } else if (ret.done) {
          // done
          wrapInAction({
            name: name,
            fn: function fn(val) {
              // we use a flow finisher to allow middlewares to tweak the return value before resolution
              return {
                value: val,
                resolution: "accept",
                accepter: resolve,
                rejecter: reject
              };
            },
            actionType: ActionContextActionType.Async,
            overrideContext: ctxOverride(ActionContextAsyncStepType.Return),
            isFlowFinisher: true
          }).call(target, ret.value);
        } else {
          // continue
          Promise.resolve(ret.value).then(onFulfilled, onRejected);
        }
      }

      onFulfilled(undefined); // kick off the process
    });
    return promise;
  };

  flowFn[modelFlowSymbol] = true;
  return flowFn;
}
/**
 * Returns if the given function is a model flow or not.
 *
 * @param fn Function to check.
 * @returns
 */

function isModelFlow(fn) {
  return typeof fn === "function" && fn[modelFlowSymbol];
}
/**
 * Decorator that turns a function generator into a model flow.
 *
 * @param target
 * @param propertyKey
 * @param [baseDescriptor]
 * @returns
 */

function modelFlow(target, propertyKey, baseDescriptor) {
  return decorateWrapMethodOrField("modelFlow", {
    target: target,
    propertyKey: propertyKey,
    baseDescriptor: baseDescriptor
  }, function (data, fn) {
    if (isModelFlow(fn)) {
      return fn;
    } else {
      checkModelFlowArgs(data.target, data.propertyKey, fn);
      return flow(data.propertyKey, fn);
    }
  });
}

function checkModelFlowArgs(target, propertyKey, value) {
  if (typeof value !== "function") {
    throw failure("modelFlow has to be used over functions");
  }

  checkModelDecoratorArgs("modelFlow", target, propertyKey);
}
/**
 * Tricks the TS compiler into thinking that a model flow generator function can be awaited
 * (is a promise).
 *
 * @typeparam A Function arguments.
 * @typeparam R Return value.
 * @param fn Flow function.
 * @returns
 */


function _async(fn) {
  return fn;
}
/**
 * Makes a promise a flow, so it can be awaited with yield*.
 *
 * @typeparam T Promise return type.
 * @param promise Promise.
 * @returns
 */

function _await(promise) {
  return promiseGenerator.call(promise);
}
/*
function* promiseGenerator<T>(
  this: Promise<T>
) {
  const ret: T = yield this
  return ret
}
*/
// above code but compiled by TS for ES5
// so we don't include a dependency to regenerator runtime

var mobxkeystone_esm_generator = function __generator(thisArg, body) {
  var _ = {
    label: 0,
    sent: function sent() {
      if (t[0] & 1) throw t[1];
      return t[1];
    },
    trys: [],
    ops: []
  },
      f,
      y,
      t,
      g;
  return g = {
    next: verb(0),
    "throw": verb(1),
    "return": verb(2)
  }, typeof Symbol === "function" && (g[Symbol.iterator] = function () {
    return this;
  }), g;

  function verb(n) {
    return function (v) {
      return step([n, v]);
    };
  }

  function step(op) {
    if (f) throw new TypeError("Generator is already executing.");

    while (_) {
      try {
        if (f = 1, y && (t = op[0] & 2 ? y["return"] : op[0] ? y["throw"] || ((t = y["return"]) && t.call(y), 0) : y.next) && !(t = t.call(y, op[1])).done) return t;
        if (y = 0, t) op = [op[0] & 2, t.value];

        switch (op[0]) {
          case 0:
          case 1:
            t = op;
            break;

          case 4:
            _.label++;
            return {
              value: op[1],
              done: false
            };

          case 5:
            _.label++;
            y = op[1];
            op = [0];
            continue;

          case 7:
            op = _.ops.pop();

            _.trys.pop();

            continue;

          default:
            if (!(t = _.trys, t = t.length > 0 && t[t.length - 1]) && (op[0] === 6 || op[0] === 2)) {
              _ = 0;
              continue;
            }

            if (op[0] === 3 && (!t || op[1] > t[0] && op[1] < t[3])) {
              _.label = op[1];
              break;
            }

            if (op[0] === 6 && _.label < t[1]) {
              _.label = t[1];
              t = op;
              break;
            }

            if (t && _.label < t[2]) {
              _.label = t[2];

              _.ops.push(op);

              break;
            }

            if (t[2]) _.ops.pop();

            _.trys.pop();

            continue;
        }

        op = body.call(thisArg, _);
      } catch (e) {
        op = [6, e];
        y = 0;
      } finally {
        f = t = 0;
      }
    }

    if (op[0] & 5) throw op[1];
    return {
      value: op[0] ? op[1] : void 0,
      done: true
    };
  }
};

function promiseGenerator() {
  var ret;
  return mobxkeystone_esm_generator(this, function (_a) {
    switch (_a.label) {
      case 0:
        return [4
        /*yield*/
        , this];

      case 1:
        ret = _a.sent();
        return [2
        /*return*/
        , ret];

      default:
        return;
    }
  });
}

/**
 * @ignore
 * @internal
 */

function assertFnModelKeyNotInUse(fnModelObj, key) {
  if (fnModelObj[key] !== undefined) {
    throw failure("key '" + key + "' cannot be redeclared");
  }
}

var fnModelActionRegistry = /*#__PURE__*/new Map();
/**
 * @ignore
 * @internal
 */

function getFnModelAction(actionName) {
  return fnModelActionRegistry.get(actionName);
}
/**
 * @ignore
 * @internal
 */

function extendFnModelActions(fnModelObj, namespace, actions) {
  for (var _i = 0, _Object$entries = Object.entries(actions); _i < _Object$entries.length; _i++) {
    var _Object$entries$_i = _Object$entries[_i],
        name = _Object$entries$_i[0],
        fn = _Object$entries$_i[1];
    addActionToFnModel(fnModelObj, namespace, name, fn, false);
  }

  return fnModelObj;
}
/**
 * @ignore
 * @internal
 */

function addActionToFnModel(fnModelObj, namespace, name, fn, isFlow) {
  assertFnModelKeyNotInUse(fnModelObj, name);
  var fullActionName = namespace + "::" + name;
  assertIsFunction(fn, fullActionName);

  if (fnModelActionRegistry.has(fullActionName)) {
    logWarning("warn", "an standalone action with name \"" + fullActionName + "\" already exists (if you are using hot-reloading you may safely ignore this warning)", "duplicateActionName - " + name);
  }

  if (isModelAction(fn)) {
    throw failure("the standalone action must not be previously marked as an action");
  }

  if (isModelFlow(fn)) {
    throw failure("the standalone action must not be previously marked as a flow action");
  }

  var wrappedAction = isFlow ? flow(fullActionName, fn) : wrapInAction({
    name: fullActionName,
    fn: fn,
    actionType: ActionContextActionType.Sync
  });

  fnModelObj[name] = function (target) {
    for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
      args[_key - 1] = arguments[_key];
    }

    return wrappedAction.apply(target, args);
  };

  fnModelActionRegistry.set(fullActionName, fnModelObj[name]);
}

/**
 * Applies a full snapshot over an object, reconciling it with the current contents of the object.
 *
 * @typeparam T Object type.
 * @param node Target object (model object, object or array).
 * @param snapshot Snapshot to apply.
 */

function applySnapshot(node, snapshot) {
  assertTweakedObject(node, "node");
  assertIsObject(snapshot, "snapshot");
  wrappedInternalApplySnapshot().call(node, snapshot);
}

function internalApplySnapshot(sn) {
  var obj = this;

  var reconcile = function reconcile() {
    var modelPool = new ModelPool(obj);
    var ret = reconcileSnapshot(obj, sn, modelPool);

    if (inDevMode()) {
      if (ret !== obj) {
        throw failure("assertion error: reconciled object has to be the same");
      }
    }
  };

  if (isArray(sn)) {
    if (!isArray(obj)) {
      throw failure("if the snapshot is an array the target must be an array too");
    }

    return reconcile();
  }

  if (isFrozenSnapshot(sn)) {
    throw failure("applySnapshot cannot be used over frozen objects");
  }

  if (isModelSnapshot(sn)) {
    var type = sn[modelTypeKey];
    var modelInfo = getModelInfoForName(type);

    if (!modelInfo) {
      throw failure("model with name \"" + type + "\" not found in the registry");
    } // we don't check by actual instance since it might be a different one due to hot reloading


    if (!isModel(obj)) {
      // not a model instance, no reconciliation possible
      throw failure("the target for a model snapshot must be a model instance");
    }

    if (obj[modelTypeKey] !== type) {
      // different kind of model, no reconciliation possible
      throw failure("snapshot model type '" + type + "' does not match target model type '" + obj[modelTypeKey] + "'");
    }

    var id = sn[modelIdKey];

    if (obj[modelIdKey] !== id) {
      // different id, no reconciliation possible
      throw failure("snapshot model id '" + id + "' does not match target model id '" + obj[modelIdKey] + "'");
    }

    return reconcile();
  }

  if (isPlainObject(sn)) {
    if (!isPlainObject(obj) && !(0,mobx_esm.isObservableObject)(obj)) {
      // no reconciliation possible
      throw failure("if the snapshot is an object the target must be an object too");
    }

    return reconcile();
  }

  throw failure("unsupported snapshot - " + sn);
}

var wrappedInternalApplySnapshot = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.ApplySnapshot,
    fn: internalApplySnapshot,
    actionType: ActionContextActionType.Sync
  });
});

/**
 * Deletes an object field wrapped in an action.
 *
 * @param node  Target object.
 * @param fieldName Field name.
 */

function applyDelete(node, fieldName) {
  assertTweakedObject(node, "node");
  wrappedInternalApplyDelete().call(node, fieldName);
}
/**
 * @ignore
 * @internal
 */

function internalApplyDelete(fieldName) {
  (0,mobx_esm.remove)(this, "" + fieldName);
}
var wrappedInternalApplyDelete = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.ApplyDelete,
    fn: internalApplyDelete,
    actionType: ActionContextActionType.Sync
  });
});

/**
 * Calls an object method wrapped in an action.
 *
 * @param node  Target object.
 * @param methodName Method name.
 */

function applyMethodCall(node, methodName) {
  assertTweakedObject(node, "node");

  for (var _len = arguments.length, args = new Array(_len > 2 ? _len - 2 : 0), _key = 2; _key < _len; _key++) {
    args[_key - 2] = arguments[_key];
  }

  return wrappedInternalApplyMethodCall().call(node, methodName, args);
}
/**
 * @ignore
 * @internal
 */

function internalApplyMethodCall(methodName, args) {
  return this[methodName].apply(this, args);
}
var wrappedInternalApplyMethodCall = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.ApplyMethodCall,
    fn: internalApplyMethodCall,
    actionType: ActionContextActionType.Sync
  });
});

/**
 * Sets an object field wrapped in an action.
 *
 * @param node  Target object.
 * @param fieldName Field name.
 * @param value Value to set.
 */

function applySet(node, fieldName, value) {
  assertTweakedObject(node, "node");
  wrappedInternalApplySet().call(node, fieldName, value);
}

function internalApplySet(fieldName, value) {
  // we need to check if it is a model since models can become observable objects
  // (e.g. by having a computed value)
  if (!isModel(this) && (0,mobx_esm.isObservable)(this)) {
    (0,mobx_esm.set)(this, fieldName, value);
  } else {
    this[fieldName] = value;
  }
}

var wrappedInternalApplySet = /*#__PURE__*/lazy(function () {
  return wrapInAction({
    name: BuiltInAction.ApplySet,
    fn: internalApplySet,
    actionType: ActionContextActionType.Sync
  });
});

var _builtInActionToFunct;
var builtInActionToFunction = (_builtInActionToFunct = {}, _builtInActionToFunct[BuiltInAction.ApplySnapshot] = applySnapshot, _builtInActionToFunct[BuiltInAction.ApplyPatches] = applyPatches, _builtInActionToFunct[BuiltInAction.Detach] = detach, _builtInActionToFunct[BuiltInAction.ApplySet] = applySet, _builtInActionToFunct[BuiltInAction.ApplyDelete] = applyDelete, _builtInActionToFunct[BuiltInAction.ApplyMethodCall] = applyMethodCall, _builtInActionToFunct);
/**
 * Applies (runs) an action over a target object.
 *
 * If you intend to apply serialized actions check one of the `applySerializedAction` methods instead.
 *
 * @param subtreeRoot Subtree root target object to run the action over.
 * @param call The action, usually as coming from `onActionMiddleware`.
 * @returns The return value of the action, if any.
 */

function applyAction(subtreeRoot, call) {
  if (call.serialized) {
    throw failure("cannot apply a serialized action call, use one of the 'applySerializedAction' methods instead");
  }

  assertTweakedObject(subtreeRoot, "subtreeRoot"); // resolve path while checking ids

  var _resolvePathCheckingI = resolvePathCheckingIds(subtreeRoot, call.targetPath, call.targetPathIds),
      current = _resolvePathCheckingI.value,
      resolved = _resolvePathCheckingI.resolved;

  if (!resolved) {
    throw failure("object at path " + JSON.stringify(call.targetPath) + " with ids " + JSON.stringify(call.targetPathIds) + " could not be resolved");
  }

  assertTweakedObject(current, "resolved " + current, true);

  if (isBuiltInAction(call.actionName)) {
    var fnToCall = builtInActionToFunction[call.actionName];

    if (!fnToCall) {
      throw failure("assertion error: unknown built-in action - " + call.actionName);
    }

    return fnToCall.apply(current, [current].concat(call.args));
  } else if (isHookAction(call.actionName)) {
    throw failure("calls to hooks (" + call.actionName + ") cannot be applied");
  } else {
    var standaloneAction = getFnModelAction(call.actionName);

    if (standaloneAction) {
      return standaloneAction.apply(current, call.args);
    } else {
      return current[call.actionName].apply(current, call.args);
    }
  }
}

var cannotSerialize = /*#__PURE__*/Symbol("cannotSerialize");

var arraySerializer = {
  id: "mobx-keystone/array",
  serialize: function serialize(value, _serialize) {
    if (!isArray(value)) return cannotSerialize; // this will also transform observable arrays into non-observable ones

    return value.map(_serialize);
  },
  deserialize: function deserialize(arr, _deserialize) {
    return arr.map(_deserialize);
  }
};

var dateSerializer = {
  id: "mobx-keystone/dateAsTimestamp",
  serialize: function serialize(date) {
    if (!(date instanceof Date)) return cannotSerialize;
    return +date;
  },
  deserialize: function deserialize(timestamp) {
    return new Date(timestamp);
  }
};

var mapSerializer = {
  id: "mobx-keystone/mapAsArray",
  serialize: function serialize(map, _serialize) {
    if (!(map instanceof Map) && !(0,mobx_esm.isObservableMap)(map)) return cannotSerialize;
    var arr = [];
    var iter = map.keys();
    var cur = iter.next();

    while (!cur.done) {
      var k = cur.value;
      var v = map.get(k);
      arr.push([_serialize(k), _serialize(v)]);
      cur = iter.next();
    }

    return arr;
  },
  deserialize: function deserialize(arr, _deserialize) {
    var map = new Map();
    var len = arr.length;

    for (var i = 0; i < len; i++) {
      var k = arr[i][0];
      var v = arr[i][1];
      map.set(_deserialize(k), _deserialize(v));
    }

    return map;
  }
};

/**
 * @ignore
 */

function rootPathToTargetPathIds(rootPath) {
  var targetPathIds = [];

  for (var i = 0; i < rootPath.path.length; i++) {
    var targetObj = rootPath.pathObjects[i + 1]; // first is root, we don't care about its ID

    var targetObjId = isModel(targetObj) ? targetObj[modelIdKey] : null;
    targetPathIds.push(targetObjId);
  }

  return targetPathIds;
}
/**
 * @ignore
 */

function pathToTargetPathIds(root, path) {
  var targetPathIds = [];
  var current = root; // we don't care about the root ID

  for (var i = 0; i < path.length; i++) {
    current = current[path[i]];
    var targetObjId = isModel(current) ? current[modelIdKey] : null;
    targetPathIds.push(targetObjId);
  }

  return targetPathIds;
}

var objectPathSerializer = {
  id: "mobx-keystone/objectPath",
  serialize: function serialize(value, _, targetRoot) {
    if (typeof value !== "object" || value === null || !isTweakedObject(value, false)) return cannotSerialize; // try to serialize a ref to its path if possible instead

    if (targetRoot) {
      var rootPath = fastGetRootPath(value);

      if (rootPath.root === targetRoot) {
        return {
          targetPath: rootPath.path,
          targetPathIds: rootPathToTargetPathIds(rootPath)
        };
      }
    }

    return cannotSerialize;
  },
  deserialize: function deserialize(ref, _, targetRoot) {
    // try to resolve the node back
    if (targetRoot) {
      var result = resolvePathCheckingIds(targetRoot, ref.targetPath, ref.targetPathIds);

      if (result.resolved) {
        return result.value;
      }
    }

    throw failure("object at path " + JSON.stringify(ref.targetPath) + " with ids " + JSON.stringify(ref.targetPathIds) + " could not be resolved");
  }
};

var objectSnapshotSerializer = {
  id: "mobx-keystone/objectSnapshot",
  serialize: function serialize(value) {
    if (typeof value !== "object" || value === null || !isTweakedObject(value, false)) return cannotSerialize;
    return getSnapshot(value);
  },
  deserialize: function deserialize(snapshot) {
    return fromSnapshot(snapshot);
  }
};

var plainObjectSerializer = {
  id: "mobx-keystone/plainObject",
  serialize: function serialize(value, _serialize) {
    if (!isPlainObject(value) && !(0,mobx_esm.isObservableObject)(value)) return cannotSerialize; // this will make observable objects non-observable ones

    return mapObjectFields(value, _serialize);
  },
  deserialize: function deserialize(obj, serialize) {
    return mapObjectFields(obj, serialize);
  }
};

function mapObjectFields(originalObj, mapFn) {
  var obj = {};
  var keys = Object.keys(originalObj);
  var len = keys.length;

  for (var i = 0; i < len; i++) {
    var k = keys[i];
    var v = originalObj[k];
    obj[k] = mapFn(v);
  }

  return obj;
}

var setSerializer = {
  id: "mobx-keystone/setAsArray",
  serialize: function serialize(set, _serialize) {
    if (!(set instanceof Set)) return cannotSerialize;
    var arr = [];
    var iter = set.keys();
    var cur = iter.next();

    while (!cur.done) {
      var k = cur.value;
      arr.push(_serialize(k));
      cur = iter.next();
    }

    return arr;
  },
  deserialize: function deserialize(arr, _deserialize) {
    var set = new Set();
    var len = arr.length;

    for (var i = 0; i < len; i++) {
      var k = arr[i];
      set.add(_deserialize(k));
    }

    return set;
  }
};

var serializersArray = [];
var serializersMap = /*#__PURE__*/new Map();
/**
 * Registers a new action call argument serializers.
 * Serializers are called in the inverse order they are registered, meaning the
 * latest one registered will be called first.
 *
 * @param serializer Serializer to register.
 * @returns A disposer to unregister the serializer.
 */

function registerActionCallArgumentSerializer(serializer) {
  if (serializersArray.includes(serializer)) {
    throw failure("action call argument serializer already registered");
  }

  if (serializersMap.has(serializer.id)) {
    throw failure("action call argument serializer with id '" + serializer.id + "' already registered");
  }

  serializersArray.unshift(serializer);
  serializersMap.set(serializer.id, serializer);
  return function () {
    var index = serializersArray.indexOf(serializer);

    if (index >= 0) {
      serializersArray.splice(index, 1);
    }

    serializersMap["delete"](serializer.id);
  };
}
/**
 * Transforms an action call argument by returning a `SerializedActionCallArgument`.
 * The following are supported out of the box:
 * - Primitives.
 * - Nodes that are under the same root node as the target root (when provided) will be serialized
 *   as a path.
 * - Nodes that are not under the same root node as the target root will be serialized as their snapshot.
 * - Arrays (observable or not).
 * - Dates.
 * - Maps (observable or not).
 * - Sets (observable or not).
 * - Plain objects (observable or not).
 *
 * If the value cannot be serialized it will throw an exception.
 *
 * @param argValue Argument value to be transformed into its serializable form.
 * @param [targetRoot] Target root node of the model where this action is being performed.
 * @returns The serializable form of the passed value.
 */

function serializeActionCallArgument(argValue, targetRoot) {
  if (isPrimitive(argValue)) {
    return argValue;
  }

  var origValue = argValue;

  var serialize = function serialize(v) {
    return serializeActionCallArgument(v, targetRoot);
  }; // try serializers


  for (var i = 0; i < serializersArray.length; i++) {
    var serializer = serializersArray[i];
    var serializedValue = serializer.serialize(argValue, serialize, targetRoot);

    if (serializedValue !== cannotSerialize) {
      return {
        $mobxKeystoneSerializer: serializer.id,
        value: serializedValue
      };
    }
  }

  throw failure("serializeActionCallArgument could not serialize the given value: " + origValue);
}
/**
 * Ensures that an action call is serializable by mapping the action arguments into its
 * serializable version by using `serializeActionCallArgument`.
 *
 * @param actionCall Action call to convert.
 * @param [targetRoot] Target root node of the model where this action is being performed.
 * @returns The serializable action call.
 */

function serializeActionCall(actionCall, targetRoot) {
  if (actionCall.serialized) {
    throw failure("cannot serialize an already serialized action call");
  }

  if (targetRoot !== undefined) {
    assertTweakedObject(targetRoot, "targetRoot");
  }

  var serialize = function serialize(v) {
    return serializeActionCallArgument(v, targetRoot);
  };

  return _extends({}, actionCall, {
    serialized: true,
    args: actionCall.args.map(serialize)
  });
}
/**
 * Transforms an action call argument by returning its deserialized equivalent.
 *
 * @param argValue Argument value to be transformed into its deserialized form.
 * @param [targetRoot] Target root node of the model where this action is being performed.
 * @returns The deserialized form of the passed value.
 */

function deserializeActionCallArgument(argValue, targetRoot) {
  if (isPrimitive(argValue)) {
    return argValue;
  }

  if (!isPlainObject(argValue) || typeof argValue.$mobxKeystoneSerializer !== "string") {
    throw failure("invalid serialized action call argument");
  }

  var serializerId = argValue.$mobxKeystoneSerializer;
  var serializer = serializersMap.get(serializerId);

  if (!serializer) {
    throw failure("a serializer with id '" + serializerId + "' could not be found");
  }

  var serializedValue = argValue;

  var deserialize = function deserialize(v) {
    return deserializeActionCallArgument(v, targetRoot);
  };

  return serializer.deserialize(serializedValue.value, deserialize, targetRoot);
}
/**
 * Ensures that an action call is deserialized by mapping the action arguments into its
 * deserialized version by using `deserializeActionCallArgument`.
 *
 * @param actionCall Action call to convert.
 * @param [targetRoot] Target root node of the model where this action is being performed.
 * @returns The deserialized action call.
 */

function deserializeActionCall(actionCall, targetRoot) {
  if (!actionCall.serialized) {
    throw failure("cannot deserialize a non-serialized action call");
  }

  if (targetRoot !== undefined) {
    assertTweakedObject(targetRoot, "targetRoot");
  }

  var deserialize = function deserialize(v) {
    return deserializeActionCallArgument(v, targetRoot);
  };

  var deserializedActionCall = _extends({}, actionCall, {
    serialized: undefined,
    args: actionCall.args.map(deserialize)
  });

  delete deserializedActionCall.serialized;
  return deserializedActionCall;
} // serializer registration (from low priority to high priority)

registerActionCallArgumentSerializer(plainObjectSerializer);
registerActionCallArgumentSerializer(setSerializer);
registerActionCallArgumentSerializer(mapSerializer);
registerActionCallArgumentSerializer(dateSerializer);
registerActionCallArgumentSerializer(arraySerializer);
registerActionCallArgumentSerializer(objectSnapshotSerializer);
registerActionCallArgumentSerializer(objectPathSerializer);

function typesObjectHelper(objFn, frozen, typeInfoGen) {
  assertIsFunction(objFn, "objFn");
  return lateTypeChecker(function () {
    var objectSchema = objFn();
    assertIsObject(objectSchema, "objectSchema");
    var schemaEntries = Object.entries(objectSchema);

    var getTypeName = function getTypeName() {
      var propsMsg = [];

      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      for (var _iterator = _createForOfIteratorHelperLoose(schemaEntries), _step; !(_step = _iterator()).done;) {
        var _step$value = _step.value,
            k = _step$value[0],
            unresolvedTc = _step$value[1];
        var tc = resolveTypeChecker(unresolvedTc);
        var propTypename = "...";

        if (!recursiveTypeCheckers.includes(tc)) {
          propTypename = tc.getTypeName.apply(tc, recursiveTypeCheckers.concat([tc]));
        }

        propsMsg.push(k + ": " + propTypename + ";");
      }

      return "{ " + propsMsg.join(" ") + " }";
    };

    var thisTc = new TypeChecker(function (obj, path) {
      if (!isObject(obj) || frozen && !(obj instanceof Frozen)) return new TypeCheckError(path, getTypeName(thisTc), obj); // note: we allow excess properties when checking objects

      for (var _iterator2 = _createForOfIteratorHelperLoose(schemaEntries), _step2; !(_step2 = _iterator2()).done;) {
        var _step2$value = _step2.value,
            k = _step2$value[0],
            unresolvedTc = _step2$value[1];
        var tc = resolveTypeChecker(unresolvedTc);
        var objVal = obj[k];
        var valueError = !tc.unchecked ? tc.check(objVal, [].concat(path, [k])) : null;

        if (valueError) {
          return valueError;
        }
      }

      return null;
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * A type that represents a plain object.
 * Note that the parameter must be a function that returns an object. This is done so objects can support self / cross types.
 *
 * Example:
 * ```ts
 * // notice the ({ ... }), not just { ... }
 * const pointType = types.object(() => ({
 *   x: types.number,
 *   y: types.number
 * }))
 * ```
 *
 * @typeparam T Type.
 * @param objectFunction Function that generates an object with types.
 * @returns
 */


function typesObject(objectFunction) {
  // we can't type this function or else we won't be able to make it work recursively
  var typeInfoGen = function typeInfoGen(t) {
    return new ObjectTypeInfo(t, objectFunction);
  };

  return typesObjectHelper(objectFunction, false, typeInfoGen);
}
/**
 * `types.object` type info.
 */

var ObjectTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(ObjectTypeInfo, _TypeInfo);

  _createClass(ObjectTypeInfo, [{
    key: "props",
    // memoize to always return the same object
    get: function get() {
      return this._props();
    }
  }]);

  function ObjectTypeInfo(thisType, _objTypeFn) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this._objTypeFn = void 0;
    _this._props = lateVal(function () {
      var objSchema = _this._objTypeFn();

      var propTypes = {};
      Object.keys(objSchema).forEach(function (propName) {
        var type = resolveStandardType(objSchema[propName]);
        propTypes[propName] = {
          type: type,
          typeInfo: getTypeInfo(type)
        };
      });
      return propTypes;
    });
    _this._objTypeFn = _objTypeFn;
    return _this;
  }

  return ObjectTypeInfo;
}(TypeInfo);
/**
 * A type that represents frozen data.
 *
 * Example:
 * ```ts
 * const frozenNumberType = types.frozen(types.number)
 * const frozenAnyType = types.frozen(types.unchecked<any>())
 * const frozenNumberArrayType = types.frozen(types.array(types.number))
 * const frozenUncheckedNumberArrayType = types.frozen(types.unchecked<number[]>())
 * ```
 *
 * @typeParam T Type.
 * @param dataType Type of the frozen data.
 * @returns
 */

function typesFrozen(dataType) {
  return typesObjectHelper(function () {
    return {
      data: dataType
    };
  }, true, function (t) {
    return new FrozenTypeInfo(t, resolveStandardType(dataType));
  });
}
/**
 * `types.frozen` type info.
 */

var FrozenTypeInfo = /*#__PURE__*/function (_TypeInfo2) {
  _inheritsLoose(FrozenTypeInfo, _TypeInfo2);

  _createClass(FrozenTypeInfo, [{
    key: "dataTypeInfo",
    get: function get() {
      return getTypeInfo(this.dataType);
    }
  }]);

  function FrozenTypeInfo(thisType, dataType) {
    var _this2;

    _this2 = _TypeInfo2.call(this, thisType) || this;
    _this2.dataType = void 0;
    _this2.dataType = dataType;
    return _this2;
  }

  return FrozenTypeInfo;
}(TypeInfo);

var unchecked = /*#__PURE__*/new TypeChecker(null, function () {
  return "any";
}, function (t) {
  return new UncheckedTypeInfo(t);
});
/**
 * A type that represents a given value that won't be type checked.
 * This is basically a way to bail out of the runtime type checking system.
 *
 * Example:
 * ```ts
 * const uncheckedSomeModel = types.unchecked<SomeModel>()
 * const anyType = types.unchecked<any>()
 * const customUncheckedType = types.unchecked<(A & B) | C>()
 * ```
 *
 * @typeparam T Type of the value, or unkown if not given.
 * @returns
 */

function typesUnchecked() {
  return unchecked;
}
/**
 * `types.unchecked` type info.
 */

var UncheckedTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(UncheckedTypeInfo, _TypeInfo);

  function UncheckedTypeInfo() {
    return _TypeInfo.apply(this, arguments) || this;
  }

  return UncheckedTypeInfo;
}(TypeInfo);

/**
 * Base abstract class for models that extends another model.
 *
 * @typeparam TProps New model properties type.
 * @typeparam TBaseModel Base class type.
 * @param baseModel Base model type.
 * @param modelProps Model properties.
 * @returns
 */

function ExtendedModel(baseModel, modelProps) {
  // note that & Object is there to support abstract classes
  return internalModel(modelProps, baseModel);
}
/**
 * Base abstract class for models.
 *
 * Never override the constructor, use `onInit` or `onAttachedToRootStore` instead.
 *
 * @typeparam TProps Model properties type.
 * @param modelProps Model properties.
 */

function Model(modelProps) {
  return internalModel(modelProps);
}

function internalModel(modelProps, baseModel) {
  var _baseModel;

  assertIsObject(modelProps, "modelProps");

  if (baseModel) {
    assertIsModelClass(baseModel, "baseModel"); // if the baseModel is wrapped with the model decorator get the original one

    var unwrappedClass = baseModel[modelUnwrappedClassSymbol];

    if (unwrappedClass) {
      baseModel = unwrappedClass;
      assertIsModelClass(baseModel, "baseModel");
    }
  }

  var extraDescriptors = {};
  var composedModelProps = modelProps;

  if (baseModel) {
    var oldModelProps = getInternalModelClassPropsInfo(baseModel);

    for (var _i = 0, _Object$keys = Object.keys(oldModelProps); _i < _Object$keys.length; _i++) {
      var oldModelPropKey = _Object$keys[_i];

      if (modelProps[oldModelPropKey]) {
        throw failure("extended model cannot redeclare base model property named '" + oldModelPropKey + "'");
      }

      composedModelProps[oldModelPropKey] = oldModelProps[oldModelPropKey];
    }
  } else {
    // define $modelId on the base
    extraDescriptors[modelIdKey] = createModelPropDescriptor(modelIdKey, undefined, true);
  } // create type checker if needed


  var dataTypeChecker;

  if (Object.values(composedModelProps).some(function (mp) {
    return !!mp.typeChecker;
  })) {
    var typeCheckerObj = {};

    for (var _i2 = 0, _Object$entries = Object.entries(composedModelProps); _i2 < _Object$entries.length; _i2++) {
      var _Object$entries$_i = _Object$entries[_i2],
          k = _Object$entries$_i[0],
          mp = _Object$entries$_i[1];
      typeCheckerObj[k] = !mp.typeChecker ? typesUnchecked() : mp.typeChecker;
    }

    dataTypeChecker = typesObject(function () {
      return typeCheckerObj;
    });
  } // skip props that are on base model, these have to be accessed through $
  // we only need to proxy new props, not old ones


  for (var _iterator = _createForOfIteratorHelperLoose(Object.keys(modelProps).filter(function (mp) {
    return !baseModelPropNames.has(mp);
  })), _step; !(_step = _iterator()).done;) {
    var modelPropName = _step.value;
    extraDescriptors[modelPropName] = createModelPropDescriptor(modelPropName, modelProps[modelPropName], false);
  }

  var extraPropNames = Object.keys(extraDescriptors);
  var extraPropNamesLen = extraPropNames.length;
  var base = (_baseModel = baseModel) != null ? _baseModel : BaseModel;
  var propsWithTransforms = Object.entries(modelProps).filter(function (_ref) {
    var _propName = _ref[0],
        prop = _ref[1];
    return !!prop.transform;
  }).map(function (_ref2) {
    var propName = _ref2[0],
        prop = _ref2[1];
    return [propName, prop.transform];
  }); // we use this weird hack rather than just class CustomBaseModel extends base {}
  // in order to work around problems with ES5 classes extending ES6 classes
  // see https://github.com/xaviergonz/mobx-keystone/issues/15

  var CustomBaseModel = function (_base) {
    _inheritsLoose$1(CustomBaseModel, _base);

    function CustomBaseModel(initialData, constructorOptions) {
      var _constructorOptions$m;

      var baseModel = new base(initialData, _extends({}, constructorOptions, {
        modelClass: (_constructorOptions$m = constructorOptions == null ? void 0 : constructorOptions.modelClass) != null ? _constructorOptions$m : this.constructor,
        propsWithTransforms: propsWithTransforms
      })); // make sure abstract classes do not override prototype props

      for (var i = 0; i < extraPropNamesLen; i++) {
        var extraPropName = extraPropNames[i];

        if (Object.getOwnPropertyDescriptor(baseModel, extraPropName)) {
          delete baseModel[extraPropName];
        }
      }

      return baseModel;
    }

    return CustomBaseModel;
  }(base);

  var initializers = base[modelInitializersSymbol];

  if (initializers) {
    CustomBaseModel[modelInitializersSymbol] = initializers.slice();
  }

  setInternalModelClassPropsInfo(CustomBaseModel, composedModelProps);
  CustomBaseModel[modelDataTypeCheckerSymbol] = dataTypeChecker;
  Object.defineProperties(CustomBaseModel.prototype, extraDescriptors);
  return CustomBaseModel;
}

function _inheritsLoose$1(subClass, superClass) {
  subClass.prototype = Object.create(superClass.prototype);
  subClass.prototype.constructor = subClass;
  subClass.__proto__ = superClass;
}

function createModelPropDescriptor(modelPropName, modelProp, enumerable) {
  return {
    enumerable: enumerable,
    configurable: true,
    get: function get() {
      return getModelInstanceDataField(this, modelProp, modelPropName);
    },
    set: function set(v) {
      // hack to only permit setting these values once fully constructed
      // this is to ignore abstract properties being set by babel
      // see https://github.com/xaviergonz/mobx-keystone/issues/18
      if (!this[modelInitializedSymbol]) {
        return;
      }

      setModelInstanceDataField(this, modelProp, modelPropName, v);
    }
  };
}

function getModelInstanceDataField(model, modelProp, modelPropName) {
  var transform = modelProp ? modelProp.transform : undefined;

  if (transform) {
    // no need to use get since these vars always get on the initial $
    var memoTransform = memoTransformCache.getOrCreateMemoTransform(model, modelPropName, transform);
    return memoTransform.propToData(model.$[modelPropName]);
  } else {
    // no need to use get since these vars always get on the initial $
    return model.$[modelPropName];
  }
}

function setModelInstanceDataField(model, modelProp, modelPropName, value) {
  if ((modelProp == null ? void 0 : modelProp.options.setterAction) && !getCurrentActionContext()) {
    // use apply set instead to wrap it in an action
    applySet(model, modelPropName, value);
    return;
  }

  var transform = modelProp == null ? void 0 : modelProp.transform;

  if (transform) {
    // no need to use set since these vars always get on the initial $
    var memoTransform = memoTransformCache.getOrCreateMemoTransform(model, modelPropName, transform);
    model.$[modelPropName] = memoTransform.dataToProp(value);
  } else {
    // no need to use set since these vars always get on the initial $
    model.$[modelPropName] = value;
  }
}

/**
 * Decorator that marks this class (which MUST inherit from the `Model` abstract class)
 * as a model.
 *
 * @param name Unique name for the model type. Note that this name must be unique for your whole
 * application, so it is usually a good idea to use some prefix unique to your application domain.
 */

var model = function model(name) {
  return function (clazz) {
    return internalModel$1(name)(clazz);
  };
};

var internalModel$1 = function internalModel(name) {
  return function (clazz) {
    assertIsModelClass(clazz, "a model class");

    if (modelInfoByName[name]) {
      logWarning("warn", "a model with name \"" + name + "\" already exists (if you are using hot-reloading you may safely ignore this warning)", "duplicateModelName - " + name);
    }

    if (clazz[modelUnwrappedClassSymbol]) {
      throw failure("a class already decorated with `@model` cannot be re-decorated");
    } // trick so plain new works


    var newClazz = function newClazz(initialData, snapshotInitialData, generateNewIds) {
      var instance = new clazz(initialData, snapshotInitialData, this.constructor, generateNewIds);
      runLateInitializationFunctions(instance); // compatibility with mobx 6

      if (getMobxVersion() >= 6) {
        try {
          ;
          (0,mobx_esm.makeObservable)(instance);
        } catch (err) {
          // sadly we need to use this hack since the PR to do this the proper way
          // was rejected on the mobx side
          if (err.message !== "[MobX] No annotations were passed to makeObservable, but no decorator members have been found either") {
            throw err;
          }
        }
      } // the object is ready


      addHiddenProp(instance, modelInitializedSymbol, true, false);

      if (instance.onInit) {
        wrapModelMethodInActionIfNeeded(instance, "onInit", HookAction.OnInit);
        instance.onInit();
      }

      return instance;
    };

    clazz.toString = function () {
      return "class " + clazz.name + "#" + name;
    };

    clazz[modelTypeKey] = name; // this also gives access to modelInitializersSymbol, modelPropertiesSymbol, modelDataTypeCheckerSymbol

    Object.setPrototypeOf(newClazz, clazz);
    newClazz.prototype = clazz.prototype;
    Object.defineProperty(newClazz, "name", _extends({}, Object.getOwnPropertyDescriptor(newClazz, "name"), {
      value: clazz.name
    }));
    newClazz[modelUnwrappedClassSymbol] = clazz;
    var modelInfo = {
      name: name,
      "class": newClazz
    };
    modelInfoByName[name] = modelInfo;
    modelInfoByClass.set(newClazz, modelInfo);
    modelInfoByClass.set(clazz, modelInfo);
    return newClazz;
  };
}; // basically taken from TS


function tsDecorate(decorators, target, key, desc) {
  var c = arguments.length,
      r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc,
      d;
  if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);else for (var i = decorators.length - 1; i >= 0; i--) {
    if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
  } // eslint-disable-next-line no-sequences

  return c > 3 && r && Object.defineProperty(target, key, r), r;
}
/**
 * Marks a class (which MUST inherit from the `Model` abstract class)
 * as a model and decorates some of its methods/properties.
 *
 * @param name Unique name for the model type. Note that this name must be unique for your whole
 * application, so it is usually a good idea to use some prefix unique to your application domain.
 * If you don't want to assign a name yet (e.g. for a base model) pass `undefined`.
 * @param clazz Model class.
 * @param decorators Decorators.
 */


function decoratedModel(name, clazz, decorators) {
  // decorate class members
  for (var _i = 0, _Object$entries = Object.entries(decorators); _i < _Object$entries.length; _i++) {
    var _Object$entries$_i = _Object$entries[_i],
        k = _Object$entries$_i[0],
        decorator = _Object$entries$_i[1];
    var prototypeValueDesc = Object.getOwnPropertyDescriptor(clazz.prototype, k); // TS seems to send null for methods in the prototype
    // (which we substitute for the descriptor to avoid a double look-up) and void 0 (undefined) for props

    tsDecorate(Array.isArray(decorator) ? decorator : [decorator], clazz.prototype, k, prototypeValueDesc ? prototypeValueDesc : void 0);
  }

  return name ? model(name)(clazz) : clazz;
}

/**
 * Applies (runs) a serialized action over a target object.
 * In this mode newly generated / modified model IDs will be tracked
 * so they can be later synchronized when applying it on another machine
 * via `applySerializedActionAndSyncNewModelIds`.
 * This means this method is usually used on the server side.
 *
 * If you intend to apply non-serialized actions check `applyAction` instead.
 *
 * @param subtreeRoot Subtree root target object to run the action over.
 * @param call The serialized action, usually as coming from the server/client.
 * @returns The return value of the action, if any, plus a new serialized action
 * with model overrides.
 */

function applySerializedActionAndTrackNewModelIds(subtreeRoot, call) {
  if (!call.serialized) {
    throw failure("cannot apply a non-serialized action call, use 'applyAction' instead");
  }

  assertTweakedObject(subtreeRoot, "subtreeRoot");
  var deserializedCall = deserializeActionCall(call, subtreeRoot);
  var modelIdOverrides = []; // set a patch listener to track changes to model ids

  var patchDisposer = onPatches(subtreeRoot, function (patches) {
    scanPatchesForModelIdChanges(subtreeRoot, modelIdOverrides, patches);
  });

  try {
    var returnValue = applyAction(subtreeRoot, deserializedCall);
    return {
      returnValue: returnValue,
      serializedActionCall: _extends({}, call, {
        modelIdOverrides: modelIdOverrides
      })
    };
  } finally {
    patchDisposer();
  }
}

function scanPatchesForModelIdChanges(root, modelIdOverrides, patches) {
  var len = patches.length;

  for (var i = 0; i < len; i++) {
    var patch = patches[i];

    if (patch.op === "replace" || patch.op === "add") {
      deepScanValueForModelIdChanges(root, modelIdOverrides, patch.value, patch.path);
    }
  }
}

function deepScanValueForModelIdChanges(root, modelIdOverrides, value, path) {
  if (path.length >= 1 && path[path.length - 1] === modelIdKey && typeof value === "string") {
    // ensure the parent is an actual model
    var parent = resolvePath(root, path.slice(0, path.length - 1)).value;

    if (isModel(parent)) {
      // found one
      modelIdOverrides.push({
        op: "replace",
        path: path.slice(),
        value: value
      });
    }
  } else if (Array.isArray(value)) {
    var len = value.length;

    for (var i = 0; i < len; i++) {
      path.push(i);
      deepScanValueForModelIdChanges(root, modelIdOverrides, value[i], path);
      path.pop();
    }
  } else if (isObject(value)) {
    // skip frozen values
    if (!value[frozenKey]) {
      var keys = Object.keys(value);
      var _len = keys.length;

      for (var _i = 0; _i < _len; _i++) {
        var propName = keys[_i];
        var propValue = value[propName];
        path.push(propName);
        deepScanValueForModelIdChanges(root, modelIdOverrides, propValue, path);
        path.pop();
      }
    }
  }
}
/**
 * Applies (runs) a serialized action over a target object.
 * In this mode newly generated / modified model IDs will be tracked
 * so they can be later synchronized when applying it on another machine
 * via `applySerializedActionAndSyncNewModelIds`.
 * This means this method is usually used on the server side.
 *
 * If you intend to apply non-serialized actions check `applyAction` instead.
 *
 * @param subtreeRoot Subtree root target object to run the action over.
 * @param call The serialized action, usually as coming from the server/client.
 * @returns The return value of the action, if any, plus a new serialized action
 * with model overrides.
 */


function applySerializedActionAndSyncNewModelIds(subtreeRoot, call) {
  if (!call.serialized) {
    throw failure("cannot apply a non-serialized action call, use 'applyAction' instead");
  }

  assertTweakedObject(subtreeRoot, "subtreeRoot");
  var deserializedCall = deserializeActionCall(call, subtreeRoot);
  var returnValue;
  (0,mobx_esm.runInAction)(function () {
    returnValue = applyAction(subtreeRoot, deserializedCall); // apply model id overrides

    applyPatches(subtreeRoot, call.modelIdOverrides);
  });
  return returnValue;
}

/**
 * Action tracking middleware finish result.
 */

var ActionTrackingResult;

(function (ActionTrackingResult) {
  /**
   * The action returned normally (without throwing).
   */
  ActionTrackingResult["Return"] = "return";
  /**
   * The action threw an error.
   */

  ActionTrackingResult["Throw"] = "throw";
})(ActionTrackingResult || (ActionTrackingResult = {}));
/**
 * Creates an action tracking middleware, which is a simplified version
 * of the standard action middleware.
 *
 * @param subtreeRoot Subtree root target object.
 * @param hooks Middleware hooks.
 * @returns The middleware disposer.
 */


function actionTrackingMiddleware(subtreeRoot, hooks) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  var dataSymbol = Symbol("actionTrackingMiddlewareData");

  function getCtxData(ctx) {
    return ctx.data[dataSymbol];
  }

  function setCtxData(ctx, partialData) {
    var currentData = ctx.data[dataSymbol];

    if (!currentData) {
      ctx.data[dataSymbol] = partialData;
    } else {
      Object.assign(currentData, partialData);
    }
  }

  var userFilter = function userFilter(ctx) {
    if (hooks.filter) {
      return hooks.filter(simplifyActionContext(ctx));
    }

    return true;
  };

  var resumeSuspendSupport = !!hooks.onResume || !!hooks.onSuspend;

  var filter = function filter(ctx) {
    if (ctx.type === ActionContextActionType.Sync) {
      // start and finish is on the same context
      var accepted = userFilter(ctx);

      if (accepted) {
        setCtxData(ctx, {
          startAccepted: true,
          state: "idle"
        });
      }

      return accepted;
    } else {
      switch (ctx.asyncStepType) {
        case ActionContextAsyncStepType.Spawn:
          var _accepted = userFilter(ctx);

          if (_accepted) {
            setCtxData(ctx, {
              startAccepted: true,
              state: "idle"
            });
          }

          return _accepted;

        case ActionContextAsyncStepType.Return:
        case ActionContextAsyncStepType.Throw:
          // depends if the spawn one was accepted or not
          var data = getCtxData(ctx.spawnAsyncStepContext);
          return data ? data.startAccepted : false;

        case ActionContextAsyncStepType.Resume:
        case ActionContextAsyncStepType.ResumeError:
          if (!resumeSuspendSupport) {
            return false;
          } else {
            // depends if the spawn one was accepted or not
            var _data = getCtxData(ctx.spawnAsyncStepContext);

            return _data ? _data.startAccepted : false;
          }

        default:
          return false;
      }
    }
  };

  var start = function start(simpleCtx) {
    setCtxData(simpleCtx, {
      state: "started"
    });

    if (hooks.onStart) {
      return hooks.onStart(simpleCtx) || undefined;
    }

    return undefined;
  };

  var finish = function finish(simpleCtx, ret) {
    // fakely resume and suspend the parent if needed
    var parentCtx = simpleCtx.parentContext;
    var parentResumed = false;

    if (parentCtx) {
      var parentData = getCtxData(parentCtx);

      if (parentData && parentData.startAccepted && parentData.state === "suspended") {
        parentResumed = true;
        resume(parentCtx, false);
      }
    }

    setCtxData(simpleCtx, {
      state: "finished"
    });

    if (hooks.onFinish) {
      ret = hooks.onFinish(simpleCtx, ret) || ret;
    }

    if (parentResumed) {
      suspend(parentCtx);
    }

    return ret;
  };

  var resume = function resume(simpleCtx, real) {
    // ensure parents are resumed
    var parentCtx = simpleCtx.parentContext;

    if (parentCtx) {
      var parentData = getCtxData(parentCtx);

      if (parentData && parentData.startAccepted && parentData.state === "suspended") {
        resume(parentCtx, false);
      }
    }

    setCtxData(simpleCtx, {
      state: real ? "realResumed" : "fakeResumed"
    });

    if (hooks.onResume) {
      hooks.onResume(simpleCtx);
    }
  };

  var suspend = function suspend(simpleCtx) {
    setCtxData(simpleCtx, {
      state: "suspended"
    });

    if (hooks.onSuspend) {
      hooks.onSuspend(simpleCtx);
    } // ensure parents are suspended if they were fakely resumed


    var parentCtx = simpleCtx.parentContext;

    if (parentCtx) {
      var parentData = getCtxData(parentCtx);

      if (parentData && parentData.startAccepted && parentData.state === "fakeResumed") {
        suspend(parentCtx);
      }
    }
  };

  var mware = function mware(ctx, next) {
    var simpleCtx = simplifyActionContext(ctx);
    var origNext = next;

    next = function next() {
      resume(simpleCtx, true);

      try {
        return origNext();
      } finally {
        suspend(simpleCtx);
      }
    };

    if (ctx.type === ActionContextActionType.Sync) {
      var retObj = start(simpleCtx);

      if (retObj) {
        // action cancelled / overriden by onStart
        resume(simpleCtx, true);
        suspend(simpleCtx);
        retObj = finish(simpleCtx, retObj);
      } else {
        try {
          retObj = finish(simpleCtx, {
            result: ActionTrackingResult.Return,
            value: next()
          });
        } catch (err) {
          retObj = finish(simpleCtx, {
            result: ActionTrackingResult.Throw,
            value: err
          });
        }
      }

      return returnOrThrowActionTrackingReturn(retObj);
    } else {
      // async
      switch (ctx.asyncStepType) {
        case ActionContextAsyncStepType.Spawn:
          {
            var _retObj = start(simpleCtx);

            if (_retObj) {
              // action cancelled / overriden by onStart
              resume(simpleCtx, true);
              suspend(simpleCtx);
              _retObj = finish(simpleCtx, _retObj);
              return returnOrThrowActionTrackingReturn(_retObj);
            } else {
              return next();
            }
          }

        case ActionContextAsyncStepType.Return:
          {
            var flowFinisher = next();

            var _retObj2 = finish(simpleCtx, {
              result: ActionTrackingResult.Return,
              value: flowFinisher.value
            });

            flowFinisher.resolution = _retObj2.result === ActionTrackingResult.Return ? "accept" : "reject";
            flowFinisher.value = _retObj2.value;
            return flowFinisher;
          }

        case ActionContextAsyncStepType.Throw:
          {
            var _flowFinisher = next();

            var _retObj3 = finish(simpleCtx, {
              result: ActionTrackingResult.Throw,
              value: _flowFinisher.value
            });

            _flowFinisher.resolution = _retObj3.result === ActionTrackingResult.Return ? "accept" : "reject";
            _flowFinisher.value = _retObj3.value;
            return _flowFinisher;
          }

        case ActionContextAsyncStepType.Resume:
        case ActionContextAsyncStepType.ResumeError:
          if (resumeSuspendSupport) {
            return next();
          } else {
            throw failure("asssertion error: async step should have been filtered out - " + ctx.asyncStepType);
          }

        default:
          throw failure("asssertion error: async step should have been filtered out - " + ctx.asyncStepType);
      }
    }
  };

  return addActionMiddleware({
    middleware: mware,
    filter: filter,
    subtreeRoot: subtreeRoot
  });
}

function returnOrThrowActionTrackingReturn(retObj) {
  if (retObj.result === ActionTrackingResult.Return) {
    return retObj.value;
  } else {
    throw retObj.value;
  }
}

var simpleDataContextSymbol = /*#__PURE__*/Symbol("simpleDataContext");
/**
 * Simplifies an action context by converting an async call hierarchy into a simpler one.
 *
 * @param ctx Action context to convert.
 * @returns Simplified action context.
 */

function simplifyActionContext(ctx) {
  while (ctx.previousAsyncStepContext) {
    ctx = ctx.previousAsyncStepContext;
  }

  var simpleCtx = ctx.data[simpleDataContextSymbol];

  if (!simpleCtx) {
    var parentContext = ctx.parentContext ? simplifyActionContext(ctx.parentContext) : undefined;
    simpleCtx = {
      actionName: ctx.actionName,
      type: ctx.type,
      target: ctx.target,
      args: ctx.args,
      data: ctx.data,
      parentContext: parentContext
    };
    simpleCtx.rootContext = parentContext ? parentContext.rootContext : simpleCtx;
    ctx.data[simpleDataContextSymbol] = simpleCtx;
  }

  return simpleCtx;
}

/**
 * Attaches an action middleware that invokes a listener for all actions of a given tree.
 * Note that the listener will only be invoked for the topmost level actions, so it won't run for child actions or intermediary flow steps.
 * Also it won't trigger the listener for calls to hooks such as `onAttachedToRootStore` or its returned disposer.
 *
 * Its main use is to keep track of top level actions that can be later replicated via `applyAction` somewhere else (another machine, etc.).
 *
 * There are two kind of possible listeners, `onStart` and `onFinish` listeners.
 * `onStart` listeners are called before the action executes and allow cancellation by returning a new return value (which might be a return or a throw).
 * `onFinish` listeners are called after the action executes, have access to the action actual return value and allow overriding by returning a
 * new return value (which might be a return or a throw).
 *
 * If you want to ensure that the actual action calls are serializable you should use either `serializeActionCallArgument` over the arguments
 * or `serializeActionCall` over the whole action before sending the action call over the wire / storing them .
 *
 * @param subtreeRoot Subtree root target object.
 * @param listeners Listener functions that will be invoked everytime a topmost action is invoked on the model or any children.
 * @returns The middleware disposer.
 */

function onActionMiddleware(subtreeRoot, listeners) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  assertIsObject(listeners, "listeners");
  return actionTrackingMiddleware(subtreeRoot, {
    filter: function filter(ctx) {
      if (ctx.parentContext) {
        // sub-action, do nothing
        return false;
      } // skip hooks


      if (isHookAction(ctx.actionName)) {
        return false;
      }

      return true;
    },
    onStart: function onStart(ctx) {
      if (listeners.onStart) {
        var actionCall = actionContextToActionCall(ctx);
        return listeners.onStart(actionCall, ctx);
      }
    },
    onFinish: function onFinish(ctx, ret) {
      if (listeners.onFinish) {
        var actionCall = actionContextToActionCall(ctx);
        return listeners.onFinish(actionCall, ctx, ret);
      }
    }
  });
}

function actionContextToActionCall(ctx) {
  var rootPath = fastGetRootPath(ctx.target);
  return {
    actionName: ctx.actionName,
    args: ctx.args,
    targetPath: rootPath.path,
    targetPathIds: rootPathToTargetPathIds(rootPath)
  };
}

/**
 * Attaches an action middleware that will throw when any action is started
 * over the node or any of the child nodes, thus effectively making the subtree
 * readonly.
 *
 * It will return an object with a `dispose` function to remove the middleware and a `allowWrite` function
 * that will allow actions to be started inside the provided code block.
 *
 * Example:
 * ```ts
 * // given a model instance named todo
 * const { dispose, allowWrite } = readonlyMiddleware(todo)
 *
 * // this will throw
 * todo.setDone(false)
 * await todo.setDoneAsync(false)
 *
 * // this will work
 * allowWrite(() => todo.setDone(false))
 * // note: for async always use one action invocation per allowWrite!
 * await allowWrite(() => todo.setDoneAsync(false))
 * ```
 *
 * @param subtreeRoot Subtree root target object.
 * @returns An object with the middleware disposer (`dispose`) and a `allowWrite` function.
 */

function readonlyMiddleware(subtreeRoot) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  var writable = false;
  var writableSymbol = Symbol("writable");
  var disposer = actionTrackingMiddleware(subtreeRoot, {
    filter: function filter(ctx) {
      // skip hooks
      if (isHookAction(ctx.actionName)) {
        return false;
      } // if we are inside allowWrite it is writable


      var currentlyWritable = writable;

      if (!currentlyWritable) {
        // if a parent context was writable then the child should be as well
        var currentCtx = ctx;

        while (currentCtx && !currentlyWritable) {
          currentlyWritable = !!currentCtx.data[writableSymbol];
          currentCtx = currentCtx.parentContext;
        }
      }

      if (currentlyWritable) {
        ctx.data[writableSymbol] = true;
        return false;
      }

      return true;
    },
    onStart: function onStart(ctx) {
      // if we get here (wasn't filtered out) it is not writable
      return {
        result: ActionTrackingResult.Throw,
        value: failure("tried to invoke action '" + ctx.actionName + "' over a readonly node")
      };
    }
  });
  return {
    dispose: disposer,
    allowWrite: function allowWrite(fn) {
      var oldWritable = writable;
      writable = true;

      try {
        return fn();
      } finally {
        writable = oldWritable;
      }
    }
  };
}

/**
 * Escapes a json pointer path.
 *
 * @param path The raw pointer
 * @return the Escaped path
 */

function escapePathComponent(path) {
  if (typeof path === "number") {
    return "" + path;
  }

  if (path.indexOf("/") === -1 && path.indexOf("~") === -1) {
    return path;
  }

  return path.replace(/~/g, "~0").replace(/\//g, "~1");
}
/**
 * Unescapes a json pointer path.
 *
 * @param path The escaped pointer
 * @return The unescaped path
 */


function unescapePathComponent(path) {
  return path.replace(/~1/g, "/").replace(/~0/g, "~");
}
/**
 * Converts a path into a JSON pointer.
 *
 * @param path Path to convert.
 * @returns Converted JSON pointer.
 */


function pathToJsonPointer(path) {
  if (path.length <= 0) {
    return "";
  }

  return "/" + path.map(escapePathComponent).join("/");
}
/**
 * Converts a JSON pointer into a path.
 *
 * @param jsonPointer JSON pointer to convert.
 * @returns Converted path.
 */

function jsonPointerToPath(jsonPointer) {
  if (jsonPointer === "") {
    return [];
  }

  if (!jsonPointer.startsWith("/")) {
    throw failure("a JSON pointer must start with '/' or be empty");
  }

  jsonPointer = jsonPointer.slice(1);
  return jsonPointer.split("/").map(unescapePathComponent);
}
/**
 * Convert a patch into a JSON patch.
 *
 * @param patch A patch.
 * @returns A JSON patch.
 */

function patchToJsonPatch(patch) {
  return _extends({}, patch, {
    path: pathToJsonPointer(patch.path)
  });
}
/**
 * Converts a JSON patch into a patch.
 *
 * @param jsonPatch A JSON patch.
 * @returns A patch.
 */

function jsonPatchToPatch(jsonPatch) {
  return _extends({}, jsonPatch, {
    path: jsonPointerToPath(jsonPatch.path)
  });
}

/**
 * Creates a patch recorder.
 *
 * @param subtreeRoot
 * @param [opts]
 * @returns The patch recorder.
 */

function patchRecorder(subtreeRoot, opts) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  return internalPatchRecorder(subtreeRoot, opts);
}
/**
 * @ignore
 * @internal
 *
 * Creates a global or local patch recorder.
 *
 * @param subtreeRoot
 * @param [opts]
 * @returns The patch recorder.
 */

function internalPatchRecorder(subtreeRoot, opts) {
  var _recording$filter$opt = _extends({
    recording: true,
    filter: alwaysAcceptFilter
  }, opts),
      recording = _recording$filter$opt.recording,
      filter = _recording$filter$opt.filter;

  var events = mobx_esm.observable.array([], {
    deep: false
  });
  var onPatchesDisposer;

  if (subtreeRoot) {
    onPatchesDisposer = onPatches(subtreeRoot, function (p, invP) {
      if (recording && filter(p, invP)) {
        events.push({
          target: subtreeRoot,
          patches: p,
          inversePatches: invP
        });
        opts == null ? void 0 : opts.onPatches == null ? void 0 : opts.onPatches(p, invP);
      }
    });
  } else {
    onPatchesDisposer = onGlobalPatches(function (target, p, invP) {
      if (recording && filter(p, invP)) {
        events.push({
          target: target,
          patches: p,
          inversePatches: invP
        });
        opts == null ? void 0 : opts.onPatches == null ? void 0 : opts.onPatches(p, invP);
      }
    });
  }

  return {
    get recording() {
      return recording;
    },

    set recording(enabled) {
      recording = enabled;
    },

    get events() {
      return events;
    },

    dispose: function dispose() {
      onPatchesDisposer();
    }
  };
}

var alwaysAcceptFilter = function alwaysAcceptFilter() {
  return true;
};

/**
 * Creates a transaction middleware, which reverts changes made by an action / child
 * actions when the root action throws an exception by applying inverse patches.
 *
 * @typeparam M Model
 * @param target Object with the root target model object (`model`) and root action name (`actionName`).
 * @returns The middleware disposer.
 */

function transactionMiddleware(target) {
  assertIsObject(target, "target");
  var model = target.model,
      actionName = target.actionName;
  assertIsModel(model, "target.model");

  if (typeof actionName !== "string") {
    throw failure("target.actionName must be a string");
  }

  var patchRecorderSymbol = Symbol("patchRecorder");

  function initPatchRecorder(ctx) {
    ctx.rootContext.data[patchRecorderSymbol] = internalPatchRecorder(undefined, {
      recording: false
    });
  }

  function getPatchRecorder(ctx) {
    return ctx.rootContext.data[patchRecorderSymbol];
  }

  return actionTrackingMiddleware(model, {
    filter: function filter(ctx) {
      // the primary action must be on the root object
      var rootContext = ctx.rootContext;
      return rootContext.target === model && rootContext.actionName === actionName;
    },
    onStart: function onStart(ctx) {
      if (ctx === ctx.rootContext) {
        initPatchRecorder(ctx);
      }
    },
    onResume: function onResume(ctx) {
      getPatchRecorder(ctx).recording = true;
    },
    onSuspend: function onSuspend(ctx) {
      getPatchRecorder(ctx).recording = false;
    },
    onFinish: function onFinish(ctx, ret) {
      if (ctx === ctx.rootContext) {
        var patchRecorder = getPatchRecorder(ctx);

        try {
          if (ret.result === ActionTrackingResult.Throw) {
            // undo changes (backwards for inverse patches)
            var events = patchRecorder.events;

            for (var i = events.length - 1; i >= 0; i--) {
              var event = events[i];
              applyPatches(event.target, event.inversePatches, true);
            }
          }
        } finally {
          patchRecorder.dispose();
        }
      }
    }
  });
}
/**
 * Transaction middleware as a decorator.
 *
 * @param target
 * @param propertyKey
 */

function transaction(target, propertyKey) {
  checkModelDecoratorArgs("transaction", target, propertyKey);
  addModelClassInitializer(target.constructor, function (modelInstance) {
    transactionMiddleware({
      model: modelInstance,
      actionName: propertyKey
    });
  });
}

/**
 * A type that represents an array of values of a given type.
 *
 * Example:
 * ```ts
 * const numberArrayType = types.array(types.number)
 * ```
 *
 * @typeparam T Item type.
 * @param itemType Type of inner items.
 * @returns
 */

function typesArray(itemType) {
  var typeInfoGen = function typeInfoGen(t) {
    return new ArrayTypeInfo(t, resolveStandardType(itemType));
  };

  return lateTypeChecker(function () {
    var itemChecker = resolveTypeChecker(itemType);

    var getTypeName = function getTypeName() {
      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      return "Array<" + itemChecker.getTypeName.apply(itemChecker, recursiveTypeCheckers.concat([itemChecker])) + ">";
    };

    var thisTc = new TypeChecker(function (array, path) {
      if (!isArray(array)) {
        return new TypeCheckError(path, getTypeName(thisTc), array);
      }

      if (!itemChecker.unchecked) {
        for (var i = 0; i < array.length; i++) {
          var itemError = itemChecker.check(array[i], [].concat(path, [i]));

          if (itemError) {
            return itemError;
          }
        }
      }

      return null;
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.array` type info.
 */

var ArrayTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(ArrayTypeInfo, _TypeInfo);

  _createClass(ArrayTypeInfo, [{
    key: "itemTypeInfo",
    get: function get() {
      return getTypeInfo(this.itemType);
    }
  }]);

  function ArrayTypeInfo(thisType, itemType) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.itemType = void 0;
    _this.itemType = itemType;
    return _this;
  }

  return ArrayTypeInfo;
}(TypeInfo);

function tProp(typeOrDefaultValue, arg1, arg2) {
  var def;
  var opts = {};
  var hasDefaultValue = false;

  switch (typeof typeOrDefaultValue) {
    case "string":
      return tProp(typesString, typeOrDefaultValue, arg1);

    case "number":
      return tProp(typesNumber, typeOrDefaultValue, arg1);

    case "boolean":
      return tProp(typesBoolean, typeOrDefaultValue, arg1);
  }

  if (arguments.length >= 3) {
    // type, default, options
    def = arg1;
    hasDefaultValue = true;
    opts = _extends({}, arg2);
  } else if (arguments.length === 2) {
    // type, default | options
    if (isObject(arg1)) {
      // options
      opts = _extends({}, arg1);
    } else {
      // default
      def = arg1;
      hasDefaultValue = true;
    }
  }

  var isDefFn = typeof def === "function";
  return {
    $propValueType: null,
    $propCreationValueType: null,
    $isOptional: null,
    $instanceValueType: null,
    $instanceCreationValueType: null,
    defaultFn: hasDefaultValue && isDefFn ? def : noDefaultValue,
    defaultValue: hasDefaultValue && !isDefFn ? def : noDefaultValue,
    typeChecker: resolveStandardType(typeOrDefaultValue),
    transform: undefined,
    options: opts
  };
}

/**
 * Store model instance for undo/redo actions.
 * Do not manipulate directly, other that creating it.
 */

var UndoStore = /*#__PURE__*/function (_Model) {
  _inheritsLoose(UndoStore, _Model);

  function UndoStore() {
    return _Model.apply(this, arguments) || this;
  }

  var _proto = UndoStore.prototype;

  /**
   * @ignore
   */
  _proto._clearUndo = function _clearUndo() {
    var _this = this;

    withoutUndo(function () {
      _this.undoEvents.length = 0;
    });
  }
  /**
   * @ignore
   */
  ;

  _proto._clearRedo = function _clearRedo() {
    var _this2 = this;

    withoutUndo(function () {
      _this2.redoEvents.length = 0;
    });
  }
  /**
   * @ignore
   */
  ;

  _proto._undo = function _undo() {
    var _this3 = this;

    withoutUndo(function () {
      var event = _this3.undoEvents.pop();

      _this3.redoEvents.push(event);
    });
  }
  /**
   * @ignore
   */
  ;

  _proto._redo = function _redo() {
    var _this4 = this;

    withoutUndo(function () {
      var event = _this4.redoEvents.pop();

      _this4.undoEvents.push(event);
    });
  }
  /**
   * @ignore
   */
  ;

  _proto._addUndo = function _addUndo(event) {
    var _this5 = this;

    withoutUndo(function () {
      _this5.undoEvents.push(event); // once an undo event is added redo queue is no longer valid


      _this5.redoEvents.length = 0;
    });
  };

  return UndoStore;
}( /*#__PURE__*/Model({
  // TODO: add proper type checking to undo store
  undoEvents: /*#__PURE__*/tProp( /*#__PURE__*/typesArray( /*#__PURE__*/typesUnchecked()), function () {
    return [];
  }),
  redoEvents: /*#__PURE__*/tProp( /*#__PURE__*/typesArray( /*#__PURE__*/typesUnchecked()), function () {
    return [];
  })
}));

__decorate([modelAction], UndoStore.prototype, "_clearUndo", null);

__decorate([modelAction], UndoStore.prototype, "_clearRedo", null);

__decorate([modelAction], UndoStore.prototype, "_undo", null);

__decorate([modelAction], UndoStore.prototype, "_redo", null);

__decorate([modelAction], UndoStore.prototype, "_addUndo", null);

UndoStore = /*#__PURE__*/__decorate([/*#__PURE__*/model("mobx-keystone/UndoStore")], UndoStore);
/**
 * Manager class returned by `undoMiddleware` that allows you to perform undo/redo actions.
 */

var UndoManager = /*#__PURE__*/function () {
  var _proto2 = UndoManager.prototype;

  /**
   * Clears the undo queue.
   */
  _proto2.clearUndo = function clearUndo() {
    this.store._clearUndo();
  }
  /**
   * The number of redo actions available.
   */
  ;

  /**
   * Clears the redo queue.
   */
  _proto2.clearRedo = function clearRedo() {
    this.store._clearRedo();
  }
  /**
   * Undoes the last action.
   * Will throw if there is no action to undo.
   */
  ;

  _proto2.undo = function undo() {
    var _this6 = this;

    if (!this.canUndo) {
      throw failure("nothing to undo");
    }

    var event = this.undoQueue[this.undoQueue.length - 1];
    withoutUndo(function () {
      applyPatches(_this6.subtreeRoot, event.inversePatches, true);
    });

    this.store._undo();
  }
  /**
   * Redoes the previous action.
   * Will throw if there is no action to redo.
   */
  ;

  _proto2.redo = function redo() {
    var _this7 = this;

    if (!this.canRedo) {
      throw failure("nothing to redo");
    }

    var event = this.redoQueue[this.redoQueue.length - 1];
    withoutUndo(function () {
      applyPatches(_this7.subtreeRoot, event.patches);
    });

    this.store._redo();
  }
  /**
   * Disposes the undo middleware.
   */
  ;

  _proto2.dispose = function dispose() {
    this.disposer();
  }
  /**
   * Creates an instance of `UndoManager`.
   * Do not use directly, use `undoMiddleware` instead.
   *
   * @param disposer
   * @param subtreeRoot
   * @param [store]
   */
  ;

  _createClass(UndoManager, [{
    key: "undoQueue",

    /**
     * The store currently being used to store undo/redo action events.
     */

    /**
     * The undo stack, where the first operation to undo will be the last of the array.
     * Do not manipulate this array directly.
     */
    get: function get() {
      return this.store.undoEvents;
    }
    /**
     * The redo stack, where the first operation to redo will be the last of the array.
     * Do not manipulate this array directly.
     */

  }, {
    key: "redoQueue",
    get: function get() {
      return this.store.redoEvents;
    }
    /**
     * The number of undo actions available.
     */

  }, {
    key: "undoLevels",
    get: function get() {
      return this.undoQueue.length;
    }
    /**
     * If undo can be performed (if there is at least one undo action available).
     */

  }, {
    key: "canUndo",
    get: function get() {
      return this.undoLevels > 0;
    }
  }, {
    key: "redoLevels",
    get: function get() {
      return this.redoQueue.length;
    }
    /**
     * If redo can be performed (if there is at least one redo action available)
     */

  }, {
    key: "canRedo",
    get: function get() {
      return this.redoLevels > 0;
    }
  }]);

  function UndoManager(disposer, subtreeRoot, store) {
    this.disposer = void 0;
    this.subtreeRoot = void 0;
    this.store = void 0;
    this.disposer = disposer;
    this.subtreeRoot = subtreeRoot;
    this.store = store != null ? store : new UndoStore({});
  }

  return UndoManager;
}();

__decorate([mobx_esm.computed], UndoManager.prototype, "undoQueue", null);

__decorate([mobx_esm.computed], UndoManager.prototype, "redoQueue", null);

__decorate([mobx_esm.computed], UndoManager.prototype, "undoLevels", null);

__decorate([mobx_esm.computed], UndoManager.prototype, "canUndo", null);

__decorate([mobx_esm.action], UndoManager.prototype, "clearUndo", null);

__decorate([mobx_esm.computed], UndoManager.prototype, "redoLevels", null);

__decorate([mobx_esm.computed], UndoManager.prototype, "canRedo", null);

__decorate([mobx_esm.action], UndoManager.prototype, "clearRedo", null);

__decorate([mobx_esm.action], UndoManager.prototype, "undo", null);

__decorate([mobx_esm.action], UndoManager.prototype, "redo", null);
/**
 * Creates an undo middleware.
 *
 * @param subtreeRoot Subtree root target object.
 * @param [store] Optional `UndoStore` where to store the undo/redo queues. Use this if you want to
 * store such queues somewhere in your models. If none is provided it will reside in memory.
 * @returns An `UndoManager` which allows you to do the manage the undo/redo operations and dispose of the middleware.
 */


function undoMiddleware(subtreeRoot, store) {
  assertTweakedObject(subtreeRoot, "subtreeRoot");
  var patchRecorderSymbol = Symbol("patchRecorder");

  function initPatchRecorder(ctx) {
    ctx.rootContext.data[patchRecorderSymbol] = {
      recorder: patchRecorder(subtreeRoot, {
        recording: false,
        filter: undoDisabledFilter
      }),
      recorderStack: 0,
      undoRootContext: ctx
    };
  }

  function getPatchRecorderData(ctx) {
    return ctx.rootContext.data[patchRecorderSymbol];
  }

  var manager;
  var middlewareDisposer = actionTrackingMiddleware(subtreeRoot, {
    onStart: function onStart(ctx) {
      if (!getPatchRecorderData(ctx)) {
        initPatchRecorder(ctx);
      }
    },
    onResume: function onResume(ctx) {
      var patchRecorderData = getPatchRecorderData(ctx);
      patchRecorderData.recorderStack++;
      patchRecorderData.recorder.recording = patchRecorderData.recorderStack > 0;
    },
    onSuspend: function onSuspend(ctx) {
      var patchRecorderData = getPatchRecorderData(ctx);
      patchRecorderData.recorderStack--;
      patchRecorderData.recorder.recording = patchRecorderData.recorderStack > 0;
    },
    onFinish: function onFinish(ctx) {
      var patchRecorderData = getPatchRecorderData(ctx);

      if (patchRecorderData && patchRecorderData.undoRootContext === ctx) {
        var _patchRecorder = patchRecorderData.recorder;

        if (_patchRecorder.events.length > 0) {
          var patches = [];
          var inversePatches = [];

          for (var _iterator = _createForOfIteratorHelperLoose(_patchRecorder.events), _step; !(_step = _iterator()).done;) {
            var event = _step.value;
            patches.push.apply(patches, event.patches);
            inversePatches.push.apply(inversePatches, event.inversePatches);
          }

          manager.store._addUndo({
            targetPath: fastGetRootPath(ctx.target).path,
            actionName: ctx.actionName,
            patches: patches,
            inversePatches: inversePatches
          });
        }

        _patchRecorder.dispose();
      }
    }
  });
  manager = new UndoManager(middlewareDisposer, subtreeRoot, store);
  return manager;
}
var undoDisabled = false;

var undoDisabledFilter = function undoDisabledFilter() {
  return !undoDisabled;
};
/**
 * Skips the undo recording mechanism for the code block that gets run synchronously inside.
 *
 * @typeparam T
 * @param fn
 * @returns
 */


function withoutUndo(fn) {
  var savedUndoDisabled = undoDisabled;
  undoDisabled = true;

  try {
    return fn();
  } finally {
    undoDisabled = savedUndoDisabled;
  }
}

function getContextValue(contextValue) {
  if (contextValue.type === "value") {
    return contextValue.value;
  } else {
    return contextValue.value.get();
  }
}

var ContextClass = /*#__PURE__*/function () {
  var _proto = ContextClass.prototype;

  _proto.getNodeAtom = function getNodeAtom(node) {
    var atomPerNode = this.nodeAtom.get(node);

    if (!atomPerNode) {
      atomPerNode = (0,mobx_esm.createAtom)("contextValue");
      this.nodeAtom.set(node, atomPerNode);
    }

    return atomPerNode;
  };

  _proto.fastGet = function fastGet(node) {
    this.getNodeAtom(node).reportObserved();
    var obsForNode = this.nodeContextValue.get(node);

    if (obsForNode) {
      return getContextValue(obsForNode);
    }

    var parent = fastGetParent(node);

    if (!parent) {
      return this.getDefault();
    }

    return this.fastGet(parent);
  };

  _proto.get = function get(node) {
    assertTweakedObject(node, "node");
    return this.fastGet(node);
  };

  _proto.fastGetProviderNode = function fastGetProviderNode(node) {
    this.getNodeAtom(node).reportObserved();
    var obsForNode = this.nodeContextValue.get(node);

    if (obsForNode) {
      return node;
    }

    var parent = fastGetParent(node);

    if (!parent) {
      return undefined;
    }

    return this.fastGetProviderNode(parent);
  };

  _proto.getProviderNode = function getProviderNode(node) {
    assertTweakedObject(node, "node");
    return this.fastGetProviderNode(node);
  };

  _proto.getDefault = function getDefault() {
    return getContextValue(this.defaultContextValue);
  };

  _proto.setDefault = function setDefault(value) {
    this.defaultContextValue = {
      type: "value",
      value: value
    };
  };

  _proto.setDefaultComputed = function setDefaultComputed(valueFn) {
    this.defaultContextValue = {
      type: "computed",
      value: (0,mobx_esm.computed)(valueFn)
    };
  };

  _proto.set = function set(node, value) {
    assertTweakedObject(node, "node");
    this.nodeContextValue.set(node, {
      type: "value",
      value: value
    });
    this.getNodeAtom(node).reportChanged();
  };

  _proto.setComputed = function setComputed(node, valueFn) {
    assertTweakedObject(node, "node");
    this.nodeContextValue.set(node, {
      type: "computed",
      value: (0,mobx_esm.computed)(valueFn)
    });
    this.getNodeAtom(node).reportChanged();
  };

  _proto.unset = function unset(node) {
    assertTweakedObject(node, "node");
    this.nodeContextValue["delete"](node);
    this.getNodeAtom(node).reportChanged();
  };

  function ContextClass(defaultValue) {
    this.defaultContextValue = void 0;
    this.nodeContextValue = new WeakMap();
    this.nodeAtom = new WeakMap();

    if (getMobxVersion() >= 6) {
      (0,mobx_esm.makeObservable)(this);
    }

    this.setDefault(defaultValue);
  }

  return ContextClass;
}();

__decorate([mobx_esm.observable.ref], ContextClass.prototype, "defaultContextValue", void 0);

__decorate([mobx_esm.action], ContextClass.prototype, "setDefault", null);

__decorate([mobx_esm.action], ContextClass.prototype, "setDefaultComputed", null);

__decorate([mobx_esm.action], ContextClass.prototype, "set", null);

__decorate([mobx_esm.action], ContextClass.prototype, "setComputed", null);

__decorate([mobx_esm.action], ContextClass.prototype, "unset", null); // base


function createContext(defaultValue) {
  return new ContextClass(defaultValue);
}

/**
 * @ignore
 * @internal
 */

function extendFnModelFlowActions(fnModelObj, namespace, flowActions) {
  for (var _i = 0, _Object$entries = Object.entries(flowActions); _i < _Object$entries.length; _i++) {
    var _Object$entries$_i = _Object$entries[_i],
        name = _Object$entries$_i[0],
        fn = _Object$entries$_i[1];
    addActionToFnModel(fnModelObj, namespace, name, fn, true);
  }

  return fnModelObj;
}

/**
 * @ignore
 * @internal
 */

function extendFnModelSetterActions(fnModelObj, namespace, setterActions) {
  var _loop = function _loop() {
    var _Object$entries$_i = _Object$entries[_i],
        name = _Object$entries$_i[0],
        fieldName = _Object$entries$_i[1];

    // make strings setters
    var fn = function fn(value) {
      this[fieldName] = value;
    };

    addActionToFnModel(fnModelObj, namespace, name, fn, false);
  };

  for (var _i = 0, _Object$entries = Object.entries(setterActions); _i < _Object$entries.length; _i++) {
    _loop();
  }

  return fnModelObj;
}

/**
 * @ignore
 * @internal
 */

function extendFnModelViews(fnModelObj, views) {
  var _loop = function _loop() {
    var _Object$entries$_i = _Object$entries[_i],
        name = _Object$entries$_i[0],
        fnOrFnWithOptions = _Object$entries$_i[1];
    assertFnModelKeyNotInUse(fnModelObj, name);
    var computedsPerObject = new WeakMap();
    var fn = void 0;
    var equals = void 0;

    if (typeof fnOrFnWithOptions === "function") {
      fn = fnOrFnWithOptions;
    } else {
      fn = fnOrFnWithOptions.get;
      equals = fnOrFnWithOptions.equals;
    }

    fnModelObj[name] = function (target) {
      var computedFn = computedsPerObject.get(target);

      if (!computedFn) {
        computedFn = (0,mobx_esm.computed)(fn, {
          name: name,
          context: target,
          equals: equals
        });
        computedsPerObject.set(target, computedFn);
      }

      return computedFn.get();
    };
  };

  for (var _i = 0, _Object$entries = Object.entries(views); _i < _Object$entries.length; _i++) {
    _loop();
  }

  return fnModelObj;
}

function fnModel(arg1, arg2) {
  var actualType = arguments.length >= 2 ? arg1 : null;
  var namespace = arguments.length >= 2 ? arg2 : arg1;
  assertIsString(namespace, "namespace");
  var fnModelObj = {
    create: actualType ? fnModelCreateWithType.bind(undefined, actualType) : fnModelCreateWithoutType,
    type: actualType
  };
  fnModelObj.views = extendFnModelViews.bind(undefined, fnModelObj);
  fnModelObj.actions = extendFnModelActions.bind(undefined, fnModelObj, namespace);
  fnModelObj.flowActions = extendFnModelFlowActions.bind(undefined, fnModelObj, namespace);
  fnModelObj.setterActions = extendFnModelSetterActions.bind(undefined, fnModelObj, namespace);
  return fnModelObj;
}

function fnModelCreateWithoutType(data) {
  return toTreeNode(data);
}

function fnModelCreateWithType(actualType, data) {
  if (isModelAutoTypeCheckingEnabled()) {
    var errors = typeCheck(actualType, data);

    if (errors) {
      errors["throw"](data);
    }
  }

  return toTreeNode(data);
}

function _splice() {
  return this.splice.apply(this, arguments);
}

var _fnArray = /*#__PURE__*/fnModel("mobx-keystone/fnArray").actions({
  set: function set$1(index, value) {
    (0,mobx_esm.set)(this, index, value);
  },
  "delete": function _delete(index) {
    return (0,mobx_esm.remove)(this, "" + index);
  },
  setLength: function setLength(length) {
    this.length = length;
  },
  concat: function concat() {
    return this.concat.apply(this, arguments);
  },
  copyWithin: function copyWithin(target, start, end) {
    return this.copyWithin(target, start, end);
  },
  fill: function fill(value, start, end) {
    return this.fill(value, start, end);
  },
  pop: function pop() {
    return this.pop();
  },
  push: function push() {
    return this.push.apply(this, arguments);
  },
  reverse: function reverse() {
    return this.reverse();
  },
  shift: function shift() {
    return this.shift();
  },
  slice: function slice(start, end) {
    return this.slice(start, end);
  },
  sort: function sort(compareFn) {
    return this.sort(compareFn);
  },
  splice: _splice,
  unshift: function unshift() {
    return this.unshift.apply(this, arguments);
  }
});

var fnArray = {
  set: _fnArray.set,
  "delete": _fnArray["delete"],
  setLength: _fnArray.setLength,
  concat: _fnArray.concat,
  copyWithin: _fnArray.copyWithin,
  fill: _fnArray.fill,
  pop: _fnArray.pop,
  push: _fnArray.push,
  reverse: _fnArray.reverse,
  shift: _fnArray.shift,
  slice: _fnArray.slice,
  sort: _fnArray.sort,
  splice: _fnArray.splice,
  unshift: _fnArray.unshift,
  create: _fnArray.create
};

var _fnObject = /*#__PURE__*/fnModel("mobx-keystone/fnObject").actions({
  set: function set$1(key, value) {
    (0,mobx_esm.set)(this, key, value);
  },
  "delete": function _delete(key) {
    return (0,mobx_esm.remove)(this, key);
  },
  call: function call(methodName) {
    for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
      args[_key - 1] = arguments[_key];
    }

    return this[methodName].apply(this, args);
  }
});

var fnObject = {
  set: _fnObject.set,
  "delete": _fnObject["delete"],
  call: _fnObject.call,
  create: _fnObject.create
};

var Lock = /*#__PURE__*/function () {
  function Lock() {
    this._locked = true;
  }

  var _proto = Lock.prototype;

  _proto.unlockedFn = function unlockedFn(fn) {
    var _this = this;

    var innerFn = function innerFn() {
      var oldLocked = _this._locked;
      _this._locked = false;

      try {
        return fn.apply(void 0, arguments);
      } finally {
        _this._locked = oldLocked;
      }
    };

    return innerFn;
  };

  _proto.withUnlock = function withUnlock(fn) {
    var oldLocked = this._locked;
    this._locked = false;

    try {
      return fn();
    } finally {
      this._locked = oldLocked;
    }
  };

  _createClass(Lock, [{
    key: "isLocked",
    get: function get() {
      return this._locked;
    }
  }]);

  return Lock;
}();

/**
 * Creates a tag data accessor for a target object of a certain type.
 * Tag data will be lazy created on access and reused for the same target object.
 *
 * @typeparam Target Target type.
 * @typeparam TagData Tag data type.
 * @param tagDataConstructor Function that will be called the first time the tag
 * for a given object is requested.
 * @returns The tag data associated with the target object.
 */
function tag(tagDataConstructor) {
  var map = new WeakMap();
  return {
    "for": function _for(target) {
      if (!map.has(target)) {
        var data = tagDataConstructor(target);
        map.set(target, data);
        return data;
      } else {
        return map.get(target);
      }
    }
  };
}

var observableMapBackedByObservableObject = /*#__PURE__*/(0,mobx_esm.action)(function (obj) {
  if (inDevMode()) {
    if (!(0,mobx_esm.isObservableObject)(obj)) {
      throw failure("assertion failed: expected an observable object");
    }
  }

  var map = mobx_esm.observable.map();
  map.dataObject = obj;
  var keys = Object.keys(obj);

  for (var i = 0; i < keys.length; i++) {
    var k = keys[i];
    map.set(k, obj[k]);
  }

  var mutationLock = new Lock(); // when the object changes the map changes

  (0,mobx_esm.observe)(obj, (0,mobx_esm.action)(mutationLock.unlockedFn(function (change) {
    switch (change.type) {
      case "add":
      case "update":
        {
          map.set(change.name, change.newValue);
          break;
        }

      case "remove":
        {
          map["delete"](change.name);
          break;
        }
    }
  }))); // when the map changes also change the object

  (0,mobx_esm.intercept)(map, (0,mobx_esm.action)(function (change) {
    if (!mutationLock.isLocked) {
      return null; // already changed
    }

    switch (change.type) {
      case "add":
      case "update":
        {
          (0,mobx_esm.set)(obj, change.name, change.newValue);
          break;
        }

      case "delete":
        {
          (0,mobx_esm.remove)(obj, change.name);
          break;
        }
    }

    return change;
  }));
  return map;
});
var observableMapBackedByObservableArray = /*#__PURE__*/(0,mobx_esm.action)(function (array) {
  if (inDevMode()) {
    if (!(0,mobx_esm.isObservableArray)(array)) {
      throw failure("assertion failed: expected an observable array");
    }
  }

  var map = mobx_esm.observable.map(array);
  map.dataObject = array;

  if (map.size !== array.length) {
    throw failure("arrays backing a map cannot contain duplicate keys");
  }

  var mutationLock = new Lock(); // for speed reasons we will just assume distinct values are only once in the array
  // also we assume tuples themselves are immutable
  // when the array changes the map changes

  (0,mobx_esm.observe)(array, (0,mobx_esm.action)(mutationLock.unlockedFn(function (change
  /*IArrayDidChange<[string, T]>*/
  ) {
    switch (change.type) {
      case "splice":
        {
          {
            var removed = change.removed;

            for (var i = 0; i < removed.length; i++) {
              map["delete"](removed[i][0]);
            }
          }
          {
            var added = change.added;

            for (var _i = 0; _i < added.length; _i++) {
              map.set(added[_i][0], added[_i][1]);
            }
          }
          break;
        }

      case "update":
        {
          map["delete"](change.oldValue[0]);
          map.set(change.newValue[0], change.newValue[1]);
          break;
        }
    }
  }))); // when the map changes also change the array

  (0,mobx_esm.intercept)(map, (0,mobx_esm.action)(function (change) {
    if (!mutationLock.isLocked) {
      return null; // already changed
    }

    switch (change.type) {
      case "update":
        {
          // replace the whole tuple to keep tuple immutability
          var i = array.findIndex(function (i) {
            return i[0] === change.name;
          });
          array[i] = [change.name, change.newValue];
          break;
        }

      case "add":
        {
          array.push([change.name, change.newValue]);
          break;
        }

      case "delete":
        {
          var _i2 = array.findIndex(function (i) {
            return i[0] === change.name;
          });

          if (_i2 >= 0) {
            array.splice(_i2, 1);
          }

          break;
        }
    }

    return change;
  }));
  return map;
});
var asMapTag = /*#__PURE__*/tag(function (objOrArray) {
  if (isArray(objOrArray)) {
    assertIsObservableArray(objOrArray, "objOrArray");
    return observableMapBackedByObservableArray(objOrArray);
  } else {
    assertIsObservableObject(objOrArray, "objOrArray");
    return observableMapBackedByObservableObject(objOrArray);
  }
});
/**
 * Wraps an observable object or a tuple array to offer a map like interface.
 *
 * @param objOrArray Object or array.
 */

function asMap(objOrArray) {
  return asMapTag["for"](objOrArray);
}
/**
 * Converts a map to an object. If the map is a collection wrapper it will return the backed object.
 *
 * @param map
 */

function mapToObject(map) {
  assertIsMap(map, "map");
  var dataObject = map.dataObject;

  if (dataObject && !isArray(dataObject)) {
    return dataObject;
  }

  var obj = {};

  for (var _iterator = _createForOfIteratorHelperLoose(map.keys()), _step; !(_step = _iterator()).done;) {
    var k = _step.value;
    obj[k] = map.get(k);
  }

  return obj;
}
/**
 * Converts a map to an array. If the map is a collection wrapper it will return the backed array.
 *
 * @param map
 */

function mapToArray(map) {
  assertIsMap(map, "map");
  var dataObject = map.dataObject;

  if (dataObject && isArray(dataObject)) {
    return dataObject;
  }

  var arr = [];

  for (var _iterator2 = _createForOfIteratorHelperLoose(map.keys()), _step2; !(_step2 = _iterator2()).done;) {
    var k = _step2.value;
    arr.push([k, map.get(k)]);
  }

  return arr;
}

var arrayAsMapInnerTransform = {
  propToData: function propToData(arr) {
    return isArray(arr) ? asMap(arr) : arr;
  },
  dataToProp: function dataToProp(newMap) {
    if (!isMap(newMap)) {
      return newMap;
    }

    return mapToArray(newMap);
  }
};
function prop_mapArray(def) {
  return transformedProp(prop(def), arrayAsMapInnerTransform, true);
}
function tProp_mapArray(typeOrDefaultValue, def) {
  return transformedProp(tProp(typeOrDefaultValue, def), arrayAsMapInnerTransform, true);
}

var observableSetBackedByObservableArray = /*#__PURE__*/(0,mobx_esm.action)(function (array) {
  if (inDevMode()) {
    if (!(0,mobx_esm.isObservableArray)(array)) {
      throw failure("assertion failed: expected an observable array");
    }
  }

  var set = mobx_esm.observable.set(array);
  set.dataObject = array;

  if (set.size !== array.length) {
    throw failure("arrays backing a set cannot contain duplicate values");
  }

  var mutationLock = new Lock(); // for speed reasons we will just assume distinct values are only once in the array
  // when the array changes the set changes

  (0,mobx_esm.observe)(array, (0,mobx_esm.action)(mutationLock.unlockedFn(function (change
  /*IArrayDidChange<T>*/
  ) {
    switch (change.type) {
      case "splice":
        {
          {
            var removed = change.removed;

            for (var i = 0; i < removed.length; i++) {
              set["delete"](removed[i]);
            }
          }
          {
            var added = change.added;

            for (var _i = 0; _i < added.length; _i++) {
              set.add(added[_i]);
            }
          }
          break;
        }

      case "update":
        {
          set["delete"](change.oldValue);
          set.add(change.newValue);
          break;
        }
    }
  }))); // when the set changes also change the array

  (0,mobx_esm.intercept)(set, (0,mobx_esm.action)(function (change) {
    if (!mutationLock.isLocked) {
      return null; // already changed
    }

    switch (change.type) {
      case "add":
        {
          array.push(change.newValue);
          break;
        }

      case "delete":
        {
          var i = array.indexOf(change.oldValue);

          if (i >= 0) {
            array.splice(i, 1);
          }

          break;
        }
    }

    return change;
  }));
  return set;
});
var asSetTag = /*#__PURE__*/tag(function (array) {
  assertIsObservableArray(array, "array");
  return observableSetBackedByObservableArray(array);
});
/**
 * Wraps an observable array to offer a set like interface.
 *
 * @param array
 */

function asSet(array) {
  return asSetTag["for"](array);
}
/**
 * Converts a set to an array. If the set is a collection wrapper it will return the backed array.
 *
 * @param set
 */

function setToArray(set) {
  assertIsSet(set, "set");
  var dataObject = set.dataObject;

  if (dataObject) {
    return dataObject;
  }

  return Array.from(set.values());
}

var arrayAsSetInnerTransform = {
  propToData: function propToData(arr) {
    return isArray(arr) ? asSet(arr) : arr;
  },
  dataToProp: function dataToProp(newSet) {
    return isSet(newSet) ? setToArray(newSet) : newSet;
  }
};
function prop_setArray(def) {
  return transformedProp(prop(def), arrayAsSetInnerTransform, true);
}
function tProp_setArray(typeOrDefaultValue, def) {
  return transformedProp(tProp(typeOrDefaultValue, def), arrayAsSetInnerTransform, true);
}

var objectAsMapInnerTransform = {
  propToData: function propToData(obj) {
    return isObject(obj) ? asMap(obj) : obj;
  },
  dataToProp: function dataToProp(newMap) {
    if (!isMap(newMap)) {
      return newMap;
    }

    return mapToObject(newMap);
  }
};
function prop_mapObject(def) {
  return transformedProp(prop(def), objectAsMapInnerTransform, true);
}
function tProp_mapObject(typeOrDefaultValue, def) {
  return transformedProp(tProp(typeOrDefaultValue, def), objectAsMapInnerTransform, true);
}

/**
 * @ignore
 * @internal
 */

function immutableDate(value) {
  return new InternalImmutableDate(value);
}
var errMessage = "this Date object is immutable";

var InternalImmutableDate = /*#__PURE__*/function (_Date) {
  _inheritsLoose(InternalImmutableDate, _Date);

  function InternalImmutableDate() {
    return _Date.apply(this, arguments) || this;
  }

  var _proto = InternalImmutableDate.prototype;

  // disable mutable methods
  _proto.setTime = function setTime() {
    throw failure(errMessage);
  };

  _proto.setMilliseconds = function setMilliseconds() {
    throw failure(errMessage);
  };

  _proto.setUTCMilliseconds = function setUTCMilliseconds() {
    throw failure(errMessage);
  };

  _proto.setSeconds = function setSeconds() {
    throw failure(errMessage);
  };

  _proto.setUTCSeconds = function setUTCSeconds() {
    throw failure(errMessage);
  };

  _proto.setMinutes = function setMinutes() {
    throw failure(errMessage);
  };

  _proto.setUTCMinutes = function setUTCMinutes() {
    throw failure(errMessage);
  };

  _proto.setHours = function setHours() {
    throw failure(errMessage);
  };

  _proto.setUTCHours = function setUTCHours() {
    throw failure(errMessage);
  };

  _proto.setDate = function setDate() {
    throw failure(errMessage);
  };

  _proto.setUTCDate = function setUTCDate() {
    throw failure(errMessage);
  };

  _proto.setMonth = function setMonth() {
    throw failure(errMessage);
  };

  _proto.setUTCMonth = function setUTCMonth() {
    throw failure(errMessage);
  };

  _proto.setFullYear = function setFullYear() {
    throw failure(errMessage);
  };

  _proto.setUTCFullYear = function setUTCFullYear() {
    throw failure(errMessage);
  };

  return InternalImmutableDate;
}( /*#__PURE__*/_wrapNativeSuper(Date));

/**
 * Property transform for ISO date strings to Date objects and vice-versa.
 * If a model property, consider using `prop_dateString` or `tProp_dateString` instead.
 */

var stringAsDate = /*#__PURE__*/propTransform({
  propToData: function propToData(prop) {
    if (typeof prop !== "string") {
      return prop;
    }

    return immutableDate(prop);
  },
  dataToProp: function dataToProp(date) {
    return date instanceof Date ? date.toJSON() : date;
  }
});
function prop_dateString(def) {
  return transformedProp(prop(def), stringAsDate, true);
}
function tProp_dateString(typeOrDefaultValue, def) {
  return transformedProp(tProp(typeOrDefaultValue, def), stringAsDate, true);
}

/**
 * Property transform for number timestamps to Date objects and vice-versa.
 * If a model property, consider using `prop_dateTimestamp` or `tProp_dateTimestamp` instead.
 */

var timestampAsDate = /*#__PURE__*/propTransform({
  propToData: function propToData(prop) {
    if (typeof prop !== "number") {
      return prop;
    }

    return immutableDate(prop);
  },
  dataToProp: function dataToProp(date) {
    return date instanceof Date ? date.getTime() : date;
  }
});
function prop_dateTimestamp(def) {
  return transformedProp(prop(def), timestampAsDate, true);
}
function tProp_dateTimestamp(typeOrDefaultValue, def) {
  return transformedProp(tProp(typeOrDefaultValue, def), timestampAsDate, true);
}

/**
 * Connects a tree node to a redux dev tools instance.
 *
 * @param remotedevPackage The remotedev package (usually the result of `require("remoteDev")`) (https://www.npmjs.com/package/remotedev).
 * @param remotedevConnection The result of a connect method from the remotedev package (usually the result of `remoteDev.connectViaExtension(...)`).
 * @param target Object to use as root.
 * @param storeName Name to be shown in the redux dev tools.
 * @param [options] Optional options object. `logArgsNearName` if it should show the arguments near the action name (default is `true`).
 */

function connectReduxDevTools(remotedevPackage, remotedevConnection, target, options) {
  assertTweakedObject(target, "target");

  var opts = _extends({
    logArgsNearName: true
  }, options);

  var handlingMonitorAction = 0; // subscribe to change state (if need more than just logging)

  remotedevConnection.subscribe(function (message) {
    if (message.type === "DISPATCH") {
      handleMonitorActions(remotedevConnection, target, message);
    }
  });
  var initialState = getSnapshot(target);
  remotedevConnection.init(initialState);
  var currentActionId = 0;
  var actionIdSymbol = Symbol("actionId");
  actionTrackingMiddleware(target, {
    onStart: function onStart(ctx) {
      ctx.data[actionIdSymbol] = currentActionId++;
    },
    onResume: function onResume(ctx) {
      // give a chance to the parent to log its own changes before the child starts
      if (ctx.parentContext) {
        log(ctx.parentContext, undefined);
      }

      log(ctx, undefined);
    },
    onSuspend: function onSuspend(ctx) {
      log(ctx, undefined);
    },
    onFinish: function onFinish(ctx, ret) {
      log(ctx, ret.result);
    }
  });

  function handleMonitorActions(remotedev2, target2, message) {
    try {
      handlingMonitorAction++;

      switch (message.payload.type) {
        case "RESET":
          applySnapshot(target2, initialState);
          return remotedev2.init(initialState);

        case "COMMIT":
          return remotedev2.init(getSnapshot(target2));

        case "ROLLBACK":
          return remotedev2.init(remotedevPackage.extractState(message));

        case "JUMP_TO_STATE":
        case "JUMP_TO_ACTION":
          applySnapshot(target2, remotedevPackage.extractState(message));
          return;

        case "IMPORT_STATE":
          var nextLiftedState = message.payload.nextLiftedState;
          var computedStates = nextLiftedState.computedStates;
          applySnapshot(target2, computedStates[computedStates.length - 1].state);
          remotedev2.send(null, nextLiftedState);
          return;

        default:
      }
    } finally {
      handlingMonitorAction--;
    }
  }

  var lastLoggedSnapshot = initialState;

  function log(ctx, result) {
    if (handlingMonitorAction) {
      return;
    }

    var sn = getSnapshot(target); // ignore actions that don't change anything (unless it is a throw)

    if (sn === lastLoggedSnapshot && result !== ActionTrackingResult.Throw) {
      return;
    }

    lastLoggedSnapshot = sn;
    var rootPath = fastGetRootPath(ctx.target);
    var name = getActionContextNameAndTypePath(ctx, rootPath, result);
    var copy = {
      type: name,
      path: rootPath.path,
      args: ctx.args
    };
    remotedevConnection.send(copy, sn);
  }

  function getActionContextNameAndTypePath(ctx, rootPath, result) {
    var pathStr = "[/" + rootPath.path.join("/") + "] ";
    var name = pathStr + ctx.actionName;

    if (opts.logArgsNearName) {
      var args = ctx.args.map(function (a) {
        try {
          return JSON.stringify(a);
        } catch (_unused) {
          return "**unserializable**";
        }
      }).join(", ");

      if (args.length > 64) {
        args = args.slice(0, 64) + "...";
      }

      name += "(" + args + ")";
    }

    var actionId = ctx.data[actionIdSymbol];
    name += " (id " + (actionId !== undefined ? actionId : "?");

    if (ctx.type === ActionContextActionType.Async) {
      name += ", async";
    }

    name += ")";

    if (result === ActionTrackingResult.Throw) {
      name += " -error thrown-";
    }

    if (ctx.parentContext) {
      var parentName = getActionContextNameAndTypePath(ctx.parentContext, fastGetRootPath(ctx.parentContext.target), undefined);

      if (parentName) {
        name = parentName + " >>> " + name;
      }
    }

    return name;
  }
}

/**
 * Adds a reaction that will trigger every time an snapshot changes.
 *
 * @typeparam T Object type.
 * @param node Object to get the snapshot from.
 * @param listener Function that will be triggered when the snapshot changes.
 * @returns A disposer.
 */

function onSnapshot(node, listener) {
  assertTweakedObject(node, "node");
  var currentSnapshot = getSnapshot(node);
  return (0,mobx_esm.reaction)(function () {
    return getSnapshot(node);
  }, function (newSnapshot) {
    var prevSn = currentSnapshot;
    currentSnapshot = newSnapshot;
    listener(newSnapshot, prevSn);
  });
}

var reduxActionType = "applyAction";
/**
 * Transforms an action call into a redux action.
 *
 * @param actionCall Action call.
 * @returns A redux action.
 */

function actionCallToReduxAction(actionCall) {
  return {
    type: reduxActionType,
    payload: actionCall
  };
}
/**
 * Generates a redux compatible store out of a mobx-keystone object.
 *
 * @template T Object type.
 * @param target Root object.
 * @param middlewares Optional list of redux middlewares.
 * @returns A redux compatible store.
 */

function asReduxStore(target) {
  assertTweakedObject(target, "target");

  var defaultDispatch = function defaultDispatch(action) {
    if (action.type !== reduxActionType) {
      throw failure("action type was expected to be '" + reduxActionType + "', but it was '" + action.type + "'");
    }

    applyAction(target, action.payload);
    return action;
  };

  var store = {
    getState: function getState() {
      return getSnapshot(target);
    },
    dispatch: function dispatch(action) {
      return runMiddlewares(action, runners, defaultDispatch);
    },
    subscribe: function subscribe(listener) {
      return onSnapshot(target, listener);
    }
  };

  for (var _len = arguments.length, middlewares = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
    middlewares[_key - 1] = arguments[_key];
  }

  var runners = middlewares.map(function (mw) {
    return mw(store);
  });
  return store;
}

function runMiddlewares(initialAction, runners, next) {
  var i = 0;

  function runNextMiddleware(action) {
    var runner = runners[i];
    i++;

    if (runner) {
      return runner(runNextMiddleware)(action);
    } else {
      return next(action);
    }
  }

  return runNextMiddleware(initialAction);
}

/**
 * A reference model base type.
 * Use `customRef` to create a custom ref constructor.
 */

var Ref = /*#__PURE__*/function (_Model) {
  _inheritsLoose(Ref, _Model);

  function Ref() {
    return _Model.apply(this, arguments) || this;
  }

  _createClass(Ref, [{
    key: "maybeCurrent",

    /**
     * The object this reference points to, or `undefined` if the reference is currently invalid.
     */
    get: function get() {
      return this.resolve();
    }
    /**
     * If the reference is currently valid.
     */

  }, {
    key: "isValid",
    get: function get() {
      return !!this.maybeCurrent;
    }
    /**
     * The object this reference points to, or throws if invalid.
     */

  }, {
    key: "current",
    get: function get() {
      var current = this.maybeCurrent;

      if (!current) {
        throw failure("a reference of type '" + this[modelTypeKey] + "' could not resolve an object with id '" + this.id + "'");
      }

      return current;
    }
  }]);

  return Ref;
}( /*#__PURE__*/Model({
  /**
   * Reference id.
   */
  id: /*#__PURE__*/tProp(typesString)
}));

__decorate([mobx_esm.computed], Ref.prototype, "maybeCurrent", null);

__decorate([mobx_esm.computed], Ref.prototype, "isValid", null);

__decorate([mobx_esm.computed], Ref.prototype, "current", null);
/**
 * Checks if a ref object is of a given ref type.
 *
 * @typeparam T Referenced object type.
 * @param ref Reference object.
 * @param refType Reference type.
 * @returns `true` if it is of the given type, false otherwise.
 */


function isRefOfType(ref, refType) {
  return ref instanceof refType.refClass;
}

/**
 * Back-references from object to the references that point to it.
 */

var objectBackRefs = /*#__PURE__*/new WeakMap();
/**
 * @ignore
 * @internal
 */

function internalCustomRef(modelTypeId, resolverGen, getId, onResolvedValueChange) {
  var _temp;

  var CustomRef = (_temp = /*#__PURE__*/function (_Ref) {
    _inheritsLoose(CustomRef, _Ref);

    function CustomRef() {
      var _this;

      for (var _len = arguments.length, args = new Array(_len), _key = 0; _key < _len; _key++) {
        args[_key] = arguments[_key];
      }

      _this = _Ref.call.apply(_Ref, [this].concat(args)) || this;
      _this.resolver = void 0;
      return _this;
    }

    var _proto = CustomRef.prototype;

    _proto.resolve = function resolve() {
      if (!this.resolver) {
        this.resolver = resolverGen(this);
      }

      return this.resolver(this);
    };

    _proto.onInit = function onInit() {
      var _this2 = this;

      // listen to changes
      var savedOldTarget;
      var savedFirstTime = true; // according to mwestrate this won't leak as long as we don't keep the disposer around

      (0,mobx_esm.reaction)(function () {
        return _this2.maybeCurrent;
      }, function (newTarget) {
        var oldTarget = savedOldTarget;
        var firstTime = savedFirstTime; // update early in case of thrown exceptions

        savedOldTarget = newTarget;
        savedFirstTime = false;
        updateBackRefs(_this2, fn, newTarget, oldTarget);

        if (!firstTime && onResolvedValueChange && newTarget !== oldTarget) {
          onResolvedValueChange(_this2, newTarget, oldTarget);
        }
      }, {
        fireImmediately: true
      });
    };

    return CustomRef;
  }(Ref), _temp);
  CustomRef = __decorate([model(modelTypeId)], CustomRef);

  var fn = function fn(target) {
    var id;

    if (typeof target === "string") {
      id = target;
    } else {
      assertIsObject(target, "target");
      id = getId(target);
    }

    if (typeof id !== "string") {
      throw failure("ref target object must have an id of string type");
    }

    var ref = new CustomRef({
      id: id
    });
    return ref;
  };

  fn.refClass = CustomRef;
  return fn;
}
/**
 * Uses a model `getRefId()` method whenever possible to get a reference ID.
 * If the model does not have an implementation of that method it returns `undefined`.
 * If the model has an implementation, but that implementation returns anything other than
 * a `string` it will throw.
 *
 * @param target Target object to get the ID from.
 * @returns The string ID or `undefined`.
 */

function getModelRefId(target) {
  if (isModel(target) && target.getRefId) {
    var id = target.getRefId();

    if (typeof id !== "string") {
      throw failure("'getRefId()' must return a string when present");
    }

    return id;
  }

  return undefined;
}

function getBackRefs(target, refType) {
  var backRefs = objectBackRefs.get(target);

  if (!backRefs) {
    backRefs = {
      all: mobx_esm.observable.set(undefined, {
        deep: false
      }),
      byType: new WeakMap()
    };
    objectBackRefs.set(target, backRefs);
  }

  if (!refType) {
    return backRefs.all;
  } else {
    var byType = backRefs.byType.get(refType);

    if (!byType) {
      byType = mobx_esm.observable.set(undefined, {
        deep: false
      });
      backRefs.byType.set(refType, byType);
    }

    return byType;
  }
}
/**
 * Gets all references that resolve to a given object.
 *
 * @typeparam T Referenced object type.
 * @param target Node the references point to.
 * @param [refType] Pass it to filter by only references of a given type, or do not to get references of any type.
 * @returns An observable set with all reference objects that point to the given object.
 */


function getRefsResolvingTo(target, refType) {
  assertTweakedObject(target, "target");
  var refTypeObject = refType;
  return getBackRefs(target, refTypeObject);
}

function updateBackRefs(ref, refClass, newTarget, oldTarget) {
  if (newTarget === oldTarget) {
    return;
  }

  if (oldTarget) {
    getBackRefs(oldTarget)["delete"](ref);
    getBackRefs(oldTarget, refClass)["delete"](ref);
  }

  if (newTarget) {
    getBackRefs(newTarget).add(ref);
    getBackRefs(newTarget, refClass).add(ref);
  }
}

/**
 * Creates a custom ref to an object, which in its snapshot form has an id.
 *
 * @typeparam T Target object type.
 * @param modelTypeId Unique model type id.
 * @param options Custom reference options.
 * @returns A function that allows you to construct that type of custom reference.
 */

var customRef = /*#__PURE__*/(0,mobx_esm.action)("customRef", function (modelTypeId, options) {
  var _options$getId;

  var getId = (_options$getId = options.getId) != null ? _options$getId : getModelRefId;
  return internalCustomRef(modelTypeId, function () {
    return options.resolve;
  }, getId, options.onResolvedValueChange);
});

var computedIdTrees = /*#__PURE__*/new WeakMap();
/**
 * Creates a root ref to an object, which in its snapshot form has an id.
 * A root ref will only be able to resolve references as long as both the Ref
 * and the referenced object share a common root.
 *
 * @typeparam T Target object type.
 * @param modelTypeId Unique model type id.
 * @param [options] Root reference options.
 * @returns A function that allows you to construct that type of root reference.
 */

var rootRef = /*#__PURE__*/(0,mobx_esm.action)("rootRef", function (modelTypeId, options) {
  var _options$getId;

  var getId = (_options$getId = options == null ? void 0 : options.getId) != null ? _options$getId : getModelRefId;
  var onResolvedValueChange = options == null ? void 0 : options.onResolvedValueChange; // cache/reuse computedIdTrees for same getId function

  var computedIdTree = computedIdTrees.get(getId);

  if (!computedIdTree) {
    computedIdTree = computedWalkTreeAggregate(getId);
    computedIdTrees.set(getId, computedIdTree);
  }

  var resolverGen = function resolverGen(ref) {
    var cachedTarget;
    return function () {
      var refRoot = fastGetRoot(ref);

      if (isRefRootCachedTargetOk(ref, refRoot, cachedTarget, getId)) {
        return cachedTarget;
      } // when not found, everytime a child is added/removed or its id changes we will perform another search
      // this search is only done once for every distinct getId function


      var idMap = computedIdTree.walk(refRoot);
      var newTarget = idMap ? idMap.get(ref.id) : undefined;

      if (newTarget) {
        cachedTarget = newTarget;
      }

      return newTarget;
    };
  };

  return internalCustomRef(modelTypeId, resolverGen, getId, onResolvedValueChange);
});

function isRefRootCachedTargetOk(ref, refRoot, cachedTarget, getId) {
  if (!cachedTarget) return false;
  if (ref.id !== getId(cachedTarget)) return false;
  if (refRoot !== fastGetRoot(cachedTarget)) return false;
  return true;
}

/**
 * Clones an object by doing a `fromSnapshot(getSnapshot(value), { generateNewIds: true })`.
 *
 * @typeparam T Object type.
 * @param node Object to clone.
 * @param [options] Options.
 * @returns The cloned object.
 */

function clone(node, options) {
  assertTweakedObject(node, "node");

  var opts = _extends({
    generateNewIds: true
  }, options);

  var sn = getSnapshot(node);
  return fromSnapshot(sn, opts);
}

/**
 * Deeply compares two values.
 *
 * Supported values are:
 * - Primitives
 * - Boxed observables
 * - Objects, observable objects
 * - Arrays, observable arrays
 * - Typed arrays
 * - Maps, observable maps
 * - Sets, observable sets
 * - Tree nodes (optimized by using snapshot comparison internally)
 *
 * Note that in the case of models the result will be false if their model IDs are different.
 *
 * @param a First value to compare.
 * @param b Second value to compare.
 * @returns `true` if they are the equivalent, `false` otherwise.
 */

function deepEquals(a, b) {
  // quick check for reference
  if (a === b) {
    return true;
  } // use snapshots to compare if possible
  // since snapshots use structural sharing it is more likely
  // to speed up comparisons


  if (isTreeNode(a)) {
    a = getSnapshot(a);
  } else if ((0,mobx_esm.isObservable)(a)) {
    a = (0,mobx_esm.toJS)(a, toJSOptions);
  }

  if (isTreeNode(b)) {
    b = getSnapshot(b);
  } else if ((0,mobx_esm.isObservable)(b)) {
    b = (0,mobx_esm.toJS)(b, toJSOptions);
  }

  return es6_default()(a, b);
}
var toJSOptions = /*#__PURE__*/getMobxVersion() >= 6 ? undefined : {
  exportMapsAsObjects: false,
  recurseEverything: false
};

/**
 * A class with the implementationm of draft.
 * Use `draft` to create an instance of this class.
 *
 * @typeparam T Data type.
 */

var Draft = /*#__PURE__*/function () {
  var _proto = Draft.prototype;

  /**
   * Draft data object.
   */

  /**
   * Commits current draft changes to the original object.
   */
  _proto.commit = function commit() {
    applySnapshot(this.originalData, getSnapshot(this.data));
  }
  /**
   * Partially commits current draft changes to the original object.
   * If the path cannot be resolved in either the draft or the original object it will throw.
   * Note that model IDs are checked to be the same when resolving the paths.
   *
   * @param path Path to commit.
   */
  ;

  _proto.commitByPath = function commitByPath(path) {
    var draftTarget = resolvePath(this.data, path);

    if (!draftTarget.resolved) {
      throw failure("path " + JSON.stringify(path) + " could not be resolved in draft object");
    }

    var draftPathIds = pathToTargetPathIdsIgnoringLast(this.data, path);
    var originalTarget = resolvePathCheckingIds(this.originalData, path, draftPathIds);

    if (!originalTarget.resolved) {
      throw failure("path " + JSON.stringify(path) + " could not be resolved in original object");
    }

    applyPatches(this.originalData, [{
      path: path,
      op: "replace",
      value: getSnapshot(draftTarget.value)
    }]);
  }
  /**
   * Resets the draft to be an exact copy of the current state of the original object.
   */
  ;

  _proto.reset = function reset() {
    applySnapshot(this.data, this.originalSnapshot);
  }
  /**
   * Partially resets current draft changes to be the same as the original object.
   * If the path cannot be resolved in either the draft or the original object it will throw.
   * Note that model IDs are checked to be the same when resolving the paths.
   *
   * @param path Path to reset.
   */
  ;

  _proto.resetByPath = function resetByPath(path) {
    var originalTarget = resolvePath(this.originalData, path);

    if (!originalTarget.resolved) {
      throw failure("path " + JSON.stringify(path) + " could not be resolved in original object");
    }

    var originalPathIds = pathToTargetPathIdsIgnoringLast(this.originalData, path);
    var draftTarget = resolvePathCheckingIds(this.data, path, originalPathIds);

    if (!draftTarget.resolved) {
      throw failure("path " + JSON.stringify(path) + " could not be resolved in draft object");
    }

    applyPatches(this.data, [{
      path: path,
      op: "replace",
      value: getSnapshot(originalTarget.value)
    }]);
  }
  /**
   * Returns `true` if the draft has changed compared to the original object, `false` otherwise.
   */
  ;

  /**
   * Returns `true` if the value at the given path of the draft has changed compared to the original object.
   * If the path cannot be resolved in the draft it will throw.
   * If the path cannot be resolved in the original object it will return `true`.
   * Note that model IDs are checked to be the same when resolving the paths.
   *
   * @param path Path to check.
   */
  _proto.isDirtyByPath = function isDirtyByPath(path) {
    var draftTarget = resolvePath(this.data, path);

    if (!draftTarget.resolved) {
      throw failure("path " + JSON.stringify(path) + " could not be resolved in draft object");
    }

    var draftPathIds = pathToTargetPathIdsIgnoringLast(this.data, path);
    var originalTarget = resolvePathCheckingIds(this.originalData, path, draftPathIds);

    if (!originalTarget.resolved) {
      return true;
    }

    return !deepEquals(draftTarget.value, originalTarget.value);
  }
  /**
   * Original data object.
   */
  ;

  _createClass(Draft, [{
    key: "isDirty",
    get: function get() {
      return !deepEquals(getSnapshot(this.data), this.originalSnapshot);
    }
  }, {
    key: "originalSnapshot",
    get: function get() {
      return getSnapshot(this.originalData);
    }
    /**
     * Creates an instance of Draft.
     * Do not use directly, use `draft` instead.
     *
     * @param original
     */

  }]);

  function Draft(original) {
    this.data = void 0;
    this.originalData = void 0;
    assertTweakedObject(original, "original");
    this.originalData = original;
    this.data = fromSnapshot(this.originalSnapshot, {
      generateNewIds: false
    });
  }

  return Draft;
}();

__decorate([mobx_esm.action], Draft.prototype, "commit", null);

__decorate([mobx_esm.action], Draft.prototype, "commitByPath", null);

__decorate([mobx_esm.action], Draft.prototype, "reset", null);

__decorate([mobx_esm.action], Draft.prototype, "resetByPath", null);

__decorate([mobx_esm.computed], Draft.prototype, "isDirty", null);

__decorate([mobx_esm.computed], Draft.prototype, "originalSnapshot", null);
/**
 * Creates a draft copy of a tree node and all its children.
 *
 * @typeparam T Data type.
 * @param original Original node.
 * @returns The draft object.
 */


function draft(original) {
  return new Draft(original);
}

function pathToTargetPathIdsIgnoringLast(root, path) {
  var pathIds = pathToTargetPathIds(root, path);

  if (pathIds.length >= 1) {
    // never check the last object id
    pathIds[pathIds.length - 1] = skipIdChecking;
  }

  return pathIds;
}

/**
 * Context that allows access to the sandbox manager this node runs under (if any).
 */

var sandboxManagerContext = /*#__PURE__*/createContext();
/**
 * Returns the sandbox manager of a node, or `undefined` if none.
 *
 * @param node Node to check.
 * @returns The sandbox manager of a node, or `undefined` if none.
 */

function getNodeSandboxManager(node) {
  return sandboxManagerContext.get(node);
}
/**
 * Returns if a given node is a sandboxed node.
 *
 * @param node Node to check.
 * @returns `true` if it is sandboxed, `false`
 */

function isSandboxedNode(node) {
  return !!getNodeSandboxManager(node);
}
/**
 * Manager class returned by `sandbox` that allows you to make changes to a sandbox copy of the
 * original subtree and apply them to the original subtree or reject them.
 */

var SandboxManager = /*#__PURE__*/function () {
  /**
   * The sandbox copy of the original subtree.
   */

  /**
   * The internal disposer.
   */

  /**
   * The internal `withSandbox` patch recorder. If `undefined`, no `withSandbox` call is being
   * executed.
   */

  /**
   * Function from `readonlyMiddleware` that will allow actions to be started inside the provided
   * code block on a readonly node.
   */

  /**
   * Creates an instance of `SandboxManager`.
   * Do not use directly, use `sandbox` instead.
   *
   * @param subtreeRoot Subtree root target object.
   */
  function SandboxManager(subtreeRoot) {
    var _this = this;

    this.subtreeRoot = void 0;
    this.subtreeRootClone = void 0;
    this.disposer = void 0;
    this.withSandboxPatchRecorder = void 0;
    this.allowWrite = void 0;
    this.subtreeRoot = subtreeRoot;
    assertTweakedObject(subtreeRoot, "subtreeRoot"); // we temporarily set the default value of the context manager so that
    // cloned nodes can access it while in their onInit phase

    var previousContextDefault = sandboxManagerContext.getDefault();
    sandboxManagerContext.setDefault(this);

    try {
      this.subtreeRootClone = clone(subtreeRoot, {
        generateNewIds: false
      });
      sandboxManagerContext.set(this.subtreeRootClone, this);
    } catch (err) {
      throw err;
    } finally {
      sandboxManagerContext.setDefault(previousContextDefault);
    }

    var wasRS = false;
    var disposeReactionRS = (0,mobx_esm.reaction)(function () {
      return isRootStore(subtreeRoot);
    }, function (isRS) {
      if (isRS !== wasRS) {
        wasRS = isRS;

        if (isRS) {
          registerRootStore(_this.subtreeRootClone);
        } else {
          unregisterRootStore(_this.subtreeRootClone);
        }
      }
    }, {
      fireImmediately: true
    });
    var disposeOnPatches = onPatches(subtreeRoot, function (patches) {
      if (_this.withSandboxPatchRecorder) {
        throw failure("original subtree must not change while 'withSandbox' executes");
      }

      _this.allowWrite(function () {
        applyPatches(_this.subtreeRootClone, patches);
      });
    });

    var _readonlyMiddleware = readonlyMiddleware(this.subtreeRootClone),
        allowWrite = _readonlyMiddleware.allowWrite,
        disposeReadonlyMW = _readonlyMiddleware.dispose;

    this.allowWrite = allowWrite;

    this.disposer = function () {
      disposeReactionRS();
      disposeOnPatches();
      disposeReadonlyMW();

      if (isRootStore(_this.subtreeRootClone)) {
        unregisterRootStore(_this.subtreeRootClone);
      }

      _this.disposer = function () {};
    };
  }
  /**
   * Executes `fn` with a sandbox copy of `node`. The changes made to the sandbox in `fn` can be
   * accepted, i.e. applied to the original subtree, or rejected.
   *
   * @typeparam T Object type.
   * @typeparam R Return type.
   * @param node Object or tuple of objects for which to obtain a sandbox copy.
   * @param fn Function that is called with a sandbox copy of `node`. Any changes made to the
   * sandbox are applied to the original subtree when `fn` returns `true` or
   * `{ commit: true, ... }`. When `fn` returns `false` or `{ commit: false, ... }` the changes made
   * to the sandbox are rejected.
   * @returns Value of type `R` when `fn` returns an object of type `{ commit: boolean; return: R }`
   * or `void` when `fn` returns a boolean.
   */


  var _proto = SandboxManager.prototype;

  _proto.withSandbox = function withSandbox(node, fn) {
    assertIsFunction(fn, "fn");
    var isNodesArray = Array.isArray(node) && !isTweakedObject(node, false);
    var nodes = isNodesArray ? node : [node];

    var _this$prepareSandboxC = this.prepareSandboxChanges(nodes),
        sandboxNodes = _this$prepareSandboxC.sandboxNodes,
        applyRecorderChanges = _this$prepareSandboxC.applyRecorderChanges;

    var commit = false;

    try {
      var returnValue = this.allowWrite(function () {
        return fn(isNodesArray ? sandboxNodes : sandboxNodes[0]);
      });

      if (typeof returnValue === "boolean") {
        commit = returnValue;
        return undefined;
      } else {
        commit = returnValue.commit;
        return returnValue["return"];
      }
    } finally {
      applyRecorderChanges(commit);
    }
  }
  /**
   * Disposes of the sandbox.
   */
  ;

  _proto.dispose = function dispose() {
    this.disposer();
  };

  _proto.prepareSandboxChanges = function prepareSandboxChanges(nodes) {
    var _this2 = this;

    var isNestedWithSandboxCall = !!this.withSandboxPatchRecorder;
    var sandboxNodes = nodes.map(function (node) {
      assertTweakedObject(node, "node");
      var path = getParentToChildPath(isNestedWithSandboxCall ? _this2.subtreeRootClone : _this2.subtreeRoot, node);

      if (!path) {
        throw failure("node is not a child of subtreeRoot" + (isNestedWithSandboxCall ? "Clone" : ""));
      }

      var sandboxNode = resolvePath(_this2.subtreeRootClone, path).value;

      if (!sandboxNode) {
        throw failure("path could not be resolved - sandbox may be out of sync with original tree");
      }

      return sandboxNode;
    });

    if (!this.withSandboxPatchRecorder) {
      this.withSandboxPatchRecorder = patchRecorder(this.subtreeRootClone);
    }

    var recorder = this.withSandboxPatchRecorder;
    var numRecorderEvents = recorder.events.length;

    var applyRecorderChanges = function applyRecorderChanges(commit) {
      if (!isNestedWithSandboxCall) {
        recorder.dispose();
        _this2.withSandboxPatchRecorder = undefined;
      }

      (0,mobx_esm.runInAction)(function () {
        if (commit) {
          if (!isNestedWithSandboxCall) {
            var len = recorder.events.length;

            for (var i = 0; i < len; i++) {
              applyPatches(_this2.subtreeRoot, recorder.events[i].patches);
            }
          }
        } else {
          _this2.allowWrite(function () {
            var i = recorder.events.length;

            while (i-- > numRecorderEvents) {
              applyPatches(_this2.subtreeRootClone, recorder.events[i].inversePatches, true);
            }
          });
        }
      });
    };

    return {
      sandboxNodes: sandboxNodes,
      applyRecorderChanges: applyRecorderChanges
    };
  };

  return SandboxManager;
}();
/**
 * Creates a sandbox.
 *
 * @param subtreeRoot Subtree root target object.
 * @returns A `SandboxManager` which allows you to manage the sandbox operations and dispose of the
 * sandbox.
 */

function sandbox(subtreeRoot) {
  return new SandboxManager(subtreeRoot);
}

/**
 * A set that is backed by an array.
 * Use `arraySet` to create it.
 */

var ArraySet = /*#__PURE__*/function (_Model) {
  _inheritsLoose(ArraySet, _Model);

  function ArraySet() {
    return _Model.apply(this, arguments) || this;
  }

  var _proto = ArraySet.prototype;

  _proto.add = function add(value) {
    var items = this.items;

    if (!items.includes(value)) {
      items.push(value);
    }

    return this;
  };

  _proto.clear = function clear() {
    this.items.length = 0;
  };

  _proto["delete"] = function _delete(value) {
    var items = this.items;
    var index = items.findIndex(function (t) {
      return t === value;
    });

    if (index >= 0) {
      items.splice(index, 1);
      return true;
    } else {
      return false;
    }
  };

  _proto.forEach = function forEach(callbackfn, thisArg) {
    // we cannot use the set implementation since we need to pass this as set
    var items = this.items;
    var len = items.length;

    for (var i = 0; i < len; i++) {
      var k = items[i];
      callbackfn.call(thisArg, k, k, this);
    }
  };

  _proto.has = function has(value) {
    return this.items.includes(value);
  };

  _proto.keys = function keys() {
    return this.values(); // yes, values
  };

  _proto.values = function values$1() {
    var items = this.items;
    return (0,mobx_esm.values)(items)[Symbol.iterator]();
  };

  _proto.entries = function entries() {
    var items = this.items; // TODO: should use an actual iterator

    return items.map(function (v) {
      return [v, v];
    }).values();
  };

  _proto[Symbol.iterator] = function () {
    return this.values();
  };

  _createClass(ArraySet, [{
    key: "size",
    get: function get() {
      return this.items.length;
    }
  }, {
    key: Symbol.toStringTag,
    get: function get() {
      return "ArraySet";
    }
  }]);

  return ArraySet;
}( /*#__PURE__*/Model({
  items: /*#__PURE__*/tProp( /*#__PURE__*/typesArray( /*#__PURE__*/typesUnchecked()), function () {
    return [];
  })
}));

__decorate([modelAction], ArraySet.prototype, "add", null);

__decorate([modelAction], ArraySet.prototype, "clear", null);

__decorate([modelAction], ArraySet.prototype, "delete", null);

ArraySet = /*#__PURE__*/__decorate([/*#__PURE__*/model("mobx-keystone/ArraySet")], ArraySet);
/**
 * Creates a new ArraySet model instance.
 *
 * @typeparam V Value type.
 * @param [entries] Optional initial values.
 */

function arraySet(values) {
  var initialArr = values ? values.slice() : [];
  return new ArraySet({
    items: initialArr
  });
}

/**
 * A type that represents an array backed set ArraySet.
 *
 * Example:
 * ```ts
 * const numberSetType = types.arraySet(types.number)
 * ```
 *
 * @typeparam T Value type.
 * @param valueType Value type.
 * @returns
 */

function typesArraySet(valueType) {
  var typeInfoGen = function typeInfoGen(t) {
    return new ArraySetTypeInfo(t, resolveStandardType(valueType));
  };

  return lateTypeChecker(function () {
    var valueChecker = resolveTypeChecker(valueType);

    var getTypeName = function getTypeName() {
      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      return "ArraySet<" + valueChecker.getTypeName.apply(valueChecker, recursiveTypeCheckers.concat([valueChecker])) + ">";
    };

    var thisTc = new TypeChecker(function (obj, path) {
      if (!(obj instanceof ArraySet)) {
        return new TypeCheckError(path, getTypeName(thisTc), obj);
      }

      var dataTypeChecker = typesObject(function () {
        return {
          items: typesArray(valueChecker)
        };
      });
      var resolvedTc = resolveTypeChecker(dataTypeChecker);
      return resolvedTc.check(obj.$, path);
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.arraySet` type info.
 */

var ArraySetTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(ArraySetTypeInfo, _TypeInfo);

  _createClass(ArraySetTypeInfo, [{
    key: "valueTypeInfo",
    get: function get() {
      return getTypeInfo(this.valueType);
    }
  }]);

  function ArraySetTypeInfo(originalType, valueType) {
    var _this;

    _this = _TypeInfo.call(this, originalType) || this;
    _this.valueType = void 0;
    _this.valueType = valueType;
    return _this;
  }

  return ArraySetTypeInfo;
}(TypeInfo);

/**
 * A type that represents the union of several other types (a | b | c | ...).
 *
 * Example:
 * ```ts
 * const booleanOrNumberType = types.or(types.boolean, types.number)
 * ```
 *
 * @typeparam T Type.
 * @param orTypes Possible types.
 * @returns
 */

function typesOr() {
  for (var _len = arguments.length, orTypes = new Array(_len), _key = 0; _key < _len; _key++) {
    orTypes[_key] = arguments[_key];
  }

  var typeInfoGen = function typeInfoGen(t) {
    return new OrTypeInfo(t, orTypes.map(resolveStandardType));
  };

  return lateTypeChecker(function () {
    var checkers = orTypes.map(resolveTypeChecker); // if the or includes unchecked then it is unchecked

    if (checkers.some(function (tc) {
      return tc.unchecked;
    })) {
      return typesUnchecked();
    }

    var getTypeName = function getTypeName() {
      for (var _len2 = arguments.length, recursiveTypeCheckers = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
        recursiveTypeCheckers[_key2] = arguments[_key2];
      }

      var typeNames = checkers.map(function (tc) {
        if (recursiveTypeCheckers.includes(tc)) {
          return "...";
        }

        return tc.getTypeName.apply(tc, recursiveTypeCheckers.concat([tc]));
      });
      return typeNames.join(" | ");
    };

    var thisTc = new TypeChecker(function (value, path) {
      var noMatchingType = checkers.every(function (tc) {
        return !!tc.check(value, path);
      });

      if (noMatchingType) {
        return new TypeCheckError(path, getTypeName(thisTc), value);
      } else {
        return null;
      }
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.or` type info.
 */

var OrTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(OrTypeInfo, _TypeInfo);

  _createClass(OrTypeInfo, [{
    key: "orTypeInfos",
    // memoize to always return the same array on the getter
    get: function get() {
      return this._orTypeInfos();
    }
  }]);

  function OrTypeInfo(thisType, orTypes) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.orTypes = void 0;
    _this._orTypeInfos = lateVal(function () {
      return _this.orTypes.map(getTypeInfo);
    });
    _this.orTypes = orTypes;
    return _this;
  }

  return OrTypeInfo;
}(TypeInfo);

function enumValues(e) {
  var vals = [];

  for (var _i = 0, _Object$keys = Object.keys(e); _i < _Object$keys.length; _i++) {
    var k = _Object$keys[_i];
    var v = e[k]; // we have to do this since TS does something weird
    // to number values
    // Hi = 0 -> { Hi: 0, 0: "Hi" }

    if (typeof v !== "string" || e[v] !== +k) {
      vals.push(v);
    }
  }

  return vals;
}

function typesEnum(enumObject) {
  assertIsObject(enumObject, "enumObject");
  var literals = enumValues(enumObject).map(function (e) {
    return typesLiteral(e);
  });
  return typesOr.apply(void 0, literals);
}

/**
 * A type that represents either a type or undefined.
 * Syntactic sugar for `types.or(baseType, types.undefined)`
 *
 * Example:
 * ```ts
 * const numberOrUndefinedType = types.maybe(types.number)
 * ```
 *
 * @typeparam S Type.
 * @param baseType Type.
 * @returns
 */

function typesMaybe(baseType) {
  return typesOr(baseType, typesUndefined);
}
/**
 * A type that represents either a type or null.
 * Syntactic sugar for `types.or(baseType, types.null)`
 *
 *  * Example:
 * ```ts
 * const numberOrNullType = types.maybeNull(types.number)
 * ```
 *
 * @typeparam S Type.
 * @param type Type.
 * @returns
 */

function typesMaybeNull(type) {
  return typesOr(type, typesNull);
}

/**
 * A type that represents an object-like map, an object with string keys and values all of a same given type.
 *
 * Example:
 * ```ts
 * // { [k: string]: number }
 * const numberMapType = types.record(types.number)
 * ```
 *
 * @typeparam T Type.
 * @param valueType Type of the values of the object-like map.
 * @returns
 */

function typesRecord(valueType) {
  var typeInfoGen = function typeInfoGen(tc) {
    return new RecordTypeInfo(tc, resolveStandardType(valueType));
  };

  return lateTypeChecker(function () {
    var valueChecker = resolveTypeChecker(valueType);

    var getTypeName = function getTypeName() {
      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      return "Record<" + valueChecker.getTypeName.apply(valueChecker, recursiveTypeCheckers.concat([valueChecker])) + ">";
    };

    var thisTc = new TypeChecker(function (obj, path) {
      if (!isObject(obj)) return new TypeCheckError(path, getTypeName(thisTc), obj);

      if (!valueChecker.unchecked) {
        var keys = Object.keys(obj);

        for (var i = 0; i < keys.length; i++) {
          var k = keys[i];
          var v = obj[k];
          var valueError = valueChecker.check(v, [].concat(path, [k]));

          if (valueError) {
            return valueError;
          }
        }
      }

      return null;
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.record` type info.
 */

var RecordTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(RecordTypeInfo, _TypeInfo);

  _createClass(RecordTypeInfo, [{
    key: "valueTypeInfo",
    get: function get() {
      return getTypeInfo(this.valueType);
    }
  }]);

  function RecordTypeInfo(thisType, valueType) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.valueType = void 0;
    _this.valueType = valueType;
    return _this;
  }

  return RecordTypeInfo;
}(TypeInfo);

/**
 * A map that is backed by an object-like map.
 * Use `objectMap` to create it.
 */

var ObjectMap = /*#__PURE__*/function (_Model) {
  _inheritsLoose(ObjectMap, _Model);

  function ObjectMap() {
    return _Model.apply(this, arguments) || this;
  }

  var _proto = ObjectMap.prototype;

  _proto.clear = function clear() {
    var items = this.items;
    var keys = Object.keys(items);
    var len = keys.length;

    for (var i = 0; i < len; i++) {
      var k = keys[i];
      (0,mobx_esm.remove)(items, k);
    }
  };

  _proto["delete"] = function _delete(key) {
    var hasKey = this.has(key);

    if (hasKey) {
      (0,mobx_esm.remove)(this.items, key);
      return true;
    } else {
      return false;
    }
  };

  _proto.forEach = function forEach(callbackfn, thisArg) {
    // we cannot use the map implementation since we need to pass this as map
    var items = this.items;
    var keys = Object.keys(items);
    var len = keys.length;

    for (var i = 0; i < len; i++) {
      var k = keys[i];
      callbackfn.call(thisArg, items[k], k, this);
    }
  };

  _proto.get = function get$1(key) {
    return (0,mobx_esm.get)(this.items, key);
  };

  _proto.has = function has$1(key) {
    return (0,mobx_esm.has)(this.items, key);
  };

  _proto.set = function set$1(key, value) {
    (0,mobx_esm.set)(this.items, key, value);

    return this;
  };

  _proto.keys = function keys$1() {
    // TODO: should use an actual iterator
    return (0,mobx_esm.keys)(this.items)[Symbol.iterator]();
  };

  _proto.values = function values$1() {
    // TODO: should use an actual iterator
    return (0,mobx_esm.values)(this.items)[Symbol.iterator]();
  };

  _proto.entries = function entries$1() {
    // TODO: should use an actual iterator
    return (0,mobx_esm.entries)(this.items)[Symbol.iterator]();
  };

  _proto[Symbol.iterator] = function () {
    return this.entries();
  };

  _createClass(ObjectMap, [{
    key: "size",
    get: function get() {
      return (0,mobx_esm.keys)(this.items).length;
    }
  }, {
    key: Symbol.toStringTag,
    get: function get() {
      return "ObjectMap";
    }
  }]);

  return ObjectMap;
}( /*#__PURE__*/Model({
  items: /*#__PURE__*/tProp( /*#__PURE__*/typesRecord( /*#__PURE__*/typesUnchecked()), function () {
    return {};
  })
}));

__decorate([modelAction], ObjectMap.prototype, "clear", null);

__decorate([modelAction], ObjectMap.prototype, "delete", null);

__decorate([modelAction], ObjectMap.prototype, "set", null);

ObjectMap = /*#__PURE__*/__decorate([/*#__PURE__*/model("mobx-keystone/ObjectMap")], ObjectMap);
/**
 * Creates a new ObjectMap model instance.
 *
 * @typeparam V Value type.
 * @param [entries] Optional initial values.
 */

function objectMap(entries) {
  var initialObj = {};

  if (entries) {
    var len = entries.length;

    for (var i = 0; i < len; i++) {
      var entry = entries[i];
      initialObj[entry[0]] = entry[1];
    }
  }

  return new ObjectMap({
    items: initialObj
  });
}

/**
 * A type that represents an object-like map ObjectMap.
 *
 * Example:
 * ```ts
 * const numberMapType = types.objectMap(types.number)
 * ```
 *
 * @typeparam T Value type.
 * @param valueType Value type.
 * @returns
 */

function typesObjectMap(valueType) {
  var typeInfoGen = function typeInfoGen(t) {
    return new ObjectMapTypeInfo(t, resolveStandardType(valueType));
  };

  return lateTypeChecker(function () {
    var valueChecker = resolveTypeChecker(valueType);

    var getTypeName = function getTypeName() {
      for (var _len = arguments.length, recursiveTypeCheckers = new Array(_len), _key = 0; _key < _len; _key++) {
        recursiveTypeCheckers[_key] = arguments[_key];
      }

      return "ObjectMap<" + valueChecker.getTypeName.apply(valueChecker, recursiveTypeCheckers.concat([valueChecker])) + ">";
    };

    var thisTc = new TypeChecker(function (obj, path) {
      if (!(obj instanceof ObjectMap)) {
        return new TypeCheckError(path, getTypeName(thisTc), obj);
      }

      var dataTypeChecker = typesObject(function () {
        return {
          items: typesRecord(valueChecker)
        };
      });
      var resolvedTc = resolveTypeChecker(dataTypeChecker);
      return resolvedTc.check(obj.$, path);
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.objectMap` type info.
 */

var ObjectMapTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(ObjectMapTypeInfo, _TypeInfo);

  _createClass(ObjectMapTypeInfo, [{
    key: "valueTypeInfo",
    get: function get() {
      return getTypeInfo(this.valueType);
    }
  }]);

  function ObjectMapTypeInfo(thisType, valueType) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.valueType = void 0;
    _this.valueType = valueType;
    return _this;
  }

  return ObjectMapTypeInfo;
}(TypeInfo);

/**
 * A type that represents a reference to an object or model.
 *
 * Example:
 * ```ts
 * const refToSomeObject = types.ref<SomeObject>()
 * ```
 *
 * @typeparam M Model type.
 * @returns
 */

function typesRef() {
  return refTypeChecker;
}
var typeName = "Ref";
var refDataTypeChecker = /*#__PURE__*/typesObject(function () {
  return {
    id: typesString
  };
});
var refTypeChecker = /*#__PURE__*/new TypeChecker(function (value, path) {
  if (!(value instanceof Ref)) {
    return new TypeCheckError(path, typeName, value);
  }

  var resolvedTc = resolveTypeChecker(refDataTypeChecker);
  return resolvedTc.check(value.$, path);
}, function () {
  return typeName;
}, function (t) {
  return new RefTypeInfo(t);
});
/**
 * `types.ref` type info.
 */

var RefTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(RefTypeInfo, _TypeInfo);

  function RefTypeInfo() {
    return _TypeInfo.apply(this, arguments) || this;
  }

  return RefTypeInfo;
}(TypeInfo);

/**
 * A type that represents an tuple of values of a given type.
 *
 * Example:
 * ```ts
 * const stringNumberTupleType = types.tuple(types.string, types.number)
 * ```
 *
 * @typeparam T Item types.
 * @param itemType Type of inner items.
 * @returns
 */

function typesTuple() {
  for (var _len = arguments.length, itemTypes = new Array(_len), _key = 0; _key < _len; _key++) {
    itemTypes[_key] = arguments[_key];
  }

  var typeInfoGen = function typeInfoGen(t) {
    return new TupleTypeInfo(t, itemTypes.map(resolveStandardType));
  };

  return lateTypeChecker(function () {
    var checkers = itemTypes.map(resolveTypeChecker);

    var getTypeName = function getTypeName() {
      for (var _len2 = arguments.length, recursiveTypeCheckers = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
        recursiveTypeCheckers[_key2] = arguments[_key2];
      }

      var typeNames = checkers.map(function (tc) {
        if (recursiveTypeCheckers.includes(tc)) {
          return "...";
        }

        return tc.getTypeName.apply(tc, recursiveTypeCheckers.concat([tc]));
      });
      return "[" + typeNames.join(", ") + "]";
    };

    var thisTc = new TypeChecker(function (array, path) {
      if (!isArray(array) || array.length !== itemTypes.length) {
        return new TypeCheckError(path, getTypeName(thisTc), array);
      }

      for (var i = 0; i < array.length; i++) {
        var itemChecker = checkers[i];

        if (!itemChecker.unchecked) {
          var itemError = itemChecker.check(array[i], [].concat(path, [i]));

          if (itemError) {
            return itemError;
          }
        }
      }

      return null;
    }, getTypeName, typeInfoGen);
    return thisTc;
  }, typeInfoGen);
}
/**
 * `types.tuple` type info.
 */

var TupleTypeInfo = /*#__PURE__*/function (_TypeInfo) {
  _inheritsLoose(TupleTypeInfo, _TypeInfo);

  _createClass(TupleTypeInfo, [{
    key: "itemTypeInfos",
    // memoize to always return the same array on the getter
    get: function get() {
      return this._itemTypeInfos();
    }
  }]);

  function TupleTypeInfo(thisType, itemTypes) {
    var _this;

    _this = _TypeInfo.call(this, thisType) || this;
    _this.itemTypes = void 0;
    _this._itemTypeInfos = lateVal(function () {
      return _this.itemTypes.map(getTypeInfo);
    });
    _this.itemTypes = itemTypes;
    return _this;
  }

  return TupleTypeInfo;
}(TypeInfo);

var types = {
  literal: typesLiteral,
  undefined: typesUndefined,
  "null": typesNull,
  "boolean": typesBoolean,
  number: typesNumber,
  string: typesString,
  or: typesOr,
  maybe: typesMaybe,
  maybeNull: typesMaybeNull,
  array: typesArray,
  record: typesRecord,
  unchecked: typesUnchecked,
  model: typesModel,
  object: typesObject,
  ref: typesRef,
  frozen: typesFrozen,
  "enum": typesEnum,
  refinement: typesRefinement,
  integer: typesInteger,
  nonEmptyString: typesNonEmptyString,
  objectMap: typesObjectMap,
  arraySet: typesArraySet,
  tuple: typesTuple,
  mapArray: function mapArray(valueType) {
    return typesArray(typesTuple(typesString, valueType));
  },
  setArray: function setArray(valueType) {
    return typesArray(valueType);
  },
  mapObject: function mapObject(valueType) {
    return typesRecord(valueType);
  },
  dateString: typesNonEmptyString,
  dateTimestamp: typesInteger
};




/***/ }),

/***/ 9637:
/*!********************************************!*\
  !*** ./node_modules/mobx/dist/mobx.esm.js ***!
  \********************************************/
/***/ ((__unused_webpack_module, __webpack_exports__, __webpack_require__) => {

"use strict";
__webpack_require__.r(__webpack_exports__);
/* harmony export */ __webpack_require__.d(__webpack_exports__, {
/* harmony export */   "$mobx": () => (/* binding */ $mobx),
/* harmony export */   "FlowCancellationError": () => (/* binding */ FlowCancellationError),
/* harmony export */   "ObservableMap": () => (/* binding */ ObservableMap),
/* harmony export */   "ObservableSet": () => (/* binding */ ObservableSet),
/* harmony export */   "Reaction": () => (/* binding */ Reaction),
/* harmony export */   "_allowStateChanges": () => (/* binding */ allowStateChanges),
/* harmony export */   "_allowStateChangesInsideComputed": () => (/* binding */ runInAction),
/* harmony export */   "_allowStateReadsEnd": () => (/* binding */ allowStateReadsEnd),
/* harmony export */   "_allowStateReadsStart": () => (/* binding */ allowStateReadsStart),
/* harmony export */   "_autoAction": () => (/* binding */ autoAction),
/* harmony export */   "_endAction": () => (/* binding */ _endAction),
/* harmony export */   "_getAdministration": () => (/* binding */ getAdministration),
/* harmony export */   "_getGlobalState": () => (/* binding */ getGlobalState),
/* harmony export */   "_interceptReads": () => (/* binding */ interceptReads),
/* harmony export */   "_isComputingDerivation": () => (/* binding */ isComputingDerivation),
/* harmony export */   "_resetGlobalState": () => (/* binding */ resetGlobalState),
/* harmony export */   "_startAction": () => (/* binding */ _startAction),
/* harmony export */   "action": () => (/* binding */ action),
/* harmony export */   "autorun": () => (/* binding */ autorun),
/* harmony export */   "comparer": () => (/* binding */ comparer),
/* harmony export */   "computed": () => (/* binding */ computed),
/* harmony export */   "configure": () => (/* binding */ configure),
/* harmony export */   "createAtom": () => (/* binding */ createAtom),
/* harmony export */   "entries": () => (/* binding */ entries),
/* harmony export */   "extendObservable": () => (/* binding */ extendObservable),
/* harmony export */   "flow": () => (/* binding */ flow),
/* harmony export */   "flowResult": () => (/* binding */ flowResult),
/* harmony export */   "get": () => (/* binding */ get),
/* harmony export */   "getAtom": () => (/* binding */ getAtom),
/* harmony export */   "getDebugName": () => (/* binding */ getDebugName),
/* harmony export */   "getDependencyTree": () => (/* binding */ getDependencyTree),
/* harmony export */   "getObserverTree": () => (/* binding */ getObserverTree),
/* harmony export */   "has": () => (/* binding */ has),
/* harmony export */   "intercept": () => (/* binding */ intercept),
/* harmony export */   "isAction": () => (/* binding */ isAction),
/* harmony export */   "isBoxedObservable": () => (/* binding */ isObservableValue),
/* harmony export */   "isComputed": () => (/* binding */ isComputed),
/* harmony export */   "isComputedProp": () => (/* binding */ isComputedProp),
/* harmony export */   "isFlowCancellationError": () => (/* binding */ isFlowCancellationError),
/* harmony export */   "isObservable": () => (/* binding */ isObservable),
/* harmony export */   "isObservableArray": () => (/* binding */ isObservableArray),
/* harmony export */   "isObservableMap": () => (/* binding */ isObservableMap),
/* harmony export */   "isObservableObject": () => (/* binding */ isObservableObject),
/* harmony export */   "isObservableProp": () => (/* binding */ isObservableProp),
/* harmony export */   "isObservableSet": () => (/* binding */ isObservableSet),
/* harmony export */   "keys": () => (/* binding */ keys),
/* harmony export */   "makeAutoObservable": () => (/* binding */ makeAutoObservable),
/* harmony export */   "makeObservable": () => (/* binding */ makeObservable),
/* harmony export */   "observable": () => (/* binding */ observable),
/* harmony export */   "observe": () => (/* binding */ observe),
/* harmony export */   "onBecomeObserved": () => (/* binding */ onBecomeObserved),
/* harmony export */   "onBecomeUnobserved": () => (/* binding */ onBecomeUnobserved),
/* harmony export */   "onReactionError": () => (/* binding */ onReactionError),
/* harmony export */   "reaction": () => (/* binding */ reaction),
/* harmony export */   "remove": () => (/* binding */ remove),
/* harmony export */   "runInAction": () => (/* binding */ runInAction),
/* harmony export */   "set": () => (/* binding */ set),
/* harmony export */   "spy": () => (/* binding */ spy),
/* harmony export */   "toJS": () => (/* binding */ toJS),
/* harmony export */   "trace": () => (/* binding */ trace),
/* harmony export */   "transaction": () => (/* binding */ transaction),
/* harmony export */   "untracked": () => (/* binding */ untracked),
/* harmony export */   "values": () => (/* binding */ values),
/* harmony export */   "when": () => (/* binding */ when)
/* harmony export */ });
var niceErrors = {
  0: "Invalid value for configuration 'enforceActions', expected 'never', 'always' or 'observed'",
  1: function _(prop) {
    return "Cannot decorate undefined property: '" + prop.toString() + "'";
  },
  2: function _(prop) {
    return "invalid decorator for '" + prop.toString() + "'";
  },
  3: function _(prop) {
    return "Cannot decorate '" + prop.toString() + "': action can only be used on properties with a function value.";
  },
  4: function _(prop) {
    return "Cannot decorate '" + prop.toString() + "': computed can only be used on getter properties.";
  },
  5: "'keys()' can only be used on observable objects, arrays, sets and maps",
  6: "'values()' can only be used on observable objects, arrays, sets and maps",
  7: "'entries()' can only be used on observable objects, arrays and maps",
  8: "'set()' can only be used on observable objects, arrays and maps",
  9: "'remove()' can only be used on observable objects, arrays and maps",
  10: "'has()' can only be used on observable objects, arrays and maps",
  11: "'get()' can only be used on observable objects, arrays and maps",
  12: "Invalid annotation",
  13: "Dynamic observable objects cannot be frozen",
  14: "Intercept handlers should return nothing or a change object",
  15: "Observable arrays cannot be frozen",
  16: "Modification exception: the internal structure of an observable array was changed.",
  17: function _(index, length) {
    return "[mobx.array] Index out of bounds, " + index + " is larger than " + length;
  },
  18: "mobx.map requires Map polyfill for the current browser. Check babel-polyfill or core-js/es6/map.js",
  19: function _(other) {
    return "Cannot initialize from classes that inherit from Map: " + other.constructor.name;
  },
  20: function _(other) {
    return "Cannot initialize map from " + other;
  },
  21: function _(dataStructure) {
    return "Cannot convert to map from '" + dataStructure + "'";
  },
  22: "mobx.set requires Set polyfill for the current browser. Check babel-polyfill or core-js/es6/set.js",
  23: "It is not possible to get index atoms from arrays",
  24: function _(thing) {
    return "Cannot obtain administration from " + thing;
  },
  25: function _(property, name) {
    return "the entry '" + property + "' does not exist in the observable map '" + name + "'";
  },
  26: "please specify a property",
  27: function _(property, name) {
    return "no observable property '" + property.toString() + "' found on the observable object '" + name + "'";
  },
  28: function _(thing) {
    return "Cannot obtain atom from " + thing;
  },
  29: "Expecting some object",
  30: "invalid action stack. did you forget to finish an action?",
  31: "missing option for computed: get",
  32: function _(name, derivation) {
    return "Cycle detected in computation " + name + ": " + derivation;
  },
  33: function _(name) {
    return "The setter of computed value '" + name + "' is trying to update itself. Did you intend to update an _observable_ value, instead of the computed property?";
  },
  34: function _(name) {
    return "[ComputedValue '" + name + "'] It is not possible to assign a new value to a computed value.";
  },
  35: "There are multiple, different versions of MobX active. Make sure MobX is loaded only once or use `configure({ isolateGlobalState: true })`",
  36: "isolateGlobalState should be called before MobX is running any reactions",
  37: function _(method) {
    return "[mobx] `observableArray." + method + "()` mutates the array in-place, which is not allowed inside a derivation. Use `array.slice()." + method + "()` instead";
  }
};
var errors =  false ? 0 : {};
function die(error) {
  for (var _len = arguments.length, args = new Array(_len > 1 ? _len - 1 : 0), _key = 1; _key < _len; _key++) {
    args[_key - 1] = arguments[_key];
  }

  if (false) { var e; }

  throw new Error(typeof error === "number" ? "[MobX] minified error nr: " + error + (args.length ? " " + args.join(",") : "") + ". Find the full error at: https://github.com/mobxjs/mobx/blob/mobx6/src/errors.ts" : "[MobX] " + error);
}

var mockGlobal = {};
function getGlobal() {
  if (typeof window !== "undefined") {
    return window;
  }

  if (typeof __webpack_require__.g !== "undefined") {
    return __webpack_require__.g;
  }

  if (typeof self !== "undefined") {
    return self;
  }

  return mockGlobal;
}

var assign = Object.assign;
var getDescriptor = Object.getOwnPropertyDescriptor;
var defineProperty = Object.defineProperty;
var objectPrototype = Object.prototype;
var EMPTY_ARRAY = [];
Object.freeze(EMPTY_ARRAY);
var EMPTY_OBJECT = {};
Object.freeze(EMPTY_OBJECT);
var hasProxy = typeof Proxy !== "undefined";
var plainObjectString = /*#__PURE__*/Object.toString();
function assertProxies() {
  if (!hasProxy) {
    die( false ? 0 : "Proxy not available");
  }
}
function warnAboutProxyRequirement(msg) {
  if (false) {}
}
function getNextId() {
  return ++globalState.mobxGuid;
}
/**
 * Makes sure that the provided function is invoked at most once.
 */

function once(func) {
  var invoked = false;
  return function () {
    if (invoked) return;
    invoked = true;
    return func.apply(this, arguments);
  };
}
var noop = function noop() {};
function isFunction(fn) {
  return typeof fn === "function";
}
function isStringish(value) {
  var t = typeof value;

  switch (t) {
    case "string":
    case "symbol":
    case "number":
      return true;
  }

  return false;
}
function isObject(value) {
  return value !== null && typeof value === "object";
}
function isPlainObject(value) {
  var _proto$constructor;

  if (!isObject(value)) return false;
  var proto = Object.getPrototypeOf(value);
  if (proto == null) return true;
  return ((_proto$constructor = proto.constructor) == null ? void 0 : _proto$constructor.toString()) === plainObjectString;
} // https://stackoverflow.com/a/37865170

function isGenerator(obj) {
  var constructor = obj == null ? void 0 : obj.constructor;
  if (!constructor) return false;
  if ("GeneratorFunction" === constructor.name || "GeneratorFunction" === constructor.displayName) return true;
  return false;
}
function addHiddenProp(object, propName, value) {
  defineProperty(object, propName, {
    enumerable: false,
    writable: true,
    configurable: true,
    value: value
  });
}
function addHiddenFinalProp(object, propName, value) {
  defineProperty(object, propName, {
    enumerable: false,
    writable: false,
    configurable: true,
    value: value
  });
}
function assertPropertyConfigurable(object, prop) {
  if (false) { var descriptor; }
}
function createInstanceofPredicate(name, theClass) {
  var propName = "isMobX" + name;
  theClass.prototype[propName] = true;
  return function (x) {
    return isObject(x) && x[propName] === true;
  };
}
function isES6Map(thing) {
  return thing instanceof Map;
}
function isES6Set(thing) {
  return thing instanceof Set;
}
var hasGetOwnPropertySymbols = typeof Object.getOwnPropertySymbols !== "undefined";
/**
 * Returns the following: own keys, prototype keys & own symbol keys, if they are enumerable.
 */

function getPlainObjectKeys(object) {
  var keys = Object.keys(object); // Not supported in IE, so there are not going to be symbol props anyway...

  if (!hasGetOwnPropertySymbols) return keys;
  var symbols = Object.getOwnPropertySymbols(object);
  if (!symbols.length) return keys;
  return [].concat(keys, symbols.filter(function (s) {
    return objectPrototype.propertyIsEnumerable.call(object, s);
  }));
} // From Immer utils
// Returns all own keys, including non-enumerable and symbolic

var ownKeys = typeof Reflect !== "undefined" && Reflect.ownKeys ? Reflect.ownKeys : hasGetOwnPropertySymbols ? function (obj) {
  return Object.getOwnPropertyNames(obj).concat(Object.getOwnPropertySymbols(obj));
} :
/* istanbul ignore next */
Object.getOwnPropertyNames;
function stringifyKey(key) {
  if (typeof key === "string") return key;
  if (typeof key === "symbol") return key.toString();
  return new String(key).toString();
}
function toPrimitive(value) {
  return value === null ? null : typeof value === "object" ? "" + value : value;
}
function hasProp(target, prop) {
  return objectPrototype.hasOwnProperty.call(target, prop);
} // From Immer utils

var getOwnPropertyDescriptors = Object.getOwnPropertyDescriptors || function getOwnPropertyDescriptors(target) {
  // Polyfill needed for Hermes and IE, see https://github.com/facebook/hermes/issues/274
  var res = {}; // Note: without polyfill for ownKeys, symbols won't be picked up

  ownKeys(target).forEach(function (key) {
    res[key] = getDescriptor(target, key);
  });
  return res;
};

var mobxDecoratorsSymbol = /*#__PURE__*/Symbol("mobx-decorators");
var mobxAppliedDecoratorsSymbol = /*#__PURE__*/Symbol("mobx-applied-decorators");
function createDecorator(type) {
  return assign(function (target, property) {
    if (property === undefined) {
      // @decorator(arg) member
      createDecoratorAndAnnotation(type, target);
    } else {
      // @decorator member
      storeDecorator(target, property, type);
    }
  }, {
    annotationType_: type
  });
}
function createDecoratorAndAnnotation(type, arg_) {
  return assign(function (target, property) {
    storeDecorator(target, property, type, arg_);
  }, {
    annotationType_: type,
    arg_: arg_
  });
}
function storeDecorator(target, property, type, arg_) {
  var desc = getDescriptor(target, mobxDecoratorsSymbol);
  var map;

  if (desc) {
    map = desc.value;
  } else {
    map = {};
    addHiddenProp(target, mobxDecoratorsSymbol, map);
  }

  map[property] = {
    annotationType_: type,
    arg_: arg_
  };
}
function applyDecorators(target) {
  if (target[mobxAppliedDecoratorsSymbol]) return true;
  var current = target; // optimization: this could be cached per prototype!
  // (then we can remove the weird short circuiting as well..)

  var annotations = [];

  while (current && current !== objectPrototype) {
    var desc = getDescriptor(current, mobxDecoratorsSymbol);

    if (desc) {
      if (!annotations.length) {
        for (var key in desc.value) {
          // second conditions is to recognize actions
          if (!hasProp(target, key) && !hasProp(current, key)) {
            // not all fields are defined yet, so we are in the makeObservable call of some super class,
            // short circuit, here, we will do this again in a later makeObservable call
            return true;
          }
        }
      }

      annotations.unshift(desc.value);
    }

    current = Object.getPrototypeOf(current);
  }

  annotations.forEach(function (a) {
    makeObservable(target, a);
  });
  addHiddenProp(target, mobxAppliedDecoratorsSymbol, true);
  return annotations.length > 0;
}

var $mobx = /*#__PURE__*/Symbol("mobx administration");
var Atom = /*#__PURE__*/function () {
  // for effective unobserving. BaseAtom has true, for extra optimization, so its onBecomeUnobserved never gets called, because it's not needed

  /**
   * Create a new atom. For debugging purposes it is recommended to give it a name.
   * The onBecomeObserved and onBecomeUnobserved callbacks can be used for resource management.
   */
  function Atom(name_) {
    if (name_ === void 0) {
      name_ = "Atom@" + getNextId();
    }

    this.name_ = void 0;
    this.isPendingUnobservation_ = false;
    this.isBeingObserved_ = false;
    this.observers_ = new Set();
    this.diffValue_ = 0;
    this.lastAccessedBy_ = 0;
    this.lowestObserverState_ = IDerivationState_.NOT_TRACKING_;
    this.onBOL = void 0;
    this.onBUOL = void 0;
    this.name_ = name_;
  } // onBecomeObservedListeners


  var _proto = Atom.prototype;

  _proto.onBO = function onBO() {
    if (this.onBOL) {
      this.onBOL.forEach(function (listener) {
        return listener();
      });
    }
  };

  _proto.onBUO = function onBUO() {
    if (this.onBUOL) {
      this.onBUOL.forEach(function (listener) {
        return listener();
      });
    }
  }
  /**
   * Invoke this method to notify mobx that your atom has been used somehow.
   * Returns true if there is currently a reactive context.
   */
  ;

  _proto.reportObserved = function reportObserved$1() {
    return reportObserved(this);
  }
  /**
   * Invoke this method _after_ this method has changed to signal mobx that all its observers should invalidate.
   */
  ;

  _proto.reportChanged = function reportChanged() {
    startBatch();
    propagateChanged(this);
    endBatch();
  };

  _proto.toString = function toString() {
    return this.name_;
  };

  return Atom;
}();
var isAtom = /*#__PURE__*/createInstanceofPredicate("Atom", Atom);
function createAtom(name, onBecomeObservedHandler, onBecomeUnobservedHandler) {
  if (onBecomeObservedHandler === void 0) {
    onBecomeObservedHandler = noop;
  }

  if (onBecomeUnobservedHandler === void 0) {
    onBecomeUnobservedHandler = noop;
  }

  var atom = new Atom(name); // default `noop` listener will not initialize the hook Set

  if (onBecomeObservedHandler !== noop) {
    onBecomeObserved(atom, onBecomeObservedHandler);
  }

  if (onBecomeUnobservedHandler !== noop) {
    onBecomeUnobserved(atom, onBecomeUnobservedHandler);
  }

  return atom;
}

function identityComparer(a, b) {
  return a === b;
}

function structuralComparer(a, b) {
  return deepEqual(a, b);
}

function shallowComparer(a, b) {
  return deepEqual(a, b, 1);
}

function defaultComparer(a, b) {
  return Object.is(a, b);
}

var comparer = {
  identity: identityComparer,
  structural: structuralComparer,
  "default": defaultComparer,
  shallow: shallowComparer
};

function deepEnhancer(v, _, name) {
  // it is an observable already, done
  if (isObservable(v)) return v; // something that can be converted and mutated?

  if (Array.isArray(v)) return observable.array(v, {
    name: name
  });
  if (isPlainObject(v)) return observable.object(v, undefined, {
    name: name
  });
  if (isES6Map(v)) return observable.map(v, {
    name: name
  });
  if (isES6Set(v)) return observable.set(v, {
    name: name
  });
  return v;
}
function shallowEnhancer(v, _, name) {
  if (v === undefined || v === null) return v;
  if (isObservableObject(v) || isObservableArray(v) || isObservableMap(v) || isObservableSet(v)) return v;
  if (Array.isArray(v)) return observable.array(v, {
    name: name,
    deep: false
  });
  if (isPlainObject(v)) return observable.object(v, undefined, {
    name: name,
    deep: false
  });
  if (isES6Map(v)) return observable.map(v, {
    name: name,
    deep: false
  });
  if (isES6Set(v)) return observable.set(v, {
    name: name,
    deep: false
  });
  if (false) {}
}
function referenceEnhancer(newValue) {
  // never turn into an observable
  return newValue;
}
function refStructEnhancer(v, oldValue) {
  if (false) {}
  if (deepEqual(v, oldValue)) return oldValue;
  return v;
}

var _annotationToEnhancer;
var OBSERVABLE = "observable";
var OBSERVABLE_REF = "observable.ref";
var OBSERVABLE_SHALLOW = "observable.shallow";
var OBSERVABLE_STRUCT = "observable.struct"; // Predefined bags of create observable options, to avoid allocating temporarily option objects
// in the majority of cases

var defaultCreateObservableOptions = {
  deep: true,
  name: undefined,
  defaultDecorator: undefined,
  proxy: true
};
Object.freeze(defaultCreateObservableOptions);
function asCreateObservableOptions(thing) {
  return thing || defaultCreateObservableOptions;
}
function getEnhancerFromOption(options) {
  return options.deep === true ? deepEnhancer : options.deep === false ? referenceEnhancer : getEnhancerFromAnnotation(options.defaultDecorator);
}
var annotationToEnhancer = (_annotationToEnhancer = {}, _annotationToEnhancer[OBSERVABLE] = deepEnhancer, _annotationToEnhancer[OBSERVABLE_REF] = referenceEnhancer, _annotationToEnhancer[OBSERVABLE_SHALLOW] = shallowEnhancer, _annotationToEnhancer[OBSERVABLE_STRUCT] = refStructEnhancer, _annotationToEnhancer);
function getEnhancerFromAnnotation(annotation) {
  var _annotationToEnhancer2;

  return !annotation ? deepEnhancer : (_annotationToEnhancer2 = annotationToEnhancer[annotation.annotationType_]) != null ? _annotationToEnhancer2 : die(12);
}
/**
 * Turns an object, array or function into a reactive structure.
 * @param v the value which should become observable.
 */

function createObservable(v, arg2, arg3) {
  // @observable someProp;
  if (isStringish(arg2)) {
    storeDecorator(v, arg2, OBSERVABLE);
    return;
  } // it is an observable already, done


  if (isObservable(v)) return v; // something that can be converted and mutated?

  var res = isPlainObject(v) ? observable.object(v, arg2, arg3) : Array.isArray(v) ? observable.array(v, arg2) : isES6Map(v) ? observable.map(v, arg2) : isES6Set(v) ? observable.set(v, arg2) : v; // this value could be converted to a new observable data structure, return it

  if (res !== v) return res;
  return observable.box(v);
}

createObservable.annotationType_ = OBSERVABLE;
var observableFactories = {
  box: function box(value, options) {
    var o = asCreateObservableOptions(options);
    return new ObservableValue(value, getEnhancerFromOption(o), o.name, true, o.equals);
  },
  array: function array(initialValues, options) {
    var o = asCreateObservableOptions(options);
    return (globalState.useProxies === false || o.proxy === false ? createLegacyArray : createObservableArray)(initialValues, getEnhancerFromOption(o), o.name);
  },
  map: function map(initialValues, options) {
    var o = asCreateObservableOptions(options);
    return new ObservableMap(initialValues, getEnhancerFromOption(o), o.name);
  },
  set: function set(initialValues, options) {
    var o = asCreateObservableOptions(options);
    return new ObservableSet(initialValues, getEnhancerFromOption(o), o.name);
  },
  object: function object(props, decorators, options) {
    var o = asCreateObservableOptions(options);
    var base = {};
    asObservableObject(base, options == null ? void 0 : options.name, getEnhancerFromOption(o));
    return extendObservable(globalState.useProxies === false || o.proxy === false ? base : createDynamicObservableObject(base), props, decorators, options);
  },
  ref: /*#__PURE__*/createDecorator(OBSERVABLE_REF),
  shallow: /*#__PURE__*/createDecorator(OBSERVABLE_SHALLOW),
  deep: /*#__PURE__*/createDecorator(OBSERVABLE),
  struct: /*#__PURE__*/createDecorator(OBSERVABLE_STRUCT)
}; // eslint-disable-next-line

var observable = /*#__PURE__*/assign(createObservable, observableFactories);

var COMPUTED = "computed";
var COMPUTED_STRUCT = "computed.struct";
/**
 * Decorator for class properties: @computed get value() { return expr; }.
 * For legacy purposes also invokable as ES5 observable created: `computed(() => expr)`;
 */

var computed = function computed(arg1, arg2, arg3) {
  if (isStringish(arg2)) {
    // @computed
    return storeDecorator(arg1, arg2, COMPUTED);
  }

  if (isPlainObject(arg1)) {
    // @computed({ options })
    return createDecoratorAndAnnotation(COMPUTED, arg1);
  } // computed(expr, options?)


  if (false) {}

  var opts = isPlainObject(arg2) ? arg2 : {};
  opts.get = arg1;
  opts.name = opts.name || arg1.name || "";
  /* for generated name */

  return new ComputedValue(opts);
};
computed.annotationType_ = COMPUTED;
computed.struct = /*#__PURE__*/assign(function (target, property) {
  storeDecorator(target, property, COMPUTED_STRUCT);
}, {
  annotationType_: COMPUTED_STRUCT
});

var _getDescriptor$config, _getDescriptor;
// mobx versions

var currentActionId = 0;
var nextActionId = 1;
var isFunctionNameConfigurable = (_getDescriptor$config = (_getDescriptor = /*#__PURE__*/getDescriptor(function () {}, "name")) == null ? void 0 : _getDescriptor.configurable) != null ? _getDescriptor$config : false; // we can safely recycle this object

var tmpNameDescriptor = {
  value: "action",
  configurable: true,
  writable: false,
  enumerable: false
};
function createAction(actionName, fn, autoAction, ref) {
  if (autoAction === void 0) {
    autoAction = false;
  }

  if (false) {}

  function res() {
    return executeAction(actionName, autoAction, fn, ref || this, arguments);
  }

  res.isMobxAction = true;

  if (isFunctionNameConfigurable) {
    tmpNameDescriptor.value = actionName;
    Object.defineProperty(res, "name", tmpNameDescriptor);
  }

  return res;
}
function executeAction(actionName, canRunAsDerivation, fn, scope, args) {
  var runInfo = _startAction(actionName, canRunAsDerivation, scope, args);

  try {
    return fn.apply(scope, args);
  } catch (err) {
    runInfo.error_ = err;
    throw err;
  } finally {
    _endAction(runInfo);
  }
}
function _startAction(actionName, canRunAsDerivation, // true for autoAction
scope, args) {
  var notifySpy_ =  false && 0;
  var startTime_ = 0;

  if (false) { var flattenedArgs; }

  var prevDerivation_ = globalState.trackingDerivation;
  var runAsAction = !canRunAsDerivation || !prevDerivation_;
  startBatch();
  var prevAllowStateChanges_ = globalState.allowStateChanges; // by default preserve previous allow

  if (runAsAction) {
    untrackedStart();
    prevAllowStateChanges_ = allowStateChangesStart(true);
  }

  var prevAllowStateReads_ = allowStateReadsStart(true);
  var runInfo = {
    runAsAction_: runAsAction,
    prevDerivation_: prevDerivation_,
    prevAllowStateChanges_: prevAllowStateChanges_,
    prevAllowStateReads_: prevAllowStateReads_,
    notifySpy_: notifySpy_,
    startTime_: startTime_,
    actionId_: nextActionId++,
    parentActionId_: currentActionId
  };
  currentActionId = runInfo.actionId_;
  return runInfo;
}
function _endAction(runInfo) {
  if (currentActionId !== runInfo.actionId_) {
    die(30);
  }

  currentActionId = runInfo.parentActionId_;

  if (runInfo.error_ !== undefined) {
    globalState.suppressReactionErrors = true;
  }

  allowStateChangesEnd(runInfo.prevAllowStateChanges_);
  allowStateReadsEnd(runInfo.prevAllowStateReads_);
  endBatch();
  if (runInfo.runAsAction_) untrackedEnd(runInfo.prevDerivation_);

  if (false) {}

  globalState.suppressReactionErrors = false;
}
function allowStateChanges(allowStateChanges, func) {
  var prev = allowStateChangesStart(allowStateChanges);

  try {
    return func();
  } finally {
    allowStateChangesEnd(prev);
  }
}
function allowStateChangesStart(allowStateChanges) {
  var prev = globalState.allowStateChanges;
  globalState.allowStateChanges = allowStateChanges;
  return prev;
}
function allowStateChangesEnd(prev) {
  globalState.allowStateChanges = prev;
}

function _defineProperties(target, props) {
  for (var i = 0; i < props.length; i++) {
    var descriptor = props[i];
    descriptor.enumerable = descriptor.enumerable || false;
    descriptor.configurable = true;
    if ("value" in descriptor) descriptor.writable = true;
    Object.defineProperty(target, descriptor.key, descriptor);
  }
}

function _createClass(Constructor, protoProps, staticProps) {
  if (protoProps) _defineProperties(Constructor.prototype, protoProps);
  if (staticProps) _defineProperties(Constructor, staticProps);
  return Constructor;
}

function _extends() {
  _extends = Object.assign || function (target) {
    for (var i = 1; i < arguments.length; i++) {
      var source = arguments[i];

      for (var key in source) {
        if (Object.prototype.hasOwnProperty.call(source, key)) {
          target[key] = source[key];
        }
      }
    }

    return target;
  };

  return _extends.apply(this, arguments);
}

function _inheritsLoose(subClass, superClass) {
  subClass.prototype = Object.create(superClass.prototype);
  subClass.prototype.constructor = subClass;
  subClass.__proto__ = superClass;
}

function _assertThisInitialized(self) {
  if (self === void 0) {
    throw new ReferenceError("this hasn't been initialised - super() hasn't been called");
  }

  return self;
}

function _unsupportedIterableToArray(o, minLen) {
  if (!o) return;
  if (typeof o === "string") return _arrayLikeToArray(o, minLen);
  var n = Object.prototype.toString.call(o).slice(8, -1);
  if (n === "Object" && o.constructor) n = o.constructor.name;
  if (n === "Map" || n === "Set") return Array.from(o);
  if (n === "Arguments" || /^(?:Ui|I)nt(?:8|16|32)(?:Clamped)?Array$/.test(n)) return _arrayLikeToArray(o, minLen);
}

function _arrayLikeToArray(arr, len) {
  if (len == null || len > arr.length) len = arr.length;

  for (var i = 0, arr2 = new Array(len); i < len; i++) arr2[i] = arr[i];

  return arr2;
}

function _createForOfIteratorHelperLoose(o, allowArrayLike) {
  var it;

  if (typeof Symbol === "undefined" || o[Symbol.iterator] == null) {
    if (Array.isArray(o) || (it = _unsupportedIterableToArray(o)) || allowArrayLike && o && typeof o.length === "number") {
      if (it) o = it;
      var i = 0;
      return function () {
        if (i >= o.length) return {
          done: true
        };
        return {
          done: false,
          value: o[i++]
        };
      };
    }

    throw new TypeError("Invalid attempt to iterate non-iterable instance.\nIn order to be iterable, non-array objects must have a [Symbol.iterator]() method.");
  }

  it = o[Symbol.iterator]();
  return it.next.bind(it);
}

var _Symbol$toPrimitive;
var CREATE = "create";
_Symbol$toPrimitive = Symbol.toPrimitive;
var ObservableValue = /*#__PURE__*/function (_Atom) {
  _inheritsLoose(ObservableValue, _Atom);

  function ObservableValue(value, enhancer, name_, notifySpy, equals) {
    var _this;

    if (name_ === void 0) {
      name_ = "ObservableValue@" + getNextId();
    }

    if (notifySpy === void 0) {
      notifySpy = true;
    }

    if (equals === void 0) {
      equals = comparer["default"];
    }

    _this = _Atom.call(this, name_) || this;
    _this.enhancer = void 0;
    _this.name_ = void 0;
    _this.equals = void 0;
    _this.hasUnreportedChange_ = false;
    _this.interceptors_ = void 0;
    _this.changeListeners_ = void 0;
    _this.value_ = void 0;
    _this.dehancer = void 0;
    _this.enhancer = enhancer;
    _this.name_ = name_;
    _this.equals = equals;
    _this.value_ = enhancer(value, undefined, name_);

    if (false) {}

    return _this;
  }

  var _proto = ObservableValue.prototype;

  _proto.dehanceValue = function dehanceValue(value) {
    if (this.dehancer !== undefined) return this.dehancer(value);
    return value;
  };

  _proto.set = function set(newValue) {
    var oldValue = this.value_;
    newValue = this.prepareNewValue_(newValue);

    if (newValue !== globalState.UNCHANGED) {
      var notifySpy = isSpyEnabled();

      if (false) {}

      this.setNewValue_(newValue);
      if (false) {}
    }
  };

  _proto.prepareNewValue_ = function prepareNewValue_(newValue) {
    checkIfStateModificationsAreAllowed(this);

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        object: this,
        type: UPDATE,
        newValue: newValue
      });
      if (!change) return globalState.UNCHANGED;
      newValue = change.newValue;
    } // apply modifier


    newValue = this.enhancer(newValue, this.value_, this.name_);
    return this.equals(this.value_, newValue) ? globalState.UNCHANGED : newValue;
  };

  _proto.setNewValue_ = function setNewValue_(newValue) {
    var oldValue = this.value_;
    this.value_ = newValue;
    this.reportChanged();

    if (hasListeners(this)) {
      notifyListeners(this, {
        type: UPDATE,
        object: this,
        newValue: newValue,
        oldValue: oldValue
      });
    }
  };

  _proto.get = function get() {
    this.reportObserved();
    return this.dehanceValue(this.value_);
  };

  _proto.intercept_ = function intercept_(handler) {
    return registerInterceptor(this, handler);
  };

  _proto.observe_ = function observe_(listener, fireImmediately) {
    if (fireImmediately) listener({
      observableKind: "value",
      debugObjectName: this.name_,
      object: this,
      type: UPDATE,
      newValue: this.value_,
      oldValue: undefined
    });
    return registerListener(this, listener);
  };

  _proto.raw = function raw() {
    // used by MST ot get undehanced value
    return this.value_;
  };

  _proto.toJSON = function toJSON() {
    return this.get();
  };

  _proto.toString = function toString() {
    return this.name_ + "[" + this.value_ + "]";
  };

  _proto.valueOf = function valueOf() {
    return toPrimitive(this.get());
  };

  _proto[_Symbol$toPrimitive] = function () {
    return this.valueOf();
  };

  return ObservableValue;
}(Atom);
var isObservableValue = /*#__PURE__*/createInstanceofPredicate("ObservableValue", ObservableValue);

var _Symbol$toPrimitive$1;
/**
 * A node in the state dependency root that observes other nodes, and can be observed itself.
 *
 * ComputedValue will remember the result of the computation for the duration of the batch, or
 * while being observed.
 *
 * During this time it will recompute only when one of its direct dependencies changed,
 * but only when it is being accessed with `ComputedValue.get()`.
 *
 * Implementation description:
 * 1. First time it's being accessed it will compute and remember result
 *    give back remembered result until 2. happens
 * 2. First time any deep dependency change, propagate POSSIBLY_STALE to all observers, wait for 3.
 * 3. When it's being accessed, recompute if any shallow dependency changed.
 *    if result changed: propagate STALE to all observers, that were POSSIBLY_STALE from the last step.
 *    go to step 2. either way
 *
 * If at any point it's outside batch and it isn't observed: reset everything and go to 1.
 */

_Symbol$toPrimitive$1 = Symbol.toPrimitive;
var ComputedValue = /*#__PURE__*/function () {
  // nodes we are looking at. Our value depends on these nodes
  // during tracking it's an array with new observed observers
  // to check for cycles
  // N.B: unminified as it is used by MST

  /**
   * Create a new computed value based on a function expression.
   *
   * The `name` property is for debug purposes only.
   *
   * The `equals` property specifies the comparer function to use to determine if a newly produced
   * value differs from the previous value. Two comparers are provided in the library; `defaultComparer`
   * compares based on identity comparison (===), and `structuralComparer` deeply compares the structure.
   * Structural comparison can be convenient if you always produce a new aggregated object and
   * don't want to notify observers if it is structurally the same.
   * This is useful for working with vectors, mouse coordinates etc.
   */
  function ComputedValue(options) {
    this.dependenciesState_ = IDerivationState_.NOT_TRACKING_;
    this.observing_ = [];
    this.newObserving_ = null;
    this.isBeingObserved_ = false;
    this.isPendingUnobservation_ = false;
    this.observers_ = new Set();
    this.diffValue_ = 0;
    this.runId_ = 0;
    this.lastAccessedBy_ = 0;
    this.lowestObserverState_ = IDerivationState_.UP_TO_DATE_;
    this.unboundDepsCount_ = 0;
    this.mapid_ = "#" + getNextId();
    this.value_ = new CaughtException(null);
    this.name_ = void 0;
    this.triggeredBy_ = void 0;
    this.isComputing_ = false;
    this.isRunningSetter_ = false;
    this.derivation = void 0;
    this.setter_ = void 0;
    this.isTracing_ = TraceMode.NONE;
    this.scope_ = void 0;
    this.equals_ = void 0;
    this.requiresReaction_ = void 0;
    this.keepAlive_ = void 0;
    this.onBOL = void 0;
    this.onBUOL = void 0;
    if (!options.get) die(31);
    this.derivation = options.get;
    this.name_ = options.name || "ComputedValue@" + getNextId();
    if (options.set) this.setter_ = createAction(this.name_ + "-setter", options.set);
    this.equals_ = options.equals || (options.compareStructural || options.struct ? comparer.structural : comparer["default"]);
    this.scope_ = options.context;
    this.requiresReaction_ = !!options.requiresReaction;
    this.keepAlive_ = !!options.keepAlive;
  }

  var _proto = ComputedValue.prototype;

  _proto.onBecomeStale_ = function onBecomeStale_() {
    propagateMaybeChanged(this);
  };

  _proto.onBO = function onBO() {
    if (this.onBOL) {
      this.onBOL.forEach(function (listener) {
        return listener();
      });
    }
  };

  _proto.onBUO = function onBUO() {
    if (this.onBUOL) {
      this.onBUOL.forEach(function (listener) {
        return listener();
      });
    }
  }
  /**
   * Returns the current value of this computed value.
   * Will evaluate its computation first if needed.
   */
  ;

  _proto.get = function get() {
    if (this.isComputing_) die(32, this.name_, this.derivation);

    if (globalState.inBatch === 0 && // !globalState.trackingDerivatpion &&
    this.observers_.size === 0 && !this.keepAlive_) {
      if (shouldCompute(this)) {
        this.warnAboutUntrackedRead_();
        startBatch(); // See perf test 'computed memoization'

        this.value_ = this.computeValue_(false);
        endBatch();
      }
    } else {
      reportObserved(this);

      if (shouldCompute(this)) {
        var prevTrackingContext = globalState.trackingContext;
        if (this.keepAlive_ && !prevTrackingContext) globalState.trackingContext = this;
        if (this.trackAndCompute()) propagateChangeConfirmed(this);
        globalState.trackingContext = prevTrackingContext;
      }
    }

    var result = this.value_;
    if (isCaughtException(result)) throw result.cause;
    return result;
  };

  _proto.set = function set(value) {
    if (this.setter_) {
      if (this.isRunningSetter_) die(33, this.name_);
      this.isRunningSetter_ = true;

      try {
        this.setter_.call(this.scope_, value);
      } finally {
        this.isRunningSetter_ = false;
      }
    } else die(34, this.name_);
  };

  _proto.trackAndCompute = function trackAndCompute() {
    // N.B: unminified as it is used by MST
    var oldValue = this.value_;
    var wasSuspended =
    /* see #1208 */
    this.dependenciesState_ === IDerivationState_.NOT_TRACKING_;
    var newValue = this.computeValue_(true);

    if (false) {}

    var changed = wasSuspended || isCaughtException(oldValue) || isCaughtException(newValue) || !this.equals_(oldValue, newValue);

    if (changed) {
      this.value_ = newValue;
    }

    return changed;
  };

  _proto.computeValue_ = function computeValue_(track) {
    this.isComputing_ = true; // don't allow state changes during computation

    var prev = allowStateChangesStart(false);
    var res;

    if (track) {
      res = trackDerivedFunction(this, this.derivation, this.scope_);
    } else {
      if (globalState.disableErrorBoundaries === true) {
        res = this.derivation.call(this.scope_);
      } else {
        try {
          res = this.derivation.call(this.scope_);
        } catch (e) {
          res = new CaughtException(e);
        }
      }
    }

    allowStateChangesEnd(prev);
    this.isComputing_ = false;
    return res;
  };

  _proto.suspend_ = function suspend_() {
    if (!this.keepAlive_) {
      clearObserving(this);
      this.value_ = undefined; // don't hold on to computed value!
    }
  };

  _proto.observe_ = function observe_(listener, fireImmediately) {
    var _this = this;

    var firstTime = true;
    var prevValue = undefined;
    return autorun(function () {
      // TODO: why is this in a different place than the spyReport() function? in all other observables it's called in the same place
      var newValue = _this.get();

      if (!firstTime || fireImmediately) {
        var prevU = untrackedStart();
        listener({
          observableKind: "computed",
          debugObjectName: _this.name_,
          type: UPDATE,
          object: _this,
          newValue: newValue,
          oldValue: prevValue
        });
        untrackedEnd(prevU);
      }

      firstTime = false;
      prevValue = newValue;
    });
  };

  _proto.warnAboutUntrackedRead_ = function warnAboutUntrackedRead_() {
    if (true) return;

    if (this.requiresReaction_ === true) {
      die("[mobx] Computed value " + this.name_ + " is read outside a reactive context");
    }

    if (this.isTracing_ !== TraceMode.NONE) {
      console.log("[mobx.trace] '" + this.name_ + "' is being read outside a reactive context. Doing a full recompute");
    }

    if (globalState.computedRequiresReaction) {
      console.warn("[mobx] Computed value " + this.name_ + " is being read outside a reactive context. Doing a full recompute");
    }
  };

  _proto.toString = function toString() {
    return this.name_ + "[" + this.derivation.toString() + "]";
  };

  _proto.valueOf = function valueOf() {
    return toPrimitive(this.get());
  };

  _proto[_Symbol$toPrimitive$1] = function () {
    return this.valueOf();
  };

  return ComputedValue;
}();
var isComputedValue = /*#__PURE__*/createInstanceofPredicate("ComputedValue", ComputedValue);

var IDerivationState_;

(function (IDerivationState_) {
  // before being run or (outside batch and not being observed)
  // at this point derivation is not holding any data about dependency tree
  IDerivationState_[IDerivationState_["NOT_TRACKING_"] = -1] = "NOT_TRACKING_"; // no shallow dependency changed since last computation
  // won't recalculate derivation
  // this is what makes mobx fast

  IDerivationState_[IDerivationState_["UP_TO_DATE_"] = 0] = "UP_TO_DATE_"; // some deep dependency changed, but don't know if shallow dependency changed
  // will require to check first if UP_TO_DATE or POSSIBLY_STALE
  // currently only ComputedValue will propagate POSSIBLY_STALE
  //
  // having this state is second big optimization:
  // don't have to recompute on every dependency change, but only when it's needed

  IDerivationState_[IDerivationState_["POSSIBLY_STALE_"] = 1] = "POSSIBLY_STALE_"; // A shallow dependency has changed since last computation and the derivation
  // will need to recompute when it's needed next.

  IDerivationState_[IDerivationState_["STALE_"] = 2] = "STALE_";
})(IDerivationState_ || (IDerivationState_ = {}));

var TraceMode;

(function (TraceMode) {
  TraceMode[TraceMode["NONE"] = 0] = "NONE";
  TraceMode[TraceMode["LOG"] = 1] = "LOG";
  TraceMode[TraceMode["BREAK"] = 2] = "BREAK";
})(TraceMode || (TraceMode = {}));

var CaughtException = function CaughtException(cause) {
  this.cause = void 0;
  this.cause = cause; // Empty
};
function isCaughtException(e) {
  return e instanceof CaughtException;
}
/**
 * Finds out whether any dependency of the derivation has actually changed.
 * If dependenciesState is 1 then it will recalculate dependencies,
 * if any dependency changed it will propagate it by changing dependenciesState to 2.
 *
 * By iterating over the dependencies in the same order that they were reported and
 * stopping on the first change, all the recalculations are only called for ComputedValues
 * that will be tracked by derivation. That is because we assume that if the first x
 * dependencies of the derivation doesn't change then the derivation should run the same way
 * up until accessing x-th dependency.
 */

function shouldCompute(derivation) {
  switch (derivation.dependenciesState_) {
    case IDerivationState_.UP_TO_DATE_:
      return false;

    case IDerivationState_.NOT_TRACKING_:
    case IDerivationState_.STALE_:
      return true;

    case IDerivationState_.POSSIBLY_STALE_:
      {
        // state propagation can occur outside of action/reactive context #2195
        var prevAllowStateReads = allowStateReadsStart(true);
        var prevUntracked = untrackedStart(); // no need for those computeds to be reported, they will be picked up in trackDerivedFunction.

        var obs = derivation.observing_,
            l = obs.length;

        for (var i = 0; i < l; i++) {
          var obj = obs[i];

          if (isComputedValue(obj)) {
            if (globalState.disableErrorBoundaries) {
              obj.get();
            } else {
              try {
                obj.get();
              } catch (e) {
                // we are not interested in the value *or* exception at this moment, but if there is one, notify all
                untrackedEnd(prevUntracked);
                allowStateReadsEnd(prevAllowStateReads);
                return true;
              }
            } // if ComputedValue `obj` actually changed it will be computed and propagated to its observers.
            // and `derivation` is an observer of `obj`
            // invariantShouldCompute(derivation)


            if (derivation.dependenciesState_ === IDerivationState_.STALE_) {
              untrackedEnd(prevUntracked);
              allowStateReadsEnd(prevAllowStateReads);
              return true;
            }
          }
        }

        changeDependenciesStateTo0(derivation);
        untrackedEnd(prevUntracked);
        allowStateReadsEnd(prevAllowStateReads);
        return false;
      }
  }
}
function isComputingDerivation() {
  return globalState.trackingDerivation !== null; // filter out actions inside computations
}
function checkIfStateModificationsAreAllowed(atom) {
  if (true) {
    return;
  }

  var hasObservers = atom.observers_.size > 0; // Should not be possible to change observed state outside strict mode, except during initialization, see #563

  if (!globalState.allowStateChanges && (hasObservers || globalState.enforceActions === "always")) console.warn("[MobX] " + (globalState.enforceActions ? "Since strict-mode is enabled, changing (observed) observable values without using an action is not allowed. Tried to modify: " : "Side effects like changing state are not allowed at this point. Are you trying to modify state from, for example, a computed value or the render function of a React component? You can wrap side effects in 'runInAction' (or decorate functions with 'action') if needed. Tried to modify: ") + atom.name_);
}
function checkIfStateReadsAreAllowed(observable) {
  if (false) {}
}
/**
 * Executes the provided function `f` and tracks which observables are being accessed.
 * The tracking information is stored on the `derivation` object and the derivation is registered
 * as observer of any of the accessed observables.
 */

function trackDerivedFunction(derivation, f, context) {
  var prevAllowStateReads = allowStateReadsStart(true); // pre allocate array allocation + room for variation in deps
  // array will be trimmed by bindDependencies

  changeDependenciesStateTo0(derivation);
  derivation.newObserving_ = new Array(derivation.observing_.length + 100);
  derivation.unboundDepsCount_ = 0;
  derivation.runId_ = ++globalState.runId;
  var prevTracking = globalState.trackingDerivation;
  globalState.trackingDerivation = derivation;
  globalState.inBatch++;
  var result;

  if (globalState.disableErrorBoundaries === true) {
    result = f.call(context);
  } else {
    try {
      result = f.call(context);
    } catch (e) {
      result = new CaughtException(e);
    }
  }

  globalState.inBatch--;
  globalState.trackingDerivation = prevTracking;
  bindDependencies(derivation);
  warnAboutDerivationWithoutDependencies(derivation);
  allowStateReadsEnd(prevAllowStateReads);
  return result;
}

function warnAboutDerivationWithoutDependencies(derivation) {
  if (true) return;
  if (derivation.observing_.length !== 0) return;

  if (globalState.reactionRequiresObservable || derivation.requiresObservable_) {
    console.warn("[mobx] Derivation " + derivation.name_ + " is created/updated without reading any observable value");
  }
}
/**
 * diffs newObserving with observing.
 * update observing to be newObserving with unique observables
 * notify observers that become observed/unobserved
 */


function bindDependencies(derivation) {
  // invariant(derivation.dependenciesState !== IDerivationState.NOT_TRACKING, "INTERNAL ERROR bindDependencies expects derivation.dependenciesState !== -1");
  var prevObserving = derivation.observing_;
  var observing = derivation.observing_ = derivation.newObserving_;
  var lowestNewObservingDerivationState = IDerivationState_.UP_TO_DATE_; // Go through all new observables and check diffValue: (this list can contain duplicates):
  //   0: first occurrence, change to 1 and keep it
  //   1: extra occurrence, drop it

  var i0 = 0,
      l = derivation.unboundDepsCount_;

  for (var i = 0; i < l; i++) {
    var dep = observing[i];

    if (dep.diffValue_ === 0) {
      dep.diffValue_ = 1;
      if (i0 !== i) observing[i0] = dep;
      i0++;
    } // Upcast is 'safe' here, because if dep is IObservable, `dependenciesState` will be undefined,
    // not hitting the condition


    if (dep.dependenciesState_ > lowestNewObservingDerivationState) {
      lowestNewObservingDerivationState = dep.dependenciesState_;
    }
  }

  observing.length = i0;
  derivation.newObserving_ = null; // newObserving shouldn't be needed outside tracking (statement moved down to work around FF bug, see #614)
  // Go through all old observables and check diffValue: (it is unique after last bindDependencies)
  //   0: it's not in new observables, unobserve it
  //   1: it keeps being observed, don't want to notify it. change to 0

  l = prevObserving.length;

  while (l--) {
    var _dep = prevObserving[l];

    if (_dep.diffValue_ === 0) {
      removeObserver(_dep, derivation);
    }

    _dep.diffValue_ = 0;
  } // Go through all new observables and check diffValue: (now it should be unique)
  //   0: it was set to 0 in last loop. don't need to do anything.
  //   1: it wasn't observed, let's observe it. set back to 0


  while (i0--) {
    var _dep2 = observing[i0];

    if (_dep2.diffValue_ === 1) {
      _dep2.diffValue_ = 0;
      addObserver(_dep2, derivation);
    }
  } // Some new observed derivations may become stale during this derivation computation
  // so they have had no chance to propagate staleness (#916)


  if (lowestNewObservingDerivationState !== IDerivationState_.UP_TO_DATE_) {
    derivation.dependenciesState_ = lowestNewObservingDerivationState;
    derivation.onBecomeStale_();
  }
}

function clearObserving(derivation) {
  // invariant(globalState.inBatch > 0, "INTERNAL ERROR clearObserving should be called only inside batch");
  var obs = derivation.observing_;
  derivation.observing_ = [];
  var i = obs.length;

  while (i--) {
    removeObserver(obs[i], derivation);
  }

  derivation.dependenciesState_ = IDerivationState_.NOT_TRACKING_;
}
function untracked(action) {
  var prev = untrackedStart();

  try {
    return action();
  } finally {
    untrackedEnd(prev);
  }
}
function untrackedStart() {
  var prev = globalState.trackingDerivation;
  globalState.trackingDerivation = null;
  return prev;
}
function untrackedEnd(prev) {
  globalState.trackingDerivation = prev;
}
function allowStateReadsStart(allowStateReads) {
  var prev = globalState.allowStateReads;
  globalState.allowStateReads = allowStateReads;
  return prev;
}
function allowStateReadsEnd(prev) {
  globalState.allowStateReads = prev;
}
/**
 * needed to keep `lowestObserverState` correct. when changing from (2 or 1) to 0
 *
 */

function changeDependenciesStateTo0(derivation) {
  if (derivation.dependenciesState_ === IDerivationState_.UP_TO_DATE_) return;
  derivation.dependenciesState_ = IDerivationState_.UP_TO_DATE_;
  var obs = derivation.observing_;
  var i = obs.length;

  while (i--) {
    obs[i].lowestObserverState_ = IDerivationState_.UP_TO_DATE_;
  }
}

/**
 * These values will persist if global state is reset
 */

var persistentKeys = ["mobxGuid", "spyListeners", "enforceActions", "computedRequiresReaction", "reactionRequiresObservable", "observableRequiresReaction", "allowStateReads", "disableErrorBoundaries", "runId", "UNCHANGED", "useProxies"];
var MobXGlobals = function MobXGlobals() {
  this.version = 6;
  this.UNCHANGED = {};
  this.trackingDerivation = null;
  this.trackingContext = null;
  this.runId = 0;
  this.mobxGuid = 0;
  this.inBatch = 0;
  this.pendingUnobservations = [];
  this.pendingReactions = [];
  this.isRunningReactions = false;
  this.allowStateChanges = false;
  this.allowStateReads = true;
  this.enforceActions = true;
  this.spyListeners = [];
  this.globalReactionErrorHandlers = [];
  this.computedRequiresReaction = false;
  this.reactionRequiresObservable = false;
  this.observableRequiresReaction = false;
  this.disableErrorBoundaries = false;
  this.suppressReactionErrors = false;
  this.useProxies = true;
  this.verifyProxies = false;
};
var canMergeGlobalState = true;
var isolateCalled = false;
var globalState = /*#__PURE__*/function () {
  var global = /*#__PURE__*/getGlobal();
  if (global.__mobxInstanceCount > 0 && !global.__mobxGlobals) canMergeGlobalState = false;
  if (global.__mobxGlobals && global.__mobxGlobals.version !== new MobXGlobals().version) canMergeGlobalState = false;

  if (!canMergeGlobalState) {
    setTimeout(function () {
      if (!isolateCalled) {
        die(35);
      }
    }, 1);
    return new MobXGlobals();
  } else if (global.__mobxGlobals) {
    global.__mobxInstanceCount += 1;
    if (!global.__mobxGlobals.UNCHANGED) global.__mobxGlobals.UNCHANGED = {}; // make merge backward compatible

    return global.__mobxGlobals;
  } else {
    global.__mobxInstanceCount = 1;
    return global.__mobxGlobals = /*#__PURE__*/new MobXGlobals();
  }
}();
function isolateGlobalState() {
  if (globalState.pendingReactions.length || globalState.inBatch || globalState.isRunningReactions) die(36);
  isolateCalled = true;

  if (canMergeGlobalState) {
    var global = getGlobal();
    if (--global.__mobxInstanceCount === 0) global.__mobxGlobals = undefined;
    globalState = new MobXGlobals();
  }
}
function getGlobalState() {
  return globalState;
}
/**
 * For testing purposes only; this will break the internal state of existing observables,
 * but can be used to get back at a stable state after throwing errors
 */

function resetGlobalState() {
  var defaultGlobals = new MobXGlobals();

  for (var key in defaultGlobals) {
    if (persistentKeys.indexOf(key) === -1) globalState[key] = defaultGlobals[key];
  }

  globalState.allowStateChanges = !globalState.enforceActions;
}

function hasObservers(observable) {
  return observable.observers_ && observable.observers_.size > 0;
}
function getObservers(observable) {
  return observable.observers_;
} // function invariantObservers(observable: IObservable) {
//     const list = observable.observers
//     const map = observable.observersIndexes
//     const l = list.length
//     for (let i = 0; i < l; i++) {
//         const id = list[i].__mapid
//         if (i) {
//             invariant(map[id] === i, "INTERNAL ERROR maps derivation.__mapid to index in list") // for performance
//         } else {
//             invariant(!(id in map), "INTERNAL ERROR observer on index 0 shouldn't be held in map.") // for performance
//         }
//     }
//     invariant(
//         list.length === 0 || Object.keys(map).length === list.length - 1,
//         "INTERNAL ERROR there is no junk in map"
//     )
// }

function addObserver(observable, node) {
  // invariant(node.dependenciesState !== -1, "INTERNAL ERROR, can add only dependenciesState !== -1");
  // invariant(observable._observers.indexOf(node) === -1, "INTERNAL ERROR add already added node");
  // invariantObservers(observable);
  observable.observers_.add(node);
  if (observable.lowestObserverState_ > node.dependenciesState_) observable.lowestObserverState_ = node.dependenciesState_; // invariantObservers(observable);
  // invariant(observable._observers.indexOf(node) !== -1, "INTERNAL ERROR didn't add node");
}
function removeObserver(observable, node) {
  // invariant(globalState.inBatch > 0, "INTERNAL ERROR, remove should be called only inside batch");
  // invariant(observable._observers.indexOf(node) !== -1, "INTERNAL ERROR remove already removed node");
  // invariantObservers(observable);
  observable.observers_["delete"](node);

  if (observable.observers_.size === 0) {
    // deleting last observer
    queueForUnobservation(observable);
  } // invariantObservers(observable);
  // invariant(observable._observers.indexOf(node) === -1, "INTERNAL ERROR remove already removed node2");

}
function queueForUnobservation(observable) {
  if (observable.isPendingUnobservation_ === false) {
    // invariant(observable._observers.length === 0, "INTERNAL ERROR, should only queue for unobservation unobserved observables");
    observable.isPendingUnobservation_ = true;
    globalState.pendingUnobservations.push(observable);
  }
}
/**
 * Batch starts a transaction, at least for purposes of memoizing ComputedValues when nothing else does.
 * During a batch `onBecomeUnobserved` will be called at most once per observable.
 * Avoids unnecessary recalculations.
 */

function startBatch() {
  globalState.inBatch++;
}
function endBatch() {
  if (--globalState.inBatch === 0) {
    runReactions(); // the batch is actually about to finish, all unobserving should happen here.

    var list = globalState.pendingUnobservations;

    for (var i = 0; i < list.length; i++) {
      var observable = list[i];
      observable.isPendingUnobservation_ = false;

      if (observable.observers_.size === 0) {
        if (observable.isBeingObserved_) {
          // if this observable had reactive observers, trigger the hooks
          observable.isBeingObserved_ = false;
          observable.onBUO();
        }

        if (observable instanceof ComputedValue) {
          // computed values are automatically teared down when the last observer leaves
          // this process happens recursively, this computed might be the last observabe of another, etc..
          observable.suspend_();
        }
      }
    }

    globalState.pendingUnobservations = [];
  }
}
function reportObserved(observable) {
  checkIfStateReadsAreAllowed(observable);
  var derivation = globalState.trackingDerivation;

  if (derivation !== null) {
    /**
     * Simple optimization, give each derivation run an unique id (runId)
     * Check if last time this observable was accessed the same runId is used
     * if this is the case, the relation is already known
     */
    if (derivation.runId_ !== observable.lastAccessedBy_) {
      observable.lastAccessedBy_ = derivation.runId_; // Tried storing newObserving, or observing, or both as Set, but performance didn't come close...

      derivation.newObserving_[derivation.unboundDepsCount_++] = observable;

      if (!observable.isBeingObserved_ && globalState.trackingContext) {
        observable.isBeingObserved_ = true;
        observable.onBO();
      }
    }

    return true;
  } else if (observable.observers_.size === 0 && globalState.inBatch > 0) {
    queueForUnobservation(observable);
  }

  return false;
} // function invariantLOS(observable: IObservable, msg: string) {
//     // it's expensive so better not run it in produciton. but temporarily helpful for testing
//     const min = getObservers(observable).reduce((a, b) => Math.min(a, b.dependenciesState), 2)
//     if (min >= observable.lowestObserverState) return // <- the only assumption about `lowestObserverState`
//     throw new Error(
//         "lowestObserverState is wrong for " +
//             msg +
//             " because " +
//             min +
//             " < " +
//             observable.lowestObserverState
//     )
// }

/**
 * NOTE: current propagation mechanism will in case of self reruning autoruns behave unexpectedly
 * It will propagate changes to observers from previous run
 * It's hard or maybe impossible (with reasonable perf) to get it right with current approach
 * Hopefully self reruning autoruns aren't a feature people should depend on
 * Also most basic use cases should be ok
 */
// Called by Atom when its value changes

function propagateChanged(observable) {
  // invariantLOS(observable, "changed start");
  if (observable.lowestObserverState_ === IDerivationState_.STALE_) return;
  observable.lowestObserverState_ = IDerivationState_.STALE_; // Ideally we use for..of here, but the downcompiled version is really slow...

  observable.observers_.forEach(function (d) {
    if (d.dependenciesState_ === IDerivationState_.UP_TO_DATE_) {
      if (false) {}

      d.onBecomeStale_();
    }

    d.dependenciesState_ = IDerivationState_.STALE_;
  }); // invariantLOS(observable, "changed end");
} // Called by ComputedValue when it recalculate and its value changed

function propagateChangeConfirmed(observable) {
  // invariantLOS(observable, "confirmed start");
  if (observable.lowestObserverState_ === IDerivationState_.STALE_) return;
  observable.lowestObserverState_ = IDerivationState_.STALE_;
  observable.observers_.forEach(function (d) {
    if (d.dependenciesState_ === IDerivationState_.POSSIBLY_STALE_) d.dependenciesState_ = IDerivationState_.STALE_;else if (d.dependenciesState_ === IDerivationState_.UP_TO_DATE_ // this happens during computing of `d`, just keep lowestObserverState up to date.
    ) observable.lowestObserverState_ = IDerivationState_.UP_TO_DATE_;
  }); // invariantLOS(observable, "confirmed end");
} // Used by computed when its dependency changed, but we don't wan't to immediately recompute.

function propagateMaybeChanged(observable) {
  // invariantLOS(observable, "maybe start");
  if (observable.lowestObserverState_ !== IDerivationState_.UP_TO_DATE_) return;
  observable.lowestObserverState_ = IDerivationState_.POSSIBLY_STALE_;
  observable.observers_.forEach(function (d) {
    if (d.dependenciesState_ === IDerivationState_.UP_TO_DATE_) {
      d.dependenciesState_ = IDerivationState_.POSSIBLY_STALE_;

      if (false) {}

      d.onBecomeStale_();
    }
  }); // invariantLOS(observable, "maybe end");
}

function logTraceInfo(derivation, observable) {
  console.log("[mobx.trace] '" + derivation.name_ + "' is invalidated due to a change in: '" + observable.name_ + "'");

  if (derivation.isTracing_ === TraceMode.BREAK) {
    var lines = [];
    printDepTree(getDependencyTree(derivation), lines, 1); // prettier-ignore

    new Function("debugger;\n/*\nTracing '" + derivation.name_ + "'\n\nYou are entering this break point because derivation '" + derivation.name_ + "' is being traced and '" + observable.name_ + "' is now forcing it to update.\nJust follow the stacktrace you should now see in the devtools to see precisely what piece of your code is causing this update\nThe stackframe you are looking for is at least ~6-8 stack-frames up.\n\n" + (derivation instanceof ComputedValue ? derivation.derivation.toString().replace(/[*]\//g, "/") : "") + "\n\nThe dependencies for this derivation are:\n\n" + lines.join("\n") + "\n*/\n    ")();
  }
}

function printDepTree(tree, lines, depth) {
  if (lines.length >= 1000) {
    lines.push("(and many more)");
    return;
  }

  lines.push("" + new Array(depth).join("\t") + tree.name); // MWE: not the fastest, but the easiest way :)

  if (tree.dependencies) tree.dependencies.forEach(function (child) {
    return printDepTree(child, lines, depth + 1);
  });
}

var Reaction = /*#__PURE__*/function () {
  // nodes we are looking at. Our value depends on these nodes
  function Reaction(name_, onInvalidate_, errorHandler_, requiresObservable_) {
    if (name_ === void 0) {
      name_ = "Reaction@" + getNextId();
    }

    if (requiresObservable_ === void 0) {
      requiresObservable_ = false;
    }

    this.name_ = void 0;
    this.onInvalidate_ = void 0;
    this.errorHandler_ = void 0;
    this.requiresObservable_ = void 0;
    this.observing_ = [];
    this.newObserving_ = [];
    this.dependenciesState_ = IDerivationState_.NOT_TRACKING_;
    this.diffValue_ = 0;
    this.runId_ = 0;
    this.unboundDepsCount_ = 0;
    this.mapid_ = "#" + getNextId();
    this.isDisposed_ = false;
    this.isScheduled_ = false;
    this.isTrackPending_ = false;
    this.isRunning_ = false;
    this.isTracing_ = TraceMode.NONE;
    this.name_ = name_;
    this.onInvalidate_ = onInvalidate_;
    this.errorHandler_ = errorHandler_;
    this.requiresObservable_ = requiresObservable_;
  }

  var _proto = Reaction.prototype;

  _proto.onBecomeStale_ = function onBecomeStale_() {
    this.schedule_();
  };

  _proto.schedule_ = function schedule_() {
    if (!this.isScheduled_) {
      this.isScheduled_ = true;
      globalState.pendingReactions.push(this);
      runReactions();
    }
  };

  _proto.isScheduled = function isScheduled() {
    return this.isScheduled_;
  }
  /**
   * internal, use schedule() if you intend to kick off a reaction
   */
  ;

  _proto.runReaction_ = function runReaction_() {
    if (!this.isDisposed_) {
      startBatch();
      this.isScheduled_ = false;

      if (shouldCompute(this)) {
        this.isTrackPending_ = true;

        try {
          this.onInvalidate_();

          if (false) {}
        } catch (e) {
          this.reportExceptionInDerivation_(e);
        }
      }

      endBatch();
    }
  };

  _proto.track = function track(fn) {
    if (this.isDisposed_) {
      return; // console.warn("Reaction already disposed") // Note: Not a warning / error in mobx 4 either
    }

    startBatch();
    var notify = isSpyEnabled();
    var startTime;

    if (false) {}

    this.isRunning_ = true;
    var prevReaction = globalState.trackingContext; // reactions could create reactions...

    globalState.trackingContext = this;
    var result = trackDerivedFunction(this, fn, undefined);
    globalState.trackingContext = prevReaction;
    this.isRunning_ = false;
    this.isTrackPending_ = false;

    if (this.isDisposed_) {
      // disposed during last run. Clean up everything that was bound after the dispose call.
      clearObserving(this);
    }

    if (isCaughtException(result)) this.reportExceptionInDerivation_(result.cause);

    if (false) {}

    endBatch();
  };

  _proto.reportExceptionInDerivation_ = function reportExceptionInDerivation_(error) {
    var _this = this;

    if (this.errorHandler_) {
      this.errorHandler_(error, this);
      return;
    }

    if (globalState.disableErrorBoundaries) throw error;
    var message =  false ? 0 : "[mobx] uncaught error in '" + this + "'";

    if (!globalState.suppressReactionErrors) {
      console.error(message, error);
      /** If debugging brought you here, please, read the above message :-). Tnx! */
    } else if (false) {} // prettier-ignore


    if (false) {}

    globalState.globalReactionErrorHandlers.forEach(function (f) {
      return f(error, _this);
    });
  };

  _proto.dispose = function dispose() {
    if (!this.isDisposed_) {
      this.isDisposed_ = true;

      if (!this.isRunning_) {
        // if disposed while running, clean up later. Maybe not optimal, but rare case
        startBatch();
        clearObserving(this);
        endBatch();
      }
    }
  };

  _proto.getDisposer_ = function getDisposer_() {
    var r = this.dispose.bind(this);
    r[$mobx] = this;
    return r;
  };

  _proto.toString = function toString() {
    return "Reaction[" + this.name_ + "]";
  };

  _proto.trace = function trace$1(enterBreakPoint) {
    if (enterBreakPoint === void 0) {
      enterBreakPoint = false;
    }

    trace(this, enterBreakPoint);
  };

  return Reaction;
}();
function onReactionError(handler) {
  globalState.globalReactionErrorHandlers.push(handler);
  return function () {
    var idx = globalState.globalReactionErrorHandlers.indexOf(handler);
    if (idx >= 0) globalState.globalReactionErrorHandlers.splice(idx, 1);
  };
}
/**
 * Magic number alert!
 * Defines within how many times a reaction is allowed to re-trigger itself
 * until it is assumed that this is gonna be a never ending loop...
 */

var MAX_REACTION_ITERATIONS = 100;

var reactionScheduler = function reactionScheduler(f) {
  return f();
};

function runReactions() {
  // Trampolining, if runReactions are already running, new reactions will be picked up
  if (globalState.inBatch > 0 || globalState.isRunningReactions) return;
  reactionScheduler(runReactionsHelper);
}

function runReactionsHelper() {
  globalState.isRunningReactions = true;
  var allReactions = globalState.pendingReactions;
  var iterations = 0; // While running reactions, new reactions might be triggered.
  // Hence we work with two variables and check whether
  // we converge to no remaining reactions after a while.

  while (allReactions.length > 0) {
    if (++iterations === MAX_REACTION_ITERATIONS) {
      console.error( false ? 0 : "[mobx] cycle in reaction: " + allReactions[0]);
      allReactions.splice(0); // clear reactions
    }

    var remainingReactions = allReactions.splice(0);

    for (var i = 0, l = remainingReactions.length; i < l; i++) {
      remainingReactions[i].runReaction_();
    }
  }

  globalState.isRunningReactions = false;
}

var isReaction = /*#__PURE__*/createInstanceofPredicate("Reaction", Reaction);
function setReactionScheduler(fn) {
  var baseScheduler = reactionScheduler;

  reactionScheduler = function reactionScheduler(f) {
    return fn(function () {
      return baseScheduler(f);
    });
  };
}

function isSpyEnabled() {
  return  false && 0;
}
function spyReport(event) {
  if (true) return; // dead code elimination can do the rest

  if (!globalState.spyListeners.length) return;
  var listeners = globalState.spyListeners;

  for (var i = 0, l = listeners.length; i < l; i++) {
    listeners[i](event);
  }
}
function spyReportStart(event) {
  if (true) return;

  var change = _extends({}, event, {
    spyReportStart: true
  });

  spyReport(change);
}
var END_EVENT = {
  type: "report-end",
  spyReportEnd: true
};
function spyReportEnd(change) {
  if (true) return;
  if (change) spyReport(_extends({}, change, {
    type: "report-end",
    spyReportEnd: true
  }));else spyReport(END_EVENT);
}
function spy(listener) {
  if (true) {
    console.warn("[mobx.spy] Is a no-op in production builds");
    return function () {};
  } else {}
}

var ACTION = "action";
var ACTION_BOUND = "action.bound";
var AUTOACTION = "autoAction";
var AUTOACTION_BOUND = "autoAction.bound";
var ACTION_UNNAMED = "<unnamed action>";

function createActionFactory(autoAction, annotation) {
  var res = function action(arg1, arg2) {
    // action(fn() {})
    if (isFunction(arg1)) return createAction(arg1.name || ACTION_UNNAMED, arg1, autoAction); // action("name", fn() {})

    if (isFunction(arg2)) return createAction(arg1, arg2, autoAction); // @action

    if (isStringish(arg2)) {
      return storeDecorator(arg1, arg2, annotation);
    } // Annation: action("name") & @action("name")


    if (isStringish(arg1)) {
      return createDecoratorAndAnnotation(annotation, arg1);
    }

    if (false) {}
  };

  res.annotationType_ = annotation;
  return res;
}

var action = /*#__PURE__*/createActionFactory(false, ACTION);
var autoAction = /*#__PURE__*/createActionFactory(true, AUTOACTION);
action.bound = /*#__PURE__*/createDecorator(ACTION_BOUND);
autoAction.bound = /*#__PURE__*/createDecorator(AUTOACTION_BOUND);
function runInAction(fn) {
  return executeAction(fn.name || ACTION_UNNAMED, false, fn, this, undefined);
}
function isAction(thing) {
  return isFunction(thing) && thing.isMobxAction === true;
}

/**
 * Creates a named reactive view and keeps it alive, so that the view is always
 * updated if one of the dependencies changes, even when the view is not further used by something else.
 * @param view The reactive view
 * @returns disposer function, which can be used to stop the view from being updated in the future.
 */

function autorun(view, opts) {
  if (opts === void 0) {
    opts = EMPTY_OBJECT;
  }

  if (false) {}

  var name = opts && opts.name || view.name || "Autorun@" + getNextId();
  var runSync = !opts.scheduler && !opts.delay;
  var reaction;

  if (runSync) {
    // normal autorun
    reaction = new Reaction(name, function () {
      this.track(reactionRunner);
    }, opts.onError, opts.requiresObservable);
  } else {
    var scheduler = createSchedulerFromOptions(opts); // debounced autorun

    var isScheduled = false;
    reaction = new Reaction(name, function () {
      if (!isScheduled) {
        isScheduled = true;
        scheduler(function () {
          isScheduled = false;
          if (!reaction.isDisposed_) reaction.track(reactionRunner);
        });
      }
    }, opts.onError, opts.requiresObservable);
  }

  function reactionRunner() {
    view(reaction);
  }

  reaction.schedule_();
  return reaction.getDisposer_();
}

var run = function run(f) {
  return f();
};

function createSchedulerFromOptions(opts) {
  return opts.scheduler ? opts.scheduler : opts.delay ? function (f) {
    return setTimeout(f, opts.delay);
  } : run;
}

function reaction(expression, effect, opts) {
  if (opts === void 0) {
    opts = EMPTY_OBJECT;
  }

  if (false) {}

  var name = opts.name || "Reaction@" + getNextId();
  var effectAction = action(name, opts.onError ? wrapErrorHandler(opts.onError, effect) : effect);
  var runSync = !opts.scheduler && !opts.delay;
  var scheduler = createSchedulerFromOptions(opts);
  var firstTime = true;
  var isScheduled = false;
  var value;
  var oldValue = undefined; // only an issue with fireImmediately

  var equals = opts.compareStructural ? comparer.structural : opts.equals || comparer["default"];
  var r = new Reaction(name, function () {
    if (firstTime || runSync) {
      reactionRunner();
    } else if (!isScheduled) {
      isScheduled = true;
      scheduler(reactionRunner);
    }
  }, opts.onError, opts.requiresObservable);

  function reactionRunner() {
    isScheduled = false;
    if (r.isDisposed_) return;
    var changed = false;
    r.track(function () {
      var nextValue = allowStateChanges(false, function () {
        return expression(r);
      });
      changed = firstTime || !equals(value, nextValue);
      oldValue = value;
      value = nextValue;
    });
    if (firstTime && opts.fireImmediately) effectAction(value, oldValue, r);else if (!firstTime && changed) effectAction(value, oldValue, r);
    firstTime = false;
  }

  r.schedule_();
  return r.getDisposer_();
}

function wrapErrorHandler(errorHandler, baseFn) {
  return function () {
    try {
      return baseFn.apply(this, arguments);
    } catch (e) {
      errorHandler.call(this, e);
    }
  };
}

var ON_BECOME_OBSERVED = "onBO";
var ON_BECOME_UNOBSERVED = "onBUO";
function onBecomeObserved(thing, arg2, arg3) {
  return interceptHook(ON_BECOME_OBSERVED, thing, arg2, arg3);
}
function onBecomeUnobserved(thing, arg2, arg3) {
  return interceptHook(ON_BECOME_UNOBSERVED, thing, arg2, arg3);
}

function interceptHook(hook, thing, arg2, arg3) {
  var atom = typeof arg3 === "function" ? getAtom(thing, arg2) : getAtom(thing);
  var cb = isFunction(arg3) ? arg3 : arg2;
  var listenersKey = hook + "L";

  if (atom[listenersKey]) {
    atom[listenersKey].add(cb);
  } else {
    atom[listenersKey] = new Set([cb]);
  }

  return function () {
    var hookListeners = atom[listenersKey];

    if (hookListeners) {
      hookListeners["delete"](cb);

      if (hookListeners.size === 0) {
        delete atom[listenersKey];
      }
    }
  };
}

var NEVER = "never";
var ALWAYS = "always";
var OBSERVED = "observed"; // const IF_AVAILABLE = "ifavailable"

function configure(options) {
  if (options.isolateGlobalState === true) {
    isolateGlobalState();
  }

  var useProxies = options.useProxies,
      enforceActions = options.enforceActions;

  if (useProxies !== undefined) {
    globalState.useProxies = useProxies === ALWAYS ? true : useProxies === NEVER ? false : typeof Proxy !== "undefined";
  }

  if (useProxies === "ifavailable") globalState.verifyProxies = true;

  if (enforceActions !== undefined) {
    var ea = enforceActions === ALWAYS ? ALWAYS : enforceActions === OBSERVED;
    globalState.enforceActions = ea;
    globalState.allowStateChanges = ea === true || ea === ALWAYS ? false : true;
  }
  ["computedRequiresReaction", "reactionRequiresObservable", "observableRequiresReaction", "disableErrorBoundaries"].forEach(function (key) {
    if (key in options) globalState[key] = !!options[key];
  });
  globalState.allowStateReads = !globalState.observableRequiresReaction;

  if (false) {}

  if (options.reactionScheduler) {
    setReactionScheduler(options.reactionScheduler);
  }
}

function extendObservable(target, properties, annotations, options) {
  if (false) {}

  var o = asCreateObservableOptions(options);
  var adm = asObservableObject(target, o.name, getEnhancerFromOption(o));
  startBatch();

  try {
    var descs = getOwnPropertyDescriptors(properties);
    getPlainObjectKeys(descs).forEach(function (key) {
      makeProperty(adm, target, key, descs[key], !annotations ? true : key in annotations ? annotations[key] : true, true, !!(options == null ? void 0 : options.autoBind));
    });
  } finally {
    endBatch();
  }

  return target;
}

function getDependencyTree(thing, property) {
  return nodeToDependencyTree(getAtom(thing, property));
}

function nodeToDependencyTree(node) {
  var result = {
    name: node.name_
  };
  if (node.observing_ && node.observing_.length > 0) result.dependencies = unique(node.observing_).map(nodeToDependencyTree);
  return result;
}

function getObserverTree(thing, property) {
  return nodeToObserverTree(getAtom(thing, property));
}

function nodeToObserverTree(node) {
  var result = {
    name: node.name_
  };
  if (hasObservers(node)) result.observers = Array.from(getObservers(node)).map(nodeToObserverTree);
  return result;
}

function unique(list) {
  return Array.from(new Set(list));
}

var FLOW = "flow";
var generatorId = 0;
function FlowCancellationError() {
  this.message = "FLOW_CANCELLED";
}
FlowCancellationError.prototype = /*#__PURE__*/Object.create(Error.prototype);
function isFlowCancellationError(error) {
  return error instanceof FlowCancellationError;
}
var flow = /*#__PURE__*/Object.assign(function flow(arg1, arg2) {
  // @flow
  if (isStringish(arg2)) {
    return storeDecorator(arg1, arg2, "flow");
  } // flow(fn)


  if (false) {}
  var generator = arg1;
  var name = generator.name || "<unnamed flow>"; // Implementation based on https://github.com/tj/co/blob/master/index.js

  var res = function res() {
    var ctx = this;
    var args = arguments;
    var runId = ++generatorId;
    var gen = action(name + " - runid: " + runId + " - init", generator).apply(ctx, args);
    var rejector;
    var pendingPromise = undefined;
    var promise = new Promise(function (resolve, reject) {
      var stepId = 0;
      rejector = reject;

      function onFulfilled(res) {
        pendingPromise = undefined;
        var ret;

        try {
          ret = action(name + " - runid: " + runId + " - yield " + stepId++, gen.next).call(gen, res);
        } catch (e) {
          return reject(e);
        }

        next(ret);
      }

      function onRejected(err) {
        pendingPromise = undefined;
        var ret;

        try {
          ret = action(name + " - runid: " + runId + " - yield " + stepId++, gen["throw"]).call(gen, err);
        } catch (e) {
          return reject(e);
        }

        next(ret);
      }

      function next(ret) {
        if (isFunction(ret == null ? void 0 : ret.then)) {
          // an async iterator
          ret.then(next, reject);
          return;
        }

        if (ret.done) return resolve(ret.value);
        pendingPromise = Promise.resolve(ret.value);
        return pendingPromise.then(onFulfilled, onRejected);
      }

      onFulfilled(undefined); // kick off the process
    });
    promise.cancel = action(name + " - runid: " + runId + " - cancel", function () {
      try {
        if (pendingPromise) cancelPromise(pendingPromise); // Finally block can return (or yield) stuff..

        var _res = gen["return"](undefined); // eat anything that promise would do, it's cancelled!


        var yieldedPromise = Promise.resolve(_res.value);
        yieldedPromise.then(noop, noop);
        cancelPromise(yieldedPromise); // maybe it can be cancelled :)
        // reject our original promise

        rejector(new FlowCancellationError());
      } catch (e) {
        rejector(e); // there could be a throwing finally block
      }
    });
    return promise;
  };

  res.isMobXFlow = true;
  return res;
}, {
  annotationType_: "flow"
});

function cancelPromise(promise) {
  if (isFunction(promise.cancel)) promise.cancel();
}

function flowResult(result) {
  return result; // just tricking TypeScript :)
}
function isFlow(fn) {
  return (fn == null ? void 0 : fn.isMobXFlow) === true;
}

function interceptReads(thing, propOrHandler, handler) {
  var target;

  if (isObservableMap(thing) || isObservableArray(thing) || isObservableValue(thing)) {
    target = getAdministration(thing);
  } else if (isObservableObject(thing)) {
    if (false) {}
    target = getAdministration(thing, propOrHandler);
  } else if (false) {}

  if (false) {}
  target.dehancer = typeof propOrHandler === "function" ? propOrHandler : handler;
  return function () {
    target.dehancer = undefined;
  };
}

function intercept(thing, propOrHandler, handler) {
  if (isFunction(handler)) return interceptProperty(thing, propOrHandler, handler);else return interceptInterceptable(thing, propOrHandler);
}

function interceptInterceptable(thing, handler) {
  return getAdministration(thing).intercept_(handler);
}

function interceptProperty(thing, property, handler) {
  return getAdministration(thing, property).intercept_(handler);
}

function _isComputed(value, property) {
  if (property !== undefined) {
    if (isObservableObject(value) === false) return false;
    if (!value[$mobx].values_.has(property)) return false;
    var atom = getAtom(value, property);
    return isComputedValue(atom);
  }

  return isComputedValue(value);
}
function isComputed(value) {
  if (false) {}
  return _isComputed(value);
}
function isComputedProp(value, propName) {
  if (false) {}
  return _isComputed(value, propName);
}

function _isObservable(value, property) {
  if (!value) return false;

  if (property !== undefined) {
    if (false) {}

    if (isObservableObject(value)) {
      return value[$mobx].values_.has(property);
    }

    return false;
  } // For first check, see #701


  return isObservableObject(value) || !!value[$mobx] || isAtom(value) || isReaction(value) || isComputedValue(value);
}

function isObservable(value) {
  if (false) {}
  return _isObservable(value);
}
function isObservableProp(value, propName) {
  if (false) {}
  return _isObservable(value, propName);
}

function keys(obj) {
  if (isObservableObject(obj)) {
    return obj[$mobx].getKeys_();
  }

  if (isObservableMap(obj) || isObservableSet(obj)) {
    return Array.from(obj.keys());
  }

  if (isObservableArray(obj)) {
    return obj.map(function (_, index) {
      return index;
    });
  }

  die(5);
}
function values(obj) {
  if (isObservableObject(obj)) {
    return keys(obj).map(function (key) {
      return obj[key];
    });
  }

  if (isObservableMap(obj)) {
    return keys(obj).map(function (key) {
      return obj.get(key);
    });
  }

  if (isObservableSet(obj)) {
    return Array.from(obj.values());
  }

  if (isObservableArray(obj)) {
    return obj.slice();
  }

  die(6);
}
function entries(obj) {
  if (isObservableObject(obj)) {
    return keys(obj).map(function (key) {
      return [key, obj[key]];
    });
  }

  if (isObservableMap(obj)) {
    return keys(obj).map(function (key) {
      return [key, obj.get(key)];
    });
  }

  if (isObservableSet(obj)) {
    return Array.from(obj.entries());
  }

  if (isObservableArray(obj)) {
    return obj.map(function (key, index) {
      return [index, key];
    });
  }

  die(7);
}
function set(obj, key, value) {
  if (arguments.length === 2 && !isObservableSet(obj)) {
    startBatch();
    var _values = key;

    try {
      for (var _key in _values) {
        set(obj, _key, _values[_key]);
      }
    } finally {
      endBatch();
    }

    return;
  }

  if (isObservableObject(obj)) {
    var adm = obj[$mobx];
    var existingObservable = adm.values_.get(key);

    if (existingObservable) {
      adm.write_(key, value);
    } else {
      adm.addObservableProp_(key, value, adm.defaultEnhancer_);
    }
  } else if (isObservableMap(obj)) {
    obj.set(key, value);
  } else if (isObservableSet(obj)) {
    obj.add(key);
  } else if (isObservableArray(obj)) {
    if (typeof key !== "number") key = parseInt(key, 10);
    if (key < 0) die("Invalid index: '" + key + "'");
    startBatch();
    if (key >= obj.length) obj.length = key + 1;
    obj[key] = value;
    endBatch();
  } else die(8);
}
function remove(obj, key) {
  if (isObservableObject(obj)) {
    obj[$mobx].remove_(key);
  } else if (isObservableMap(obj)) {
    obj["delete"](key);
  } else if (isObservableSet(obj)) {
    obj["delete"](key);
  } else if (isObservableArray(obj)) {
    if (typeof key !== "number") key = parseInt(key, 10);
    obj.splice(key, 1);
  } else {
    die(9);
  }
}
function has(obj, key) {
  if (isObservableObject(obj)) {
    // return keys(obj).indexOf(key) >= 0
    return getAdministration(obj).has_(key);
  } else if (isObservableMap(obj)) {
    return obj.has(key);
  } else if (isObservableSet(obj)) {
    return obj.has(key);
  } else if (isObservableArray(obj)) {
    return key >= 0 && key < obj.length;
  }

  die(10);
}
function get(obj, key) {
  if (!has(obj, key)) return undefined;

  if (isObservableObject(obj)) {
    return obj[key];
  } else if (isObservableMap(obj)) {
    return obj.get(key);
  } else if (isObservableArray(obj)) {
    return obj[key];
  }

  die(11);
}

function observe(thing, propOrCb, cbOrFire, fireImmediately) {
  if (isFunction(cbOrFire)) return observeObservableProperty(thing, propOrCb, cbOrFire, fireImmediately);else return observeObservable(thing, propOrCb, cbOrFire);
}

function observeObservable(thing, listener, fireImmediately) {
  return getAdministration(thing).observe_(listener, fireImmediately);
}

function observeObservableProperty(thing, property, listener, fireImmediately) {
  return getAdministration(thing, property).observe_(listener, fireImmediately);
}

function cache(map, key, value) {
  map.set(key, value);
  return value;
}

function toJSHelper(source, __alreadySeen) {
  if (source == null || typeof source !== "object" || source instanceof Date || !isObservable(source)) return source;
  if (isObservableValue(source)) return toJSHelper(source.get(), __alreadySeen);

  if (__alreadySeen.has(source)) {
    return __alreadySeen.get(source);
  }

  if (isObservableArray(source)) {
    var res = cache(__alreadySeen, source, new Array(source.length));
    source.forEach(function (value, idx) {
      res[idx] = toJSHelper(value, __alreadySeen);
    });
    return res;
  }

  if (isObservableSet(source)) {
    var _res = cache(__alreadySeen, source, new Set());

    source.forEach(function (value) {
      _res.add(toJSHelper(value, __alreadySeen));
    });
    return _res;
  }

  if (isObservableMap(source)) {
    var _res2 = cache(__alreadySeen, source, new Map());

    source.forEach(function (value, key) {
      _res2.set(key, toJSHelper(value, __alreadySeen));
    });
    return _res2;
  } else {
    // must be observable object
    keys(source); // make sure keys are observed

    var _res3 = cache(__alreadySeen, source, {});

    getPlainObjectKeys(source).forEach(function (key) {
      _res3[key] = toJSHelper(source[key], __alreadySeen);
    });
    return _res3;
  }
}
/**
 * Basically, a deep clone, so that no reactive property will exist anymore.
 */


function toJS(source, options) {
  if (false) {}
  return toJSHelper(source, new Map());
}

function trace() {
  if (true) die("trace() is not available in production builds");
  var enterBreakPoint = false;

  for (var _len = arguments.length, args = new Array(_len), _key = 0; _key < _len; _key++) {
    args[_key] = arguments[_key];
  }

  if (typeof args[args.length - 1] === "boolean") enterBreakPoint = args.pop();
  var derivation = getAtomFromArgs(args);

  if (!derivation) {
    return die("'trace(break?)' can only be used inside a tracked computed value or a Reaction. Consider passing in the computed value or reaction explicitly");
  }

  if (derivation.isTracing_ === TraceMode.NONE) {
    console.log("[mobx.trace] '" + derivation.name_ + "' tracing enabled");
  }

  derivation.isTracing_ = enterBreakPoint ? TraceMode.BREAK : TraceMode.LOG;
}

function getAtomFromArgs(args) {
  switch (args.length) {
    case 0:
      return globalState.trackingDerivation;

    case 1:
      return getAtom(args[0]);

    case 2:
      return getAtom(args[0], args[1]);
  }
}

/**
 * During a transaction no views are updated until the end of the transaction.
 * The transaction will be run synchronously nonetheless.
 *
 * @param action a function that updates some reactive state
 * @returns any value that was returned by the 'action' parameter.
 */

function transaction(action, thisArg) {
  if (thisArg === void 0) {
    thisArg = undefined;
  }

  startBatch();

  try {
    return action.apply(thisArg);
  } finally {
    endBatch();
  }
}

function when(predicate, arg1, arg2) {
  if (arguments.length === 1 || arg1 && typeof arg1 === "object") return whenPromise(predicate, arg1);
  return _when(predicate, arg1, arg2 || {});
}

function _when(predicate, effect, opts) {
  var timeoutHandle;

  if (typeof opts.timeout === "number") {
    timeoutHandle = setTimeout(function () {
      if (!disposer[$mobx].isDisposed_) {
        disposer();
        var error = new Error("WHEN_TIMEOUT");
        if (opts.onError) opts.onError(error);else throw error;
      }
    }, opts.timeout);
  }

  opts.name = opts.name || "When@" + getNextId();
  var effectAction = createAction(opts.name + "-effect", effect); // eslint-disable-next-line

  var disposer = autorun(function (r) {
    // predicate should not change state
    var cond = allowStateChanges(false, predicate);

    if (cond) {
      r.dispose();
      if (timeoutHandle) clearTimeout(timeoutHandle);
      effectAction();
    }
  }, opts);
  return disposer;
}

function whenPromise(predicate, opts) {
  if (false) {}
  var cancel;
  var res = new Promise(function (resolve, reject) {
    var disposer = _when(predicate, resolve, _extends({}, opts, {
      onError: reject
    }));

    cancel = function cancel() {
      disposer();
      reject("WHEN_CANCELLED");
    };
  });
  res.cancel = cancel;
  return res;
}

function getAdm(target) {
  return target[$mobx];
} // Optimization: we don't need the intermediate objects and could have a completely custom administration for DynamicObjects,
// and skip either the internal values map, or the base object with its property descriptors!


var objectProxyTraps = {
  has: function has(target, name) {
    if (name === $mobx || name === "constructor") return true;
    if (false) {}
    var adm = getAdm(target); // MWE: should `in` operator be reactive? If not, below code path will be faster / more memory efficient
    // check performance stats!
    // if (adm.values.get(name as string)) return true

    if (isStringish(name)) return adm.has_(name);
    return name in target;
  },
  get: function get(target, name) {
    if (name === $mobx || name === "constructor") return target[name];
    var adm = getAdm(target);
    var observable = adm.values_.get(name);

    if (observable instanceof Atom) {
      var result = observable.get();

      if (result === undefined) {
        // This fixes #1796, because deleting a prop that has an
        // undefined value won't retrigger a observer (no visible effect),
        // the autorun wouldn't subscribe to future key changes (see also next comment)
        adm.has_(name);
      }

      return result;
    } // make sure we start listening to future keys
    // note that we only do this here for optimization


    if (isStringish(name)) adm.has_(name);
    return target[name];
  },
  set: function set$1(target, name, value) {
    if (!isStringish(name)) return false;

    if (false) {}

    set(target, name, value);

    return true;
  },
  deleteProperty: function deleteProperty(target, name) {
    if (false) {}
    if (!isStringish(name)) return false;
    var adm = getAdm(target);
    adm.remove_(name);
    return true;
  },
  ownKeys: function ownKeys(target) {
    if (false) {}
    var adm = getAdm(target);
    adm.keysAtom_.reportObserved();
    return Reflect.ownKeys(target);
  },
  preventExtensions: function preventExtensions(target) {
    die(13);
  }
};
function createDynamicObservableObject(base) {
  assertProxies();
  var proxy = new Proxy(base, objectProxyTraps);
  base[$mobx].proxy_ = proxy;
  return proxy;
}

function hasInterceptors(interceptable) {
  return interceptable.interceptors_ !== undefined && interceptable.interceptors_.length > 0;
}
function registerInterceptor(interceptable, handler) {
  var interceptors = interceptable.interceptors_ || (interceptable.interceptors_ = []);
  interceptors.push(handler);
  return once(function () {
    var idx = interceptors.indexOf(handler);
    if (idx !== -1) interceptors.splice(idx, 1);
  });
}
function interceptChange(interceptable, change) {
  var prevU = untrackedStart();

  try {
    // Interceptor can modify the array, copy it to avoid concurrent modification, see #1950
    var interceptors = [].concat(interceptable.interceptors_ || []);

    for (var i = 0, l = interceptors.length; i < l; i++) {
      change = interceptors[i](change);
      if (change && !change.type) die(14);
      if (!change) break;
    }

    return change;
  } finally {
    untrackedEnd(prevU);
  }
}

function hasListeners(listenable) {
  return listenable.changeListeners_ !== undefined && listenable.changeListeners_.length > 0;
}
function registerListener(listenable, handler) {
  var listeners = listenable.changeListeners_ || (listenable.changeListeners_ = []);
  listeners.push(handler);
  return once(function () {
    var idx = listeners.indexOf(handler);
    if (idx !== -1) listeners.splice(idx, 1);
  });
}
function notifyListeners(listenable, change) {
  var prevU = untrackedStart();
  var listeners = listenable.changeListeners_;
  if (!listeners) return;
  listeners = listeners.slice();

  for (var i = 0, l = listeners.length; i < l; i++) {
    listeners[i](change);
  }

  untrackedEnd(prevU);
}

var CACHED_ANNOTATIONS = /*#__PURE__*/Symbol("mobx-cached-annotations");

function makeAction(target, key, name, fn, asAutoAction) {
  addHiddenProp(target, key, asAutoAction ? autoAction(name || key, fn) : action(name || key, fn));
}

function getInferredAnnotation(desc, defaultAnnotation, autoBind) {
  if (desc.get) return computed;
  if (desc.set) return false; // ignore pure setters
  // if already wrapped in action, don't do that another time, but assume it is already set up properly

  if (isFunction(desc.value)) return isGenerator(desc.value) ? flow : isAction(desc.value) ? false : autoBind ? autoAction.bound : autoAction; // if (!desc.configurable || !desc.writable) return false

  return defaultAnnotation != null ? defaultAnnotation : observable.deep;
}

function getDescriptorInChain(target, prop) {
  var current = target;

  while (current && current !== objectPrototype) {
    // Optimization: cache meta data, especially for members from prototypes?
    var desc = getDescriptor(current, prop);

    if (desc) {
      return [desc, current];
    }

    current = Object.getPrototypeOf(current);
  }

  die(1, prop);
}

function makeProperty(adm, owner, key, descriptor, annotation, forceCopy, // extend observable will copy even unannotated properties
autoBind) {
  var _annotation$annotatio;

  var target = adm.target_;
  var defaultAnnotation = observable; // ideally grap this from adm's defaultEnahncer instead!

  var originAnnotation = annotation;

  if (annotation === true) {
    annotation = getInferredAnnotation(descriptor, defaultAnnotation, autoBind);
  }

  if (annotation === false) {
    if (forceCopy) {
      defineProperty(target, key, descriptor);
    }

    return;
  }

  if (!annotation || annotation === true || !annotation.annotationType_) {
    return die(2, key);
  }

  var type = annotation.annotationType_;

  switch (type) {
    case AUTOACTION:
    case ACTION:
      {
        var fn = descriptor.value;
        if (!isFunction(fn)) die(3, key);

        if (owner !== target && !forceCopy) {
          if (!isAction(owner[key])) makeAction(owner, key, annotation.arg_, fn, type === AUTOACTION);
        } else {
          makeAction(target, key, annotation.arg_, fn, type === AUTOACTION);
        }

        break;
      }

    case AUTOACTION_BOUND:
    case ACTION_BOUND:
      {
        var _fn = descriptor.value;
        if (!isFunction(_fn)) die(3, key);
        makeAction(target, key, annotation.arg_, _fn.bind(adm.proxy_ || target), type === AUTOACTION_BOUND);
        break;
      }

    case FLOW:
      {
        if (owner !== target && !forceCopy) {
          if (!isFlow(owner[key])) addHiddenProp(owner, key, flow(descriptor.value));
        } else {
          addHiddenProp(target, key, flow(descriptor.value));
        }

        break;
      }

    case COMPUTED:
    case COMPUTED_STRUCT:
      {
        if (!descriptor.get) die(4, key);
        adm.addComputedProp_(target, key, _extends({
          get: descriptor.get,
          set: descriptor.set,
          compareStructural: annotation.annotationType_ === COMPUTED_STRUCT
        }, annotation.arg_));
        break;
      }

    case OBSERVABLE:
    case OBSERVABLE_REF:
    case OBSERVABLE_SHALLOW:
    case OBSERVABLE_STRUCT:
      {
        if (false) {}
        if (false) {} // if the originAnnotation was true, preferred the adm's default enhancer over the inferred one

        var enhancer = originAnnotation === true ? adm.defaultEnhancer_ : getEnhancerFromAnnotation(annotation);
        adm.addObservableProp_(key, descriptor.value, enhancer);
        break;
      }

    default:
      if (false) {}
  }
}
function makeObservable(target, annotations, options) {
  var autoBind = !!(options == null ? void 0 : options.autoBind);
  var adm = asObservableObject(target, options == null ? void 0 : options.name, getEnhancerFromAnnotation(options == null ? void 0 : options.defaultDecorator));
  startBatch();

  try {
    if (!annotations) {
      var didDecorate = applyDecorators(target);
      if (false) {}
      return target;
    }

    var make = function make(key) {
      var annotation = annotations[key];

      var _getDescriptorInChain = getDescriptorInChain(target, key),
          desc = _getDescriptorInChain[0],
          owner = _getDescriptorInChain[1];

      makeProperty(adm, owner, key, desc, annotation, false, autoBind);
    };

    ownKeys(annotations).forEach(make);
  } finally {
    endBatch();
  }

  return target;
}
function makeAutoObservable(target, overrides, options) {
  var proto = Object.getPrototypeOf(target);
  var isPlain = proto == null || proto === objectPrototype;

  if (false) {}

  var annotations;

  if (!isPlain && hasProp(proto, CACHED_ANNOTATIONS)) {
    // shortcut, reuse inferred annotations for this type from the previous time
    annotations = proto[CACHED_ANNOTATIONS];
  } else {
    annotations = _extends({}, overrides);
    extractAnnotationsFromObject(target, annotations, options);

    if (!isPlain) {
      extractAnnotationsFromProto(proto, annotations, options);
      addHiddenProp(proto, CACHED_ANNOTATIONS, annotations);
    }
  }

  makeObservable(target, annotations, options);
  return target;
}

function extractAnnotationsFromObject(target, collector, options) {
  var _options$defaultDecor;

  var autoBind = !!(options == null ? void 0 : options.autoBind);
  var defaultAnnotation = (options == null ? void 0 : options.deep) === undefined ? (_options$defaultDecor = options == null ? void 0 : options.defaultDecorator) != null ? _options$defaultDecor : observable.deep : (options == null ? void 0 : options.deep) ? observable.deep : observable.ref;
  Object.entries(getOwnPropertyDescriptors(target)).forEach(function (_ref) {
    var key = _ref[0],
        descriptor = _ref[1];
    if (key in collector || key === "constructor") return;
    collector[key] = getInferredAnnotation(descriptor, defaultAnnotation, autoBind);
  });
}

function extractAnnotationsFromProto(proto, collector, options) {
  Object.entries(getOwnPropertyDescriptors(proto)).forEach(function (_ref2) {
    var key = _ref2[0],
        prop = _ref2[1];
    if (key in collector || key === "constructor") return;

    if (prop.get) {
      collector[key] = computed;
    } else if (isFunction(prop.value)) {
      collector[key] = isGenerator(prop.value) ? flow : (options == null ? void 0 : options.autoBind) ? autoAction.bound : autoAction;
    }
  });
}

var SPLICE = "splice";
var UPDATE = "update";
var MAX_SPLICE_SIZE = 10000; // See e.g. https://github.com/mobxjs/mobx/issues/859

var arrayTraps = {
  get: function get(target, name) {
    var adm = target[$mobx];
    if (name === $mobx) return adm;
    if (name === "length") return adm.getArrayLength_();

    if (typeof name === "string" && !isNaN(name)) {
      return adm.get_(parseInt(name));
    }

    if (hasProp(arrayExtensions, name)) {
      return arrayExtensions[name];
    }

    return target[name];
  },
  set: function set(target, name, value) {
    var adm = target[$mobx];

    if (name === "length") {
      adm.setArrayLength_(value);
    }

    if (typeof name === "symbol" || isNaN(name)) {
      target[name] = value;
    } else {
      // numeric string
      adm.set_(parseInt(name), value);
    }

    return true;
  },
  preventExtensions: function preventExtensions() {
    die(15);
  }
};
var ObservableArrayAdministration = /*#__PURE__*/function () {
  // this is the prop that gets proxied, so can't replace it!
  function ObservableArrayAdministration(name, enhancer, owned_, legacyMode_) {
    this.owned_ = void 0;
    this.legacyMode_ = void 0;
    this.atom_ = void 0;
    this.values_ = [];
    this.interceptors_ = void 0;
    this.changeListeners_ = void 0;
    this.enhancer_ = void 0;
    this.dehancer = void 0;
    this.proxy_ = void 0;
    this.lastKnownLength_ = 0;
    this.owned_ = owned_;
    this.legacyMode_ = legacyMode_;
    this.atom_ = new Atom(name || "ObservableArray@" + getNextId());

    this.enhancer_ = function (newV, oldV) {
      return enhancer(newV, oldV, name + "[..]");
    };
  }

  var _proto = ObservableArrayAdministration.prototype;

  _proto.dehanceValue_ = function dehanceValue_(value) {
    if (this.dehancer !== undefined) return this.dehancer(value);
    return value;
  };

  _proto.dehanceValues_ = function dehanceValues_(values) {
    if (this.dehancer !== undefined && values.length > 0) return values.map(this.dehancer);
    return values;
  };

  _proto.intercept_ = function intercept_(handler) {
    return registerInterceptor(this, handler);
  };

  _proto.observe_ = function observe_(listener, fireImmediately) {
    if (fireImmediately === void 0) {
      fireImmediately = false;
    }

    if (fireImmediately) {
      listener({
        observableKind: "array",
        object: this.proxy_,
        debugObjectName: this.atom_.name_,
        type: "splice",
        index: 0,
        added: this.values_.slice(),
        addedCount: this.values_.length,
        removed: [],
        removedCount: 0
      });
    }

    return registerListener(this, listener);
  };

  _proto.getArrayLength_ = function getArrayLength_() {
    this.atom_.reportObserved();
    return this.values_.length;
  };

  _proto.setArrayLength_ = function setArrayLength_(newLength) {
    if (typeof newLength !== "number" || newLength < 0) die("Out of range: " + newLength);
    var currentLength = this.values_.length;
    if (newLength === currentLength) return;else if (newLength > currentLength) {
      var newItems = new Array(newLength - currentLength);

      for (var i = 0; i < newLength - currentLength; i++) {
        newItems[i] = undefined;
      } // No Array.fill everywhere...


      this.spliceWithArray_(currentLength, 0, newItems);
    } else this.spliceWithArray_(newLength, currentLength - newLength);
  };

  _proto.updateArrayLength_ = function updateArrayLength_(oldLength, delta) {
    if (oldLength !== this.lastKnownLength_) die(16);
    this.lastKnownLength_ += delta;
    if (this.legacyMode_ && delta > 0) reserveArrayBuffer(oldLength + delta + 1);
  };

  _proto.spliceWithArray_ = function spliceWithArray_(index, deleteCount, newItems) {
    var _this = this;

    checkIfStateModificationsAreAllowed(this.atom_);
    var length = this.values_.length;
    if (index === undefined) index = 0;else if (index > length) index = length;else if (index < 0) index = Math.max(0, length + index);
    if (arguments.length === 1) deleteCount = length - index;else if (deleteCount === undefined || deleteCount === null) deleteCount = 0;else deleteCount = Math.max(0, Math.min(deleteCount, length - index));
    if (newItems === undefined) newItems = EMPTY_ARRAY;

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        object: this.proxy_,
        type: SPLICE,
        index: index,
        removedCount: deleteCount,
        added: newItems
      });
      if (!change) return EMPTY_ARRAY;
      deleteCount = change.removedCount;
      newItems = change.added;
    }

    newItems = newItems.length === 0 ? newItems : newItems.map(function (v) {
      return _this.enhancer_(v, undefined);
    });

    if (this.legacyMode_ || "production" !== "production") {
      var lengthDelta = newItems.length - deleteCount;
      this.updateArrayLength_(length, lengthDelta); // checks if internal array wasn't modified
    }

    var res = this.spliceItemsIntoValues_(index, deleteCount, newItems);
    if (deleteCount !== 0 || newItems.length !== 0) this.notifyArraySplice_(index, newItems, res);
    return this.dehanceValues_(res);
  };

  _proto.spliceItemsIntoValues_ = function spliceItemsIntoValues_(index, deleteCount, newItems) {
    if (newItems.length < MAX_SPLICE_SIZE) {
      var _this$values_;

      return (_this$values_ = this.values_).splice.apply(_this$values_, [index, deleteCount].concat(newItems));
    } else {
      var res = this.values_.slice(index, index + deleteCount);
      var oldItems = this.values_.slice(index + deleteCount);
      this.values_.length = index + newItems.length - deleteCount;

      for (var i = 0; i < newItems.length; i++) {
        this.values_[index + i] = newItems[i];
      }

      for (var _i = 0; _i < oldItems.length; _i++) {
        this.values_[index + newItems.length + _i] = oldItems[_i];
      }

      return res;
    }
  };

  _proto.notifyArrayChildUpdate_ = function notifyArrayChildUpdate_(index, newValue, oldValue) {
    var notifySpy = !this.owned_ && isSpyEnabled();
    var notify = hasListeners(this);
    var change = notify || notifySpy ? {
      observableKind: "array",
      object: this.proxy_,
      type: UPDATE,
      debugObjectName: this.atom_.name_,
      index: index,
      newValue: newValue,
      oldValue: oldValue
    } : null; // The reason why this is on right hand side here (and not above), is this way the uglifier will drop it, but it won't
    // cause any runtime overhead in development mode without NODE_ENV set, unless spying is enabled

    if (false) {}
    this.atom_.reportChanged();
    if (notify) notifyListeners(this, change);
    if (false) {}
  };

  _proto.notifyArraySplice_ = function notifyArraySplice_(index, added, removed) {
    var notifySpy = !this.owned_ && isSpyEnabled();
    var notify = hasListeners(this);
    var change = notify || notifySpy ? {
      observableKind: "array",
      object: this.proxy_,
      debugObjectName: this.atom_.name_,
      type: SPLICE,
      index: index,
      removed: removed,
      added: added,
      removedCount: removed.length,
      addedCount: added.length
    } : null;
    if (false) {}
    this.atom_.reportChanged(); // conform: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Array/observe

    if (notify) notifyListeners(this, change);
    if (false) {}
  };

  _proto.get_ = function get_(index) {
    if (index < this.values_.length) {
      this.atom_.reportObserved();
      return this.dehanceValue_(this.values_[index]);
    }

    console.warn( false ? 0 : "[mobx.array] Attempt to read an array index (" + index + ") that is out of bounds (" + this.values_.length + "). Please check length first. Out of bound indices will not be tracked by MobX");
  };

  _proto.set_ = function set_(index, newValue) {
    var values = this.values_;

    if (index < values.length) {
      // update at index in range
      checkIfStateModificationsAreAllowed(this.atom_);
      var oldValue = values[index];

      if (hasInterceptors(this)) {
        var change = interceptChange(this, {
          type: UPDATE,
          object: this.proxy_,
          index: index,
          newValue: newValue
        });
        if (!change) return;
        newValue = change.newValue;
      }

      newValue = this.enhancer_(newValue, oldValue);
      var changed = newValue !== oldValue;

      if (changed) {
        values[index] = newValue;
        this.notifyArrayChildUpdate_(index, newValue, oldValue);
      }
    } else if (index === values.length) {
      // add a new item
      this.spliceWithArray_(index, 0, [newValue]);
    } else {
      // out of bounds
      die(17, index, values.length);
    }
  };

  return ObservableArrayAdministration;
}();
function createObservableArray(initialValues, enhancer, name, owned) {
  if (name === void 0) {
    name = "ObservableArray@" + getNextId();
  }

  if (owned === void 0) {
    owned = false;
  }

  assertProxies();
  var adm = new ObservableArrayAdministration(name, enhancer, owned, false);
  addHiddenFinalProp(adm.values_, $mobx, adm);
  var proxy = new Proxy(adm.values_, arrayTraps);
  adm.proxy_ = proxy;

  if (initialValues && initialValues.length) {
    var prev = allowStateChangesStart(true);
    adm.spliceWithArray_(0, 0, initialValues);
    allowStateChangesEnd(prev);
  }

  return proxy;
} // eslint-disable-next-line

var arrayExtensions = {
  clear: function clear() {
    return this.splice(0);
  },
  replace: function replace(newItems) {
    var adm = this[$mobx];
    return adm.spliceWithArray_(0, adm.values_.length, newItems);
  },
  // Used by JSON.stringify
  toJSON: function toJSON() {
    return this.slice();
  },

  /*
   * functions that do alter the internal structure of the array, (based on lib.es6.d.ts)
   * since these functions alter the inner structure of the array, the have side effects.
   * Because the have side effects, they should not be used in computed function,
   * and for that reason the do not call dependencyState.notifyObserved
   */
  splice: function splice(index, deleteCount) {
    for (var _len = arguments.length, newItems = new Array(_len > 2 ? _len - 2 : 0), _key = 2; _key < _len; _key++) {
      newItems[_key - 2] = arguments[_key];
    }

    var adm = this[$mobx];

    switch (arguments.length) {
      case 0:
        return [];

      case 1:
        return adm.spliceWithArray_(index);

      case 2:
        return adm.spliceWithArray_(index, deleteCount);
    }

    return adm.spliceWithArray_(index, deleteCount, newItems);
  },
  spliceWithArray: function spliceWithArray(index, deleteCount, newItems) {
    return this[$mobx].spliceWithArray_(index, deleteCount, newItems);
  },
  push: function push() {
    var adm = this[$mobx];

    for (var _len2 = arguments.length, items = new Array(_len2), _key2 = 0; _key2 < _len2; _key2++) {
      items[_key2] = arguments[_key2];
    }

    adm.spliceWithArray_(adm.values_.length, 0, items);
    return adm.values_.length;
  },
  pop: function pop() {
    return this.splice(Math.max(this[$mobx].values_.length - 1, 0), 1)[0];
  },
  shift: function shift() {
    return this.splice(0, 1)[0];
  },
  unshift: function unshift() {
    var adm = this[$mobx];

    for (var _len3 = arguments.length, items = new Array(_len3), _key3 = 0; _key3 < _len3; _key3++) {
      items[_key3] = arguments[_key3];
    }

    adm.spliceWithArray_(0, 0, items);
    return adm.values_.length;
  },
  reverse: function reverse() {
    // reverse by default mutates in place before returning the result
    // which makes it both a 'derivation' and a 'mutation'.
    if (globalState.trackingDerivation) {
      die(37, "reverse");
    }

    this.replace(this.slice().reverse());
    return this;
  },
  sort: function sort() {
    // sort by default mutates in place before returning the result
    // which goes against all good practices. Let's not change the array in place!
    if (globalState.trackingDerivation) {
      die(37, "sort");
    }

    var copy = this.slice();
    copy.sort.apply(copy, arguments);
    this.replace(copy);
    return this;
  },
  remove: function remove(value) {
    var adm = this[$mobx];
    var idx = adm.dehanceValues_(adm.values_).indexOf(value);

    if (idx > -1) {
      this.splice(idx, 1);
      return true;
    }

    return false;
  }
};
/**
 * Wrap function from prototype
 * Without this, everything works as well, but this works
 * faster as everything works on unproxied values
 */

addArrayExtension("concat", simpleFunc);
addArrayExtension("flat", simpleFunc);
addArrayExtension("includes", simpleFunc);
addArrayExtension("indexOf", simpleFunc);
addArrayExtension("join", simpleFunc);
addArrayExtension("lastIndexOf", simpleFunc);
addArrayExtension("slice", simpleFunc);
addArrayExtension("toString", simpleFunc);
addArrayExtension("toLocaleString", simpleFunc); // map

addArrayExtension("every", mapLikeFunc);
addArrayExtension("filter", mapLikeFunc);
addArrayExtension("find", mapLikeFunc);
addArrayExtension("findIndex", mapLikeFunc);
addArrayExtension("flatMap", mapLikeFunc);
addArrayExtension("forEach", mapLikeFunc);
addArrayExtension("map", mapLikeFunc);
addArrayExtension("some", mapLikeFunc); // reduce

addArrayExtension("reduce", reduceLikeFunc);
addArrayExtension("reduceRight", reduceLikeFunc);

function addArrayExtension(funcName, funcFactory) {
  if (typeof Array.prototype[funcName] === "function") {
    arrayExtensions[funcName] = funcFactory(funcName);
  }
} // Report and delegate to dehanced array


function simpleFunc(funcName) {
  return function () {
    var adm = this[$mobx];
    adm.atom_.reportObserved();
    var dehancedValues = adm.dehanceValues_(adm.values_);
    return dehancedValues[funcName].apply(dehancedValues, arguments);
  };
} // Make sure callbacks recieve correct array arg #2326


function mapLikeFunc(funcName) {
  return function (callback, thisArg) {
    var _this2 = this;

    var adm = this[$mobx];
    adm.atom_.reportObserved();
    var dehancedValues = adm.dehanceValues_(adm.values_);
    return dehancedValues[funcName](function (element, index) {
      return callback.call(thisArg, element, index, _this2);
    });
  };
} // Make sure callbacks recieve correct array arg #2326


function reduceLikeFunc(funcName) {
  return function () {
    var _this3 = this;

    var adm = this[$mobx];
    adm.atom_.reportObserved();
    var dehancedValues = adm.dehanceValues_(adm.values_); // #2432 - reduce behavior depends on arguments.length

    var callback = arguments[0];

    arguments[0] = function (accumulator, currentValue, index) {
      return callback(accumulator, currentValue, index, _this3);
    };

    return dehancedValues[funcName].apply(dehancedValues, arguments);
  };
}

var isObservableArrayAdministration = /*#__PURE__*/createInstanceofPredicate("ObservableArrayAdministration", ObservableArrayAdministration);
function isObservableArray(thing) {
  return isObject(thing) && isObservableArrayAdministration(thing[$mobx]);
}

var _Symbol$iterator, _Symbol$toStringTag;
var ObservableMapMarker = {};
var ADD = "add";
var DELETE = "delete"; // just extend Map? See also https://gist.github.com/nestharus/13b4d74f2ef4a2f4357dbd3fc23c1e54
// But: https://github.com/mobxjs/mobx/issues/1556

_Symbol$iterator = Symbol.iterator;
_Symbol$toStringTag = Symbol.toStringTag;
var ObservableMap = /*#__PURE__*/function () {
  // hasMap, not hashMap >-).
  function ObservableMap(initialData, enhancer_, name_) {
    if (enhancer_ === void 0) {
      enhancer_ = deepEnhancer;
    }

    if (name_ === void 0) {
      name_ = "ObservableMap@" + getNextId();
    }

    this.enhancer_ = void 0;
    this.name_ = void 0;
    this[$mobx] = ObservableMapMarker;
    this.data_ = void 0;
    this.hasMap_ = void 0;
    this.keysAtom_ = void 0;
    this.interceptors_ = void 0;
    this.changeListeners_ = void 0;
    this.dehancer = void 0;
    this.enhancer_ = enhancer_;
    this.name_ = name_;

    if (!isFunction(Map)) {
      die(18);
    }

    this.keysAtom_ = createAtom(this.name_ + ".keys()");
    this.data_ = new Map();
    this.hasMap_ = new Map();
    this.merge(initialData);
  }

  var _proto = ObservableMap.prototype;

  _proto.has_ = function has_(key) {
    return this.data_.has(key);
  };

  _proto.has = function has(key) {
    var _this = this;

    if (!globalState.trackingDerivation) return this.has_(key);
    var entry = this.hasMap_.get(key);

    if (!entry) {
      var newEntry = entry = new ObservableValue(this.has_(key), referenceEnhancer, this.name_ + "." + stringifyKey(key) + "?", false);
      this.hasMap_.set(key, newEntry);
      onBecomeUnobserved(newEntry, function () {
        return _this.hasMap_["delete"](key);
      });
    }

    return entry.get();
  };

  _proto.set = function set(key, value) {
    var hasKey = this.has_(key);

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        type: hasKey ? UPDATE : ADD,
        object: this,
        newValue: value,
        name: key
      });
      if (!change) return this;
      value = change.newValue;
    }

    if (hasKey) {
      this.updateValue_(key, value);
    } else {
      this.addValue_(key, value);
    }

    return this;
  };

  _proto["delete"] = function _delete(key) {
    var _this2 = this;

    checkIfStateModificationsAreAllowed(this.keysAtom_);

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        type: DELETE,
        object: this,
        name: key
      });
      if (!change) return false;
    }

    if (this.has_(key)) {
      var notifySpy = isSpyEnabled();
      var notify = hasListeners(this);

      var _change = notify || notifySpy ? {
        observableKind: "map",
        debugObjectName: this.name_,
        type: DELETE,
        object: this,
        oldValue: this.data_.get(key).value_,
        name: key
      } : null;

      if (false) {}
      transaction(function () {
        _this2.keysAtom_.reportChanged();

        _this2.updateHasMapEntry_(key, false);

        var observable = _this2.data_.get(key);

        observable.setNewValue_(undefined);

        _this2.data_["delete"](key);
      });
      if (notify) notifyListeners(this, _change);
      if (false) {}
      return true;
    }

    return false;
  };

  _proto.updateHasMapEntry_ = function updateHasMapEntry_(key, value) {
    var entry = this.hasMap_.get(key);

    if (entry) {
      entry.setNewValue_(value);
    }
  };

  _proto.updateValue_ = function updateValue_(key, newValue) {
    var observable = this.data_.get(key);
    newValue = observable.prepareNewValue_(newValue);

    if (newValue !== globalState.UNCHANGED) {
      var notifySpy = isSpyEnabled();
      var notify = hasListeners(this);
      var change = notify || notifySpy ? {
        observableKind: "map",
        debugObjectName: this.name_,
        type: UPDATE,
        object: this,
        oldValue: observable.value_,
        name: key,
        newValue: newValue
      } : null;
      if (false) {}
      observable.setNewValue_(newValue);
      if (notify) notifyListeners(this, change);
      if (false) {}
    }
  };

  _proto.addValue_ = function addValue_(key, newValue) {
    var _this3 = this;

    checkIfStateModificationsAreAllowed(this.keysAtom_);
    transaction(function () {
      var observable = new ObservableValue(newValue, _this3.enhancer_, _this3.name_ + "." + stringifyKey(key), false);

      _this3.data_.set(key, observable);

      newValue = observable.value_; // value might have been changed

      _this3.updateHasMapEntry_(key, true);

      _this3.keysAtom_.reportChanged();
    });
    var notifySpy = isSpyEnabled();
    var notify = hasListeners(this);
    var change = notify || notifySpy ? {
      observableKind: "map",
      debugObjectName: this.name_,
      type: ADD,
      object: this,
      name: key,
      newValue: newValue
    } : null;
    if (false) {}
    if (notify) notifyListeners(this, change);
    if (false) {}
  };

  _proto.get = function get(key) {
    if (this.has(key)) return this.dehanceValue_(this.data_.get(key).get());
    return this.dehanceValue_(undefined);
  };

  _proto.dehanceValue_ = function dehanceValue_(value) {
    if (this.dehancer !== undefined) {
      return this.dehancer(value);
    }

    return value;
  };

  _proto.keys = function keys() {
    this.keysAtom_.reportObserved();
    return this.data_.keys();
  };

  _proto.values = function values() {
    var self = this;
    var keys = this.keys();
    return makeIterable({
      next: function next() {
        var _keys$next = keys.next(),
            done = _keys$next.done,
            value = _keys$next.value;

        return {
          done: done,
          value: done ? undefined : self.get(value)
        };
      }
    });
  };

  _proto.entries = function entries() {
    var self = this;
    var keys = this.keys();
    return makeIterable({
      next: function next() {
        var _keys$next2 = keys.next(),
            done = _keys$next2.done,
            value = _keys$next2.value;

        return {
          done: done,
          value: done ? undefined : [value, self.get(value)]
        };
      }
    });
  };

  _proto[_Symbol$iterator] = function () {
    return this.entries();
  };

  _proto.forEach = function forEach(callback, thisArg) {
    for (var _iterator = _createForOfIteratorHelperLoose(this), _step; !(_step = _iterator()).done;) {
      var _step$value = _step.value,
          key = _step$value[0],
          value = _step$value[1];
      callback.call(thisArg, value, key, this);
    }
  }
  /** Merge another object into this object, returns this. */
  ;

  _proto.merge = function merge(other) {
    var _this4 = this;

    if (isObservableMap(other)) {
      other = new Map(other);
    }

    transaction(function () {
      if (isPlainObject(other)) getPlainObjectKeys(other).forEach(function (key) {
        return _this4.set(key, other[key]);
      });else if (Array.isArray(other)) other.forEach(function (_ref) {
        var key = _ref[0],
            value = _ref[1];
        return _this4.set(key, value);
      });else if (isES6Map(other)) {
        if (other.constructor !== Map) die(19, other);
        other.forEach(function (value, key) {
          return _this4.set(key, value);
        });
      } else if (other !== null && other !== undefined) die(20, other);
    });
    return this;
  };

  _proto.clear = function clear() {
    var _this5 = this;

    transaction(function () {
      untracked(function () {
        for (var _iterator2 = _createForOfIteratorHelperLoose(_this5.keys()), _step2; !(_step2 = _iterator2()).done;) {
          var key = _step2.value;

          _this5["delete"](key);
        }
      });
    });
  };

  _proto.replace = function replace(values) {
    var _this6 = this;

    // Implementation requirements:
    // - respect ordering of replacement map
    // - allow interceptors to run and potentially prevent individual operations
    // - don't recreate observables that already exist in original map (so we don't destroy existing subscriptions)
    // - don't _keysAtom.reportChanged if the keys of resulting map are indentical (order matters!)
    // - note that result map may differ from replacement map due to the interceptors
    transaction(function () {
      // Convert to map so we can do quick key lookups
      var replacementMap = convertToMap(values);
      var orderedData = new Map(); // Used for optimization

      var keysReportChangedCalled = false; // Delete keys that don't exist in replacement map
      // if the key deletion is prevented by interceptor
      // add entry at the beginning of the result map

      for (var _iterator3 = _createForOfIteratorHelperLoose(_this6.data_.keys()), _step3; !(_step3 = _iterator3()).done;) {
        var key = _step3.value;

        // Concurrently iterating/deleting keys
        // iterator should handle this correctly
        if (!replacementMap.has(key)) {
          var deleted = _this6["delete"](key); // Was the key removed?


          if (deleted) {
            // _keysAtom.reportChanged() was already called
            keysReportChangedCalled = true;
          } else {
            // Delete prevented by interceptor
            var value = _this6.data_.get(key);

            orderedData.set(key, value);
          }
        }
      } // Merge entries


      for (var _iterator4 = _createForOfIteratorHelperLoose(replacementMap.entries()), _step4; !(_step4 = _iterator4()).done;) {
        var _step4$value = _step4.value,
            _key = _step4$value[0],
            _value = _step4$value[1];

        // We will want to know whether a new key is added
        var keyExisted = _this6.data_.has(_key); // Add or update value


        _this6.set(_key, _value); // The addition could have been prevent by interceptor


        if (_this6.data_.has(_key)) {
          // The update could have been prevented by interceptor
          // and also we want to preserve existing values
          // so use value from _data map (instead of replacement map)
          var _value2 = _this6.data_.get(_key);

          orderedData.set(_key, _value2); // Was a new key added?

          if (!keyExisted) {
            // _keysAtom.reportChanged() was already called
            keysReportChangedCalled = true;
          }
        }
      } // Check for possible key order change


      if (!keysReportChangedCalled) {
        if (_this6.data_.size !== orderedData.size) {
          // If size differs, keys are definitely modified
          _this6.keysAtom_.reportChanged();
        } else {
          var iter1 = _this6.data_.keys();

          var iter2 = orderedData.keys();
          var next1 = iter1.next();
          var next2 = iter2.next();

          while (!next1.done) {
            if (next1.value !== next2.value) {
              _this6.keysAtom_.reportChanged();

              break;
            }

            next1 = iter1.next();
            next2 = iter2.next();
          }
        }
      } // Use correctly ordered map


      _this6.data_ = orderedData;
    });
    return this;
  };

  _proto.toString = function toString() {
    return "[object ObservableMap]";
  };

  _proto.toJSON = function toJSON() {
    return Array.from(this);
  };

  /**
   * Observes this object. Triggers for the events 'add', 'update' and 'delete'.
   * See: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/observe
   * for callback details
   */
  _proto.observe_ = function observe_(listener, fireImmediately) {
    if (false) {}
    return registerListener(this, listener);
  };

  _proto.intercept_ = function intercept_(handler) {
    return registerInterceptor(this, handler);
  };

  _createClass(ObservableMap, [{
    key: "size",
    get: function get() {
      this.keysAtom_.reportObserved();
      return this.data_.size;
    }
  }, {
    key: _Symbol$toStringTag,
    get: function get() {
      return "Map";
    }
  }]);

  return ObservableMap;
}(); // eslint-disable-next-line

var isObservableMap = /*#__PURE__*/createInstanceofPredicate("ObservableMap", ObservableMap);

function convertToMap(dataStructure) {
  if (isES6Map(dataStructure) || isObservableMap(dataStructure)) {
    return dataStructure;
  } else if (Array.isArray(dataStructure)) {
    return new Map(dataStructure);
  } else if (isPlainObject(dataStructure)) {
    var map = new Map();

    for (var key in dataStructure) {
      map.set(key, dataStructure[key]);
    }

    return map;
  } else {
    return die(21, dataStructure);
  }
}

var _Symbol$iterator$1, _Symbol$toStringTag$1;
var ObservableSetMarker = {};
_Symbol$iterator$1 = Symbol.iterator;
_Symbol$toStringTag$1 = Symbol.toStringTag;
var ObservableSet = /*#__PURE__*/function () {
  function ObservableSet(initialData, enhancer, name_) {
    if (enhancer === void 0) {
      enhancer = deepEnhancer;
    }

    if (name_ === void 0) {
      name_ = "ObservableSet@" + getNextId();
    }

    this.name_ = void 0;
    this[$mobx] = ObservableSetMarker;
    this.data_ = new Set();
    this.atom_ = void 0;
    this.changeListeners_ = void 0;
    this.interceptors_ = void 0;
    this.dehancer = void 0;
    this.enhancer_ = void 0;
    this.name_ = name_;

    if (!isFunction(Set)) {
      die(22);
    }

    this.atom_ = createAtom(this.name_);

    this.enhancer_ = function (newV, oldV) {
      return enhancer(newV, oldV, name_);
    };

    if (initialData) {
      this.replace(initialData);
    }
  }

  var _proto = ObservableSet.prototype;

  _proto.dehanceValue_ = function dehanceValue_(value) {
    if (this.dehancer !== undefined) {
      return this.dehancer(value);
    }

    return value;
  };

  _proto.clear = function clear() {
    var _this = this;

    transaction(function () {
      untracked(function () {
        for (var _iterator = _createForOfIteratorHelperLoose(_this.data_.values()), _step; !(_step = _iterator()).done;) {
          var value = _step.value;

          _this["delete"](value);
        }
      });
    });
  };

  _proto.forEach = function forEach(callbackFn, thisArg) {
    for (var _iterator2 = _createForOfIteratorHelperLoose(this), _step2; !(_step2 = _iterator2()).done;) {
      var value = _step2.value;
      callbackFn.call(thisArg, value, value, this);
    }
  };

  _proto.add = function add(value) {
    var _this2 = this;

    checkIfStateModificationsAreAllowed(this.atom_);

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        type: ADD,
        object: this,
        newValue: value
      });
      if (!change) return this; // ideally, value = change.value would be done here, so that values can be
      // changed by interceptor. Same applies for other Set and Map api's.
    }

    if (!this.has(value)) {
      transaction(function () {
        _this2.data_.add(_this2.enhancer_(value, undefined));

        _this2.atom_.reportChanged();
      });
      var notifySpy =  false && 0;
      var notify = hasListeners(this);

      var _change = notify || notifySpy ? {
        observableKind: "set",
        debugObjectName: this.name_,
        type: ADD,
        object: this,
        newValue: value
      } : null;

      if (notifySpy && "production" !== "production") spyReportStart(_change);
      if (notify) notifyListeners(this, _change);
      if (notifySpy && "production" !== "production") spyReportEnd();
    }

    return this;
  };

  _proto["delete"] = function _delete(value) {
    var _this3 = this;

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        type: DELETE,
        object: this,
        oldValue: value
      });
      if (!change) return false;
    }

    if (this.has(value)) {
      var notifySpy =  false && 0;
      var notify = hasListeners(this);

      var _change2 = notify || notifySpy ? {
        observableKind: "set",
        debugObjectName: this.name_,
        type: DELETE,
        object: this,
        oldValue: value
      } : null;

      if (notifySpy && "production" !== "production") spyReportStart(_change2);
      transaction(function () {
        _this3.atom_.reportChanged();

        _this3.data_["delete"](value);
      });
      if (notify) notifyListeners(this, _change2);
      if (notifySpy && "production" !== "production") spyReportEnd();
      return true;
    }

    return false;
  };

  _proto.has = function has(value) {
    this.atom_.reportObserved();
    return this.data_.has(this.dehanceValue_(value));
  };

  _proto.entries = function entries() {
    var nextIndex = 0;
    var keys = Array.from(this.keys());
    var values = Array.from(this.values());
    return makeIterable({
      next: function next() {
        var index = nextIndex;
        nextIndex += 1;
        return index < values.length ? {
          value: [keys[index], values[index]],
          done: false
        } : {
          done: true
        };
      }
    });
  };

  _proto.keys = function keys() {
    return this.values();
  };

  _proto.values = function values() {
    this.atom_.reportObserved();
    var self = this;
    var nextIndex = 0;
    var observableValues = Array.from(this.data_.values());
    return makeIterable({
      next: function next() {
        return nextIndex < observableValues.length ? {
          value: self.dehanceValue_(observableValues[nextIndex++]),
          done: false
        } : {
          done: true
        };
      }
    });
  };

  _proto.replace = function replace(other) {
    var _this4 = this;

    if (isObservableSet(other)) {
      other = new Set(other);
    }

    transaction(function () {
      if (Array.isArray(other)) {
        _this4.clear();

        other.forEach(function (value) {
          return _this4.add(value);
        });
      } else if (isES6Set(other)) {
        _this4.clear();

        other.forEach(function (value) {
          return _this4.add(value);
        });
      } else if (other !== null && other !== undefined) {
        die("Cannot initialize set from " + other);
      }
    });
    return this;
  };

  _proto.observe_ = function observe_(listener, fireImmediately) {
    // ... 'fireImmediately' could also be true?
    if (false) {}
    return registerListener(this, listener);
  };

  _proto.intercept_ = function intercept_(handler) {
    return registerInterceptor(this, handler);
  };

  _proto.toJSON = function toJSON() {
    return Array.from(this);
  };

  _proto.toString = function toString() {
    return "[object ObservableSet]";
  };

  _proto[_Symbol$iterator$1] = function () {
    return this.values();
  };

  _createClass(ObservableSet, [{
    key: "size",
    get: function get() {
      this.atom_.reportObserved();
      return this.data_.size;
    }
  }, {
    key: _Symbol$toStringTag$1,
    get: function get() {
      return "Set";
    }
  }]);

  return ObservableSet;
}(); // eslint-disable-next-line

var isObservableSet = /*#__PURE__*/createInstanceofPredicate("ObservableSet", ObservableSet);

var REMOVE = "remove";
var ObservableObjectAdministration = /*#__PURE__*/function () {
  function ObservableObjectAdministration(target_, values_, name_, defaultEnhancer_) {
    if (values_ === void 0) {
      values_ = new Map();
    }

    this.target_ = void 0;
    this.values_ = void 0;
    this.name_ = void 0;
    this.defaultEnhancer_ = void 0;
    this.keysAtom_ = void 0;
    this.changeListeners_ = void 0;
    this.interceptors_ = void 0;
    this.proxy_ = void 0;
    this.pendingKeys_ = void 0;
    this.keysValue_ = [];
    this.isStaledKeysValue_ = true;
    this.target_ = target_;
    this.values_ = values_;
    this.name_ = name_;
    this.defaultEnhancer_ = defaultEnhancer_;
    this.keysAtom_ = new Atom(name_ + ".keys");
  }

  var _proto = ObservableObjectAdministration.prototype;

  _proto.read_ = function read_(key) {
    return this.values_.get(key).get();
  };

  _proto.write_ = function write_(key, newValue) {
    var instance = this.target_;
    var observable = this.values_.get(key);

    if (observable instanceof ComputedValue) {
      observable.set(newValue);
      return;
    } // intercept


    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        type: UPDATE,
        object: this.proxy_ || instance,
        name: key,
        newValue: newValue
      });
      if (!change) return;
      newValue = change.newValue;
    }

    newValue = observable.prepareNewValue_(newValue); // notify spy & observers

    if (newValue !== globalState.UNCHANGED) {
      var notify = hasListeners(this);
      var notifySpy =  false && 0;

      var _change = notify || notifySpy ? {
        type: UPDATE,
        observableKind: "object",
        debugObjectName: this.name_,
        object: this.proxy_ || instance,
        oldValue: observable.value_,
        name: key,
        newValue: newValue
      } : null;

      if (false) {}
      observable.setNewValue_(newValue);
      if (notify) notifyListeners(this, _change);
      if (false) {}
    }
  };

  _proto.has_ = function has_(key) {
    var map = this.pendingKeys_ || (this.pendingKeys_ = new Map());
    var entry = map.get(key);
    if (entry) return entry.get();else {
      var exists = !!this.values_.get(key); // Possible optimization: Don't have a separate map for non existing keys,
      // but store them in the values map instead, using a special symbol to denote "not existing"

      entry = new ObservableValue(exists, referenceEnhancer, this.name_ + "." + stringifyKey(key) + "?", false);
      map.set(key, entry);
      return entry.get(); // read to subscribe
    }
  };

  _proto.addObservableProp_ = function addObservableProp_(propName, newValue, enhancer) {
    if (enhancer === void 0) {
      enhancer = this.defaultEnhancer_;
    }

    var target = this.target_;
    if (false) {}

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        object: this.proxy_ || target,
        name: propName,
        type: ADD,
        newValue: newValue
      });
      if (!change) return;
      newValue = change.newValue;
    }

    var observable = new ObservableValue(newValue, enhancer, this.name_ + "." + stringifyKey(propName), false);
    this.values_.set(propName, observable);
    newValue = observable.value_; // observableValue might have changed it

    defineProperty(target, propName, generateObservablePropConfig(propName));
    this.notifyPropertyAddition_(propName, newValue);
  };

  _proto.addComputedProp_ = function addComputedProp_(propertyOwner, // where is the property declared?
  propName, options) {
    var target = this.target_;
    options.name = options.name || this.name_ + "." + stringifyKey(propName);
    options.context = this.proxy_ || target;
    this.values_.set(propName, new ComputedValue(options)); // Doesn't seem we need this condition:
    // if (propertyOwner === target || isPropertyConfigurable(propertyOwner, propName))

    defineProperty(propertyOwner, propName, generateComputedPropConfig(propName));
  };

  _proto.remove_ = function remove_(key) {
    if (!this.values_.has(key)) return;
    var target = this.target_;

    if (hasInterceptors(this)) {
      var change = interceptChange(this, {
        object: this.proxy_ || target,
        name: key,
        type: REMOVE
      });
      if (!change) return;
    }

    try {
      startBatch();
      var notify = hasListeners(this);
      var notifySpy =  false && 0;
      var oldObservable = this.values_.get(key);
      var oldValue = oldObservable && oldObservable.get();
      oldObservable && oldObservable.set(undefined); // notify key and keyset listeners

      this.reportKeysChanged();
      this.values_["delete"](key);

      if (this.pendingKeys_) {
        var entry = this.pendingKeys_.get(key);
        if (entry) entry.set(false);
      } // delete the prop


      delete this.target_[key];

      var _change2 = notify || notifySpy ? {
        type: REMOVE,
        observableKind: "object",
        object: this.proxy_ || target,
        debugObjectName: this.name_,
        oldValue: oldValue,
        name: key
      } : null;

      if (false) {}
      if (notify) notifyListeners(this, _change2);
      if (false) {}
    } finally {
      endBatch();
    }
  }
  /**
   * Observes this object. Triggers for the events 'add', 'update' and 'delete'.
   * See: https://developer.mozilla.org/en-US/docs/Web/JavaScript/Reference/Global_Objects/Object/observe
   * for callback details
   */
  ;

  _proto.observe_ = function observe_(callback, fireImmediately) {
    if (false) {}
    return registerListener(this, callback);
  };

  _proto.intercept_ = function intercept_(handler) {
    return registerInterceptor(this, handler);
  };

  _proto.notifyPropertyAddition_ = function notifyPropertyAddition_(key, newValue) {
    var notify = hasListeners(this);
    var notifySpy =  false && 0;
    var change = notify || notifySpy ? {
      type: ADD,
      observableKind: "object",
      debugObjectName: this.name_,
      object: this.proxy_ || this.target_,
      name: key,
      newValue: newValue
    } : null;
    if (false) {}
    if (notify) notifyListeners(this, change);
    if (false) {}

    if (this.pendingKeys_) {
      var entry = this.pendingKeys_.get(key);
      if (entry) entry.set(true);
    }

    this.reportKeysChanged();
  };

  _proto.getKeys_ = function getKeys_() {
    this.keysAtom_.reportObserved();

    if (!this.isStaledKeysValue_) {
      return this.keysValue_;
    } // return Reflect.ownKeys(this.values) as any


    this.keysValue_ = [];

    for (var _iterator = _createForOfIteratorHelperLoose(this.values_), _step; !(_step = _iterator()).done;) {
      var _step$value = _step.value,
          key = _step$value[0],
          value = _step$value[1];
      if (value instanceof ObservableValue) this.keysValue_.push(key);
    }

    if (false) {}
    this.isStaledKeysValue_ = false;
    return this.keysValue_;
  };

  _proto.reportKeysChanged = function reportKeysChanged() {
    this.isStaledKeysValue_ = true;
    this.keysAtom_.reportChanged();
  };

  return ObservableObjectAdministration;
}();
function asObservableObject(target, name, defaultEnhancer) {
  if (name === void 0) {
    name = "";
  }

  if (defaultEnhancer === void 0) {
    defaultEnhancer = deepEnhancer;
  }

  if (hasProp(target, $mobx)) return target[$mobx];
  if (false) {}
  if (!isPlainObject(target)) name = (target.constructor.name || "ObservableObject") + "@" + getNextId();
  if (!name) name = "ObservableObject@" + getNextId();
  var adm = new ObservableObjectAdministration(target, new Map(), stringifyKey(name), defaultEnhancer);
  addHiddenProp(target, $mobx, adm);
  return adm;
}
var observablePropertyConfigs = /*#__PURE__*/Object.create(null);
var computedPropertyConfigs = /*#__PURE__*/Object.create(null);
function generateObservablePropConfig(propName) {
  return observablePropertyConfigs[propName] || (observablePropertyConfigs[propName] = {
    configurable: true,
    enumerable: true,
    get: function get() {
      return this[$mobx].read_(propName);
    },
    set: function set(v) {
      this[$mobx].write_(propName, v);
    }
  });
}
function generateComputedPropConfig(propName) {
  return computedPropertyConfigs[propName] || (computedPropertyConfigs[propName] = {
    configurable: true,
    enumerable: false,
    get: function get() {
      return this[$mobx].read_(propName);
    },
    set: function set(v) {
      this[$mobx].write_(propName, v);
    }
  });
}
var isObservableObjectAdministration = /*#__PURE__*/createInstanceofPredicate("ObservableObjectAdministration", ObservableObjectAdministration);
function isObservableObject(thing) {
  if (isObject(thing)) {
    return isObservableObjectAdministration(thing[$mobx]);
  }

  return false;
}

/**
 * This array buffer contains two lists of properties, so that all arrays
 * can recycle their property definitions, which significantly improves performance of creating
 * properties on the fly.
 */

var OBSERVABLE_ARRAY_BUFFER_SIZE = 0; // Typescript workaround to make sure ObservableArray extends Array

var StubArray = function StubArray() {};

function inherit(ctor, proto) {
  if (Object.setPrototypeOf) {
    Object.setPrototypeOf(ctor.prototype, proto);
  } else if (ctor.prototype.__proto__ !== undefined) {
    ctor.prototype.__proto__ = proto;
  } else {
    ctor.prototype = proto;
  }
}

inherit(StubArray, Array.prototype); // Weex proto freeze protection was here,
// but it is unclear why the hack is need as MobX never changed the prototype
// anyway, so removed it in V6

var LegacyObservableArray = /*#__PURE__*/function (_StubArray) {
  _inheritsLoose(LegacyObservableArray, _StubArray);

  function LegacyObservableArray(initialValues, enhancer, name, owned) {
    var _this;

    if (name === void 0) {
      name = "ObservableArray@" + getNextId();
    }

    if (owned === void 0) {
      owned = false;
    }

    _this = _StubArray.call(this) || this;
    var adm = new ObservableArrayAdministration(name, enhancer, owned, true);
    adm.proxy_ = _assertThisInitialized(_this);
    addHiddenFinalProp(_assertThisInitialized(_this), $mobx, adm);

    if (initialValues && initialValues.length) {
      var prev = allowStateChangesStart(true); // @ts-ignore

      _this.spliceWithArray(0, 0, initialValues);

      allowStateChangesEnd(prev);
    }

    return _this;
  }

  var _proto = LegacyObservableArray.prototype;

  _proto.concat = function concat() {
    this[$mobx].atom_.reportObserved();

    for (var _len = arguments.length, arrays = new Array(_len), _key = 0; _key < _len; _key++) {
      arrays[_key] = arguments[_key];
    }

    return Array.prototype.concat.apply(this.slice(), //@ts-ignore
    arrays.map(function (a) {
      return isObservableArray(a) ? a.slice() : a;
    }));
  };

  _proto[Symbol.iterator] = function () {
    var self = this;
    var nextIndex = 0;
    return makeIterable({
      next: function next() {
        // @ts-ignore
        return nextIndex < self.length ? {
          value: self[nextIndex++],
          done: false
        } : {
          done: true,
          value: undefined
        };
      }
    });
  };

  _createClass(LegacyObservableArray, [{
    key: "length",
    get: function get() {
      return this[$mobx].getArrayLength_();
    },
    set: function set(newLength) {
      this[$mobx].setArrayLength_(newLength);
    }
  }, {
    key: Symbol.toStringTag,
    get: function get() {
      return "Array";
    }
  }]);

  return LegacyObservableArray;
}(StubArray);

Object.entries(arrayExtensions).forEach(function (_ref) {
  var prop = _ref[0],
      fn = _ref[1];
  if (prop !== "concat") addHiddenProp(LegacyObservableArray.prototype, prop, fn);
});

function createArrayEntryDescriptor(index) {
  return {
    enumerable: false,
    configurable: true,
    get: function get() {
      return this[$mobx].get_(index);
    },
    set: function set(value) {
      this[$mobx].set_(index, value);
    }
  };
}

function createArrayBufferItem(index) {
  defineProperty(LegacyObservableArray.prototype, "" + index, createArrayEntryDescriptor(index));
}

function reserveArrayBuffer(max) {
  if (max > OBSERVABLE_ARRAY_BUFFER_SIZE) {
    for (var index = OBSERVABLE_ARRAY_BUFFER_SIZE; index < max + 100; index++) {
      createArrayBufferItem(index);
    }

    OBSERVABLE_ARRAY_BUFFER_SIZE = max;
  }
}
reserveArrayBuffer(1000);
function createLegacyArray(initialValues, enhancer, name) {
  return new LegacyObservableArray(initialValues, enhancer, name);
}

function getAtom(thing, property) {
  if (typeof thing === "object" && thing !== null) {
    if (isObservableArray(thing)) {
      if (property !== undefined) die(23);
      return thing[$mobx].atom_;
    }

    if (isObservableSet(thing)) {
      return thing[$mobx];
    }

    if (isObservableMap(thing)) {
      if (property === undefined) return thing.keysAtom_;
      var observable = thing.data_.get(property) || thing.hasMap_.get(property);
      if (!observable) die(25, property, getDebugName(thing));
      return observable;
    }

    if (isObservableObject(thing)) {
      if (!property) return die(26);

      var _observable = thing[$mobx].values_.get(property);

      if (!_observable) die(27, property, getDebugName(thing));
      return _observable;
    }

    if (isAtom(thing) || isComputedValue(thing) || isReaction(thing)) {
      return thing;
    }
  } else if (isFunction(thing)) {
    if (isReaction(thing[$mobx])) {
      // disposer function
      return thing[$mobx];
    }
  }

  die(28);
}
function getAdministration(thing, property) {
  if (!thing) die(29);
  if (property !== undefined) return getAdministration(getAtom(thing, property));
  if (isAtom(thing) || isComputedValue(thing) || isReaction(thing)) return thing;
  if (isObservableMap(thing) || isObservableSet(thing)) return thing;
  if (thing[$mobx]) return thing[$mobx];
  die(24, thing);
}
function getDebugName(thing, property) {
  var named;
  if (property !== undefined) named = getAtom(thing, property);else if (isObservableObject(thing) || isObservableMap(thing) || isObservableSet(thing)) named = getAdministration(thing);else named = getAtom(thing); // valid for arrays as well

  return named.name_;
}

var toString = objectPrototype.toString;
function deepEqual(a, b, depth) {
  if (depth === void 0) {
    depth = -1;
  }

  return eq(a, b, depth);
} // Copied from https://github.com/jashkenas/underscore/blob/5c237a7c682fb68fd5378203f0bf22dce1624854/underscore.js#L1186-L1289
// Internal recursive comparison function for `isEqual`.

function eq(a, b, depth, aStack, bStack) {
  // Identical objects are equal. `0 === -0`, but they aren't identical.
  // See the [Harmony `egal` proposal](http://wiki.ecmascript.org/doku.php?id=harmony:egal).
  if (a === b) return a !== 0 || 1 / a === 1 / b; // `null` or `undefined` only equal to itself (strict comparison).

  if (a == null || b == null) return false; // `NaN`s are equivalent, but non-reflexive.

  if (a !== a) return b !== b; // Exhaust primitive checks

  var type = typeof a;
  if (!isFunction(type) && type !== "object" && typeof b != "object") return false; // Compare `[[Class]]` names.

  var className = toString.call(a);
  if (className !== toString.call(b)) return false;

  switch (className) {
    // Strings, numbers, regular expressions, dates, and booleans are compared by value.
    case "[object RegExp]": // RegExps are coerced to strings for comparison (Note: '' + /a/i === '/a/i')

    case "[object String]":
      // Primitives and their corresponding object wrappers are equivalent; thus, `"5"` is
      // equivalent to `new String("5")`.
      return "" + a === "" + b;

    case "[object Number]":
      // `NaN`s are equivalent, but non-reflexive.
      // Object(NaN) is equivalent to NaN.
      if (+a !== +a) return +b !== +b; // An `egal` comparison is performed for other numeric values.

      return +a === 0 ? 1 / +a === 1 / b : +a === +b;

    case "[object Date]":
    case "[object Boolean]":
      // Coerce dates and booleans to numeric primitive values. Dates are compared by their
      // millisecond representations. Note that invalid dates with millisecond representations
      // of `NaN` are not equivalent.
      return +a === +b;

    case "[object Symbol]":
      return typeof Symbol !== "undefined" && Symbol.valueOf.call(a) === Symbol.valueOf.call(b);

    case "[object Map]":
    case "[object Set]":
      // Maps and Sets are unwrapped to arrays of entry-pairs, adding an incidental level.
      // Hide this extra level by increasing the depth.
      if (depth >= 0) {
        depth++;
      }

      break;
  } // Unwrap any wrapped objects.


  a = unwrap(a);
  b = unwrap(b);
  var areArrays = className === "[object Array]";

  if (!areArrays) {
    if (typeof a != "object" || typeof b != "object") return false; // Objects with different constructors are not equivalent, but `Object`s or `Array`s
    // from different frames are.

    var aCtor = a.constructor,
        bCtor = b.constructor;

    if (aCtor !== bCtor && !(isFunction(aCtor) && aCtor instanceof aCtor && isFunction(bCtor) && bCtor instanceof bCtor) && "constructor" in a && "constructor" in b) {
      return false;
    }
  }

  if (depth === 0) {
    return false;
  } else if (depth < 0) {
    depth = -1;
  } // Assume equality for cyclic structures. The algorithm for detecting cyclic
  // structures is adapted from ES 5.1 section 15.12.3, abstract operation `JO`.
  // Initializing stack of traversed objects.
  // It's done here since we only need them for objects and arrays comparison.


  aStack = aStack || [];
  bStack = bStack || [];
  var length = aStack.length;

  while (length--) {
    // Linear search. Performance is inversely proportional to the number of
    // unique nested structures.
    if (aStack[length] === a) return bStack[length] === b;
  } // Add the first object to the stack of traversed objects.


  aStack.push(a);
  bStack.push(b); // Recursively compare objects and arrays.

  if (areArrays) {
    // Compare array lengths to determine if a deep comparison is necessary.
    length = a.length;
    if (length !== b.length) return false; // Deep compare the contents, ignoring non-numeric properties.

    while (length--) {
      if (!eq(a[length], b[length], depth - 1, aStack, bStack)) return false;
    }
  } else {
    // Deep compare objects.
    var keys = Object.keys(a);
    var key;
    length = keys.length; // Ensure that both objects contain the same number of properties before comparing deep equality.

    if (Object.keys(b).length !== length) return false;

    while (length--) {
      // Deep compare each member
      key = keys[length];
      if (!(hasProp(b, key) && eq(a[key], b[key], depth - 1, aStack, bStack))) return false;
    }
  } // Remove the first object from the stack of traversed objects.


  aStack.pop();
  bStack.pop();
  return true;
}

function unwrap(a) {
  if (isObservableArray(a)) return a.slice();
  if (isES6Map(a) || isObservableMap(a)) return Array.from(a.entries());
  if (isES6Set(a) || isObservableSet(a)) return Array.from(a.entries());
  return a;
}

function makeIterable(iterator) {
  iterator[Symbol.iterator] = getSelf;
  return iterator;
}

function getSelf() {
  return this;
}

/**
 * (c) Michel Weststrate 2015 - 2020
 * MIT Licensed
 *
 * Welcome to the mobx sources! To get an global overview of how MobX internally works,
 * this is a good place to start:
 * https://medium.com/@mweststrate/becoming-fully-reactive-an-in-depth-explanation-of-mobservable-55995262a254#.xvbh6qd74
 *
 * Source folders:
 * ===============
 *
 * - api/     Most of the public static methods exposed by the module can be found here.
 * - core/    Implementation of the MobX algorithm; atoms, derivations, reactions, dependency trees, optimizations. Cool stuff can be found here.
 * - types/   All the magic that is need to have observable objects, arrays and values is in this folder. Including the modifiers like `asFlat`.
 * - utils/   Utility stuff.
 *
 */
["Symbol", "Map", "Set", "Symbol"].forEach(function (m) {
  var g = getGlobal();

  if (typeof g[m] === "undefined") {
    die("MobX requires global '" + m + "' to be available or polyfilled");
  }
});

if (typeof __MOBX_DEVTOOLS_GLOBAL_HOOK__ === "object") {
  // See: https://github.com/andykog/mobx-devtools/
  __MOBX_DEVTOOLS_GLOBAL_HOOK__.injectMobx({
    spy: spy,
    extras: {
      getDebugName: getDebugName
    },
    $mobx: $mobx
  });
}




/***/ }),

/***/ 350:
/*!**********************************************************!*\
  !*** ./node_modules/nanoid/index.browser.js + 1 modules ***!
  \**********************************************************/
/***/ ((__unused_webpack___webpack_module__, __webpack_exports__, __webpack_require__) => {

"use strict";
// ESM COMPAT FLAG
__webpack_require__.r(__webpack_exports__);

// EXPORTS
__webpack_require__.d(__webpack_exports__, {
  "customAlphabet": () => (/* binding */ customAlphabet),
  "customRandom": () => (/* binding */ customRandom),
  "nanoid": () => (/* binding */ nanoid),
  "random": () => (/* binding */ random),
  "urlAlphabet": () => (/* reexport */ urlAlphabet)
});

;// CONCATENATED MODULE: ./node_modules/nanoid/url-alphabet/index.js
// This alphabet uses `A-Za-z0-9_-` symbols. The genetic algorithm helped
// optimize the gzip compression for this alphabet.
let urlAlphabet =
  'ModuleSymbhasOwnPr-0123456789ABCDEFGHNRVfgctiUvz_KqYTJkLxpZXIjQW'



;// CONCATENATED MODULE: ./node_modules/nanoid/index.browser.js
// This file replaces `index.js` in bundlers like webpack or Rollup,
// according to `browser` config in `package.json`.



if (false) {}

let random = bytes => crypto.getRandomValues(new Uint8Array(bytes))

let customRandom = (alphabet, size, getRandom) => {
  // First, a bitmask is necessary to generate the ID. The bitmask makes bytes
  // values closer to the alphabet size. The bitmask calculates the closest
  // `2^31 - 1` number, which exceeds the alphabet size.
  // For example, the bitmask for the alphabet size 30 is 31 (00011111).
  // `Math.clz32` is not used, because it is not available in browsers.
  let mask = (2 << (Math.log(alphabet.length - 1) / Math.LN2)) - 1
  // Though, the bitmask solution is not perfect since the bytes exceeding
  // the alphabet size are refused. Therefore, to reliably generate the ID,
  // the random bytes redundancy has to be satisfied.

  // Note: every hardware random generator call is performance expensive,
  // because the system call for entropy collection takes a lot of time.
  // So, to avoid additional system calls, extra bytes are requested in advance.

  // Next, a step determines how many random bytes to generate.
  // The number of random bytes gets decided upon the ID size, mask,
  // alphabet size, and magic number 1.6 (using 1.6 peaks at performance
  // according to benchmarks).

  // `-~f => Math.ceil(f)` if f is a float
  // `-~i => i + 1` if i is an integer
  let step = -~((1.6 * mask * size) / alphabet.length)

  return () => {
    let id = ''
    while (true) {
      let bytes = getRandom(step)
      // A compact alternative for `for (var i = 0; i < step; i++)`.
      let j = step
      while (j--) {
        // Adding `|| ''` refuses a random byte that exceeds the alphabet size.
        id += alphabet[bytes[j] & mask] || ''
        // `id.length + 1 === size` is a more compact option.
        if (id.length === +size) return id
      }
    }
  }
}

let customAlphabet = (alphabet, size) => customRandom(alphabet, size, random)

let nanoid = (size = 21) => {
  let id = ''
  let bytes = crypto.getRandomValues(new Uint8Array(size))

  // A compact alternative for `for (var i = 0; i < step; i++)`.
  while (size--) {
    // It is incorrect to use bytes exceeding the alphabet size.
    // The following mask reduces the random byte in the 0-255 value
    // range to the 0-63 value range. Therefore, adding hacks, such
    // as empty string fallback or magic numbers, is unneccessary because
    // the bitmask trims bytes down to the alphabet size.
    let byte = bytes[size] & 63
    if (byte < 36) {
      // `0-9a-z`
      id += byte.toString(36)
    } else if (byte < 62) {
      // `A-Z`
      id += (byte - 26).toString(36).toUpperCase()
    } else if (byte < 63) {
      id += '_'
    } else {
      id += '-'
    }
  }
  return id
}




/***/ }),

/***/ 3624:
/*!************************************************************!*\
  !*** ./node_modules/webextension-polyfill-ts/lib/index.js ***!
  \************************************************************/
/***/ ((__unused_webpack_module, exports, __webpack_require__) => {

"use strict";

Object.defineProperty(exports, "__esModule", ({ value: true }));

// if not in a browser, assume we're in a test, return a dummy
if (typeof window === "undefined") exports.browser = {};
else exports.browser = __webpack_require__(/*! webextension-polyfill */ 4390);


/***/ }),

/***/ 4390:
/*!*********************************************************************!*\
  !*** ./node_modules/webextension-polyfill/dist/browser-polyfill.js ***!
  \*********************************************************************/
/***/ (function(module, exports) {

var __WEBPACK_AMD_DEFINE_FACTORY__, __WEBPACK_AMD_DEFINE_ARRAY__, __WEBPACK_AMD_DEFINE_RESULT__;(function (global, factory) {
  if (true) {
    !(__WEBPACK_AMD_DEFINE_ARRAY__ = [module], __WEBPACK_AMD_DEFINE_FACTORY__ = (factory),
		__WEBPACK_AMD_DEFINE_RESULT__ = (typeof __WEBPACK_AMD_DEFINE_FACTORY__ === 'function' ?
		(__WEBPACK_AMD_DEFINE_FACTORY__.apply(exports, __WEBPACK_AMD_DEFINE_ARRAY__)) : __WEBPACK_AMD_DEFINE_FACTORY__),
		__WEBPACK_AMD_DEFINE_RESULT__ !== undefined && (module.exports = __WEBPACK_AMD_DEFINE_RESULT__));
  } else { var mod; }
})(typeof globalThis !== "undefined" ? globalThis : typeof self !== "undefined" ? self : this, function (module) {
  /* webextension-polyfill - v0.6.0 - Mon Dec 23 2019 12:32:53 */

  /* -*- Mode: indent-tabs-mode: nil; js-indent-level: 2 -*- */

  /* vim: set sts=2 sw=2 et tw=80: */

  /* This Source Code Form is subject to the terms of the Mozilla Public
   * License, v. 2.0. If a copy of the MPL was not distributed with this
   * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
  "use strict";

  if (typeof browser === "undefined" || Object.getPrototypeOf(browser) !== Object.prototype) {
    const CHROME_SEND_MESSAGE_CALLBACK_NO_RESPONSE_MESSAGE = "The message port closed before a response was received.";
    const SEND_RESPONSE_DEPRECATION_WARNING = "Returning a Promise is the preferred way to send a reply from an onMessage/onMessageExternal listener, as the sendResponse will be removed from the specs (See https://developer.mozilla.org/docs/Mozilla/Add-ons/WebExtensions/API/runtime/onMessage)"; // Wrapping the bulk of this polyfill in a one-time-use function is a minor
    // optimization for Firefox. Since Spidermonkey does not fully parse the
    // contents of a function until the first time it's called, and since it will
    // never actually need to be called, this allows the polyfill to be included
    // in Firefox nearly for free.

    const wrapAPIs = extensionAPIs => {
      // NOTE: apiMetadata is associated to the content of the api-metadata.json file
      // at build time by replacing the following "include" with the content of the
      // JSON file.
      const apiMetadata = {
        "alarms": {
          "clear": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "clearAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "get": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "getAll": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "bookmarks": {
          "create": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "get": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getChildren": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getRecent": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getSubTree": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getTree": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "move": {
            "minArgs": 2,
            "maxArgs": 2
          },
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeTree": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "search": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "update": {
            "minArgs": 2,
            "maxArgs": 2
          }
        },
        "browserAction": {
          "disable": {
            "minArgs": 0,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "enable": {
            "minArgs": 0,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "getBadgeBackgroundColor": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getBadgeText": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getPopup": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getTitle": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "openPopup": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "setBadgeBackgroundColor": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "setBadgeText": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "setIcon": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "setPopup": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "setTitle": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          }
        },
        "browsingData": {
          "remove": {
            "minArgs": 2,
            "maxArgs": 2
          },
          "removeCache": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeCookies": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeDownloads": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeFormData": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeHistory": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeLocalStorage": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removePasswords": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removePluginData": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "settings": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "commands": {
          "getAll": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "contextMenus": {
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "update": {
            "minArgs": 2,
            "maxArgs": 2
          }
        },
        "cookies": {
          "get": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getAll": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getAllCookieStores": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "set": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "devtools": {
          "inspectedWindow": {
            "eval": {
              "minArgs": 1,
              "maxArgs": 2,
              "singleCallbackArg": false
            }
          },
          "panels": {
            "create": {
              "minArgs": 3,
              "maxArgs": 3,
              "singleCallbackArg": true
            }
          }
        },
        "downloads": {
          "cancel": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "download": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "erase": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getFileIcon": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "open": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "pause": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeFile": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "resume": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "search": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "show": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          }
        },
        "extension": {
          "isAllowedFileSchemeAccess": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "isAllowedIncognitoAccess": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "history": {
          "addUrl": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "deleteAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "deleteRange": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "deleteUrl": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getVisits": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "search": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "i18n": {
          "detectLanguage": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getAcceptLanguages": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "identity": {
          "launchWebAuthFlow": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "idle": {
          "queryState": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "management": {
          "get": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "getSelf": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "setEnabled": {
            "minArgs": 2,
            "maxArgs": 2
          },
          "uninstallSelf": {
            "minArgs": 0,
            "maxArgs": 1
          }
        },
        "notifications": {
          "clear": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "create": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "getAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "getPermissionLevel": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "update": {
            "minArgs": 2,
            "maxArgs": 2
          }
        },
        "pageAction": {
          "getPopup": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getTitle": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "hide": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "setIcon": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "setPopup": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "setTitle": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          },
          "show": {
            "minArgs": 1,
            "maxArgs": 1,
            "fallbackToNoCallback": true
          }
        },
        "permissions": {
          "contains": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getAll": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "request": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "runtime": {
          "getBackgroundPage": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "getPlatformInfo": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "openOptionsPage": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "requestUpdateCheck": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "sendMessage": {
            "minArgs": 1,
            "maxArgs": 3
          },
          "sendNativeMessage": {
            "minArgs": 2,
            "maxArgs": 2
          },
          "setUninstallURL": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "sessions": {
          "getDevices": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "getRecentlyClosed": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "restore": {
            "minArgs": 0,
            "maxArgs": 1
          }
        },
        "storage": {
          "local": {
            "clear": {
              "minArgs": 0,
              "maxArgs": 0
            },
            "get": {
              "minArgs": 0,
              "maxArgs": 1
            },
            "getBytesInUse": {
              "minArgs": 0,
              "maxArgs": 1
            },
            "remove": {
              "minArgs": 1,
              "maxArgs": 1
            },
            "set": {
              "minArgs": 1,
              "maxArgs": 1
            }
          },
          "managed": {
            "get": {
              "minArgs": 0,
              "maxArgs": 1
            },
            "getBytesInUse": {
              "minArgs": 0,
              "maxArgs": 1
            }
          },
          "sync": {
            "clear": {
              "minArgs": 0,
              "maxArgs": 0
            },
            "get": {
              "minArgs": 0,
              "maxArgs": 1
            },
            "getBytesInUse": {
              "minArgs": 0,
              "maxArgs": 1
            },
            "remove": {
              "minArgs": 1,
              "maxArgs": 1
            },
            "set": {
              "minArgs": 1,
              "maxArgs": 1
            }
          }
        },
        "tabs": {
          "captureVisibleTab": {
            "minArgs": 0,
            "maxArgs": 2
          },
          "create": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "detectLanguage": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "discard": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "duplicate": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "executeScript": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "get": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getCurrent": {
            "minArgs": 0,
            "maxArgs": 0
          },
          "getZoom": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "getZoomSettings": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "highlight": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "insertCSS": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "move": {
            "minArgs": 2,
            "maxArgs": 2
          },
          "query": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "reload": {
            "minArgs": 0,
            "maxArgs": 2
          },
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "removeCSS": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "sendMessage": {
            "minArgs": 2,
            "maxArgs": 3
          },
          "setZoom": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "setZoomSettings": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "update": {
            "minArgs": 1,
            "maxArgs": 2
          }
        },
        "topSites": {
          "get": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "webNavigation": {
          "getAllFrames": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "getFrame": {
            "minArgs": 1,
            "maxArgs": 1
          }
        },
        "webRequest": {
          "handlerBehaviorChanged": {
            "minArgs": 0,
            "maxArgs": 0
          }
        },
        "windows": {
          "create": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "get": {
            "minArgs": 1,
            "maxArgs": 2
          },
          "getAll": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "getCurrent": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "getLastFocused": {
            "minArgs": 0,
            "maxArgs": 1
          },
          "remove": {
            "minArgs": 1,
            "maxArgs": 1
          },
          "update": {
            "minArgs": 2,
            "maxArgs": 2
          }
        }
      };

      if (Object.keys(apiMetadata).length === 0) {
        throw new Error("api-metadata.json has not been included in browser-polyfill");
      }
      /**
       * A WeakMap subclass which creates and stores a value for any key which does
       * not exist when accessed, but behaves exactly as an ordinary WeakMap
       * otherwise.
       *
       * @param {function} createItem
       *        A function which will be called in order to create the value for any
       *        key which does not exist, the first time it is accessed. The
       *        function receives, as its only argument, the key being created.
       */


      class DefaultWeakMap extends WeakMap {
        constructor(createItem, items = undefined) {
          super(items);
          this.createItem = createItem;
        }

        get(key) {
          if (!this.has(key)) {
            this.set(key, this.createItem(key));
          }

          return super.get(key);
        }

      }
      /**
       * Returns true if the given object is an object with a `then` method, and can
       * therefore be assumed to behave as a Promise.
       *
       * @param {*} value The value to test.
       * @returns {boolean} True if the value is thenable.
       */


      const isThenable = value => {
        return value && typeof value === "object" && typeof value.then === "function";
      };
      /**
       * Creates and returns a function which, when called, will resolve or reject
       * the given promise based on how it is called:
       *
       * - If, when called, `chrome.runtime.lastError` contains a non-null object,
       *   the promise is rejected with that value.
       * - If the function is called with exactly one argument, the promise is
       *   resolved to that value.
       * - Otherwise, the promise is resolved to an array containing all of the
       *   function's arguments.
       *
       * @param {object} promise
       *        An object containing the resolution and rejection functions of a
       *        promise.
       * @param {function} promise.resolve
       *        The promise's resolution function.
       * @param {function} promise.rejection
       *        The promise's rejection function.
       * @param {object} metadata
       *        Metadata about the wrapped method which has created the callback.
       * @param {integer} metadata.maxResolvedArgs
       *        The maximum number of arguments which may be passed to the
       *        callback created by the wrapped async function.
       *
       * @returns {function}
       *        The generated callback function.
       */


      const makeCallback = (promise, metadata) => {
        return (...callbackArgs) => {
          if (extensionAPIs.runtime.lastError) {
            promise.reject(extensionAPIs.runtime.lastError);
          } else if (metadata.singleCallbackArg || callbackArgs.length <= 1 && metadata.singleCallbackArg !== false) {
            promise.resolve(callbackArgs[0]);
          } else {
            promise.resolve(callbackArgs);
          }
        };
      };

      const pluralizeArguments = numArgs => numArgs == 1 ? "argument" : "arguments";
      /**
       * Creates a wrapper function for a method with the given name and metadata.
       *
       * @param {string} name
       *        The name of the method which is being wrapped.
       * @param {object} metadata
       *        Metadata about the method being wrapped.
       * @param {integer} metadata.minArgs
       *        The minimum number of arguments which must be passed to the
       *        function. If called with fewer than this number of arguments, the
       *        wrapper will raise an exception.
       * @param {integer} metadata.maxArgs
       *        The maximum number of arguments which may be passed to the
       *        function. If called with more than this number of arguments, the
       *        wrapper will raise an exception.
       * @param {integer} metadata.maxResolvedArgs
       *        The maximum number of arguments which may be passed to the
       *        callback created by the wrapped async function.
       *
       * @returns {function(object, ...*)}
       *       The generated wrapper function.
       */


      const wrapAsyncFunction = (name, metadata) => {
        return function asyncFunctionWrapper(target, ...args) {
          if (args.length < metadata.minArgs) {
            throw new Error(`Expected at least ${metadata.minArgs} ${pluralizeArguments(metadata.minArgs)} for ${name}(), got ${args.length}`);
          }

          if (args.length > metadata.maxArgs) {
            throw new Error(`Expected at most ${metadata.maxArgs} ${pluralizeArguments(metadata.maxArgs)} for ${name}(), got ${args.length}`);
          }

          return new Promise((resolve, reject) => {
            if (metadata.fallbackToNoCallback) {
              // This API method has currently no callback on Chrome, but it return a promise on Firefox,
              // and so the polyfill will try to call it with a callback first, and it will fallback
              // to not passing the callback if the first call fails.
              try {
                target[name](...args, makeCallback({
                  resolve,
                  reject
                }, metadata));
              } catch (cbError) {
                console.warn(`${name} API method doesn't seem to support the callback parameter, ` + "falling back to call it without a callback: ", cbError);
                target[name](...args); // Update the API method metadata, so that the next API calls will not try to
                // use the unsupported callback anymore.

                metadata.fallbackToNoCallback = false;
                metadata.noCallback = true;
                resolve();
              }
            } else if (metadata.noCallback) {
              target[name](...args);
              resolve();
            } else {
              target[name](...args, makeCallback({
                resolve,
                reject
              }, metadata));
            }
          });
        };
      };
      /**
       * Wraps an existing method of the target object, so that calls to it are
       * intercepted by the given wrapper function. The wrapper function receives,
       * as its first argument, the original `target` object, followed by each of
       * the arguments passed to the original method.
       *
       * @param {object} target
       *        The original target object that the wrapped method belongs to.
       * @param {function} method
       *        The method being wrapped. This is used as the target of the Proxy
       *        object which is created to wrap the method.
       * @param {function} wrapper
       *        The wrapper function which is called in place of a direct invocation
       *        of the wrapped method.
       *
       * @returns {Proxy<function>}
       *        A Proxy object for the given method, which invokes the given wrapper
       *        method in its place.
       */


      const wrapMethod = (target, method, wrapper) => {
        return new Proxy(method, {
          apply(targetMethod, thisObj, args) {
            return wrapper.call(thisObj, target, ...args);
          }

        });
      };

      let hasOwnProperty = Function.call.bind(Object.prototype.hasOwnProperty);
      /**
       * Wraps an object in a Proxy which intercepts and wraps certain methods
       * based on the given `wrappers` and `metadata` objects.
       *
       * @param {object} target
       *        The target object to wrap.
       *
       * @param {object} [wrappers = {}]
       *        An object tree containing wrapper functions for special cases. Any
       *        function present in this object tree is called in place of the
       *        method in the same location in the `target` object tree. These
       *        wrapper methods are invoked as described in {@see wrapMethod}.
       *
       * @param {object} [metadata = {}]
       *        An object tree containing metadata used to automatically generate
       *        Promise-based wrapper functions for asynchronous. Any function in
       *        the `target` object tree which has a corresponding metadata object
       *        in the same location in the `metadata` tree is replaced with an
       *        automatically-generated wrapper function, as described in
       *        {@see wrapAsyncFunction}
       *
       * @returns {Proxy<object>}
       */

      const wrapObject = (target, wrappers = {}, metadata = {}) => {
        let cache = Object.create(null);
        let handlers = {
          has(proxyTarget, prop) {
            return prop in target || prop in cache;
          },

          get(proxyTarget, prop, receiver) {
            if (prop in cache) {
              return cache[prop];
            }

            if (!(prop in target)) {
              return undefined;
            }

            let value = target[prop];

            if (typeof value === "function") {
              // This is a method on the underlying object. Check if we need to do
              // any wrapping.
              if (typeof wrappers[prop] === "function") {
                // We have a special-case wrapper for this method.
                value = wrapMethod(target, target[prop], wrappers[prop]);
              } else if (hasOwnProperty(metadata, prop)) {
                // This is an async method that we have metadata for. Create a
                // Promise wrapper for it.
                let wrapper = wrapAsyncFunction(prop, metadata[prop]);
                value = wrapMethod(target, target[prop], wrapper);
              } else {
                // This is a method that we don't know or care about. Return the
                // original method, bound to the underlying object.
                value = value.bind(target);
              }
            } else if (typeof value === "object" && value !== null && (hasOwnProperty(wrappers, prop) || hasOwnProperty(metadata, prop))) {
              // This is an object that we need to do some wrapping for the children
              // of. Create a sub-object wrapper for it with the appropriate child
              // metadata.
              value = wrapObject(value, wrappers[prop], metadata[prop]);
            } else if (hasOwnProperty(metadata, "*")) {
              // Wrap all properties in * namespace.
              value = wrapObject(value, wrappers[prop], metadata["*"]);
            } else {
              // We don't need to do any wrapping for this property,
              // so just forward all access to the underlying object.
              Object.defineProperty(cache, prop, {
                configurable: true,
                enumerable: true,

                get() {
                  return target[prop];
                },

                set(value) {
                  target[prop] = value;
                }

              });
              return value;
            }

            cache[prop] = value;
            return value;
          },

          set(proxyTarget, prop, value, receiver) {
            if (prop in cache) {
              cache[prop] = value;
            } else {
              target[prop] = value;
            }

            return true;
          },

          defineProperty(proxyTarget, prop, desc) {
            return Reflect.defineProperty(cache, prop, desc);
          },

          deleteProperty(proxyTarget, prop) {
            return Reflect.deleteProperty(cache, prop);
          }

        }; // Per contract of the Proxy API, the "get" proxy handler must return the
        // original value of the target if that value is declared read-only and
        // non-configurable. For this reason, we create an object with the
        // prototype set to `target` instead of using `target` directly.
        // Otherwise we cannot return a custom object for APIs that
        // are declared read-only and non-configurable, such as `chrome.devtools`.
        //
        // The proxy handlers themselves will still use the original `target`
        // instead of the `proxyTarget`, so that the methods and properties are
        // dereferenced via the original targets.

        let proxyTarget = Object.create(target);
        return new Proxy(proxyTarget, handlers);
      };
      /**
       * Creates a set of wrapper functions for an event object, which handles
       * wrapping of listener functions that those messages are passed.
       *
       * A single wrapper is created for each listener function, and stored in a
       * map. Subsequent calls to `addListener`, `hasListener`, or `removeListener`
       * retrieve the original wrapper, so that  attempts to remove a
       * previously-added listener work as expected.
       *
       * @param {DefaultWeakMap<function, function>} wrapperMap
       *        A DefaultWeakMap object which will create the appropriate wrapper
       *        for a given listener function when one does not exist, and retrieve
       *        an existing one when it does.
       *
       * @returns {object}
       */


      const wrapEvent = wrapperMap => ({
        addListener(target, listener, ...args) {
          target.addListener(wrapperMap.get(listener), ...args);
        },

        hasListener(target, listener) {
          return target.hasListener(wrapperMap.get(listener));
        },

        removeListener(target, listener) {
          target.removeListener(wrapperMap.get(listener));
        }

      }); // Keep track if the deprecation warning has been logged at least once.


      let loggedSendResponseDeprecationWarning = false;
      const onMessageWrappers = new DefaultWeakMap(listener => {
        if (typeof listener !== "function") {
          return listener;
        }
        /**
         * Wraps a message listener function so that it may send responses based on
         * its return value, rather than by returning a sentinel value and calling a
         * callback. If the listener function returns a Promise, the response is
         * sent when the promise either resolves or rejects.
         *
         * @param {*} message
         *        The message sent by the other end of the channel.
         * @param {object} sender
         *        Details about the sender of the message.
         * @param {function(*)} sendResponse
         *        A callback which, when called with an arbitrary argument, sends
         *        that value as a response.
         * @returns {boolean}
         *        True if the wrapped listener returned a Promise, which will later
         *        yield a response. False otherwise.
         */


        return function onMessage(message, sender, sendResponse) {
          let didCallSendResponse = false;
          let wrappedSendResponse;
          let sendResponsePromise = new Promise(resolve => {
            wrappedSendResponse = function (response) {
              if (!loggedSendResponseDeprecationWarning) {
                console.warn(SEND_RESPONSE_DEPRECATION_WARNING, new Error().stack);
                loggedSendResponseDeprecationWarning = true;
              }

              didCallSendResponse = true;
              resolve(response);
            };
          });
          let result;

          try {
            result = listener(message, sender, wrappedSendResponse);
          } catch (err) {
            result = Promise.reject(err);
          }

          const isResultThenable = result !== true && isThenable(result); // If the listener didn't returned true or a Promise, or called
          // wrappedSendResponse synchronously, we can exit earlier
          // because there will be no response sent from this listener.

          if (result !== true && !isResultThenable && !didCallSendResponse) {
            return false;
          } // A small helper to send the message if the promise resolves
          // and an error if the promise rejects (a wrapped sendMessage has
          // to translate the message into a resolved promise or a rejected
          // promise).


          const sendPromisedResult = promise => {
            promise.then(msg => {
              // send the message value.
              sendResponse(msg);
            }, error => {
              // Send a JSON representation of the error if the rejected value
              // is an instance of error, or the object itself otherwise.
              let message;

              if (error && (error instanceof Error || typeof error.message === "string")) {
                message = error.message;
              } else {
                message = "An unexpected error occurred";
              }

              sendResponse({
                __mozWebExtensionPolyfillReject__: true,
                message
              });
            }).catch(err => {
              // Print an error on the console if unable to send the response.
              console.error("Failed to send onMessage rejected reply", err);
            });
          }; // If the listener returned a Promise, send the resolved value as a
          // result, otherwise wait the promise related to the wrappedSendResponse
          // callback to resolve and send it as a response.


          if (isResultThenable) {
            sendPromisedResult(result);
          } else {
            sendPromisedResult(sendResponsePromise);
          } // Let Chrome know that the listener is replying.


          return true;
        };
      });

      const wrappedSendMessageCallback = ({
        reject,
        resolve
      }, reply) => {
        if (extensionAPIs.runtime.lastError) {
          // Detect when none of the listeners replied to the sendMessage call and resolve
          // the promise to undefined as in Firefox.
          // See https://github.com/mozilla/webextension-polyfill/issues/130
          if (extensionAPIs.runtime.lastError.message === CHROME_SEND_MESSAGE_CALLBACK_NO_RESPONSE_MESSAGE) {
            resolve();
          } else {
            reject(extensionAPIs.runtime.lastError);
          }
        } else if (reply && reply.__mozWebExtensionPolyfillReject__) {
          // Convert back the JSON representation of the error into
          // an Error instance.
          reject(new Error(reply.message));
        } else {
          resolve(reply);
        }
      };

      const wrappedSendMessage = (name, metadata, apiNamespaceObj, ...args) => {
        if (args.length < metadata.minArgs) {
          throw new Error(`Expected at least ${metadata.minArgs} ${pluralizeArguments(metadata.minArgs)} for ${name}(), got ${args.length}`);
        }

        if (args.length > metadata.maxArgs) {
          throw new Error(`Expected at most ${metadata.maxArgs} ${pluralizeArguments(metadata.maxArgs)} for ${name}(), got ${args.length}`);
        }

        return new Promise((resolve, reject) => {
          const wrappedCb = wrappedSendMessageCallback.bind(null, {
            resolve,
            reject
          });
          args.push(wrappedCb);
          apiNamespaceObj.sendMessage(...args);
        });
      };

      const staticWrappers = {
        runtime: {
          onMessage: wrapEvent(onMessageWrappers),
          onMessageExternal: wrapEvent(onMessageWrappers),
          sendMessage: wrappedSendMessage.bind(null, "sendMessage", {
            minArgs: 1,
            maxArgs: 3
          })
        },
        tabs: {
          sendMessage: wrappedSendMessage.bind(null, "sendMessage", {
            minArgs: 2,
            maxArgs: 3
          })
        }
      };
      const settingMetadata = {
        clear: {
          minArgs: 1,
          maxArgs: 1
        },
        get: {
          minArgs: 1,
          maxArgs: 1
        },
        set: {
          minArgs: 1,
          maxArgs: 1
        }
      };
      apiMetadata.privacy = {
        network: {
          "*": settingMetadata
        },
        services: {
          "*": settingMetadata
        },
        websites: {
          "*": settingMetadata
        }
      };
      return wrapObject(extensionAPIs, staticWrappers, apiMetadata);
    };

    if (typeof chrome != "object" || !chrome || !chrome.runtime || !chrome.runtime.id) {
      throw new Error("This script should only be loaded in a browser extension.");
    } // The build process adds a UMD wrapper around this file, which makes the
    // `module` variable available.


    module.exports = wrapAPIs(chrome);
  } else {
    module.exports = browser;
  }
});


/***/ }),

/***/ 964:
/*!*******************************!*\
  !*** ./src/core/ts/config.ts ***!
  \*******************************/
/***/ ((__unused_webpack_module, exports) => {

"use strict";

Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.modelRegistry = exports.config = void 0;
const developmentBuild = "production" !== "production";
exports.config = {
    bergamotRestApiUrl: "http://127.0.0.1:8787",
    useBergamotRestApi: "0" === "1",
    sentryDsn: "https://<key>@<organization>.ingest.sentry.io/<project>",
    bergamotModelsBaseUrl: developmentBuild
        ? "http://0.0.0.0:4000/models"
        : "https://storage.googleapis.com/bergamot-models-sandbox/0.2.0",
    telemetryAppId: "org-mozilla-bergamot",
    telemetryDebugMode: developmentBuild,
    extensionBuildId: `${"v0.4.0"}-${"local"}#${"HEAD"}`,
    supportedLanguagePairs: [
        // "German, French, Spanish, Polish, Czech, and Estonian in and out of English"
        // ISO 639-1 codes
        // Language pairs that are not available are commented out
        // ["de","en"],
        // ["fr","en"],
        ["es", "en"],
        // ["pl","en"],
        // ["cs", "en"],
        ["et", "en"],
        ["en", "de"],
        // ["en","fr"],
        ["en", "es"],
        // ["en","pl"],
        // ["en", "cs"],
        ["en", "et"],
    ],
    privacyNoticeUrl: "https://example.com/privacy-notice",
    feedbackSurveyUrl: "https://qsurvey.mozilla.com/s3/bergamot-translate-product-feedback",
};
exports.modelRegistry = {
    esen: {
        lex: {
            name: "lex.50.50.esen.s2t.bin",
            size: 3860888,
            estimatedCompressedSize: 1978538,
            expectedSha256Hash: "f11a2c23ef85ab1fee1c412b908d69bc20d66fd59faa8f7da5a5f0347eddf969",
        },
        model: {
            name: "model.esen.intgemm.alphas.bin",
            size: 17140755,
            estimatedCompressedSize: 13215960,
            expectedSha256Hash: "4b6b7f451094aaa447d012658af158ffc708fc8842dde2f871a58404f5457fe0",
        },
        vocab: {
            name: "vocab.esen.spm",
            size: 825463,
            estimatedCompressedSize: 414566,
            expectedSha256Hash: "909b1eea1face0d7f90a474fe29a8c0fef8d104b6e41e65616f864c964ba8845",
        },
    },
    eten: {
        lex: {
            name: "lex.50.50.eten.s2t.bin",
            size: 3974944,
            estimatedCompressedSize: 1920655,
            expectedSha256Hash: "6992bedc590e60e610a28129c80746fe5f33144a4520e2c5508d87db14ca54f8",
        },
        model: {
            name: "model.eten.intgemm.alphas.bin",
            size: 17140754,
            estimatedCompressedSize: 12222624,
            expectedSha256Hash: "aac98a2371e216ee2d4843cbe896c617f6687501e17225ac83482eba52fd0028",
        },
        vocab: {
            name: "vocab.eten.spm",
            size: 828426,
            estimatedCompressedSize: 416995,
            expectedSha256Hash: "e3b66bc141f6123cd40746e2fb9b8ee4f89cbf324ab27d6bbf3782e52f15fa2d",
        },
    },
    ende: {
        lex: {
            name: "lex.50.50.ende.s2t.bin",
            size: 3062492,
            estimatedCompressedSize: 1575385,
            expectedSha256Hash: "764797d075f0642c0b079cce6547348d65fe4e92ac69fa6a8605cd8b53dacb3f",
        },
        model: {
            name: "model.ende.intgemm.alphas.bin",
            size: 17140498,
            estimatedCompressedSize: 13207068,
            expectedSha256Hash: "f0946515c6645304f0706fa66a051c3b7b7c507f12d0c850f276c18165a10c14",
        },
        vocab: {
            name: "vocab.deen.spm",
            size: 797501,
            estimatedCompressedSize: 412505,
            expectedSha256Hash: "bc8f8229933d8294c727f3eab12f6f064e7082b929f2d29494c8a1e619ba174c",
        },
    },
    enes: {
        lex: {
            name: "lex.50.50.enes.s2t.bin",
            size: 3347104,
            estimatedCompressedSize: 1720700,
            expectedSha256Hash: "3a113d713dec3cf1d12bba5b138ae616e28bba4bbc7fe7fd39ba145e26b86d7f",
        },
        model: {
            name: "model.enes.intgemm.alphas.bin",
            size: 17140755,
            estimatedCompressedSize: 12602853,
            expectedSha256Hash: "fa7460037a3163e03fe1d23602f964bff2331da6ee813637e092ddf37156ef53",
        },
        vocab: {
            name: "vocab.esen.spm",
            size: 825463,
            estimatedCompressedSize: 414566,
            expectedSha256Hash: "909b1eea1face0d7f90a474fe29a8c0fef8d104b6e41e65616f864c964ba8845",
        },
    },
    enet: {
        lex: {
            name: "lex.50.50.enet.s2t.bin",
            size: 2700780,
            estimatedCompressedSize: 1336443,
            expectedSha256Hash: "3d1b40ff43ebef82cf98d416a88a1ea19eb325a85785eef102f59878a63a829d",
        },
        model: {
            name: "model.enet.intgemm.alphas.bin",
            size: 17140754,
            estimatedCompressedSize: 12543318,
            expectedSha256Hash: "a28874a8b702a519a14dc71bcee726a5cb4b539eeaada2d06492f751469a1fd6",
        },
        vocab: {
            name: "vocab.eten.spm",
            size: 828426,
            estimatedCompressedSize: 416995,
            expectedSha256Hash: "e3b66bc141f6123cd40746e2fb9b8ee4f89cbf324ab27d6bbf3782e52f15fa2d",
        },
    },
};


/***/ }),

/***/ 3345:
/*!********************************************************!*\
  !*** ./src/core/ts/shared-resources/ErrorReporting.ts ***!
  \********************************************************/
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
exports.Sentry = exports.captureExceptionWithExtras = exports.initErrorReportingInContentScript = exports.initErrorReportingInBackgroundScript = exports.toggleErrorReportingBasedAsPerExtensionPreferences = void 0;
const Sentry = __importStar(__webpack_require__(/*! @sentry/browser */ 6614));
exports.Sentry = Sentry;
const config_1 = __webpack_require__(/*! ../config */ 964);
const subscribeToExtensionPreferenceChanges_1 = __webpack_require__(/*! ./state-management/subscribeToExtensionPreferenceChanges */ 4971);
const flat_1 = __webpack_require__(/*! flat */ 9602);
// Initialize Sentry SDK if we have a non-placeholder DSN configured
let sentryIntialized = false;
if (!config_1.config.sentryDsn.includes("<project>")) {
    Sentry.init({ dsn: config_1.config.sentryDsn });
    sentryIntialized = true;
}
const enableErrorReporting = () => sentryIntialized &&
    (Sentry.getCurrentHub()
        .getClient()
        .getOptions().enabled = true);
const disableErrorReporting = () => sentryIntialized &&
    (Sentry.getCurrentHub()
        .getClient()
        .getOptions().enabled = false);
// Start with Sentry disabled
disableErrorReporting();
const toggleErrorReportingBasedAsPerExtensionPreferences = (extensionPreferences) => {
    if (extensionPreferences.enableErrorReporting) {
        enableErrorReporting();
        Sentry.configureScope(function (scope) {
            scope.setTag("extension.version", extensionPreferences.extensionVersion);
            scope.setUser({
                id: extensionPreferences.extensionInstallationErrorReportingId,
            });
        });
        console.info("Enabled error reporting");
    }
    else {
        disableErrorReporting();
        console.info("Disabled error reporting");
    }
};
exports.toggleErrorReportingBasedAsPerExtensionPreferences = toggleErrorReportingBasedAsPerExtensionPreferences;
const initErrorReportingInBackgroundScript = (store, portNames) => __awaiter(void 0, void 0, void 0, function* () {
    console.info(`Inquiring about error reporting preference in background script`);
    yield subscribeToExtensionPreferenceChanges_1.subscribeToExtensionPreferenceChangesInBackgroundScript(store, exports.toggleErrorReportingBasedAsPerExtensionPreferences);
    // Set up the port listeners that initErrorReportingInContentScript()
    // expects to be available
    return subscribeToExtensionPreferenceChanges_1.communicateExtensionPreferenceChangesToContentScripts(store, portNames);
});
exports.initErrorReportingInBackgroundScript = initErrorReportingInBackgroundScript;
const initErrorReportingInContentScript = (portName) => __awaiter(void 0, void 0, void 0, function* () {
    console.info(`Inquiring about error reporting preference in "${portName}"`);
    yield subscribeToExtensionPreferenceChanges_1.subscribeToExtensionPreferenceChangesInContentScript(portName, exports.toggleErrorReportingBasedAsPerExtensionPreferences);
});
exports.initErrorReportingInContentScript = initErrorReportingInContentScript;
const captureExceptionWithExtras = (exception, extras = null, level = null) => {
    Sentry.withScope(scope => {
        if (extras !== null) {
            scope.setExtras(flat_1.flatten(extras));
        }
        if (level !== null) {
            scope.setLevel(level);
        }
        Sentry.captureException(exception);
    });
};
exports.captureExceptionWithExtras = captureExceptionWithExtras;
Sentry.configureScope(scope => {
    scope.addEventProcessor((event) => __awaiter(void 0, void 0, void 0, function* () {
        // console.log("Unprocessed sentry event", Object.assign({}, { event }));
        const normalizeUrl = url => {
            return url.replace(/(webpack_require__@)?(moz|chrome)-extension:\/\/[^\/]+\//, "~/");
        };
        if (event.exception &&
            event.exception.values &&
            event.exception.values[0]) {
            // Required for Sentry to map web extension sourcemap paths properly
            if (event.culprit) {
                event.culprit = normalizeUrl(event.culprit);
            }
            if (event.exception.values[0].stacktrace &&
                event.exception.values[0].stacktrace.frames) {
                event.exception.values[0].stacktrace.frames = event.exception.values[0].stacktrace.frames.map(frame => {
                    frame.filename = normalizeUrl(frame.filename);
                    return frame;
                });
            }
            // console.log("Processed sentry event", { event });
            return event;
        }
        return null;
    }));
});


/***/ }),

/***/ 5602:
/*!*********************************************************!*\
  !*** ./src/core/ts/shared-resources/LanguageSupport.ts ***!
  \*********************************************************/
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
exports.LanguageSupport = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const config_1 = __webpack_require__(/*! ../config */ 964);
class LanguageSupport {
    constructor() {
        this.getAcceptedTargetLanguages = () => __awaiter(this, void 0, void 0, function* () {
            return [
                ...new Set([
                    webextension_polyfill_ts_1.browser.i18n.getUILanguage(),
                    ...(yield webextension_polyfill_ts_1.browser.i18n.getAcceptLanguages()),
                ].map(localeCode => localeCode.split("-")[0])),
            ];
        });
        this.getSupportedSourceLanguages = () => {
            return [...new Set(config_1.config.supportedLanguagePairs.map(lp => lp[0]))];
        };
        this.getSupportedTargetLanguagesGivenASourceLanguage = (translateFrom) => {
            return [
                ...new Set(config_1.config.supportedLanguagePairs
                    .filter(lp => lp[0] === translateFrom)
                    .map(lp => lp[1])),
            ];
        };
        this.getAllPossiblySupportedTargetLanguages = () => {
            return [...new Set(config_1.config.supportedLanguagePairs.map(lp => lp[1]))];
        };
        this.getDefaultSourceLanguage = (translateFrom, detectedLanguage) => {
            return translateFrom || detectedLanguage;
        };
        this.getDefaultTargetLanguage = (translateFrom, acceptedTargetLanguages) => {
            const supportedTargetLanguages = translateFrom
                ? this.getSupportedTargetLanguagesGivenASourceLanguage(translateFrom)
                : this.getAllPossiblySupportedTargetLanguages();
            const possibleDefaultTargetLanguages = acceptedTargetLanguages.filter(languageCode => supportedTargetLanguages.includes(languageCode));
            if (possibleDefaultTargetLanguages.length) {
                return possibleDefaultTargetLanguages[0];
            }
            return null;
        };
        this.summarizeLanguageSupport = (detectedLanguageResults) => __awaiter(this, void 0, void 0, function* () {
            const acceptedTargetLanguages = yield this.getAcceptedTargetLanguages();
            const defaultSourceLanguage = this.getDefaultSourceLanguage(null, detectedLanguageResults === null || detectedLanguageResults === void 0 ? void 0 : detectedLanguageResults.language);
            const defaultTargetLanguage = this.getDefaultTargetLanguage(defaultSourceLanguage, acceptedTargetLanguages);
            const supportedSourceLanguages = this.getSupportedSourceLanguages();
            const supportedTargetLanguagesGivenDefaultSourceLanguage = this.getSupportedTargetLanguagesGivenASourceLanguage(defaultSourceLanguage);
            const allPossiblySupportedTargetLanguages = this.getAllPossiblySupportedTargetLanguages();
            return {
                acceptedTargetLanguages,
                defaultSourceLanguage,
                defaultTargetLanguage,
                supportedSourceLanguages,
                supportedTargetLanguagesGivenDefaultSourceLanguage,
                allPossiblySupportedTargetLanguages,
            };
        });
    }
}
exports.LanguageSupport = LanguageSupport;


/***/ }),

/***/ 4779:
/*!*********************************************************************!*\
  !*** ./src/core/ts/shared-resources/models/BaseTranslationState.ts ***!
  \*********************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.BaseTranslationState = exports.TranslationStatus = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const mobx_1 = __webpack_require__(/*! mobx */ 9637);
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
/* eslint-disable no-unused-vars, no-shadow */
// TODO: update typescript-eslint when support for this kind of declaration is supported
var TranslationStatus;
(function (TranslationStatus) {
    TranslationStatus["UNKNOWN"] = "UNKNOWN";
    TranslationStatus["UNAVAILABLE"] = "UNAVAILABLE";
    TranslationStatus["DETECTING_LANGUAGE"] = "DETECTING_LANGUAGE";
    TranslationStatus["LANGUAGE_NOT_DETECTED"] = "LANGUAGE_NOT_DETECTED";
    TranslationStatus["SOURCE_LANGUAGE_UNDERSTOOD"] = "SOURCE_LANGUAGE_UNDERSTOOD";
    TranslationStatus["TRANSLATION_UNSUPPORTED"] = "TRANSLATION_UNSUPPORTED";
    TranslationStatus["OFFER"] = "OFFER";
    TranslationStatus["DOWNLOADING_TRANSLATION_MODEL"] = "DOWNLOADING_TRANSLATION_MODEL";
    TranslationStatus["TRANSLATING"] = "TRANSLATING";
    TranslationStatus["TRANSLATED"] = "TRANSLATED";
    TranslationStatus["ERROR"] = "ERROR";
})(TranslationStatus = exports.TranslationStatus || (exports.TranslationStatus = {}));
/* eslint-enable no-unused-vars, no-shadow */
let BaseTranslationState = class BaseTranslationState extends mobx_keystone_1.Model({
    isVisible: mobx_keystone_1.prop({ setterAction: true }),
    displayQualityEstimation: mobx_keystone_1.prop({ setterAction: true }),
    translationRequested: mobx_keystone_1.prop({ setterAction: true }),
    cancellationRequested: mobx_keystone_1.prop({ setterAction: true }),
    detectedLanguageResults: mobx_keystone_1.prop(() => null, {
        setterAction: true,
    }),
    translateFrom: mobx_keystone_1.prop({ setterAction: true }),
    translateTo: mobx_keystone_1.prop({ setterAction: true }),
    translationStatus: mobx_keystone_1.prop(TranslationStatus.UNKNOWN, {
        setterAction: true,
    }),
    tabId: mobx_keystone_1.prop(),
    wordCount: mobx_keystone_1.prop(),
    wordCountVisible: mobx_keystone_1.prop(),
    wordCountVisibleInViewport: mobx_keystone_1.prop(),
    translationInitiationTimestamp: mobx_keystone_1.prop(),
    totalModelLoadWallTimeMs: mobx_keystone_1.prop(),
    totalTranslationWallTimeMs: mobx_keystone_1.prop(),
    totalTranslationEngineRequestCount: mobx_keystone_1.prop(),
    queuedTranslationEngineRequestCount: mobx_keystone_1.prop(),
    modelLoadNecessary: mobx_keystone_1.prop(),
    modelDownloadNecessary: mobx_keystone_1.prop(),
    modelDownloading: mobx_keystone_1.prop(),
    modelDownloadProgress: mobx_keystone_1.prop(),
    modelLoading: mobx_keystone_1.prop(),
    modelLoaded: mobx_keystone_1.prop(),
    translationFinished: mobx_keystone_1.prop(),
    modelLoadErrorOccurred: mobx_keystone_1.prop(),
    modelDownloadErrorOccurred: mobx_keystone_1.prop(),
    translationErrorOccurred: mobx_keystone_1.prop(),
    otherErrorOccurred: mobx_keystone_1.prop(),
}) {
    get effectiveTranslateFrom() {
        var _a;
        return this.translateFrom || ((_a = this.detectedLanguageResults) === null || _a === void 0 ? void 0 : _a.language);
    }
    get effectiveTranslateTo() {
        const browserUiLanguageCode = webextension_polyfill_ts_1.browser.i18n.getUILanguage().split("-")[0];
        return this.translateTo || browserUiLanguageCode;
    }
};
__decorate([
    mobx_1.computed
], BaseTranslationState.prototype, "effectiveTranslateFrom", null);
__decorate([
    mobx_1.computed
], BaseTranslationState.prototype, "effectiveTranslateTo", null);
BaseTranslationState = __decorate([
    mobx_keystone_1.model("bergamotTranslate/BaseTranslationState")
], BaseTranslationState);
exports.BaseTranslationState = BaseTranslationState;


/***/ }),

/***/ 5482:
/*!*************************************************************************!*\
  !*** ./src/core/ts/shared-resources/models/DocumentTranslationState.ts ***!
  \*************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.DocumentTranslationState = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const TabTranslationState_1 = __webpack_require__(/*! ./TabTranslationState */ 6556);
let DocumentTranslationState = class DocumentTranslationState extends mobx_keystone_1.ExtendedModel(TabTranslationState_1.TabTranslationState, {
    frameId: mobx_keystone_1.prop(),
}) {
};
DocumentTranslationState = __decorate([
    mobx_keystone_1.model("bergamotTranslate/DocumentTranslationState")
], DocumentTranslationState);
exports.DocumentTranslationState = DocumentTranslationState;


/***/ }),

/***/ 65:
/*!***************************************************************!*\
  !*** ./src/core/ts/shared-resources/models/ExtensionState.ts ***!
  \***************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.ExtensionState = exports.translateOwnTextTranslationStateMapKey = exports.documentTranslationStateMapKey = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const mobx_1 = __webpack_require__(/*! mobx */ 9637);
const TabTranslationState_1 = __webpack_require__(/*! ./TabTranslationState */ 6556);
const BaseTranslationState_1 = __webpack_require__(/*! ./BaseTranslationState */ 4779);
const documentTranslationStateMapKey = (frameInfo) => `${frameInfo.tabId}-${frameInfo.frameId}`;
exports.documentTranslationStateMapKey = documentTranslationStateMapKey;
const translateOwnTextTranslationStateMapKey = (totts) => `${totts.tabId}`;
exports.translateOwnTextTranslationStateMapKey = translateOwnTextTranslationStateMapKey;
let ExtensionState = class ExtensionState extends mobx_keystone_1.Model({
    documentTranslationStates: mobx_keystone_1.prop_mapObject(() => new Map()),
    fragmentTranslationStates: mobx_keystone_1.prop_mapObject(() => new Map()),
    translateOwnTextTranslationStates: mobx_keystone_1.prop_mapObject(() => new Map()),
}) {
    setDocumentTranslationState(documentTranslationState) {
        this.documentTranslationStates.set(exports.documentTranslationStateMapKey(documentTranslationState), documentTranslationState);
    }
    patchDocumentTranslationStateByFrameInfo(frameInfo, patches) {
        const key = exports.documentTranslationStateMapKey(frameInfo);
        const dts = this.documentTranslationStates.get(key);
        mobx_keystone_1.applyPatches(dts, patches);
        this.documentTranslationStates.set(key, dts);
    }
    deleteDocumentTranslationStateByFrameInfo(frameInfo) {
        this.documentTranslationStates.delete(exports.documentTranslationStateMapKey(frameInfo));
    }
    setTranslateOwnTextTranslationState(translateOwnTextTranslationState) {
        this.translateOwnTextTranslationStates.set(exports.translateOwnTextTranslationStateMapKey(translateOwnTextTranslationState), translateOwnTextTranslationState);
    }
    deleteTranslateOwnTextTranslationState(translateOwnTextTranslationState) {
        this.translateOwnTextTranslationStates.delete(exports.translateOwnTextTranslationStateMapKey(translateOwnTextTranslationState));
    }
    get documentTranslationStatesByTabId() {
        const map = new Map();
        this.documentTranslationStates.forEach((documentTranslationState) => {
            if (map.has(documentTranslationState.tabId)) {
                map.set(documentTranslationState.tabId, [
                    ...map.get(documentTranslationState.tabId),
                    documentTranslationState,
                ]);
            }
            else {
                map.set(documentTranslationState.tabId, [documentTranslationState]);
            }
        });
        return map;
    }
    get tabTranslationStates() {
        const map = new Map();
        this.documentTranslationStatesByTabId.forEach((documentTranslationStates, tabId) => {
            const tabTopFrameState = documentTranslationStates.find((dts) => dts.frameId === 0);
            // If the top frame state is unavailable (may happen during the early stages of a
            // document translation state model instance), return an equally uninformative tab translation state
            if (!tabTopFrameState) {
                const tabTranslationState = new TabTranslationState_1.TabTranslationState(Object.assign({ tabId }, mobx_keystone_1.getSnapshot(documentTranslationStates[0])));
                map.set(tabId, tabTranslationState);
                return;
            }
            // Use top frame state attributes to represent most of the tab state if available
            const { isVisible, displayQualityEstimation, translationRequested, cancellationRequested, detectedLanguageResults, translateFrom, translateTo, windowId, showOriginal, url, } = mobx_keystone_1.getSnapshot(tabTopFrameState);
            // Sum some state attributes
            const wordCount = documentTranslationStates
                .map(dts => dts.wordCount)
                .reduce((a, b) => a + b, 0);
            const wordCountVisible = documentTranslationStates
                .map(dts => dts.wordCountVisible)
                .reduce((a, b) => a + b, 0);
            const wordCountVisibleInViewport = documentTranslationStates
                .map(dts => dts.wordCountVisibleInViewport)
                .reduce((a, b) => a + b, 0);
            const totalModelLoadWallTimeMs = documentTranslationStates
                .map(dts => dts.totalModelLoadWallTimeMs)
                .reduce((a, b) => a + b, 0);
            const totalTranslationWallTimeMs = documentTranslationStates
                .map(dts => dts.totalTranslationWallTimeMs)
                .reduce((a, b) => a + b, 0);
            const totalTranslationEngineRequestCount = documentTranslationStates
                .map(dts => dts.totalTranslationEngineRequestCount)
                .reduce((a, b) => a + b, 0);
            const queuedTranslationEngineRequestCount = documentTranslationStates
                .map(dts => dts.queuedTranslationEngineRequestCount)
                .reduce((a, b) => a + b, 0);
            // Merge translation-progress-related booleans as per src/core/ts/shared-resources/state-management/DocumentTranslationStateCommunicator.ts
            const translationInitiationTimestamps = documentTranslationStates.map((dts) => dts.translationInitiationTimestamp);
            const translationInitiationTimestamp = Math.min(...translationInitiationTimestamps);
            const modelLoadNecessary = !!documentTranslationStates.filter((dts) => dts.modelLoadNecessary).length;
            const modelDownloadNecessary = !!documentTranslationStates.filter((dts) => dts.modelDownloadNecessary).length;
            const modelDownloading = !!documentTranslationStates.filter((dts) => dts.modelDownloading).length;
            const modelLoading = modelLoadNecessary
                ? !!documentTranslationStates.find((dts) => dts.modelLoading)
                : undefined;
            const modelLoaded = modelLoadNecessary
                ? !!documentTranslationStates.find((dts) => !dts.modelLoaded)
                : undefined;
            const translationFinished = documentTranslationStates.filter((dts) => !dts.translationFinished).length === 0;
            // Merge model download progress as per src/core/ts/shared-resources/state-management/DocumentTranslationStateCommunicator.ts
            const emptyDownloadProgress = {
                bytesDownloaded: 0,
                bytesToDownload: 0,
                startTs: undefined,
                durationMs: 0,
                endTs: undefined,
            };
            const modelDownloadProgress = documentTranslationStates
                .map((dts) => mobx_keystone_1.getSnapshot(dts.modelDownloadProgress))
                .filter((mdp) => mdp)
                .reduce((a, b) => {
                const startTs = a.startTs && a.startTs <= b.startTs ? a.startTs : b.startTs;
                const endTs = a.endTs && a.endTs >= b.endTs ? a.endTs : b.endTs;
                return {
                    bytesDownloaded: a.bytesDownloaded + b.bytesDownloaded,
                    bytesToDownload: a.bytesToDownload + b.bytesToDownload,
                    startTs,
                    durationMs: endTs ? endTs - startTs : Date.now() - startTs,
                    endTs,
                };
            }, emptyDownloadProgress);
            // Merge errorOccurred attributes
            const modelLoadErrorOccurred = !!documentTranslationStates.filter((dts) => dts.modelLoadErrorOccurred).length;
            const modelDownloadErrorOccurred = !!documentTranslationStates.filter((dts) => dts.modelDownloadErrorOccurred).length;
            const translationErrorOccurred = !!documentTranslationStates.filter((dts) => dts.translationErrorOccurred).length;
            const otherErrorOccurred = !!documentTranslationStates.filter((dts) => dts.otherErrorOccurred).length;
            // Special merging of translation status
            const anyTabHasTranslationStatus = (translationStatus) => documentTranslationStates.find(dts => dts.translationStatus === translationStatus);
            const tabTranslationStatus = () => {
                if (anyTabHasTranslationStatus(BaseTranslationState_1.TranslationStatus.DETECTING_LANGUAGE)) {
                    return BaseTranslationState_1.TranslationStatus.DETECTING_LANGUAGE;
                }
                if (anyTabHasTranslationStatus(BaseTranslationState_1.TranslationStatus.DOWNLOADING_TRANSLATION_MODEL)) {
                    return BaseTranslationState_1.TranslationStatus.DOWNLOADING_TRANSLATION_MODEL;
                }
                if (anyTabHasTranslationStatus(BaseTranslationState_1.TranslationStatus.TRANSLATING)) {
                    return BaseTranslationState_1.TranslationStatus.TRANSLATING;
                }
                if (anyTabHasTranslationStatus(BaseTranslationState_1.TranslationStatus.ERROR)) {
                    return BaseTranslationState_1.TranslationStatus.ERROR;
                }
                return tabTopFrameState.translationStatus;
            };
            const translationStatus = tabTranslationStatus();
            const tabTranslationStateData = {
                isVisible,
                displayQualityEstimation,
                translationRequested,
                cancellationRequested,
                detectedLanguageResults,
                translateFrom,
                translateTo,
                translationStatus,
                tabId,
                windowId,
                showOriginal,
                url,
                wordCount,
                wordCountVisible,
                wordCountVisibleInViewport,
                translationInitiationTimestamp,
                totalModelLoadWallTimeMs,
                totalTranslationWallTimeMs,
                totalTranslationEngineRequestCount,
                queuedTranslationEngineRequestCount,
                modelLoadNecessary,
                modelDownloadNecessary,
                modelDownloading,
                modelDownloadProgress,
                modelLoading,
                modelLoaded,
                translationFinished,
                modelLoadErrorOccurred,
                modelDownloadErrorOccurred,
                translationErrorOccurred,
                otherErrorOccurred,
            };
            const tabTranslationState = new TabTranslationState_1.TabTranslationState(tabTranslationStateData);
            map.set(tabId, tabTranslationState);
        });
        return map;
    }
    tabSpecificDocumentTranslationStates(tabId) {
        const result = [];
        this.documentTranslationStates.forEach((documentTranslationState) => {
            if (documentTranslationState.tabId === tabId) {
                result.push(documentTranslationState);
            }
        });
        return result;
    }
    requestTranslationOfAllFramesInTab(tabId, from, to) {
        // Request translation of all frames in a specific tab
        this.tabSpecificDocumentTranslationStates(tabId).forEach((dts) => {
            this.patchDocumentTranslationStateByFrameInfo(dts, [
                {
                    op: "replace",
                    path: ["translateFrom"],
                    value: from,
                },
                {
                    op: "replace",
                    path: ["translateTo"],
                    value: to,
                },
                {
                    op: "replace",
                    path: ["translationRequested"],
                    value: true,
                },
            ]);
        });
    }
    showOriginalInTab(tabId) {
        this.tabSpecificDocumentTranslationStates(tabId).forEach((dts) => {
            this.patchDocumentTranslationStateByFrameInfo(dts, [
                {
                    op: "replace",
                    path: ["showOriginal"],
                    value: true,
                },
            ]);
        });
    }
    hideOriginalInTab(tabId) {
        this.tabSpecificDocumentTranslationStates(tabId).forEach((dts) => {
            this.patchDocumentTranslationStateByFrameInfo(dts, [
                {
                    op: "replace",
                    path: ["showOriginal"],
                    value: false,
                },
            ]);
        });
    }
};
__decorate([
    mobx_keystone_1.modelAction
], ExtensionState.prototype, "setDocumentTranslationState", null);
__decorate([
    mobx_keystone_1.modelAction
], ExtensionState.prototype, "patchDocumentTranslationStateByFrameInfo", null);
__decorate([
    mobx_keystone_1.modelAction
], ExtensionState.prototype, "deleteDocumentTranslationStateByFrameInfo", null);
__decorate([
    mobx_keystone_1.modelAction
], ExtensionState.prototype, "setTranslateOwnTextTranslationState", null);
__decorate([
    mobx_keystone_1.modelAction
], ExtensionState.prototype, "deleteTranslateOwnTextTranslationState", null);
__decorate([
    mobx_1.computed
], ExtensionState.prototype, "documentTranslationStatesByTabId", null);
__decorate([
    mobx_1.computed
], ExtensionState.prototype, "tabTranslationStates", null);
ExtensionState = __decorate([
    mobx_keystone_1.model("bergamotTranslate/ExtensionState")
], ExtensionState);
exports.ExtensionState = ExtensionState;


/***/ }),

/***/ 6556:
/*!********************************************************************!*\
  !*** ./src/core/ts/shared-resources/models/TabTranslationState.ts ***!
  \********************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.TabTranslationState = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const BaseTranslationState_1 = __webpack_require__(/*! ./BaseTranslationState */ 4779);
let TabTranslationState = class TabTranslationState extends mobx_keystone_1.ExtendedModel(BaseTranslationState_1.BaseTranslationState, {
    windowId: mobx_keystone_1.prop(),
    showOriginal: mobx_keystone_1.prop({ setterAction: true }),
    url: mobx_keystone_1.prop(),
}) {
};
TabTranslationState = __decorate([
    mobx_keystone_1.model("bergamotTranslate/TabTranslationState")
], TabTranslationState);
exports.TabTranslationState = TabTranslationState;


/***/ }),

/***/ 8238:
/*!*********************************************************************************!*\
  !*** ./src/core/ts/shared-resources/models/TranslateOwnTextTranslationState.ts ***!
  \*********************************************************************************/
/***/ (function(__unused_webpack_module, exports, __webpack_require__) {

"use strict";

/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */
var __decorate = (this && this.__decorate) || function (decorators, target, key, desc) {
    var c = arguments.length, r = c < 3 ? target : desc === null ? desc = Object.getOwnPropertyDescriptor(target, key) : desc, d;
    if (typeof Reflect === "object" && typeof Reflect.decorate === "function") r = Reflect.decorate(decorators, target, key, desc);
    else for (var i = decorators.length - 1; i >= 0; i--) if (d = decorators[i]) r = (c < 3 ? d(r) : c > 3 ? d(target, key, r) : d(target, key)) || r;
    return c > 3 && r && Object.defineProperty(target, key, r), r;
};
Object.defineProperty(exports, "__esModule", ({ value: true }));
exports.TranslateOwnTextTranslationState = void 0;
const mobx_keystone_1 = __webpack_require__(/*! mobx-keystone */ 7680);
const BaseTranslationState_1 = __webpack_require__(/*! ./BaseTranslationState */ 4779);
let TranslateOwnTextTranslationState = class TranslateOwnTextTranslationState extends mobx_keystone_1.ExtendedModel(BaseTranslationState_1.BaseTranslationState, {
    translateAutomatically: mobx_keystone_1.prop(true),
}) {
};
TranslateOwnTextTranslationState = __decorate([
    mobx_keystone_1.model("bergamotTranslate/TranslateOwnTextTranslationState")
], TranslateOwnTextTranslationState);
exports.TranslateOwnTextTranslationState = TranslateOwnTextTranslationState;


/***/ }),

/***/ 4971:
/*!************************************************************************************************!*\
  !*** ./src/core/ts/shared-resources/state-management/subscribeToExtensionPreferenceChanges.ts ***!
  \************************************************************************************************/
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
exports.communicateExtensionPreferenceChangesToContentScripts = exports.subscribeToExtensionPreferenceChangesInContentScript = exports.subscribeToExtensionPreferenceChangesInBackgroundScript = void 0;
const webextension_polyfill_ts_1 = __webpack_require__(/*! webextension-polyfill-ts */ 3624);
const subscribeToExtensionPreferenceChangesInBackgroundScript = (store, callback) => __awaiter(void 0, void 0, void 0, function* () {
    const extensionPreferences = yield store.getExtensionPreferences();
    callback(extensionPreferences);
    const originalSetExtensionPreferences = store.setExtensionPreferences.bind(store);
    store.setExtensionPreferences = ($extensionPreferences) => __awaiter(void 0, void 0, void 0, function* () {
        yield originalSetExtensionPreferences($extensionPreferences);
        callback(yield store.getExtensionPreferences());
    });
});
exports.subscribeToExtensionPreferenceChangesInBackgroundScript = subscribeToExtensionPreferenceChangesInBackgroundScript;
const subscribeToExtensionPreferenceChangesInContentScript = (portName, callback) => __awaiter(void 0, void 0, void 0, function* () {
    const port = webextension_polyfill_ts_1.browser.runtime.connect(webextension_polyfill_ts_1.browser.runtime.id, {
        name: portName,
    });
    return new Promise((resolve, reject) => {
        port.postMessage({
            requestExtensionPreferences: true,
        });
        port.onMessage.addListener((m) => __awaiter(void 0, void 0, void 0, function* () {
            if (m.extensionPreferences) {
                const { extensionPreferences } = m;
                callback(extensionPreferences);
                resolve();
            }
            reject("Unexpected message");
        }));
    });
});
exports.subscribeToExtensionPreferenceChangesInContentScript = subscribeToExtensionPreferenceChangesInContentScript;
/**
 * Set up communication channels with content scripts so that they
 * can subscribe to changes in extension preferences
 * @param store
 * @param portNames
 */
const communicateExtensionPreferenceChangesToContentScripts = (store, portNames) => __awaiter(void 0, void 0, void 0, function* () {
    const connectedPorts = {};
    // Broadcast updates to extension preferences
    const originalSetExtensionPreferences = store.setExtensionPreferences.bind(store);
    store.setExtensionPreferences = (extensionPreferences) => __awaiter(void 0, void 0, void 0, function* () {
        yield originalSetExtensionPreferences(extensionPreferences);
        Object.keys(connectedPorts).forEach((portName) => __awaiter(void 0, void 0, void 0, function* () {
            const port = connectedPorts[portName];
            port.postMessage({
                extensionPreferences: yield store.getExtensionPreferences(),
            });
        }));
    });
    const extensionPreferencesContentScriptPortListener = (port) => {
        if (!portNames.includes(port.name)) {
            return;
        }
        port.onMessage.addListener(function (m) {
            return __awaiter(this, void 0, void 0, function* () {
                connectedPorts[port.name] = port;
                // console.debug(`Message from port "${port.name}"`, { m });
                if (m.requestExtensionPreferences) {
                    port.postMessage({
                        extensionPreferences: yield store.getExtensionPreferences(),
                    });
                }
                if (m.saveExtensionPreferences) {
                    const { updatedExtensionPreferences } = m.saveExtensionPreferences;
                    yield store.setExtensionPreferences(updatedExtensionPreferences);
                    port.postMessage({
                        extensionPreferences: yield store.getExtensionPreferences(),
                    });
                }
            });
        });
        port.onDisconnect.addListener(($port) => {
            delete connectedPorts[$port.name];
        });
    };
    webextension_polyfill_ts_1.browser.runtime.onConnect.addListener(extensionPreferencesContentScriptPortListener);
    return extensionPreferencesContentScriptPortListener;
});
exports.communicateExtensionPreferenceChangesToContentScripts = communicateExtensionPreferenceChangesToContentScripts;


/***/ })

}]);
//# sourceMappingURL=commons.js.map