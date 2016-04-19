/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

'use strict';

// This is a modified version of l10n.js that the pdf.js extension uses.
// It uses an explicitly passed object for the strings/locale functionality,
// and does not automatically translate on DOMContentLoaded, but requires
// initialize to be called. This improves testability and helps to avoid race
// conditions. It has also been updated to be closer to the gaia l10n.js api.
(function(window) {
  var gL10nDetails;
  var gLanguage = '';
  // These are the available plural functions that give the appropriate index
  // based on the plural rule number specified. The first element is the number
  // of plural forms and the second is the function to figure out the index.
  // NOTE: these rule functions are - unfortunately - a copy from the `gFunctions`
  //       array in intl/locale/PluralForm.jsm. An attempt should be made to keep
  //       this in sync with that source.
  //       These need to be copied over, because there's no way to copy Function
  //       objects over from chrome to content and the PluralForm.jsm module can
  //       only be loaded in a chrome context.
  var kPluralFunctions = [
    // 0: Chinese
    [1, function(n) {
      return 0
    }],
    // 1: English
    [2, function(n) {
      return n!=1?1:0
    }],
    // 2: French
    [2, function(n) {
      return n>1?1:0
    }],
    // 3: Latvian
    [3, function(n) {
      return n%10==1&&n%100!=11?1:n!=0?2:0
    }],
    // 4: Scottish Gaelic
    [4, function(n) {
      return n==1||n==11?0:n==2||n==12?1:n>0&&n<20?2:3
    }],
    // 5: Romanian
    [3, function(n) {
      return n==1?0:n==0||n%100>0&&n%100<20?1:2
    }],
    // 6: Lithuanian
    [3, function(n) {
      return n%10==1&&n%100!=11?0:n%10>=2&&(n%100<10||n%100>=20)?2:1
    }],
    // 7: Russian
    [3, function(n) {
      return n%10==1&&n%100!=11?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2
    }],
    // 8: Slovak
    [3, function(n) {
      return n==1?0:n>=2&&n<=4?1:2
    }],
    // 9: Polish
    [3, function(n) {
      return n==1?0:n%10>=2&&n%10<=4&&(n%100<10||n%100>=20)?1:2
    }],
    // 10: Slovenian
    [4, function(n) {
      return n%100==1?0:n%100==2?1:n%100==3||n%100==4?2:3
    }],
    // 11: Irish Gaeilge
    [5, function(n) {
      return n==1?0:n==2?1:n>=3&&n<=6?2:n>=7&&n<=10?3:4
    }],
    // 12: Arabic
    [6, function(n) {
      return n==0?5:n==1?0:n==2?1:n%100>=3&&n%100<=10?2:n%100>=11&&n%100<=99?3:4
    }],
    // 13: Maltese
    [4, function(n) {
      return n==1?0:n==0||n%100>0&&n%100<=10?1:n%100>10&&n%100<20?2:3
    }],
    // 14: Macedonian
    [3, function(n) {
      return n%10==1?0:n%10==2?1:2
    }],
    // 15: Icelandic
    [2, function(n) {
      return n%10==1&&n%100!=11?0:1
    }],
    // 16: Breton
    [5, function(n) {
      return n%10==1&&n%100!=11&&n%100!=71&&n%100!=91?0:n%10==2&&n%100!=12&&
        n%100!=72&&n%100!=92?1:(n%10==3||n%10==4||n%10==9)&&n%100!=13&&n%100!=14&&
        n%100!=19&&n%100!=73&&n%100!=74&&n%100!=79&&n%100!=93&&n%100!=94&&n%100!=99?2:n%1000000==0&&n!=0?3:4
    }]
  ];
  var gPluralFunc = null;

  // fetch an l10n objects
  function getL10nData(key, num) {
    var response = gL10nDetails.getStrings(key);
    var data = JSON.parse(response);
    if (!data)
      console.warn('[l10n] #' + key + ' missing for [' + gLanguage + ']');
    if (num !== undefined) {
      for (var prop in data) {
        data[prop] = gL10nDetails.getPluralForm(num, data[prop]);
      }
    }
    return data;
  }

  function fallbackGetPluralForm(num, str) {
    // Figure out which index to use for the semi-colon separated words.
    var index = gPluralFunc(num ? Number(num) : 0);
    var words = str ? str.split(/;/) : [""];

    // Explicitly check bounds to avoid strict warnings.
    var ret = index < words.length ? words[index] : undefined;

    // Check for array out of bounds or empty strings.
    if ((ret == undefined) || (ret == "")) {
      // Display a message in the error console
      console.error("Index #" + index + " of '" + str + "' for value " + num +
          " is invalid -- plural rule #" + ret);

      // Default to the first entry (which might be empty, but not undefined).
      ret = words[0];
    }

    return ret;
  }

  // replace {{arguments}} with their values
  function substArguments(text, args) {
    if (!args)
      return text;

    return text.replace(/\{\{\s*(\w+)\s*\}\}/g, function(all, name) {
      return name in args ? args[name] : '{{' + name + '}}';
    });
  }

  // translate a string
  function translateString(key, args, fallback) {
    var num;
    if (args && ("num" in args)) {
      num = args.num;
    }
    var data = getL10nData(key, num);
    if (!data && fallback)
      data = {textContent: fallback};
    if (!data)
      return '{{' + key + '}}';
    return substArguments(data.textContent, args);
  }

  // translate an HTML element
  function translateElement(element) {
    if (!element || !element.dataset)
      return;

    // get the related l10n object
    var key = element.dataset.l10nId;
    var data = getL10nData(key);
    if (!data)
      return;

    // get arguments (if any)
    // TODO: more flexible parser?
    var args;
    if (element.dataset.l10nArgs) try {
      args = JSON.parse(element.dataset.l10nArgs);
    } catch (e) {
      console.warn('[l10n] could not parse arguments for #' + key + '');
    }

    // translate element
    // TODO: security check?
    for (var k in data)
      element[k] = substArguments(data[k], args);
  }


  // translate an HTML subtree
  function translateFragment(element) {
    element = element || document.querySelector('html');

    // check all translatable children (= w/ a `data-l10n-id' attribute)
    var children = element.querySelectorAll('*[data-l10n-id]');
    var elementCount = children.length;
    for (var i = 0; i < elementCount; i++)
      translateElement(children[i]);

    // translate element itself if necessary
    if (element.dataset.l10nId)
      translateElement(element);
  }

  // Public API
  document.mozL10n = {
    /**
     * Called to do the initial translation, this should be called
     * when DOMContentLoaded is fired, or the equivalent time.
     *
     * @param {Object} l10nDetails An object implementing the locale attribute
     *                             and getStrings(key) function.
     */
    initialize: function(l10nDetails) {
      gL10nDetails = l10nDetails;
      gLanguage = gL10nDetails.locale;
      // Fallback to a working - synchronous - implementation of retrieving the
      // plural form of a string.
      if (!gL10nDetails.getPluralForm && ("pluralRule" in gL10nDetails)) {
        var ruleNum = gL10nDetails.pluralRule;

        // Default to "all plural" if the value is out of bounds or invalid
        if (ruleNum < 0 || ruleNum >= kPluralFunctions.length || isNaN(ruleNum)) {
          console.error(["Invalid rule number: ", ruleNum, " -- defaulting to 0"]);
          ruleNum = 0;
        }

        gPluralFunc = kPluralFunctions[ruleNum][1];
        gL10nDetails.getPluralForm = fallbackGetPluralForm;
      }

      translateFragment();
    },

    // get a localized string
    get: translateString,

    // get the document language
    language: {
      set code(lang) {
        throw new Error("unsupported");
      },
      get code() {
        return gLanguage;
      },
      get direction() {
        // http://www.w3.org/International/questions/qa-scripts
        // Arabic, Hebrew, Farsi, Pashto, Urdu
        var rtlList = ['ar', 'he', 'fa', 'ps', 'ur'];
        return (rtlList.indexOf(gLanguage) >= 0) ? 'rtl' : 'ltr';
      }
    },

    // translate an element or document fragment
    translate: translateFragment
  };
})(this);
