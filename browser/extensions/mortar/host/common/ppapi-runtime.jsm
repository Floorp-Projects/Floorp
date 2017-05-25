/* -*- Mode: Java; tab-width: 2; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { classes: Cc, interfaces: Ci, results: Cr, utils: Cu } = Components;

Cu.import("resource://gre/modules/ctypes.jsm");
Cu.import("resource://gre/modules/Services.jsm");
Cu.import("resource://ppapi.js/opengles2-utils.jsm");
Cu.importGlobalProperties(['URL']);

const PP_OK = 0;
const PP_OK_COMPLETIONPENDING = -1;
const PP_ERROR_FAILED = -2;
const PP_ERROR_ABORTED = -3;
const PP_ERROR_BADARGUMENT = -4;
const PP_ERROR_BADRESOURCE = -5;
const PP_ERROR_NOINTERFACE = -6;
const PP_ERROR_NOACCESS = -7;
const PP_ERROR_NOMEMORY = -8;
const PP_ERROR_NOSPACE = -9;
const PP_ERROR_NOQUOTA = -10;
const PP_ERROR_INPROGRESS = -11;
const PP_ERROR_NOTSUPPORTED = -12;
const PP_ERROR_BLOCKS_MAIN_THREAD = -13;
const PP_ERROR_MALFORMED_INPUT = -14;
const PP_ERROR_RESOURCE_FAILED = -15;
const PP_ERROR_FILENOTFOUND = -20;
const PP_ERROR_FILEEXISTS = -21;
const PP_ERROR_FILETOOBIG = -22;
const PP_ERROR_FILECHANGED = -23;
const PP_ERROR_NOTAFILE = -24;
const PP_ERROR_TIMEDOUT = -30;
const PP_ERROR_USERCANCEL = -40;
const PP_ERROR_NO_USER_GESTURE = -41;
const PP_ERROR_CONTEXT_LOST = -50;
const PP_ERROR_NO_MESSAGE_LOOP = -51;
const PP_ERROR_WRONG_THREAD = -52;
const PP_ERROR_WOULD_BLOCK_THREAD = -53;
const PP_ERROR_CONNECTION_CLOSED = -100;
const PP_ERROR_CONNECTION_RESET = -101;
const PP_ERROR_CONNECTION_REFUSED = -102;
const PP_ERROR_CONNECTION_ABORTED = -103;
const PP_ERROR_CONNECTION_FAILED = -104;
const PP_ERROR_CONNECTION_TIMEDOUT = -105;
const PP_ERROR_ADDRESS_INVALID = -106;
const PP_ERROR_ADDRESS_UNREACHABLE = -107;
const PP_ERROR_ADDRESS_IN_USE = -108;
const PP_ERROR_MESSAGE_TOO_BIG = -109;
const PP_ERROR_NAME_NOT_RESOLVED = -110;

// Point is defined as 1/72 of an inch (25.4mm)
const POINT_PER_INCH = 72;
const POINT_PER_MILLIMETER = POINT_PER_INCH / 25.4;

const PRINT_FILE_NAME = "print.pdf";
const PRINT_TEMP_KEY = "TmpD";

const PP_Bool = {
  PP_FALSE: 0,
  PP_TRUE: 1,
};

const PP_AudioFrameSize = {
  PP_AUDIOMINSAMPLEFRAMECOUNT: 64,
  PP_AUDIOMAXSAMPLEFRAMECOUNT: 32768,
};

const PP_BrowserFont_Trusted_Family = {
  PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT: 0,
  PP_BROWSERFONT_TRUSTED_FAMILY_SERIF: 1,
  PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF: 2,
  PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE: 3,
};

const PP_BrowserFont_Trusted_Weight = {
  PP_BROWSERFONT_TRUSTED_WEIGHT_100: 0,
  PP_BROWSERFONT_TRUSTED_WEIGHT_200: 1,
  PP_BROWSERFONT_TRUSTED_WEIGHT_300: 2,
  PP_BROWSERFONT_TRUSTED_WEIGHT_400: 3,
  PP_BROWSERFONT_TRUSTED_WEIGHT_500: 4,
  PP_BROWSERFONT_TRUSTED_WEIGHT_600: 5,
  PP_BROWSERFONT_TRUSTED_WEIGHT_700: 6,
  PP_BROWSERFONT_TRUSTED_WEIGHT_800: 7,
  PP_BROWSERFONT_TRUSTED_WEIGHT_900: 8,
  PP_BROWSERFONT_TRUSTED_WEIGHT_NORMAL: 3,
  PP_BROWSERFONT_TRUSTED_WEIGHT_BOLD: 6,
};

const PP_CursorType_Dev = {
  PP_CURSORTYPE_CUSTOM: -1,
  PP_CURSORTYPE_POINTER: 0,
  PP_CURSORTYPE_CROSS: 1,
  PP_CURSORTYPE_HAND: 2,
  PP_CURSORTYPE_IBEAM: 3,
  PP_CURSORTYPE_WAIT: 4,
  PP_CURSORTYPE_HELP: 5,
  PP_CURSORTYPE_EASTRESIZE: 6,
  PP_CURSORTYPE_NORTHRESIZE: 7,
  PP_CURSORTYPE_NORTHEASTRESIZE: 8,
  PP_CURSORTYPE_NORTHWESTRESIZE: 9,
  PP_CURSORTYPE_SOUTHRESIZE: 10,
  PP_CURSORTYPE_SOUTHEASTRESIZE: 11,
  PP_CURSORTYPE_SOUTHWESTRESIZE: 12,
  PP_CURSORTYPE_WESTRESIZE: 13,
  PP_CURSORTYPE_NORTHSOUTHRESIZE: 14,
  PP_CURSORTYPE_EASTWESTRESIZE: 15,
  PP_CURSORTYPE_NORTHEASTSOUTHWESTRESIZE: 16,
  PP_CURSORTYPE_NORTHWESTSOUTHEASTRESIZE: 17,
  PP_CURSORTYPE_COLUMNRESIZE: 18,
  PP_CURSORTYPE_ROWRESIZE: 19,
  PP_CURSORTYPE_MIDDLEPANNING: 20,
  PP_CURSORTYPE_EASTPANNING: 21,
  PP_CURSORTYPE_NORTHPANNING: 22,
  PP_CURSORTYPE_NORTHEASTPANNING: 23,
  PP_CURSORTYPE_NORTHWESTPANNING: 24,
  PP_CURSORTYPE_SOUTHPANNING: 25,
  PP_CURSORTYPE_SOUTHEASTPANNING: 26,
  PP_CURSORTYPE_SOUTHWESTPANNING: 27,
  PP_CURSORTYPE_WESTPANNING: 28,
  PP_CURSORTYPE_MOVE: 29,
  PP_CURSORTYPE_VERTICALTEXT: 30,
  PP_CURSORTYPE_CELL: 31,
  PP_CURSORTYPE_CONTEXTMENU: 32,
  PP_CURSORTYPE_ALIAS: 33,
  PP_CURSORTYPE_PROGRESS: 34,
  PP_CURSORTYPE_NODROP: 35,
  PP_CURSORTYPE_COPY: 36,
  PP_CURSORTYPE_NONE: 37,
  PP_CURSORTYPE_NOTALLOWED: 38,
  PP_CURSORTYPE_ZOOMIN: 39,
  PP_CURSORTYPE_ZOOMOUT: 40,
  PP_CURSORTYPE_GRAB: 41,
  PP_CURSORTYPE_GRABBING: 42,
};

const PP_FileOpenFlags = {
  PP_FILEOPENFLAG_READ: 1 << 0,
  PP_FILEOPENFLAG_WRITE: 1 << 1,
  PP_FILEOPENFLAG_CREATE: 1 << 2,
  PP_FILEOPENFLAG_TRUNCATE: 1 << 3,
  PP_FILEOPENFLAG_EXCLUSIVE: 1 << 4,
  PP_FILEOPENFLAG_APPEND: 1 << 5
};

const PP_FileSystemType = {
  PP_FILESYSTEMTYPE_INVALID: 0,
  PP_FILESYSTEMTYPE_EXTERNAL: 1,
  PP_FILESYSTEMTYPE_LOCALPERSISTENT: 2,
  PP_FILESYSTEMTYPE_LOCALTEMPORARY: 3,
  PP_FILESYSTEMTYPE_ISOLATED: 4
};

const PP_FileType = {
  PP_FILETYPE_REGULAR: 0,
  PP_FILETYPE_DIRECTORY: 1,
  PP_FILETYPE_OTHER: 2
};

const PP_FlashLSORestrictions = {
  PP_FLASHLSORESTRICTIONS_NONE: 1,
  PP_FLASHLSORESTRICTIONS_BLOC: 2,
  PP_FLASHLSORESTRICTIONS_IN_MEMORY: 3,
};

const PP_FlashSetting = {
  PP_FLASHSETTING_3DENABLED: 1,
  PP_FLASHSETTING_INCOGNITO: 2,
  PP_FLASHSETTING_STAGE3DENABLED: 3,
  PP_FLASHSETTING_LANGUAGE: 4,
  PP_FLASHSETTING_NUMCORES: 5,
  PP_FLASHSETTING_LSORESTRICTIONS: 6,
  PP_FLASHSETTING_STAGE3DBASELINEENABLED: 7,
};

const PP_Graphics3DAttrib = {
  PP_GRAPHICS3DATTRIB_ALPHA_SIZE: 0x3021,
  PP_GRAPHICS3DATTRIB_BLUE_SIZE: 0x3022,
  PP_GRAPHICS3DATTRIB_GREEN_SIZE: 0x3023,
  PP_GRAPHICS3DATTRIB_RED_SIZE: 0x3024,
  PP_GRAPHICS3DATTRIB_DEPTH_SIZE: 0x3025,
  PP_GRAPHICS3DATTRIB_STENCIL_SIZE: 0x3026,
  PP_GRAPHICS3DATTRIB_SAMPLES: 0x3031,
  PP_GRAPHICS3DATTRIB_SAMPLE_BUFFERS: 0x3032,
  PP_GRAPHICS3DATTRIB_NONE: 0x3038,
  PP_GRAPHICS3DATTRIB_HEIGHT: 0x3056,
  PP_GRAPHICS3DATTRIB_WIDTH: 0x3057,
  PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR: 0x3093,
  PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED: 0x3094,
  PP_GRAPHICS3DATTRIB_BUFFER_DESTROYED: 0x3095,
  PP_GRAPHICS3DATTRIB_GPU_PREFERENCE: 0x11000,
  PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER: 0x11001,
  PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_PERFORMANCE: 0x11002
};

const PP_ImageDataFormat = {
  PP_IMAGEDATAFORMAT_BGRA_PREMUL: 0,
  PP_IMAGEDATAFORMAT_RGBA_PREMUL: 1,
};

const PP_InputEvent_Class = {
  PP_INPUTEVENT_CLASS_MOUSE: 1 << 0,
  PP_INPUTEVENT_CLASS_KEYBOARD: 1 << 1,
  PP_INPUTEVENT_CLASS_WHEEL: 1 << 2,
  PP_INPUTEVENT_CLASS_TOUCH: 1 << 3,
  PP_INPUTEVENT_CLASS_IME: 1 << 4
};

const PP_InputEvent_Modifier = {
  PP_INPUTEVENT_MODIFIER_SHIFTKEY: 1 << 0,
  PP_INPUTEVENT_MODIFIER_CONTROLKEY: 1 << 1,
  PP_INPUTEVENT_MODIFIER_ALTKEY: 1 << 2,
  PP_INPUTEVENT_MODIFIER_METAKEY: 1 << 3,
  PP_INPUTEVENT_MODIFIER_ISKEYPAD: 1 << 4,
  PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT: 1 << 5,
  PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN: 1 << 6,
  PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN: 1 << 7,
  PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN: 1 << 8,
  PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY: 1 << 9,
  PP_INPUTEVENT_MODIFIER_NUMLOCKKEY: 1 << 10,
  PP_INPUTEVENT_MODIFIER_ISLEFT: 1 << 11,
  PP_INPUTEVENT_MODIFIER_ISRIGHT: 1 << 12
};

const PP_InputEvent_MouseButton = {
  PP_INPUTEVENT_MOUSEBUTTON_NONE: -1,
  PP_INPUTEVENT_MOUSEBUTTON_LEFT: 0,
  PP_INPUTEVENT_MOUSEBUTTON_MIDDLE: 1,
  PP_INPUTEVENT_MOUSEBUTTON_RIGHT: 2,
};

const PP_InputEvent_Type = {
  PP_INPUTEVENT_TYPE_UNDEFINED: -1,
  PP_INPUTEVENT_TYPE_MOUSEDOWN: 0,
  PP_INPUTEVENT_TYPE_MOUSEUP: 1,
  PP_INPUTEVENT_TYPE_MOUSEMOVE: 2,
  PP_INPUTEVENT_TYPE_MOUSEENTER: 3,
  PP_INPUTEVENT_TYPE_MOUSELEAVE: 4,
  PP_INPUTEVENT_TYPE_WHEEL: 5,
  PP_INPUTEVENT_TYPE_RAWKEYDOWN: 6,
  PP_INPUTEVENT_TYPE_KEYDOWN: 7,
  PP_INPUTEVENT_TYPE_KEYUP: 8,
  PP_INPUTEVENT_TYPE_CHAR: 9,
  PP_INPUTEVENT_TYPE_CONTEXTMENU: 10,
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_START: 11,
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE: 12,
  PP_INPUTEVENT_TYPE_IME_COMPOSITION_END: 13,
  PP_INPUTEVENT_TYPE_IME_TEXT: 14,
  PP_INPUTEVENT_TYPE_TOUCHSTART: 15,
  PP_INPUTEVENT_TYPE_TOUCHMOVE: 16,
  PP_INPUTEVENT_TYPE_TOUCHEND: 17,
  PP_INPUTEVENT_TYPE_TOUCHCANCEL: 18
};

const PP_NetworkList_State = {
  PP_NETWORKLIST_STATE_DOWN: 0,
  PP_NETWORKLIST_STATE_UP: 1
};

const PP_NetworkList_Type = {
  PP_NETWORKLIST_TYPE_UNKNOWN: 0,
  PP_NETWORKLIST_TYPE_ETHERNET: 1,
  PP_NETWORKLIST_TYPE_WIFI: 2,
  PP_NETWORKLIST_TYPE_CELLULAR: 3
};

const PP_PrintOrientation_Dev = {
  PP_PRINTORIENTATION_NORMAL: 0,
  PP_PRINTORIENTATION_ROTATED_90_CW: 1,
  PP_PRINTORIENTATION_ROTATED_180: 2,
  PP_PRINTORIENTATION_ROTATED_90_CCW: 3
};

const PP_PrintOutputFormat_Dev = {
  PP_PRINTOUTPUTFORMAT_RASTER: 1 << 0,
  PP_PRINTOUTPUTFORMAT_PDF: 1 << 1,
  PP_PRINTOUTPUTFORMAT_POSTSCRIPT: 1 << 2,
  PP_PRINTOUTPUTFORMAT_EMF: 1 << 3
};

const PP_PrintScalingOption_Dev = {
  PP_PRINTSCALINGOPTION_NONE: 0,
  PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA: 1,
  PP_PRINTSCALINGOPTION_SOURCE_SIZE: 2
};

const PP_TextInput_Type_Dev = {
  PP_TEXTINPUT_TYPE_DEV_NONE: 0,
  PP_TEXTINPUT_TYPE_DEV_TEXT: 1,
  PP_TEXTINPUT_TYPE_DEV_PASSWORD: 2,
  PP_TEXTINPUT_TYPE_DEV_SEARCH: 3,
  PP_TEXTINPUT_TYPE_DEV_EMAIL: 4,
  PP_TEXTINPUT_TYPE_DEV_NUMBER: 5,
  PP_TEXTINPUT_TYPE_DEV_TELEPHONE: 6,
  PP_TEXTINPUT_TYPE_DEV_URL: 7
};

const PP_URLRequestProperty = {
  PP_URLREQUESTPROPERTY_URL: 0,
  PP_URLREQUESTPROPERTY_METHOD: 1,
  PP_URLREQUESTPROPERTY_HEADERS: 2,
  PP_URLREQUESTPROPERTY_STREAMTOFILE: 3,
  PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS: 4,
  PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS: 5,
  PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS: 6,
  PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL: 7,
  PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS: 8,
  PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS: 9,
  PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING: 10,
  PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD: 11,
  PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD: 12,
  PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT: 13,
};

const PP_URLResponseProperty = {
  PP_URLRESPONSEPROPERTY_URL: 0,
  PP_URLRESPONSEPROPERTY_REDIRECTURL: 1,
  PP_URLRESPONSEPROPERTY_REDIRECTMETHOD: 2,
  PP_URLRESPONSEPROPERTY_STATUSCODE: 3,
  PP_URLRESPONSEPROPERTY_STATUSLINE: 4,
  PP_URLRESPONSEPROPERTY_HEADERS: 5
};

const PP_VarType = {
  PP_VARTYPE_UNDEFINED: 0,
  PP_VARTYPE_NULL: 1,
  PP_VARTYPE_BOOL: 2,
  PP_VARTYPE_INT32: 3,
  PP_VARTYPE_DOUBLE: 4,
  PP_VARTYPE_STRING: 5,
  PP_VARTYPE_OBJECT: 6,
  PP_VARTYPE_ARRAY: 7,
  PP_VARTYPE_DICTIONARY: 8,
  PP_VARTYPE_ARRAY_BUFFER: 9,
  PP_VARTYPE_RESOURCE: 10,
};

const PP_Flash_Clipboard_Format = {
  PP_FLASH_CLIPBOARD_FORMAT_INVALID: 0,
  PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT: 1,
  PP_FLASH_CLIPBOARD_FORMAT_HTML: 2,
  PP_FLASH_CLIPBOARD_FORMAT_RTF: 3,
};


const PR_RDONLY = 0x01;
const PR_WRONLY = 0x02;
const PR_RDWR = 0x04;
const PR_CREATE_FILE = 0x08;
const PR_APPEND = 0x10;
const PR_TRUNCATE = 0x20;
const PR_SYNC = 0x40;
const PR_EXCL = 0x80;

/* File mode bits */
const PR_IRWXU = 0o700;  /* read, write, execute/search by owner */
const PR_IRUSR = 0o400;  /* read permission, owner */
const PR_IWUSR = 0o200;  /* write permission, owner */
const PR_IXUSR = 0o100;  /* execute/search permission, owner */
const PR_IRWXG = 0o070;  /* read, write, execute/search by group */
const PR_IRGRP = 0o040;  /* read permission, group */
const PR_IWGRP = 0o020;  /* write permission, group */
const PR_IXGRP = 0o010;  /* execute/search permission, group */
const PR_IRWXO = 0o007;  /* read, write, execute/search by others */
const PR_IROTH = 0o004;  /* read permission, others */
const PR_IWOTH = 0o002;  /* write permission, others */
const PR_IXOTH = 0o001;  /* execute/search permission, others */

class InterfaceMemberCall {
  constructor(interfaceName, memberName, args) {
    this.__interface = interfaceName;
    this.__member = memberName;
    Object.assign(this, args);
  }
}

class InterfaceInstanceMemberCall {
  constructor(interfaceName, instance, memberName, args) {
    this.__interface = interfaceName;
    this.__instance = instance;
    this.__member = memberName;
    Object.assign(this, args);
  }
}

class CallbackCall {
  constructor(callbackName, callback, args) {
    this.__callback = callbackName;
    this.__callbackStruct = callback;
    Object.assign(this, args);
  }
}


class OffscreenCanvas {
  constructor(instance, width, height) {
    this._canvas = instance.window.document.createElement("canvas");
    this._canvas.width = width;
    this._canvas.height = height;
  }

  set width(width) {
    this._canvas.width = width;
  }
  set heigth(height) {
    this._canvas.height = height;
  }

  getContext(contextId, contextOptions) {
    return this._canvas.getContext(contextId, contextOptions);
  }
  transferToImageBitmap() {
    let window = this._canvas.ownerGlobal;
    return window.createImageBitmap(this._canvas);
  }
}


class ObjectCache {
  constructor(getkeyForLookup=(v) => v) {
    this.objects = [];
    this.getkeyForLookup = getkeyForLookup;
  }

  add(object) {
    let i = 1;
    while (i < this.objects.length && i in this.objects) {
      ++i;
    }
    this.objects[i] = object;
    return i;
  }
  lookup(i) {
    return this.objects[this.getkeyForLookup(i)];
  }
  destroy(i) {
    delete this.objects[this.getkeyForLookup(i)];
  }
}


class PP_Var {
  constructor(value, instance) {
    if (typeof value == 'undefined' && this.constructor.field == "as_bool") {
      value = PP_Bool.PP_FALSE;
    }

    this.type = this.constructor.type;
    this.padding = 0;
    this.value = {};
    this.value[this.constructor.field] = this.constructor.convertValue(value, instance);
  }

  static get field() {
    return this.fields[this.type];
  }
  static convertValue(value) {
    return value;
  }
  static getAsJSValue(value) {
    return value.value[this.field];
  }
  static normalize({ type, value }) {
    let field = this.fields[type];
    return {
      type,
      padding: 0,
      value: {
        [field]: value[field],
      },
    };
  }
}
// Our JSON parser always needs a field for a union, so we use "as_bool" for
// PP_VARTYPE_UNDEFINED/PP_VARTYPE_NULL.
PP_Var.fields = [
  "as_bool",   // PP_VARTYPE_UNDEFINED
  "as_bool",   // PP_VARTYPE_NULL
  "as_bool",   // PP_VARTYPE_BOOL
  "as_int",    // PP_VARTYPE_INT32
  "as_double", // PP_VARTYPE_DOUBLE
  "as_id",     // PP_VARTYPE_STRING
  "as_id",     // PP_VARTYPE_OBJECT
  "as_id",     // PP_VARTYPE_ARRAY
  "as_id",     // PP_VARTYPE_DICTIONARY
  "as_id",     // PP_VARTYPE_ARRAY_BUFFER
  "as_id",     // PP_VARTYPE_RESOURCE
];
PP_Var.type = PP_VarType.PP_VARTYPE_UNDEFINED;

class PP_Var_Cached extends PP_Var {
  static convertValue(value, instance) {
    return this.cache.add({ value, instance, refcnt: 1 });
  }
  static getAsJSValue(value) {
    return this.cache.lookup(value).value;
  }
  static getAsJSValueWithInstance(value) {
    let r = this.cache.lookup(value);
    return [r.value, r.instance];
  }
  static addRef(value) {
    let cache = this.caches[value.type];
    if (cache) {
      ++cache.lookup(value).refcnt;
    }
  }
  static release(value) {
    let cache = this.caches[value.type];
    if (cache && --cache.lookup(value).refcnt === 0) {
      cache.destroy(value);
    }
  }
  static get cache() {
    return this.caches[this.type];
  }
  static isCached(type) {
    return this.caches[type] !== undefined;
  }
}
PP_Var_Cached.caches = PP_Var.fields.map((f) => f == "as_id" ? new ObjectCache((v) => v.value[f]) : undefined);

