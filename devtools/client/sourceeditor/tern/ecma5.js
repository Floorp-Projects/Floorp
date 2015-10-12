module.exports = {
  "!name": "ecma5",
  "!define": {"Error.prototype": "Error.prototype"},
  "Infinity": {
    "!type": "number",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Infinity",
    "!doc": "A numeric value representing infinity."
  },
  "undefined": {
    "!type": "?",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/undefined",
    "!doc": "The value undefined."
  },
  "NaN": {
    "!type": "number",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/NaN",
    "!doc": "A value representing Not-A-Number."
  },
  "Object": {
    "!type": "fn()",
    "getPrototypeOf": {
      "!type": "fn(obj: ?) -> ?",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/getPrototypeOf",
      "!doc": "Returns the prototype (i.e. the internal prototype) of the specified object."
    },
    "create": {
      "!type": "fn(proto: ?) -> !custom:Object_create",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/create",
      "!doc": "Creates a new object with the specified prototype object and properties."
    },
    "defineProperty": {
      "!type": "fn(obj: ?, prop: string, desc: ?)",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/defineProperty",
      "!doc": "Defines a new property directly on an object, or modifies an existing property on an object, and returns the object. If you want to see how to use the Object.defineProperty method with a binary-flags-like syntax, see this article."
    },
    "defineProperties": {
      "!type": "fn(obj: ?, props: ?)",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/defineProperty",
      "!doc": "Defines a new property directly on an object, or modifies an existing property on an object, and returns the object. If you want to see how to use the Object.defineProperty method with a binary-flags-like syntax, see this article."
    },
    "getOwnPropertyDescriptor": {
      "!type": "fn(obj: ?, prop: string) -> ?",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/getOwnPropertyDescriptor",
      "!doc": "Returns a property descriptor for an own property (that is, one directly present on an object, not present by dint of being along an object's prototype chain) of a given object."
    },
    "keys": {
      "!type": "fn(obj: ?) -> [string]",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/keys",
      "!doc": "Returns an array of a given object's own enumerable properties, in the same order as that provided by a for-in loop (the difference being that a for-in loop enumerates properties in the prototype chain as well)."
    },
    "getOwnPropertyNames": {
      "!type": "fn(obj: ?) -> [string]",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/getOwnPropertyNames",
      "!doc": "Returns an array of all properties (enumerable or not) found directly upon a given object."
    },
    "seal": {
      "!type": "fn(obj: ?)",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/seal",
      "!doc": "Seals an object, preventing new properties from being added to it and marking all existing properties as non-configurable. Values of present properties can still be changed as long as they are writable."
    },
    "isSealed": {
      "!type": "fn(obj: ?) -> bool",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/isSealed",
      "!doc": "Determine if an object is sealed."
    },
    "freeze": {
      "!type": "fn(obj: ?)",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/freeze",
      "!doc": "Freezes an object: that is, prevents new properties from being added to it; prevents existing properties from being removed; and prevents existing properties, or their enumerability, configurability, or writability, from being changed. In essence the object is made effectively immutable. The method returns the object being frozen."
    },
    "isFrozen": {
      "!type": "fn(obj: ?) -> bool",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/isFrozen",
      "!doc": "Determine if an object is frozen."
    },
    "prototype": {
      "!stdProto": "Object",
      "toString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/toString",
        "!doc": "Returns a string representing the object."
      },
      "toLocaleString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/toLocaleString",
        "!doc": "Returns a string representing the object. This method is meant to be overriden by derived objects for locale-specific purposes."
      },
      "valueOf": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/valueOf",
        "!doc": "Returns the primitive value of the specified object"
      },
      "hasOwnProperty": {
        "!type": "fn(prop: string) -> bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/hasOwnProperty",
        "!doc": "Returns a boolean indicating whether the object has the specified property."
      },
      "propertyIsEnumerable": {
        "!type": "fn(prop: string) -> bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object/propertyIsEnumerable",
        "!doc": "Returns a Boolean indicating whether the specified property is enumerable."
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Object",
    "!doc": "Creates an object wrapper."
  },
  "Function": {
    "!type": "fn(body: string) -> fn()",
    "prototype": {
      "!stdProto": "Function",
      "apply": {
        "!type": "fn(this: ?, args: [?])",
        "!effects": [
          "call and return !this this=!0 !1.<i> !1.<i> !1.<i>"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Function/apply",
        "!doc": "Calls a function with a given this value and arguments provided as an array (or an array like object)."
      },
      "call": {
        "!type": "fn(this: ?, args?: ?) -> !this.!ret",
        "!effects": [
          "call and return !this this=!0 !1 !2 !3 !4"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Function/call",
        "!doc": "Calls a function with a given this value and arguments provided individually."
      },
      "bind": {
        "!type": "fn(this: ?, args?: ?) -> !custom:Function_bind",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Function/bind",
        "!doc": "Creates a new function that, when called, has its this keyword set to the provided value, with a given sequence of arguments preceding any provided when the new function was called."
      },
      "prototype": "?"
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Function",
    "!doc": "Every function in JavaScript is actually a Function object."
  },
  "Array": {
    "!type": "fn(size: number) -> !custom:Array_ctor",
    "isArray": {
      "!type": "fn(value: ?) -> bool",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/isArray",
      "!doc": "Returns true if an object is an array, false if it is not."
    },
    "prototype": {
      "!stdProto": "Array",
      "length": {
        "!type": "number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/length",
        "!doc": "An unsigned, 32-bit integer that specifies the number of elements in an array."
      },
      "concat": {
        "!type": "fn(other: [?]) -> !this",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/concat",
        "!doc": "Returns a new array comprised of this array joined with other array(s) and/or value(s)."
      },
      "join": {
        "!type": "fn(separator?: string) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/join",
        "!doc": "Joins all elements of an array into a string."
      },
      "splice": {
        "!type": "fn(pos: number, amount: number)",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/splice",
        "!doc": "Changes the content of an array, adding new elements while removing old elements."
      },
      "pop": {
        "!type": "fn() -> !this.<i>",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/pop",
        "!doc": "Removes the last element from an array and returns that element."
      },
      "push": {
        "!type": "fn(newelt: ?) -> number",
        "!effects": [
          "propagate !0 !this.<i>"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/push",
        "!doc": "Mutates an array by appending the given elements and returning the new length of the array."
      },
      "shift": {
        "!type": "fn() -> !this.<i>",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/shift",
        "!doc": "Removes the first element from an array and returns that element. This method changes the length of the array."
      },
      "unshift": {
        "!type": "fn(newelt: ?) -> number",
        "!effects": [
          "propagate !0 !this.<i>"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/unshift",
        "!doc": "Adds one or more elements to the beginning of an array and returns the new length of the array."
      },
      "slice": {
        "!type": "fn(from: number, to?: number) -> !this",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/slice",
        "!doc": "Returns a shallow copy of a portion of an array."
      },
      "reverse": {
        "!type": "fn()",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/reverse",
        "!doc": "Reverses an array in place.  The first array element becomes the last and the last becomes the first."
      },
      "sort": {
        "!type": "fn(compare?: fn(a: ?, b: ?) -> number)",
        "!effects": [
          "call !0 !this.<i> !this.<i>"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/sort",
        "!doc": "Sorts the elements of an array in place and returns the array."
      },
      "indexOf": {
        "!type": "fn(elt: ?, from?: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/indexOf",
        "!doc": "Returns the first index at which a given element can be found in the array, or -1 if it is not present."
      },
      "lastIndexOf": {
        "!type": "fn(elt: ?, from?: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/lastIndexOf",
        "!doc": "Returns the last index at which a given element can be found in the array, or -1 if it is not present. The array is searched backwards, starting at fromIndex."
      },
      "every": {
        "!type": "fn(test: fn(elt: ?, i: number) -> bool, context?: ?) -> bool",
        "!effects": [
          "call !0 this=!1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/every",
        "!doc": "Tests whether all elements in the array pass the test implemented by the provided function."
      },
      "some": {
        "!type": "fn(test: fn(elt: ?, i: number) -> bool, context?: ?) -> bool",
        "!effects": [
          "call !0 this=!1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/some",
        "!doc": "Tests whether some element in the array passes the test implemented by the provided function."
      },
      "filter": {
        "!type": "fn(test: fn(elt: ?, i: number) -> bool, context?: ?) -> !this",
        "!effects": [
          "call !0 this=!1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/filter",
        "!doc": "Creates a new array with all elements that pass the test implemented by the provided function."
      },
      "forEach": {
        "!type": "fn(f: fn(elt: ?, i: number), context?: ?)",
        "!effects": [
          "call !0 this=!1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/forEach",
        "!doc": "Executes a provided function once per array element."
      },
      "map": {
        "!type": "fn(f: fn(elt: ?, i: number) -> ?, context?: ?) -> [!0.!ret]",
        "!effects": [
          "call !0 this=!1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/map",
        "!doc": "Creates a new array with the results of calling a provided function on every element in this array."
      },
      "reduce": {
        "!type": "fn(combine: fn(sum: ?, elt: ?, i: number) -> ?, init?: ?) -> !0.!ret",
        "!effects": [
          "call !0 !1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/Reduce",
        "!doc": "Apply a function against an accumulator and each value of the array (from left-to-right) as to reduce it to a single value."
      },
      "reduceRight": {
        "!type": "fn(combine: fn(sum: ?, elt: ?, i: number) -> ?, init?: ?) -> !0.!ret",
        "!effects": [
          "call !0 !1 !this.<i> number"
        ],
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array/ReduceRight",
        "!doc": "Apply a function simultaneously against two values of the array (from right-to-left) as to reduce it to a single value."
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Array",
    "!doc": "The JavaScript Array global object is a constructor for arrays, which are high-level, list-like objects."
  },
  "String": {
    "!type": "fn(value: ?) -> string",
    "fromCharCode": {
      "!type": "fn(code: number) -> string",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/fromCharCode",
      "!doc": "Returns a string created by using the specified sequence of Unicode values."
    },
    "prototype": {
      "!stdProto": "String",
      "length": {
        "!type": "number",
        "!url": "https://developer.mozilla.org/en/docs/JavaScript/Reference/Global_Objects/String/length",
        "!doc": "Represents the length of a string."
      },
      "<i>": "string",
      "charAt": {
        "!type": "fn(i: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/charAt",
        "!doc": "Returns the specified character from a string."
      },
      "charCodeAt": {
        "!type": "fn(i: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/charCodeAt",
        "!doc": "Returns the numeric Unicode value of the character at the given index (except for unicode codepoints > 0x10000)."
      },
      "indexOf": {
        "!type": "fn(char: string, from?: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/indexOf",
        "!doc": "Returns the index within the calling String object of the first occurrence of the specified value, starting the search at fromIndex,\nreturns -1 if the value is not found."
      },
      "lastIndexOf": {
        "!type": "fn(char: string, from?: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/lastIndexOf",
        "!doc": "Returns the index within the calling String object of the last occurrence of the specified value, or -1 if not found. The calling string is searched backward, starting at fromIndex."
      },
      "substring": {
        "!type": "fn(from: number, to?: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/substring",
        "!doc": "Returns a subset of a string between one index and another, or through the end of the string."
      },
      "substr": {
        "!type": "fn(from: number, length?: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/substr",
        "!doc": "Returns the characters in a string beginning at the specified location through the specified number of characters."
      },
      "slice": {
        "!type": "fn(from: number, to?: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/slice",
        "!doc": "Extracts a section of a string and returns a new string."
      },
      "trim": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/Trim",
        "!doc": "Removes whitespace from both ends of the string."
      },
      "trimLeft": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/TrimLeft",
        "!doc": "Removes whitespace from the left end of the string."
      },
      "trimRight": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/TrimRight",
        "!doc": "Removes whitespace from the right end of the string."
      },
      "toUpperCase": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/toUpperCase",
        "!doc": "Returns the calling string value converted to uppercase."
      },
      "toLowerCase": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/toLowerCase",
        "!doc": "Returns the calling string value converted to lowercase."
      },
      "toLocaleUpperCase": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/toLocaleUpperCase",
        "!doc": "Returns the calling string value converted to upper case, according to any locale-specific case mappings."
      },
      "toLocaleLowerCase": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/toLocaleLowerCase",
        "!doc": "Returns the calling string value converted to lower case, according to any locale-specific case mappings."
      },
      "split": {
        "!type": "fn(pattern: string) -> [string]",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/split",
        "!doc": "Splits a String object into an array of strings by separating the string into substrings."
      },
      "concat": {
        "!type": "fn(other: string) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/concat",
        "!doc": "Combines the text of two or more strings and returns a new string."
      },
      "localeCompare": {
        "!type": "fn(other: string) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/localeCompare",
        "!doc": "Returns a number indicating whether a reference string comes before or after or is the same as the given string in sort order."
      },
      "match": {
        "!type": "fn(pattern: +RegExp) -> [string]",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/match",
        "!doc": "Used to retrieve the matches when matching a string against a regular expression."
      },
      "replace": {
        "!type": "fn(pattern: +RegExp, replacement: string) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/replace",
        "!doc": "Returns a new string with some or all matches of a pattern replaced by a replacement.  The pattern can be a string or a RegExp, and the replacement can be a string or a function to be called for each match."
      },
      "search": {
        "!type": "fn(pattern: +RegExp) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String/search",
        "!doc": "Executes the search for a match between a regular expression and this String object."
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/String",
    "!doc": "The String global object is a constructor for strings, or a sequence of characters."
  },
  "Number": {
    "!type": "fn(value: ?) -> number",
    "MAX_VALUE": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/MAX_VALUE",
      "!doc": "The maximum numeric value representable in JavaScript."
    },
    "MIN_VALUE": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/MIN_VALUE",
      "!doc": "The smallest positive numeric value representable in JavaScript."
    },
    "POSITIVE_INFINITY": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/POSITIVE_INFINITY",
      "!doc": "A value representing the positive Infinity value."
    },
    "NEGATIVE_INFINITY": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/NEGATIVE_INFINITY",
      "!doc": "A value representing the negative Infinity value."
    },
    "prototype": {
      "!stdProto": "Number",
      "toString": {
        "!type": "fn(radix?: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/toString",
        "!doc": "Returns a string representing the specified Number object"
      },
      "toFixed": {
        "!type": "fn(digits: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/toFixed",
        "!doc": "Formats a number using fixed-point notation"
      },
      "toExponential": {
        "!type": "fn(digits: number) -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number/toExponential",
        "!doc": "Returns a string representing the Number object in exponential notation"
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Number",
    "!doc": "The Number JavaScript object is a wrapper object allowing you to work with numerical values. A Number object is created using the Number() constructor."
  },
  "Boolean": {
    "!type": "fn(value: ?) -> bool",
    "prototype": {
      "!stdProto": "Boolean"
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Boolean",
    "!doc": "The Boolean object is an object wrapper for a boolean value."
  },
  "RegExp": {
    "!type": "fn(source: string, flags?: string)",
    "prototype": {
      "!stdProto": "RegExp",
      "exec": {
        "!type": "fn(input: string) -> [string]",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/exec",
        "!doc": "Executes a search for a match in a specified string. Returns a result array, or null."
      },
      "compile": {
        "!type": "fn(source: string, flags?: string)",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp",
        "!doc": "Creates a regular expression object for matching text with a pattern."
      },
      "test": {
        "!type": "fn(input: string) -> bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/test",
        "!doc": "Executes the search for a match between a regular expression and a specified string. Returns true or false."
      },
      "global": {
        "!type": "bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp",
        "!doc": "Creates a regular expression object for matching text with a pattern."
      },
      "ignoreCase": {
        "!type": "bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp",
        "!doc": "Creates a regular expression object for matching text with a pattern."
      },
      "multiline": {
        "!type": "bool",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/multiline",
        "!doc": "Reflects whether or not to search in strings across multiple lines.\n"
      },
      "source": {
        "!type": "string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/source",
        "!doc": "A read-only property that contains the text of the pattern, excluding the forward slashes.\n"
      },
      "lastIndex": {
        "!type": "number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp/lastIndex",
        "!doc": "A read/write integer property that specifies the index at which to start the next match."
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RegExp",
    "!doc": "Creates a regular expression object for matching text with a pattern."
  },
  "Date": {
    "!type": "fn(ms: number)",
    "parse": {
      "!type": "fn(source: string) -> +Date",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/parse",
      "!doc": "Parses a string representation of a date, and returns the number of milliseconds since January 1, 1970, 00:00:00 UTC."
    },
    "UTC": {
      "!type": "fn(year: number, month: number, date: number, hour?: number, min?: number, sec?: number, ms?: number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/UTC",
      "!doc": "Accepts the same parameters as the longest form of the constructor, and returns the number of milliseconds in a Date object since January 1, 1970, 00:00:00, universal time."
    },
    "now": {
      "!type": "fn() -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/now",
      "!doc": "Returns the number of milliseconds elapsed since 1 January 1970 00:00:00 UTC."
    },
    "prototype": {
      "toUTCString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toUTCString",
        "!doc": "Converts a date to a string, using the universal time convention."
      },
      "toISOString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toISOString",
        "!doc": "JavaScript provides a direct way to convert a date object into a string in ISO format, the ISO 8601 Extended Format."
      },
      "toDateString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toDateString",
        "!doc": "Returns the date portion of a Date object in human readable form in American English."
      },
      "toTimeString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toTimeString",
        "!doc": "Returns the time portion of a Date object in human readable form in American English."
      },
      "toLocaleDateString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toLocaleDateString",
        "!doc": "Converts a date to a string, returning the \"date\" portion using the operating system's locale's conventions.\n"
      },
      "toLocaleTimeString": {
        "!type": "fn() -> string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/toLocaleTimeString",
        "!doc": "Converts a date to a string, returning the \"time\" portion using the current locale's conventions."
      },
      "getTime": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getTime",
        "!doc": "Returns the numeric value corresponding to the time for the specified date according to universal time."
      },
      "getFullYear": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getFullYear",
        "!doc": "Returns the year of the specified date according to local time."
      },
      "getYear": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getYear",
        "!doc": "Returns the year in the specified date according to local time."
      },
      "getMonth": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getMonth",
        "!doc": "Returns the month in the specified date according to local time."
      },
      "getUTCMonth": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCMonth",
        "!doc": "Returns the month of the specified date according to universal time.\n"
      },
      "getDate": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getDate",
        "!doc": "Returns the day of the month for the specified date according to local time."
      },
      "getUTCDate": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCDate",
        "!doc": "Returns the day (date) of the month in the specified date according to universal time.\n"
      },
      "getDay": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getDay",
        "!doc": "Returns the day of the week for the specified date according to local time."
      },
      "getUTCDay": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCDay",
        "!doc": "Returns the day of the week in the specified date according to universal time.\n"
      },
      "getHours": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getHours",
        "!doc": "Returns the hour for the specified date according to local time."
      },
      "getUTCHours": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCHours",
        "!doc": "Returns the hours in the specified date according to universal time.\n"
      },
      "getMinutes": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getMinutes",
        "!doc": "Returns the minutes in the specified date according to local time."
      },
      "getUTCMinutes": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date",
        "!doc": "Creates JavaScript Date instances which let you work with dates and times."
      },
      "getSeconds": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getSeconds",
        "!doc": "Returns the seconds in the specified date according to local time."
      },
      "getUTCSeconds": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCSeconds",
        "!doc": "Returns the seconds in the specified date according to universal time.\n"
      },
      "getMilliseconds": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getMilliseconds",
        "!doc": "Returns the milliseconds in the specified date according to local time."
      },
      "getUTCMilliseconds": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getUTCMilliseconds",
        "!doc": "Returns the milliseconds in the specified date according to universal time.\n"
      },
      "getTimezoneOffset": {
        "!type": "fn() -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/getTimezoneOffset",
        "!doc": "Returns the time-zone offset from UTC, in minutes, for the current locale."
      },
      "setTime": {
        "!type": "fn(date: +Date) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setTime",
        "!doc": "Sets the Date object to the time represented by a number of milliseconds since January 1, 1970, 00:00:00 UTC.\n"
      },
      "setFullYear": {
        "!type": "fn(year: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setFullYear",
        "!doc": "Sets the full year for a specified date according to local time.\n"
      },
      "setUTCFullYear": {
        "!type": "fn(year: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCFullYear",
        "!doc": "Sets the full year for a specified date according to universal time.\n"
      },
      "setMonth": {
        "!type": "fn(month: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setMonth",
        "!doc": "Set the month for a specified date according to local time."
      },
      "setUTCMonth": {
        "!type": "fn(month: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCMonth",
        "!doc": "Sets the month for a specified date according to universal time.\n"
      },
      "setDate": {
        "!type": "fn(day: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setDate",
        "!doc": "Sets the day of the month for a specified date according to local time."
      },
      "setUTCDate": {
        "!type": "fn(day: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCDate",
        "!doc": "Sets the day of the month for a specified date according to universal time.\n"
      },
      "setHours": {
        "!type": "fn(hour: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setHours",
        "!doc": "Sets the hours for a specified date according to local time, and returns the number of milliseconds since 1 January 1970 00:00:00 UTC until the time represented by the updated Date instance."
      },
      "setUTCHours": {
        "!type": "fn(hour: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCHours",
        "!doc": "Sets the hour for a specified date according to universal time.\n"
      },
      "setMinutes": {
        "!type": "fn(min: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setMinutes",
        "!doc": "Sets the minutes for a specified date according to local time."
      },
      "setUTCMinutes": {
        "!type": "fn(min: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCMinutes",
        "!doc": "Sets the minutes for a specified date according to universal time.\n"
      },
      "setSeconds": {
        "!type": "fn(sec: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setSeconds",
        "!doc": "Sets the seconds for a specified date according to local time."
      },
      "setUTCSeconds": {
        "!type": "fn(sec: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCSeconds",
        "!doc": "Sets the seconds for a specified date according to universal time.\n"
      },
      "setMilliseconds": {
        "!type": "fn(ms: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setMilliseconds",
        "!doc": "Sets the milliseconds for a specified date according to local time.\n"
      },
      "setUTCMilliseconds": {
        "!type": "fn(ms: number) -> number",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date/setUTCMilliseconds",
        "!doc": "Sets the milliseconds for a specified date according to universal time.\n"
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Date",
    "!doc": "Creates JavaScript Date instances which let you work with dates and times."
  },
  "Error": {
    "!type": "fn(message: string)",
    "prototype": {
      "name": {
        "!type": "string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Error/name",
        "!doc": "A name for the type of error."
      },
      "message": {
        "!type": "string",
        "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Error/message",
        "!doc": "A human-readable description of the error."
      }
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Error",
    "!doc": "Creates an error object."
  },
  "SyntaxError": {
    "!type": "fn(message: string)",
    "prototype": "Error.prototype",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/SyntaxError",
    "!doc": "Represents an error when trying to interpret syntactically invalid code."
  },
  "ReferenceError": {
    "!type": "fn(message: string)",
    "prototype": "Error.prototype",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/ReferenceError",
    "!doc": "Represents an error when a non-existent variable is referenced."
  },
  "URIError": {
    "!type": "fn(message: string)",
    "prototype": "Error.prototype",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/URIError",
    "!doc": "Represents an error when a malformed URI is encountered."
  },
  "EvalError": {
    "!type": "fn(message: string)",
    "prototype": "Error.prototype",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/EvalError",
    "!doc": "Represents an error regarding the eval function."
  },
  "RangeError": {
    "!type": "fn(message: string)",
    "prototype": "Error.prototype",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/RangeError",
    "!doc": "Represents an error when a number is not within the correct range allowed."
  },
  "parseInt": {
    "!type": "fn(string: string, radix?: number) -> number",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/parseInt",
    "!doc": "Parses a string argument and returns an integer of the specified radix or base."
  },
  "parseFloat": {
    "!type": "fn(string: string) -> number",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/parseFloat",
    "!doc": "Parses a string argument and returns a floating point number."
  },
  "isNaN": {
    "!type": "fn(value: number) -> bool",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/isNaN",
    "!doc": "Determines whether a value is NaN or not. Be careful, this function is broken. You may be interested in ECMAScript 6 Number.isNaN."
  },
  "eval": {
    "!type": "fn(code: string) -> ?",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/eval",
    "!doc": "Evaluates JavaScript code represented as a string."
  },
  "encodeURI": {
    "!type": "fn(uri: string) -> string",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/encodeURI",
    "!doc": "Encodes a Uniform Resource Identifier (URI) by replacing each instance of certain characters by one, two, three, or four escape sequences representing the UTF-8 encoding of the character (will only be four escape sequences for characters composed of two \"surrogate\" characters)."
  },
  "encodeURIComponent": {
    "!type": "fn(uri: string) -> string",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/encodeURIComponent",
    "!doc": "Encodes a Uniform Resource Identifier (URI) component by replacing each instance of certain characters by one, two, three, or four escape sequences representing the UTF-8 encoding of the character (will only be four escape sequences for characters composed of two \"surrogate\" characters)."
  },
  "decodeURI": {
    "!type": "fn(uri: string) -> string",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/decodeURI",
    "!doc": "Decodes a Uniform Resource Identifier (URI) previously created by encodeURI or by a similar routine."
  },
  "decodeURIComponent": {
    "!type": "fn(uri: string) -> string",
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/decodeURIComponent",
    "!doc": "Decodes a Uniform Resource Identifier (URI) component previously created by encodeURIComponent or by a similar routine."
  },
  "Math": {
    "E": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/E",
      "!doc": "The base of natural logarithms, e, approximately 2.718."
    },
    "LN2": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/LN2",
      "!doc": "The natural logarithm of 2, approximately 0.693."
    },
    "LN10": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/LN10",
      "!doc": "The natural logarithm of 10, approximately 2.302."
    },
    "LOG2E": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/LOG2E",
      "!doc": "The base 2 logarithm of E (approximately 1.442)."
    },
    "LOG10E": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/LOG10E",
      "!doc": "The base 10 logarithm of E (approximately 0.434)."
    },
    "SQRT1_2": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/SQRT1_2",
      "!doc": "The square root of 1/2; equivalently, 1 over the square root of 2, approximately 0.707."
    },
    "SQRT2": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/SQRT2",
      "!doc": "The square root of 2, approximately 1.414."
    },
    "PI": {
      "!type": "number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/PI",
      "!doc": "The ratio of the circumference of a circle to its diameter, approximately 3.14159."
    },
    "abs": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/abs",
      "!doc": "Returns the absolute value of a number."
    },
    "cos": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/cos",
      "!doc": "Returns the cosine of a number."
    },
    "sin": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/sin",
      "!doc": "Returns the sine of a number."
    },
    "tan": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/tan",
      "!doc": "Returns the tangent of a number."
    },
    "acos": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/acos",
      "!doc": "Returns the arccosine (in radians) of a number."
    },
    "asin": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/asin",
      "!doc": "Returns the arcsine (in radians) of a number."
    },
    "atan": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/atan",
      "!doc": "Returns the arctangent (in radians) of a number."
    },
    "atan2": {
      "!type": "fn(number, number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/atan2",
      "!doc": "Returns the arctangent of the quotient of its arguments."
    },
    "ceil": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/ceil",
      "!doc": "Returns the smallest integer greater than or equal to a number."
    },
    "floor": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/floor",
      "!doc": "Returns the largest integer less than or equal to a number."
    },
    "round": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/round",
      "!doc": "Returns the value of a number rounded to the nearest integer."
    },
    "exp": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/exp",
      "!doc": "Returns Ex, where x is the argument, and E is Euler's constant, the base of the natural logarithms."
    },
    "log": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/log",
      "!doc": "Returns the natural logarithm (base E) of a number."
    },
    "sqrt": {
      "!type": "fn(number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/sqrt",
      "!doc": "Returns the square root of a number."
    },
    "pow": {
      "!type": "fn(number, number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/pow",
      "!doc": "Returns base to the exponent power, that is, baseexponent."
    },
    "max": {
      "!type": "fn(number, number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/max",
      "!doc": "Returns the largest of zero or more numbers."
    },
    "min": {
      "!type": "fn(number, number) -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/min",
      "!doc": "Returns the smallest of zero or more numbers."
    },
    "random": {
      "!type": "fn() -> number",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math/random",
      "!doc": "Returns a floating-point, pseudo-random number in the range [0, 1) that is, from 0 (inclusive) up to but not including 1 (exclusive), which you can then scale to your desired range."
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/Math",
    "!doc": "A built-in object that has properties and methods for mathematical constants and functions."
  },
  "JSON": {
    "parse": {
      "!type": "fn(json: string) -> ?",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/JSON/parse",
      "!doc": "Parse a string as JSON, optionally transforming the value produced by parsing."
    },
    "stringify": {
      "!type": "fn(value: ?) -> string",
      "!url": "https://developer.mozilla.org/en-US/docs/JavaScript/Reference/Global_Objects/JSON/stringify",
      "!doc": "Convert a value to JSON, optionally replacing values if a replacer function is specified, or optionally including only the specified properties if a replacer array is specified."
    },
    "!url": "https://developer.mozilla.org/en-US/docs/JSON",
    "!doc": "JSON (JavaScript Object Notation) is a data-interchange format.  It closely resembles a subset of JavaScript syntax, although it is not a strict subset. (See JSON in the JavaScript Reference for full details.)  It is useful when writing any kind of JavaScript-based application, including websites and browser extensions.  For example, you might store user information in JSON format in a cookie, or you might store extension preferences in JSON in a string-valued browser preference."
  }
}
