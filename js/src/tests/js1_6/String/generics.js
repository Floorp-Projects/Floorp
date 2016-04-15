var BUGNUMBER = 1263558;
var summary = "Self-host all String generics.";

print(BUGNUMBER + ": " + summary);

var result;
var str = "ABCde";
var strObj = {
  toString() {
    return "ABCde";
  }
};

// String.substring.
assertThrowsInstanceOf(() => String.substring(), TypeError);
assertEq(String.substring(str), "ABCde");
assertEq(String.substring(str, 1), "BCde");
assertEq(String.substring(str, 1, 3), "BC");
assertEq(String.substring(strObj), "ABCde");
assertEq(String.substring(strObj, 1), "BCde");
assertEq(String.substring(strObj, 1, 3), "BC");

// String.substr.
assertThrowsInstanceOf(() => String.substr(), TypeError);
assertEq(String.substr(str), "ABCde");
assertEq(String.substr(str, 1), "BCde");
assertEq(String.substr(str, 1, 3), "BCd");
assertEq(String.substr(strObj), "ABCde");
assertEq(String.substr(strObj, 1), "BCde");
assertEq(String.substr(strObj, 1, 3), "BCd");

// String.slice.
assertThrowsInstanceOf(() => String.slice(), TypeError);
assertEq(String.slice(str), "ABCde");
assertEq(String.slice(str, 1), "BCde");
assertEq(String.slice(str, 1, 3), "BC");
assertEq(String.slice(strObj), "ABCde");
assertEq(String.slice(strObj, 1), "BCde");
assertEq(String.slice(strObj, 1, 3), "BC");

// String.match.
assertThrowsInstanceOf(() => String.match(), TypeError);
result = String.match(str);
assertEq(result.index, 0);
assertEq(result.length, 1);
assertEq(result[0], "");
result = String.match(str, /c/i);
assertEq(result.index, 2);
assertEq(result.length, 1);
assertEq(result[0], "C");
result = String.match(strObj);
assertEq(result.index, 0);
assertEq(result.length, 1);
assertEq(result[0], "");
result = String.match(strObj, /c/i);
assertEq(result.index, 2);
assertEq(result.length, 1);
assertEq(result[0], "C");

// String.replace.
assertThrowsInstanceOf(() => String.replace(), TypeError);
assertEq(String.replace(str), "ABCde");
assertEq(String.replace(str, /c/i), "ABundefinedde");
assertEq(String.replace(str, /c/i, "x"), "ABxde");
assertEq(String.replace(strObj), "ABCde");
assertEq(String.replace(strObj, /c/i), "ABundefinedde");
assertEq(String.replace(strObj, /c/i, "x"), "ABxde");

// String.search.
assertThrowsInstanceOf(() => String.search(), TypeError);
assertEq(String.search(str), 0);
assertEq(String.search(str, /c/i), 2);
assertEq(String.search(strObj), 0);
assertEq(String.search(strObj, /c/i), 2);

// String.split.
assertThrowsInstanceOf(() => String.split(), TypeError);
assertEq(String.split(str).join(","), "ABCde");
assertEq(String.split(str, /[bd]/i).join(","), "A,C,e");
assertEq(String.split(str, /[bd]/i, 2).join(","), "A,C");
assertEq(String.split(strObj).join(","), "ABCde");
assertEq(String.split(strObj, /[bd]/i).join(","), "A,C,e");
assertEq(String.split(strObj, /[bd]/i, 2).join(","), "A,C");

// String.toLowerCase.
assertThrowsInstanceOf(() => String.toLowerCase(), TypeError);
assertEq(String.toLowerCase(str), "abcde");
assertEq(String.toLowerCase(strObj), "abcde");

// String.toUpperCase.
assertThrowsInstanceOf(() => String.toUpperCase(), TypeError);
assertEq(String.toUpperCase(str), "ABCDE");
assertEq(String.toUpperCase(strObj), "ABCDE");

// String.charAt.
assertThrowsInstanceOf(() => String.charAt(), TypeError);
assertEq(String.charAt(str), "A");
assertEq(String.charAt(str, 2), "C");
assertEq(String.charAt(strObj), "A");
assertEq(String.charAt(strObj, 2), "C");

// String.charCodeAt.
assertThrowsInstanceOf(() => String.charCodeAt(), TypeError);
assertEq(String.charCodeAt(str), 65);
assertEq(String.charCodeAt(str, 2), 67);
assertEq(String.charCodeAt(strObj), 65);
assertEq(String.charCodeAt(strObj, 2), 67);

// String.includes.
assertThrowsInstanceOf(() => String.includes(), TypeError);
assertEq(String.includes(str), false);
assertEq(String.includes(str, "C"), true);
assertEq(String.includes(str, "C", 2), true);
assertEq(String.includes(str, "C", 3), false);
assertEq(String.includes(strObj), false);
assertEq(String.includes(strObj, "C"), true);
assertEq(String.includes(strObj, "C", 2), true);
assertEq(String.includes(strObj, "C", 3), false);

