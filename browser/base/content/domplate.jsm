/*
 * Software License Agreement (BSD License)
 *
 * Copyright (c) 2007, Parakey Inc.
 * All rights reserved.
 *
 * Redistribution and use of this software in source and binary forms, with or without modification,
 * are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above
 *   copyright notice, this list of conditions and the
 *   following disclaimer in the documentation and/or other
 *   materials provided with the distribution.
 *
 * * Neither the name of Parakey Inc. nor the names of its
 *   contributors may be used to endorse or promote products
 *   derived from this software without specific prior
 *   written permission of Parakey Inc.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND
 * FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
 * IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
 * OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * Creator:
 *  Joe Hewitt
 * Contributors:
 *  John J. Barton (IBM Almaden)
 *  Jan Odvarko (Mozilla Corp.)
 *  Max Stepanov (Aptana Inc.)
 *  Rob Campbell (Mozilla Corp.)
 *  Hans Hillen (Paciello Group, Mozilla)
 *  Curtis Bartley (Mozilla Corp.)
 *  Mike Collins (IBM Almaden)
 *  Kevin Decker
 *  Mike Ratcliffe (Comartis AG)
 *  Hernan Rodríguez Colmeiro
 *  Austin Andrews
 *  Christoph Dorn
 *  Steven Roussey (AppCenter Inc, Network54)
 * Firefox Port Contributors:
 *  Rob Campbell
 *  Julian Viereck
 *  Mihai Sucan
 */

var EXPORTED_SYMBOLS = ["domplate", "HTMLTemplates", "domplateUtils"];

const Ci = Components.interfaces;
const Cu = Components.utils;

const invisibleTags = {
  "head": true,
  "base": true,
  "basefont": true,
  "isindex": true,
  "link": true,
  "meta": true,
  "script": true,
  "style": true,
  "title": true,
};

// End tags for void elements are forbidden
// http://wiki.whatwg.org/wiki/HTML_vs._XHTML
const selfClosingTags = {
  "meta": 1,
  "link": 1,
  "area": 1,
  "base": 1,
  "col": 1,
  "input": 1,
  "img": 1,
  "br": 1,
  "hr": 1,
  "param": 1,
  "embed": 1
};

const reNotWhitespace = /[^\s]/;
const showTextNodesWithWhitespace = false;

var DOM = {};
var domplateUtils = {};

/**
 * Utility function to allow outside caller to set a global scope within
 * domplate's DOM object. Specifically for access to DOM constants and classes.
 * @param aGlobal
 *        The global object whose scope we wish to capture.
 */
domplateUtils.setDOM = function(aGlobal)
{
  DOM = aGlobal;
  if (!aGlobal) {
    womb = null;
  }
};

/**
 * main domplate constructor function.
 */

let domplate = function()
{
  let lastSubject;
  for (let i = 0; i < arguments.length; ++i)
    lastSubject = lastSubject ? copyObject(lastSubject, arguments[i]) : arguments[i];

  for (let name in lastSubject) {
    let val = lastSubject[name];
    if (isTag(val))
      val.tag.subject = lastSubject;
  }

  return lastSubject;
};

var womb = null;

///////////////////////////////////////////////////////////////////////////
//// Base functions

function DomplateTag(tagName)
{
  this.tagName = tagName;
}

function DomplateEmbed()
{
}

function DomplateLoop()
{
}

///////////////////////////////////////////////////////////////////////////
//// Definitions

domplate.context = function(context, fn)
{
  let lastContext = domplate.lastContext;
  domplate.topContext = context;
  fn.apply(context);
  domplate.topContext = lastContext;
};

domplate.TAG = function()
{
  let embed = new DomplateEmbed();
  return embed.merge(arguments);
};

domplate.FOR = function()
{
  let loop = new DomplateLoop();
  return loop.merge(arguments);
};