class Undefined_PP_Var extends PP_Var {
}
Undefined_PP_Var.type = PP_VarType.PP_VARTYPE_UNDEFINED;

class Null_PP_Var extends PP_Var {
}
Null_PP_Var.type = PP_VarType.PP_VARTYPE_NULL;

class Bool_PP_Var extends PP_Var {
  static convertValue(value) {
    return (typeof value == 'boolean') ? (value ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE) : value;
  }
  static getAsJSValue(value) {
    return PP_Bool[value.value[this.field]] == PP_Bool.PP_TRUE;
  }
}
Bool_PP_Var.type = PP_VarType.PP_VARTYPE_BOOL;

class Int32_PP_Var extends PP_Var {
}
Int32_PP_Var.type = PP_VarType.PP_VARTYPE_INT32;

class Double_PP_Var extends PP_Var {
}
Double_PP_Var.type = PP_VarType.PP_VARTYPE_DOUBLE;

class String_PP_Var extends PP_Var_Cached {
}
String_PP_Var.type = PP_VarType.PP_VARTYPE_STRING;

class Object_PP_Var extends PP_Var_Cached {
}
Object_PP_Var.type = PP_VarType.PP_VARTYPE_OBJECT;

class Array_PP_Var extends PP_Var_Cached {
  constructor(array = [], instance) {
    if (!Array.isArray(array)) {
      throw new Error("Array_PP_Var is not constructed from an array.");
    }
    super(array, instance);
  }
}
Array_PP_Var.type = PP_VarType.PP_VARTYPE_ARRAY;

// Dictionary is a specialized class for the value of Dictionary_PP_Var only.
class Dictionary {
  constructor(object) {
    Object.assign(this, object);
  }
}
class Dictionary_PP_Var extends PP_Var_Cached {
  constructor(dict = new Dictionary(), instance) {
    if (!(dict instanceof Dictionary)) {
      throw new Error("Dictionary_PP_Var is not constructed from a Dictionary.");
    }
    super(dict, instance);
  }
}
Dictionary_PP_Var.type = PP_VarType.PP_VARTYPE_DICTIONARY;

class ArrayBuffer_PP_Var extends PP_Var_Cached {
  constructor(ab, instance) {
    super((ab instanceof ArrayBuffer) ? ab : new ArrayBuffer(ab), instance);
  }
}
ArrayBuffer_PP_Var.type = PP_VarType.PP_VARTYPE_ARRAYBUFFER

class Resource_PP_Var extends PP_Var_Cached {
}
Resource_PP_Var.type = PP_VarType.PP_VARTYPE_RESOURCE;

/* Convert a native JavaScript value to a object-based PP_Var */
PP_Var.fromJSValue = function(v, instance) {
  switch (typeof v) {
    case "undefined":
      return new PP_Var(undefined, instance);
    case "boolean":
      return new Bool_PP_Var(v, instance);
    case "number":
      if (Number.isInteger(v) && v >= (-(1 << 31)) && v <= ((1 << 31) - 1)) {
        return new Int32_PP_Var(v, instance);
      }
      return new Double_PP_Var(v, instance);
    case "string":
      return new String_PP_Var(v, instance);
    case "symbol":
      throw new Error("Don't know how to convert Symbol to PP_Var.");
    case "function":
      /* falls through */
    default:
      if (v === null) {
        return new Null_PP_Var(null, instance);
      } else if (Array.isArray(v)) {
        return v.map((value) => PP_Var.fromJSValue(value, instance));
      } else if (v instanceof Dictionary) {
        let dict = new Dictionary();
        for (let [key, value] of Object.entries(v)) {
          dict[key] = PP_Var.fromJSValue(value, instance);
        }
        return new Dictionary_PP_Var(dict, instance);
      } else if (v instanceof ArrayBuffer) {
        return new ArrayBuffer_PP_Var(v, instance);
      }
      return new Object_PP_Var(v, instance);
  }
};

/* Convert a json-based PP_Var to a native JavaScript value */
PP_Var.toJSValue = function(json, instance) {
  if (!("type" in json) || !("padding" in json) || !("value" in json)) {
    return undefined;
  }
  let type = json.type;
  // Sometimes json.type is a String instead of a Number
  if (typeof type === "string") {
    type = PP_VarType[json.type];
  }
  switch (type) {
    case PP_VarType.PP_VARTYPE_UNDEFINED:
      return undefined;
    case PP_VarType.PP_VARTYPE_NULL:
      return null;
    case PP_VarType.PP_VARTYPE_BOOL:
      return Boolean(Bool_PP_Var.getAsJSValue(json, instance));
    case PP_VarType.PP_VARTYPE_INT32:
      return Number.parseInt(String(Int32_PP_Var.getAsJSValue(json, instance)), 10);
    case PP_VarType.PP_VARTYPE_DOUBLE:
      return Number.parseFloat(String(Double_PP_Var.getAsJSValue(json, instance)));
    case PP_VarType.PP_VARTYPE_STRING:
      return String(String_PP_Var.getAsJSValue(json, instance));
    case PP_VarType.PP_VARTYPE_OBJECT:
      return Object(Object_PP_Var.getAsJSValue(json, instance));
    case PP_VarType.PP_VARTYPE_ARRAY:
      return Array_PP_Var.getAsJSValue(json, instance).map((v) => PP_Var.toJSValue(v, instance));
    case PP_VarType.PP_VARTYPE_DICTIONARY:
      let dict = new Dictionary();
      for (let [key, value] of Object.entries(Dictionary_PP_Var.getAsJSValue(json, instance))) {
        dict[key] = PP_Var.toJSValue(value, instance);
      }
      return dict;
    case PP_VarType.PP_VARTYPE_ARRAY_BUFFER:
      return ArrayBuffer_PP_Var.getAsJSValue(json.value, instance);
    case PP_VarType.PP_VARTYPE_RESOURCE:
      return Resource_PP_Var.getAsJSValue(json.value, instance);
    default:
      throw new Error("Don't know how to convert PP_Var with type(" + type + ") to a proper JavaScript object.");
  }
}

class PP_Resource {
  constructor(instance) {
    // XXX Need to check that this is correct!
    this.refcnt = 1;
    this.id = PP_Resource.cache.add(this);
    this.instance = instance;
  }

  destroy() {
    PP_Resource.cache.destroy(this.id);
  }

  addRef() {
    ++this.refcnt;
  }
  release() {
    if (--this.refcnt === 0) {
      this.destroy();
    }
  }

  toJSON() {
    return this.id;
  }

  static lookup(id) {
    return this.cache.lookup(id);
  }
}
PP_Resource.cache = new ObjectCache();

const INT16_MIN = -Math.pow(2, 15);
const INT16_MAX = Math.pow(2, 15) - 1;
const negDiv = SIMD.Float32x4.splat(-INT16_MIN);
const posDiv = SIMD.Float32x4.splat(INT16_MAX);
const zeroFloat32 = SIMD.Float32x4.splat(0);
const littleEndian = (new Uint8Array(Uint32Array.of(0xdeadbeef).buffer))[0] == 0xef;

class Audio extends PP_Resource {
  constructor(instance, bufferSize, frameCount, callback, data) {
    super(instance);
    let rt = instance.rt;
    let mem = rt.allocateCachedBuffer(bufferSize);
    let samples = new Float32Array(frameCount * 2);
    let left = new Float32Array(samples.buffer, 0, frameCount);
    let right = new Float32Array(samples.buffer, frameCount * 4, frameCount);

    // FIXME Wish we could use MSE, but it doesn't have what we need yet.
    this.context = new instance.window.AudioContext();
    this.callbackNode = this.context.createScriptProcessor(frameCount, 0, 2);
    this.callbackNode.addEventListener("audioprocess", (e) => {
      rt.call(new CallbackCall("PPB_Audio_Callback_1_0", callback, { sample_buffer: mem, buffer_size_in_bytes: bufferSize, user_data: data }),
              true);
      let buffer = new Int16Array(rt.getCachedBuffer(mem));

      // FIXME Ideally we'd convert straight into the outputBuffer's channel
      //       data, but because our AudioContext is from the window we get
      //       CCWs here, and the SIMD code we use can't deal with those.
      //let left = e.outputBuffer.getChannelData(0);
      //let right = e.outputBuffer.getChannelData(1);
      // Sigh, have to convert between Int16 and Float32 (in a range between
      // -1 and 1).
      this.constructor.splitAndConvertInt16toFloat32(buffer, left, right);
      e.outputBuffer.copyToChannel(left, 0, 0);
      e.outputBuffer.copyToChannel(right, 1, 0);
    });
  }

  static splitAndConvertInt16toFloat32(buffer, leftResult, rightResult) {
    // We'll process groups of 4 frames (8 samples, 4 left and 4 right,
    // interleaved as [l1, r1, l2, r2, l3, r3, l4, r4]). We load as Int32x4, to
    // make sure the JIT inlines, so our data really looks like
    // [r1l1, r2l2, r3l3, r4l4] or [l1r1, l2r2, l3r3, l4r4], depending on
    // endianness.
    let firstResult, secondResult;
    if (littleEndian) {
      firstResult = rightResult;
      secondResult = leftResult;
    } else {
      firstResult = leftResult;
      secondResult = rightResult;
    }
    let frameCount = leftResult.length;
    for (let i = 0; i < frameCount; i += 4) {
      let interleaved = SIMD.Int32x4.load(buffer, i * 2);

      let first = SIMD.Int32x4.shiftRightByScalar(interleaved, 16);
      // Convert to float.
      first = SIMD.Float32x4.fromInt32x4(first);
      // Divide positive numbers by INT16_MAX and negative numbers by -INT16_MIN,
      // to have a Float32 in a range between -1 and 1.
      let pos = SIMD.Float32x4.greaterThan(first, zeroFloat32);
      first = SIMD.Float32x4.div(first,
                                 SIMD.Float32x4.select(pos, posDiv, negDiv));

      let second = SIMD.Int32x4.shiftLeftByScalar(interleaved, 16);
      second = SIMD.Int32x4.shiftRightByScalar(second, 16);
      // Convert to float.
      second = SIMD.Float32x4.fromInt32x4(second);
      // Divide positive numbers by INT16_MAX and negative numbers by -INT16_MIN,
      // to have a Float32 in a range between -1 and 1.
      pos = SIMD.Float32x4.greaterThan(second, zeroFloat32);
      second = SIMD.Float32x4.div(second,
                                  SIMD.Float32x4.select(pos, posDiv, negDiv));

      SIMD.Float32x4.store(firstResult, i, first);
      SIMD.Float32x4.store(secondResult, i, second);
    }
  }

  start() {
    this.callbackNode.connect(this.context.destination);
  }
  stop() {
    this.callbackNode.disconnect(this.context.destination);
  }
}
class AudioConfig extends PP_Resource {
  constructor(instance, frameCount) {
    super(instance);
    this.frameCount = frameCount;
    this.bufferSize = frameCount * 2 /* channels */ * 2 /* bytes per frame */;
  }
}
class BrowserFont_Trusted extends PP_Resource {
  constructor(instance, description) {
    super(instance);
    this.description = description;
    this.customFamily = undefined;
    if (PP_VarType[this.description.face.type] == PP_VarType.PP_VARTYPE_STRING) {
        this.customFamily = String_PP_Var.getAsJSValue(description.face);
    }
  }

  get fontRule() {
    if (!("_fontRule" in this)) {
      let fontRule = [];
      if (PP_Bool[this.description.italic] == PP_Bool.PP_TRUE) {
        fontRule.push("italic");
      }
      if (PP_Bool[this.description.small_caps] == PP_Bool.PP_TRUE) {
        fontRule.push("small-caps");
      }
      let weight;
      switch (PP_BrowserFont_Trusted_Weight[this.description.weight]) {
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_100:
          weight = 100;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_200:
          weight = 200;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_300:
          weight = 300;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_400:
          weight = 400;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_500:
          weight = 500;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_600:
          weight = 600;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_700:
          weight = 700;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_800:
          weight = 800;
          break;
        case PP_BrowserFont_Trusted_Weight.PP_BROWSERFONT_TRUSTED_WEIGHT_900:
          weight = 900;
          break;
      }
      fontRule.push(weight);
      fontRule.push(this.description.size + "px");
      let family;
      if (PP_VarType[this.description.face.type] == PP_VarType.PP_VARTYPE_UNDEFINED) {
        switch (PP_BrowserFont_Trusted_Family[this.description.family]) {
          case PP_BrowserFont_Trusted_Family.PP_BROWSERFONT_TRUSTED_FAMILY_DEFAULT:
            throw new Error("Don't know default font.");
          case PP_BrowserFont_Trusted_Family.PP_BROWSERFONT_TRUSTED_FAMILY_SERIF:
            family = "serif";
            break;
          case PP_BrowserFont_Trusted_Family.PP_BROWSERFONT_TRUSTED_FAMILY_SANSSERIF:
            family = "sans-serif";
            break;
          case PP_BrowserFont_Trusted_Family.PP_BROWSERFONT_TRUSTED_FAMILY_MONOSPACE:
            family = "monospace";
            break;
        }
      } else {
        family = this.customFamily;
      }

      // This shouldn't happen, but make sure there is a font family assigned.
      family = (family == "") ? "serif" : family;

      fontRule.push(family);
      if (this.description.letter_spacing > 0) {
        //throw new Error("Need to implement support for letter_spacing.");
      }
      if (this.description.word_spacing > 0) {
        //throw new Error("Need to implement support for word_spacing.");
      }
      this._fontRule = fontRule.join(" ");
    }
    return this._fontRule;
  }
  measureText(text) {
    // FIXME Can we avoid creating a context?
    if (!("_context" in this)) {
      let canvas = this.instance.window.document.createElement("canvas");
      this._context = canvas.getContext("2d");
      this._context.font = this.fontRule;
    }
    let metrics = this._context.measureText(text);
    return Math.round(metrics.width);
  }
}
class Buffer extends PP_Resource {
  constructor(instance, size) {
    super(instance);
    this.size = size;
    this.mappedCount = 0;
  }

  map() {
    if (++this.mappedCount == 1) {
      this.mem = this.instance.rt.allocateCachedBuffer(this.size);
    }
    return this.mem;
  }
  unmap() {
    if (--this.mappedCount == 0) {
      this.instance.rt.freeCachedBuffer(this.mem);
      delete this.mem;
    }
  }
}
class Flash_MessageLoop extends PP_Resource {
  run() {
    this._running = true;
    let thread = Cc["@mozilla.org/thread-manager;1"].getService().currentThread;
    while (this._running) {
      thread.processNextEvent(true);
    }
  }
  quit() {
    this._running = false;
  }
}
class Graphics extends PP_Resource {
  constructor(instance) {
    super(instance);
    this.canvas = instance.window.document.createElement("canvas");
  }
  destroy() {
    this.canvas.remove();
    super.destroy();
  }
  changeSize(width, height) {
    let devicePixelRatio = this.instance.window.devicePixelRatio;
    this.canvas.style.width = (width / devicePixelRatio) + "px";
    this.canvas.style.height = (height / devicePixelRatio) + "px";
    this.canvas.width = width;
    this.canvas.height = height;
  }
}
class Graphics2DPaintOperation {
  constructor(imageData, x, y, dirtyRect=[]) {
    this.imageData = imageData;
    this.imageData.addRef();
    this.x = x;
    this.y = y;
    this.dirtyRect = dirtyRect;
  }

  destroy() {
    this.imageData.release();
  }

  execute(context) {
    context.putImageData(this.imageData.getDOMImageData(), this.x, this.y, ...this.dirtyRect);
  }
}
class Graphics2DScrollOperation {
  constructor(clipRect, amountX, amountY) {
    this.clipRect = clipRect;
    this.amountX = amountX;
    this.amountY = amountY;
  }

  destroy() {
    // Nothing to do here.
  }

  execute(context) {
    let clip = context.getImageData(...this.clipRect);
    context.putImageData(clip, this.clipRect[0] + this.amountX, this.clipRect[1] + this.amountY);
  }
}
class Graphics2D extends Graphics {
  constructor(instance, width, height) {
    super(instance);

    // FIXME We should probably do transferControlToOffscreen instead, once
    //       that's available.
    this.bitmapContext = this.canvas.getContext("bitmaprenderer");
    this.offscreen = new OffscreenCanvas(instance, width, height);
    this.context = this.offscreen.getContext("2d");
    this.changeSize(width, height);
    this.operations = [];
  }

  destroy() {
    this.clearOperations();
    super.destroy();
  }

  changeSize(width, height) {
    this.offscreen.width = width;
    this.offscreen.height = height;
    super.changeSize(width, height);
    this.bitmapContext.width = width;
    this.bitmapContext.height = height;
  }

  addOperation(operation) {
    this.operations.push(operation);
  }

  clearOperations() {
    for (let operation of this.operations) {
      operation.destroy();
    }
    this.operations = [];
  }

  flush(callback) {
    for (let operation of this.operations) {
      operation.execute(this.context);
      operation.destroy();
    }
    this.operations = [];
    //dump(this.canvas.toDataURL());

    this.offscreen.transferToImageBitmap().then((bitmap) => {
      //dump(this.offscreen._canvas.toDataURL());
      this.bitmapContext.transferImageBitmap(bitmap);
      this.instance.rt.call(new CallbackCall("PP_CompletionCallback", callback, { result: PP_OK }));
    }, () => {
      this.instance.rt.call(new CallbackCall("PP_CompletionCallback", callback, { result: PP_ERROR_FAILED }));
    });
    return PP_OK_COMPLETIONPENDING;
  }
}
class Graphics3D extends Graphics {
  constructor(instance, attributes) {
    let width = -1, height = -1;
    let contextAttributes = {};
    for (let [k, v] of attributes.entries()) {
      switch (k) {
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_DEPTH_SIZE:
          contextAttributes.depth = v > 0;
          break;
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_STENCIL_SIZE:
          contextAttributes.stencil = v > 0;
          break;
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_HEIGHT:
          height = v;
          break;
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_WIDTH:
          width = v;
          break;
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_SWAP_BEHAVIOR:
          contextAttributes.preserveDrawingBuffer = v == PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_BUFFER_PRESERVED;
          break;
        case PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_GPU_PREFERENCE:
          contextAttributes.preferLowPowerToHighPerformance = v == PP_Graphics3DAttrib.PP_GRAPHICS3DATTRIB_GPU_PREFERENCE_LOW_POWER;
          break;
        // FIXME Deal with other attributes!
      }
    }
    // FIXME WORKAROUND LINUX WebGL BUG.
    //contextAttributes.antialias = false;
    let window = instance.window;
    if (width < 0) {
      width = window.innerWidth * window.devicePixelRatio;
    }
    if (height < 0) {
      height = window.innerHeight * window.devicePixelRatio;
    }

    super(instance);
    this.context = this.canvas.getContext("webgl", contextAttributes);
    this.changeSize(width, height);
  }

  changeSize(width, height) {
    super.changeSize(width, height);
    this.context.viewport(0, 0, this.context.drawingBufferWidth, this.context.drawingBufferHeight);
  }
  flush(callback) {
    this.context.flush();
    this.instance.window.requestAnimationFrame(() => {
      this.instance.rt.call(new CallbackCall("PP_CompletionCallback", callback, { result: PP_OK }));
    });
    return PP_OK_COMPLETIONPENDING;
  }
  get objects() {
    if (!("_objects" in this)) {
      this._objects = new ObjectCache();
    }
    return this._objects;
  }
  get mappedTextures() {
    if (!("_mappedTextures" in this)) {
      this._mappedTextures = new Map();
    }
    return this._mappedTextures;
  }
}
class ImageData extends PP_Resource {
  constructor(instance, format, size) {
    let cached = instance.cachedImageData;
    if (cached &&
        cached.format == format &&
        cached.size.width == size.width &&
        cached.size.height == size.height) {
      return cached;
    }

    super(instance);
    this.format = format;
    this.size = size;
    this.mapped = null;
  }

  destroy() {
    if (this.mapped) {
      this.instance.rt.freeCachedBuffer(this.mapped);
      this.mappedSize = 0;
      this.mapped = null;
    }
    super.destroy();
  }

  map() {
    if (!this.mapped) {
      this.mappedSize = this.size.height * this.stride;
      this.mapped = this.instance.rt.allocateCachedBuffer(this.mappedSize);
    }
    return this.mapped;
  }

  // You should surround any code drawing to the context with
  // beginDrawing/endDrawing calls.
  get context() {
    if (!("_context" in this)) {
      let canvas = this.instance.window.document.createElement("canvas");
      canvas.width = this.size.width;
      canvas.height = this.size.height;
      this._context = canvas.getContext("2d");
    }
    return this._context;
  }
  beginDrawing() {
    this.map();
    this.context.save();
    let currentData = new Uint8ClampedArray(this.instance.rt.getCachedBuffer(this.mapped));
    this.context.putImageData(new this.instance.window.ImageData(currentData, this.size.width, this.size.height), 0, 0);
    return this.context;
  }
  endDrawing() {
    let data = this.context.getImageData(0, 0, this.size.width, this.size.height).data;
    if (this.mappedSize != data.byteLength) {
      debugger;
      throw new Error("Trying to set too " + (this.mappedSize > data.byteLength ? "little" : "much") + " data, src: " + data.byteLength + " dest: " + this.mappedSize + ".\n");
    }
    this.instance.rt.setBuffer(this.mapped, data);
    this.context.restore();
  }
  get stride() {
    return this.size.width * 4;
  }
  getDOMImageData() {
    let dataArray = new Uint8ClampedArray(this.instance.rt.getCachedBuffer(this.mapped));
    this._colorConvert(dataArray);
    let imagedata = new this.instance.window.ImageData(dataArray, this.size.width, this.size.height);
    return imagedata;
  }
  _colorConvert(dataArray) {
    if (this.format == PP_ImageDataFormat.PP_IMAGEDATAFORMAT_BGRA_PREMUL) {
      let tmp;
      for (let i = 0; i < dataArray.length; i += 4) {
        tmp = dataArray[i];
        dataArray[i] = dataArray[i + 2];
        dataArray[i + 2] = tmp;
      }
    }
  }
}
class InputEvent extends PP_Resource {
  constructor(instance, event) {
    super(instance);
    this.eventType = EventTypes.get(event.type).eventType;
    this.domEvent = event;
    this.timeStamp = event.timeStamp / 1000;
  }
}
class NetworkMonitor extends PP_Resource {
}
class PrintingDev extends PP_Resource {
}
class TCPSocketPrivate extends PP_Resource {
  constructor(instance) {
    super(instance);
  }