// String.indexOf.
assertThrowsInstanceOf(() => String.indexOf(), TypeError);
assertEq(String.indexOf(str), -1);
assertEq(String.indexOf(str, "C"), 2);
assertEq(String.indexOf(str, "C", 2), 2);
assertEq(String.indexOf(str, "C", 3), -1);
assertEq(String.indexOf(strObj), -1);
assertEq(String.indexOf(strObj, "C"), 2);
assertEq(String.indexOf(strObj, "C", 2), 2);
assertEq(String.indexOf(strObj, "C", 3), -1);

// String.lastIndexOf.
assertThrowsInstanceOf(() => String.lastIndexOf(), TypeError);
assertEq(String.lastIndexOf(str), -1);
assertEq(String.lastIndexOf(str, "C"), 2);
assertEq(String.lastIndexOf(str, "C", 2), 2);
assertEq(String.lastIndexOf(str, "C", 1), -1);
assertEq(String.lastIndexOf(strObj), -1);
assertEq(String.lastIndexOf(strObj, "C"), 2);
assertEq(String.lastIndexOf(strObj, "C", 2), 2);
assertEq(String.lastIndexOf(strObj, "C", 1), -1);

// String.startsWith.
assertThrowsInstanceOf(() => String.startsWith(), TypeError);
assertEq(String.startsWith(str), false);
assertEq(String.startsWith(str, "A"), true);
assertEq(String.startsWith(str, "B", 0), false);
assertEq(String.startsWith(str, "B", 1), true);
assertEq(String.startsWith(strObj), false);
assertEq(String.startsWith(strObj, "A"), true);
assertEq(String.startsWith(strObj, "B", 0), false);
assertEq(String.startsWith(strObj, "B", 1), true);

// String.endsWith.
assertThrowsInstanceOf(() => String.endsWith(), TypeError);
assertEq(String.endsWith(str), false);
assertEq(String.endsWith(str, "e"), true);
assertEq(String.endsWith(str, "B", 0), false);
assertEq(String.endsWith(str, "B", 2), true);
assertEq(String.endsWith(strObj), false);
assertEq(String.endsWith(strObj, "e"), true);
assertEq(String.endsWith(strObj, "B", 0), false);
assertEq(String.endsWith(strObj, "B", 2), true);

// String.trim.
var str2 = "  ABCde  ";
var strObj2 = {
  toString() {
    return "  ABCde  ";
  }
};
assertThrowsInstanceOf(() => String.trim(), TypeError);
assertEq(String.trim(str2), "ABCde");
assertEq(String.trim(strObj2), "ABCde");

// String.trimLeft.
assertThrowsInstanceOf(() => String.trimLeft(), TypeError);
assertEq(String.trimLeft(str2), "ABCde  ");
assertEq(String.trimLeft(strObj2), "ABCde  ");

// String.trimRight.
assertThrowsInstanceOf(() => String.trimRight(), TypeError);
assertEq(String.trimRight(str2), "  ABCde");
assertEq(String.trimRight(strObj2), "  ABCde");

// String.toLocaleLowerCase.
assertThrowsInstanceOf(() => String.toLocaleLowerCase(), TypeError);
assertEq(String.toLocaleLowerCase(str), str.toLocaleLowerCase());
assertEq(String.toLocaleLowerCase(strObj), str.toLocaleLowerCase());

// String.toLocaleUpperCase.
assertThrowsInstanceOf(() => String.toLocaleUpperCase(), TypeError);
assertEq(String.toLocaleUpperCase(str), str.toLocaleUpperCase());
assertEq(String.toLocaleUpperCase(strObj), str.toLocaleUpperCase());

// String.localeCompare.
assertThrowsInstanceOf(() => String.localeCompare(), TypeError);
assertEq(String.localeCompare(str), str.localeCompare());
assertEq(String.localeCompare(str, "abcde"), str.localeCompare("abcde"));
assertEq(String.localeCompare(strObj), str.localeCompare());
assertEq(String.localeCompare(strObj, "abcde"), str.localeCompare("abcde"));

// String.normalize.
if ("normalize" in String.prototype) {
  var str3 = "\u3082\u3058\u3089 \u3082\u3057\u3099\u3089";
  var strObj3 = {
    toString() {
      return "\u3082\u3058\u3089 \u3082\u3057\u3099\u3089";
    }
  };
  assertThrowsInstanceOf(() => String.normalize(), TypeError);

  assertEq(String.normalize(str3), "\u3082\u3058\u3089 \u3082\u3058\u3089");
  assertEq(String.normalize(str3, "NFD"), "\u3082\u3057\u3099\u3089 \u3082\u3057\u3099\u3089");
  assertEq(String.normalize(strObj3), "\u3082\u3058\u3089 \u3082\u3058\u3089");
  assertEq(String.normalize(strObj3, "NFD"), "\u3082\u3057\u3099\u3089 \u3082\u3057\u3099\u3089");
}

// String.concat.
assertThrowsInstanceOf(() => String.concat(), TypeError);
assertEq(String.concat(str), "ABCde");
assertEq(String.concat(str, "f"), "ABCdef");
assertEq(String.concat(str, "f", "g"), "ABCdefg");
assertEq(String.concat(strObj), "ABCde");
assertEq(String.concat(strObj, "f"), "ABCdef");
assertEq(String.concat(strObj, "f", "g"), "ABCdefg");

if (typeof reportCompare === "function")
  reportCompare(true, true);