DomplateTag.prototype =
{
  /**
   * Initializer for DOM templates. Called to create new Functions objects
   * like TR, TD, OBJLINK, etc. See defineTag
   * @param args keyword argments for the template, the {} brace stuff after
   *        the tag name, eg TR({...}, TD(...
   * @param oldTag a nested tag, eg the TD tag in TR({...}, TD(...
   */
  merge: function(args, oldTag)
  {
    if (oldTag)
      this.tagName = oldTag.tagName;

    this.context = oldTag ? oldTag.context : null;  // normally null on construction
    this.subject = oldTag ? oldTag.subject : null;
    this.attrs = oldTag ? copyObject(oldTag.attrs) : {};
    this.classes = oldTag ? copyObject(oldTag.classes) : {};
    this.props = oldTag ? copyObject(oldTag.props) : null;
    this.listeners = oldTag ? copyArray(oldTag.listeners) : null;
    this.children = oldTag ? copyArray(oldTag.children) : [];
    this.vars = oldTag ? copyArray(oldTag.vars) : [];

    let attrs = args.length ? args[0] : null;
    let hasAttrs = typeof(attrs) == "object" && !isTag(attrs);

    // Do not clear children, they can be copied from the oldTag.
    //this.children = [];

    if (domplate.topContext)
      this.context = domplate.topContext;

    if (args.length)
      parseChildren(args, hasAttrs ? 1 : 0, this.vars, this.children);

    if (hasAttrs)
      this.parseAttrs(attrs);

    return creator(this, DomplateTag);
  },

  /**
   * Parse node attributes.
   * @param args
   *        Object of arguments to process.
   */
  parseAttrs: function(args)
  {
    for (let name in args) {
      let val = parseValue(args[name]);
      readPartNames(val, this.vars);

      if (name.indexOf("on") == 0) {
        let eventName = name.substr(2);
        if (!this.listeners)
          this.listeners = [];
        this.listeners.push(eventName, val);
      } else if (name[0] == "_") {
        let propName = name.substr(1);
        if (!this.props)
          this.props = {};
        this.props[propName] = val;
      } else if (name[0] == "$") {
        let className = name.substr(1);
        if (!this.classes)
          this.classes = {};
        this.classes[className] = val;
      } else {
        if (name == "class" && this.attrs.hasOwnProperty(name))
          this.attrs[name] += " " + val;
        else
          this.attrs[name] = val;
      }
    }
  },

  compile: function()
  {
    if (this.renderMarkup)
      return;

    this.compileMarkup();
    this.compileDOM();
  },

  compileMarkup: function()
  {
    this.markupArgs = [];
    let topBlock = [], topOuts = [], blocks = [],
      info = {args: this.markupArgs, argIndex: 0};

    this.generateMarkup(topBlock, topOuts, blocks, info);
    this.addCode(topBlock, topOuts, blocks);

    let fnBlock = ['(function (__code__, __context__, __in__, __out__'];
    for (let i = 0; i < info.argIndex; ++i)
      fnBlock.push(', s', i);
    fnBlock.push(') {\n');

    if (this.subject)
      fnBlock.push('with (this) {\n');
    if (this.context)
      fnBlock.push('with (__context__) {\n');
    fnBlock.push('with (__in__) {\n');

    fnBlock.push.apply(fnBlock, blocks);

    if (this.subject)
      fnBlock.push('}\n');
    if (this.context)
      fnBlock.push('}\n');

    fnBlock.push('}})\n');

    function __link__(tag, code, outputs, args)
    {
      tag.tag.compile();

      let tagOutputs = [];
      let markupArgs = [code, tag.tag.context, args, tagOutputs];
      markupArgs.push.apply(markupArgs, tag.tag.markupArgs);
      tag.tag.renderMarkup.apply(tag.tag.subject, markupArgs);

      outputs.push(tag);
      outputs.push(tagOutputs);
    }

    function __escape__(value)
    {
      function replaceChars(ch)
      {
        switch (ch) {
          case "<":
            return "&lt;";
          case ">":
            return "&gt;";
          case "&":
            return "&amp;";
          case "'":
            return "&#39;";
          case '"':
            return "&quot;";
        }
        return "?";
      };
      return String(value).replace(/[<>&"']/g, replaceChars);
    }

    function __loop__(iter, outputs, fn)
    {
      let iterOuts = [];
      outputs.push(iterOuts);

      if (iter instanceof Array)
        iter = new ArrayIterator(iter);

      try {
        while (1) {
          let value = iter.next();
          let itemOuts = [0, 0];
          iterOuts.push(itemOuts);
          fn.apply(this, [value, itemOuts]);
        }
      } catch (exc) {
        if (exc != StopIteration)
          throw exc;
      }
    }

    let js = fnBlock.join("");
    this.renderMarkup = eval(js);
  },

  getVarNames: function(args)
  {
    if (this.vars)
      args.push.apply(args, this.vars);

    for (let i = 0; i < this.children.length; ++i) {
      let child = this.children[i];
      if (isTag(child))
        child.tag.getVarNames(args);
      else if (child instanceof Parts) {
        for (let i = 0; i < child.parts.length; ++i) {
          if (child.parts[i] instanceof Variable) {
            let name = child.parts[i].name;
            let names = name.split(".");
            args.push(names[0]);
          }
        }
      }
    }
  },

  generateMarkup: function(topBlock, topOuts, blocks, info)
  {
    topBlock.push(',"<', this.tagName, '"');

    for (let name in this.attrs) {
      if (name != "class") {
        let val = this.attrs[name];
        topBlock.push(', " ', name, '=\\""');
        addParts(val, ',', topBlock, info, true);
        topBlock.push(', "\\""');
      }
    }

    if (this.listeners) {
      for (let i = 0; i < this.listeners.length; i += 2)
        readPartNames(this.listeners[i+1], topOuts);
    }

    if (this.props) {
      for (let name in this.props)
        readPartNames(this.props[name], topOuts);
    }

    if (this.attrs.hasOwnProperty("class") || this.classes) {
      topBlock.push(', " class=\\""');
      if (this.attrs.hasOwnProperty("class"))
        addParts(this.attrs["class"], ',', topBlock, info, true);
      topBlock.push(', " "');
      for (let name in this.classes) {
        topBlock.push(', (');
        addParts(this.classes[name], '', topBlock, info);
        topBlock.push(' ? "', name, '" + " " : "")');
      }
      topBlock.push(', "\\""');
    }
    topBlock.push(',">"');

    this.generateChildMarkup(topBlock, topOuts, blocks, info);
    topBlock.push(',"</', this.tagName, '>"');
  },

  generateChildMarkup: function(topBlock, topOuts, blocks, info)
  {
    for (let i = 0; i < this.children.length; ++i) {
      let child = this.children[i];
      if (isTag(child))
        child.tag.generateMarkup(topBlock, topOuts, blocks, info);
      else
        addParts(child, ',', topBlock, info, true);
    }
  },

  addCode: function(topBlock, topOuts, blocks)
  {
    if (topBlock.length)
      blocks.push('__code__.push(""', topBlock.join(""), ');\n');
    if (topOuts.length)
      blocks.push('__out__.push(', topOuts.join(","), ');\n');
    topBlock.splice(0, topBlock.length);
    topOuts.splice(0, topOuts.length);
  },

  addLocals: function(blocks)
  {
    let varNames = [];
    this.getVarNames(varNames);

    let map = {};
    for (let i = 0; i < varNames.length; ++i) {
      let name = varNames[i];
      if ( map.hasOwnProperty(name) )
        continue;

      map[name] = 1;
      let names = name.split(".");
      blocks.push('var ', names[0] + ' = ' + '__in__.' + names[0] + ';\n');
    }
  },

  compileDOM: function()
  {
    let path = [];
    let blocks = [];
    this.domArgs = [];
    path.embedIndex = 0;
    path.loopIndex = 0;
    path.staticIndex = 0;
    path.renderIndex = 0;
    let nodeCount = this.generateDOM(path, blocks, this.domArgs);

    let fnBlock = ['(function (root, context, o'];

    for (let i = 0; i < path.staticIndex; ++i)
      fnBlock.push(', ', 's'+i);

    for (let i = 0; i < path.renderIndex; ++i)
      fnBlock.push(', ', 'd'+i);

    fnBlock.push(') {\n');
    for (let i = 0; i < path.loopIndex; ++i)
      fnBlock.push('var l', i, ' = 0;\n');
    for (let i = 0; i < path.embedIndex; ++i)
      fnBlock.push('var e', i, ' = 0;\n');

    if (this.subject)
      fnBlock.push('with (this) {\n');
    if (this.context)
      fnBlock.push('with (context) {\n');

    fnBlock.push(blocks.join(""));

    if (this.subject)
      fnBlock.push('}\n');
    if (this.context)
      fnBlock.push('}\n');

    fnBlock.push('return ', nodeCount, ';\n');
    fnBlock.push('})\n');

    function __bind__(object, fn)
    {
      return function(event) { return fn.apply(object, [event]); }
    }

    function __link__(node, tag, args)
    {
      if (!tag || !tag.tag)
        return;

      tag.tag.compile();

      let domArgs = [node, tag.tag.context, 0];
      domArgs.push.apply(domArgs, tag.tag.domArgs);
      domArgs.push.apply(domArgs, args);

      return tag.tag.renderDOM.apply(tag.tag.subject, domArgs);
    }

    function __loop__(iter, fn)
    {
      let nodeCount = 0;
      for (let i = 0; i < iter.length; ++i) {
        iter[i][0] = i;
        iter[i][1] = nodeCount;
        nodeCount += fn.apply(this, iter[i]);
      }
      return nodeCount;
    }

    function __path__(parent, offset)
    {
      let root = parent;

      for (let i = 2; i < arguments.length; ++i) {
        let index = arguments[i];
        if (i == 3)
          index += offset;

        if (index == -1)
          parent = parent.parentNode;
        else
          parent = parent.childNodes[index];
      }

      return parent;
    }
    let js = fnBlock.join("");
    // Exceptions on this line are often in the eval
    this.renderDOM = eval(js);
  },

  generateDOM: function(path, blocks, args)
  {
    if (this.listeners || this.props)
      this.generateNodePath(path, blocks);

    if (this.listeners) {
      for (let i = 0; i < this.listeners.length; i += 2) {
        let val = this.listeners[i+1];
        let arg = generateArg(val, path, args);
        blocks.push('node.addEventListener("', this.listeners[i],
          '", __bind__(this, ', arg, '), false);\n');
      }
    }

    if (this.props) {
      for (let name in this.props) {
        let val = this.props[name];
        let arg = generateArg(val, path, args);
        blocks.push('node.', name, ' = ', arg, ';\n');
      }
    }

    this.generateChildDOM(path, blocks, args);
    return 1;
  },

  generateNodePath: function(path, blocks)
  {
    blocks.push("var node = __path__(root, o");
    for (let i = 0; i < path.length; ++i)
      blocks.push(",", path[i]);
    blocks.push(");\n");
  },

  generateChildDOM: function(path, blocks, args)
  {
    path.push(0);
    for (let i = 0; i < this.children.length; ++i) {
      let child = this.children[i];
      if (isTag(child))
        path[path.length - 1] += '+' + child.tag.generateDOM(path, blocks, args);
      else
        path[path.length - 1] += '+1';
    }
    path.pop();
  },

  /*
   * We are just hiding from javascript.options.strict. For some reasons it's
   * ok if we return undefined here.
   * @return null or undefined or possibly a context.
   */
  getContext: function()
  {
    return this.context;
  }
};

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

DomplateEmbed.prototype = copyObject(DomplateTag.prototype,
{
  merge: function(args, oldTag)
  {
    this.value = oldTag ? oldTag.value : parseValue(args[0]);
    this.attrs = oldTag ? oldTag.attrs : {};
    this.vars = oldTag ? copyArray(oldTag.vars) : [];

    let attrs = args[1];
    for (let name in attrs) {
      let val = parseValue(attrs[name]);
      this.attrs[name] = val;
      readPartNames(val, this.vars);
    }

    return creator(this, DomplateEmbed);
  },

  getVarNames: function(names)
  {
    if (this.value instanceof Parts)
      names.push(this.value.parts[0].name);

    if (this.vars)
      names.push.apply(names, this.vars);
  },

  generateMarkup: function(topBlock, topOuts, blocks, info)
  {
    this.addCode(topBlock, topOuts, blocks);

    blocks.push('__link__(');
    addParts(this.value, '', blocks, info);
    blocks.push(', __code__, __out__, {\n');

    let lastName = null;
    for (let name in this.attrs) {
      if (lastName)
        blocks.push(',');
      lastName = name;

      let val = this.attrs[name];
      blocks.push('"', name, '":');
      addParts(val, '', blocks, info);
    }

    blocks.push('});\n');
  },

  generateDOM: function(path, blocks, args)
  {
    let embedName = 'e' + path.embedIndex++;

    this.generateNodePath(path, blocks);

    let valueName = 'd' + path.renderIndex++;
    let argsName = 'd' + path.renderIndex++;
    blocks.push(embedName + ' = __link__(node, ', valueName, ', ',
      argsName, ');\n');

    return embedName;
  }
});

// * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *

DomplateLoop.prototype = copyObject(DomplateTag.prototype,
{
  merge: function(args, oldTag)
  {
    this.isLoop = true;
    this.varName = oldTag ? oldTag.varName : args[0];
    this.iter = oldTag ? oldTag.iter : parseValue(args[1]);
    this.vars = [];

    this.children = oldTag ? copyArray(oldTag.children) : [];

    let offset = Math.min(args.length, 2);
    parseChildren(args, offset, this.vars, this.children);

    return creator(this, DomplateLoop);
  },

  getVarNames: function(names)
  {
    if (this.iter instanceof Parts)
      names.push(this.iter.parts[0].name);

    DomplateTag.prototype.getVarNames.apply(this, [names]);
  },

  generateMarkup: function(topBlock, topOuts, blocks, info)
  {
    this.addCode(topBlock, topOuts, blocks);

    let iterName;
    if (this.iter instanceof Parts) {
      let part = this.iter.parts[0];
      iterName = part.name;

      if (part.format) {
        for (let i = 0; i < part.format.length; ++i)
          iterName = part.format[i] + "(" + iterName + ")";
      }
    } else
      iterName = this.iter;

    blocks.push('__loop__.apply(this, [', iterName, ', __out__, function(', this.varName, ', __out__) {\n');
    this.generateChildMarkup(topBlock, topOuts, blocks, info);
    this.addCode(topBlock, topOuts, blocks);
    blocks.push('}]);\n');
  },

  generateDOM: function(path, blocks, args)
  {
    let iterName = 'd' + path.renderIndex++;
    let counterName = 'i' + path.loopIndex;
    let loopName = 'l' + path.loopIndex++;

    if (!path.length)
      path.push(-1, 0);

    let preIndex = path.renderIndex;
    path.renderIndex = 0;

    let nodeCount = 0;

    let subBlocks = [];
    let basePath = path[path.length-1];
    for (let i = 0; i < this.children.length; ++i) {
      path[path.length - 1] = basePath + '+' + loopName + '+' + nodeCount;

      let child = this.children[i];
      if (isTag(child))
        nodeCount += '+' + child.tag.generateDOM(path, subBlocks, args);
      else
        nodeCount += '+1';
    }

    path[path.length - 1] = basePath + '+' + loopName;

    blocks.push(loopName,' = __loop__.apply(this, [',
      iterName, ', function(', counterName, ',', loopName);
    for (let i = 0; i < path.renderIndex; ++i)
      blocks.push(',d' + i);
    blocks.push(') {\n');
    blocks.push(subBlocks.join(""));
    blocks.push('return ', nodeCount, ';\n');
    blocks.push('}]);\n');

    path.renderIndex = preIndex;

    return loopName;
  }
});

///////////////////////////////////////////////////////////////////////////

function Variable(name, format)
{
  this.name = name;
  this.format = format;
}

function Parts(parts)
{
  this.parts = parts;
}

///////////////////////////////////////////////////////////////////////////

function parseParts(str)
{
  let re = /\$([_A-Za-z][_A-Za-z0-9.|]*)/g;
  let index = 0;
  let parts = [];

  let m;
  while (m = re.exec(str)) {
    let pre = str.substr(index, (re.lastIndex-m[0].length)-index);
    if (pre)
      parts.push(pre);

    let expr = m[1].split("|");
    parts.push(new Variable(expr[0], expr.slice(1)));
    index = re.lastIndex;
  }

  if (!index)
    return str;

  let post = str.substr(index);
  if (post)
    parts.push(post);

  return new Parts(parts);
}

function parseValue(val)
{
  return typeof(val) == 'string' ? parseParts(val) : val;
}

function parseChildren(args, offset, vars, children)
{
  for (let i = offset; i < args.length; ++i) {
    let val = parseValue(args[i]);
    children.push(val);
    readPartNames(val, vars);
  }
}

function readPartNames(val, vars)
{
  if (val instanceof Parts) {
    for (let i = 0; i < val.parts.length; ++i) {
      let part = val.parts[i];
      if (part instanceof Variable)
        vars.push(part.name);
    }
  }
}

function generateArg(val, path, args)
{
  if (val instanceof Parts) {
    let vals = [];
    for (let i = 0; i < val.parts.length; ++i) {
      let part = val.parts[i];
      if (part instanceof Variable) {
        let varName = 'd' + path.renderIndex++;
        if (part.format) {
          for (let j = 0; j < part.format.length; ++j)
            varName = part.format[j] + '(' + varName + ')';
        }

        vals.push(varName);
      }
      else
        vals.push('"' + part.replace(/"/g, '\\"') + '"');
    }

    return vals.join('+');
  } else {
    args.push(val);
    return 's' + path.staticIndex++;
  }
}

function addParts(val, delim, block, info, escapeIt)
{
  let vals = [];
  if (val instanceof Parts) {
    for (let i = 0; i < val.parts.length; ++i) {
      let part = val.parts[i];
      if (part instanceof Variable) {
        let partName = part.name;
        if (part.format) {
          for (let j = 0; j < part.format.length; ++j)
            partName = part.format[j] + "(" + partName + ")";
        }

        if (escapeIt)
          vals.push("__escape__(" + partName + ")");
        else
          vals.push(partName);
      }
      else
        vals.push('"' + part + '"');
    }
  } else if (isTag(val)) {
    info.args.push(val);
    vals.push('s' + info.argIndex++);
  } else
    vals.push('"' + val + '"');

  let parts = vals.join(delim);
  if (parts)
    block.push(delim, parts);
}

function isTag(obj)
{
  return (typeof(obj) == "function" || obj instanceof Function) && !!obj.tag;
}

///////////////////////////////////////////////////////////////////////////
//// creator

function creator(tag, cons)
{
  let fn = new Function(
    "var tag = arguments.callee.tag;" +
    "var cons = arguments.callee.cons;" +
    "var newTag = new cons();" +
    "return newTag.merge(arguments, tag);");

  fn.tag = tag;
  fn.cons = cons;
  extend(fn, Renderer);

  return fn;
}

///////////////////////////////////////////////////////////////////////////
//// Utility functions

function arrayInsert(array, index, other)
{
  for (let i = 0; i < other.length; ++i)
    array.splice(i+index, 0, other[i]);

  return array;
}

function cloneArray(array, fn)
{
  let newArray = [];

  if (fn)
    for (let i = 0; i < array.length; ++i)
      newArray.push(fn(array[i]));
  else
    for (let i = 0; i < array.length; ++i)
      newArray.push(array[i]);

  return newArray;
}

// fn, thisObject, args => thisObject.fn(args, arguments);
function bind()
{
  let args = cloneArray(arguments), fn = args.shift(), object = args.shift();
  return function bind()
  {
    return fn.apply(object, arrayInsert(cloneArray(args), 0, arguments));
  }
}

function copyArray(oldArray)
{
  let array = [];
  if (oldArray)
    for (let i = 0; i < oldArray.length; ++i)
      array.push(oldArray[i]);
  return array;
}

function copyObject(l, r)
{
  let m = {};
  extend(m, l);
  extend(m, r);
  return m;
}

function escapeNewLines(value)
{
  return value.replace(/\r/gm, "\\r").replace(/\n/gm, "\\n");
}

function extend(l, r)
{
  for (let n in r)
    l[n] = r[n];
}

function cropString(text, limit, alterText)
{
  if (!alterText)
    alterText = "..."; //…

  text = text + "";

  if (!limit)
    limit = 88; // todo
  var halfLimit = (limit / 2);
  halfLimit -= 2; // adjustment for alterText's increase in size

  if (text.length > limit)
    return text.substr(0, halfLimit) + alterText +
      text.substr(text.length - halfLimit);
  else
    return text;
}

function cropMultipleLines(text, limit)
{
  return escapeNewLines(this.cropString(text, limit));
}

function isVisible(elt)
{
  if (elt.localName) {
    return elt.offsetWidth > 0 || elt.offsetHeight > 0 ||
      elt.localName.toLowerCase() in invisibleTags;
  } else {
    return elt.offsetWidth > 0 || elt.offsetHeight > 0;
  }
    // || isElementSVG(elt) || isElementMathML(elt);
}

// Local Helpers

function isElementXHTML(node)
{
  return node.nodeName == node.nodeName.toLowerCase();
}

function isContainerElement(element)
{
  let tag = element.localName.toLowerCase();
  switch (tag) {
    case "script":
    case "style":
    case "iframe":
    case "frame":
    case "tabbrowser":
    case "browser":
      return true;
    case "link":
      return element.getAttribute("rel") == "stylesheet";
    case "embed":
      return element.getSVGDocument();
  }
  return false;
}

domplateUtils.isWhitespace = function isWhitespace(text)
{
  return !reNotWhitespace.exec(text);
};

domplateUtils.isWhitespaceText = function isWhitespaceText(node)
{
  if (node instanceof DOM.HTMLAppletElement)
    return false;
  return node.nodeType == DOM.Node.TEXT_NODE && this.isWhitespace(node.nodeValue);
}

function isSelfClosing(element)
{
  //if (isElementSVG(element) || isElementMathML(element))
  //    return true;
  var tag = element.localName.toLowerCase();
  return (selfClosingTags.hasOwnProperty(tag));
};

function isEmptyElement(element)
{
  if (showTextNodesWithWhitespace) {
    return !element.firstChild && isSelfClosing(element);
  } else {
    for (let child = element.firstChild; child; child = child.nextSibling) {
      if (!domplateUtils.isWhitespaceText(child))
        return false;
    }
  }
  return isSelfClosing(element);
}

function getEmptyElementTag(node)
{
  let isXhtml= isElementXHTML(node);
  if (isXhtml)
    return HTMLTemplates.XEmptyElement.tag;
  else
    return HTMLTemplates.EmptyElement.tag;
}

/**
 * Determines if the given node has any children which are elements.
 *
 * @param {Element} element Element to test.
 * @return true if immediate children of type Element exist, false otherwise
 */
function hasNoElementChildren(element)
{
  if (element.childElementCount != 0)
    return false;

  return true;
}

domplateUtils.getNodeTag = function getNodeTag(node, expandAll)
{
  if (node instanceof DOM.Element) {
    if (node instanceof DOM.HTMLHtmlElement && node.ownerDocument
        && node.ownerDocument.doctype)
      return HTMLTemplates.HTMLHtmlElement.tag;
    else if (node instanceof DOM.HTMLAppletElement)
      return getEmptyElementTag(node);
    else if (isContainerElement(node))
      return HTMLTemplates.Element.tag;
    else if (isEmptyElement(node))
      return getEmptyElementTag(node);
    else if (hasNoElementChildren(node))
      return HTMLTemplates.TextElement.tag;
    else
      return HTMLTemplates.Element.tag;
  }
  else if (node instanceof DOM.Text)
    return HTMLTemplates.TextNode.tag;
  else if (node instanceof DOM.CDATASection)
    return HTMLTemplates.CDATANode.tag;
  else if (node instanceof DOM.Comment)
    return HTMLTemplates.CommentNode.tag;
  else if (node instanceof DOM.SourceText)
    return HTMLTemplates.SourceText.tag;
  else
    return HTMLTemplates.Nada.tag;
}

function getNodeBoxTag(nodeBox)
{
  let re = /([^\s]+)NodeBox/;
  let m = re.exec(nodeBox.className);
  if (!m)
    return null;

  let nodeBoxType = m[1];
  if (nodeBoxType == "container")
    return HTMLTemplates.Element.tag;
  else if (nodeBoxType == "text")
    return HTMLTemplates.TextElement.tag;
  else if (nodeBoxType == "empty")
    return HTMLTemplates.EmptyElement.tag;
}

///////////////////////////////////////////////////////////////////////////
//// ArrayIterator

function ArrayIterator(array)
{
  let index = -1;

  this.next = function()
  {
    if (++index >= array.length)
      throw StopIteration;

    return array[index];
  };
}

function StopIteration() {}

domplate.$break = function()
{
  throw StopIteration;
};

///////////////////////////////////////////////////////////////////////////
//// Renderer

var Renderer =
{
  renderHTML: function(args, outputs, self)
  {
    let code = [];
    let markupArgs = [code, this.tag.getContext(), args, outputs];
    markupArgs.push.apply(markupArgs, this.tag.markupArgs);
    this.tag.renderMarkup.apply(self ? self : this.tag.subject, markupArgs);
    return code.join("");
  },

  insertRows: function(args, before, self)
  {
    if (!args)
      args = {};

    this.tag.compile();

    let outputs = [];
    let html = this.renderHTML(args, outputs, self);

    let doc = before.ownerDocument;
    let table = doc.createElement("table");
    table.innerHTML = html;

    let tbody = table.firstChild;
    let parent = before.localName.toLowerCase() == "tr" ? before.parentNode : before;
    let after = before.localName.toLowerCase() == "tr" ? before.nextSibling : null;

    let firstRow = tbody.firstChild, lastRow;
    while (tbody.firstChild) {
      lastRow = tbody.firstChild;
      if (after)
        parent.insertBefore(lastRow, after);
      else
        parent.appendChild(lastRow);
    }

    // To save the next poor soul:
    // In order to properly apply properties and event handlers on elements
    // constructed by a FOR tag, the tag needs to be able to iterate up and
    // down the tree, meaning if FOR is the root element as is the case with
    // many insertRows calls, it will need to iterator over portions of the
    // new parent.
    //
    // To achieve this end, __path__ defines the -1 operator which allows
    // parent traversal. When combined with the offset that we calculate
    // below we are able to iterate over the elements.
    //
    // This fails when applied to a non-loop element as non-loop elements
    // Do not generate to proper path to bounce up and down the tree.
    let offset = 0;
    if (this.tag.isLoop) {
      let node = firstRow.parentNode.firstChild;
      for (; node && node != firstRow; node = node.nextSibling)
        ++offset;
    }

    // strict warning: this.tag.context undefined
    let domArgs = [firstRow, this.tag.getContext(), offset];
    domArgs.push.apply(domArgs, this.tag.domArgs);
    domArgs.push.apply(domArgs, outputs);

    this.tag.renderDOM.apply(self ? self : this.tag.subject, domArgs);
    return [firstRow, lastRow];
  },

  insertBefore: function(args, before, self)
  {
    return this.insertNode(args, before.ownerDocument,
      function(frag) {
        before.parentNode.insertBefore(frag, before);
      }, self);
  },

  insertAfter: function(args, after, self)
  {
    return this.insertNode(args, after.ownerDocument,
      function(frag) {
        after.parentNode.insertBefore(frag, after.nextSibling);
      }, self);
  },

  insertNode: function(args, doc, inserter, self)
  {
    if (!args)
      args = {};

    this.tag.compile();

    let outputs = [];
    let html = this.renderHTML(args, outputs, self);

    let range = doc.createRange();
    range.selectNode(doc.body);
    let frag = range.createContextualFragment(html);

    let root = frag.firstChild;
    root = inserter(frag) || root;

    let domArgs = [root, this.tag.context, 0];
    domArgs.push.apply(domArgs, this.tag.domArgs);
    domArgs.push.apply(domArgs, outputs);

    this.tag.renderDOM.apply(self ? self : this.tag.subject, domArgs);

    return root;
  },

  replace: function(args, parent, self)
  {
    if (!args)
      args = {};

    this.tag.compile();

    let outputs = [];
    let html = this.renderHTML(args, outputs, self);

    let root;
    if (parent.nodeType == DOM.Node.ELEMENT_NODE) {
      parent.innerHTML = html;
      root = parent.firstChild;
    } else {
      if (!parent || parent.nodeType != DOM.Node.DOCUMENT_NODE)
        return;

      if (!womb || womb.ownerDocument != parent)
        womb = parent.createElement("div");

      womb.innerHTML = html;

      root = womb.firstChild;
    }

    let domArgs = [root, this.tag.context, 0];
    domArgs.push.apply(domArgs, this.tag.domArgs);
    domArgs.push.apply(domArgs, outputs);
    this.tag.renderDOM.apply(self ? self : this.tag.subject, domArgs);

    return root;
  },

  append: function(args, parent, self)
  {
    if (!args)
      args = {};

    this.tag.compile();

    let outputs = [];
    let html = this.renderHTML(args, outputs, self);

    if (!womb || womb.ownerDocument != parent.ownerDocument)
      womb = parent.ownerDocument.createElement("div");
    womb.innerHTML = html;

    let root = womb.firstChild;
    while (womb.firstChild)
      parent.appendChild(womb.firstChild);

    let domArgs = [root, this.tag.context, 0];
    domArgs.push.apply(domArgs, this.tag.domArgs);
    domArgs.push.apply(domArgs, outputs);

    this.tag.renderDOM.apply(self ? self : this.tag.subject, domArgs);

    return root;
  }
};

///////////////////////////////////////////////////////////////////////////
//// defineTags macro

/**
 * Create default tags for a list of tag names.
 * @param Arguments
 *        list of string arguments
 */

function defineTags()
{
  for (let i = 0; i < arguments.length; ++i) {
    let tagName = arguments[i];
    let fn = new Function("var newTag = new DomplateTag('" + tagName +
      "'); return newTag.merge(arguments);");

    let fnName = tagName.toUpperCase();
    domplate[fnName] = fn;
  }
}

defineTags(
  "a", "button", "br", "canvas", "col", "colgroup", "div", "fieldset", "form",
  "h1", "h2", "h3", "hr", "img", "input", "label", "legend", "li", "ol",
  "optgroup", "option", "p", "pre", "select", "b", "span", "strong", "table",
  "tbody", "td", "textarea", "tfoot", "th", "thead", "tr", "tt", "ul", "iframe",
  "code"
);

///////////////////////////////////////////////////////////////////////////
//// HTMLTemplates

let HTMLTemplates = {
  showTextNodesWithWhitespace: false
};

let BaseTemplates = {
  showTextNodesWithWhitespace: false
};

///////////////////////////////////////////////////////////////////////////
//// HTMLTemplates.Reps

BaseTemplates.OBJECTLINK = domplate.A({
  "class": "objectLink objectLink-$className a11yFocus",
  _repObject: "$object"
});

BaseTemplates.Rep = domplate(
{
  className: "",
  inspectable: true,

  supportsObject: function(object, type)
  {
    return false;
  },

  inspectObject: function(object, context)
  {
    // Firebug.chrome.select(object);  // todo
  },

  browseObject: function(object, context)
  {
  },

  persistObject: function(object, context)
  {
  },

  getRealObject: function(object, context)
  {
    return object;
  },

  /**
   * Return a sensible string title for the given object, removing any wrapper
   * information from it.
   * @param aObject
   *        The object to get the title of.
   * @returns string
   */

  getTitle: function(aObject)
  {
    // e.g., [object XPCWrappedNative [object foo]]
    let label = safeToString(aObject);

    const re =/\[object ([^\]]*)/;
    let objectMatch = re.exec(label);
    let secondObjectMatch = null;
    if (objectMatch) {
      // e.g., XPCWrappedNative [object foo
      secondObjectMatch = re.exec(objectMatch[1]);
    }

    if (secondObjectMatch)
      return secondObjectMatch[1];  // eg foo
    else
      return objectMatch ? objectMatch[1] : label;
  },

  getTooltip: function(object)
  {
    return null;
  },

  /**
   * Called by chrome.onContextMenu to build the context menu when the
   * underlying object has this rep.
   * See also Panel for a similar function also called by onContextMenu.
   * Extensions may monkey patch and chain off this call.
   * @param object: the 'realObject', a model value, eg a DOM property
   * @param target: the HTML element clicked on.
   * @param context: the context, probably FirebugContext
   * @returns an array of menu items.
   */
  getContextMenuItems: function(object, target, context)
  {
    return [];
  },

  /////////////////////////////////////////////////////////////////////////
  // Convenience for domplates

  STR: function(name)
  {
    return name; // todo getproperty?
  },

  cropString: function(text)
  {
    return cropString(text);
  },

  cropMultipleLines: function(text, limit)
  {
    return cropMultipleLines(text, limit);
  },

  toLowerCase: function(text)
  {
    return text ? text.toLowerCase() : text;
  },

  plural: function(n)
  {
    return n == 1 ? "" : "s";
  }
});

BaseTemplates.Element = domplate(BaseTemplates.Rep,
{
  tag:
    BaseTemplates.OBJECTLINK(
      "&lt;",
      domplate.SPAN({"class": "nodeTag"},
        "$object.localName|toLowerCase"),
      domplate.FOR("attr", "$object|attrIterator",
        "&nbsp;$attr.localName=&quot;",
        domplate.SPAN({"class": "nodeValue"},
          "$attr.nodeValue"),
        "&quot;"
      ),
      "&gt;"
    ),

  shortTag:
    BaseTemplates.OBJECTLINK(
      domplate.SPAN({"class": "$object|getVisible"},
        domplate.SPAN({"class": "selectorTag"},
          "$object|getSelectorTag"),
        domplate.SPAN({"class": "selectorId"},
          "$object|getSelectorId"),
        domplate.SPAN({"class": "selectorClass"},
          "$object|getSelectorClass"),
        domplate.SPAN({"class": "selectorValue"},
          "$object|getValue")
      )
    ),

  getVisible: function(elt)
  {
    return isVisible(elt) ? "" : "selectorHidden";
  },

  getSelectorTag: function(elt)
  {
    return elt.localName.toLowerCase();
  },

  getSelectorId: function(elt)
  {
    return elt.id ? ("#" + elt.id) : "";
  },

  getSelectorClass: function(elt)
  {
    return elt.getAttribute("class")
      ? ("." + elt.getAttribute("class").split(" ")[0])
      : "";
  },

  getValue: function(elt)
  { // todo getFileName
    let value;
/*
    if (elt instanceof HTMLImageElement)
      value = getFileName(elt.getAttribute("src"));
    else if (elt instanceof HTMLAnchorElement)
      value = getFileName(elt.getAttribute("href"));
    else if (elt instanceof HTMLInputElement)
      value = elt.getAttribute("value");
    else if (elt instanceof HTMLFormElement)
      value = getFileName(elt.getAttribute("action"));
    else if (elt instanceof HTMLScriptElement)
      value = getFileName(elt.getAttribute("src"));

    return value ? " " + cropMultipleLines(value, 20) : ""; */
    // trying a simplified version from above commented section
    // todo
    if (elt instanceof DOM.HTMLImageElement)
      value = elt.getAttribute("src");
    else if (elt instanceof DOM.HTMLAnchorElement)
      value = elt.getAttribute("href");
    else if (elt instanceof DOM.HTMLInputElement)
      value = elt.getAttribute("value");
    else if (elt instanceof DOM.HTMLFormElement)
      value = elt.getAttribute("action");
    else if (elt instanceof DOM.HTMLScriptElement)
      value = elt.getAttribute("src");

    return value ? " " + cropMultipleLines(value, 20) : "";
  },

  attrIterator: function(elt)
  {
    let attrs = [];
    let idAttr, classAttr;
    if (elt.attributes) {
      for (let i = 0; i < elt.attributes.length; ++i) {
        var attr = elt.attributes[i];
        if (attr.localName.indexOf("-moz-math") != -1)
          continue;
        else if (attr.localName == "id")
          idAttr = attr;
        else if (attr.localName == "class")
          classAttr = attr;
        else
          attrs.push(attr);
      }
    }
    if (classAttr)
      attrs.unshift(classAttr);
    if (idAttr)
      attrs.unshift(idAttr);
    return attrs;
  },

  shortAttrIterator: function(elt)
  {
    let attrs = [];
    if (elt.attributes) {
      for (let i = 0; i < elt.attributes.length; ++i) {
        let attr = elt.attributes[i];
          if (attr.localName == "id" || attr.localName == "class")
            attrs.push(attr);
      }
    }

    return attrs;
  },

  getHidden: function(elt)
  {
    return isVisible(elt) ? "" : "nodeHidden";
  },

/* getXPath: function(elt)
  {
    return getElementTreeXPath(elt); // todo
  }, */

  getNodeTextGroups: function(element)
  {
    let text =  element.textContent;
    return [{str: text, 'class': '', extra: ''}];
  },

  className: "element",

  supportsObject: function(object, type)
  {
    return object instanceof DOM.Element;
  },

  browseObject: function(elt, context)
  {
    let tag = elt.localName.toLowerCase();
    return true;
  },
});


///////////////////////////////////////////////////////////////////////////
//// HTMLTemplates.tags

BaseTemplates.AttrTag =
  domplate.SPAN({"class": "nodeAttr editGroup"},
    "&nbsp;",
    domplate.SPAN({"class": "nodeName editable"}, "$attr.nodeName"),
    "=&quot;",
    domplate.SPAN({"class": "nodeValue editable"}, "$attr.nodeValue"),
    "&quot;");

BaseTemplates.TextTag =
  domplate.SPAN({"class": "nodeText editable"},
    domplate.FOR("chr", "$object|getNodeTextGroups",
      domplate.SPAN({"class": "$chr.class $chr.extra"},
        "$chr.str")));

///////////////////////////////////////////////////////////////////////////
//// HTMLTemplates



HTMLTemplates.CompleteElement = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class":
        "nodeBox open $object|getHidden repIgnore",
        _repObject: "$object", role : 'presentation'},
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.SPAN({"class": "nodeLabelBox repTarget repTarget",
          role : 'treeitem', 'aria-expanded' : 'false'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class": "nodeBracket"}, "&gt;")
        )
      ),
      domplate.DIV({"class": "nodeChildBox", role :"group"},
        domplate.FOR("child", "$object|childIterator",
          domplate.TAG("$child|getNodeTag", {object: "$child"})
        )
      ),
      domplate.DIV({"class": "nodeCloseLabel", role:"presentation"},
        "&lt;/",
        domplate.SPAN({"class": "nodeTag"},
          "$object.nodeName|toLowerCase"),
        "&gt;"
      )
    ),

  getNodeTag: function(node)
  {
    return domplateUtils.getNodeTag(node, true);
  },

  childIterator: function(node)
  {
    if (node.contentDocument)
      return [node.contentDocument.documentElement];

    if (this.showTextNodesWithWhitespace)
      return cloneArray(node.childNodes);
    else {
      let nodes = [];
      for (let child = node.firstChild; child; child = child.nextSibling) {
        if (child.nodeType != DOM.Node.TEXT_NODE || !domplateUtils.isWhitespaceText(child))
          nodes.push(child);
      }
      return nodes;
    }
  }
});