  callCallback(callback, result) {
    this.instance.rt.call(new CallbackCall("PP_CompletionCallback", callback, { result }));
    this.impl.onerror = null;
  }
  connect(host, port, callback) {
    this.impl = new TCPSocket(host, port, { binaryType: "arraybuffer" });
    this.impl.onopen = (e) => {
      this.impl.suspend();

      this.callCallback(callback, PP_OK);
      this.impl.onopen = null;
    };
    this.impl.onerror = (e) => {
      this.callCallback(callback, PP_ERROR_FAILED);
      this.impl.onopen = null;
    };
  }
  read(buffer, bytesToRead, callback) {
    this.impl.ondata = (e) => {
      e.target.suspend();
      if (e.data) {
        if (e.data.byteLength > bytesToRead) {
          throw new Error("We need to cache the data.");
        }
        this.instance.rt.setBuffer(buffer, e.data);
        this.callCallback(callback, e.data.byteLength);
      } else {
        this.callCallback(callback, PP_ERROR_FAILED);
      }
      this.impl.ondata = null;
    };
    this.impl.onerror = (e) => {
      this.callCallback(callback, PP_ERROR_FAILED);
      this.impl.ondata = null;
    };
    this.impl.resume();
  }
  write(data) {
    return this.impl.send(Uint8ClampedArray.from(data).buffer);
  }
  close() {
    this.impl.close();
    this.impl = null;
  }
  get localAddress() {
    if (this.impl.readyState != "open") {
      return null;
    }
    //let address = socket.wrappedJSObject._transport.getScriptableSelfAddr();
    return "127.0.0.0";
  }
  get remoteAddress() {
    if (this.impl.readyState != "open") {
      return null;
    }
    //let address = socket.wrappedJSObject._transport.getScriptablePeerAddr();
    return "127.0.0.0";
  }
}
class URLLoader extends PP_Resource {
  constructor(instance) {
    super(instance);
    this.responseReadCallback = null;
    this.responseUnreadChunks = [];
    this.bytes_received = 0;
    this.total_bytes_to_be_received = -1;
  }

  openURL(method, url, callback) {
    // FIXME Should we use this.instance.info.embed.window if the plugin
    //       doesn't call PPB_URLLoaderTrusted::GrantUniversalAccess?
    this.req = new this.instance.window.XMLHttpRequest({ mozSystem: true });

    this.req.responseType = "moz-chunked-arraybuffer";
    this.headersReceivedCallback = callback;
    this.req.onreadystatechange = this.onreadystatechange.bind(this);
    this.req.onprogress = this.onprogress.bind(this);

    url = new this.instance.window.URL(url, this.instance.info.url);
    this.req.open(method, url, true);
  }
  onreadystatechange() {
    switch (this.req.readyState) {
      case this.req.OPENED:
        this.req.send();
        let channel = this.req.channel;
        try {
          if (channel.QueryInterface(Ci.nsIFileChannel)) {
            channel.contentType = "application/x-unknown-content-type";
            channel.loadFlags |= Ci.nsIChannel.LOAD_CALL_CONTENT_SNIFFERS;
          }
        } catch (e) {
        }
        //this.req.overrideMimeType("application/x-unknown-content-type");
        break;
      case this.req.HEADERS_RECEIVED:
        // Send callback.
        this.headersReceivedCallback(0);
        break;
      case this.req.DONE:
        if (this.responseReadCallback) {
          let callback = this.responseReadCallback;
          this.responseReadCallback = null;
          callback();
        }
        break;
    }
  }
  onprogress(e) {
    this.bytes_received = e.loaded;
    this.total_bytes_to_be_received = e.lengthComputable ? e.total : -1;
    let chunk = this.req.response;
    if (chunk.byteLength > 0) {
      this.responseUnreadChunks.push(new Uint8Array(chunk));
      if (this.responseReadCallback) {
        let callback = this.responseReadCallback;
        this.responseReadCallback = null;
        callback();
      }
    }
  }
  get responseInfo() {
    return new URLResponseInfo(this);
  }
  readResponseIntoBuffer(buffer, bytesToRead) {
    let chunks = this.responseUnreadChunks;
    let bytesRead = 0;
    let i;
    for (i = 0; i < chunks.length; ++i) {
      let chunk = chunks[i];
      let readFromChunk = Math.min(chunk.length, bytesToRead - bytesRead);
      this.instance.rt.setBuffer(buffer + bytesRead, chunk, readFromChunk);
      bytesRead += readFromChunk;
      if (bytesRead == bytesToRead) {
        // We've read all we need to read.
        if (readFromChunk < chunk.length) {
          // But we haven't consumed all the data of the last chunk we read
          // from, store a new view into the buffer that only contains what we
          // haven't read.
          chunks[i] = new Uint8Array(chunk.buffer, chunk.byteOffset + readFromChunk);
        } else {
          ++i;
        }
        break;
      }
    }
    if (i > 0) {
      this.responseUnreadChunks = chunks.slice(i);
    }
    return bytesRead;
  }
  readResponse(buffer, bytesToRead, callback) {
    if (this.responseUnreadChunks.length > 0 ||
        this.req.readyState == this.req.DONE) {
      callback(this.readResponseIntoBuffer(buffer, bytesToRead));
      return -1;
    }

    this.responseReadCallback = () => {
      callback(this.readResponseIntoBuffer(buffer, bytesToRead));
    };
    return -1;
  }
}
class URLRequestInfo extends PP_Resource {
  constructor(instance) {
    super(instance);
    this.propertyMap = new Map();
  }

  getProperty(property) {
    return this.propertyMap.get(property);
  }
  setProperty(property, value) {
    this.propertyMap.set(property, value);
  }
}
class URLResponseInfo extends PP_Resource {
  constructor(loader) {
    super(loader.instance);
    this.req = loader.req;
  }

  getProperty(property) {
    switch (property) {
      case PP_URLResponseProperty.PP_URLRESPONSEPROPERTY_URL:
        return new String_PP_Var(this.req.responseURL);
      case PP_URLResponseProperty.PP_URLRESPONSEPROPERTY_STATUSCODE:
        return new Int32_PP_Var(this.req.status);
      case PP_URLResponseProperty.PP_URLRESPONSEPROPERTY_STATUSLINE:
        return new String_PP_Var(this.req.statusText);
      case PP_URLResponseProperty.PP_URLRESPONSEPROPERTY_HEADERS:
        return new String_PP_Var(this.req.getAllResponseHeaders().split("\r\n").join("\n"));
    }
  }
}
class View extends PP_Resource {
}

class IMEInputEvent extends InputEvent {
  constructor (instance, event) {
    super(instance, event);
    let clauseArray = event.ranges;
    if (event.type == "text" && clauseArray) {
      for (let i = 0; i < clauseArray.length - 1; ++i) {
        if (clauseArray[i].isTargetClause) {
          this.targetSegment = i;
          break;
        }
      }
      this.segmentOffset = [];
      this.segmentOffset[0] = 0;
      let data = event.data;
      let encoder = new TextEncoder("utf-8");
      for (let i = 0, len = 0; i < clauseArray.length - 1; ++i) {
        for (let j = clauseArray[i].startOffset; j < clauseArray[i].endOffset; ++j) {
          len += encoder.encode(data[j]).length;
        }
        this.segmentOffset[i + 1] = len;
      }
    }
  }
}
class KeyboardInputEvent extends InputEvent {
}
class MouseInputEvent extends InputEvent {
}
class TouchInputEvent extends InputEvent {
}
class WheelInputEvent extends InputEvent {
}

const EventTypeArray = [
  [PP_InputEvent_Class.PP_INPUTEVENT_CLASS_KEYBOARD,
   KeyboardInputEvent,
   [["keydown", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_KEYDOWN],
    ["keyup", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_KEYUP],
    ["keypress", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_CHAR]]],
  [PP_InputEvent_Class.PP_INPUTEVENT_CLASS_WHEEL,
   WheelInputEvent,
   [["wheel", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_WHEEL]]],
  [PP_InputEvent_Class.PP_INPUTEVENT_CLASS_MOUSE,
   MouseInputEvent,
   [["mousedown", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_MOUSEDOWN],
    ["mouseup", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_MOUSEUP],
    ["mousemove", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_MOUSEMOVE],
    ["mouseenter", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_MOUSEENTER],
    ["mouseleave", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_MOUSELEAVE],
    ["contextmenu", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_CONTEXTMENU]]],
  [PP_InputEvent_Class.PP_INPUTEVENT_CLASS_IME,
   IMEInputEvent,
   [["compositionstart", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_START],
    ["text", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE],
    ["compositionend", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_END],
    //There is no strict equivalent in Gecko for PP_INPUTEVENT_TYPE_IME_TEXT.
    //["text", PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_TEXT]
    ]],
];

// Map from PP_InputEvent_Class to an array of DOM event name strings.
const EventTypesByClass = EventTypeArray.reduce((map, [eventClass, resourceCtor, events]) => {
    map.set(eventClass, events.map(([domEvent, ppapiEvent]) => domEvent));
    return map;
  }, new Map());

// Map from DOM event name to an object with properties resourceCtor (PP_Resource constructor) and eventType (PP_InputEvent_Type value).
const EventTypes = EventTypeArray.reduce((map, [eventClass, resourceCtor, events]) => {
    events.forEach(([domEvent, ppapiEvent]) => {
      map.set(domEvent, { resourceCtor: resourceCtor, eventType: ppapiEvent, eventClass: eventClass });
    });
    return map;
  }, new Map());

// Map from PP_InputEvent_Type to DOM event name.
const EventByTypes = EventTypeArray.reduce((map, [eventClass, resourceCtor, events]) => {
    events.forEach(([domEvent, ppapiEvent]) => {
      map.set(ppapiEvent, domEvent);
    });
    return map;
  }, new Map());

// Special case for PP_INPUTEVENT_TYPE_IME_TEXT, there is no strict equivalent
// in Gecko, use compositionend.
EventByTypes.set(PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_TEXT, "compositionend");

const ModifierMap = [
  [ "Shift", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_SHIFTKEY ],
  [ "Control", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_CONTROLKEY ],
  [ "Alt", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ALTKEY ],
  [ "Meta", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_METAKEY ],
  [ "CapsLock", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_CAPSLOCKKEY ],
  [ "NumLock", PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_NUMLOCKKEY ],
];

// An abstract object to expose and handle all viewport-related properties and
// operations, so we can seperate the UI logic from other parts in runtime.
// Methods below can be overridden separately in UI layer by creating a
// "createCustomViewport" function in "viewer.html" returning an object with the
// same methods. It is useful if you want to customize your layout in UI layer.
class PPAPIViewport {
  constructor(instance) {
    this._window = instance.window;
    this._defaultEventTarget = instance.eventHandler;

    if (typeof this._window.createCustomViewport === "function") {
      let customViewport = this._window.createCustomViewport(
          instance.viewportActionHandler.bind(instance));
      Object.setPrototypeOf(customViewport, this);
      return customViewport;
    }
  }

  // Appends a canvas to the viewport
  addView(canvas) {
    this._window.document.body.appendChild(canvas);
  }

  // Removes all canvases from the viewport
  clearView() {
    Array.from(this._window.document.body.getElementsByTagName("canvas"))
      .forEach(canvas => canvas.remove());
  }

  // Returns the size of the viewport and its position relative to the window.
  getBoundingClientRect() {
    return this._window.document.body.getBoundingClientRect();
  }

  // Returns a Boolean indicating whether an element is considered the viewport.
  is(element) {
    return element == this._window.document.body;
  }

  // Binds the specified listener on the viewport. Note that it will find the
  // right DOM element to bind according to the event type.
  bindUIEvent(type, listener) {
    this._getEventTarget(type).addEventListener(type, listener);
  }

  // Removes the specified listener from the viewport.
  unbindUIEvent(type, listener) {
    this._getEventTarget(type).removeEventListener(type, listener);
  }

  // Sets the mouse cursor style of the viewport
  setCursor(cursor) {
    this._window.document.body.style.cursor = cursor;
  }

  // Returns the position that the viewport has already been scrolled.
  getScrollOffset() {
    return {
      x: this._window.scrollX,
      y: this._window.scrollY
    };
  }

  // PRIVATE: Returns the right DOM element for event binding.
  _getEventTarget(type) {
    switch(type) {
      case 'fullscreenchange':
      case 'MozScrolledAreaChanged':
        return this._window.document;
      case 'resize':
      case 'focus':
      case 'blur':
        return this._window;
      default:
        return this._defaultEventTarget;
    }
  }
}

class PPAPIInstance {
  constructor(id, rt, info, window, eventHandler, containerWindow, mm) {
    this.id = id;
    this.rt = rt;
    this.info = info;
    this.window = window;
    this.eventHandler = eventHandler;
    this.containerWindow = containerWindow;
    this.mm = mm;
    this.eventHandlers = 0;
    this.filteringEventHandlers = 0;
    this.throttled_ = false;
    this.cachedImageData = null;
    this.viewport = new PPAPIViewport(this);
    this.selectedText = "";
    this.ppapiPrintSettings = {};
    this.pageRangeInfo = {};

    this.notifyHashChange(info.url);

    this.mm.addMessageListener("ppapi.js:fullscreenchange", (evt) => {
      this.viewport.notify({
        type: "fullscreenChange",
        fullscreen: evt.data.fullscreen
      });
    });

    this.mm.addMessageListener("ppapipdf.js:hashchange", (evt) => {
      this.notifyHashChange(evt.data.url);
    });

    this.mm.addMessageListener("ppapipdf.js:oncommand", (evt) => {
      this.viewport.notify({
        type: "command",
        name: evt.data.name
      });
    });

    this.mm.addMessageListener("ppapipdf.js:printsettingschanged", (evt) => {
      // Convert trim print settings to ppapi print settings
      let ps = evt.data.trimPrintSettings;
      let point = (ps.paperSizeUnit == Ci.nsIPrintSettings.kPaperSizeInches)
        ? POINT_PER_INCH : POINT_PER_MILLIMETER;
      // Unit of margins are in inches but paper size could be inches or mm
      this.ppapiPrintSettings.printable_area = {
        point: {
          x: ps.unwriteableMarginLeft * POINT_PER_INCH,
          y: ps.unwriteableMarginTop * POINT_PER_INCH
        },
        size: {
          width: ps.paperWidth * point - (ps.unwriteableMarginLeft +
                 ps.unwriteableMarginRight) * POINT_PER_INCH,
          height: ps.paperHeight * point - (ps.unwriteableMarginTop +
                  ps.unwriteableMarginBottom) * POINT_PER_INCH
        }
      };
      this.ppapiPrintSettings.content_area = {
        point: {
          x: ps.marginLeft * POINT_PER_INCH,
          y: ps.marginTop * POINT_PER_INCH
        },
        size: {
          width: ps.paperWidth * point - (ps.marginLeft +
                 ps.marginRight) * POINT_PER_INCH,
          height: ps.paperHeight * point - (ps.marginTop +
                  ps.marginBottom) * POINT_PER_INCH
        }
      };
      this.ppapiPrintSettings.paper_size = {
        width: ps.paperWidth * point,
        height: ps.paperHeight * point
      };
      // XXX The print dialog does not provide a way for user to set resolution
      //     which causes us not able to access the resolution. So here we
      //     workaround by setting resolution to a default value 0 and pass it
      //     to PDFium for getting PDF file. It is fine to pass a unmeaningful
      //     value of resolution when print as PDF.
      this.ppapiPrintSettings.dpi = 0;
      // XXX We only support kPortraitOrientation and kLandscapeOreintation,
      //     otherwise we return directly.
      switch (ps.orientation) {
        case Ci.nsIPrintSettings.kPortraitOrientation:
          this.ppapiPrintSettings.orientation =
            PP_PrintOrientation_Dev.PP_PRINTORIENTATION_NORMAL;
          break;
        case Ci.nsIPrintSettings.kLandscapeOrientation:
          this.ppapiPrintSettings.orientation =
            PP_PrintOrientation_Dev.PP_PRINTORIENTATION_ROTATED_90_CW;
          break;
        default:
          return;
      }
      this.ppapiPrintSettings.print_scaling_option = ps.shrinkToFit ?
        PP_PrintScalingOption_Dev.PP_PRINTSCALINGOPTION_FIT_TO_PRINTABLE_AREA :
        PP_PrintScalingOption_Dev.PP_PRINTSCALINGOPTION_NONE;
      this.ppapiPrintSettings.grayscale = !ps.printInColor;
      this.ppapiPrintSettings.format =
        PP_PrintOutputFormat_Dev.PP_PRINTOUTPUTFORMAT_PDF;

      // Store page range information
      this.pageRangeInfo.printRange = ps.printRange;
      this.pageRangeInfo.startPageRange = ps.startPageRange;
      this.pageRangeInfo.endPageRange = ps.endPageRange;

      let message = {type: 'print'};
      this.viewportActionHandler(message);
    });
  }

  notifyHashChange(url) {
    let location = new URL(url);
    if (location.hash) {
      this.viewport.notify({
        type: "hashChange",
        // substring(1) for getting rid of the first '#' character
        hash: location.hash.substring(1)
      });
    }
  }

  bindGraphics(graphicsDevice) {
    if (graphicsDevice) {
      let canvas = graphicsDevice.canvas;

      // FIXME This size should be adjusted according to devicePixelRatio.
      canvas.style.width = canvas.width;
      canvas.style.height = canvas.height;

      // Attach the canvas of this Graphics object to DOM for displaying.
      this.viewport.addView(canvas);
    } else {
      // Unbind all graphics objects, which means remove all canvas elements from DOM.
      this.viewport.clearView();
    }
  }
  handleEvent(event) {
    dump(`EVENT ${event.type}\n`);
    if ((event.type == "keydown" || event.type == "keyup") &&
        event.keyCode == 224) {
      return;
    }
    if (event.type == "keypress" && event.charCode === 0) {
      return;
    }

    // To avoid cursor misalignment, we regenerate the mouse event which
    // position is based on coordinate (0, 0) of viewport
    let eventType = EventTypes.get(event.type);
    let resource;
    if (event instanceof this.window.MouseEvent) {
      let rect = this.boundingRect;
      let mouseEventInit = {
        altkey: event.altkey,
        button: event.button,
        buttons: event.buttons,
        clientX: event.clientX - rect.left,
        clientY: event.clientY - rect.top,
        ctrlKey: event.ctrlKey,
        detail: event.detail,
        metaKey: event.metaKey,
        movementX: event.movementX - rect.left,
        movementY: event.movementY - rect.top,
        shiftKey: event.shiftKey,
      };
      let offset_evt = new this.window.MouseEvent(event.type, mouseEventInit);
      resource = new eventType.resourceCtor(this, offset_evt);
      resource.timeStamp = event.timeStamp;
    } else {
      resource = new eventType.resourceCtor(this, event);
    }

    // We should only use sync call for filtering events
    if (this.filteringEventHandlers & eventType.eventClass) {
      let handled = this.rt.call(new InterfaceMemberCall("PPP_InputEvent;0.1", "HandleInputEvent", { instance: this, input_event: resource }), true);
      if (handled) {
        event.stopPropagation();
      } else {
        // FIXME Retarget at frameElement!
      }
    } else {
      this.rt.call(new InterfaceMemberCall("PPP_InputEvent;0.1", "HandleInputEvent", { instance: this, input_event: resource }));
    }

    if (event.type == "compositionend") {
      resource = new eventType.resourceCtor(this, event);
      resource.eventType = PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_TEXT;
      if (this.filteringEventHandlers & eventType.eventClass) {
        throw new Error("Add code for filtering for PP_INPUTEVENT_TYPE_IME_TEXT.");
      }
      this.rt.call(new InterfaceMemberCall("PPP_InputEvent;0.1", "HandleInputEvent", { instance: this, input_event: resource }));
    }
  }
  registerEventHandler(eventClasses, filtering) {
    let handler = this.handleEvent.bind(this);
    let target = this.eventHandler;
    let registeredHandlers = this.eventHandlers | this.filteringEventHandlers;
    EventTypesByClass.forEach((domEvents, eventClass) => {
      if ((eventClasses & eventClass) && !(registeredHandlers & eventClass)) {
        domEvents.forEach((domEvent) => {
          if (domEvent == "text") {
            Cc["@mozilla.org/eventlistenerservice;1"].getService(Ci.nsIEventListenerService).
            addSystemEventListener(target, "text", handler, true);
          } else {
            this.viewport.bindUIEvent(domEvent, handler);
          }
        });
      }
    });

    if (filtering) {
      this.eventHandlers &= ~eventClasses;
      this.filteringEventHandlers |= eventClasses;
    } else {
      this.eventHandlers |= eventClasses;
      this.filteringEventHandlers &= ~eventClasses;
    }
    return PP_OK;
  }
  get throttled() {
    return this.throttled_;
  }
  set throttled(on) {
    if (this.throttled_ != on) {
      this.throttled_ = on;
      let view = new View(this);
      let call = new InterfaceMemberCall("PPP_Instance;1.1", "DidChangeView", { instance: this, view });
      this.rt.call(call, true);
    }
  }
  get boundingRect() {
    if (!this.boundingRect_) {
      this.boundingRect_ = this.viewport.getBoundingClientRect();
    }
    return this.boundingRect_;
  }

  toJSON() {
    return this.id;
  }

  viewportActionHandler(message) {
    switch(message.type) {
      case 'setFullscreen':
        this.mm.sendAsyncMessage("ppapi.js:setFullscreen", message.fullscreen);
        break;
      case 'save':
        this.mm.sendAsyncMessage("ppapipdf.js:save", {
          url: this.info.url });
        break;
      case 'setHash':
        this.mm.sendAsyncMessage("ppapipdf.js:setHash", message.hash);
        break;
      case 'startPrint':
        // We need permission for showing print dialog to get print settings
        this.mm.sendAsyncMessage("ppapipdf.js:getPrintSettings", {
          url: this.info.url });
        break;
      case 'openLink':
        this.mm.sendAsyncMessage("ppapipdf.js:openLink", {
          url: message.url,
          disposition: message.disposition
        });
        break;
      case 'viewport':
      case 'rotateClockwise':
      case 'rotateCounterclockwise':
      case 'selectAll':
      case 'getSelectedText':
      case 'getNamedDestination':
      case 'getPasswordComplete':
      case 'print':
        let data = PP_Var.fromJSValue(new Dictionary(message), this);
        this.rt.call(new InterfaceMemberCall("PPP_Messaging;1.0", "HandleMessage",
          { instance: this, var: data }));
        break;
      default:
        throw new Error(`Invalid message type "${message.type}".`);
    }
  }
  selectFindResult(isForward) {
    let forward = Bool_PP_Var.convertValue(isForward);
    this.rt.call(new InterfaceMemberCall("PPP_Find_Private;0.3", "SelectFindResult",
      { instance: this, forward }));
  }
  startFind(term, isCaseSensitive) {
    let case_sensitive = Bool_PP_Var.convertValue(isCaseSensitive);
    this.rt.call(new InterfaceMemberCall("PPP_Find_Private;0.3", "StartFind",
      { instance: this, text: term, case_sensitive }));
  }
  stopFind() {
    this.rt.call(new InterfaceMemberCall("PPP_Find_Private;0.3", "StopFind",
      { instance: this }));
  }
  didChangeFocus() {
    if (this.focusChangeTimeout) {
      this.window.clearTimeout(this.focusChangeTimeout);
    }
    // we use setTimeout to hold a small amount of time to
    // make sure the focus state is stable.
    this.focusChangeTimeout = this.window.setTimeout(() => {
      let focusState = PP_Bool.PP_FALSE;
      if (this.window.document.hasFocus() &&
          (this.viewport.is(this.window.document.activeElement) ||
          this.window.document.activeElement == this.window.document.getElementById('IMEInput'))) {
        focusState = PP_Bool.PP_TRUE;
      }
      if (this.focusState != focusState) {
        this.focusState = focusState;
        let call = new InterfaceMemberCall("PPP_Instance;1.1", "DidChangeFocus", { instance: this, has_focus: focusState });
        this.rt.call(call, true);
      }
    });
  }
  copyString() {
    // Prevent user from calling copy without selecting any text and rewrite
    // an empty string into clipboard.
    if (!this.selectedText) {
      return;
    }

    let clipboardHelper =
      Components.classes["@mozilla.org/widget/clipboardhelper;1"].
      getService(Components.interfaces.nsIClipboardHelper);
    clipboardHelper.copyString(this.selectedText);
  }
}

