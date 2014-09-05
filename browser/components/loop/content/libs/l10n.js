/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set shiftwidth=2 tabstop=2 autoindent cindent expandtab: */

'use strict';

// This is a modified version of l10n.js that the pdf.js extension uses.
// It uses an explicitly passed object for the strings/locale functionality,
// and does not automatically translate on DOMContentLoaded, but requires
// initialize to be called. This improves testability and helps to avoid race
// conditions.
(function(window) {
  var gL10nDetails;
  var gLanguage = '';

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
    if (args && args.num) {
      var num = args && args.num;
      delete args.num;
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

      translateFragment();
    },

    // get a localized string
    get: translateString,

    // get the document language
    getLanguage: function() { return gLanguage; },

    // get the direction (ltr|rtl) of the current language
    getDirection: function() {
      // http://www.w3.org/International/questions/qa-scripts
      // Arabic, Hebrew, Farsi, Pashto, Urdu
      var rtlList = ['ar', 'he', 'fa', 'ps', 'ur'];
      return (rtlList.indexOf(gLanguage) >= 0) ? 'rtl' : 'ltr';
    },

    // translate an element or document fragment
    translate: translateFragment
  };
})(this);