HTMLTemplates.SoloElement = domplate(HTMLTemplates.CompleteElement,
{
  tag:
    domplate.DIV({"class": "soloElement",
      onmousedown: "$onMouseDown"},
      HTMLTemplates.CompleteElement.tag),

  onMouseDown: function(event)
  {
    for (let child = event.target; child; child = child.parentNode) {
      if (child.repObject) { // todo
          // let panel = Firebug.getElementPanel(child);
          // Firebug.chrome.select(child.repObject);
          break;
      }
    }
  }
});

HTMLTemplates.Element = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class": "nodeBox containerNodeBox $object|getHidden repIgnore",
      _repObject: "$object", role: "presentation"},
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.IMG({"class": "twisty", role: "presentation"}),
        domplate.SPAN({"class": "nodeLabelBox repTarget",
          role: 'treeitem', 'aria-expanded': 'false'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class": "nodeBracket editable insertBefore"},
            "&gt;")
        )
      ),
      domplate.DIV({"class": "nodeChildBox", role: "group"}), /* nodeChildBox is special signal in insideOutBox */
      domplate.DIV({"class": "nodeCloseLabel", role: "presentation"},
        domplate.SPAN({"class": "nodeCloseLabelBox repTarget"},
          "&lt;/",
          domplate.SPAN({"class": "nodeTag"}, "$object.nodeName|toLowerCase"),
          "&gt;"
        )
      )
    )
});