function PPAPIRuntime(process) {
  this.instances = [];
  this.process = process;
  this.urlParser = Cc["@mozilla.org/network/url-parser;1?auth=maybe"]
                   .getService(Ci.nsIURLParser);
}

PPAPIRuntime.prototype = {
  get callback() {
    return this.handler.bind(this);
  },
  handler: function(json) {
    let obj;
    try {
      obj = JSON.parse(json);
    } catch (e) {
      dump(e.message + "\n");
      let result = e.message.match(/line (\d+) column (\d+)/);
      if (result) {
        dump("  " + json + "\n");
        dump("  " + new Array(parseInt(result[2], 10)).join("-") + "^\n");
      }
    }
    if (!obj || !obj.__interface || !obj.__version || !obj.__method) {
      dump("Invalid JSON RPC call: " + json + "\n");
      return null;
    }
    let fn = obj.__interface + "_" + obj.__method;
    let f = this.table[fn];
    if (!f) {
      dump(
`Not implemented: ${json}
    /**
     *
     */
    ${obj.__interface}_${obj.__method}: function(json) {
    },

`);
      return null;
    }
    let result = f.call(this, obj);
    if (typeof result == 'undefined') {
      return null;
    }
    return JSON.stringify([result]);
  },

  toPP_Var: function(v, instance) {
    return PP_Var.fromJSValue(v, instance);
  },

  parseURL: function(url) {
    let schemePos = {}, schemeLen = {};
    let authorityPos = {}, authorityLen = {};
    let pathPos = {}, pathLen = {};
    this.urlParser.parseURL(url, url.length, schemePos, schemeLen, authorityPos,
                            authorityLen, pathPos, pathLen);
    let usernamePos = {}, usernameLen = {};
    let passwordPos = {}, passwordLen = {};
    let hostnamePos = {}, hostnameLen = {};
    let port = {};
    this.urlParser.parseAuthority(url.substr(authorityPos.value, authorityLen.value),
                                  authorityLen.value, usernamePos, usernameLen,
                                  passwordPos, passwordLen, hostnamePos, hostnameLen,
                                  port);
    let portPos = port < 0 ? 0 : hostnamePos.value + hostnameLen.value + 1;
    let portLen = port < 0 ? -1 : pathPos.value - portPos;
    let filepathPos = {}, filepathLen = {};
    let queryPos = {}, queryLen = {};
    let refPos = {}, refLen = {};
    this.urlParser.parsePath(url.substr(pathPos.value, pathLen.value),
                             pathLen.value, filepathPos, filepathLen, queryPos,
                             queryLen, refPos, refLen);
    return {
      scheme: { begin: Math.min(0, schemePos.value), len: schemeLen.value },
      username: { begin: Math.min(0, usernamePos.value), len: usernameLen.value },
      password: { begin: Math.min(0, passwordPos.value), len: passwordLen.value },
      host: { begin: Math.min(0, hostnamePos.value), len: hostnameLen.value },
      port: { begin: portPos, len: portLen },
      path: { begin: Math.min(0, filepathPos.value), len: filepathLen.value },
      query: { begin: Math.min(0, queryPos.value), len: queryLen.value },
      ref: { begin: Math.min(0, refPos.value), len: refLen.value },
    };
  },

  allocateCachedBuffer: function(size) {
    return this.process.allocateCachedBuffer(size);
  },
  getCachedBuffer: function(ptr) {
    return this.process.getCachedBuffer(ptr);
  },
  freeCachedBuffer: function(ptr) {
    this.process.freeCachedBuffer(ptr);
  },
  setBuffer: function(dest, source, size=source.byteLength) {
    return this.process.setBuffer(source, size, dest);
  },
  copyBuffer: function(ptr, size) {
    return this.process.copyFromBuffer(ptr, size);
  },

  get moduleLocalFiles() {
    if (!("_moduleLocalFiles" in this)) {
      this._moduleLocalFiles = Cc['@mozilla.org/file/local;1'].createInstance(Ci.nsIFile);
      this._moduleLocalFiles.initWithPath(Services.cpmm.sendRpcMessage("ppapiflash.js:getModuleLocalFilesPath"));
    }
    return this._moduleLocalFiles;
  },

  createInstance: function(instanceInfo, window, eventHandler, containerWindow, mm) {
    let i = 0;
    for (; i < this.instances.length; ++i) {
      if (!(i in this.instances)) {
        break;
      }
    }

    let instance = this.instances[i] = new PPAPIInstance(i, this, instanceInfo, window, eventHandler, containerWindow, mm);

    let didChangeView = this.didChangeView.bind(this, instance);
    instance.viewport.bindUIEvent("MozScrolledAreaChanged", didChangeView);
    instance.viewport.bindUIEvent("fullscreenchange", didChangeView);
    instance.viewport.bindUIEvent("resize", didChangeView);
    instance.viewport.bindUIEvent("blur", instance.didChangeFocus.bind(instance));
    instance.viewport.bindUIEvent("focus", instance.didChangeFocus.bind(instance));

    let argn = instanceInfo.arguments.keys;
    let argv = instanceInfo.arguments.values;

    this.call(new InterfaceMemberCall("PPP_Instance;1.1", "DidCreate", { instance: i, argc: argn.length, argn: argn, argv: argv }), true);
    if (instanceInfo.setupJSInstanceObject) {
      let jsObj = this.call(new InterfaceMemberCall("PPP_Instance_Private;0.1", "GetInstanceObject", { instance: i }), true);
      if (PP_VarType[jsObj.type] == PP_VarType.PP_VARTYPE_OBJECT) {
        instance.mm.sendRpcMessage("ppapiflash.js:setInstancePrototype", undefined, { proto: Object_PP_Var.getAsJSValue(jsObj) });
      }
    }

    if (instanceInfo.isFullFrame) {
      let loader = new URLLoader(instance);
      loader.openURL("GET", instanceInfo.url,
                     (result) => { this.call(new InterfaceMemberCall("PPP_Instance;1.1", "HandleDocumentLoad", { instance, url_loader: loader })); });
    }
    this.didChangeView(instance);
    return i;
  },
  destroyInstances: function() {
    for (let i of this.instances) {
      this.destroyInstance(i, false);
    }
    this.instances.length = 0;
  },
  destroyInstance: function(instance, truncateInstancesArray=true) {
    this.call(new InterfaceMemberCall("PPP_Instance;1.1", "DidDestroy", { instance: instance }), true);
    delete this.instances[instance];
    if (truncateInstancesArray) {
      let i = this.instances.length;
      while (--i >= 0) {
        if (i in this.instances) {
          break;
        }
      }
      this.instances.length = i + 1;
    }
  },
  hasInstances: function() {
    return this.instances.some(() => true);
  },
  didChangeView: function(instance) {
    // In case of |getBoundingClientRect()| will reflush the layout and cause
    // bad performance, we cache the bounding rectangle for frequently
    // regenerating the offset mouse event.
    instance.boundingRect_ = instance.viewport.getBoundingClientRect();

    let view = new View(instance);
    let call = new InterfaceMemberCall("PPP_Instance;1.1", "DidChangeView", { instance, view });
    this.call(call, true);
  },

  call: function(call, sync=false) {
    if (sync) {
dump(`callFromJSON: > ${JSON.stringify(call)}\n`);
      let result = this.process.sendMessage(JSON.stringify(call));
dump(`callFromJSON: < ${JSON.stringify(call)}\n`);
      return result ? JSON.parse(result) : result;
    }

    let thread = Services.tm.currentThread;
    thread.dispatch(() => {
dump(`callFromJSON (async): > ${JSON.stringify(call)}\n`);
      let result = this.process.sendMessage(JSON.stringify(call));
dump(`callFromJSON: < ${JSON.stringify(call)}\n`);
    }, Ci.nsIThread.DISPATCH_NORMAL);
  },

  table: {
    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_Resource config,
     *     [in] PPB_Audio_Callback audio_callback,
     *     [inout] mem_t user_data);
     *    */
    PPB_Audio_Create: function(json) {
      let instance = this.instances[json.instance];
      let config = PP_Resource.lookup(json.config);
      return new Audio(instance, config.bufferSize, config.frameCount,
                       json.audio_callback, json.user_data);
    },

    /**
     * PP_Bool StartPlayback(
     *     [in] PP_Resource audio);
     */
    PPB_Audio_StartPlayback: function(json) {
      let audio = PP_Resource.lookup(json.audio);
      audio.start();
      return PP_Bool.PP_TRUE;
    },

    /**
     * PP_Bool StopPlayback(
     *     [in] PP_Resource audio);
     */
    PPB_Audio_StopPlayback: function(json) {
      let audio = PP_Resource.lookup(json.audio);
      audio.stop();
      return PP_Bool.PP_TRUE;
    },

    /**
     * PP_Resource CreateStereo16Bit(
     *     [in] PP_Instance instance,
     *     [in] PP_AudioSampleRate sample_rate,
     *     [in] uint32_t sample_frame_count);
     */
    PPB_AudioConfig_CreateStereo16Bit: function(json) {
      return new AudioConfig(this.instances[json.instance],
                             json.sample_frame_count);
    },

    /**
     * uint32_t RecommendSampleFrameCount(
     *     [in] PP_Instance instance,
     *     [in] PP_AudioSampleRate sample_rate,
     *     [in] uint32_t requested_sample_frame_count);
     */
    PPB_AudioConfig_RecommendSampleFrameCount: function(json) {
      let rate = Math.pow(2, Math.ceil(Math.log2(json.sample_rate)));
      return Math.max(256, Math.min(rate, 16384));
    },

    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance);
     */
    PPB_AudioInput_Dev_Create: function(json) {
      return 0;
    },

    /**
     * int32_t EnumerateDevices(
     *     [in] PP_Resource audio_input,
     *     [in] PP_ArrayOutput output,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_AudioInput_Dev_EnumerateDevices: function(json) {
      return PP_ERROR_BADRESOURCE;
    },


    /**
     * PP_Var GetFontFamilies(
     *     [in] PP_Instance instance);
     */
    PPB_BrowserFont_Trusted_GetFontFamilies: function(json) {
      let instance = this.instances[json.instance];
      let enumerator = Cc["@mozilla.org/gfx/fontenumerator;1"].createInstance(Ci.nsIFontEnumerator);
      return new String_PP_Var(enumerator.EnumerateAllFonts({}).join('\0'), instance);
    },

    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_BrowserFont_Trusted_Description description);
     */
    PPB_BrowserFont_Trusted_Create: function(json) {
      return new BrowserFont_Trusted(this.instances[json.instance],
                                     json.description);
    },

    /**
     * PP_Bool Describe(
     *     [in] PP_Resource font,
     *     [out] PP_BrowserFont_Trusted_Description description,
     *     [out] PP_BrowserFont_Trusted_Metrics metrics);
     */
    PPB_BrowserFont_Trusted_Describe: function(json) {
      let font = PP_Resource.lookup(json.font);
      let description = Object.assign({}, font.description);
      if (font.customFamily) {
        description.face = new String_PP_Var(font.customFamily);
      } else {
        description.face = new Undefined_PP_Var();
      }
      description.family = PP_BrowserFont_Trusted_Family[description.family];
      description.weight = PP_BrowserFont_Trusted_Weight[description.weight];
      description.italic = PP_Bool[description.italic];
      description.small_caps = PP_Bool[description.small_caps];
      return [PP_Bool.PP_TRUE, { description: description, metrics: { height: 12, ascent: 12, descent: 12, line_spacing: 12, x_height: 12 } }];
    },

    /**
     * PP_Bool DrawTextAt(
     *     [in] PP_Resource font,
     *     [in] PP_Resource image_data,
     *     [in] PP_BrowserFont_Trusted_TextRun text,
     *     [in] PP_Point position,
     *     [in] uint32_t color,
     *     [in] PP_Rect clip,
     *     [in] PP_Bool image_data_is_opaque);
     */
    PPB_BrowserFont_Trusted_DrawTextAt: function(json) {
      let font = PP_Resource.lookup(json.font);
      let imageData = PP_Resource.lookup(json.image_data);
      let textType = PP_VarType[json.text.text.type];
      let text;
      if (textType == PP_VarType.PP_VARTYPE_UNDEFINED ||
          textType == PP_VarType.PP_VARTYPE_NULL) {
        text = "";
      } else {
        text = String_PP_Var.getAsJSValue(json.text.text);
      }
      let context = imageData.beginDrawing();
      context.font = font.fontRule;
      let r = ((json.color & (0xff << 0)) >>> 0);
      let g = ((json.color & (0xff << 8)) >>> 8);
      let b = ((json.color & (0xff << 16)) >>> 16);
      let a = ((json.color & (0xff << 24)) >>> 24) / 255;
      // It seems the color matches its ImageData format.
      context.fillStyle = `rgba(${r}, ${g}, ${b}, ${a})`;
      let { x, y } = json.position;
      let { point: { x: clipX, y: clipY }, size: { width: clipW, height: clipH } } = json.clip;
      context.rect(clipX, clipY, clipW, clipH);
      context.clip();
      context.fillText(text, x, y);
      //dump(context.canvas.toDataURL() + "\n");
      imageData.endDrawing();
      return PP_Bool.PP_TRUE;
    },

    /**
     * int32_t MeasureText(
     *     [in] PP_Resource font,
     *     [in] PP_TextRun_Dev text);
     */
    PPB_BrowserFont_Trusted_MeasureText: function(json) {
      let font = PP_Resource.lookup(json.font);
      let textType = PP_VarType[json.text.text.type];
      let text;
      if (textType == PP_VarType.PP_VARTYPE_UNDEFINED ||
          textType == PP_VarType.PP_VARTYPE_NULL) {
        text = "";
      } else {
        text = String_PP_Var.getAsJSValue(json.text.text);
      }
      return font.measureText(text);
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] uint32_t size_in_bytes);
     */
    PPB_Buffer_Dev_Create: function(json) {
      return new Buffer(this.instances[json.instance],
                        json.size_in_bytes);
    },

    /**
     * PP_Bool Describe(
     *     [in] PP_Resource resource,
     *     [out] uint32_t size_in_bytes);
     */
    PPB_Buffer_Dev_Describe: function(json) {
      let buffer = PP_Resource.lookup(json.resource);
      return [PP_Bool.PP_TRUE, { size_in_bytes: buffer.size }];
    },

    /**
     * mem_t Map(
     *     [in] PP_Resource resource);
     */
    PPB_Buffer_Dev_Map: function(json) {
      let buffer = PP_Resource.lookup(json.resource);
      return buffer.map();
    },

    /**
     * void Unmap(
     *     [in] PP_Resource resource);
     */
    PPB_Buffer_Dev_Unmap: function(json) {
      let buffer = PP_Resource.lookup(json.resource);
      buffer.unmap();
    },


    /**
     * PP_Var GetDefaultCharSet([in] PP_Instance instance);
     */
    PPB_CharSet_Dev_GetDefaultCharSet: function(json) {
      return new String_PP_Var("utf-8");
    },

    /**
     * str_t UTF16ToCharSet([in] PP_Instance instance,
     *                      [in, size_as=utf16_len] uint16_t[] utf16,
     *                      [in] uint32_t utf16_len,
     *                      [in] str_t output_char_set,
     *                      [in] PP_CharSet_ConversionError on_error,
     *                      [out] uint32_t output_length);
     */
    PPB_CharSet_Dev_UTF16ToCharSet: function(json) {
      let instance = this.instances[json.instance];
      let utf16 = Uint16Array.from(json.utf16);
      let decoded = new TextDecoder("utf-16").decode(utf16);
      let converted = new TextEncoder(json.output_char_set).encode(decoded);
      return [Array.from(new Uint8Array(converted)), { output_length: converted.buffer.byteLength }];
    },

    /**
     * uint16_ptr_t CharSetToUTF16([in] PP_Instance instance,
     *                             [in] str_t input,
     *                             [in] uint32_t input_len,
     *                             [in] str_t input_char_set,
     *                             [in] PP_CharSet_ConversionError on_error,
     *                             [out] uint32_t output_utf16_length);
     */
    PPB_CharSet_Dev_CharSetToUTF16: function(json) {
      let instance = this.instances[json.instance];
      let input = Uint8ClampedArray.from(json.input);
      let decoded = new TextDecoder(json.input_char_set).decode(input);
      let converted = new TextEncoder("utf-16").encode(decoded);
      return [Array.from(new Uint16Array(converted.buffer)), { output_utf16_length: converted.byteLength / 2 }];
    },


    /**
     * void Log(
     *     [in] PP_Instance instance,
     *     [in] PP_LogLevel level,
     *     [in] PP_Var value);
     */
    PPB_Console_Log: function(json) {
      let instance = this.instances[json.instance];
      dump("FROM FLASH: " + String_PP_Var.getAsJSValue(json.value) + "\n");
      instance.mm.sendRpcMessage("ppapiflash.js:log", String_PP_Var.getAsJSValue(json.value));
    },


    /**
     * void AddRefResource([in] PP_Resource resource);
     */
    PPB_Core_AddRefResource: function(json) {
      PP_Resource.lookup(json.resource).addRef();
    },

    /**
     * void ReleaseResource([in] PP_Resource resource);
     */
    PPB_Core_ReleaseResource: function(json) {
      PP_Resource.lookup(json.resource).release();
    },

    /**
     * PP_Time GetTime();
     */
    PPB_Core_GetTime: function(json) {
      return Date.now() / 1000;
    },

    /**
     * void CallOnMainThread(
     *     [in] int32_t delay_in_milliseconds,
     *     [in] PP_CompletionCallback callback,
     *     [in] int32_t result);
     */
    PPB_Core_CallOnMainThread: function(json) {
      let callback = new CallbackCall("PP_CompletionCallback", json.callback, { result: json.result });
      if (json.delay_in_milliseconds > 0) {
        // It'd be better if we had the instance so we could use setTimeout on
        // the window.
        let timer = Cc["@mozilla.org/timer;1"].createInstance(Ci.nsITimer);
        timer.initWithCallback(() => { this.call(callback); },
                               json.delay_in_milliseconds,
                               Ci.nsITimer.TYPE_ONE_SHOT);
      } else {
        this.call(callback);
      }
    },

    /**
     * PP_Bool IsMainThread();
    PPB_Core_IsMainThread: function(json, mainThread) {
      return mainThread ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },
     */

    /**
     * void GetRandomBytes([out] str_t buffer, [in] uint32_t num_bytes);
     */
    PPB_Crypto_Dev_GetRandomBytes: function(json) {
      let bytes = Services.cpmm.sendRpcMessage("ppapi.js:generateRandomBytes", json.num_bytes);
      return [{ buffer: bytes }];
    },

    /**
     * PP_Bool SetCursor([in] PP_Instance instance,
     *                   [in] PP_CursorType_Dev type,
     *                   [in] PP_Resource custom_image,
     *                   [in] PP_Point hot_spot);
     */
    PPB_CursorControl_Dev_SetCursor: function(json) {
      let instance = this.instances[json.instance];
      let cursor;
      switch (PP_CursorType_Dev[json.type]) {
        case PP_CursorType_Dev.PP_CURSORTYPE_CUSTOM:
          throw new Error("Custom cursors not implemented.");
        case PP_CursorType_Dev.PP_CURSORTYPE_POINTER:
          cursor = "default";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_CROSS:
          cursor = "crosshair";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_HAND:
          cursor = "pointer";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_IBEAM:
          cursor = "text";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_WAIT:
          cursor = "wait";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_HELP:
          cursor = "help";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_EASTRESIZE:
          cursor = "e-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHRESIZE:
          cursor = "n-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHEASTRESIZE:
          cursor = "ne-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHWESTRESIZE:
          cursor = "nw-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHRESIZE:
          cursor = "s-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHEASTRESIZE:
          cursor = "se-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHWESTRESIZE:
          cursor = "sw-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_WESTRESIZE:
          cursor = "w-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHSOUTHRESIZE:
          cursor = "ns-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_EASTWESTRESIZE:
          cursor = "ew-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHEASTSOUTHWESTRESIZE:
          cursor = "nesw-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHWESTSOUTHEASTRESIZE:
          cursor = "nwse-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_COLUMNRESIZE:
          cursor = "col-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_ROWRESIZE:
          cursor = "row-resize";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_MIDDLEPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_EASTPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHEASTPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_NORTHWESTPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHEASTPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_SOUTHWESTPANNING:
        case PP_CursorType_Dev.PP_CURSORTYPE_WESTPANNING:
          throw new Error("Panning cursors not implemented.");
        case PP_CursorType_Dev.PP_CURSORTYPE_MOVE:
          cursor = "move";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_VERTICALTEXT:
          cursor = "vertical-text";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_CELL:
          cursor = "cell";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_CONTEXTMENU:
          cursor = "context-menu";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_ALIAS:
          cursor = "alias";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_PROGRESS:
          cursor = "progress";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NODROP:
          cursor = "no-drop";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_COPY:
          cursor = "copy";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NONE:
          cursor = "none";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_NOTALLOWED:
          cursor = "not-allowed";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_ZOOMIN:
          cursor = "zoom-in";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_ZOOMOUT:
          cursor = "zoom-out";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_GRAB:
          cursor = "grab";
          break;
        case PP_CursorType_Dev.PP_CURSORTYPE_GRABBING:
          cursor = "grabbing";
          break;
      }
      instance.viewport.setCursor(cursor);
      return PP_Bool.PP_TRUE;
    },


    /**
     * void NumberOfFindResultsChanged(
     *     [in] PP_Instance instance,
     *     [in] int32_t total,
     *     [in] PP_Bool final_result);
     */
    PPB_Find_Private_NumberOfFindResultsChanged: function(json) {
      let instance = this.instances[json.instance];
      instance.viewport.notify({
        type: "numberOfFindResultsChanged",
        total: json.total,
        finalResult: (PP_Bool[json.final_result] == PP_Bool.PP_TRUE),
      });
    },

    /**
     * void SelectedFindResultChanged(
     *     [in] PP_Instance instance,
     *     [in] int32_t index);
     */
    PPB_Find_Private_SelectedFindResultChanged: function(json) {
      let instance = this.instances[json.instance];
      instance.viewport.notify({
        type: "selectedFindResultChanged",
        index: json.index,
      });
    },

    /**
     * void SetTickmarks(
     *     [in] PP_Instance instance,
     *     [in, size_as=count] PP_Rect[] tickmarks,
     *     [in] uint32_t count);
     */
    PPB_Find_Private_SetTickmarks: function(json) {
      let instance = this.instances[json.instance];
      instance.viewport.notify({
        type: "setTickmarks",
        tickmarks: json.tickmarks,
        count: json.count,
      });
    },


    /**
     * PP_Bool IsFormatAvailable(
     *     [in] PP_Instance instance_id,
     *     [in] PP_Flash_Clipboard_Type clipboard_type,
     *     [in] uint32_t format);
     */
    PPB_Flash_Clipboard_IsFormatAvailable: function(json) {
      //FIXME We only support standard clipboard w/ plaintext format
      if (PP_Flash_Clipboard_Format[json.clipboard_type] ==
          PP_Flash_Clipboard_Format.PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
        if (json.format ==
            PP_Flash_Clipboard_Format.PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) {
          return PP_Bool.PP_TRUE;
        }
      }
      return PP_Bool.PP_FALSE;
    },

    /**
     * PP_Var ReadData([in] PP_Instance instance_id,
     *     [in] PP_Flash_Clipboard_Type clipboard_type,
     *     [in] uint32_t format);
     */
    PPB_Flash_Clipboard_ReadData: function(json) {
      //FIXME We only support standard clipboard w/ plaintext format
      if (PP_Flash_Clipboard_Format[json.clipboard_type] ==
          PP_Flash_Clipboard_Format.PP_FLASH_CLIPBOARD_TYPE_STANDARD) {
        if (json.format ==
            PP_Flash_Clipboard_Format.PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) {
          let trans = Cc["@mozilla.org/widget/transferable;1"]
            .createInstance(Ci.nsITransferable);
          trans.init(null)
          trans.addDataFlavor("text/unicode");
          Services.clipboard.getData(
            trans, Services.clipboard.kGlobalClipboard);
          let str = {};
          let strLength = {};
          trans.getTransferData("text/unicode", str, strLength);
          let pasteText = str.value.QueryInterface(Ci.nsISupportsString).data;
          return new String_PP_Var(pasteText);
        }
      }
      return new PP_Var();
    },

    /**
     * int32_t WriteData(
     *     [in] PP_Instance instance_id,
     *     [in] PP_Flash_Clipboard_Type clipboard_type,
     *     [in] uint32_t data_item_count,
     *     [in, size_is(data_item_count)] uint32_t[] formats,
     *     [in, size_is(data_item_count)] PP_Var[] data_items);
     */
    PPB_Flash_Clipboard_WriteData: function(json) {
      //FIXME We only support standard clipboard w/ plaintext format
      let clipboardHelper =
        Components.classes["@mozilla.org/widget/clipboardhelper;1"].
        getService(Components.interfaces.nsIClipboardHelper);
      for (let i = 0; i < json.data_item_count; ++i) {
        if (json.formats[i] ==
            PP_Flash_Clipboard_Format.PP_FLASH_CLIPBOARD_FORMAT_PLAINTEXT) {
          clipboardHelper.copyString(
            String_PP_Var.getAsJSValue(json.data_items[i]));
        } else {
          clipboardHelper.copyString("");
          return PP_ERROR_BADARGUMENT;
        }
      }
      return PP_OK;
    },


    /**
     * void SetInstanceAlwaysOnTop(
     *     [in] PP_Instance instance,
     *     [in] PP_Bool on_top);
     */
    PPB_Flash_SetInstanceAlwaysOnTop: function(json) {
    },

    /**
     * double_t GetLocalTimeZoneOffset(
     *     [in] PP_Instance instance,
     *     [in] PP_Time t);
     */
    PPB_Flash_GetLocalTimeZoneOffset: function(json) {
      return (new Date(json.t)).getTimezoneOffset() * 60;
    },

    /**
     * PP_Var GetCommandLineArgs(
     *     [in] PP_Module module);
     */
    PPB_Flash_GetCommandLineArgs: function(json) {
      // "enable_trace_to_console=1 log_level=5"
      return new String_PP_Var("enable_trace_to_console=1 log_level=5");
    },

    /**
     * PP_Var GetSetting(PP_Instance instance, PP_FlashSetting setting);
     */
    PPB_Flash_GetSetting: function(json) {
      switch (PP_FlashSetting[json.setting]) {
        case PP_FlashSetting.PP_FLASHSETTING_3DENABLED:
          return new Bool_PP_Var(true);
        case PP_FlashSetting.PP_FLASHSETTING_INCOGNITO:
          return new Bool_PP_Var(false);
        case PP_FlashSetting.PP_FLASHSETTING_STAGE3DENABLED:
          return new Bool_PP_Var(true);
        case PP_FlashSetting.PP_FLASHSETTING_LANGUAGE:
          return new String_PP_Var("en-US");
        case PP_FlashSetting.PP_FLASHSETTING_NUMCORES:
          return new Int32_PP_Var(4);
        case PP_FlashSetting.PP_FLASHSETTING_LSORESTRICTIONS:
          return new Int32_PP_Var(PP_FlashLSORestrictions.PP_FLASHLSORESTRICTIONS_IN_MEMORY);
      }
      return 0;
    },

    /**
     * PP_Bool SetCrashData([in] PP_Instance instance,
     *                      [in] PP_FlashCrashKey key,
     *                      [in] PP_Var value);
     */
    PPB_Flash_SetCrashData: function(json) {
      return PP_Bool.PP_TRUE;
    },


    /**
     * int32_t OpenFile(
     *     [in] PP_Instance instance,
     *     [in] str_t path,
     *     [in] int32_t mode,
     *     [out] PP_FileHandle file);
     */
    PPB_Flash_File_ModuleLocal_OpenFile: function(json) {
      let file = this.moduleLocalFiles.clone();
      json.path.split("/").forEach((pathItem) => file.appendRelativePath(pathItem));

      let flags;
      if (!(json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_WRITE)) {
        flags = PR_RDONLY;
        if (json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_APPEND) {
          flags |= PR_APPEND;
        }
      } else if (json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_READ) {
        flags = PR_RDWR;
      } else {
        flags = PR_WRONLY;
      }
      if (json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_TRUNCATE) {
        flags |= PR_TRUNCATE;
      }
      if (json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_CREATE) {
        flags |= PR_CREATE_FILE;
        if ((json.mode & PP_FileOpenFlags.PP_FILEOPENFLAG_EXCLUSIVE) && file.exists()) {
          flags |= PR_EXCL;
        }
      }

      try {
        let handle = this.process.openAndSend(file, flags, 0o600);
        return [PP_OK, { file: handle }];
      } catch (e) {
        return [PP_ERROR_FAILED, { file: null }];
      }
    },

    /**
     * int32_t RenameFile(
     *     [in] PP_Instance instance,
     *     [in] str_t path_from,
     *     [in] str_t path_to);
     */
    PPB_Flash_File_ModuleLocal_RenameFile: function(json) {
      let fromFile = this.moduleLocalFiles.clone();
      json.path_from.split("/").forEach((pathItem) => fromFile.appendRelativePath(pathItem));
      let toItems = json.path_to.split("/");
      let newName = toItems.pop();
      let toDir = this.moduleLocalFiles.clone();
      toItems.forEach((pathItem) => toDir.appendRelativePath(pathItem));
      try {
        fromFile.renameTo(toDir, newName);
        return PP_OK;
      } catch (e) {
        return PP_ERROR_FAILED;
      }
    },

    /**
     * int32_t DeleteFileOrDir(
     *     [in] PP_Instance instance,
     *     [in] str_t path,
     *     [in] PP_Bool recursive);
     */
    PPB_Flash_File_ModuleLocal_DeleteFileOrDir: function(json) {
      let file = this.moduleLocalFiles.clone();
      json.path.split("/").forEach((pathItem) => file.appendRelativePath(pathItem));
      if (file.exists()) {
        try {
          file.remove(PP_Bool[json.recursive] == PP_Bool.PP_TRUE);
        } catch (e) {
          dump(e.toSource() + "\n");
          return PP_ERROR_FAILED;
        }
      }
      return PP_OK;
    },

    /**
     * int32_t CreateDir(
     *     [in] PP_Instance instance,
     *     [in] str_t path);
     */
    PPB_Flash_File_ModuleLocal_CreateDir: function(json) {
      let directory = this.moduleLocalFiles.clone();
      json.path.split("/").forEach((pathItem) => directory.appendRelativePath(pathItem));
      try {
        directory.create(Ci.nsIFile.DIRECTORY_TYPE, 0o700);
      } catch (e) {
        if (e.result != Cr.NS_ERROR_FILE_ALREADY_EXISTS) {
          return PP_ERROR_FAILED;
        }
      }
      return PP_OK;
    },

    /**
     * int32_t QueryFile(
     *     [in] PP_Instance instance,
     *     [in] str_t path,
     *     [out] PP_FileInfo info);
     */
    PPB_Flash_File_ModuleLocal_QueryFile: function(json) {
      let file = this.moduleLocalFiles.clone();
      json.path.split("/").forEach((pathItem) => file.appendRelativePath(pathItem));
      if (!file.exists()) {
        return [PP_ERROR_FILENOTFOUND, { info: null }];
      }

      return [PP_OK, { info: {
        size: file.fileSize,
        type: file.isDirectory() ? PP_FileType.PP_FILETYPE_DIRECTORY : PP_FileType.PP_FILETYPE_REGULAR,
        system_type: PP_FileSystemType.PP_FILESYSTEMTYPE_LOCALPERSISTENT,
        creation_time: file.lastModifiedTime,
        last_access_time: file.lastModifiedTime,
        last_modified_time: file.lastModifiedTime
      } }];
    },

    /**
     * int32_t GetDirContents(
     *     [in] PP_Instance instance,
     *     [in] str_t path,
     *     [out] PP_DirContents_Dev contents);
     */
    PPB_Flash_File_ModuleLocal_GetDirContents: function(json) {
      let directory = this.moduleLocalFiles.clone();
      json.path.split("/").forEach((pathItem) => directory.appendRelativePath(pathItem));
      if (!directory.exists() || !directory.isDirectory()) {
        return [PP_ERROR_FILENOTFOUND, { contents: null }];
      }

      let entries = [];
      let enumerator = directory.directoryEntries;
      while (enumerator.hasMoreElements()) {
        let file = enumerator.getNext().QueryInterface(Ci.nsIFile);
        entries.push({ name: file.leafName, is_dir: file.isDirectory() });
      }
      return [PP_OK, { contents: { count: entries.length, entries } }];
    },

    /**
     * int32_t CreateTemporaryFile(
     *     [in] PP_Instance instance,
     *     [out] PP_FileHandle file);
     */
    PPB_Flash_File_ModuleLocal_CreateTemporaryFile: function(json) {
      let handle = this.process.openAndSend(null, PR_RDWR, 0o600);
      return [PP_OK, { file: handle }];
    },


    /**
     * PP_Bool IsFullscreen(
     *     [in] PP_Instance instance);
     */
    PPB_FlashFullscreen_IsFullscreen: function(json) {
      let instance = this.instances[json.instance];
      return instance.mm.sendRpcMessage("ppapi.js:isFullscreen")[0] ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * PP_Bool SetFullscreen(
     *     [in] PP_Instance instance,
     *     [in] PP_Bool fullscreen);
     */
    PPB_FlashFullscreen_SetFullscreen: function(json) {
      let instance = this.instances[json.instance];
      instance.mm.sendRpcMessage("ppapi.js:setFullscreen", json.fullScreen == PP_Bool.PP_TRUE);
    },

    /**
     * PP_Bool GetScreenSize(
     *     [in] PP_Instance instance,
     *     [out] PP_Size size);
     */
    PPB_FlashFullscreen_GetScreenSize: function(json) {
      let screen = this.instances[json.instance].window.screen;
      return [PP_Bool.PP_FALSE, { size: { width: screen.width, height: screen.height } }];
    },


    /**
     * PP_Resource Create([in] PP_Instance instance);
     */
    PPB_Flash_MessageLoop_Create: function(json) {
      return new Flash_MessageLoop(this.instances[json.instance]);
    },

    /**
     * int32_t Run([in] PP_Resource flash_message_loop);
     */
    PPB_Flash_MessageLoop_Run: function(json) {
      let messageLoop = PP_Resource.lookup(json.flash_message_loop);
      messageLoop.run();
      return PP_OK;
    },

    /**
     * void Quit([in] PP_Resource flash_message_loop);
     */
    PPB_Flash_MessageLoop_Quit: function(json) {
      let messageLoop = PP_Resource.lookup(json.flash_message_loop);
      messageLoop.quit();
    },


    /**
     * PP_Var GetFontFamilies(
     *     [in] PP_Instance instance);
     */
    PPB_Font_Dev_GetFontFamilies: function(json) {
      let enumerator = Cc["@mozilla.org/gfx/fontenumerator;1"].createInstance(Ci.nsIFontEnumerator);
      return new String_PP_Var(enumerator.EnumerateAllFonts({}).join('\0'));
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_Size size,
     *     [in] PP_Bool is_always_opaque);
     */
    PPB_Graphics2D_Create: function(json) {
      return new Graphics2D(this.instances[json.instance], json.size.width, json.size.height);
    },

    /**
     * void PaintImageData(
     *     [in] PP_Resource graphics_2d,
     *     [in] PP_Resource image_data,
     *     [in] PP_Point top_left,
     *     [in] PP_Rect src_rect);
     */
    PPB_Graphics2D_PaintImageData: function(json) {
      let graphics = PP_Resource.lookup(json.graphics_2d);
      let imageData = PP_Resource.lookup(json.image_data);
      let operation = new Graphics2DPaintOperation(imageData, json.top_left.x, json.top_left.y,
          [ json.src_rect.point.x, json.src_rect.point.y, json.src_rect.size.width, json.src_rect.size.height ]);
      graphics.addOperation(operation);
    },

    /**
     * void ReplaceContents(
     *     [in] PP_Resource graphics_2d,
     *     [in] PP_Resource image_data);
     */
    PPB_Graphics2D_ReplaceContents: function(json) {
      let graphics = PP_Resource.lookup(json.graphics_2d);
      let imageData = PP_Resource.lookup(json.image_data);
      let operation = new Graphics2DPaintOperation(imageData, 0, 0);
      graphics.clearOperations();
      graphics.addOperation(operation);
    },

    /**
     * void Scroll(
     *     [in] PP_Resource graphics_2d,
     *     [in] PP_Rect clip_rect,
     *     [in] PP_Point amount);
     */
    PPB_Graphics2D_Scroll: function(json) {
      let graphics = PP_Resource.lookup(json.graphics_2d);
      let operation = new Graphics2DScrollOperation([json.clip_rect.point.x, json.clip_rect.point.y,
          json.clip_rect.size.width, json.clip_rect.size.height], json.amount.x, json.amount.y);
      graphics.addOperation(operation);
    },

    /**
     * int32_t Flush(
     *     [in] PP_Resource graphics_2d,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_Graphics2D_Flush: function(json) {
      let graphics = PP_Resource.lookup(json.graphics_2d);
      return graphics.flush(json.callback);
    },

    /**
     * PP_Bool SetScale(
     *     [in] PP_Resource resource,
     *     [in] float_t scale);
     */
    PPB_Graphics2D_SetScale: function(json) {
      let graphics = PP_Resource.lookup(json.resource);
      let context = graphics.context;
      context.scale(json.scale, json.scale);
      return PP_Bool.PP_TRUE;
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_Resource share_context,
     *     [in] int32_t[] attrib_list);
     */
    PPB_Graphics3D_Create: function(json) {
      let instance = this.instances[json.instance];
      let attributes = new Map();
      for (let i = 0; i < json.attrib_list.length; i += 2) {
        attributes.set(json.attrib_list[i], json.attrib_list[i + 1]);
      }
      return new Graphics3D(instance, attributes);
    },

    /**
     * int32_t ResizeBuffers(
     *     [in] PP_Resource context,
     *     [in] int32_t width,
     *     [in] int32_t height);
     */
    PPB_Graphics3D_ResizeBuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      graphics.changeSize(json.width, json.height);
      return PP_OK;
    },

    /**
     * int32_t SwapBuffers(
     *     [in] PP_Resource context,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_Graphics3D_SwapBuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      return graphics.flush(json.callback);
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_ImageDataFormat format,
     *     [in] PP_Size size,
     *     [in] PP_Bool init_to_zero);
     */
    PPB_ImageData_Create: function(json) {
      return new ImageData(this.instances[json.instance],
                           PP_ImageDataFormat[json.format],
                           json.size);
    },

    /**
     * PP_Bool Describe(
     *     [in] PP_Resource image_data,
     *     [out] PP_ImageDataDesc desc);
     */
    PPB_ImageData_Describe: function(json) {
      let imageData = PP_Resource.lookup(json.image_data);
      return [PP_Bool.PP_TRUE, { desc: { format: imageData.format, size: imageData.size, stride: imageData.stride } }];
    },

    /**
     * mem_t Map(
     *     [in] PP_Resource image_data);
     */
    PPB_ImageData_Map: function(json) {
      let imageData = PP_Resource.lookup(json.image_data);
      return imageData.map();
    },


    /**
     * int32_t RequestInputEvents([in] PP_Instance instance,
     *                            [in] uint32_t event_classes);
     */
    PPB_InputEvent_RequestInputEvents: function(json) {
      let instance = this.instances[json.instance];
      return instance.registerEventHandler(json.event_classes, false);
    },

    /**
     * int32_t RequestFilteringInputEvents([in] PP_Instance instance,
     *                                     [in] uint32_t event_classes);
     */
    PPB_InputEvent_RequestFilteringInputEvents: function(json) {
      let instance = this.instances[json.instance];
      return instance.registerEventHandler(json.event_classes, true);
    },

    /**
     * PP_Bool IsInputEvent([in] PP_Resource resource);
     */
    PPB_InputEvent_IsInputEvent: function(json) {
      let event = PP_Resource.lookup(json.resource);
      return event instanceof InputEvent ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * PP_InputEvent_Type GetType([in] PP_Resource event);
     */
    PPB_InputEvent_GetType: function(json) {
      let event = PP_Resource.lookup(json.event);
      return event.eventType;
    },

    /**
     * PP_TimeTicks GetTimeStamp([in] PP_Resource event);
     */
    PPB_InputEvent_GetTimeStamp: function(json) {
      let event = PP_Resource.lookup(json.event);
      return event.timeStamp;
    },

    /**
     * uint32_t GetModifiers([in] PP_Resource event);
     */
    PPB_InputEvent_GetModifiers: function(json) {
      let event = PP_Resource.lookup(json.event);
      let modifiers = 0;
      for (let [gecko, ppapi] of ModifierMap) {
        if (event.domEvent.getModifierState(gecko)) {
          modifiers |= ppapi;
        }
      }

      if (event instanceof KeyboardInputEvent) {
        if (event.domEvent.location == event.domEvent.DOM_KEY_LOCATION_NUMPAD) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISKEYPAD;
        } else if (event.domEvent.location & event.domEvent.DOM_KEY_LOCATION_LEFT) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISLEFT;
        } else if (event.domEvent.location & event.domEvent.DOM_KEY_LOCATION_RIGHT) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISRIGHT;
        }

        if (event.domEvent.repeat) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT;
        }
      } else if (event instanceof MouseInputEvent) {
        if (event.domEvent.buttons & 0x01) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN;
        }
        if (event.domEvent.buttons & 0x04) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN;
        }
        if (event.domEvent.buttons & 0x02) {
          modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN;
        }
        if (event.domEvent.type == 'mouseup') {
          // mouseup event indicates the key released only in domEvent.button
          // rather than domEvent.buttons, but PDFium do use modifiers to
          // determine which button is released. So we make it up here.
          if (event.domEvent.button == 0) {
            modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_LEFTBUTTONDOWN;
          } else if (event.domEvent.button == 1) {
            modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_MIDDLEBUTTONDOWN;
          } else if (event.domEvent.button == 2) {
            modifiers |= PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_RIGHTBUTTONDOWN;
          }
        }
      }

      dump("MODIFIERS " + modifiers + "\n");
      return modifiers;
    },


    /**
     * PP_Bool BindGraphics(
     *     [in] PP_Instance instance,
     *     [in] PP_Resource device);
     */
    PPB_Instance_BindGraphics: function(json) {
      let instance = this.instances[json.instance];
      if (json.device === 0) {
        instance.bindGraphics(undefined);
      } else {
        instance.bindGraphics(PP_Resource.lookup(json.device));
      }
      return PP_Bool.PP_TRUE;
    },

    /**
     * PP_Bool IsFullFrame(
     *     [in] PP_Instance instance);
     */
    PPB_Instance_IsFullFrame: function(json) {
      let instance = this.instances[json.instance];
      return instance.info.isFullFrame ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },


    /**
     * PP_Var GetWindowObject([in] PP_Instance instance);
     */
    PPB_Instance_Private_GetWindowObject: function(json) {
      let instance = this.instances[json.instance];
      return new Object_PP_Var(instance.containerWindow, instance);
    },

    /**
     * PP_Var ExecuteScript([in] PP_Instance instance,
     *                      [in] PP_Var script,
     *                      [out] PP_Var exception);
     */
    PPB_Instance_Private_ExecuteScript: function(json) {
      let instance = this.instances[json.instance];
      let script = String_PP_Var.getAsJSValue(json.script);
      let result = instance.mm.sendRpcMessage("ppapiflash.js:executeScript", script, { instance })[0];
      return [result, { exception: null }];
      //return [new PP_Var(), { exception: PP_Var.fromJSValue(e) }];
    },

    /**
    * PP_Resource Create([in] PP_Instance instance,
    *                    [in] PP_InputEvent_Type type,
    *                    [in] PP_TimeTicks time_stamp,
    *                    [in] uint32_t modifiers,
    *                    [in] uint32_t key_code,
    *                    [in] PP_Var character_text,
    *                    [in] PP_Var code);
    */
    PPB_KeyboardInputEvent_Create: function(json) {
      let instance = this.instances[json.instance];
      let charCode = 0;
      if (PP_VarType[json.character_text.type] ==
          PP_VarType.PP_VARTYPE_STRING) {
        charCode = String_PP_Var.getAsJSValue(json.character_text).charCodeAt(0);
      }
      let location = instance.window.KeyboardEvent.DOM_KEY_LOCATION_STANDARD;
      if (PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISLEFT &
          json.modifiers) {
        location = instance.window.KeyboardEvent.DOM_KEY_LOCATION_LEFT;
      } else if (PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISRIGHT &
                 json.modifiers) {
        location = instance.window.KeyboardEvent.DOM_KEY_LOCATION_RIGHT;
      } else if(PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISKEYPAD &
                json.modifiers) {
        location = instance.window.KeyboardEvent.DOM_KEY_LOCATION_NUMPAD;
      }

      // FIXME I skipped to put |PP_Var code| into keyboardEventInit here
      // because I neither find any useful |code| value passing into here
      // nor have any PPB APIs which gets access to |code| now.
      let keyboardEventInit = {
        altKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ALTKEY &
          json.modifiers,
        charCode: charCode,
        ctrlKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_CONTROLKEY &
          json.modifiers,
        keyCode: json.key_code,
        location: location,
        metaKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_METAKEY &
          json.modifiers,
        repeat: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ISAUTOREPEAT &
          json.modifiers,
        shiftKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_SHIFTKEY &
          json.modifiers,
      };
      let eventName = EventByTypes.get(PP_InputEvent_Type[json.type]);
      let event = new instance.window.KeyboardEvent(eventName,
                                                    keyboardEventInit);
      let resource = new KeyboardInputEvent(instance, event);
      resource.timeStamp = json.time_stamp;
      return resource;
    },

    /**
     * PP_Bool IsKeyboardInputEvent([in] PP_Resource resource);
     */
    PPB_KeyboardInputEvent_IsKeyboardInputEvent: function(json) {
      let resource = PP_Resource.lookup(json.resource);
      return resource instanceof KeyboardInputEvent ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * uint32_t GetKeyCode([in] PP_Resource key_event);
     */
    PPB_KeyboardInputEvent_GetKeyCode: function(json) {
      let event = PP_Resource.lookup(json.key_event);
      return event.domEvent.keyCode;
    },

    /**
     * PP_Var GetCharacterText([in] PP_Resource character_event);
     */
    PPB_KeyboardInputEvent_GetCharacterText: function(json) {
      let event = PP_Resource.lookup(json.character_event);
      let charCode = event.domEvent.charCode;
      if (charCode === 0 ||
          event.domEvent.getModifierState("Control") ||
          event.domEvent.getModifierState("Meta")) {
        return new PP_Var();
      }
      return new String_PP_Var(String.fromCharCode(charCode));
    },


    /**
     * void PostMessage([in] PP_Instance instance, [in] PP_Var message);
     */
    PPB_Messaging_PostMessage: function(json) {
      if (PP_VarType[json.message.type] != PP_VarType.PP_VARTYPE_DICTIONARY) {
        return;
      }
      let instance = this.instances[json.instance];
      let message = PP_Var.toJSValue(json.message, instance);
      instance.viewport.notify(message);
    },

    /**
     * void SetTextInputType([in] PP_Instance instance,
     *                       [in] PP_TextInput_Type_Dev type);
     */
    PPB_TextInput_Dev_SetTextInputType: function(json) {
      let instance = this.instances[json.instance];
      let inputBox = instance.window.document.getElementById("IMEInput");
      switch (PP_TextInput_Type_Dev[json.type]) {
        case PP_TextInput_Type_Dev.PP_TEXTINPUT_TYPE_DEV_TEXT:
          if (inputBox) {
            inputBox.value = "";
            // we use setTimeout here to hold a small amount of time
            // to make sure that the focus state is stable when we
            // try to transfer focus. Same reason as the one later.
            instance.window.setTimeout(function() { inputBox.focus(); });
            return;
          }
          inputBox = instance.window.document.createElement("input");
          inputBox.type = "text";
          inputBox.id = "IMEInput";
          inputBox.style.bottom = "10px";
          inputBox.style.opacity = 0;
          inputBox.style.position = "fixed";
          inputBox.style.tabindex = -1;
          inputBox.style.zIndex = -1;
          instance.window.document.body.appendChild(inputBox);
          inputBox.addEventListener("blur", () => {
            instance.window.setTimeout(function() {
              // in PP_TEXTINPUT_TYPE_DEV_TEXT state, we only release the focus of inputBox
              // when the plugin(viewport) doesn't have focus.
              if (instance.viewport.is(instance.window.document.activeElement)) {
                inputBox.focus();
              } else {
                // if both inputBox and viewport don't have focus,
                // we might need to inform the plugin that it has lost focus.
                instance.didChangeFocus();
              }
            });
          });
          // when plugins accept text input but doesn't register text input related APIs,
          // the characters are delievered to plugins via PP_INPUTEVENT_TYPE_CHAR.
          if (!(instance.eventHandlers & PP_InputEvent_Class.PP_INPUTEVENT_CLASS_IME)) {
            inputBox.addEventListener("compositionstart", () => {
              inputBox.value = "";
              inputBox.style.opacity = 1;
              inputBox.style.zIndex = 1;
            });
            inputBox.addEventListener("compositionupdate", (e) => {
              if (!e.data) {
                inputBox.style.opacity = 0;
                inputBox.style.zIndex = -1;
              }
            });
            inputBox.addEventListener("compositionend", (e) => {
              inputBox.style.opacity = 0;
              inputBox.style.zIndex = -1;
              for (let i = 0; i < e.data.length; i++) {
                let keyboardEventInit = {
                  charCode: e.data.charCodeAt(i)
                };
                let event = new instance.window.KeyboardEvent("keypress", keyboardEventInit);
                instance.handleEvent(event);
              }
            });
          }
          instance.window.setTimeout(function() { inputBox.focus(); });
          break;
        default:
          if (inputBox) {
            instance.window.document.body.removeChild(inputBox);
          }
      }
    },

     /**
      * void UpdateCaretPosition([in] PP_Instance instance,
      *                          [in] PP_Rect caret,
      *                          [in] PP_Rect bounding_box);
      */
     PPB_TextInput_Dev_UpdateCaretPosition: function(json) {
       let instance = this.instances[json.instance];
       let inputBox = instance.window.document.getElementById("IMEInput");
       if (inputBox) {
         inputBox.style.left = json.bounding_box.point.x + 'px';
         inputBox.style.top = json.bounding_box.point.y + 'px';
       }
     },

    /**
     * PP_Bool IsIMEInputEvent([in] PP_Resource resource);
     */
    PPB_IMEInputEvent_Dev_IsIMEInputEvent: function(json) {
      let resource = PP_Resource.lookup(json.resource);
      return resource instanceof IMEInputEvent ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * PP_Var GetText([in] PP_Resource ime_event);
     */
    PPB_IMEInputEvent_Dev_GetText: function(json) {
      let event = PP_Resource.lookup(json.ime_event);
      return new String_PP_Var(event.domEvent.data);
    },


    /**
     * uint32_t GetSegmentNumber([in] PP_Resource ime_event)
     */
    PPB_IMEInputEvent_Dev_GetSegmentNumber: function(json) {
      let resource = PP_Resource.lookup(json.ime_event);
      if (resource.eventType == PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE) {
        let clauseArray = resource.domEvent.ranges;
        if (clauseArray && clauseArray.length) {
          return clauseArray.length - 1;
        }
      }
      return 0;
    },

    /**
     * int32_t GetTargetSegment([in] PP_Resource ime_event);
     */
    PPB_IMEInputEvent_Dev_GetTargetSegment: function(json) {
      let resource = PP_Resource.lookup(json.ime_event);
      if (resource.eventType == PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE) {
        if (resource.targetSegment != undefined) {
          return resource.targetSegment;
        }
      }
      return -1;
    },

    /**
     * uint32_t GetSegmentOffset([in] PP_Resource ime_event,[in] uint32_t index)
     */
    PPB_IMEInputEvent_Dev_GetSegmentOffset: function(json) {
      let resource = PP_Resource.lookup(json.ime_event);
      if (resource.eventType == PP_InputEvent_Type.PP_INPUTEVENT_TYPE_IME_COMPOSITION_UPDATE) {
        let index = json.index;
        if (resource.segmentOffset && resource.segmentOffset[index]) {
          return resource.segmentOffset[index];
        }
      }
      return 0;
    },

    /**
     * void GetSelection([in] PP_Resource ime_event,
     *                   [out] uint32_t start,
     *                   [out] uint32_t end);
     */
     PPB_IMEInputEvent_Dev_GetSelection: function(json) {
       let resource = PP_Resource.lookup(json.ime_event);
       let startOffset, endOffset;
       // Note: vaild targetSegment or segmentOffset could be zero.
       if (resource.targetSegment != undefined &&
           resource.segmentOffset[resource.targetSegment] != undefined &&
           resource.segmentOffset[resource.targetSegment + 1] != undefined) {
           startOffset = resource.segmentOffset[resource.targetSegment];
           endOffset = resource.segmentOffset[resource.targetSegment + 1];
       } else {
         // Spec doesn't define what values to return if there's no selected text.
         // In this case, startOffset and endOffset should be the same.
         // Here we set them to the end offset of the text.
         let encoder = new TextEncoder("utf-8");
         let data = resource.domEvent.data;
         startOffset = endOffset = encoder.encode(data).length;
       }
       return [{ start: startOffset, end: endOffset }];
     },

    /**
     * PP_Resource Create([in] PP_Instance instance,
     *                    [in] PP_InputEvent_Type type,
     *                    [in] PP_TimeTicks time_stamp,
     *                    [in] uint32_t modifiers,
     *                    [in] PP_InputEvent_MouseButton mouse_button,
     *                    [in] PP_Point mouse_position,
     *                    [in] int32_t click_count,
     *                    [in] PP_Point mouse_movement);
     */
    PPB_MouseInputEvent_Create: function(json) {
      let instance = this.instances[json.instance];
      let mouseEventInit = {
        altkey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_ALTKEY &
          json.modifiers,
        button: PP_InputEvent_MouseButton[json.mouse_button],
        clientX: json.mouse_position.x,
        clientY: json.mouse_position.y,
        ctrlKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_CONTROLKEY &
          json.modifiers,
        detail: json.click_count,
        metaKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_METAKEY &
          json.modifiers,
        movementX: json.mouse_movement.x,
        movementY: json.mouse_movement.y,
        shiftKey: PP_InputEvent_Modifier.PP_INPUTEVENT_MODIFIER_SHIFTKEY &
          json.modifiers
      };
      let eventName = EventByTypes.get(PP_InputEvent_Type[json.type]);
      let event = new instance.window.MouseEvent(eventName, mouseEventInit);
      let resource = new MouseInputEvent(instance, event);
      resource.timeStamp = json.time_stamp;
      return resource;
    },

    /**
     * PP_Bool IsMouseInputEvent([in] PP_Resource resource);
     */
    PPB_MouseInputEvent_IsMouseInputEvent: function(json) {
      let resource = PP_Resource.lookup(json.resource);
      return resource instanceof MouseInputEvent ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * PP_InputEvent_MouseButton GetButton([in] PP_Resource mouse_event);
     */
    PPB_MouseInputEvent_GetButton: function(json) {
      let event = PP_Resource.lookup(json.mouse_event);
      let button = event.domEvent.button;
      switch (button) {
        case -1:
          return PP_InputEvent_MouseButton.PP_INPUTEVENT_MOUSEBUTTON_NONE;
        case 0:
          return PP_InputEvent_MouseButton.PP_INPUTEVENT_MOUSEBUTTON_LEFT;
        case 1:
          return PP_InputEvent_MouseButton.PP_INPUTEVENT_MOUSEBUTTON_MIDDLE;
        case 2:
          return PP_InputEvent_MouseButton.PP_INPUTEVENT_MOUSEBUTTON_RIGHT;
        default:
          return PP_InputEvent_MouseButton.PP_INPUTEVENT_MOUSEBUTTON_LEFT;
      }
    },

    /**
     * PP_Point GetPosition([in] PP_Resource mouse_event)
     */
    PPB_MouseInputEvent_GetPosition: function(json) {
      let event = PP_Resource.lookup(json.mouse_event);
      return { x: event.domEvent.clientX, y: event.domEvent.clientY };
    },

    /**
     * int32_t GetClickCount([in] PP_Resource mouse_event);
     */
    PPB_MouseInputEvent_GetClickCount: function(json) {
      let event = PP_Resource.lookup(json.mouse_event);
      return event.domEvent.detail;
    },

    /**
     * PP_Point GetMovement([in] PP_Resource mouse_event);
     */
    PPB_MouseInputEvent_GetMovement: function(json) {
      let event = PP_Resource.lookup(json.mouse_event);
      return { x: event.domEvent.movementX, y: event.domEvent.movementY };
    },


    /**
     * PP_Resource Create([in] PP_Instance instance);
     */
    PPB_NetworkMonitor_Create: function(json) {
      return new NetworkMonitor(this.instances[json.instance]);
    },

    /**
     * int32_t UpdateNetworkList([in] PP_Resource network_monitor,
     *                           [out] PP_Resource network_list,
     *                           [in] PP_CompletionCallback callback);
     */
    PPB_NetworkMonitor_UpdateNetworkList: function(json) {
      return [ PP_ERROR_NOACCESS, { network_list: null } ];
    },


    /**
     * void ActiveTexture([in] PP_Resource context,
     *                    [in] GLenum texture);
     */
    PPB_OpenGLES2_ActiveTexture: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.activeTexture(json.texture);
    },

    /**
     * void AttachShader([in] PP_Resource context,
     *                   [in] GLuint program,
     *                   [in] GLuint shader);
     */
    PPB_OpenGLES2_AttachShader: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      let shader = graphics.objects.lookup(json.shader);
      context.attachShader(program, shader);
    },

    /**
     * void BindAttribLocation([in] PP_Resource context,
     *                         [in] GLuint program,
     *                         [in] GLuint index,
     *                         [in] cstr_t name);
     */
    PPB_OpenGLES2_BindAttribLocation: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      context.bindAttribLocation(program, json.index, json.name);
    },

    /**
     * void BindBuffer([in] PP_Resource context,
     *                 [in] GLenum target,
     *                 [in] GLuint buffer);
     */
    PPB_OpenGLES2_BindBuffer: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let buffer = graphics.objects.lookup(json.buffer);
      context.bindBuffer(json.target, buffer);
    },

    /**
     * void BindFramebuffer([in] PP_Resource context,
     *                      [in] GLenum target,
     *                      [in] GLuint framebuffer);
     */
    PPB_OpenGLES2_BindFramebuffer: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let frameBuffer = PP_Resource.lookup(json.framebuffer);
      context.bindFramebuffer(json.target, frameBuffer);
    },

    /**
     * void BindRenderbuffer([in] PP_Resource context,
     *                       [in] GLenum target,
     *                       [in] GLuint renderbuffer);
     */
    PPB_OpenGLES2_BindRenderbuffer: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let renderBuffer = PP_Resource.lookup(json.renderbuffer);
      context.bindRenderbuffer(json.target, renderBuffer);
    },

    /**
     * void BindTexture([in] PP_Resource context,
     *                  [in] GLenum target,
     *                  [in] GLuint texture);
     */
    PPB_OpenGLES2_BindTexture: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let texture = graphics.objects.lookup(json.texture);
      context.bindTexture(json.target, texture);
    },

    /**
     * void BlendColor([in] PP_Resource context,
     *                 [in] GLclampf red,
     *                 [in] GLclampf green,
     *                 [in] GLclampf blue,
     *                 [in] GLclampf alpha);
     */
    PPB_OpenGLES2_BlendColor: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.blendColor(json.red, json.green, json.blue, json.alpha);
    },

    /**
     * void BlendEquation([in] PP_Resource context,
     *                    [in] GLenum mode);
     */
    PPB_OpenGLES2_BlendEquation: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.blendEquation(json.mode);
    },

    /**
     * void BlendEquationSeparate([in] PP_Resource context,
     *                            [in] GLenum modeRGB,
     *                            [in] GLenum modeAlpha);
     */
    PPB_OpenGLES2_BlendEquationSeparate: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.blendEquationSeparate(json.modeRGB, json.modeAlpha);
    },

    /**
     * void BlendFunc([in] PP_Resource context,
     *                [in] GLenum sfactor,
     *                [in] GLenum dfactor);
     */
    PPB_OpenGLES2_BlendFunc: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.blendFunc(json.sfactor, json.dfactor);
    },

    /**
     * void BlendFuncSeparate([in] PP_Resource context,
     *                        [in] GLenum srcRGB,
     *                        [in] GLenum dstRGB,
     *                        [in] GLenum srcAlpha,
     *                        [in] GLenum dstAlpha);
     */
    PPB_OpenGLES2_BlendFuncSeparate: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.blendFuncSeparate(json.srcRGB, json.dstRGB, json.srcAlpha, json.dstAlpha);
    },

    /**
     * void BufferData([in] PP_Resource context,
     *                 [in] GLenum target,
     *                 [in] GLsizeiptr size,
     *                 [in] mem_t data,
     *                 [in] GLenum usage);
     */
    PPB_OpenGLES2_BufferData: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let data = this.copyBuffer(json.data, json.size);
      context.bufferData(json.target, data, json.usage);
    },

    /**
     * void BufferSubData([in] PP_Resource context,
     *                    [in] GLenum target,
     *                    [in] GLintptr offset,
     *                    [in] GLsizeiptr size,
     *                    [in] mem_t data);
     */
    PPB_OpenGLES2_BufferSubData: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let data = this.copyBuffer(json.data, json.size);
      context.bufferSubData(json.target, json.offset, data);
    },

    /**
     * GLenum CheckFramebufferStatus([in] PP_Resource context,
     *                               [in] GLenum target);
     */
    PPB_OpenGLES2_CheckFramebufferStatus: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return context.checkFramebufferStatus(json.target);
    },

    /**
     * void Clear([in] PP_Resource context,
     *            [in] GLbitfield mask);
     */
    PPB_OpenGLES2_Clear: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.clear(json.mask);
    },

    /**
     * void ClearColor([in] PP_Resource context,
     *                 [in] GLclampf red,
     *                 [in] GLclampf green,
     *                 [in] GLclampf blue,
     *                 [in] GLclampf alpha);
     */
    PPB_OpenGLES2_ClearColor: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.clearColor(json.red, json.green, json.blue, json.alpha);
    },

    /**
     * void ClearDepthf([in] PP_Resource context,
     *                  [in] GLclampf depth);
     */
    PPB_OpenGLES2_ClearDepthf: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.clearDepthf(json.depth);
    },

    /**
     * void ColorMask([in] PP_Resource context,
     *                [in] GLboolean red,
     *                [in] GLboolean green,
     *                [in] GLboolean blue,
     *                [in] GLboolean alpha);
     */
    PPB_OpenGLES2_ColorMask: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.colorMask(json.red, json.green, json.blue, json.alpha);
    },

    /**
     * void CompileShader([in] PP_Resource context,
     *                    [in] GLuint shader);
     */
    PPB_OpenGLES2_CompileShader: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let shader = graphics.objects.lookup(json.shader);
      context.compileShader(shader);
    },

    /**
     * void CopyTexImage2D([in] PP_Resource context,
     *                     [in] GLenum target,
     *                     [in] GLint level,
     *                     [in] GLenum internalformat,
     *                     [in] GLint x,
     *                     [in] GLint y,
     *                     [in] GLsizei width,
     *                     [in] GLsizei height,
     *                     [in] GLint border);
     */
    PPB_OpenGLES2_CopyTexImage2D: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.copyTexImage2D(json.target, json.level, json.internalformat,
                             json.x, json.y, json.width, json.height,
                             json.border);
    },

    /**
     * void CopyTexSubImage2D([in] PP_Resource context,
     *                        [in] GLenum target,
     *                        [in] GLint level,
     *                        [in] GLint xoffset,
     *                        [in] GLint yoffset,
     *                        [in] GLint x,
     *                        [in] GLint y,
     *                        [in] GLsizei width,
     *                        [in] GLsizei height);
     */
    PPB_OpenGLES2_CopyTexSubImage2D: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.copyTexSubImage2D(json.target, json.level, json.xoffset,
                                json.yoffset, json.x, json.y, json.width,
                                json.height);
    },

    /**
     * GLuint CreateProgram([in] PP_Resource context);
     */
    PPB_OpenGLES2_CreateProgram: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      return graphics.objects.add(graphics.context.createProgram());
    },

    /**
     * GLuint CreateShader([in] PP_Resource context,
     *                     [in] GLenum type);
     */
    PPB_OpenGLES2_CreateShader: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      return graphics.objects.add(graphics.context.createShader(json.type));
    },

    /**
     * void CullFace([in] PP_Resource context,
     *               [in] GLenum mode);
     */
    PPB_OpenGLES2_CullFace: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.cullFace(json.mode);
    },

    /**
     * void DeleteBuffers([in] PP_Resource context,
     *                    [in] GLsizei n,
     *                    [in, size_is(n)] GLuint[] buffers);
     */
    PPB_OpenGLES2_DeleteBuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      for (let i = 0; i < json.n; ++i) {
        context.deleteBuffer(graphics.objects.lookup(json.buffers[i]));
        graphics.objects.destroy(json.buffers[i]);
      }
    },

    /**
     * void DeleteFramebuffers([in] PP_Resource context,
     *                         [in] GLsizei n,
     *                         [in, size_is(n)] GLuint[] buffers);
     */
    PPB_OpenGLES2_DeleteFramebuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      for (let i = 0; i < json.n; ++i) {
        context.deleteFramebuffer(graphics.objects.lookup(json.buffers[i]));
        graphics.objects.destroy(json.buffers[i]);
      }
    },

    /**
     * void DeleteProgram([in] PP_Resource context,
     *                    [in] GLuint program);
     */
    PPB_OpenGLES2_DeleteProgram: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.deleteProgram(graphics.objects.lookup(json.program));
      graphics.objects.destroy(json.program);
    },

    /**
     * void DeleteRenderbuffers([in] PP_Resource context,
     *                          [in] GLsizei n,
     *                          [in, size_is(n)] GLuint[] renderbuffers);
     */
    PPB_OpenGLES2_DeleteRenderbuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      for (let i = 0; i < json.n; ++i) {
        context.deleteRenderbuffer(graphics.objects.lookup(json.renderbuffers[i]));
        graphics.objects.destroy(json.renderbuffers[i]);
      }
    },

    /**
     * void DeleteShader([in] PP_Resource context,
     *                   [in] GLuint shader);
     */
    PPB_OpenGLES2_DeleteShader: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.deleteShader(graphics.objects.lookup(json.shader));
      graphics.objects.destroy(json.shader);
    },

    /**
     * void DeleteTextures([in] PP_Resource context,
     *                     [in] GLsizei n,
     *                     [in, size_is(n)] GLuint[] textures);
     */
    PPB_OpenGLES2_DeleteTextures: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      for (let i = 0; i < json.n; ++i) {
        context.deleteTexture(graphics.objects.lookup(json.textures[i]));
        graphics.objects.destroy(json.textures[i]);
      }
    },

    /**
     * void DepthFunc([in] PP_Resource context,
     *                [in] GLenum func);
     */
    PPB_OpenGLES2_DepthFunc: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.depthFunc(json.func);
    },

    /**
     * void DepthMask([in] PP_Resource context,
     *                [in] GLboolean flag);
     */
    PPB_OpenGLES2_DepthMask: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.depthMask(json.flag);
    },

    /**
     * void DepthRangef([in] PP_Resource context,
     *                  [in] GLclampf zNear,
     *                  [in] GLclampf zFar);
     */
    PPB_OpenGLES2_DepthRangef: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.depthRangef(json.zNear, json.zFar);
    },

    /**
     * void DetachShader([in] PP_Resource context,
     *                   [in] GLuint program,
     *                   [in] GLuint shader);
     */
    PPB_OpenGLES2_DetachShader: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.detachShader(json.program, graphics.objects.lookup(json.shader));
    },

    /**
     * void Disable([in] PP_Resource context,
     *              [in] GLenum cap);
     */
    PPB_OpenGLES2_Disable: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.disable(json.cap);
    },

    /**
     * void DisableVertexAttribArray([in] PP_Resource context,
     *                               [in] GLuint index);
     */
    PPB_OpenGLES2_DisableVertexAttribArray: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.disableVertexAttribArray(json.index);
    },

    /**
     * void DrawArrays([in] PP_Resource context,
     *                 [in] GLenum mode,
     *                 [in] GLint first,
     *                 [in] GLsizei count);
     */
    PPB_OpenGLES2_DrawArrays: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.drawArrays(json.mode, json.first, json.count);
    },

    /**
     * void DrawElements([in] PP_Resource context,
     *                   [in] GLenum mode,
     *                   [in] GLsizei count,
     *                   [in] GLenum type,
     *                   [in] mem_t indices);
     */
    PPB_OpenGLES2_DrawElements: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.drawElements(json.mode, json.count, json.type, json.indices);
    },

    /**
     * void Enable([in] PP_Resource context,
     *             [in] GLenum cap);
     */
    PPB_OpenGLES2_Enable: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.enable(json.cap);
    },

    /**
     * void EnableVertexAttribArray([in] PP_Resource context,
     *                              [in] GLuint index);
     */
    PPB_OpenGLES2_EnableVertexAttribArray: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.enableVertexAttribArray(json.index);
    },

    /**
     * void Finish([in] PP_Resource context);
     */
    PPB_OpenGLES2_Finish: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.finish();
    },

    /**
     * void Flush([in] PP_Resource context);
     */
    PPB_OpenGLES2_Flush: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.flush();
    },

    /**
     * void FramebufferTexture2D([in] PP_Resource context,
     *                           [in] GLenum target,
     *                           [in] GLenum attachment,
     *                           [in] GLenum textarget,
     *                           [in] GLuint texture,
     *                           [in] GLint level);
     */
    PPB_OpenGLES2_FramebufferTexture2D: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.framebufferTexture2D(json.target, json.attachment, json.textarget, json.texture, json.level);
    },

    /**
     * void FrontFace([in] PP_Resource context,
     *                [in] GLenum mode);
     */
    PPB_OpenGLES2_FrontFace: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.frontFace(json.mode);
    },

    /**
     * void GenBuffers([in] PP_Resource context,
     *                 [in] GLsizei n,
     *                 [inout, size_is(n)] GLuint[] buffers);
     */
    PPB_OpenGLES2_GenBuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let buffers = [];
      for (let i = 0; i < json.n; ++i) {
        buffers.push(graphics.objects.add(context.createBuffer()));
      }
      return [{ buffers: buffers }];
    },

    /**
     * void GenerateMipmap([in] PP_Resource context,
     *                     [in] GLenum target);
     */
    PPB_OpenGLES2_GenerateMipmap: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.generateMipmap(json.target);
    },

    /**
     * void GenFramebuffers([in] PP_Resource context,
     *                      [in] GLsizei n,
     *                      [inout, size_is(n)] GLuint[] framebuffers);
     */
    PPB_OpenGLES2_GenFramebuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let framebuffers = [];
      for (let i = 0; i < json.n; ++i) {
        framebuffers.push(graphics.objects.add(context.createFramebuffer()));
      }
      return [{ framebuffers: framebuffers }];
    },

    /**
     * void GenRenderbuffers([in] PP_Resource context,
     *                       [in] GLsizei n,
     *                       [inout, size_is(n)] GLuint[] renderbuffers);
     */
    PPB_OpenGLES2_GenRenderbuffers: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let renderbuffers = [];
      for (let i = 0; i < json.n; ++i) {
        renderbuffers.push(graphics.objects.add(context.createRenderbuffer()));
      }
      return [{ renderbuffers: renderbuffers }];
    },

    /**
     * void GenTextures([in] PP_Resource context,
     *                  [in] GLsizei n,
     *                  [inout, size_is(n)] GLuint[] textures);
     */
    PPB_OpenGLES2_GenTextures: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let textures = [];
      for (let i = 0; i < json.n; ++i) {
        textures.push(graphics.objects.add(context.createTexture()));
      }
      return [{ textures: textures }];
    },

    /**
     * GLint GetAttribLocation([in] PP_Resource context,
     *                         [in] GLuint program,
     *                         [in] cstr_t name);
     */
    PPB_OpenGLES2_GetAttribLocation: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return context.getAttribLocation(json.program, json.name);
    },

    /**
     * GLenum GetError([in] PP_Resource context);
     */
    PPB_OpenGLES2_GetError: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return context.getError();
    },

    /**
     * void GetFloatv([in] PP_Resource context,
     *                [in] GLenum pname,
     *                [out] GLint_ptr_t params);
     */
    PPB_OpenGLES2_GetFloatv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return [{ params: context.getParameter(json.pname) }];
    },

    /**
     * void GetIntegerv([in] PP_Resource context,
     *                  [in] GLenum pname,
     *                  [out] GLint_ptr_t params);
     */
    PPB_OpenGLES2_GetIntegerv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return [{ params: context.getParameter(json.pname) }];
    },

    /**
     * void GetProgramiv([in] PP_Resource context,
     *                   [in] GLuint program,
     *                   [in] GLenum pname,
     *                   [out] GLint_ptr_t params);
     */
    PPB_OpenGLES2_GetProgramiv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      let param = context.getProgramParameter(program, json.pname);
      if (param === null) {
        param = context.getError();
      } else if (typeof param == "boolean") {
        param = param ? 1 : 0;
      }
      return [{ params: param }];
    },

    /**
     * void GetShaderiv([in] PP_Resource context,
     *                  [in] GLuint shader,
     *                  [in] GLenum pname,
     *                  [out] GLint_ptr_t params);
     */
    PPB_OpenGLES2_GetShaderiv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let shader = graphics.objects.lookup(json.shader);
      let param;
      if (json.pname == 0x8b84) { // GL_INFO_LOG_LENGTH
        let infoLog = context.getShaderInfoLog(shader);
        if (infoLog) {
          param = infoLog.length;
          if (param > 0) {
            param += 1;
          }
        } else {
          param = context.getError();
        }
      } else {
        param = context.getShaderParameter(shader, json.pname);
        if (param === null) {
          param = context.getError();
        } else if (typeof param == "boolean") {
          param = param ? 1 : 0;
        }
      }
      return [{ params: param }];
    },

    /**
     * void GetShaderInfoLog([in] PP_Resource context,
     *                       [in] GLuint shader,
     *                       [in] GLsizei bufsize,
     *                       [out] GLsizei_ptr_t length,
     *                       [out] str_t infolog);
     */
    PPB_OpenGLES2_GetShaderInfoLog: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let shader = graphics.objects.lookup(json.shader);
      let log = context.getShaderInfoLog(shader) + " ";
      return [{ length: log.length, infolog: log }];
    },

    /**
     * GLubyte_ptr_t GetString([in] PP_Resource context,
     *                         [in] GLenum name);
     */
    PPB_OpenGLES2_GetString: function(json) {
      let context = PP_Resource.lookup(json.context).context;
      if (json.name == 0x1f03) { // GL_EXTENSIONS
        let extensions = context.getSupportedExtensions();
        extensions = extensions.map((v) => {
          if (v.startsWith("EXT_") || v.startsWith("OES_")) {
            return "GL_" + v;
          }
          if (v == "WEBGL_compressed_texture_s3tc") {
            return "GL_EXT_texture_compression_s3tc";
          }
          if (v == "WEBGL_draw_buffers") {
            return "GL_EXT_draw_buffers";
          }
          return v;
        });
        return extensions.join(" ");
      }
      return PP_Resource.lookup(json.context).context.getParameter(json.name);
    },

    /**
     * GLint GetUniformLocation([in] PP_Resource context,
     *                          [in] GLuint program,
     *                          [in] cstr_t name);
     */
    PPB_OpenGLES2_GetUniformLocation: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      let location = context.getUniformLocation(program, json.name);
      return graphics.objects.add(location);
    },

    /**
     * void Hint([in] PP_Resource context,
     *           [in] GLenum target,
     *           [in] GLenum mode);
     */
    PPB_OpenGLES2_Hint: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.hint(json.target, json.mode);
    },

    /**
     * GLboolean IsEnabled([in] PP_Resource context,
     *                     [in] GLenum cap);
     */
    PPB_OpenGLES2_IsEnabled: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      return context.isEnabled(json.cap);
    },

    /**
     * void LineWidth([in] PP_Resource context,
     *                [in] GLfloat width);
     */
    PPB_OpenGLES2_LineWidth: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.lineWidth(json.width);
    },

    /**
     * void LinkProgram([in] PP_Resource context,
     *                  [in] GLuint program);
     */
    PPB_OpenGLES2_LinkProgram: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      context.linkProgram(program);
    },

    /**
     * void PixelStorei([in] PP_Resource context,
     *                  [in] GLenum pname,
     *                  [in] GLint param);
     */
    PPB_OpenGLES2_PixelStorei: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.pixelStorei(json.pname, json.param);
    },

    /**
     * void PolygonOffset([in] PP_Resource context,
     *                    [in] GLfloat factor,
     *                    [in] GLfloat units);
     */
    PPB_OpenGLES2_PolygonOffset: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.polygonOffset(json.factor, json.units);
    },

    /**
     * void ReadPixels([in] PP_Resource context,
     *                 [in] GLint x,
     *                 [in] GLint y,
     *                 [in] GLsizei width,
     *                 [in] GLsizei height,
     *                 [in] GLenum format,
     *                 [in] GLenum type,
     *                 [out] mem_t pixels);
     */
    PPB_OpenGLES2_ReadPixels: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let size = GLES2Utils.computeImageDataSize(context, json.width, json.height, json.format, json.type);
      let bytes = GLES2Utils.bytesPerElement(context, json.type);
      let source;
      switch (bytes) {
        case 4:
          source = new Float32Array(size / 4);
          break;
        case 2:
          source = new Uint16Array(size / 2);
          break;
        case 1:
          source = new Uint8Array(size);
          break;
      }
      context.readPixels(json.x, json.y, json.width, json.height, json.format, json.type, source);
      this.setBuffer(json.pixels, source);
      return [{ pixels: json.pixels }];
    },

    /**
     * void ReleaseShaderCompiler([in] PP_Resource context);
     */
    PPB_OpenGLES2_ReleaseShaderCompiler: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.releaseShaderCompiler();
    },

    /**
     * void RenderbufferStorage([in] PP_Resource context,
     *                          [in] GLenum target,
     *                          [in] GLenum internalformat,
     *                          [in] GLsizei width,
     *                          [in] GLsizei height);
     */
    PPB_OpenGLES2_RenderbufferStorage: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.renderbufferStorage(json.target, json.internalformat,
                                  json.width, json.height);
    },

    /**
     * void SampleCoverage([in] PP_Resource context,
     *                     [in] GLclampf value,
     *                     [in] GLboolean invert);
     */
    PPB_OpenGLES2_SampleCoverage: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.sampleCoverage(json.value, json.invert);
    },

    /**
     * void Scissor([in] PP_Resource context,
     *              [in] GLint x,
     *              [in] GLint y,
     *              [in] GLsizei width,
     *              [in] GLsizei height);
     */
    PPB_OpenGLES2_Scissor: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.scissor(json.x, json.y, json.width, json.height);
    },

    /**
     * void ShaderSource([in] PP_Resource context,
     *                   [in] GLuint shader,
     *                   [in] GLsizei count,
     *                   [in, size_is(count)] str_t[] str,
     *                   [in] GLint_ptr_t length);
     */
    PPB_OpenGLES2_ShaderSource: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let shader = graphics.objects.lookup(json.shader);
      for (let i = 0; i < json.count; ++i) {
        context.shaderSource(shader, json.str[i]);
      }
    },

    /**
     * void StencilFunc([in] PP_Resource context,
     *                  [in] GLenum func,
     *                  [in] GLint ref,
     *                  [in] GLuint mask);
     */
    PPB_OpenGLES2_StencilFunc: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilFunc(json.func, json.ref, json.mask);
    },

    /**
     * void StencilFuncSeparate([in] PP_Resource context,
     *                          [in] GLenum face,
     *                          [in] GLenum func,
     *                          [in] GLint ref,
     *                          [in] GLuint mask);
     */
    PPB_OpenGLES2_StencilFuncSeparate: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilFuncSeparate(json.face, json.func, json.ref, json.mask);
    },

    /**
     * void StencilMask([in] PP_Resource context,
     *                  [in] GLuint mask);
     */
    PPB_OpenGLES2_StencilMask: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilMask(json.mask);
    },

    /**
     * void StencilMaskSeparate([in] PP_Resource context,
     *                          [in] GLenum face,
     *                          [in] GLuint mask);
     */
    PPB_OpenGLES2_StencilMaskSeparate: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilMaskSeparate(json.face, json.mask);
    },

    /**
     * void StencilOp([in] PP_Resource context,
     *                [in] GLenum fail,
     *                [in] GLenum zfail,
     *                [in] GLenum zpass);
     */
    PPB_OpenGLES2_StencilOp: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilOp(json.fail, json.zfail, json.zpass);
    },

    /**
     * void StencilOpSeparate([in] PP_Resource context,
     *                        [in] GLenum face,
     *                        [in] GLenum fail,
     *                        [in] GLenum zfail,
     *                        [in] GLenum zpass);
     */
    PPB_OpenGLES2_StencilOpSeparate: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.stencilOpSeparate(json.face, json.fail, json.zfail, json.zpass);
    },

    /**
     * void TexImage2D([in] PP_Resource context,
     *                 [in] GLenum target,
     *                 [in] GLint level,
     *                 [in] GLint internalformat,
     *                 [in] GLsizei width,
     *                 [in] GLsizei height,
     *                 [in] GLint border,
     *                 [in] GLenum format,
     *                 [in] GLenum type,
     *                 [in] mem_t pixels);
     */
    PPB_OpenGLES2_TexImage2D: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.texImage2D(json.target, json.level, json.internalformat,
                         json.width, json.height, json.border, json.format,
                         json.type, json.pixels);
    },

    /**
     * void TexParameterf([in] PP_Resource context,
     *                    [in] GLenum target,
     *                    [in] GLenum pname,
     *                    [in] GLfloat param);
     */
    PPB_OpenGLES2_TexParameterf: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.texParameterf(json.target, json.pname, json.param);
    },

    /**
     * void TexParameteri([in] PP_Resource context,
     *                    [in] GLenum target,
     *                    [in] GLenum pname,
     *                    [in] GLint param);
     */
    PPB_OpenGLES2_TexParameteri: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.texParameteri(json.target, json.pname, json.param);
    },

    /**
     *  void TexSubImage2D([in] PP_Resource context,
     *                     [in] GLenum target,
     *                     [in] GLint level,
     *                     [in] GLint xoffset,
     *                     [in] GLint yoffset,
     *                     [in] GLsizei width,
     *                     [in] GLsizei height,
     *                     [in] GLenum format,
     *                     [in] GLenum type,
     *                     [in] mem_t pixels);
     */
    PPB_OpenGLES2_TexSubImage2D: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let size = GLES2Utils.computeImageDataSize(context, json.width, json.height,
                                                 json.format, json.type);
      let data = this.copyBuffer(json.pixels, size);
      let bytes = GLES2Utils.bytesPerElement(context, json.type);
      let view;
      switch (bytes) {
        case 4:
          view = new Float32Array(data);
          break;
        case 2:
          view = new Uint16Array(data);
          break;
        case 1:
          view = new Uint8Array(data);
          break;
      }
      context.texSubImage2D(json.target, json.level, json.xoffset, json.yoffset,
                            json.width, json.height, json.format, json.type, view);
    },

    /**
     * void Uniform1f([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLfloat x);
     */
    PPB_OpenGLES2_Uniform1f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform1f(location, json.x);
    },

    /**
     * void Uniform1i([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLint x);
     */
    PPB_OpenGLES2_Uniform1i: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform1i(location, json.x);
    },

    /**
     * void Uniform2f([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLfloat x,
     *                [in] GLfloat y);
     */
    PPB_OpenGLES2_Uniform2f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform2f(location, json.x, json.y);
    },

    /**
     * void Uniform2i([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLint x,
     *                [in] GLint y);
     */
    PPB_OpenGLES2_Uniform2i: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform2i(location, json.x, json.y);
    },

    /**
     * void Uniform3f([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLfloat x,
     *                [in] GLfloat y,
     *                [in] GLfloat z);
     */
    PPB_OpenGLES2_Uniform3f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform3f(location, json.x, json.y, json.z);
    },

    /**
     * void Uniform3i([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLint x,
     *                [in] GLint y,
     *                [in] GLint z);
     */
    PPB_OpenGLES2_Uniform3i: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform3i(location, json.x, json.y, json.z);
    },

    /**
     * void Uniform4fv([in] PP_Resource context,
     *                 [in] GLint location,
     *                 [in] GLsizei count,
     *                 [in] GLfloat_ptr_t v);
     */
    PPB_OpenGLES2_Uniform4fv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      let data = this.copyBuffer(json.v, json.count * 4);
      context.uniform4fv(location, new Float32Array(data));
    },

    /**
     * void Uniform4f([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLfloat x,
     *                [in] GLfloat y,
     *                [in] GLfloat z,
     *                [in] GLfloat w);
     */
    PPB_OpenGLES2_Uniform4f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform4f(location, json.x, json.y, json.z, json.w);
    },

    /**
     * void Uniform4i([in] PP_Resource context,
     *                [in] GLint location,
     *                [in] GLint x,
     *                [in] GLint y,
     *                [in] GLint z,
     *                [in] GLint w);
     */
    PPB_OpenGLES2_Uniform4i: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniform4i(location, json.x, json.y, json.z, json.w);
    },

    /**
     * void UniformMatrix3fv([in] PP_Resource context,
     *                       [in] GLint location,
     *                       [in] GLsizei count,
     *                       [in] GLboolean transpose,
     *                       [in] GLfloat_ptr_t value);
     */
    PPB_OpenGLES2_UniformMatrix3fv: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let location = graphics.objects.lookup(json.location);
      context.uniformMatrix3fv(location, json.transpose, json.value);
    },

    /**
     * void UseProgram([in] PP_Resource context,
     *                 [in] GLuint program);
     */
    PPB_OpenGLES2_UseProgram: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      context.useProgram(program);
    },

    /**
     * void ValidateProgram([in] PP_Resource context,
     *                      [in] GLuint program);
     */
    PPB_OpenGLES2_ValidateProgram: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let program = graphics.objects.lookup(json.program);
      context.validateProgram(program);
    },

    /**
     * void VertexAttrib1f([in] PP_Resource context,
     *                     [in] GLuint indx,
     *                     [in] GLfloat x);
     */
    PPB_OpenGLES2_VertexAttrib1f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.vertexAttrib1f(json.indx, json.x);
    },

    /**
     * void VertexAttrib2f([in] PP_Resource context,
     *                     [in] GLuint indx,
     *                     [in] GLfloat x,
     *                     [in] GLfloat y);
     */
    PPB_OpenGLES2_VertexAttrib2f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.vertexAttrib2f(json.indx, json.x, json.y);
    },

    /**
     * void VertexAttrib3f([in] PP_Resource context,
     *                     [in] GLuint indx,
     *                     [in] GLfloat x,
     *                     [in] GLfloat y,
     *                     [in] GLfloat z);
     */
    PPB_OpenGLES2_VertexAttrib3f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.vertexAttrib3f(json.indx, json.x, json.y, json.z);
    },

    /**
     * void VertexAttrib4f([in] PP_Resource context,
     *                     [in] GLuint indx,
     *                     [in] GLfloat x,
     *                     [in] GLfloat y,
     *                     [in] GLfloat z,
     *                     [in] GLfloat w);
     */
    PPB_OpenGLES2_VertexAttrib4f: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.vertexAttrib4f(json.indx, json.x, json.y, json.z, json.w);
    },

    /**
     * void VertexAttribPointer([in] PP_Resource context,
     *                          [in] GLuint indx,
     *                          [in] GLint size,
     *                          [in] GLenum type,
     *                          [in] GLboolean normalized,
     *                          [in] GLsizei stride,
     *                          [in] mem_t ptr);
     */
    PPB_OpenGLES2_VertexAttribPointer: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      if (json.ptr !== null)
        throw Error("Need to convert ptr to an offset!");
      context.vertexAttribPointer(json.indx, json.size, json.type,
                                  json.normalized, json.stride, json.ptr);
    },

    /**
     * void Viewport([in] PP_Resource context,
     *               [in] GLint x,
     *               [in] GLint y,
     *               [in] GLsizei width,
     *               [in] GLsizei height);
     */
    PPB_OpenGLES2_Viewport: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      context.viewport(json.x, json.y, json.width, json.height);
    },


    /**
     * mem_t MapTexSubImage2DCHROMIUM([in] PP_Resource context,
     *                                [in] GLenum target,
     *                                [in] GLint level,
     *                                [in] GLint xoffset,
     *                                [in] GLint yoffset,
     *                                [in] GLsizei width,
     *                                [in] GLsizei height,
     *                                [in] GLenum format,
     *                                [in] GLenum type,
     *                                [in] GLenum access);
     */
    PPB_OpenGLES2ChromiumMapSub_MapTexSubImage2DCHROMIUM: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let size = GLES2Utils.computeImageDataSize(context, json.width, json.height,
                                                 json.format, json.type);
      let mem = this.allocateCachedBuffer(size);
      graphics.mappedTextures.set(mem, () => {
        let source = this.getCachedBuffer(mem);
        let bytes = GLES2Utils.bytesPerElement(context, json.type);
        let view;
        switch (bytes) {
          case 4:
            view = new Float32Array(source);
            break;
          case 2:
            view = new Uint16Array(source);
            break;
          case 1:
            view = new Uint8Array(source);
            break;
        }
        context.texSubImage2D(json.target, json.level, json.xoffset,
                              json.yoffset, json.width, json.height,
                              json.format, json.type, view);
        this.freeCachedBuffer(mem);
      });
      return mem;
    },

    /**
     * void UnmapTexSubImage2DCHROMIUM([in] PP_Resource context,
     *                                 [in] mem_t mem);
     */
    PPB_OpenGLES2ChromiumMapSub_UnmapTexSubImage2DCHROMIUM: function(json) {
      let graphics = PP_Resource.lookup(json.context);
      let context = graphics.context;
      let mappedTexture = graphics.mappedTextures.get(json.mem);
      graphics.mappedTextures.delete(json.mem);
      mappedTexture();
    },


    /**
     * PP_Resource GetFontFileWithFallback([in] PP_Instance instance,
     *                                     [in] PP_BrowserFont_Trusted_Description description,
     *                                     [in] PP_PrivateFontCharset charset);
     */
    PPB_PDF_GetFontFileWithFallback: function(json) {
      return 0;
    },

    /**
     * void Print(
     *   [in] PP_Instance instance);
     */
    PPB_PDF_Print: function(json) {
      let instance = this.instances[json.instance];
      let pageRangeInfo = instance.pageRangeInfo;

      // Query supported formats
      let supportedFormats = instance.rt.call(new InterfaceMemberCall(
        "PPP_Printing(Dev);0.6", "QuerySupportedFormats", { instance }), true);
      // XXX We only support PDF output format now.
      if (!(PP_PrintOutputFormat_Dev.PP_PRINTOUTPUTFORMAT_PDF &
          supportedFormats)) {
        return;
      }

      // Begin of printing
      let totalPageNumber = instance.rt.call(new InterfaceMemberCall(
        "PPP_Printing(Dev);0.6", "Begin", { instance, print_settings:
        instance.ppapiPrintSettings }), true);
      if (totalPageNumber <= 0) {
        return;
      }

      // Get PDF file
      if (pageRangeInfo.printRange == Ci.nsIPrintSettings.kRangeAllPages) {
        pageRangeInfo.startPageRange = 1;
        pageRangeInfo.endPageRange = totalPageNumber;
      } else {
        if (pageRangeInfo.startPageRange < 1) {
          pageRanges.startPageRange = 1;
        }
        if (pageRangeInfo.endPageRange > totalPageNumber) {
          pageRangeInfo.endPageRange = totalPageNumber;
        }
        if (pageRangeInfo.startPageRange > pageRangeInfo.endPageRange) {
          return;
        }
      }
      // XXX We only support one page range now:
      //     Gecko is using OS native print dialog now. And the print dialog
      //     on Linux do support multiple page ranges. However, there is no
      //     existing XPCOM interface for us to get the multiple page ranges
      //     information. In addition, the print dialog on Mac and Windows only
      //     support one page range.
      //
      //     The behavior of printing muliple page ranges on Linux now is we
      //     will print out the pages from the minimum start page to the
      //     maximum last page of the page ranges.
      let pageRanges = [{ from: pageRangeInfo.startPageRange - 1,
        to: pageRangeInfo.endPageRange - 1 }];
      let pageRangeCount = pageRanges.length;
      let bufferId = instance.rt.call(new InterfaceMemberCall(
        "PPP_Printing(Dev);0.6", "PrintPages", {
          instance, page_ranges: pageRanges, page_range_count: pageRangeCount
        }), true);
      if (!bufferId) {
        return;
      }
      let buffer = PP_Resource.lookup(bufferId);

      // Save PDF to file
      let file = Services.dirsvc.get(PRINT_TEMP_KEY, Ci.nsIFile);
      file.append(PRINT_FILE_NAME);
      file.createUnique(Ci.nsIFile.NORMAL_FILE_TYPE, PR_IRUSR | PR_IWUSR);
      let stream = Cc["@mozilla.org/network/file-output-stream;1"].
                   createInstance(Ci.nsIFileOutputStream);
      stream.init(file, PR_RDWR | PR_CREATE_FILE | PR_TRUNCATE,
                  PR_IRUSR | PR_IWUSR, 0);
      let binaryStream = Cc["@mozilla.org/binaryoutputstream;1"].
                         createInstance(Ci.nsIBinaryOutputStream);
      binaryStream.setOutputStream(stream);
      let data = new Uint8ClampedArray(instance.rt.getCachedBuffer(buffer.mem));
      binaryStream.writeByteArray(Array.from(data), buffer.size);

      // End of printing
      stream.flush();
      stream.close();
      buffer.unmap();
      buffer.release();
      instance.rt.call(new InterfaceMemberCall(
        "PPP_Printing(Dev);0.6", "End", { instance }), true);
      // We need permission for printing PDF file
      instance.mm.sendAsyncMessage("ppapipdf.js:printPDF", {
        filePath: file.path });
    },

    /**
     * void SearchString(
     *   [in] PP_Instance instance,
     *   [in] mem_t str,
     *   [in] mem_t term,
     *   [in] PP_Bool case_sensitive,
     *   [out, size_is(count)] PP_PrivateFindResult[] results,
     *   [out] int32_t count);
     */
    PPB_PDF_SearchString: function(json) {
      // Retrieve data from buffer
      let tmp = "";
      let data = "";
      let term = "";
      let i = 0;
      while (1) {
        tmp = this.copyBuffer(json.str + i * 2, 2);
        if ((new Uint16Array(tmp))[0] == 0) {
          tmp = this.copyBuffer(json.str, i * 2);
          data = new TextDecoder("utf-16").decode(tmp);
          break;
        }
        ++i;
      }

      // Retrieve term from buffer
      i = 0;
      while (1) {
        tmp = this.copyBuffer(json.term + i * 2, 2);
        if ((new Uint16Array(tmp))[0] == 0) {
          tmp = this.copyBuffer(json.term, i * 2);
          term = new TextDecoder("utf-16").decode(tmp);
          break;
        }
        ++i;
      }

      // FIXME Should use ICU to search string
      let results = [];
      let n = -1;
      if (PP_Bool[json.case_sensitive] == PP_Bool.PP_FALSE) {
        // case-insensitive
        data = data.toLowerCase();
        term = term.toLowerCase();
      }
      while ((n = data.indexOf(term, n + 1)) != -1) {
        results.push({start_index: n, length: term.length});
      }
      return [{results, count: results.length}];
    },

    /*
     * void SetSelectedText(
     *     [in] PP_Instance instance,
     *     [in] str_t selected_text);
     */
    PPB_PDF_SetSelectedText: function(json) {
      let instance = this.instances[json.instance];
      instance.selectedText = json.selected_text;
    },


    /**
     * PP_Resource Create([in] PP_Instance instance);
     */
    PPB_Printing_Dev_Create: function(json) {
      return new PrintingDev(this.instances[json.instance]);
    },


    /**
     * PP_Resource Create([in] PP_Instance instance);
     */
    PPB_TCPSocket_Private_Create: function(json) {
      return new TCPSocketPrivate(this.instances[json.instance]);
    },

    /**
     * int32_t Connect([in] PP_Resource tcp_socket,
     *                 [in] str_t host,
     *                 [in] uint16_t port,
     *                 [in] PP_CompletionCallback callback);
     */
    PPB_TCPSocket_Private_Connect: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket);
      socket.connect(json.host, json.port, json.callback);
      return PP_OK_COMPLETIONPENDING;
    },

    /**
     * PP_Bool GetLocalAddress([in] PP_Resource tcp_socket,
     *                         [out] PP_NetAddress_Private local_addr);
     */
    PPB_TCPSocket_Private_GetLocalAddress: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket);
      let localAddr = socket.localAddress;
      if (!localAddr) {
        return [PP_Bool.PP_FALSE, { local_addr: null }];
      }
      return [PP_Bool.PP_FALSE, { local_addr: { size: localAddr.length, data: localAddr } }];
    },

    /**
     * PP_Bool GetRemoteAddress([in] PP_Resource tcp_socket,
     *                          [out] PP_NetAddress_Private remote_addr);
     */
    PPB_TCPSocket_Private_GetRemoteAddress: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket);
      let remoteAddr = socket.remoteAddress;
      if (!remoteAddr) {
        return [PP_Bool.PP_FALSE, { remote_addr: null }];
      }
      return [PP_Bool.PP_FALSE, { remote_addr: { size: remoteAddr.length, data: remoteAddr } }];
    },

    /**
     * int32_t Read([in] PP_Resource tcp_socket,
     *              [out] str_t buffer,
     *              [in] int32_t bytes_to_read,
     *              [in] PP_CompletionCallback callback);
     */
    PPB_TCPSocket_Private_Read: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket);
      socket.read(json.buffer, json.bytes_to_read, json.callback);
      return [PP_OK_COMPLETIONPENDING, { buffer: "" }];
    },

    /**
     * int32_t Write([in] PP_Resource tcp_socket,
     *               [in] str_t buffer,
     *               [in] int32_t bytes_to_write,
     *               [in] PP_CompletionCallback callback);
     */
    PPB_TCPSocket_Private_Write: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket);
      if (!socket.write(json.buffer)) {
        return PP_ERROR_FAILED;
      }
      socket.instance.rt.call(new CallbackCall("PP_CompletionCallback", json.callback, { result: json.bytes_to_write }));
      return PP_OK_COMPLETIONPENDING;
    },

    /**
     * void Disconnect([in] PP_Resource tcp_socket);
     */
    PPB_TCPSocket_Private_Disconnect: function(json) {
      let socket = PP_Resource.lookup(json.tcp_socket).impl;
      socket.close();
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance);
     */
    PPB_URLLoader_Create: function(json) {
      return new URLLoader(this.instances[json.instance]);
    },

    /**
     * int32_t Open(
     *     [in] PP_Resource loader,
     *     [in] PP_Resource request_info,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_URLLoader_Open: function(json) {
      let loader = PP_Resource.lookup(json.loader);
      let requestInfo = PP_Resource.lookup(json.request_info);
      loader.openURL(requestInfo.getProperty(PP_URLRequestProperty.PP_URLREQUESTPROPERTY_METHOD),
                     requestInfo.getProperty(PP_URLRequestProperty.PP_URLREQUESTPROPERTY_URL),
                     (result) => { loader.instance.rt.call(new CallbackCall("PP_CompletionCallback", json.callback, { result: result })); });
      return PP_OK_COMPLETIONPENDING;
    },

    /**
     * [always_set_output_parameters]
     * PP_Bool GetDownloadProgress(
     *     [in] PP_Resource loader,
     *     [out] int64_t bytes_received,
     *     [out] int64_t total_bytes_to_be_received);
     */
    PPB_URLLoader_GetDownloadProgress: function(json) {
      let loader = PP_Resource.lookup(json.loader);
      return [PP_Bool.PP_TRUE, { bytes_received: loader.bytes_received, total_bytes_to_be_received: loader.total_bytes_to_be_received }];
    },

    /**
     * PP_Resource GetResponseInfo(
     *     [in] PP_Resource loader);
     */
    PPB_URLLoader_GetResponseInfo: function(json) {
      return PP_Resource.lookup(json.loader).responseInfo;
    },

    /**
     * int32_t ReadResponseBody(
     *     [in] PP_Resource loader,
     *     [out] mem_t buffer,
     *     [in] int32_t bytes_to_read,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_URLLoader_ReadResponseBody: function(json) {
      let loader = PP_Resource.lookup(json.loader);
      let bytesRead = loader.readResponse(json.buffer,
                                          json.bytes_to_read,
                                          (bytesRead) => loader.instance.rt.call(new CallbackCall("PP_CompletionCallback", json.callback, { result: bytesRead })));
      // FIXME Why is buffer an out param?
      return [bytesRead < 0 ? PP_OK_COMPLETIONPENDING : bytesRead, { buffer: json.buffer }];
    },


    /**
     * void GrantUniversalAccess([in] PP_Resource loader);
     */
    PPB_URLLoaderTrusted_GrantUniversalAccess: function(json) {
      PP_Resource.lookup(json.loader).grantedUniversalAccess = true;
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance);
     */
    PPB_URLRequestInfo_Create: function(json) {
      return new URLRequestInfo(this.instances[json.instance]);
    },

    /**
     * PP_Bool SetProperty(
     *     [in] PP_Resource request,
     *     [in] PP_URLRequestProperty property,
     *     [in] PP_Var value);
     */
    PPB_URLRequestInfo_SetProperty: function(json) {
      let request = PP_Resource.lookup(json.request);
      let value;
      let type = PP_VarType[json.value.type];
      let property = PP_URLRequestProperty[json.property];
      switch (property) {
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_CUSTOMREFERRERURL:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_CUSTOMCONTENTTRANSFERENCODING:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_CUSTOMUSERAGENT:
          if (type == PP_VarType.PP_VARTYPE_UNDEFINED) {
            value = undefined;
            break;
          }
          /* falls through */

        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_URL:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_METHOD:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_HEADERS:
          if (type != PP_VarType.PP_VARTYPE_STRING) {
            throw Error("Expected string resource property");
          }
          value = String_PP_Var.getAsJSValue(json.value);
          break;

        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_STREAMTOFILE:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_FOLLOWREDIRECTS:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_RECORDDOWNLOADPROGRESS:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_RECORDUPLOADPROGRESS:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_ALLOWCROSSORIGINREQUESTS:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_ALLOWCREDENTIALS:
          if (type != PP_VarType.PP_VARTYPE_BOOL) {
            throw Error("Expected boolean resource property");
          }
          value = Bool_PP_Var.getAsJSValue(json.value);
          break;

        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_PREFETCHBUFFERUPPERTHRESHOLD:
        case PP_URLRequestProperty.PP_URLREQUESTPROPERTY_PREFETCHBUFFERLOWERTHRESHOLD:
          if (type != PP_VarType.PP_VARTYPE_INT32) {
            throw Error("Expected integer resource property");
          }
          value = Int32_PP_Var.getAsJSValue(json.value);
          break;
      }

      request.setProperty(property, value);
      return PP_Bool.PP_TRUE;
    },


    /**
     * PP_Var GetProperty(
     *     [in] PP_Resource response,
     *     [in] PP_URLResponseProperty property);
     */
    PPB_URLResponseInfo_GetProperty: function(json) {
      let response = PP_Resource.lookup(json.response);
      return response.getProperty(PP_URLResponseProperty[json.property]);
    },


    /**
     * PP_Var GetDocumentURL([in] PP_Instance instance,
     *                       [out] PP_URLComponents_Dev components);
     */
    PPB_URLUtil_Dev_GetDocumentURL: function(json) {
      var url = this.instances[json.instance].info.documentURL;
      return [new String_PP_Var(url), { components: this.parseURL(url) }];
    },

    /**
     * PP_Var GetPluginInstanceURL([in] PP_Instance instance,
     *                             [out] PP_URLComponents_Dev components);
     */
    PPB_URLUtil_Dev_GetPluginInstanceURL: function(json) {
      var url = this.instances[json.instance].info.url;
      return [new String_PP_Var(url), { components: this.parseURL(url) }];
    },


    /**
     * void AddRef([in] PP_Var var);
     */
    PPB_Var_AddRef: function(json) {
      json.var.type = PP_VarType[json.var.type];
      PP_Var_Cached.addRef(json.var);
    },

    /**
     * void Release([in] PP_Var var);
     */
    PPB_Var_Release: function(json) {
      json.var.type = PP_VarType[json.var.type];
      PP_Var_Cached.release(json.var);
    },

    /**
     * PP_Var VarFromUtf8([in] str_t data, [in] uint32_t len);
     */
    PPB_Var_VarFromUtf8: function(json) {
      return new String_PP_Var(json.data);
    },

    /**
     * str_t VarToUtf8([in] PP_Var var, [out] uint32_t len);
     */
    PPB_Var_VarToUtf8: function(json) {
      if (PP_VarType[json.var.type] != PP_VarType.PP_VARTYPE_STRING) {
        return [null, { len: 0 }];
      }
      let data = String_PP_Var.getAsJSValue(json.var);
      let converted = new TextEncoder().encode(data);
      return [data, { len: converted.length }];
    },


    /**
     * PP_Bool IsInstanceOf([in] PP_Var var,
     *                      [in, ref] PPP_Class_Deprecated object_class,
     *                      [out] mem_t object_data);
     */
    PPB_Var_Deprecated_IsInstanceOf: function(json) {
      let [object, instance] = Object_PP_Var.getAsJSValueWithInstance(json.var);
      let [isInstanceOf, object_data] = instance.mm.sendRpcMessage("ppapiflash.js:isInstanceOf", { objectClass: json.object_class }, { object, instance })[0];
      if (!isInstanceOf) {
        return PP_Bool.PP_FALSE;
      }
      return [PP_Bool.PP_TRUE, { object_data }];
    },

    /**
     * PP_Var CreateObject([in] PP_Instance instance,
     *                     [in, ref] PPP_Class_Deprecated object_class,
     *                     [inout] mem_t object_data);
     */
    PPB_Var_Deprecated_CreateObject: function(json) {
      let instance = this.instances[json.instance];
      let call = (name, args) => {
        let callObj = { __interface: "PPP_Class_Deprecated;1.0", __instance: json.object_class, __member: name, object: json.object_data };
        args = JSON.parse(args);
        if (args) {
          args.entries().map((name, value) => {
            callObj[name] = PP_Var.fromJSValue(value, instance);
          });
        }
        return this.call(callObj, true);
      };
      return instance.mm.sendRpcMessage("ppapiflash.js:createObject", { objectClass: json.object_class, objectData: json.object_data }, { instance, call })[0];
    },

    /**
     * PP_Var GetProperty([in] PP_Var object,
     *                    [in] PP_Var name,
     *                    [out] PP_Var exception);
     */
    PPB_Var_Deprecated_GetProperty: function(json) {
      let [object, instance] = Object_PP_Var.getAsJSValueWithInstance(json.object);
      let name = String_PP_Var.getAsJSValue(json.name);
      return instance.mm.sendRpcMessage("ppapiflash.js:getProperty", { name }, { object, instance })[0];
    },


    /**
     * PP_Var Create();
     */
    PPB_VarArray_Create: function(json) {
      return new Array_PP_Var();
    },

    /**
     * PP_Var Get([in] PP_Var array, [in] uint32_t index);
     */
    PPB_VarArray_Get: function(json) {
      let array = Array_PP_Var.getAsJSValue(json.dict);
      if (json.index >= array.length) {
        return new PP_Var();
      }
      let value = array[json.index];
      PP_Var_Cached.addRef(value);
      return PP_Var.normalize(value);
    },

    /**
     * PP_Bool Set([in] PP_Var array, [in] uint32_t index, [in] PP_Var value);
     */
    PPB_VarArray_Set: function(json) {
      json.value.type = PP_VarType[json.value.type];
      Array_PP_Var.getAsJSValue(json.array)[json.index] = json.value;
      PP_Var_Cached.addRef(json.value);
      return PP_Bool.PP_TRUE;
    },

    /**
     * uint32_t GetLength([in] PP_Var array);
     */
    PPB_VarArray_GetLength: function(json) {
      return Array_PP_Var.getAsJSValue(json.array).length;
    },

    /**
     * PP_Var Create();
     */
    PPB_VarDictionary_Create: function(json) {
      return new Dictionary_PP_Var();
    },

    /**
     * PP_Var Get([in] PP_Var dict, [in] PP_Var key);
     */
    PPB_VarDictionary_Get: function(json) {
      let value = Dictionary_PP_Var.getAsJSValue(json.dict)[String_PP_Var.getAsJSValue(json.key)];
      if (value === undefined) {
        return new PP_Var();
      }
      PP_Var_Cached.addRef(value);
      return PP_Var.normalize(value);
    },

    /**
     * PP_Bool Set([in] PP_Var dict, [in] PP_Var key, [in] PP_Var value);
     */
    PPB_VarDictionary_Set: function(json) {
      json.value.type = PP_VarType[json.value.type];
      Dictionary_PP_Var.getAsJSValue(json.dict)[String_PP_Var.getAsJSValue(json.key)] = json.value;
      PP_Var_Cached.addRef(json.value);
      return PP_Bool.PP_TRUE;
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance);
     */
    PPB_VideoCapture_Dev_Create: function(json) {
      return 0;
    },

    /**
     * int32_t EnumerateDevices(
     *     [in] PP_Resource video_capture,
     *     [in] PP_ArrayOutput output,
     *     [in] PP_CompletionCallback callback);
     */
    PPB_VideoCapture_Dev_EnumerateDevices: function(json) {
      return PP_ERROR_BADRESOURCE;
    },


    /**
     * PP_Resource Create(
     *     [in] PP_Instance instance,
     *     [in] PP_Resource context,
     *     [in] PP_VideoDecoder_Profile profile);
     */
    PPB_VideoDecoder_Dev_Create: function(json) {
      return 0;
    },


    /**
     * PP_Bool GetRect([in] PP_Resource resource,
     *                 [out] PP_Rect rect);
     */
    PPB_View_GetRect: function(json) {
      let view = PP_Resource.lookup(json.resource);
      let rect = view.instance.boundingRect;
      return [PP_Bool.PP_TRUE, { rect: { point: { x: rect.left, y: rect.top }, size: { width: rect.width, height: rect.height } } }];
    },

    /**
     * PP_Bool GetClipRect([in] PP_Resource resource,
     *                     [out] PP_Rect clip);
     */
    PPB_View_GetClipRect: function(json) {
      let view = PP_Resource.lookup(json.resource);
      let point = { x: 0, y: 0 };
      let size;
      if (view.instance.throttled) {
        size = { width: 0, height: 0 };
      } else {
        let rect = view.instance.boundingRect;
        size = { width: rect.width, height: rect.height };
      }
      return [PP_Bool.PP_TRUE, { rect: { point, size } }];
    },

    /**
     * float_t GetDeviceScale([in] PP_Resource resource);
     */
    PPB_View_GetDeviceScale: function(json) {
      let view = PP_Resource.lookup(json.resource);
      return view.instance.window.devicePixelRatio;
    },

    /**
     * float_t GetCSSScale([in] PP_Resource resource);
     */
    PPB_View_GetCSSScale: function(json) {
      let view = PP_Resource.lookup(json.resource);
      return 1;
    },

    /**
     * PP_Bool GetScrollOffset([in] PP_Resource resource,
     *                         [out] PP_Point offset);
     */
    PPB_View_GetScrollOffset: function(json) {
      let view = PP_Resource.lookup(json.resource);
      let position = view.instance.viewport.getScrollOffset();
      return [ PP_Bool.PP_TRUE, { offset: position }];
    },

    /**
     * float_t GetDeviceScale([in] PP_Resource resource);
     */
    PPB_View_Dev_GetDeviceScale: function(json) {
      let view = PP_Resource.lookup(json.resource);
      return view.instance.window.devicePixelRatio;
    },

    /**
     * PP_Bool IsWheelInputEvent([in] PP_Resource resource);
     */
    PPB_WheelInputEvent_IsWheelInputEvent: function(json) {
      let resource = PP_Resource.lookup(json.resource);
      return resource instanceof WheelInputEvent ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },

    /**
     * PP_FloatPoint GetTicks([in] PP_Resource wheel_event);
     */
    PPB_WheelInputEvent_GetTicks: function(json) {
      let event = PP_Resource.lookup(json.wheel_event);
      return { x: event.domEvent.deltaX, y: event.domEvent.deltaY };
    },

    /**
     * PP_Bool GetScrollByPage([in] PP_Resource wheel_event);
     */
    PPB_WheelInputEvent_GetScrollByPage: function(json) {
      let event = PP_Resource.lookup(json.wheel_event);
      return event.domEvent.deltaMode == event.domEvent.DOM_DELTA_PAGE
        ? PP_Bool.PP_TRUE : PP_Bool.PP_FALSE;
    },
  },
};

var EXPORTED_SYMBOLS = ["PPAPIRuntime"];
