/**
 * The MIT License (MIT)
 *
 * Copyright (c) 2014 Gabriel Llamas
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

"use strict";

var hex = function (c){
  switch (c){
    case "0": return 0;
    case "1": return 1;
    case "2": return 2;
    case "3": return 3;
    case "4": return 4;
    case "5": return 5;
    case "6": return 6;
    case "7": return 7;
    case "8": return 8;
    case "9": return 9;
    case "a": case "A": return 10;
    case "b": case "B": return 11;
    case "c": case "C": return 12;
    case "d": case "D": return 13;
    case "e": case "E": return 14;
    case "f": case "F": return 15;
  }
};

var parse = function (data, options, handlers, control){
  var c;
  var code;
  var escape;
  var skipSpace = true;
  var isCommentLine;
  var isSectionLine;
  var newLine = true;
  var multiLine;
  var isKey = true;
  var key = "";
  var value = "";
  var section;
  var unicode;
  var unicodeRemaining;
  var escapingUnicode;
  var keySpace;
  var sep;
  var ignoreLine;

  var line = function (){
    if (key || value || sep){
      handlers.line (key, value);
      key = "";
      value = "";
      sep = false;
    }
  };

  var escapeString = function (key, c, code){
    if (escapingUnicode && unicodeRemaining){
      unicode = (unicode << 4) + hex (c);
      if (--unicodeRemaining) return key;
      escape = false;
      escapingUnicode = false;
      return key + String.fromCharCode (unicode);
    }

    //code 117: u
    if (code === 117){
      unicode = 0;
      escapingUnicode = true;
      unicodeRemaining = 4;
      return key;
    }

    escape = false;

    //code 116: t
    //code 114: r
    //code 110: n
    //code 102: f
    if (code === 116) return key + "\t";
    else if (code === 114) return key + "\r";
    else if (code === 110) return key + "\n";
    else if (code === 102) return key + "\f";

    return key + c;
  };

  var isComment;
  var isSeparator;

  if (options._strict){
    isComment = function (c, code, options){
      return options._comments[c];
    };

    isSeparator = function (c, code, options){
      return options._separators[c];
    };
  }else{
    isComment = function (c, code, options){
      //code 35: #
      //code 33: !
      return code === 35 || code === 33 || options._comments[c];
    };

    isSeparator = function (c, code, options){
      //code 61: =
      //code 58: :
      return code === 61 || code === 58 || options._separators[c];
    };
  }

  for (var i=~~control.resume; i<data.length; i++){
    if (control.abort) return;
    if (control.pause){
      //The next index is always the start of a new line, it's a like a fresh
      //start, there's no need to save the current state
      control.resume = i;
      return;
    }

    c = data[i];
    code = data.charCodeAt (i);

    //code 13: \r
    if (code === 13) continue;

    if (isCommentLine){
      //code 10: \n
      if (code === 10){
        isCommentLine = false;
        newLine = true;
        skipSpace = true;
      }
      continue;
    }

    //code 93: ]
    if (isSectionLine && code === 93){
      handlers.section (section);
      //Ignore chars after the section in the same line
      ignoreLine = true;
      continue;
    }

    if (skipSpace){
      //code 32: " " (space)
      //code 9: \t
      //code 12: \f
      if (code === 32 || code === 9 || code === 12){
        continue;
      }
      //code 10: \n
      if (!multiLine && code === 10){
        //Empty line or key w/ separator and w/o value
        isKey = true;
        keySpace = false;
        newLine = true;
        line ();
        continue;
      }
      skipSpace = false;
      multiLine = false;
    }

    if (newLine){
      newLine = false;
      if (isComment (c, code, options)){
        isCommentLine = true;
        continue;
      }
      //code 91: [
      if (options.sections && code === 91){
        section = "";
        isSectionLine = true;
        control.skipSection = false;
        continue;
      }
    }

    //code 10: \n
    if (code !== 10){
      if (control.skipSection || ignoreLine) continue;

      if (!isSectionLine){
        if (!escape && isKey && isSeparator (c, code, options)){
          //sep is needed to detect empty key and empty value with a
          //non-whitespace separator
          sep = true;
          isKey = false;
          keySpace = false;
          //Skip whitespace between separator and value
          skipSpace = true;
          continue;
        }
      }

      //code 92: "\" (backslash)
      if (code === 92){
        if (escape){
          if (escapingUnicode) continue;

          if (keySpace){
            //Line with whitespace separator
            keySpace = false;
            isKey = false;
          }

          if (isSectionLine) section += "\\";
          else if (isKey) key += "\\";
          else value += "\\";
        }
        escape = !escape;
      }else{
        if (keySpace){
          //Line with whitespace separator
          keySpace = false;
          isKey = false;
        }

        if (isSectionLine){
          if (escape) section = escapeString (section, c, code);
          else section += c;
        }else if (isKey){
          if (escape){
            key = escapeString (key, c, code);
          }else{
            //code 32: " " (space)
            //code 9: \t
            //code 12: \f
            if (code === 32 || code === 9 || code === 12){
              keySpace = true;
              //Skip whitespace between key and separator
              skipSpace = true;
              continue;
            }
            key += c;
          }
        }else{
          if (escape) value = escapeString (value, c, code);
          else value += c;
        }
      }
    }else{
      if (escape){
        if (!escapingUnicode){
          escape = false;
        }
        skipSpace = true;
        multiLine = true;
      }else{
        if (isSectionLine){
          isSectionLine = false;
          if (!ignoreLine){
            //The section doesn't end with ], it's a key
            control.error = new Error ("The section line \"" + section +
                "\" must end with \"]\"");
            return;
          }
          ignoreLine = false;
        }
        newLine = true;
        skipSpace = true;
        isKey = true;

        line ();
      }
    }
  }

  control.parsed = true;

  if (isSectionLine && !ignoreLine){
    //The section doesn't end with ], it's a key
    control.error = new Error ("The section line \"" + section + "\" must end" +
        "with \"]\"");
    return;
  }
  line ();
};

var INCLUDE_KEY = "include";
var INDEX_FILE = "index.properties";

var cast = function (value){
  if (value === null || value === "null") return null;
  if (value === "undefined") return undefined;
  if (value === "true") return true;
  if (value === "false") return false;
  var v = Number (value);
  return isNaN (v) ? value : v;
};

var expand = function  (o, str, options, cb){
  if (!options.variables || !str) return cb (null, str);

  var stack = [];
  var c;
  var cp;
  var key = "";
  var section = null;
  var v;
  var holder;
  var t;
  var n;

  for (var i=0; i<str.length; i++){
    c = str[i];

    if (cp === "$" && c === "{"){
      key = key.substring (0, key.length - 1);
      stack.push ({
        key: key,
        section: section
      });
      key = "";
      section = null;
      continue;
    }else if (stack.length){
      if (options.sections && c === "|"){
        section = key;
        key = "";
        continue;
      }else if (c === "}"){
        holder = section !== null ? searchValue (o, section, true) : o;
        if (!holder){
          return cb (new Error ("The section \"" + section + "\" does not " +
              "exist"));
        }

        v = options.namespaces ? searchValue (holder, key) : holder[key];
        if (v === undefined){
          //Read the external vars
          v = options.namespaces
              ? searchValue (options._vars, key)
              : options._vars[key]

          if (v === undefined){
            return cb (new Error ("The property \"" + key + "\" does not " +
                "exist"));
          }
        }

        t = stack.pop ();
        section = t.section;
        key = t.key + (v === null ? "" : v);
        continue;
      }
    }

    cp = c;
    key += c;
  }

  if (stack.length !== 0){
    return cb (new Error ("Malformed variable: " + str));
  }

  cb (null, key);
};

var searchValue = function (o, chain, section){
  var n = chain.split (".");
  var str;

  for (var i=0; i<n.length-1; i++){
    str = n[i];
    if (o[str] === undefined) return;
    o = o[str];
  }

  var v = o[n[n.length - 1]];
  if (section){
    if (typeof v !== "object") return;
    return v;
  }else{
    if (typeof v === "object") return;
    return v;
  }
};

var namespaceKey = function (o, key, value){
  var n = key.split (".");
  var str;

  for (var i=0; i<n.length-1; i++){
    str = n[i];
    if (o[str] === undefined){
      o[str] = {};
    }else if (typeof o[str] !== "object"){
      throw new Error ("Invalid namespace chain in the property name '" +
          key + "' ('" + str + "' has already a value)");
    }
    o = o[str];
  }

  o[n[n.length - 1]] = value;
};

var namespaceSection = function (o, section){
  var n = section.split (".");
  var str;

  for (var i=0; i<n.length; i++){
    str = n[i];
    if (o[str] === undefined){
      o[str] = {};
    }else if (typeof o[str] !== "object"){
      throw new Error ("Invalid namespace chain in the section name '" +
          section + "' ('" + str + "' has already a value)");
    }
    o = o[str];
  }

  return o;
};

var merge = function (o1, o2){
  for (var p in o2){
    try{
      if (o1[p].constructor === Object){
        o1[p] = merge (o1[p], o2[p]);
      }else{
        o1[p] = o2[p];
      }
    }catch (e){
      o1[p] = o2[p];
    }
  }
  return o1;
}

var build = function (data, options, dirname, cb){
  var o = {};

  if (options.namespaces){
    var n = {};
  }

  var control = {
    abort: false,
    skipSection: false
  };

  if (options.include){
    var remainingIncluded = 0;

    var include = function (value){
      if (currentSection !== null){
        return abort (new Error ("Cannot include files from inside a " +
            "section: " + currentSection));
      }

      var p = path.resolve (dirname, value);
      if (options._included[p]) return;

      options._included[p] = true;
      remainingIncluded++;
      control.pause = true;

      read (p, options, function (error, included){
        if (error) return abort (error);

        remainingIncluded--;
        merge (options.namespaces ? n : o, included);
        control.pause = false;

        if (!control.parsed){
          parse (data, options, handlers, control);
          if (control.error) return abort (control.error);
        }

        if (!remainingIncluded) cb (null, options.namespaces ? n : o);
      });
    };
  }

  if (!data){
    if (cb) return cb (null, o);
    return o;
  }

  var currentSection = null;
  var currentSectionStr = null;

  var abort = function (error){
    control.abort = true;
    if (cb) return cb (error);
    throw error;
  };

  var handlers = {};
  var reviver = {
    assert: function (){
      return this.isProperty ? reviverLine.value : true;
    }
  };
  var reviverLine = {};

  //Line handler
  //For speed reasons, if "namespaces" is enabled, the old object is still
  //populated, e.g.: ${a.b} reads the "a.b" property from { "a.b": 1 }, instead
  //of having a unique object { a: { b: 1 } } which is slower to search for
  //the "a.b" value
  //If "a.b" is not found, then the external vars are read. If "namespaces" is
  //enabled, the var "a.b" is split and it searches the a.b value. If it is not
  //enabled, then the var "a.b" searches the "a.b" value

  var line;
  var error;

  if (options.reviver){
    if (options.sections){
      line = function (key, value){
        if (options.include && key === INCLUDE_KEY) return include (value);

        reviverLine.value = value;
        reviver.isProperty = true;
        reviver.isSection = false;

        value = options.reviver.call (reviver, key, value, currentSectionStr);
        if (value !== undefined){
          if (options.namespaces){
            try{
              namespaceKey (currentSection === null ? n : currentSection,
                  key, value);
            }catch (error){
              abort (error);
            }
          }else{
            if (currentSection === null) o[key] = value;
            else currentSection[key] = value;
          }
        }
      };
    }else{
      line = function (key, value){
        if (options.include && key === INCLUDE_KEY) return include (value);

        reviverLine.value = value;
        reviver.isProperty = true;
        reviver.isSection = false;

        value = options.reviver.call (reviver, key, value);
        if (value !== undefined){
          if (options.namespaces){
            try{
              namespaceKey (n, key, value);
            }catch (error){
              abort (error);
            }
          }else{
            o[key] = value;
          }
        }
      };
    }
  }else{
    if (options.sections){
      line = function (key, value){
        if (options.include && key === INCLUDE_KEY) return include (value);

        if (options.namespaces){
          try{
            namespaceKey (currentSection === null ? n : currentSection, key,
                value);
          }catch (error){
            abort (error);
          }
        }else{
          if (currentSection === null) o[key] = value;
          else currentSection[key] = value;
        }
      };
    }else{
      line = function (key, value){
        if (options.include && key === INCLUDE_KEY) return include (value);

        if (options.namespaces){
          try{
            namespaceKey (n, key, value);
          }catch (error){
            abort (error);
          }
        }else{
          o[key] = value;
        }
      };
    }
  }

  //Section handler
  var section;
  if (options.sections){
    if (options.reviver){
      section = function (section){
        currentSectionStr = section;
        reviverLine.section = section;
        reviver.isProperty = false;
        reviver.isSection = true;

        var add = options.reviver.call (reviver, null, null, section);
        if (add){
          if (options.namespaces){
            try{
              currentSection = namespaceSection (n, section);
            }catch (error){
              abort (error);
            }
          }else{
            currentSection = o[section] = {};
          }
        }else{
          control.skipSection = true;
        }
      };
    }else{
      section = function (section){
        currentSectionStr = section;
        if (options.namespaces){
          try{
            currentSection = namespaceSection (n, section);
          }catch (error){
            abort (error);
          }
        }else{
          currentSection = o[section] = {};
        }
      };
    }
  }

  //Variables
  if (options.variables){
    handlers.line = function (key, value){
      expand (options.namespaces ? n : o, key, options, function (error, key){
        if (error) return abort (error);

        expand (options.namespaces ? n : o, value, options,
            function (error, value){
          if (error) return abort (error);

          line (key, cast (value || null));
        });
      });
    };

    if (options.sections){
      handlers.section = function (s){
        expand (options.namespaces ? n : o, s, options, function (error, s){
          if (error) return abort (error);

          section (s);
        });
      };
    }
  }else{
    handlers.line = function (key, value){
      line (key, cast (value || null));
    };

    if (options.sections){
      handlers.section = section;
    }
  }

  parse (data, options, handlers, control);
  if (control.error) return abort (control.error);

  if (control.abort || control.pause) return;

  if (cb) return cb (null, options.namespaces ? n : o);
  return options.namespaces ? n : o;
};

var read = function (f, options, cb){
  fs.stat (f, function (error, stats){
    if (error) return cb (error);

    var dirname;

    if (stats.isDirectory ()){
      dirname = f;
      f = path.join (f, INDEX_FILE);
    }else{
      dirname = path.dirname (f);
    }

    fs.readFile (f, { encoding: "utf8" }, function (error, data){
      if (error) return cb (error);
      build (data, options, dirname, cb);
    });
  });
};

module.exports = function (data, options, cb){
  if (typeof options === "function"){
    cb = options;
    options = {};
  }

  options = options || {};
  var code;

  if (options.include){
    if (!cb) throw new Error ("A callback must be passed if the 'include' " +
        "option is enabled");
    options._included = {};
  }

  options = options || {};
  options._strict = options.strict && (options.comments || options.separators);
  options._vars = options.vars || {};

  var comments = options.comments || [];
  if (!Array.isArray (comments)) comments = [comments];
  var c = {};
  comments.forEach (function (comment){
    code = comment.charCodeAt (0);
    if (comment.length > 1 || code < 33 || code > 126){
      throw new Error ("The comment token must be a single printable ASCII " +
          "character");
    }
    c[comment] = true;
  });
  options._comments = c;

  var separators = options.separators || [];
  if (!Array.isArray (separators)) separators = [separators];
  var s = {};
  separators.forEach (function (separator){
    code = separator.charCodeAt (0);
    if (separator.length > 1 || code < 33 || code > 126){
      throw new Error ("The separator token must be a single printable ASCII " +
          "character");
    }
    s[separator] = true;
  });
  options._separators = s;

  if (options.path){
    if (!cb) throw new Error ("A callback must be passed if the 'path' " +
        "option is enabled");
    if (options.include){
      read (data, options, cb);
    }else{
      fs.readFile (data, { encoding: "utf8" }, function (error, data){
        if (error) return cb (error);
        build (data, options, ".", cb);
      });
    }
  }else{
    return build (data, options, ".", cb);
  }
};