HTMLTemplates.HTMLHtmlElement = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class":
        "nodeBox htmlNodeBox containerNodeBox $object|getHidden repIgnore",
        _repObject: "$object", role: "presentation"},
      domplate.DIV({"class": "docType"},
        "$object|getDocType"),
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.IMG({"class": "twisty", role: "presentation"}),
        domplate.SPAN({"class": "nodeLabelBox repTarget",
            role: 'treeitem', 'aria-expanded' : 'false'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class":
            "nodeBracket editable insertBefore"}, "&gt;")
        )
      ), /* nodeChildBox is special signal in insideOutBox */
      domplate.DIV({"class": "nodeChildBox", role: "group"}),
      domplate.DIV({"class": "nodeCloseLabel", role:  "presentation"},
        domplate.SPAN({"class": "nodeCloseLabelBox repTarget"},
          "&lt;/",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          "&gt;"
        )
      )
    ),

  getDocType: function(obj)
  {
    let doctype = obj.ownerDocument.doctype;
    return '<!DOCTYPE ' + doctype.name + (doctype.publicId ? ' PUBLIC "' +
      doctype.publicId + '"': '') + (doctype.systemId ? ' "' +
      doctype.systemId + '"' : '') + '>';
  }
});

HTMLTemplates.TextElement = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class":
        "nodeBox textNodeBox $object|getHidden repIgnore",
        _repObject: "$object", role: 'presentation'},
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.SPAN({"class": "nodeLabelBox repTarget",
            role: 'treeitem'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class":
            "nodeBracket editable insertBefore"}, "&gt;"),
          BaseTemplates.TextTag,
          "&lt;/",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          "&gt;"
        )
      )
    )
});

