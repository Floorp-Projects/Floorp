var goog$global = this, goog$isString = function(val) {
  return typeof val == "string"
};
Math.floor(Math.random() * 2147483648).toString(36);
var goog$now = Date.now || function() {
  return(new Date).getTime()
}, goog$inherits = function(childCtor, parentCtor) {
  function tempCtor() {
  }
  tempCtor.prototype = parentCtor.prototype;
  childCtor.superClass_ = parentCtor.prototype;
  childCtor.prototype = new tempCtor
};var goog$array$peek = function(array) {
  return array[array.length - 1]
}, goog$array$indexOf = function(arr, obj, opt_fromIndex) {
  if(arr.indexOf)return arr.indexOf(obj, opt_fromIndex);
  if(Array.indexOf)return Array.indexOf(arr, obj, opt_fromIndex);
  for(var fromIndex = opt_fromIndex == null ? 0 : opt_fromIndex < 0 ? Math.max(0, arr.length + opt_fromIndex) : opt_fromIndex, i = fromIndex;i < arr.length;i++)if(i in arr && arr[i] === obj)return i;
  return-1
}, goog$array$map = function(arr, f, opt_obj) {
  if(arr.map)return arr.map(f, opt_obj);
  if(Array.map)return Array.map(arr, f, opt_obj);
  for(var l = arr.length, res = [], resLength = 0, arr2 = goog$isString(arr) ? arr.split("") : arr, i = 0;i < l;i++)if(i in arr2)res[resLength++] = f.call(opt_obj, arr2[i], i, arr);
  return res
}, goog$array$some = function(arr, f, opt_obj) {
  if(arr.some)return arr.some(f, opt_obj);
  if(Array.some)return Array.some(arr, f, opt_obj);
  for(var l = arr.length, arr2 = goog$isString(arr) ? arr.split("") : arr, i = 0;i < l;i++)if(i in arr2 && f.call(opt_obj, arr2[i], i, arr))return true;
  return false
}, goog$array$every = function(arr, f, opt_obj) {
  if(arr.every)return arr.every(f, opt_obj);
  if(Array.every)return Array.every(arr, f, opt_obj);
  for(var l = arr.length, arr2 = goog$isString(arr) ? arr.split("") : arr, i = 0;i < l;i++)if(i in arr2 && !f.call(opt_obj, arr2[i], i, arr))return false;
  return true
}, goog$array$find = function(arr, f, opt_obj) {
  var i;
  JSCompiler_inline_label_goog$array$findIndex_12: {
    for(var JSCompiler_inline_l = arr.length, JSCompiler_inline_arr2 = goog$isString(arr) ? arr.split("") : arr, JSCompiler_inline_i = 0;JSCompiler_inline_i < JSCompiler_inline_l;JSCompiler_inline_i++)if(JSCompiler_inline_i in JSCompiler_inline_arr2 && f.call(opt_obj, JSCompiler_inline_arr2[JSCompiler_inline_i], JSCompiler_inline_i, arr)) {
      i = JSCompiler_inline_i;
      break JSCompiler_inline_label_goog$array$findIndex_12
    }i = -1
  }return i < 0 ? null : goog$isString(arr) ? arr.charAt(i) : arr[i]
};var goog$string$trim = function(str) {
  return str.replace(/^[\s\xa0]+|[\s\xa0]+$/g, "")
}, goog$string$htmlEscape = function(str, opt_isLikelyToContainHtmlChars) {
  if(opt_isLikelyToContainHtmlChars)return str.replace(goog$string$amperRe_, "&amp;").replace(goog$string$ltRe_, "&lt;").replace(goog$string$gtRe_, "&gt;").replace(goog$string$quotRe_, "&quot;");
  else {
    if(!goog$string$allRe_.test(str))return str;
    if(str.indexOf("&") != -1)str = str.replace(goog$string$amperRe_, "&amp;");
    if(str.indexOf("<") != -1)str = str.replace(goog$string$ltRe_, "&lt;");
    if(str.indexOf(">") != -1)str = str.replace(goog$string$gtRe_, "&gt;");
    if(str.indexOf('"') != -1)str = str.replace(goog$string$quotRe_, "&quot;");
    return str
  }
}, goog$string$amperRe_ = /&/g, goog$string$ltRe_ = /</g, goog$string$gtRe_ = />/g, goog$string$quotRe_ = /\"/g, goog$string$allRe_ = /[&<>\"]/, goog$string$contains = function(s, ss) {
  return s.indexOf(ss) != -1
}, goog$string$compareVersions = function(version1, version2) {
  for(var order = 0, v1Subs = goog$string$trim(String(version1)).split("."), v2Subs = goog$string$trim(String(version2)).split("."), subCount = Math.max(v1Subs.length, v2Subs.length), subIdx = 0;order == 0 && subIdx < subCount;subIdx++) {
    var v1Sub = v1Subs[subIdx] || "", v2Sub = v2Subs[subIdx] || "", v1CompParser = new RegExp("(\\d*)(\\D*)", "g"), v2CompParser = new RegExp("(\\d*)(\\D*)", "g");
    do {
      var v1Comp = v1CompParser.exec(v1Sub) || ["", "", ""], v2Comp = v2CompParser.exec(v2Sub) || ["", "", ""];
      if(v1Comp[0].length == 0 && v2Comp[0].length == 0)break;
      var v1CompNum = v1Comp[1].length == 0 ? 0 : parseInt(v1Comp[1], 10), v2CompNum = v2Comp[1].length == 0 ? 0 : parseInt(v2Comp[1], 10);
      order = goog$string$compareElements_(v1CompNum, v2CompNum) || goog$string$compareElements_(v1Comp[2].length == 0, v2Comp[2].length == 0) || goog$string$compareElements_(v1Comp[2], v2Comp[2])
    }while(order == 0)
  }return order
}, goog$string$compareElements_ = function(left, right) {
  if(left < right)return-1;
  else if(left > right)return 1;
  return 0
};
goog$now();var goog$userAgent$detectedOpera_, goog$userAgent$detectedIe_, goog$userAgent$detectedWebkit_, goog$userAgent$detectedMobile_, goog$userAgent$detectedGecko_, goog$userAgent$detectedCamino_, goog$userAgent$detectedMac_, goog$userAgent$detectedWindows_, goog$userAgent$detectedLinux_, goog$userAgent$detectedX11_, goog$userAgent$getUserAgentString = function() {
  return goog$global.navigator ? goog$global.navigator.userAgent : null
}, goog$userAgent$getNavigator = function() {
  return goog$global.navigator
};
goog$userAgent$detectedCamino_ = goog$userAgent$detectedGecko_ = goog$userAgent$detectedMobile_ = goog$userAgent$detectedWebkit_ = goog$userAgent$detectedIe_ = goog$userAgent$detectedOpera_ = false;
var JSCompiler_inline_ua_15;
if(JSCompiler_inline_ua_15 = goog$userAgent$getUserAgentString()) {
  var JSCompiler_inline_navigator$$1_16 = goog$userAgent$getNavigator();
  goog$userAgent$detectedOpera_ = JSCompiler_inline_ua_15.indexOf("Opera") == 0;
  goog$userAgent$detectedIe_ = !goog$userAgent$detectedOpera_ && JSCompiler_inline_ua_15.indexOf("MSIE") != -1;
  goog$userAgent$detectedMobile_ = (goog$userAgent$detectedWebkit_ = !goog$userAgent$detectedOpera_ && JSCompiler_inline_ua_15.indexOf("WebKit") != -1) && JSCompiler_inline_ua_15.indexOf("Mobile") != -1;
  goog$userAgent$detectedCamino_ = (goog$userAgent$detectedGecko_ = !goog$userAgent$detectedOpera_ && !goog$userAgent$detectedWebkit_ && JSCompiler_inline_navigator$$1_16.product == "Gecko") && JSCompiler_inline_navigator$$1_16.vendor == "Camino"
}var goog$userAgent$OPERA = goog$userAgent$detectedOpera_, goog$userAgent$IE = goog$userAgent$detectedIe_, goog$userAgent$GECKO = goog$userAgent$detectedGecko_, goog$userAgent$WEBKIT = goog$userAgent$detectedWebkit_, goog$userAgent$MOBILE = goog$userAgent$detectedMobile_, goog$userAgent$PLATFORM, JSCompiler_inline_navigator$$2_19 = goog$userAgent$getNavigator();
goog$userAgent$PLATFORM = JSCompiler_inline_navigator$$2_19 && JSCompiler_inline_navigator$$2_19.platform || "";
goog$userAgent$detectedMac_ = goog$string$contains(goog$userAgent$PLATFORM, "Mac");
goog$userAgent$detectedWindows_ = goog$string$contains(goog$userAgent$PLATFORM, "Win");
goog$userAgent$detectedLinux_ = goog$string$contains(goog$userAgent$PLATFORM, "Linux");
goog$userAgent$detectedX11_ = !!goog$userAgent$getNavigator() && goog$string$contains(goog$userAgent$getNavigator().appVersion || "", "X11");
var goog$userAgent$VERSION, JSCompiler_inline_version$$6_26 = "", JSCompiler_inline_re$$2_27;
if(goog$userAgent$OPERA && goog$global.opera) {
  var JSCompiler_inline_operaVersion_28 = goog$global.opera.version;
  JSCompiler_inline_version$$6_26 = typeof JSCompiler_inline_operaVersion_28 == "function" ? JSCompiler_inline_operaVersion_28() : JSCompiler_inline_operaVersion_28
}else {
  if(goog$userAgent$GECKO)JSCompiler_inline_re$$2_27 = /rv\:([^\);]+)(\)|;)/;
  else if(goog$userAgent$IE)JSCompiler_inline_re$$2_27 = /MSIE\s+([^\);]+)(\)|;)/;
  else if(goog$userAgent$WEBKIT)JSCompiler_inline_re$$2_27 = /WebKit\/(\S+)/;
  if(JSCompiler_inline_re$$2_27) {
    var JSCompiler_inline_arr$$41_29 = JSCompiler_inline_re$$2_27.exec(goog$userAgent$getUserAgentString());
    JSCompiler_inline_version$$6_26 = JSCompiler_inline_arr$$41_29 ? JSCompiler_inline_arr$$41_29[1] : ""
  }
}goog$userAgent$VERSION = JSCompiler_inline_version$$6_26;
var goog$userAgent$isVersionCache_ = {}, goog$userAgent$isVersion = function(version) {
  return goog$userAgent$isVersionCache_[version] || (goog$userAgent$isVersionCache_[version] = goog$string$compareVersions(goog$userAgent$VERSION, version) >= 0)
};var goog$dom$getWindow = function(opt_doc) {
  return opt_doc ? goog$dom$getWindow_(opt_doc) : window
}, goog$dom$getWindow_ = function(doc) {
  if(doc.parentWindow)return doc.parentWindow;
  if(goog$userAgent$WEBKIT && !goog$userAgent$isVersion("500") && !goog$userAgent$MOBILE) {
    var scriptElement = doc.createElement("script");
    scriptElement.innerHTML = "document.parentWindow=window";
    var parentElement = doc.documentElement;
    parentElement.appendChild(scriptElement);
    parentElement.removeChild(scriptElement);
    return doc.parentWindow
  }return doc.defaultView
}, goog$dom$appendChild = function(parent, child) {
  parent.appendChild(child)
}, goog$dom$BAD_CONTAINS_WEBKIT_ = goog$userAgent$WEBKIT && goog$userAgent$isVersion("522"), goog$dom$contains = function(parent, descendant) {
  if(typeof parent.contains != "undefined" && !goog$dom$BAD_CONTAINS_WEBKIT_ && descendant.nodeType == 1)return parent == descendant || parent.contains(descendant);
  if(typeof parent.compareDocumentPosition != "undefined")return parent == descendant || Boolean(parent.compareDocumentPosition(descendant) & 16);
  for(;descendant && parent != descendant;)descendant = descendant.parentNode;
  return descendant == parent
}, goog$dom$compareNodeOrder = function(node1, node2) {
  if(node1 == node2)return 0;
  if(node1.compareDocumentPosition)return node1.compareDocumentPosition(node2) & 2 ? 1 : -1;
  if("sourceIndex" in node1 || node1.parentNode && "sourceIndex" in node1.parentNode) {
    var isElement1 = node1.nodeType == 1, isElement2 = node2.nodeType == 1;
    if(isElement1 && isElement2)return node1.sourceIndex - node2.sourceIndex;
    else {
      var parent1 = node1.parentNode, parent2 = node2.parentNode;
      if(parent1 == parent2)return goog$dom$compareSiblingOrder_(node1, node2);
      if(!isElement1 && goog$dom$contains(parent1, node2))return-1 * goog$dom$compareParentsDescendantNodeIe_(node1, node2);
      if(!isElement2 && goog$dom$contains(parent2, node1))return goog$dom$compareParentsDescendantNodeIe_(node2, node1);
      return(isElement1 ? node1.sourceIndex : parent1.sourceIndex) - (isElement2 ? node2.sourceIndex : parent2.sourceIndex)
    }
  }var doc = goog$dom$getOwnerDocument(node1), range1, range2;
  range1 = doc.createRange();
  range1.selectNode(node1);
  range1.collapse(true);
  range2 = doc.createRange();
  range2.selectNode(node2);
  range2.collapse(true);
  return range1.compareBoundaryPoints(goog$global.Range.START_TO_END, range2)
}, goog$dom$compareParentsDescendantNodeIe_ = function(textNode, node) {
  var parent = textNode.parentNode;
  if(parent == node)return-1;
  for(var sibling = node;sibling.parentNode != parent;)sibling = sibling.parentNode;
  return goog$dom$compareSiblingOrder_(sibling, textNode)
}, goog$dom$compareSiblingOrder_ = function(node1, node2) {
  for(var s = node2;s = s.previousSibling;)if(s == node1)return-1;
  return 1
}, goog$dom$findCommonAncestor = function() {
  var i, count = arguments.length;
  if(count) {
    if(count == 1)return arguments[0]
  }else return null;
  var paths = [], minLength = Infinity;
  for(i = 0;i < count;i++) {
    for(var ancestors = [], node = arguments[i];node;) {
      ancestors.unshift(node);
      node = node.parentNode
    }paths.push(ancestors);
    minLength = Math.min(minLength, ancestors.length)
  }var output = null;
  for(i = 0;i < minLength;i++) {
    for(var first = paths[0][i], j = 1;j < count;j++)if(first != paths[j][i])return output;
    output = first
  }return output
}, goog$dom$getOwnerDocument = function(node) {
  // Added 'editorDoc' as hack for browsers that don't support node.ownerDocument
  return node.nodeType == 9 ? node : node.ownerDocument || node.document || editorDoc
}, goog$dom$DomHelper = function(opt_document) {
  this.document_ = opt_document || goog$global.document || document
};
goog$dom$DomHelper.prototype.getDocument = function() {
  return this.document_
};
goog$dom$DomHelper.prototype.createElement = function(name) {
  return this.document_.createElement(name)
};
goog$dom$DomHelper.prototype.getWindow = function() {
  return goog$dom$getWindow_(this.document_)
};
goog$dom$DomHelper.prototype.appendChild = goog$dom$appendChild;
goog$dom$DomHelper.prototype.contains = goog$dom$contains;var goog$Disposable = function() {
};if("StopIteration" in goog$global)var goog$iter$StopIteration = goog$global.StopIteration;
else goog$iter$StopIteration = Error("StopIteration");
var goog$iter$Iterator = function() {
};
goog$iter$Iterator.prototype.next = function() {
  throw goog$iter$StopIteration;
};
goog$iter$Iterator.prototype.__iterator__ = function() {
  return this
};var goog$debug$exposeException = function(err, opt_fn) {
  try {
    var e, JSCompiler_inline_href_34;
    JSCompiler_inline_label_goog$getObjectByName_61: {
      for(var JSCompiler_inline_parts = "window.location.href".split("."), JSCompiler_inline_cur = goog$global, JSCompiler_inline_part;JSCompiler_inline_part = JSCompiler_inline_parts.shift();)if(JSCompiler_inline_cur[JSCompiler_inline_part])JSCompiler_inline_cur = JSCompiler_inline_cur[JSCompiler_inline_part];
      else {
        JSCompiler_inline_href_34 = null;
        break JSCompiler_inline_label_goog$getObjectByName_61
      }JSCompiler_inline_href_34 = JSCompiler_inline_cur
    }e = typeof err == "string" ? {message:err, name:"Unknown error", lineNumber:"Not available", fileName:JSCompiler_inline_href_34, stack:"Not available"} : !err.lineNumber || !err.fileName || !err.stack ? {message:err.message, name:err.name, lineNumber:err.lineNumber || err.line || "Not available", fileName:err.fileName || err.filename || err.sourceURL || JSCompiler_inline_href_34, stack:err.stack || "Not available"} : err;
    var error = "Message: " + goog$string$htmlEscape(e.message) + '\nUrl: <a href="view-source:' + e.fileName + '" target="_new">' + e.fileName + "</a>\nLine: " + e.lineNumber + "\n\nBrowser stack:\n" + goog$string$htmlEscape(e.stack + "-> ") + "[end]\n\nJS stack traversal:\n" + goog$string$htmlEscape(goog$debug$getStacktrace(opt_fn) + "-> ");
    return error
  }catch(e2) {
    return"Exception trying to expose exception! You win, we lose. " + e2
  }
}, goog$debug$getStacktrace = function(opt_fn) {
  return goog$debug$getStacktraceHelper_(opt_fn || arguments.callee.caller, [])
}, goog$debug$getStacktraceHelper_ = function(fn, visited) {
  var sb = [], JSCompiler_inline_result_36;
  JSCompiler_inline_label_goog$array$contains_41:JSCompiler_inline_result_36 = visited.contains ? visited.contains(fn) : goog$array$indexOf(visited, fn) > -1;
  if(JSCompiler_inline_result_36)sb.push("[...circular reference...]");
  else if(fn && visited.length < 50) {
    sb.push(goog$debug$getFunctionName(fn) + "(");
    for(var args = fn.arguments, i = 0;i < args.length;i++) {
      i > 0 && sb.push(", ");
      var argDesc, arg = args[i];
      switch(typeof arg) {
        case "object":
          argDesc = arg ? "object" : "null";
          break;
        case "string":
          argDesc = arg;
          break;
        case "number":
          argDesc = String(arg);
          break;
        case "boolean":
          argDesc = arg ? "true" : "false";
          break;
        case "function":
          argDesc = (argDesc = goog$debug$getFunctionName(arg)) ? argDesc : "[fn]";
          break;
        case "undefined":
        ;
        default:
          argDesc = typeof arg;
          break
      }
      if(argDesc.length > 40)argDesc = argDesc.substr(0, 40) + "...";
      sb.push(argDesc)
    }visited.push(fn);
    sb.push(")\n");
    try {
      sb.push(goog$debug$getStacktraceHelper_(fn.caller, visited))
    }catch(e) {
      sb.push("[exception trying to get caller]\n")
    }
  }else fn ? sb.push("[...long stack...]") : sb.push("[end]");
  return sb.join("")
}, goog$debug$getFunctionName = function(fn) {
  var functionSource = String(fn);
  if(!goog$debug$fnNameCache_[functionSource]) {
    var matches = /function ([^\(]+)/.exec(functionSource);
    if(matches) {
      var method = matches[1];
      goog$debug$fnNameCache_[functionSource] = method
    }else goog$debug$fnNameCache_[functionSource] = "[Anonymous]"
  }return goog$debug$fnNameCache_[functionSource]
}, goog$debug$fnNameCache_ = {};var goog$debug$LogRecord = function(level, msg, loggerName, opt_time, opt_sequenceNumber) {
  this.sequenceNumber_ = typeof opt_sequenceNumber == "number" ? opt_sequenceNumber : goog$debug$LogRecord$nextSequenceNumber_++;
  this.time_ = opt_time || goog$now();
  this.level_ = level;
  this.msg_ = msg;
  this.loggerName_ = loggerName
};
goog$debug$LogRecord.prototype.exception_ = null;
goog$debug$LogRecord.prototype.exceptionText_ = null;
var goog$debug$LogRecord$nextSequenceNumber_ = 0;
goog$debug$LogRecord.prototype.setException = function(exception) {
  this.exception_ = exception
};
goog$debug$LogRecord.prototype.setExceptionText = function(text) {
  this.exceptionText_ = text
};
goog$debug$LogRecord.prototype.setLevel = function(level) {
  this.level_ = level
};var goog$debug$Logger = function(name) {
  this.name_ = name;
  this.parent_ = null;
  this.children_ = {};
  this.handlers_ = []
};
goog$debug$Logger.prototype.level_ = null;
var goog$debug$Logger$Level = function(name, value) {
  this.name = name;
  this.value = value
};
goog$debug$Logger$Level.prototype.toString = function() {
  return this.name
};
new goog$debug$Logger$Level("OFF", Infinity);
new goog$debug$Logger$Level("SHOUT", 1200);
var goog$debug$Logger$Level$SEVERE = new goog$debug$Logger$Level("SEVERE", 1000), goog$debug$Logger$Level$WARNING = new goog$debug$Logger$Level("WARNING", 900);
new goog$debug$Logger$Level("INFO", 800);
var goog$debug$Logger$Level$CONFIG = new goog$debug$Logger$Level("CONFIG", 700);
new goog$debug$Logger$Level("FINE", 500);
new goog$debug$Logger$Level("FINER", 400);
new goog$debug$Logger$Level("FINEST", 300);
new goog$debug$Logger$Level("ALL", 0);
goog$debug$Logger.prototype.setLevel = function(level) {
  this.level_ = level
};
goog$debug$Logger.prototype.isLoggable = function(level) {
  if(this.level_)return level.value >= this.level_.value;
  if(this.parent_)return this.parent_.isLoggable(level);
  return false
};
goog$debug$Logger.prototype.log = function(level, msg, opt_exception) {
  this.isLoggable(level) && this.logRecord(this.getLogRecord(level, msg, opt_exception))
};
goog$debug$Logger.prototype.getLogRecord = function(level, msg, opt_exception) {
  var logRecord = new goog$debug$LogRecord(level, String(msg), this.name_);
  if(opt_exception) {
    logRecord.setException(opt_exception);
    logRecord.setExceptionText(goog$debug$exposeException(opt_exception, arguments.callee.caller))
  }return logRecord
};
goog$debug$Logger.prototype.severe = function(msg, opt_exception) {
  this.log(goog$debug$Logger$Level$SEVERE, msg, opt_exception)
};
goog$debug$Logger.prototype.warning = function(msg, opt_exception) {
  this.log(goog$debug$Logger$Level$WARNING, msg, opt_exception)
};
goog$debug$Logger.prototype.logRecord = function(logRecord) {
  if(this.isLoggable(logRecord.level_))for(var target = this;target;) {
    target.callPublish_(logRecord);
    target = target.parent_
  }
};
goog$debug$Logger.prototype.callPublish_ = function(logRecord) {
  for(var i = 0;i < this.handlers_.length;i++)this.handlers_[i](logRecord)
};
goog$debug$Logger.prototype.setParent_ = function(parent) {
  this.parent_ = parent
};
goog$debug$Logger.prototype.addChild_ = function(name, logger) {
  this.children_[name] = logger
};
var goog$debug$LogManager$loggers_ = {}, goog$debug$LogManager$rootLogger_ = null, goog$debug$LogManager$getLogger = function(name) {
  if(!goog$debug$LogManager$rootLogger_) {
    goog$debug$LogManager$rootLogger_ = new goog$debug$Logger("");
    goog$debug$LogManager$loggers_[""] = goog$debug$LogManager$rootLogger_;
    goog$debug$LogManager$rootLogger_.setLevel(goog$debug$Logger$Level$CONFIG)
  }return name in goog$debug$LogManager$loggers_ ? goog$debug$LogManager$loggers_[name] : goog$debug$LogManager$createLogger_(name)
}, goog$debug$LogManager$createLogger_ = function(name) {
  var logger = new goog$debug$Logger(name), parts = name.split("."), leafName = parts[parts.length - 1];
  parts.length = parts.length - 1;
  var parentName = parts.join("."), parentLogger = goog$debug$LogManager$getLogger(parentName);
  parentLogger.addChild_(leafName, logger);
  logger.setParent_(parentLogger);
  return goog$debug$LogManager$loggers_[name] = logger
};var goog$dom$SavedRange = function() {
  goog$Disposable.call(this)
};
goog$inherits(goog$dom$SavedRange, goog$Disposable);
goog$debug$LogManager$getLogger("goog.dom.SavedRange");var goog$dom$TagIterator = function(opt_node, opt_reversed, opt_unconstrained, opt_tagType, opt_depth) {
  this.reversed = !!opt_reversed;
  opt_node && this.setPosition(opt_node, opt_tagType);
  this.depth = opt_depth != undefined ? opt_depth : this.tagType || 0;
  if(this.reversed)this.depth *= -1;
  this.constrained = !opt_unconstrained
};
goog$inherits(goog$dom$TagIterator, goog$iter$Iterator);
goog$dom$TagIterator.prototype.node = null;
goog$dom$TagIterator.prototype.tagType = null;
goog$dom$TagIterator.prototype.started_ = false;
goog$dom$TagIterator.prototype.setPosition = function(node, opt_tagType, opt_depth) {
  if(this.node = node)this.tagType = typeof opt_tagType == "number" ? opt_tagType : this.node.nodeType != 1 ? 0 : this.reversed ? -1 : 1;
  if(typeof opt_depth == "number")this.depth = opt_depth
};
goog$dom$TagIterator.prototype.next = function() {
  var node;
  if(this.started_) {
    if(!this.node || this.constrained && this.depth == 0)throw goog$iter$StopIteration;node = this.node;
    var startType = this.reversed ? -1 : 1;
    if(this.tagType == startType) {
      var child = this.reversed ? node.lastChild : node.firstChild;
      child ? this.setPosition(child) : this.setPosition(node, startType * -1)
    }else {
      var sibling = this.reversed ? node.previousSibling : node.nextSibling;
      sibling ? this.setPosition(sibling) : this.setPosition(node.parentNode, startType * -1)
    }this.depth += this.tagType * (this.reversed ? -1 : 1)
  }else this.started_ = true;
  node = this.node;
  if(!this.node)throw goog$iter$StopIteration;return node
};
goog$dom$TagIterator.prototype.isStartTag = function() {
  return this.tagType == 1
};var goog$dom$AbstractRange = function() {
};
goog$dom$AbstractRange.prototype.getTextRanges = function() {
  for(var output = [], i = 0, len = this.getTextRangeCount();i < len;i++)output.push(this.getTextRange(i));
  return output
};
goog$dom$AbstractRange.prototype.getAnchorNode = function() {
  return this.isReversed() ? this.getEndNode() : this.getStartNode()
};
goog$dom$AbstractRange.prototype.getAnchorOffset = function() {
  return this.isReversed() ? this.getEndOffset() : this.getStartOffset()
};
goog$dom$AbstractRange.prototype.getFocusNode = function() {
  return this.isReversed() ? this.getStartNode() : this.getEndNode()
};
goog$dom$AbstractRange.prototype.getFocusOffset = function() {
  return this.isReversed() ? this.getStartOffset() : this.getEndOffset()
};
goog$dom$AbstractRange.prototype.isReversed = function() {
  return false
};
goog$dom$AbstractRange.prototype.getDocument = function() {
  return goog$dom$getOwnerDocument(goog$userAgent$IE ? this.getContainer() : this.getStartNode())
};
goog$dom$AbstractRange.prototype.getWindow = function() {
  return goog$dom$getWindow(this.getDocument())
};
goog$dom$AbstractRange.prototype.containsNode = function(node, opt_allowPartial) {
  return this.containsRange(goog$dom$TextRange$createFromNodeContents(node, undefined), opt_allowPartial)
};
var goog$dom$RangeIterator = function(node, opt_reverse) {
  goog$dom$TagIterator.call(this, node, opt_reverse, true)
};
goog$inherits(goog$dom$RangeIterator, goog$dom$TagIterator);var goog$dom$AbstractMultiRange = function() {
};
goog$inherits(goog$dom$AbstractMultiRange, goog$dom$AbstractRange);
goog$dom$AbstractMultiRange.prototype.containsRange = function(otherRange, opt_allowPartial) {
  var ranges = this.getTextRanges(), otherRanges = otherRange.getTextRanges(), fn = opt_allowPartial ? goog$array$some : goog$array$every;
  return fn(otherRanges, function(otherRange) {
    return goog$array$some(ranges, function(range) {
      return range.containsRange(otherRange, opt_allowPartial)
    })
  })
};var goog$dom$TextRangeIterator = function(startNode, startOffset, endNode, endOffset, opt_reverse) {
  var goNext;
  if(startNode) {
    this.startNode_ = startNode;
    this.startOffset_ = startOffset;
    this.endNode_ = endNode;
    this.endOffset_ = endOffset;
    if(startNode.nodeType == 1 && startNode.tagName != "BR") {
      var startChildren = startNode.childNodes, candidate = startChildren[startOffset];
      if(candidate) {
        this.startNode_ = candidate;
        this.startOffset_ = 0
      }else {
        if(startChildren.length)this.startNode_ = goog$array$peek(startChildren);
        goNext = true
      }
    }if(endNode.nodeType == 1)if(this.endNode_ = endNode.childNodes[endOffset])this.endOffset_ = 0;
    else this.endNode_ = endNode
  }goog$dom$RangeIterator.call(this, opt_reverse ? this.endNode_ : this.startNode_, opt_reverse);
  if(goNext)try {
    this.next()
  }catch(e) {
    if(e != goog$iter$StopIteration)throw e;
  }
};
goog$inherits(goog$dom$TextRangeIterator, goog$dom$RangeIterator);
goog$dom$TextRangeIterator.prototype.startNode_ = null;
goog$dom$TextRangeIterator.prototype.endNode_ = null;
goog$dom$TextRangeIterator.prototype.startOffset_ = 0;
goog$dom$TextRangeIterator.prototype.endOffset_ = 0;
goog$dom$TextRangeIterator.prototype.getStartNode = function() {
  return this.startNode_
};
goog$dom$TextRangeIterator.prototype.getEndNode = function() {
  return this.endNode_
};
goog$dom$TextRangeIterator.prototype.isLast = function() {
  return this.started_ && this.node == this.endNode_ && (!this.endOffset_ || !this.isStartTag())
};
goog$dom$TextRangeIterator.prototype.next = function() {
  if(this.isLast())throw goog$iter$StopIteration;return goog$dom$TextRangeIterator.superClass_.next.call(this)
};var goog$userAgent$jscript$DETECTED_HAS_JSCRIPT_, goog$userAgent$jscript$DETECTED_VERSION_, JSCompiler_inline_hasScriptEngine_44 = "ScriptEngine" in goog$global;
goog$userAgent$jscript$DETECTED_VERSION_ = (goog$userAgent$jscript$DETECTED_HAS_JSCRIPT_ = JSCompiler_inline_hasScriptEngine_44 && goog$global.ScriptEngine() == "JScript") ? goog$global.ScriptEngineMajorVersion() + "." + goog$global.ScriptEngineMinorVersion() + "." + goog$global.ScriptEngineBuildVersion() : "0";var goog$dom$browserrange$AbstractRange = function() {
};
goog$dom$browserrange$AbstractRange.prototype.containsRange = function(range, opt_allowPartial) {
  return this.containsBrowserRange(range.range_, opt_allowPartial)
};
goog$dom$browserrange$AbstractRange.prototype.containsBrowserRange = function(range, opt_allowPartial) {
  try {
    return opt_allowPartial ? this.compareBrowserRangeEndpoints(range, 0, 1) >= 0 && this.compareBrowserRangeEndpoints(range, 1, 0) <= 0 : this.compareBrowserRangeEndpoints(range, 0, 0) >= 0 && this.compareBrowserRangeEndpoints(range, 1, 1) <= 0
  }catch(e) {
    if(!goog$userAgent$IE)throw e;return false
  }
};
goog$dom$browserrange$AbstractRange.prototype.containsNode = function(node, opt_allowPartial) {
  return this.containsRange(goog$userAgent$IE ? goog$dom$browserrange$IeRange$createFromNodeContents(node) : goog$userAgent$WEBKIT ? new goog$dom$browserrange$WebKitRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)) : goog$userAgent$GECKO ? new goog$dom$browserrange$GeckoRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)) : new goog$dom$browserrange$W3cRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)), opt_allowPartial)
};
goog$dom$browserrange$AbstractRange.prototype.__iterator__ = function() {
  return new goog$dom$TextRangeIterator(this.getStartNode(), this.getStartOffset(), this.getEndNode(), this.getEndOffset())
};var goog$dom$browserrange$W3cRange = function(range) {
  this.range_ = range
};
goog$inherits(goog$dom$browserrange$W3cRange, goog$dom$browserrange$AbstractRange);
var goog$dom$browserrange$W3cRange$getBrowserRangeForNode = function(node) {
  var nodeRange = goog$dom$getOwnerDocument(node).createRange();
  if(node.nodeType == 3) {
    nodeRange.setStart(node, 0);
    nodeRange.setEnd(node, node.length)
  }else {
    for(var tempNode, leaf = node;tempNode = leaf.firstChild;)leaf = tempNode;
    nodeRange.setStart(leaf, 0);
    for(leaf = node;tempNode = leaf.lastChild;)leaf = tempNode;
    nodeRange.setEnd(leaf, leaf.nodeType == 1 ? leaf.childNodes.length : leaf.length)
  }return nodeRange
}, goog$dom$browserrange$W3cRange$getBrowserRangeForNodes_ = function(startNode, startOffset, endNode, endOffset) {
  var nodeRange = goog$dom$getOwnerDocument(startNode).createRange();
  nodeRange.setStart(startNode, startOffset);
  nodeRange.setEnd(endNode, endOffset);
  return nodeRange
};
goog$dom$browserrange$W3cRange.prototype.getContainer = function() {
  return this.range_.commonAncestorContainer
};
goog$dom$browserrange$W3cRange.prototype.getStartNode = function() {
  return this.range_.startContainer
};
goog$dom$browserrange$W3cRange.prototype.getStartOffset = function() {
  return this.range_.startOffset
};
goog$dom$browserrange$W3cRange.prototype.getEndNode = function() {
  return this.range_.endContainer
};
goog$dom$browserrange$W3cRange.prototype.getEndOffset = function() {
  return this.range_.endOffset
};
goog$dom$browserrange$W3cRange.prototype.compareBrowserRangeEndpoints = function(range, thisEndpoint, otherEndpoint) {
  return this.range_.compareBoundaryPoints(otherEndpoint == 1 ? thisEndpoint == 1 ? goog$global.Range.START_TO_START : goog$global.Range.START_TO_END : thisEndpoint == 1 ? goog$global.Range.END_TO_START : goog$global.Range.END_TO_END, range)
};
goog$dom$browserrange$W3cRange.prototype.isCollapsed = function() {
  return this.range_.collapsed
};
goog$dom$browserrange$W3cRange.prototype.select = function(reverse) {
  var win = goog$dom$getWindow(goog$dom$getOwnerDocument(this.getStartNode()));
  this.selectInternal(win.getSelection(), reverse)
};
goog$dom$browserrange$W3cRange.prototype.selectInternal = function(selection) {
  selection.addRange(this.range_)
};
goog$dom$browserrange$W3cRange.prototype.collapse = function(toStart) {
  this.range_.collapse(toStart)
};var goog$dom$browserrange$GeckoRange = function(range) {
  goog$dom$browserrange$W3cRange.call(this, range)
};
goog$inherits(goog$dom$browserrange$GeckoRange, goog$dom$browserrange$W3cRange);
goog$dom$browserrange$GeckoRange.prototype.selectInternal = function(selection, reversed) {
  var anchorNode = reversed ? this.getEndNode() : this.getStartNode(), anchorOffset = reversed ? this.getEndOffset() : this.getStartOffset(), focusNode = reversed ? this.getStartNode() : this.getEndNode(), focusOffset = reversed ? this.getStartOffset() : this.getEndOffset();
  selection.collapse(anchorNode, anchorOffset);
  if(anchorNode != focusNode || anchorOffset != focusOffset)selection.extend(focusNode, focusOffset)
};var goog$dom$browserrange$IeRange = function(range, doc) {
  this.range_ = range;
  this.doc_ = doc
};
goog$inherits(goog$dom$browserrange$IeRange, goog$dom$browserrange$AbstractRange);
var goog$dom$browserrange$IeRange$logger_ = goog$debug$LogManager$getLogger("goog.dom.browserrange.IeRange"), goog$dom$browserrange$IeRange$getBrowserRangeForNode_ = function(node) {
  var nodeRange = goog$dom$getOwnerDocument(node).body.createTextRange();
  if(node.nodeType == 1)nodeRange.moveToElementText(node);
  else {
    for(var offset = 0, sibling = node;sibling = sibling.previousSibling;) {
      var nodeType = sibling.nodeType;
      if(nodeType == 3)offset += sibling.length;
      else if(nodeType == 1) {
        nodeRange.moveToElementText(sibling);
        break
      }
    }sibling || nodeRange.moveToElementText(node.parentNode);
    nodeRange.collapse(!sibling);
    offset && nodeRange.move("character", offset);
    nodeRange.moveEnd("character", node.length)
  }return nodeRange
}, goog$dom$browserrange$IeRange$getBrowserRangeForNodes_ = function(startNode, startOffset, endNode, endOffset) {
  var child, collapse = false;
  if(startNode.nodeType == 1) {
    startOffset > startNode.childNodes.length && goog$dom$browserrange$IeRange$logger_.severe("Cannot have startOffset > startNode child count");
    child = startNode.childNodes[startOffset];
    collapse = !child;
    startNode = child || startNode;
    startOffset = 0
  }var leftRange = goog$dom$browserrange$IeRange$getBrowserRangeForNode_(startNode);
  startOffset && leftRange.move("character", startOffset);
  collapse && leftRange.collapse(false);
  collapse = false;
  if(endNode.nodeType == 1) {
    startOffset > startNode.childNodes.length && goog$dom$browserrange$IeRange$logger_.severe("Cannot have endOffset > endNode child count");
    endNode = (child = endNode.childNodes[endOffset]) || endNode;
    if(endNode.tagName == "BR")endOffset = 1;
    else {
      endOffset = 0;
      collapse = !child
    }
  }var rightRange = goog$dom$browserrange$IeRange$getBrowserRangeForNode_(endNode);
  rightRange.collapse(!collapse);
  endOffset && rightRange.moveEnd("character", endOffset);
  leftRange.setEndPoint("EndToEnd", rightRange);
  return leftRange
}, goog$dom$browserrange$IeRange$createFromNodeContents = function(node) {
  var range = new goog$dom$browserrange$IeRange(goog$dom$browserrange$IeRange$getBrowserRangeForNode_(node), goog$dom$getOwnerDocument(node));
  range.parentNode_ = node;
  return range
};
goog$dom$browserrange$IeRange.prototype.parentNode_ = null;
goog$dom$browserrange$IeRange.prototype.startNode_ = null;
goog$dom$browserrange$IeRange.prototype.endNode_ = null;
goog$dom$browserrange$IeRange.prototype.clearCachedValues_ = function() {
  this.parentNode_ = this.startNode_ = this.endNode_ = null
};
goog$dom$browserrange$IeRange.prototype.getContainer = function() {
  if(!this.parentNode_) {
    for(var selectText = this.range_.text, i = 1;selectText.charAt(selectText.length - i) == " ";i++)this.range_.moveEnd("character", -1);
    for(var parent = this.range_.parentElement(), htmlText = this.range_.htmlText.replace(/(\r\n|\r|\n)+/g, " ");htmlText.length > parent.outerHTML.replace(/(\r\n|\r|\n)+/g, " ").length;)parent = parent.parentNode;
    for(;parent.childNodes.length == 1 && parent.innerText == (parent.firstChild.nodeType == 3 ? parent.firstChild.nodeValue : parent.firstChild.innerText);) {
      if(parent.firstChild.tagName == "IMG")break;
      parent = parent.firstChild
    }if(selectText.length == 0)parent = this.findDeepestContainer_(parent);
    this.parentNode_ = parent
  }return this.parentNode_
};
goog$dom$browserrange$IeRange.prototype.findDeepestContainer_ = function(node) {
  for(var childNodes = node.childNodes, i = 0, len = childNodes.length;i < len;i++) {
    var child = childNodes[i];
    if(child.nodeType == 1)if(this.range_.inRange(goog$dom$browserrange$IeRange$getBrowserRangeForNode_(child)))return this.findDeepestContainer_(child)
  }return node
};
goog$dom$browserrange$IeRange.prototype.getStartNode = function() {
  return this.startNode_ || (this.startNode_ = this.getEndpointNode_(1))
};
goog$dom$browserrange$IeRange.prototype.getStartOffset = function() {
  return this.getOffset_(1)
};
goog$dom$browserrange$IeRange.prototype.getEndNode = function() {
  return this.endNode_ || (this.endNode_ = this.getEndpointNode_(0))
};
goog$dom$browserrange$IeRange.prototype.getEndOffset = function() {
  return this.getOffset_(0)
};
goog$dom$browserrange$IeRange.prototype.containsRange = function(range, opt_allowPartial) {
  return this.containsBrowserRange(range.range_, opt_allowPartial)
};
goog$dom$browserrange$IeRange.prototype.compareBrowserRangeEndpoints = function(range, thisEndpoint, otherEndpoint) {
  return this.range_.compareEndPoints((thisEndpoint == 1 ? "Start" : "End") + "To" + (otherEndpoint == 1 ? "Start" : "End"), range)
};
goog$dom$browserrange$IeRange.prototype.getEndpointNode_ = function(endpoint, opt_node) {
  var node = opt_node || this.getContainer();
  if(!node || !node.firstChild) {
    if(endpoint == 0 && node.previousSibling && node.previousSibling.tagName == "BR" && this.getOffset_(endpoint, node) == 0)node = node.previousSibling;
    return node.tagName == "BR" ? node.parentNode : node
  }for(var child = endpoint == 1 ? node.firstChild : node.lastChild;child;) {
    if(this.containsNode(child, true))return this.getEndpointNode_(endpoint, child);
    child = endpoint == 1 ? child.nextSibling : child.previousSibling
  }return node
};
goog$dom$browserrange$IeRange.prototype.getOffset_ = function(endpoint, opt_container) {
  var container = opt_container || (endpoint == 1 ? this.getStartNode() : this.getEndNode());
  if(container.nodeType == 1) {
    for(var children = container.childNodes, len = children.length, i = endpoint == 1 ? 0 : len - 1;i >= 0 && i < len;) {
      var child = children[i];
      if(this.containsNode(child, true)) {
        endpoint == 0 && child.previousSibling && child.previousSibling.tagName == "BR" && this.getOffset_(endpoint, child) == 0 && i--;
        break
      }i += endpoint == 1 ? 1 : -1
    }return i == -1 ? 0 : i
  }else {
    var range = this.range_.duplicate(), nodeRange = goog$dom$browserrange$IeRange$getBrowserRangeForNode_(container);
    range.setEndPoint(endpoint == 1 ? "EndToEnd" : "StartToStart", nodeRange);
    var rangeLength = range.text.length;
    return endpoint == 0 ? rangeLength : container.length - rangeLength
  }
};
goog$dom$browserrange$IeRange.prototype.isCollapsed = function() {
  return this.range_.text == ""
};
goog$dom$browserrange$IeRange.prototype.select = function() {
  this.range_.select()
};
goog$dom$browserrange$IeRange.prototype.collapse = function(toStart) {
  this.range_.collapse(toStart);
  if(toStart)this.endNode_ = this.startNode_;
  else this.startNode_ = this.endNode_
};var goog$dom$browserrange$WebKitRange = function(range) {
  goog$dom$browserrange$W3cRange.call(this, range)
};
goog$inherits(goog$dom$browserrange$WebKitRange, goog$dom$browserrange$W3cRange);
goog$dom$browserrange$WebKitRange.prototype.compareBrowserRangeEndpoints = function(range, thisEndpoint, otherEndpoint) {
  if(goog$userAgent$isVersion("528"))return goog$dom$browserrange$WebKitRange.superClass_.compareBrowserRangeEndpoints.call(this, range, thisEndpoint, otherEndpoint);
  return this.range_.compareBoundaryPoints(otherEndpoint == 1 ? thisEndpoint == 1 ? goog$global.Range.START_TO_START : goog$global.Range.END_TO_START : thisEndpoint == 1 ? goog$global.Range.START_TO_END : goog$global.Range.END_TO_END, range)
};
goog$dom$browserrange$WebKitRange.prototype.selectInternal = function(selection, reversed) {
  selection.removeAllRanges();
  reversed ? selection.setBaseAndExtent(this.getEndNode(), this.getEndOffset(), this.getStartNode(), this.getStartOffset()) : selection.setBaseAndExtent(this.getStartNode(), this.getStartOffset(), this.getEndNode(), this.getEndOffset())
};var goog$dom$browserrange$createRangeFromNodes = function(startNode, startOffset, endNode, endOffset) {
  return goog$userAgent$IE ? new goog$dom$browserrange$IeRange(goog$dom$browserrange$IeRange$getBrowserRangeForNodes_(startNode, startOffset, endNode, endOffset), goog$dom$getOwnerDocument(startNode)) : goog$userAgent$WEBKIT ? new goog$dom$browserrange$WebKitRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNodes_(startNode, startOffset, endNode, endOffset)) : goog$userAgent$GECKO ? new goog$dom$browserrange$GeckoRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNodes_(startNode, startOffset, 
  endNode, endOffset)) : new goog$dom$browserrange$W3cRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNodes_(startNode, startOffset, endNode, endOffset))
};var goog$dom$TextRange = function() {
};
goog$inherits(goog$dom$TextRange, goog$dom$AbstractRange);
var goog$dom$TextRange$createFromBrowserRangeWrapper_ = function(browserRange, opt_isReversed) {
  var range = new goog$dom$TextRange;
  range.browserRangeWrapper_ = browserRange;
  range.isReversed_ = !!opt_isReversed;
  return range
}, goog$dom$TextRange$createFromNodeContents = function(node, opt_isReversed) {
  return goog$dom$TextRange$createFromBrowserRangeWrapper_(goog$userAgent$IE ? goog$dom$browserrange$IeRange$createFromNodeContents(node) : goog$userAgent$WEBKIT ? new goog$dom$browserrange$WebKitRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)) : goog$userAgent$GECKO ? new goog$dom$browserrange$GeckoRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)) : new goog$dom$browserrange$W3cRange(goog$dom$browserrange$W3cRange$getBrowserRangeForNode(node)), opt_isReversed)
}, goog$dom$TextRange$createFromNodes = function(anchorNode, anchorOffset, focusNode, focusOffset) {
  var range = new goog$dom$TextRange;
  range.isReversed_ = goog$dom$Range$isReversed(anchorNode, anchorOffset, focusNode, focusOffset);
  if(anchorNode.tagName == "BR") {
    var parent = anchorNode.parentNode;
    anchorOffset = goog$array$indexOf(parent.childNodes, anchorNode);
    anchorNode = parent
  }if(focusNode.tagName == "BR") {
    parent = focusNode.parentNode;
    focusOffset = goog$array$indexOf(parent.childNodes, focusNode);
    focusNode = parent
  }if(range.isReversed_) {
    range.startNode_ = focusNode;
    range.startOffset_ = focusOffset;
    range.endNode_ = anchorNode;
    range.endOffset_ = anchorOffset
  }else {
    range.startNode_ = anchorNode;
    range.startOffset_ = anchorOffset;
    range.endNode_ = focusNode;
    range.endOffset_ = focusOffset
  }return range
};
goog$dom$TextRange.prototype.browserRangeWrapper_ = null;
goog$dom$TextRange.prototype.startNode_ = null;
goog$dom$TextRange.prototype.startOffset_ = null;
goog$dom$TextRange.prototype.endNode_ = null;
goog$dom$TextRange.prototype.endOffset_ = null;
goog$dom$TextRange.prototype.isReversed_ = false;
goog$dom$TextRange.prototype.getType = function() {
  return"text"
};
goog$dom$TextRange.prototype.getBrowserRangeObject = function() {
  return this.getBrowserRangeWrapper_().range_
};
goog$dom$TextRange.prototype.clearCachedValues_ = function() {
  this.startNode_ = this.startOffset_ = this.endNode_ = this.endOffset_ = null
};
goog$dom$TextRange.prototype.getTextRangeCount = function() {
  return 1
};
goog$dom$TextRange.prototype.getTextRange = function() {
  return this
};
goog$dom$TextRange.prototype.getBrowserRangeWrapper_ = function() {
  return this.browserRangeWrapper_ || (this.browserRangeWrapper_ = goog$dom$browserrange$createRangeFromNodes(this.getStartNode(), this.getStartOffset(), this.getEndNode(), this.getEndOffset()))
};
goog$dom$TextRange.prototype.getContainer = function() {
  return this.getBrowserRangeWrapper_().getContainer()
};
goog$dom$TextRange.prototype.getStartNode = function() {
  return this.startNode_ || (this.startNode_ = this.getBrowserRangeWrapper_().getStartNode())
};
goog$dom$TextRange.prototype.getStartOffset = function() {
  return this.startOffset_ != null ? this.startOffset_ : (this.startOffset_ = this.getBrowserRangeWrapper_().getStartOffset())
};
goog$dom$TextRange.prototype.getEndNode = function() {
  return this.endNode_ || (this.endNode_ = this.getBrowserRangeWrapper_().getEndNode())
};
goog$dom$TextRange.prototype.getEndOffset = function() {
  return this.endOffset_ != null ? this.endOffset_ : (this.endOffset_ = this.getBrowserRangeWrapper_().getEndOffset())
};
goog$dom$TextRange.prototype.isReversed = function() {
  return this.isReversed_
};
goog$dom$TextRange.prototype.containsRange = function(otherRange, opt_allowPartial) {
  var otherRangeType = otherRange.getType();
  if(otherRangeType == "text")return this.getBrowserRangeWrapper_().containsRange(otherRange.getBrowserRangeWrapper_(), opt_allowPartial);
  else if(otherRangeType == "control") {
    var elements = otherRange.getElements(), fn = opt_allowPartial ? goog$array$some : goog$array$every;
    return fn(elements, function(el) {
      return this.containsNode(el, opt_allowPartial)
    }, this)
  }
};
goog$dom$TextRange.prototype.isCollapsed = function() {
  return this.getBrowserRangeWrapper_().isCollapsed()
};
goog$dom$TextRange.prototype.__iterator__ = function() {
  return new goog$dom$TextRangeIterator(this.getStartNode(), this.getStartOffset(), this.getEndNode(), this.getEndOffset())
};
goog$dom$TextRange.prototype.select = function() {
  this.getBrowserRangeWrapper_().select(this.isReversed_)
};
goog$dom$TextRange.prototype.saveUsingDom = function() {
  return new goog$dom$DomSavedTextRange_(this)
};
goog$dom$TextRange.prototype.collapse = function(toAnchor) {
  var toStart = this.isReversed() ? !toAnchor : toAnchor;
  this.browserRangeWrapper_ && this.browserRangeWrapper_.collapse(toStart);
  if(toStart) {
    this.endNode_ = this.startNode_;
    this.endOffset_ = this.startOffset_
  }else {
    this.startNode_ = this.endNode_;
    this.startOffset_ = this.endOffset_
  }this.isReversed_ = false
};
var goog$dom$DomSavedTextRange_ = function(range) {
  this.anchorNode_ = range.getAnchorNode();
  this.anchorOffset_ = range.getAnchorOffset();
  this.focusNode_ = range.getFocusNode();
  this.focusOffset_ = range.getFocusOffset()
};
goog$inherits(goog$dom$DomSavedTextRange_, goog$dom$SavedRange);var goog$dom$ControlRange = function() {
};
goog$inherits(goog$dom$ControlRange, goog$dom$AbstractMultiRange);
goog$dom$ControlRange.prototype.range_ = null;
goog$dom$ControlRange.prototype.elements_ = null;
goog$dom$ControlRange.prototype.sortedElements_ = null;
goog$dom$ControlRange.prototype.clearCachedValues_ = function() {
  this.sortedElements_ = this.elements_ = null
};
goog$dom$ControlRange.prototype.getType = function() {
  return"control"
};
goog$dom$ControlRange.prototype.getBrowserRangeObject = function() {
  return this.range_ || document.body.createControlRange()
};
goog$dom$ControlRange.prototype.getTextRangeCount = function() {
  return this.range_ ? this.range_.length : 0
};
goog$dom$ControlRange.prototype.getTextRange = function(i) {
  return goog$dom$TextRange$createFromNodeContents(this.range_.item(i))
};
goog$dom$ControlRange.prototype.getContainer = function() {
  return goog$dom$findCommonAncestor.apply(null, this.getElements())
};
goog$dom$ControlRange.prototype.getStartNode = function() {
  return this.getSortedElements()[0]
};
goog$dom$ControlRange.prototype.getStartOffset = function() {
  return 0
};
goog$dom$ControlRange.prototype.getEndNode = function() {
  var sorted = this.getSortedElements(), startsLast = goog$array$peek(sorted);
  return goog$array$find(sorted, function(el) {
    return goog$dom$contains(el, startsLast)
  })
};
goog$dom$ControlRange.prototype.getEndOffset = function() {
  return this.getEndNode().childNodes.length
};
goog$dom$ControlRange.prototype.getElements = function() {
  if(!this.elements_) {
    this.elements_ = [];
    if(this.range_)for(var i = 0;i < this.range_.length;i++)this.elements_.push(this.range_.item(i))
  }return this.elements_
};
goog$dom$ControlRange.prototype.getSortedElements = function() {
  if(!this.sortedElements_) {
    this.sortedElements_ = this.getElements().concat();
    this.sortedElements_.sort(function(a, b) {
      return a.sourceIndex - b.sourceIndex
    })
  }return this.sortedElements_
};
goog$dom$ControlRange.prototype.isCollapsed = function() {
  return!this.range_ || !this.range_.length
};
goog$dom$ControlRange.prototype.__iterator__ = function() {
  return new goog$dom$ControlRangeIterator(this)
};
goog$dom$ControlRange.prototype.select = function() {
  this.range_ && this.range_.select()
};
goog$dom$ControlRange.prototype.saveUsingDom = function() {
  return new goog$dom$DomSavedControlRange_(this)
};
goog$dom$ControlRange.prototype.collapse = function() {
  this.range_ = null;
  this.clearCachedValues_()
};
var goog$dom$DomSavedControlRange_ = function(range) {
  this.elements_ = range.getElements()
};
goog$inherits(goog$dom$DomSavedControlRange_, goog$dom$SavedRange);
var goog$dom$ControlRangeIterator = function(range) {
  if(range) {
    this.elements_ = range.getSortedElements();
    this.startNode_ = this.elements_.shift();
    this.endNode_ = goog$array$peek(this.elements_) || this.startNode_
  }goog$dom$RangeIterator.call(this, this.startNode_, false)
};
goog$inherits(goog$dom$ControlRangeIterator, goog$dom$RangeIterator);
goog$dom$ControlRangeIterator.prototype.startNode_ = null;
goog$dom$ControlRangeIterator.prototype.endNode_ = null;
goog$dom$ControlRangeIterator.prototype.elements_ = null;
goog$dom$ControlRangeIterator.prototype.getStartNode = function() {
  return this.startNode_
};
goog$dom$ControlRangeIterator.prototype.getEndNode = function() {
  return this.endNode_
};
goog$dom$ControlRangeIterator.prototype.isLast = function() {
  return!this.depth && !this.elements_.length
};
goog$dom$ControlRangeIterator.prototype.next = function() {
  if(this.isLast())throw goog$iter$StopIteration;else if(!this.depth) {
    var el = this.elements_.shift();
    this.setPosition(el, 1, 1);
    return el
  }return goog$dom$ControlRangeIterator.superClass_.next.call(this)
};var goog$dom$MultiRange = function() {
  this.browserRanges_ = [];
  this.ranges_ = [];
  this.container_ = this.sortedRanges_ = null
};
goog$inherits(goog$dom$MultiRange, goog$dom$AbstractMultiRange);
goog$dom$MultiRange.prototype.logger_ = goog$debug$LogManager$getLogger("goog.dom.MultiRange");
goog$dom$MultiRange.prototype.clearCachedValues_ = function() {
  this.ranges_ = [];
  this.container_ = this.sortedRanges_ = null
};
goog$dom$MultiRange.prototype.getType = function() {
  return"mutli"
};
goog$dom$MultiRange.prototype.getBrowserRangeObject = function() {
  this.browserRanges_.length > 1 && this.logger_.warning("getBrowserRangeObject called on MultiRange with more than 1 range");
  return this.browserRanges_[0]
};
goog$dom$MultiRange.prototype.getTextRangeCount = function() {
  return this.browserRanges_.length
};
goog$dom$MultiRange.prototype.getTextRange = function(i) {
  this.ranges_[i] || (this.ranges_[i] = goog$dom$TextRange$createFromBrowserRangeWrapper_(goog$userAgent$IE ? new goog$dom$browserrange$IeRange(this.browserRanges_[i], goog$dom$getOwnerDocument(this.browserRanges_[i].parentElement())) : goog$userAgent$WEBKIT ? new goog$dom$browserrange$WebKitRange(this.browserRanges_[i]) : goog$userAgent$GECKO ? new goog$dom$browserrange$GeckoRange(this.browserRanges_[i]) : new goog$dom$browserrange$W3cRange(this.browserRanges_[i]), undefined));
  return this.ranges_[i]
};
goog$dom$MultiRange.prototype.getContainer = function() {
  if(!this.container_) {
    for(var nodes = [], i = 0, len = this.getTextRangeCount();i < len;i++)nodes.push(this.getTextRange(i).getContainer());
    this.container_ = goog$dom$findCommonAncestor.apply(null, nodes)
  }return this.container_
};
goog$dom$MultiRange.prototype.getSortedRanges = function() {
  if(!this.sortedRanges_) {
    this.sortedRanges_ = this.getTextRanges();
    this.sortedRanges_.sort(function(a, b) {
      var aStartNode = a.getStartNode(), aStartOffset = a.getStartOffset(), bStartNode = b.getStartNode(), bStartOffset = b.getStartOffset();
      if(aStartNode == bStartNode && aStartOffset == bStartOffset)return 0;
      return goog$dom$Range$isReversed(aStartNode, aStartOffset, bStartNode, bStartOffset) ? 1 : -1
    })
  }return this.sortedRanges_
};
goog$dom$MultiRange.prototype.getStartNode = function() {
  return this.getSortedRanges()[0].getStartNode()
};
goog$dom$MultiRange.prototype.getStartOffset = function() {
  return this.getSortedRanges()[0].getStartOffset()
};
goog$dom$MultiRange.prototype.getEndNode = function() {
  return goog$array$peek(this.getSortedRanges()).getEndNode()
};
goog$dom$MultiRange.prototype.getEndOffset = function() {
  return goog$array$peek(this.getSortedRanges()).getEndOffset()
};
goog$dom$MultiRange.prototype.isCollapsed = function() {
  return this.browserRanges_.length == 0 || this.browserRanges_.length == 1 && this.getTextRange(0).isCollapsed()
};
goog$dom$MultiRange.prototype.__iterator__ = function() {
  return new goog$dom$MultiRangeIterator(this)
};
goog$dom$MultiRange.prototype.select = function() {
  var selection;
  JSCompiler_inline_label_goog$dom$AbstractRange$getBrowserSelectionForWindow_50: {
    var JSCompiler_inline_win = this.getWindow();
    if(JSCompiler_inline_win.getSelection)selection = JSCompiler_inline_win.getSelection();
    else {
      var JSCompiler_inline_doc = JSCompiler_inline_win.document;
      selection = JSCompiler_inline_doc.selection || JSCompiler_inline_doc.getSelection && JSCompiler_inline_doc.getSelection()
    }
  }selection.removeAllRanges();
  for(var i = 0, len = this.getTextRangeCount();i < len;i++)selection.addRange(this.getTextRange(i).getBrowserRangeObject())
};
goog$dom$MultiRange.prototype.saveUsingDom = function() {
  return new goog$dom$DomSavedMultiRange_(this)
};
goog$dom$MultiRange.prototype.collapse = function(toAnchor) {
  if(!this.isCollapsed()) {
    var range = toAnchor ? this.getTextRange(0) : this.getTextRange(this.getTextRangeCount() - 1);
    this.clearCachedValues_();
    range.collapse(toAnchor);
    this.ranges_ = [range];
    this.sortedRanges_ = [range];
    this.browserRanges_ = [range.getBrowserRangeObject()]
  }
};
var goog$dom$DomSavedMultiRange_ = function(range) {
  this.savedRanges_ = goog$array$map(range.getTextRanges(), function(range) {
    return range.saveUsingDom()
  })
};
goog$inherits(goog$dom$DomSavedMultiRange_, goog$dom$SavedRange);
var goog$dom$MultiRangeIterator = function(range) {
  if(range) {
    this.ranges_ = range.getSortedRanges();
    if(this.ranges_.length) {
      this.startNode_ = this.ranges_[0].getStartNode();
      this.endNode_ = goog$array$peek(this.ranges_).getEndNode()
    }
  }goog$dom$RangeIterator.call(this, this.startNode_, false)
};
goog$inherits(goog$dom$MultiRangeIterator, goog$dom$RangeIterator);
goog$dom$MultiRangeIterator.prototype.startNode_ = null;
goog$dom$MultiRangeIterator.prototype.endNode_ = null;
goog$dom$MultiRangeIterator.prototype.ranges_ = null;
goog$dom$MultiRangeIterator.prototype.getStartNode = function() {
  return this.startNode_
};
goog$dom$MultiRangeIterator.prototype.getEndNode = function() {
  return this.endNode_
};
goog$dom$MultiRangeIterator.prototype.isLast = function() {
  return this.ranges_.length == 1 && this.ranges_[0].isLast()
};
goog$dom$MultiRangeIterator.prototype.next = function() {
  do try {
    this.ranges_[0].next();
    break
  }catch(ex) {
    if(ex != goog$iter$StopIteration)throw ex;this.ranges_.shift()
  }while(this.ranges_.length);
  if(this.ranges_.length) {
    var range = this.ranges_[0];
    this.setPosition(range.node, range.tagType, range.depth);
    return range.node
  }else throw goog$iter$StopIteration;
};var goog$dom$Range$createCaret = function(node, offset) {
  return goog$dom$TextRange$createFromNodes(node, offset, node, offset)
}, goog$dom$Range$createFromNodes = function(startNode, startOffset, endNode, endOffset) {
  return goog$dom$TextRange$createFromNodes(startNode, startOffset, endNode, endOffset)
}, goog$dom$Range$isReversed = function(anchorNode, anchorOffset, focusNode, focusOffset) {
  if(anchorNode == focusNode)return focusOffset < anchorOffset;
  var child;
  if(anchorNode.nodeType == 1 && anchorOffset)if(child = anchorNode.childNodes[anchorOffset]) {
    anchorNode = child;
    anchorOffset = 0
  }else if(goog$dom$contains(anchorNode, focusNode))return true;
  if(focusNode.nodeType == 1 && focusOffset)if(child = focusNode.childNodes[focusOffset]) {
    focusNode = child;
    focusOffset = 0
  }else if(goog$dom$contains(focusNode, anchorNode))return false;
  return(goog$dom$compareNodeOrder(anchorNode, focusNode) || anchorOffset - focusOffset) > 0
};window.createCaret = goog$dom$Range$createCaret;
window.createFromNodes = goog$dom$Range$createFromNodes;
try {
  goog$dom$Range$createCaret(document.body, 0).select()
}catch(e$$13) {
};

/**************************************************
  Trace:
      56.427 Start        Handling request
    0 56.427 Start        Building cUnit
    1 56.428 Done    1 ms Building cUnit
    0 56.428 Start        Checking memcacheg
    0 56.428 Start        Connecting to memcacheg
    8 56.436 Done    8 ms Connecting to memcacheg
    1 56.437 Done    9 ms Checking memcacheg
    0 56.437 Done   10 ms Handling request
**************************************************/
