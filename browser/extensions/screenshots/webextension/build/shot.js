this.shot = (function () {let exports={}; // Note: in this library we can't use any "system" dependencies because this can be used from multiple
// environments

/** Throws an error if the condition isn't true.  Any extra arguments after the condition
    are used as console.error() arguments. */
function assert(condition, ...args) {
  if (condition) {
    return;
  }
  console.error("Failed assertion", ...args);
  throw new Error(`Failed assertion: ${args.join(" ")}`);
}

/** True if `url` is a valid URL */
function isUrl(url) {
  // FIXME: this is rather naive, obviously
  if ((/^about:.{1,8000}$/i).test(url)) {
    return true;
  }
  if ((/^file:\/.{0,8000}$/i).test(url)) {
    return true;
  }
  if ((/^data:.*$/i).test(url)) {
    return true;
  }
  if ((/^chrome:.{0,8000}/i).test(url)) {
    return true;
  }
  if ((/^view-source:/i).test(url)) {
    return isUrl(url.substr("view-source:".length));
  }
  return (/^https?:\/\/[a-z0-9\.\-]{1,8000}[a-z0-9](:[0-9]{1,8000})?\/?/i).test(url);
}

function assertUrl(url) {
  if (!url) {
    throw new Error("Empty value is not URL");
  }
  if (!isUrl(url)) {
    let exc = new Error("Not a URL");
    exc.scheme = url.split(":")[0];
    throw exc;
  }
}

function assertOrigin(url) {
  assertUrl(url);
  if (url.search(/^https?:/i) != -1) {
    let match = (/^https?:\/\/[^/:]{1,4000}\/?$/i).exec(url);
    if (!match) {
      throw new Error("Bad origin, might include path");
    }
  }
}

function originFromUrl(url) {
  if (!url) {
    return null;
  }
  if (url.search(/^https?:/i) == -1) {
    // Non-HTTP URLs don't have an origin
    return null;
  }
  let match = (/^https?:\/\/[^/:]{1,4000}/i).exec(url);
  if (match) {
    return match[0];
  }
  return null;
}

/** Check if the given object has all of the required attributes, and no extra
    attributes exception those in optional */
function checkObject(obj, required, optional) {
  if (typeof obj != "object" || obj === null) {
    throw new Error("Cannot check non-object: " + (typeof obj) + " that is " + JSON.stringify(obj));
  }
  required = required || [];
  for (let attr of required) {
    if (!(attr in obj)) {
      return false;
    }
  }
  optional = optional || [];
  for (let attr in obj) {
    if (required.indexOf(attr) == -1 && optional.indexOf(attr) == -1) {
      return false;
    }
  }
  return true;
}

/** Create a JSON object from a normal object, given the required and optional
    attributes (filtering out any other attributes).  Optional attributes are
    only kept when they are truthy. */
function jsonify(obj, required, optional) {
  required = required || [];
  let result = {};
  for (let attr of required) {
    result[attr] = obj[attr];
  }
  optional = optional || [];
  for (let attr of optional) {
    if (obj[attr]) {
      result[attr] = obj[attr];
    }
  }
  return result;
}

/** Resolve url relative to base */
function resolveUrl(base, url) {
  // FIXME: totally ad hoc and probably incorrect, but we can't
  // use any libraries in this file
  if (url.search(/^https?:/) != -1) {
    // Absolute url
    return url;
  }
  if (url.indexOf("//") === 0) {
    // Protocol-relative URL
    return (/^https?:/i).exec(base)[0] + url;
  }
  if (url.indexOf("/") === 0) {
    // Domain-relative URL
    return (/^https?:\/\/[a-z0-9\.\-]{1,4000}/i).exec(base)[0] + url;
  }
  // Otherwise, a full relative URL
  while (url.indexOf("./") === 0) {
    url = url.substr(2);
  }
  if (!base) {
    // It's not an absolute URL, and we don't have a base URL, so we have
    // to throw away the URL
    return null;
  }
  let match = (/.*\//).exec(base)[0];
  if (match.search(/^https?:\/$/i) === 0) {
    // Domain without path
    match = match + "/";
  }
  return match + url;
}

/** True if the two objects look alike.  Null, undefined, and absent properties
    are all treated as equivalent.  Traverses objects and arrays */
function deepEqual(a, b) {
  if ((a === null || a === undefined) && (b === null || b === undefined)) {
    return true;
  }
  if (typeof a != "object" || typeof b != "object") {
    return a === b;
  }
  if (Array.isArray(a)) {
    if (!Array.isArray(b)) {
      return false;
    }
    if (a.length != b.length) {
      return false;
    }
    for (let i = 0; i < a.length; i++) {
      if (!deepEqual(a[i], b[i])) {
        return false;
      }
    }
  }
  if (Array.isArray(b)) {
    return false;
  }
  let seen = new Set();
  for (let attr of Object.keys(a)) {
    if (!deepEqual(a[attr], b[attr])) {
      return false;
    }
    seen.add(attr);
  }
  for (let attr of Object.keys(b)) {
    if (!seen.has(attr)) {
      if (!deepEqual(a[attr], b[attr])) {
        return false;
      }
    }
  }
  return true;
}

function makeRandomId() {
  // Note: this isn't for secure contexts, only for non-conflicting IDs
  let id = "";
  while (id.length < 12) {
    let num;
    if (!id) {
      num = Date.now() % Math.pow(36, 3);
    } else {
      num = Math.floor(Math.random() * Math.pow(36, 3));
    }
    id += num.toString(36);
  }
  return id;
}

class AbstractShot {

  constructor(backend, id, attrs) {
    attrs = attrs || {};
    assert((/^[a-zA-Z0-9]{1,4000}\/[a-z0-9\.-]{1,4000}$/).test(id), "Bad ID (should be alphanumeric):", JSON.stringify(id));
    this._backend = backend;
    this._id = id;
    this.origin = attrs.origin || null;
    this.fullUrl = attrs.fullUrl || null;
    if ((!attrs.fullUrl) && attrs.url) {
      console.warn("Received deprecated attribute .url");
      this.fullUrl = attrs.url;
    }
    this.docTitle = attrs.docTitle || null;
    this.userTitle = attrs.userTitle || null;
    this.createdDate = attrs.createdDate || Date.now();
    this.favicon = attrs.favicon || null;
    this.siteName = attrs.siteName || null;
    this.images = [];
    if (attrs.images) {
      this.images = attrs.images.map(
        (json) => new this.Image(json));
    }
    this.openGraph = attrs.openGraph || null;
    this.twitterCard = attrs.twitterCard || null;
    this.documentSize = attrs.documentSize || null;
    this.fullScreenThumbnail = attrs.fullScreenThumbnail || null;
    this.abTests = attrs.abTests || null;
    this._clips = {};
    if (attrs.clips) {
      for (let clipId in attrs.clips) {
        let clip = attrs.clips[clipId];
        this._clips[clipId] = new this.Clip(this, clipId, clip);
      }
    }

    for (let attr in attrs) {
      if (attr !== "clips" && attr !== "id" && this.REGULAR_ATTRS.indexOf(attr) === -1 && this.DEPRECATED_ATTRS.indexOf(attr) === -1) {
        throw new Error("Unexpected attribute: " + attr);
      } else if (attr === "id") {
        console.warn("passing id in attrs in AbstractShot constructor");
        console.trace();
        assert(attrs.id === this.id);
      }
    }
  }

  /** Update any and all attributes in the json object, with deep updating
      of `json.clips` */
  update(json) {
    let ALL_ATTRS = ["clips"].concat(this.REGULAR_ATTRS);
    assert(checkObject(json, [], ALL_ATTRS), "Bad attr to new Shot():", Object.keys(json));
    for (let attr in json) {
      if (attr == "clips") {
        continue;
      }
      if (typeof json[attr] == "object" && typeof this[attr] == "object" && this[attr] !== null) {
        let val = this[attr];
        if (val.asJson) {
          val = val.asJson();
        }
        if (!deepEqual(json[attr], val)) {
          this[attr] = json[attr];
        }
      } else if (json[attr] !== this[attr] &&
          (json[attr] || this[attr])) {
        this[attr] = json[attr];
      }
    }
    if (json.clips) {
      for (let clipId in json.clips) {
        if (!json.clips[clipId]) {
          this.delClip(clipId);
        } else if (!this.getClip(clipId)) {
          this.setClip(clipId, json.clips[clipId]);
        } else if (!deepEqual(this.getClip(clipId).asJson(), json.clips[clipId])) {
          this.setClip(clipId, json.clips[clipId]);
        }
      }
    }

  }

  /** Returns a JSON version of this shot */
  asJson() {
    let result = {};
    for (let attr of this.REGULAR_ATTRS) {
      var val = this[attr];
      if (val && val.asJson) {
        val = val.asJson();
      }
      result[attr] = val;
    }
    result.clips = {};
    for (let attr in this._clips) {
      result.clips[attr] = this._clips[attr].asJson();
    }
    return result;
  }

  /** A more minimal JSON representation for creating indexes of shots */
  asRecallJson() {
    let result = {clips: {}};
    for (let attr of this.RECALL_ATTRS) {
      var val = this[attr];
      if (val && val.asJson) {
        val = val.asJson();
      }
      result[attr] = val;
    }
    for (let name of this.clipNames()) {
      result.clips[name] = this.getClip(name).asJson();
    }
    return result;
  }

  get backend() {
    return this._backend;
  }

  get id() {
    return this._id;
  }

  get url() {
    return this.fullUrl || this.origin;
  }
  set url(val) {
    throw new Error(".url is read-only");
  }

  get fullUrl() {
    return this._fullUrl;
  }
  set fullUrl(val) {
    if (val) {
      assertUrl(val);
    }
    this._fullUrl = val || undefined;
  }

  get origin() {
    return this._origin;
  }
  set origin(val) {
    if (val) {
      assertOrigin(val);
    }
    this._origin = val || undefined;
  }

  get filename() {
    let filenameTitle = this.title;
    let date = new Date(this.createdDate);
    filenameTitle = filenameTitle.replace(/[:\\<>\/!@&*.|\n\r\t]/g, " ");
    filenameTitle = filenameTitle.replace(/\s{1,4000}/g, " ");
    let clipFilename = `Screenshot-${date.getFullYear()}-${date.getMonth() + 1}-${date.getDate()} ${filenameTitle}`;
    const clipFilenameBytesSize = clipFilename.length * 2; // JS STrings are UTF-16
    if (clipFilenameBytesSize > 251) { // 255 bytes (Usual filesystems max) - 4 for the ".png" file extension string
      const excedingchars = (clipFilenameBytesSize - 246) / 2; // 251 - 5 for ellipsis "[...]"
      clipFilename = clipFilename.substring(0, clipFilename.length - excedingchars);
      clipFilename = clipFilename + '[...]';
    }
    return clipFilename + '.png';
  }

  get urlDisplay() {
    if (!this.url) {
      return null;
    }
    if (this.url.search(/^https?/i) != -1) {
      let txt = this.url;
      txt = txt.replace(/^[a-z]{1,4000}:\/\//i, "");
      txt = txt.replace(/\/.{0,4000}/, "");
      txt = txt.replace(/^www\./i, "");
      return txt;
    } else if (this.url.startsWith("data:")) {
      return "data:url";
    }
    let txt = this.url;
    txt = txt.replace(/\?.{0,4000}/, "");
    return txt;
  }

  get viewUrl() {
    let url = this.backend + "/" + this.id;
    return url;
  }

  get creatingUrl() {
    let url = `${this.backend}/creating/${this.id}`;
    url += `?title=${encodeURIComponent(this.title || "")}`;
    url += `&url=${encodeURIComponent(this.url)}`;
    return url;
  }

  get jsonUrl() {
    return this.backend + "/data/" + this.id;
  }

  get oembedUrl() {
    return this.backend + "/oembed?url=" + encodeURIComponent(this.viewUrl);
  }

  get docTitle() {
    return this._title;
  }
  set docTitle(val) {
    assert(val === null || typeof val == "string", "Bad docTitle:", val);
    this._title = val;
  }

  get openGraph() {
    return this._openGraph || null;
  }
  set openGraph(val) {
    assert(val === null || typeof val == "object", "Bad openGraph:", val);
    if (val) {
      assert(checkObject(val, [], this._OPENGRAPH_PROPERTIES), "Bad attr to openGraph:", Object.keys(val));
      this._openGraph = val;
    } else {
      this._openGraph = null;
    }
  }

  get twitterCard() {
    return this._twitterCard || null;
  }
  set twitterCard(val) {
    assert(val === null || typeof val == "object", "Bad twitterCard:", val);
    if (val) {
      assert(checkObject(val, [], this._TWITTERCARD_PROPERTIES), "Bad attr to twitterCard:", Object.keys(val));
      this._twitterCard = val;
    } else {
      this._twitterCard = null;
    }
  }

  get userTitle() {
    return this._userTitle;
  }
  set userTitle(val) {
    assert(val === null || typeof val == "string", "Bad userTitle:", val);
    this._userTitle = val;
  }

  get title() {
    // FIXME: we shouldn't support both openGraph.title and ogTitle
    let ogTitle = this.openGraph && this.openGraph.title;
    let twitterTitle = this.twitterCard && this.twitterCard.title;
    let title = this.userTitle || ogTitle || twitterTitle || this.docTitle || this.url;
    if (Array.isArray(title)) {
      title = title[0];
    }
    if (!title) {
      title = "Screenshot";
    }
    return title;
  }

  get createdDate() {
    return this._createdDate;
  }
  set createdDate(val) {
    assert(val === null || typeof val == "number", "Bad createdDate:", val);
    this._createdDate = val;
  }

  get favicon() {
    return this._favicon;
  }
  set favicon(val) {
    // We allow but ignore bad favicon URLs, as they seem somewhat common
    val = val || null;
    if (!isUrl(val)) {
      val = null;
    }
    if (val) {
      val = resolveUrl(this.url, val);
    }
    this._favicon = val;
  }

  clipNames() {
    let names = Object.getOwnPropertyNames(this._clips);
    names.sort(function(a, b) {
      return a.sortOrder < b.sortOrder ? 1 : 0;
    });
    return names;
  }
  getClip(name) {
    return this._clips[name];
  }
  addClip(val) {
    let name = makeRandomId();
    this.setClip(name, val);
    return name;
  }
  setClip(name, val) {
    let clip = new this.Clip(this, name, val);
    this._clips[name] = clip;
  }
  delClip(name) {
    if (!this._clips[name]) {
      throw new Error("No existing clip with id: " + name);
    }
    delete this._clips[name];
  }
  delAllClips() {
    this._clips = {};
  }
  biggestClipSortOrder() {
    let biggest = 0;
    for (let clipId in this._clips) {
      biggest = Math.max(biggest, this._clips[clipId].sortOrder);
    }
    return biggest;
  }
  updateClipUrl(clipId, clipUrl) {
    let clip = this.getClip(clipId);
    if ( clip && clip.image ) {
      clip.image.url = clipUrl;
    } else {
      console.warn("Tried to update the url of a clip with no image:", clip);
    }
  }

  get siteName() {
    return this._siteName || null;
  }
  set siteName(val) {
    assert(typeof val == "string" || !val);
    this._siteName = val;
  }

  get documentSize() {
    return this._documentSize;
  }
  set documentSize(val) {
    assert(typeof val == "object" || !val);
    if (val) {
      assert(checkObject(val, ["height", "width"], "Bad attr to documentSize:", Object.keys(val)));
      assert(typeof val.height == "number");
      assert(typeof val.width == "number");
      this._documentSize = val;
    } else {
      this._documentSize = null;
    }
  }

  get fullScreenThumbnail() {
    return this._fullScreenThumbnail;
  }
  set fullScreenThumbnail(val) {
    assert(typeof val == "string" || !val);
    if (val) {
      assert(isUrl(val));
      this._fullScreenThumbnail = val;
    } else {
      this._fullScreenThumbnail = null;
    }
  }

  get abTests() {
    return this._abTests;
  }
  set abTests(val) {
    if (val === null || val === undefined) {
      this._abTests = null;
      return;
    }
    assert(typeof val == "object", "abTests should be an object, not:", typeof val);
    assert(!Array.isArray(val), "abTests should not be an Array");
    for (let name in val) {
      assert(val[name] && typeof val[name] == "string", `abTests.${name} should be a string:`, typeof val[name]);
    }
    this._abTests = val;
  }

}

AbstractShot.prototype.REGULAR_ATTRS = (`
origin fullUrl docTitle userTitle createdDate favicon images
siteName openGraph twitterCard documentSize
fullScreenThumbnail abTests
`).split(/\s+/g);

// Attributes that will be accepted in the constructor, but ignored/dropped
AbstractShot.prototype.DEPRECATED_ATTRS = (`
microdata history ogTitle createdDevice head body htmlAttrs bodyAttrs headAttrs
readable hashtags comments showPage isPublic resources deviceId url
`).split(/\s+/g);

AbstractShot.prototype.RECALL_ATTRS = (`
url docTitle userTitle createdDate favicon
openGraph twitterCard images fullScreenThumbnail
`).split(/\s+/g);

AbstractShot.prototype._OPENGRAPH_PROPERTIES = (`
title type url image audio description determiner locale site_name video
image:secure_url image:type image:width image:height
video:secure_url video:type video:width image:height
audio:secure_url audio:type
article:published_time article:modified_time article:expiration_time article:author article:section article:tag
book:author book:isbn book:release_date book:tag
profile:first_name profile:last_name profile:username profile:gender
`).split(/\s+/g);

AbstractShot.prototype._TWITTERCARD_PROPERTIES = (`
card site title description image
player player:width player:height player:stream player:stream:content_type
`).split(/\s+/g);

/** Represents one found image in the document (not a clip) */
class _Image {
  // FIXME: either we have to notify the shot of updates, or make
  // this read-only
  constructor(json) {
    assert(typeof json === "object", "Clip Image given a non-object", json);
    assert(checkObject(json, ["url"], ["dimensions", "title", "alt"]), "Bad attrs for Image:", Object.keys(json));
    assert(isUrl(json.url), "Bad Image url:", json.url);
    this.url = json.url;
    assert((!json.dimensions) ||
           (typeof json.dimensions.x == "number" && typeof json.dimensions.y == "number"),
           "Bad Image dimensions:", json.dimensions);
    this.dimensions = json.dimensions;
    assert(typeof json.title == "string" || !json.title, "Bad Image title:", json.title);
    this.title = json.title;
    assert(typeof json.alt == "string" || !json.alt, "Bad Image alt:", json.alt);
    this.alt = json.alt;
  }

  asJson() {
    return jsonify(this, ["url"], ["dimensions"]);
  }
}

AbstractShot.prototype.Image = _Image;

/** Represents a clip, either a text or image clip */
class _Clip {
  constructor(shot, id, json) {
    this._shot = shot;
    assert(checkObject(json, ["createdDate", "image"], ["sortOrder"]), "Bad attrs for Clip:", Object.keys(json));
    assert(typeof id == "string" && id, "Bad Clip id:", id);
    this._id = id;
    this.createdDate = json.createdDate;
    if ('sortOrder' in json) {
      assert(typeof json.sortOrder == "number" || !json.sortOrder, "Bad Clip sortOrder:", json.sortOrder);
    }
    if ('sortOrder' in json) {
      this.sortOrder = json.sortOrder;
    } else {
      let biggestOrder = shot.biggestClipSortOrder();
      this.sortOrder = biggestOrder + 100;
    }
    this.image = json.image;
  }

  toString() {
    return `[Shot Clip id=${this.id} sortOrder=${this.sortOrder} image ${this.image.dimensions.x}x${this.image.dimensions.y}]`;
  }

  asJson() {
    return jsonify(this, ["createdDate"], ["sortOrder", "image"]);
  }

  get id() {
    return this._id;
  }

  get createdDate() {
    return this._createdDate;
  }
  set createdDate(val) {
    assert(typeof val == "number" || !val, "Bad Clip createdDate:", val);
    this._createdDate = val;
  }

  get image() {
    return this._image;
  }
  set image(image) {
    if (!image) {
      this._image = undefined;
      return;
    }
    assert(checkObject(image, ["url"], ["dimensions", "text", "location", "captureType"]), "Bad attrs for Clip Image:", Object.keys(image));
    assert(isUrl(image.url), "Bad Clip image URL:", image.url);
    assert(image.captureType == "madeSelection" || image.captureType == "selection" || image.captureType == "visible" || image.captureType == "auto" || image.captureType == "fullPage" || !image.captureType, "Bad image.captureType:", image.captureType);
    assert(typeof image.text == "string" || !image.text, "Bad Clip image text:", image.text);
    if (image.dimensions) {
      assert(typeof image.dimensions.x == "number" && typeof image.dimensions.y == "number", "Bad Clip image dimensions:", image.dimensions);
    }
    assert(image.location &&
      typeof image.location.left == "number" &&
      typeof image.location.right == "number" &&
      typeof image.location.top == "number" &&
      typeof image.location.bottom == "number", "Bad Clip image pixel location:", image.location);
    if (image.location.topLeftElement || image.location.topLeftOffset ||
        image.location.bottomRightElement || image.location.bottomRightOffset) {
      assert(typeof image.location.topLeftElement == "string" &&
        image.location.topLeftOffset &&
        typeof image.location.topLeftOffset.x == "number" &&
        typeof image.location.topLeftOffset.y == "number" &&
        typeof image.location.bottomRightElement == "string" &&
        image.location.bottomRightOffset &&
        typeof image.location.bottomRightOffset.x == "number" &&
        typeof image.location.bottomRightOffset.y == "number",
        "Bad Clip image element location:", image.location);
    }
    this._image = image;
  }

  isDataUrl() {
    if (this.image) {
      return this.image.url.startsWith("data:");
    }
    return false;
  }

  get sortOrder() {
    return this._sortOrder || null;
  }
  set sortOrder(val) {
    assert(typeof val == "number" || !val, "Bad Clip sortOrder:", val);
    this._sortOrder = val;
  }

}

AbstractShot.prototype.Clip = _Clip;

if (typeof exports != "undefined") {
  exports.AbstractShot = AbstractShot;
  exports.originFromUrl = originFromUrl;
}

return exports;
})();
null;