HTMLTemplates.EmptyElement = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class":
        "nodeBox emptyNodeBox $object|getHidden repIgnore",
        _repObject: "$object", role: 'presentation'},
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.SPAN({"class": "nodeLabelBox repTarget",
            role: 'treeitem'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class":
            "nodeBracket editable insertBefore"}, "&gt;")
        )
      )
    )
});

HTMLTemplates.XEmptyElement = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class":
        "nodeBox emptyNodeBox $object|getHidden repIgnore",
        _repObject: "$object", role: 'presentation'},
      domplate.DIV({"class": "nodeLabel", role: "presentation"},
        domplate.SPAN({"class": "nodeLabelBox repTarget",
            role : 'treeitem'},
          "&lt;",
          domplate.SPAN({"class": "nodeTag"},
            "$object.nodeName|toLowerCase"),
          domplate.FOR("attr", "$object|attrIterator", BaseTemplates.AttrTag),
          domplate.SPAN({"class":
            "nodeBracket editable insertBefore"}, "/&gt;")
        )
      )
    )
});

HTMLTemplates.AttrNode = domplate(BaseTemplates.Element,
{
  tag: BaseTemplates.AttrTag
});

HTMLTemplates.TextNode = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class": "nodeBox", _repObject: "$object",
      role: 'presentation'}, BaseTemplates.TextTag)
});

HTMLTemplates.CDATANode = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class": "nodeBox", _repObject: "$object",
      role: 'presentation'},
        "&lt;![CDATA[",
        domplate.SPAN({"class": "nodeText nodeCDATA editable"},
          "$object.nodeValue"),
        "]]&gt;")
});

HTMLTemplates.CommentNode = domplate(BaseTemplates.Element,
{
  tag:
    domplate.DIV({"class": "nodeBox nodeComment",
        _repObject: "$object", role : 'presentation'},
      "&lt;!--",
      domplate.SPAN({"class": "nodeComment editable"},
        "$object.nodeValue"),
      "--&gt;")
});

HTMLTemplates.Nada = domplate(BaseTemplates.Rep,
{
  tag: domplate.SPAN(""),
  className: "nada"
});

